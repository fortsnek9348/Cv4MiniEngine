#include "SettlingAdvisor.h"

#include <PlayerBotGameBinding/EnumDefs.h>

#include <CommonStuff/range.h>

using namespace mybot;

namespace
{
	int evalSettleLocation(MapGeometry geom, const DynamicArray2D<Plot>& map, ivec2 coord,
		const DynamicArray2D<MultipleSourceDistanceFieldCell>& stepDistanceAnalysis)
	{
		int value = 0;

		// Just count up the food.
		for (const ivec2 dc : kCityWorkPlotCoords)
			if (const auto optWorkCoord = geom.resolve(coord + dc))
				if (stepDistanceAnalysis[*optWorkCoord].distance > 1) // Ignore plots that are right next to an existing city. 
				{
					value += map[*optWorkCoord].yields[EYield::Food] * 2;
					value += (map[*optWorkCoord].bonus != EBonus::None) * 5;
					value += map[*optWorkCoord].isRiverside;
				}
			
		return value;
	}
}

void SettlingAdvisor::update(
	MapGeometry geom, 
	const DynamicArray2D<Plot>& map,
	const DynamicArray2D<PathingPlot>& pathingMap,
	std::span<const ivec2> myCityCoords,
	const DynamicArray2D<MultipleSourceDistanceFieldCell>& stepDistanceAnalysis,
	const IGame& game
)
{
	// Pick a location that is close to an existing city and has high value.
	// The location must also be valid and not be too close to a city.

	const int minStepDistanceToExistingCity = 3;
	const int maxPathLength = 5;
	const int minFoundValue = 5;

	const auto settlingPathPengthAnalysis = computeMultipleSourcePathLengthField({ geom, pathingMap.view() },
		static_cast<PathingPlot::EFlag>(PathingPlot::Unrevealed | PathingPlot::Impassible | PathingPlot::Water | PathingPlot::OwnedByOtherPlayer),
		maxPathLength,
		myCityCoords
	);

	std::optional<ivec2> bestLocation = optTarget;

	// NOTE: When the previous optTarget becomes non-visible, it is stale and hasFoundActionAt isn't reliable. So only invalidate is the plot is also visible.
	//       If the plot is not settleable, we'll find out when the settle arrives.
	if (bestLocation && ((map[*bestLocation].isVisible && !game.hasFoundActionAt(*bestLocation)) || !settlingPathPengthAnalysis[*bestLocation].isReachable()))
		bestLocation = std::nullopt;
	int bestValue = bestLocation ? evalSettleLocation(geom, map, *bestLocation, stepDistanceAnalysis) : minFoundValue;

	for (const int y : heck::range(geom.dim.y))
	{
		for (const int x : heck::range(geom.dim.x))
		{
			const ivec2 coord{ x, y };

			if (stepDistanceAnalysis[coord].distance < minStepDistanceToExistingCity)
				continue;
			if (settlingPathPengthAnalysis[coord].distance > maxPathLength)
				continue;
			if (map[coord].type != EPlotType::Land && map[coord].type != EPlotType::Hills)
				continue;
			if (!game.hasFoundActionAt(coord))
				continue;

			const int value = evalSettleLocation(geom, map, coord, stepDistanceAnalysis);

			if (value > bestValue)
			{
				bestLocation = coord;
				bestValue = value;
			}
		}
	}

	optTarget = bestLocation;
}

int SettlingAdvisor::getSettlerDemand() const
{
	return optTarget ? 1 : 0;
}