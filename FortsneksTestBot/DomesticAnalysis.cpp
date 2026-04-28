#include "DomesticAnalysis.h"
#include "Pathing.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/EnumDefs.h>

#include <climits>

using namespace mybot;

namespace
{
	constexpr int kMaxBarbSpawnPathDistance = 10;
	constexpr int kMaxWaterBonusBarbThreatDistance = 8; // AI_barbAttackSeaMove, 4 moves of a gallery

	constexpr ivec2 kAdj[]{
		{ -1, -1 },
		{ -1, +0 },
		{ -1, +1 },
		{ +0, -1 },
		//{ +0, +0 },
		{ +0, +1 },
		{ +1, -1 },
		{ +1, +0 },
		{ +1, +1 },
	};

	// Return a list of all rival plots that are on our border.
	std::vector<ivec2> findRivalBorderPlots(EPlayer activePlayerI, MapGeometry mapGeom, Span2D<const Plot> map)
	{
		std::vector<ivec2> borderPlots;
		borderPlots.reserve(100);

		for (const int y : range(map.dim.y))
		{
			for (const int x : range(map.dim.x))
			{
				const Plot& plot = map[{ x, y }];
				// Quick filter: Border plots have to be visible and not owned by us.
				if (plot.isVisible && plot.owner != activePlayerI && plot.owner != kNoPlayer)
				{
					for (const ivec2 adjD : kAdj)
					{
						if (const auto optAdjCoord = mapGeom.resolve(ivec2(x, y) + adjD))
						{
							if (map[*optAdjCoord].owner == activePlayerI)
							{
								borderPlots.emplace_back(x, y);
								break;
							}
						}
					}
				}
			}
		}

		return borderPlots;
	}

	std::vector<CityAnalysis> getInitialCityAnalyses(std::span<const City> myCities, int secondRingCulture)
	{
		std::vector<CityAnalysis> analysises;
		analysises.reserve(myCities.size());

		for (const City& city : myCities)
		{
			analysises.push_back({
				.name = city.name,
				.hasSecondRing = city.optInspectableCityInfo->culture >= secondRingCulture,
				.isInterior = true,
			});
		}

		return analysises;
	}

	bool isPotentialBarbSpawnPlotIgnoringSuppression(const Plot& plot)
	{
		// We assume plots with revealed owner are still owned.
		return !plot.isVisible && (plot.type == EPlotType::None || plot.owner == kNoPlayer);
	}

	// 3x3 dilation
	void dilate(MapGeometry mapGeom, DynamicArray2D<bool>& map)
	{
		// Dilate H
		for (const int y : range(map.dim.y))
		{
			bool prev = mapGeom.isWrapX && map[{ map.dim.x - 1, y }];
			for (const int x : range(map.dim.x))
				map[{ x, y }] |= std::exchange(prev, map[{ x, y }]);
			prev = mapGeom.isWrapX && map[{ 0, y }];
			for (const int x : range(map.dim.x) | std::views::reverse)
				map[{ x, y }] |= std::exchange(prev, map[{ x, y }]);
		}
		// Dilate V
		for (const int y : range(map.dim.y))
			if (const std::optional<ivec2> yBelow = mapGeom.resolve({ 0, y + 1 }))
				for (const int x : range(map.dim.x))
					map[{ x, y }] |= map[{ x, yBelow->y }];
		for (const int y : range(map.dim.y) | std::views::reverse)
			if (const std::optional<ivec2> yAbove = mapGeom.resolve({ 0, y - 1 }))
				for (const int x : range(map.dim.x))
					map[{ x, y }] |= map[{ x, yAbove->y }];
	}
}

