#include "CityProduction.h"
#include "CityBuildingProductionAdvisor.h"
#include "AssignmentProblem.h"
#include "UnitProductionDemand.h"
#include "Pathing.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/IGame.h>

#include <algorithm>

using namespace mybot;

namespace
{
	static constexpr int kMaxUnitDemandPercentCities = 80; // rounded up

	static constexpr auto kUrgencyTimeFlexibility = std::to_array({
			1000, // None
			100, // MilitaryPolice,
			100, // NonCombat,
			50, // Escort,
			8, // ShowOfForce
			10, // MilitaryBuildUp,
			4, // UndefendedCity,
			2, // TerritoryDefence,
			1, // Survival,
		});
	static_assert(kUrgencyTimeFlexibility.size() == (size_t)EUnitProductionUrgency::Num);

	// Remember: Bulding final assignment value is around 100.

	constexpr auto kUrgencyPerTurnValue = std::to_array({
		1, // None
		50, // MilitaryPolice,
		80, // NonCombat,
		100, // Escort,
		250, // ShowOfForce
		300, // MilitaryBuildUp,
		500, // UndefendedCity,
		1000, // TerritoryDefence,
		10'000, // Survival,
		});
	static_assert(kUrgencyPerTurnValue.size() == (size_t)EUnitProductionUrgency::Num);

