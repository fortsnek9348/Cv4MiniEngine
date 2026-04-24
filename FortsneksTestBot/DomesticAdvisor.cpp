#include "DomesticAdvisor.h"
#include "Pathing.h"
#include "AssignmentProblem.h"

#include <PlayerBotGameBinding/EnumDefs.h>
#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/IGame.h>
#include <PlayerBotGameBinding/EnumStrings.h>

#include <algorithm>
#include <iostream>

using namespace mybot;

// TODO: No wonder control. Cities should not decide to build wonder on their own, as wonders need civ-wide decision.

namespace
{
	constexpr int kMaxBarbSpawnPathDistance = 10;
	constexpr int kMaxWaterBonusBarbThreatDistance = 8; // AI_barbAttackSeaMove, 4 moves of a gallery

	constexpr int kMaxUnitDemandPercentCities = 80; // rounded up

	struct BorderPlotInfo
	{
		i16vec2 coord{};
		std::bitset<kMaxPlayers + 1> adjBits{};
	};

	std::vector< BorderPlotInfo> findBorderPlots(EPlayer activePlayerI, MapGeometry mapGeom, Span2D<const Plot> map)
	{
		std::vector<BorderPlotInfo> borderPlots;
		borderPlots.reserve(100);

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

		for (const int y : range(map.dim.y))
			for (const int x : range(map.dim.x))
				if (map[{ x, y }].owner == activePlayerI)
				{
					BorderPlotInfo info{ .coord = static_cast<i16vec2>(ivec2(x, y)) };
					for (const ivec2 adjD : kAdj)
						if (const auto optAdjCoord = mapGeom.resolve(ivec2(x, y) + adjD))
							info.adjBits[map[*optAdjCoord].owner + 1] = true;
					info.adjBits[activePlayerI + 1] = false;

					if (info.adjBits.any())
						borderPlots.emplace_back(info);
				}

		return borderPlots;
	}