DomesticAnalysis mybot::doDomesticAnalysis(
	EPlayer activePlayerI,
	std::span<const City> myCities,
	MapGeometry mapGeom,
	Span2D<const Plot> map,
	Span2D<const PathingPlot> pathingMap,
	Span2D<const MultipleSourceDistanceFieldCell> cityPathLengthField,
	std::span<const Unit> allVisibleUnits,
	[[maybe_unused]] const MapUnitLookup& mapMyUnitsLookup,
	const GlobalInfoData& infos
)
{
	const std::vector<ivec2> myCityCoords(std::from_range, myCities | std::views::transform(&City::coord));
	// We include unrevealed as a worst case against barbs.
	const DynamicArray2D<MultipleSourceDistanceFieldCell> cityPathLengthFieldIncludingUnrevealed = computeMultipleSourcePathLengthField(
		{ mapGeom, pathingMap },
		static_cast<PathingPlot::EFlag>(PathingPlot::Impassible | PathingPlot::Water),
		INT_MAX,
		myCityCoords
	);

	// Only need to care about coastal water bonuses. Barbs can't get open sea bonuses.
	std::vector<ivec2> myCoastalWaterBonusesCoords;
	for (const int y : range(map.dim.y))
		for (const int x : range(map.dim.x))
			if (const Plot& plot = map[{ x, y }]; plot.type == EPlotType::Ocean && plot.owner == activePlayerI && plot.bonus != EBonus::None && plot.isCoastalWater)
				myCoastalWaterBonusesCoords.emplace_back(x, y);

	// Path distance field for barbs targeting our water bonuses.
	const DynamicArray2D<MultipleSourceDistanceFieldCell> coastalWaterBonusBarbPathLengthField = computeMultipleSourcePathLengthField(
		{ mapGeom, pathingMap },
		static_cast<PathingPlot::EFlag>(PathingPlot::Impassible | PathingPlot::NoGoForBarbCoastalUnits), // We include unrevealed as a worst case against barbs.
		kMaxWaterBonusBarbThreatDistance,
		myCoastalWaterBonusesCoords
	);

	// Path distance field to find the closest city to a water bonus.
	const DynamicArray2D<MultipleSourceDistanceFieldCell> cityCoastalPathLengthField = computeMultipleSourcePathLengthField(
		{ mapGeom, pathingMap },
		static_cast<PathingPlot::EFlag>(PathingPlot::Impassible | PathingPlot::Unrevealed | PathingPlot::NoGoForOurCoastalUnits),
		INT_MAX,
		myCityCoords
	);

	DomesticAnalysis output;

	output.cityAnalyses = getInitialCityAnalyses(myCities, infos.cultureLevels[2].culture);

	// Count border plots
	const std::vector<ivec2> rivalBorderPlots = findRivalBorderPlots(activePlayerI, mapGeom, map);
	for (const ivec2 coord : rivalBorderPlots)
	{
		const Plot& plot = map[coord];
		for (const ivec2 adjD : kAdj)
		{
			if (const auto optAdjCoord = mapGeom.resolve(coord + adjD))
			{
				if (map[*optAdjCoord].owner == activePlayerI)
				{
					if (cityPathLengthField[*optAdjCoord].isReachable())
					{
						const size_t closestCityI = cityPathLengthField[*optAdjCoord].index;
						output.cityAnalyses[closestCityI].isInterior = false;
						++output.cityAnalyses[closestCityI].rivalCivBorderPlotCounts[plot.owner];
					}
				}
			}
		}
	}

	// Find plots where barb spawns are suppressed.
	output.barbSuppressionMap = DynamicArray2D<bool>(mapGeom.dim);
	// 5x5 around all visible units.
	for (const Unit& unit : allVisibleUnits)
		output.barbSuppressionMap[unit.coord] = true;
	dilate(mapGeom, output.barbSuppressionMap);
	// 3x3 around all owned plots
	for (const int y : range(map.dim.y))
		for (const int x : range(map.dim.x))
			if (map[{ x, y }].owner != kNoPlayer)
				output.barbSuppressionMap[{ x, y }] = true;
	dilate(mapGeom, output.barbSuppressionMap); // This will also do 5x5 around units.


	output.barbPotentialSpawnMap = DynamicArray2D<bool>(mapGeom.dim);

	// Count up barb spawn potential.
	for (const int y : range(map.dim.y))
	{
		for (const int x : range(map.dim.x))
		{
			const ivec2 coord{ x, y };
			const bool potentialSpawn = isPotentialBarbSpawnPlotIgnoringSuppression(map[coord]) && !output.barbSuppressionMap[coord];
			output.barbPotentialSpawnMap[coord] = potentialSpawn;
			if (potentialSpawn)
			{
				// Land
				{
					const auto& cell = cityPathLengthFieldIncludingUnrevealed[coord];
					if (cell.isReachable() && cell.distance <= kMaxBarbSpawnPathDistance)
						++output.cityAnalyses[cell.index].landBarbsExposureLevel;
				}

				// Water
				{
					const auto& cell = coastalWaterBonusBarbPathLengthField[coord];
					if (cell.isReachable()) // && cell.distance <= kMaxBarbSpawnPathDistance)
					{
						const auto& myCitiesCell = cityCoastalPathLengthField[coord];
						if (myCitiesCell.isReachable())
							++output.cityAnalyses[myCitiesCell.index].waterBarbsExposureLevel;
					}
				}
			}
		}
	}

	return output;
}