	int computeArrivalTimeScoreModifier(unsigned int averageProductionRate, int gameSpeedScale, int progress, int cost, int moveSteps, int deadlineTurns, EUnitProductionUrgency urgency)
	{
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

	int computeUnitScore(
		const GlobalInfoData& infos,
		const MapGeometry& geom,
		EUnitClass choice,
		ivec2 cityCoord,
		ivec2 target,
		int deadlineTurns,
		unsigned int averageProductionRate,
		int gameSpeedScale,
		int prodProgress,
		EUnitProductionUrgency urgency
	)
	{
		const int urgencyScore = kUrgencyPerTurnValue[std::to_underlying(urgency)];
		const UnitInfo& info = infos.units[choice];
		const int cost = info.productionCost;
		const int distance = info.domain == EDomain::Immobile ? 0 : computeDistance(geom, cityCoord, target, EDistanceMetric::Step);
		const int arrivalTimeScoreModifier = computeArrivalTimeScoreModifier(averageProductionRate, gameSpeedScale, prodProgress, cost, distance / std::max<int>(1, info.moveSteps), deadlineTurns, urgency);
		return urgencyScore * arrivalTimeScoreModifier;
	}
}

std::vector<ProductionChoice> mybot::assignCityProduction(
	IGame& game,
	const GlobalInfoData& infos,
	MapGeometry geom,
	std::span<const City> myCities,
	std::span<const CityBuildingProductionRecomendation> buildingRecommendations,
	std::span<const UnitProductionDemand> productionDemands
)
{
	assert(myCities.size() == buildingRecommendations.size());

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

	// First, generate a list of tasks. Limit demands that are in excess of cities.
	const int maxUnitDemand = cdiv(static_cast<int>(myCities.size() * kMaxUnitDemandPercentCities), 100u);

	std::vector<UnitProductionDemand> limitedDemands; // count == 1 for all
	for (const UnitProductionDemand demand : productionDemands)
	{
		UnitProductionDemand singleDemand = demand;
		singleDemand.count = 1;
		limitedDemands.insert(limitedDemands.end(), demand.count, singleDemand);
	}
	std::ranges::stable_sort(limitedDemands, std::greater(), &UnitProductionDemand::urgency);
	if (static_cast<int>(limitedDemands.size()) > maxUnitDemand)
		limitedDemands.resize(maxUnitDemand);

	// Now generate assignment solver input.
	// A task is either to build a building or a unit.
	// Building values are given, unit values are computed.
	// < buildingRecommendations.size() == building demand index
	// >= buildingRecommendations.size() == limitedDemands index
	const size_t totalTasks = buildingRecommendations.size() + limitedDemands.size();

	DynamicArray2D<int> solverInput(ivec2{ static_cast<int>(myCities.size()), static_cast<int>(totalTasks) });
	for (int workerI = 0; workerI < static_cast<int>(myCities.size()); ++workerI)
		solverInput[{ workerI, workerI }] = buildingRecommendations[workerI].finalAssignmentValue;
	
	for (int workerI = 0; workerI < static_cast<int>(myCities.size()); ++workerI)
	{
		const unsigned int averageProductionRate = myCities[workerI].optInspectableCityInfo->yieldRates[EYield::Production];
		const i16vec2 cityCoord = myCities[workerI].coord;
		for (int unitTaskI = 0; unitTaskI < static_cast<int>(limitedDemands.size()); ++unitTaskI)
		{
			const UnitProductionDemand& demand = limitedDemands[unitTaskI];
			solverInput[{ workerI, static_cast<int>(buildingRecommendations.size() + unitTaskI) }] = computeUnitScore(
				infos,
				geom,
				demand.klass,
				cityCoord,
				demand.target,
				demand.turns,
				averageProductionRate,
				infos.speedInfo.unitProdPercent,
				game.getCityProductionProgress(cityCoord, demand.klass),
				demand.urgency
			);
		}
	}

	// Solve.
	const std::vector<int> taskIndices = computeOptimalAssignment(solverInput.view());

	// Assign.
	std::vector<ProductionChoice> out(myCities.size());
	for (const size_t workerI : range(myCities.size()))
	{
		const size_t taskI = taskIndices[workerI];
		ProductionChoice choice;
		if (taskI >= totalTasks) // -1
			choice = std::monostate();
		else if (taskI < buildingRecommendations.size())
		{
			if (taskI == workerI) // A city should only build its own buildings... Other building tasks have a value of zero, so shouldn't be selected.
				choice = buildingRecommendations[workerI].choice;
			else
				choice = std::monostate();
		}
		else
			choice = limitedDemands[taskI - buildingRecommendations.size()].klass;
		out[workerI] = choice;
		game.tryChangeProduction(myCities[workerI].coord, choice);
	}
	return out;

	//// Generate lists of production choices.
	//std::vector<std::vector<CityBuildChoice>> productionChoices;
	//for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	//{
	//	//game.tryChangeProduction(myCities[workerI].coord, std::monostate());
	//	productionChoices.push_back(game.getCityProductionChoices(myCities[workerI].coord));
	//	// Include current (even if duplicate).
	//	productionChoices.back().push_back({ myCities[workerI].optInspectableCityInfo->productionChoice, myCities[workerI].optInspectableCityInfo->prodBinLevel, myCities[workerI].optInspectableCityInfo->prodBinMax });
	//}
	//
	//const int valueForecastTurns = 100 * infos.speedInfo.buildingProdPercent / 100;
	//
	//// Build assignment solver input.
	//DynamicArray2D<int> solverInput(ivec2{ static_cast<int>(myCities.size()), static_cast<int>(limitedDemands.size()) });
	//for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	//{
	//	const ProductionEvaluator eval{
	//		.infos = infos,
	//		.leader = infos.leaders[activePlayer.leader],
	//		.geom = geom,
	//		.city = myCities[workerI],
	//		.analysis = cityAnalyses[workerI],
	//		.averageProductionRate = std::max(1u, static_cast<unsigned int>(myCities[workerI].optInspectableCityInfo->prodBinRate)),
	//		.valueForecastTurns = valueForecastTurns,
	//	};
	//
	//	for (size_t taskI = 0; taskI < limitedDemands.size(); ++taskI)
	//	{
	//		if (std::ranges::contains(productionChoices[workerI], limitedDemands[taskI].thing))
	//		{
	//			const int value = eval.computeProductionChoiceScore(limitedDemands[taskI]);
	//
	//			solverInput[{ static_cast<int>(workerI), static_cast<int>(taskI) }] = value;
	//		}
	//	}
	//}
	//
	//// Solve.
	//const std::vector<int> taskIndices = computeOptimalAssignment(solverInput.view());
	//
	//// Get solution.
	//std::vector<ProductionDemand> choices;
	//for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	//{
	//	const int taskI = taskIndices[workerI];
	//	if (taskI >= 0 && solverInput[{ static_cast<int>(workerI), taskI }] > 0) // Ignore unusable assignments
	//		choices.push_back(limitedDemands[taskI]);
	//	else
	//		choices.push_back({});
	//}
	//
	//// Produce buildings in cities with low assignment value.
	//for (size_t workerI = 0; workerI < myCities.size(); ++workerI)
	//{
	//	const ProductionEvaluator eval{
	//		.infos = infos,
	//		.leader = infos.leaders[activePlayer.leader],
	//		.geom = geom,
	//		.city = myCities[workerI],
	//		.analysis = cityAnalyses[workerI],
	//		.averageProductionRate = std::max(1u, static_cast<unsigned int>(myCities[workerI].optInspectableCityInfo->prodBinRate)),
	//		.valueForecastTurns = valueForecastTurns,
	//	};
	//
	//	ProductionChoice bestChoice{};
	//	int bestValue{};
	//
	//	for (const ProductionChoice& availChoice : productionChoices[workerI])
	//	{
	//		if (!std::holds_alternative<EUnitClass>(availChoice))
	//		{
	//			const int value = eval.computeProductionChoiceScore({
	//				.thing = availChoice,
	//				.turns = 30,
	//				.count = 1,
	//				.urgency = std::holds_alternative<EProject>(availChoice) ? EProductionUrgency::WorldWonder : EProductionUrgency::None,
	//				});
	//
	//			if (value > bestValue)
	//			{
	//				bestValue = value;
	//				bestChoice = availChoice;
	//			}
	//		}
	//	}
	//
	//	const int taskI = taskIndices[workerI];
	//	const int assignmentValue = taskI >= 0 ? solverInput[{ static_cast<int>(workerI), taskI }] : 0;
	//
	//	if (assignmentValue == 0 || choices[workerI].urgency == EProductionUrgency::None)
	//		game.tryChangeProduction(myCities[workerI].coord, bestChoice);
	//	else
	//		game.tryChangeProduction(myCities[workerI].coord, choices[workerI].thing);
	//}
}