	std::vector<DomesticAdvisor::CityAnalysis*> getCityAnalysisPointers(std::map<i16vec2, DomesticAdvisor::CityAnalysis>& analysisMap, std::span<const City> myCities,
		int secondRingCulture)
	{
		std::vector<DomesticAdvisor::CityAnalysis*> analysises;
		analysises.reserve(myCities.size());

		std::map<i16vec2, DomesticAdvisor::CityAnalysis> newAnalysisMap;
		for (const City& city : myCities)
		{
			auto& analysis = [&]() -> DomesticAdvisor::CityAnalysis& {
				if (const auto it = analysisMap.find(city.coord); it != analysisMap.end())
					return newAnalysisMap.insert(analysisMap.extract(it)).position->second;
				else
					return newAnalysisMap[city.coord];
				}();

			analysis.name = city.name;
			analysis.hasCivOnBorder.reset();
			analysis.isInterior = true;
			analysis.hasSecondRing = city.optInspectableCityInfo->culture >= secondRingCulture;

			analysises.push_back(&analysis);
		}

		std::swap(analysisMap, newAnalysisMap);

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

void DomesticAdvisor::analyse(
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

	const std::vector<BorderPlotInfo> borderPlots = findBorderPlots(activePlayerI, mapGeom, map);
	const std::vector<DomesticAdvisor::CityAnalysis*> cityAnalysises = getCityAnalysisPointers(mAnalysises, myCities, infos.cultureLevels[2].culture);
	
	// For any city closest to a border plot, set isInterior.
	// For any city closest to a border plot that is adj to a rival civ, update hasCivOnBorder.
	for (const BorderPlotInfo& borderPlotInfo : borderPlots)
	{
		auto bits = borderPlotInfo.adjBits;
		//const bool bordersUnowned = bits[0];
		//bits[0] = false;
		//const bool bordersBarbs = bits[kMaxPlayers];
		//bits[kMaxPlayers] = false;
		//const bool bordersRivalCivs = bits.any();

		if (cityPathLengthField[borderPlotInfo.coord].isReachable())
		{
			static_assert(kMaxCivPlayers <= 64);
			const std::bitset<kMaxCivPlayers> adjCivMask((bits >> 1).to_ullong());
			const size_t closestCityI = cityPathLengthField[borderPlotInfo.coord].index;
			cityAnalysises[closestCityI]->hasCivOnBorder |= adjCivMask;
			cityAnalysises[closestCityI]->isInterior = false;
		}
	}

	// Find plots where barb spawns are suppressed.
	mBarbSuppressionMap = DynamicArray2D<bool>(mapGeom.dim);
	// 5x5 around all visible units.
	for (const Unit& unit : allVisibleUnits)
		mBarbSuppressionMap[unit.coord] = true;
	dilate(mapGeom, mBarbSuppressionMap);
	// 3x3 around all owned plots
	for (const int y : range(map.dim.y))
		for (const int x : range(map.dim.x))
			if (map[{ x, y }].owner != kNoPlayer)
				mBarbSuppressionMap[{ x, y }] = true;
	dilate(mapGeom, mBarbSuppressionMap); // This will also do 5x5 around units.
	

	mBarbPotentialSpawnMap = DynamicArray2D<bool>(mapGeom.dim);

	// Count up barb spawn potential.
	for (const int y : range(map.dim.y))
	{
		for (const int x : range(map.dim.x))
		{
			const ivec2 coord{ x, y };
			const bool potentialSpawn = isPotentialBarbSpawnPlotIgnoringSuppression(map[coord]) && !mBarbSuppressionMap[coord];
			mBarbPotentialSpawnMap[coord] = potentialSpawn;
			if (potentialSpawn)
			{
				// Land
				{
					const auto& cell = cityPathLengthFieldIncludingUnrevealed[coord];
					if (cell.isReachable() && cell.distance <= kMaxBarbSpawnPathDistance)
						++cityAnalysises[cell.index]->landBarbsExposureLevel;
				}

				// Water
				{
					const auto& cell = coastalWaterBonusBarbPathLengthField[coord];
					if (cell.isReachable()) // && cell.distance <= kMaxBarbSpawnPathDistance)
					{
						const auto& myCitiesCell = cityCoastalPathLengthField[coord];
						if (myCitiesCell.isReachable())
							++cityAnalysises[myCitiesCell.index]->waterBarbsExposureLevel;
					}
				}
			}
		}
	}
}


const DomesticAdvisor::CityAnalysis& DomesticAdvisor::getCityAnalysis(i16vec2 coord) const
{
	return mAnalysises.at(coord);
}

bool DomesticAdvisor::areBarbsSuppressed(ivec2 coord) const
{
	return mBarbSuppressionMap[coord];
}
bool DomesticAdvisor::isPotentialBarbSpawnAt(ivec2 coord) const
{
	return mBarbPotentialSpawnMap[coord];
}

namespace
{
	int computeArrivalTimeScoreModifier(unsigned int averageProductionRate, int gameSpeedScale, int progress, int cost, int moveSteps, int deadlineTurns, EProductionUrgency urgency)
	{
		static constexpr auto kUrgencyTimeFlexibility = std::to_array({
			1000, // None
			100, // MilitaryPolice,
			100, // NonCombat,
			50, // Escort,
			5, // WorldWonder,
			8, // ShowOfForce
			10, // MilitaryBuildUp,
			4, // UndefendedCity,
			2, // TerritoryDefence,
			1, // Survival,
			});
		//static constexpr auto kUrgencyTimeSensitivity = std::to_array({
		//	1, // None
		//	2, // MilitaryPolice,
		//	2, // NonCombat,
		//	4, // Escort,
		//	50, // WorldWonder,
		//	100, // MilitaryBuildUp,
		//	200, // UndefendedCity,
		//	500, // TerritoryDefence,
		//	1000, // Survival,
		//	});
		static_assert(kUrgencyTimeFlexibility.size() == (size_t)EProductionUrgency::Num);

		const int productionTurns = cdiv(cost - progress, averageProductionRate);
		const int arrivalTime = productionTurns + moveSteps;
		//const int arrivalTimeScore = (deadlineTurns - arrivalTime) * kUrgencyTimeSensitivity[std::to_underlying(urgency)];

		// (a + k) / (b + k)
		const int flexibility = kUrgencyTimeFlexibility[std::to_underlying(urgency)];
		// Above the threshold, the score will always be close to kArrivalTimeScoreScale.
		//constexpr int kArrivalTimeScoreThreshold = 10000;
		constexpr int kArrivalTimeScoreScale = 100;
		// arrivalTimeScore is kArrivalTimeScoreScale if unit arrives on time. Less flexibility will cause a bigger deviation if the unit does not.
		return (deadlineTurns + flexibility * gameSpeedScale / 100) * kArrivalTimeScoreScale / std::max(1, arrivalTime + flexibility * gameSpeedScale / 100);

	}

	constexpr auto kUrgencyPerTurnValue = std::to_array({
		1, // None
		50, // MilitaryPolice,
		80, // NonCombat,
		100, // Escort,
		200, // WorldWonder,
		250, // ShowOfForce
		300, // MilitaryBuildUp,
		500, // UndefendedCity,
		1000, // TerritoryDefence,
		10'000, // Survival,
	});
	static_assert(kUrgencyPerTurnValue.size() == (size_t)EProductionUrgency::Num);

	int computeUnitScore(
		const GlobalInfoData& infos,
		const MapGeometry& geom,
		EUnitClass choice,
		ivec2 cityCoord,
		ivec2 target,
		uint16_t deadlineTurns,
		unsigned int averageProductionRate,
		int gameSpeedScale,
		int prodProgress,
		EProductionUrgency urgency
	)
	{
		const int urgencyScore = kUrgencyPerTurnValue[std::to_underlying(urgency)];
		const UnitInfo& info = infos.units[choice];
		const int cost = info.productionCost;
		const int distance = info.domain == EDomain::Immobile ? 0 : computeDistance(geom, cityCoord, target, EDistanceMetric::Step);
		const int arrivalTimeScoreModifier = computeArrivalTimeScoreModifier(averageProductionRate, gameSpeedScale, prodProgress, cost, distance / std::max<int>(1, info.moveSteps), deadlineTurns, urgency);
		return urgencyScore * arrivalTimeScoreModifier;
	}

	//struct BuildingActualEffect
	//{
	//	// NOTE: Yields includes the effects of plot changes, such as lighthouse.
	//	std::array<int8_t, EYield::Num> yields{}; // food, hammers, commerce
	//	// NOTE: Includes the effect of extra trade routes and courthouse.
	//	std::array<int8_t, ECommerce::Num> commerce{}; // gold, beakers, culture, spy points
	//	std::array<int8_t, ESpecialist::Num> extraSpecialistSlots{};
	//	int8_t specialistPoints{}; // free specialists and modifiers
	//	int8_t happiness{}; // actual change
	//	int8_t healthiness{}; // actual change
	//	int8_t defence{};
	//	bool providesPower : 1 {};
	//
	//	// NOTE: Effects that are handled specially: Unit XP, espionage defence, free promotions
	//};

	int getBuildingBaseValue(
		[[maybe_unused]] const GlobalInfoData& infos,
		const LeaderInfo& leader,
		const City& city,
		const DomesticAdvisor::CityAnalysis& analysis,
		int averageProductionRate,
		EBuildingClass choice)
	{
		const auto& cityData = *city.optInspectableCityInfo;

		const bool isCultureUseful = analysis.hasCivOnBorder.any() || (!analysis.hasSecondRing && cityData.commerceRates[ECommerce::Culture] <= 0);

		switch (choice)
		{
		case EBuildingClass::Granary:
			return 300;
		case EBuildingClass::Lighthouse:
			// TODO: Should adjust based on number of water plots.
			return 200;

		case EBuildingClass::Obelisk:
			return std::ranges::contains(leader.traits, ELeaderTrait::Charismatic) * 200 + isCultureUseful * 200;

		case EBuildingClass::Barracks:
			// Low value. Other decision code will build this if the city has unit demand.
			return 50;
		case EBuildingClass::Stable:
			// Low value. Other decision code will build this if the city has unit demand.
			return 40;

		case EBuildingClass::Library:
		case EBuildingClass::University:
			return 100 + analysis.hasCivOnBorder.any() * 100;
		case EBuildingClass::Observatory:
		case EBuildingClass::Laboratory:
			return 100;

		case EBuildingClass::Forge:
			return averageProductionRate * 125 * 20 / 100;
		case EBuildingClass::Levee:
			// TODO: Build forge first or not?
			return 300;
		case EBuildingClass::Factory:
			return averageProductionRate * (125 + 25 * city.isPowered) * 20 / 100;
		case EBuildingClass::IronWorks:
			return averageProductionRate * 200 * 20 / 100;

		case EBuildingClass::Theatre:
		case EBuildingClass::BroadcastTower:
			return analysis.hasCivOnBorder.any() * 400;

		case EBuildingClass::Colosseum:
			return cityData.happiness <= 0 ? 300 : 0;
		
		case EBuildingClass::Market:
		case EBuildingClass::Grocer:
		case EBuildingClass::Bank:
			return 50;

		case EBuildingClass::GreatPalace: // Forbidden
			// TODO: Should determine the best city to build this in.
			return 200;
		case EBuildingClass::Versailles:
			// TODO: Should determine the best city to build this in.
			return 300;
		case EBuildingClass::Walls:
		case EBuildingClass::Castle:
			return 0;

		
		
		case EBuildingClass::Bunker:
		case EBuildingClass::BombShelter:
			// TODO: These will have a useful if rivals ever become a modern threat.
			return 0;

		
		
		case EBuildingClass::Hospital:
		case EBuildingClass::RecyclingCenter:
		case EBuildingClass::Aqueduct:
		case EBuildingClass::PublicTransportation:
			return cityData.healthiness < 0 ? 200 : 0;
		case EBuildingClass::Supermarket:
			return (cityData.healthiness < 0 ? 200 : 0) + 100;
		
		case EBuildingClass::Harbor:
			return cityData.healthiness < 0 ? 200 : 100;

		case EBuildingClass::CustomHouse:
			// TODO: Only useful if not in mercantilism.
			return 50;

		case EBuildingClass::Drydock:
			// TODO: Build this if doing naval stuff.
			return 0;

		case EBuildingClass::Airport:
			return 100;

	

		
		case EBuildingClass::CoalPlant:
		case EBuildingClass::NuclearPlant:
			return !city.isPowered * 80;
		case EBuildingClass::HydroPlant:
			return !city.isPowered * 100;

		case EBuildingClass::IndustrialPark:
			return 100;

		case EBuildingClass::Courthouse:
			return cityData.maintainence * 100;

		case EBuildingClass::Jail:
			return cityData.percentAngerContributions[City::InspectableCityInfo::WarWeariness] * 100;
		
		case EBuildingClass::MtRushmore:
			return cityData.percentAngerContributions[City::InspectableCityInfo::WarWeariness] * 100;

		case EBuildingClass::IntelligenceAgency:
		case EBuildingClass::NationalSecurity:
			return 50;

		case EBuildingClass::JewishTemple:
		case EBuildingClass::ChristianTemple:
		case EBuildingClass::IslamicTemple:
		case EBuildingClass::HinduTemple:
		case EBuildingClass::BuddhistTemple:
		case EBuildingClass::ConfucianTemple:
		case EBuildingClass::TaoistTemple:
			return (cityData.happiness <= 0) * 200 + isCultureUseful * 200;

		case EBuildingClass::JewishCathedral:
		case EBuildingClass::ChristianCathedral:
		case EBuildingClass::IslamicCathedral:
		case EBuildingClass::HinduCathedral:
		case EBuildingClass::BuddhistCathedral:
		case EBuildingClass::ConfucianCathedral:
		case EBuildingClass::TaoistCathedral:
			return (cityData.happiness <= 0) * 200 + isCultureUseful * 200;

		case EBuildingClass::JewishMonastery:
		case EBuildingClass::ChristianMonastery:
		case EBuildingClass::IslamicMonastery:
		case EBuildingClass::HinduMonastery:
		case EBuildingClass::BuddhistMonastery:
		case EBuildingClass::ConfucianMonastery:
		case EBuildingClass::TaoistMonastery:
			return 50 + isCultureUseful * 200;

		case EBuildingClass::HeroicEpic:
		case EBuildingClass::WestPoint:
			return averageProductionRate * 10;

		case EBuildingClass::NationalEpic:
			return 50;

		case EBuildingClass::GlobeTheatre:
		case EBuildingClass::NationalPark:
			return city.pop * 20;

		case EBuildingClass::Hermitage:
			return analysis.hasCivOnBorder.any() * 300;

		case EBuildingClass::OxfordUniversity:
		case EBuildingClass::WallStreet:
			return city.isCapital * 300;
		
		
		
		
		case EBuildingClass::RedCross:
			return 200;

		case EBuildingClass::MoaiStatues:
			return 50;

		case EBuildingClass::Pyramid:
			return 80;

		case EBuildingClass::Stonehenge:
			return !std::ranges::contains(leader.traits, ELeaderTrait::Creative) * 50 + 30;

		case EBuildingClass::Oracle:
		case EBuildingClass::TajMahal:
		case EBuildingClass::GreatLibrary:
		case EBuildingClass::Colossus:
			return 200;

		case EBuildingClass::Parthenon:
		case EBuildingClass::Hollywood:
		case EBuildingClass::Rocknroll:
		case EBuildingClass::Broadway:
		case EBuildingClass::EiffelTower:
		case EBuildingClass::GreatLighthouse:
			return 100;
		
		case EBuildingClass::HangingGarden:
		case EBuildingClass::AngkorWat:
		case EBuildingClass::HagiaSophia:
		case EBuildingClass::ChichenItza:
		case EBuildingClass::SistineChapel:
		case EBuildingClass::NotreDame:
		case EBuildingClass::SpiralMinaret:
		case EBuildingClass::Kremlin:
		case EBuildingClass::StatueOfLiberty:
		case EBuildingClass::GreatDam:
		case EBuildingClass::Pentagon:
		case EBuildingClass::UnitedNations:
		case EBuildingClass::Artemis:
		case EBuildingClass::Sankore:
		case EBuildingClass::GreatWall:
		case EBuildingClass::StatueOfZeus:
		case EBuildingClass::MausoleumOfMaussollos:
		case EBuildingClass::CristoRedentor:
		case EBuildingClass::ShwedagonPaya:
		case EBuildingClass::ApostolicPalace:
			return 50;

		case EBuildingClass::SpaceElevator:
			// Corp HQs, MilitaryAcademyScotlandYard, religion shrines, Academy
		default:
			return 0;
		}
	}

	int computeBuildingScore(
		const GlobalInfoData& infos,
		const LeaderInfo& leader,
		const City& city,
		const DomesticAdvisor::CityAnalysis& analysis,
		EBuildingClass choice,
		// NOTE: Non-world-wonder buildings can have a deadline too if they are a prerequisite.
		uint16_t deadlineTurns,
		int valueForcastTurns,
		unsigned int averageProductionRate,
		int gameSpeedScale,
		int prodProgress,
		EProductionUrgency urgency
	)
	{
		const BuildingClassInfo& klassInfo = infos.buildingClasses[choice];
		const BuildingInfo& typeInfo = infos.buildings[klassInfo.activeType];

		const int urgencyScore = kUrgencyPerTurnValue[std::to_underlying(urgency)];
		const int cost = typeInfo.productionCost;
		const int productionTurns = cdiv(cost - prodProgress, averageProductionRate);
		const int arrivalTimeScoreModifier = computeArrivalTimeScoreModifier(averageProductionRate, gameSpeedScale, prodProgress, cost, 0, deadlineTurns, urgency);
		const int baseValue = getBuildingBaseValue(infos, leader, city, analysis, averageProductionRate, choice);

		//if (choice == EBuildingClass::Barracks || choice == EBuildingClass::Stonehenge)
		//{
		//	std::clog << cvbot::kBuildingClassNames[choice] << ": " << urgencyScore << ", " << arrivalTimeScoreModifier << ", " << baseValue
		//		<< ", " << (baseValue * (valueForcastTurns - productionTurns))
		//		<< ", " << (urgencyScore * (baseValue * (valueForcastTurns - productionTurns)) * arrivalTimeScoreModifier)
		//		<< '\n';
		//}

		return urgencyScore * (baseValue * std::max(1, valueForcastTurns - productionTurns)) * arrivalTimeScoreModifier / 200;
	}

	int computeProjectScore(
		const GlobalInfoData& infos,
		[[maybe_unused]] const City& city,
		[[maybe_unused]] const DomesticAdvisor::CityAnalysis& analysis,
		EProject choice,
		uint16_t deadlineTurns,
		int valueForcastTurns,
		unsigned int averageProductionRate,
		int gameSpeedScale,
		int prodProgress,
		EProductionUrgency urgency
	)
	{
		const ProjectInfo& info = infos.projects[choice];

		const int urgencyScore = kUrgencyPerTurnValue[std::to_underlying(urgency)];
		const int cost = info.productionCost;
		const int productionTurns = cdiv(cost - prodProgress, averageProductionRate);
		const int arrivalTimeScoreModifier = computeArrivalTimeScoreModifier(averageProductionRate, gameSpeedScale, prodProgress, cost, 0, deadlineTurns, urgency);
		int baseScore{};

		switch (choice)
		{
		case EProject::ManhattanProject:
		case EProject::Sdi:
			// Not handling nukes...
			baseScore = 0;
			break;
		default:
			baseScore = 200 + (info.optVictoryPrereq != EVictory::None) * 500;
			break;
		}

		return urgencyScore * (baseScore * std::max(1, valueForcastTurns - productionTurns)) * arrivalTimeScoreModifier / 200;
	}

	int computeProcessScore(
		EProcess choice,
		unsigned int averageProductionRate
	)
	{
		ECommerce commerce{};
		switch (choice)
		{
		default:
		case EProcess::Wealth: commerce = ECommerce::Gold; break;
		case EProcess::Research: commerce = ECommerce::Research; break;
		case EProcess::Culture: commerce = ECommerce::Culture; break;
		}

		const int commerceOutput = averageProductionRate;

		return commerce == ECommerce::Culture ? 0 : 5 * commerceOutput + (commerce == ECommerce::Gold) * 10;
	}

	struct ProductionEvaluator
	{
		const GlobalInfoData& infos;
		const LeaderInfo& leader;
		const MapGeometry& geom;
		const City& city;
		const DomesticAdvisor::CityAnalysis& analysis;
		unsigned int averageProductionRate;
		int valueForecastTurns;

		int computeProductionChoiceScore(ProductionDemand demand) const
		{
			const int progress = demand.thing == city.optInspectableCityInfo->productionChoice ? city.optInspectableCityInfo->prodBinLevel : 0;
			if (const auto* const unitClass = std::get_if<EUnitClass>(&demand.thing))
				return computeUnitScore(infos, geom, *unitClass, city.coord, demand.target, demand.turns, averageProductionRate, infos.speedInfo.unitProdPercent, progress, demand.urgency);
			if (const auto* const buildingClass = std::get_if<EBuildingClass>(&demand.thing))
				return computeBuildingScore(infos, leader, city, analysis, *buildingClass, demand.turns, valueForecastTurns, averageProductionRate, infos.speedInfo.buildingProdPercent, progress, demand.urgency);
			if (const auto* const project = std::get_if<EProject>(&demand.thing))
				return computeProjectScore(infos, city, analysis, *project, demand.turns, valueForecastTurns, averageProductionRate, infos.speedInfo.projectProdPercent, progress, demand.urgency);
			if (const auto* const process = std::get_if<EProcess>(&demand.thing))
				return computeProcessScore(*process, averageProductionRate);
			return 0;
		}
	};

}

void DomesticAdvisor::assignProduction(
	IGame& game,
	const GlobalInfoData& infos,
	const MapGeometry& geom,
	const Player& activePlayer,
	std::span<const City> myCities,
	std::span<const ProductionDemand> productionDemands
)
{
	// So we've got a bunch of production demands and we somehow have to spread them out across our cities.
	// This is similar to the unit assignment problem in MilitaryAdvisor.
	// But there's a difference: Progress from one task cannot be transferred over to another task
	//     (ie: if you interrupt production, the hammers will probably end up wasted as tasks get reassigned).
	// In contrast to units, that if reassigned, will keep most of their "progress" as movement naturally penalises progress towards some other target.
	// The greedy algorithm, if used for city production, won't know when it discards the progress of another task.
	// Two solutions:
	//     * Subtract value from a candidate task assignment according to a penalty. This would act as a tie-breaker.
	//     * Global optimisation: https://en.wikipedia.org/wiki/Hungarian_method
	//          * With this method, instead of penalising task reassignment, a task would get a natural bonus from existing progress.
	// 
	// Task assignment scoring:
	//     * +score for proximity to target (but reduce the effect on units that are not time-critcal)
	//     * +score for quick production (consider the case where unit demand >>> num cities: let's not waste production in weak cities that can't be used in time)
	//     * +score for existing production
	//         * All of the above could be combined into a "+score for getting unit to target earlier".
	//     * +score for military units from city with barracks/GG
	//     * +score for various building bonuses/process utility
	//     * +score for food-consuming production in cities at the growth cap
	//
	// Necessarily, to score a task of producing a unit with a target, for all cities, you'll need a path distance map for each city.
	//     But this could be approximated by using the A* landmarks heuristic. For each pathing area, pick N border cities as landmarks.
	//     Or, just use straight-line distance.
	//     Even better, produce an acceleration structure for these arbitrary plot-plot path length queries.
	//         Decompose map into convex regions.
	//             Triangulate then greedy combine?
	//             Use plot centers? Marching squares?
	//             Most domestic queries should be contained in one region.
	//         Resurrect SIMD Block A*?
	//         Dilate to ignore small lakes/mountains.
	//         Compute target-city distances *before* scoring candidate task assignments.
	//
	// Performance optimisation:
	//     * At first, try to assign demands to cities according to existing production. If no higher priority is left, cull those cities and tasks from solving.
	//     * Don't give general building/process production to the assignment solver. Decide building/process production afterwards.
	//         * This doesn't include world wonders which would be given as demands and can be built in only one city.
	
	// Anyway...

	// First, generate a list of tasks. Limit demands that are in excess of cities.
	const int maxUnitDemand = cdiv(static_cast<int>(myCities.size() * kMaxUnitDemandPercentCities), 100u);

	std::vector<ProductionDemand> limitedDemands; // count == 1 for all
	for (const ProductionDemand demand : productionDemands)
	{
		ProductionDemand singleDemand = demand;
		singleDemand.count = 1;
		limitedDemands.insert(limitedDemands.end(), demand.count, singleDemand);
	}
	std::ranges::stable_sort(limitedDemands, std::greater(), &ProductionDemand::urgency);
	if (limitedDemands.size() > maxUnitDemand)
		limitedDemands.resize(maxUnitDemand);

	// Generate lists of production choices.
	std::vector<std::vector<CityBuildChoice>> productionChoices;
	for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	{
		//game.tryChangeProduction(myCities[workerI].coord, std::monostate());
		productionChoices.push_back(game.getCityProductionChoices(myCities[workerI].coord));
		// Include current (even if duplicate).
		productionChoices.back().push_back({ myCities[workerI].optInspectableCityInfo->productionChoice, myCities[workerI].optInspectableCityInfo->prodBinLevel, myCities[workerI].optInspectableCityInfo->prodBinMax });
	}

	const int valueForecastTurns = 50 * infos.speedInfo.buildingProdPercent / 100;

	// Build assignment solver input.
	DynamicArray2D<int> solverInput(ivec2{ static_cast<int>(myCities.size()), static_cast<int>(limitedDemands.size()) });
	for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	{
		const ProductionEvaluator eval{
			.infos = infos,
			.leader = infos.leaders[activePlayer.leader],
			.geom = geom,
			.city = myCities[workerI],
			.analysis = mAnalysises.at(myCities[workerI].coord),
			.averageProductionRate = std::max(1u, static_cast<unsigned int>(myCities[workerI].optInspectableCityInfo->prodBinRate)),
			.valueForecastTurns = valueForecastTurns,
		};

		for (size_t taskI = 0; taskI < limitedDemands.size(); ++taskI)
		{
			if (std::ranges::contains(productionChoices[workerI], limitedDemands[taskI].thing))
			{
				const int value = eval.computeProductionChoiceScore(limitedDemands[taskI]);

				solverInput[{ static_cast<int>(workerI), static_cast<int>(taskI) }] = value;
			}
		}
	}

	// Solve.
	const std::vector<int> taskIndices = computeOptimalAssignment(solverInput.view());

	// Get solution.
	std::vector<ProductionDemand> choices;
	for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	{
		const int taskI = taskIndices[workerI];
		if (taskI >= 0 && solverInput[{ static_cast<int>(workerI), taskI }] > 0) // Ignore unusable assignments
			choices.push_back(limitedDemands[taskI]);
		else
			choices.push_back({});
	}

	// Produce buildings in cities with low assignment value.
	for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	{
		const ProductionEvaluator eval{
			.infos = infos,
			.leader = infos.leaders[activePlayer.leader],
			.geom = geom,
			.city = myCities[workerI],
			.analysis = mAnalysises.at(myCities[workerI].coord),
			.averageProductionRate = std::max(1u, static_cast<unsigned int>(myCities[workerI].optInspectableCityInfo->prodBinRate)),
			.valueForecastTurns = valueForecastTurns,
		};

		ProductionChoice bestChoice{};
		int bestValue{};

		for (const ProductionChoice& availChoice : productionChoices[workerI])
		{
			if (!std::holds_alternative<EUnitClass>(availChoice))
			{
				const int value = eval.computeProductionChoiceScore({
					.thing = availChoice,
					.turns = 10,
					.count = 1,
					.urgency = EProductionUrgency::None,
					});

				if (value > bestValue)
				{
					bestValue = value;
					bestChoice = availChoice;
				}
			}
		}

		const int taskI = taskIndices[workerI];
		const int assignmentValue = taskI >= 0 ? solverInput[{ static_cast<int>(workerI), taskI }] : 0;

		if (assignmentValue == 0 || choices[workerI].urgency == EProductionUrgency::None)
			game.tryChangeProduction(myCities[workerI].coord, bestChoice);
		else
			game.tryChangeProduction(myCities[workerI].coord, choices[workerI].thing);
	}
}