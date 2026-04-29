#include "CityBuildingProductionAdvisor.h"
#include "DomesticAnalysis.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/EnumDefs.h>
#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/IGame.h>

#include <algorithm>
#include <map>
#include <climits>

using namespace mybot;

namespace
{
	constexpr bool kEnableNonVictoryLimitedBuildings = false;
	//constexpr int kValueForecastTurns = 100; // To be scaled

	int getBuildingBaseValue(
		[[maybe_unused]] const GlobalInfoData& infos,
		const LeaderInfo& leader,
		const City& city,
		const CityAnalysis& analysis,
		EBuildingClass choice)
	{
		const auto& cityData = *city.optInspectableCityInfo;

		const bool hasBorderingCivs = std::ranges::any_of(analysis.rivalCivBorderPlotCounts, std::identity());
		const bool isCultureUseful = hasBorderingCivs || (!analysis.hasSecondRing && cityData.commerceRates[ECommerce::Culture] <= 0);

		const int averageProductionRate = city.optInspectableCityInfo->prodBinRate;

		switch (choice)
		{
		case EBuildingClass::Granary:
			return 200;
		case EBuildingClass::Lighthouse:
			// TODO: Should adjust based on number of water plots.
			return 190;

		case EBuildingClass::Obelisk:
			return std::ranges::contains(leader.traits, ELeaderTrait::Charismatic) * 200 + isCultureUseful * 200;

		case EBuildingClass::Barracks:
		case EBuildingClass::Stable:
			return 0;

		case EBuildingClass::Library:
		case EBuildingClass::University:
			return 100 + hasBorderingCivs * 100;
		case EBuildingClass::Observatory:
		case EBuildingClass::Laboratory:
			return 100;

		case EBuildingClass::Forge:
			//return averageProductionRate * 50;
			return 150;
		case EBuildingClass::Levee:
			// TODO: Build forge first or not?
			return 120;
		case EBuildingClass::Factory:
			//return averageProductionRate * 40 + city.isPowered * 50;
			//return 60 + city.isPowered * 20;
			return 100;
		case EBuildingClass::IronWorks:
			return averageProductionRate * 30;

		case EBuildingClass::Theatre:
		case EBuildingClass::BroadcastTower:
			return hasBorderingCivs * 400;

		case EBuildingClass::Colosseum:
			//return cityData.happiness <= 0 ? 300 : 0;
			return 0;

		//case EBuildingClass::Market:
		//	return city.optInspectableCityInfo->baseCommerceRateTimes100WithZeroSlider + (cityData.happiness < 0) * 50;
		//case EBuildingClass::Grocer:
		//	return city.optInspectableCityInfo->baseCommerceRateTimes100WithZeroSlider + (cityData.healthiness < 0) * 50;
		//case EBuildingClass::Bank:
		//	return city.optInspectableCityInfo->baseCommerceRateTimes100WithZeroSlider / 2;
		case EBuildingClass::Market:
		case EBuildingClass::Grocer:
		case EBuildingClass::Bank:
			return 0;

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


			// These only get built if city is unhealthy.
		case EBuildingClass::Hospital:
		case EBuildingClass::RecyclingCenter:
		case EBuildingClass::Aqueduct:
		case EBuildingClass::PublicTransportation:
			return (cityData.healthiness < 0) * 200;

			// These have some other slight use.
		case EBuildingClass::Supermarket:
			return (cityData.healthiness < 0) * 200 + 100;

		case EBuildingClass::Harbor:
			return (cityData.healthiness < 0) * 100 + 100;

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
			return std::min<int>(cityData.maintainenceCents, 500);

		case EBuildingClass::Jail:
			return cityData.percentAngerContributions[City::InspectableCityInfo::WarWeariness] * 100;

		case EBuildingClass::MtRushmore:
			return cityData.percentAngerContributions[City::InspectableCityInfo::WarWeariness] * 100;

		case EBuildingClass::IntelligenceAgency:
		case EBuildingClass::NationalSecurity:
			return 0;

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
			return city.optInspectableCityInfo->prodBinRate * 30;
		case EBuildingClass::WestPoint:
			return city.optInspectableCityInfo->prodBinRate * 10;
			//return averageProductionRate * 10;


		case EBuildingClass::NationalEpic:
			return 50;

		case EBuildingClass::GlobeTheatre:
		case EBuildingClass::NationalPark:
			return city.pop * 20;

		case EBuildingClass::Hermitage:
			return hasBorderingCivs * 300;

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

			// World wonders. For now, we don't really know the civ-wide value.
		case EBuildingClass::Oracle:
		case EBuildingClass::TajMahal:
		case EBuildingClass::GreatLibrary:
		case EBuildingClass::Colossus:
		case EBuildingClass::Parthenon:
		case EBuildingClass::Hollywood:
		case EBuildingClass::Rocknroll:
		case EBuildingClass::Broadway:
		case EBuildingClass::EiffelTower:
		case EBuildingClass::SistineChapel:
			return 200;

		case EBuildingClass::GreatLighthouse:
		case EBuildingClass::HangingGarden:
		case EBuildingClass::NotreDame:
		case EBuildingClass::StatueOfLiberty:
		case EBuildingClass::GreatDam:
		case EBuildingClass::MausoleumOfMaussollos:
			return 150;
		
		case EBuildingClass::AngkorWat:
		case EBuildingClass::HagiaSophia:
		case EBuildingClass::ChichenItza:
		case EBuildingClass::SpiralMinaret:
		case EBuildingClass::Kremlin:
		case EBuildingClass::Pentagon:
		case EBuildingClass::UnitedNations:
		case EBuildingClass::Artemis:
		case EBuildingClass::Sankore:
		case EBuildingClass::GreatWall:
		case EBuildingClass::StatueOfZeus:
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

	//int computeBuildingScore(
	//	const GlobalInfoData& infos,
	//	const LeaderInfo& leader,
	//	const City& city,
	//	const CityAnalysis& analysis,
	//	EBuildingClass choice,
	//	// NOTE: Non-world-wonder buildings can have a deadline too if they are a prerequisite.
	//	//uint16_t deadlineTurns,
	//	int valueForecastTurns,
	//	int remainingTurns
	//	//int gameSpeedScale,
	//)
	//{
	//	//const BuildingClassInfo& klassInfo = infos.buildingClasses[choice];
	//	//const BuildingInfo& typeInfo = infos.buildings[klassInfo.activeType];
	//
	//	const int numTurnsBuilt = std::max(1, valueForecastTurns - remainingTurns);
	//	const int baseValue = getBuildingBaseValue(infos, leader, city, analysis, choice);
	//
	//	//if (choice == EBuildingClass::Barracks || choice == EBuildingClass::Stonehenge)
	//	//{
	//	//	std::clog << cvbot::kBuildingClassNames[choice] << ": " << urgencyScore << ", " << arrivalTimeScoreModifier << ", " << baseValue
	//	//		<< ", " << (baseValue * (valueForcastTurns - productionTurns))
	//	//		<< ", " << (urgencyScore * (baseValue * (valueForcastTurns - productionTurns)) * arrivalTimeScoreModifier)
	//	//		<< '\n';
	//	//}
	//
	//	return baseValue * numTurnsBuilt;
	//}

	int getProjectBaseValue(const GlobalInfoData& infos, EProject choice)
	{
		const ProjectInfo& info = infos.projects[choice];

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

		return baseScore;
	}

	//int computeProjectScore(
	//	const GlobalInfoData& infos,
	//	[[maybe_unused]] const City& city,
	//	[[maybe_unused]] const CityAnalysis& analysis,
	//	EProject choice,
	//	//uint16_t deadlineTurns,
	//	int valueForecastTurns,
	//	int remainingTurns
	//	//int gameSpeedScale,
	//)
	//{
	//	const int numTurnsBuilt = std::max(1, valueForecastTurns - remainingTurns);
	//	const int baseScore = getProjectPerTurnValue(infos, choice);
	//	return baseScore * numTurnsBuilt;
	//}

	int computeProcessScore(
		EProcess choice,
		[[maybe_unused]] unsigned int averageProductionRate
	)
	{
		//ECommerce commerce{};
		//switch (choice)
		//{
		//default:
		//case EProcess::Wealth: commerce = ECommerce::Gold; break;
		//case EProcess::Research: commerce = ECommerce::Research; break;
		//case EProcess::Culture: commerce = ECommerce::Culture; break;
		//}
		//
		//const int commerceOutput = averageProductionRate;
		//
		//return commerce == ECommerce::Culture ? 0 : 5 * commerceOutput + (commerce == ECommerce::Gold) * 10;

		switch (choice)
		{
		case EProcess::Wealth: return 50;
		case EProcess::Research: return 20;
		case EProcess::Culture:
		default:
			return 10;
		}
	}

	static int stabliseProduction(int baseValue, int progress, int cost)
	{
		return baseValue + baseValue * progress / cost / 1000;
	}

	struct ProductionEvaluator
	{
		const GlobalInfoData& infos;
		const LeaderInfo& leader;
		//const MapGeometry& geom;
		const City& city;
		const CityAnalysis& analysis;
		//int valueForecastTurns;
		unsigned int averageProductionRate;

		explicit ProductionEvaluator(
			const GlobalInfoData& infos,
			const LeaderInfo& leader,
			const City& city,
			const CityAnalysis& analysis
		)
			: infos(infos)
			, leader(leader)
			, city(city)
			, analysis(analysis)
			//, valueForecastTurns(kValueForecastTurns* infos.speedInfo.projectProdPercent / 100)
			, averageProductionRate(city.optInspectableCityInfo->yieldRates[EYield::Production])
		{
		}

		int computeProductionChoiceScore(const CityBuildChoice& choice) const
		{
			//const int remainingTurns = cdiv(choice.cost - choice.progress, std::max(1u, averageProductionRate));
			if (const auto* const buildingClass = std::get_if<EBuildingClass>(&choice))
				return stabliseProduction(getBuildingBaseValue(infos, leader, city, analysis, *buildingClass), choice.progress, choice.cost);
			if (const auto* const project = std::get_if<EProject>(&choice))
				return stabliseProduction(getProjectBaseValue(infos, *project), choice.progress, choice.cost);
			if (const auto* const process = std::get_if<EProcess>(&choice))
				return computeProcessScore(*process, averageProductionRate);
			return 0;
		}

		int computeProductionChoiceBaseScore(const ProductionChoice& choice) const
		{
			if (const auto* const buildingClass = std::get_if<EBuildingClass>(&choice))
				return getBuildingBaseValue(infos, leader, city, analysis, *buildingClass);
			if (const auto* const project = std::get_if<EProject>(&choice))
				return getProjectBaseValue(infos, *project);
			if (const auto* const process = std::get_if<EProcess>(&choice))
				return computeProcessScore(*process, averageProductionRate);
			return 0;
		}
	};

	// Greedy solver.
	// costs[{ workerI, taskI }]
	// INT_MAX = can't build
	// Returns result[taskI] = workerI. -1 = unassigned task.
	// Tasks are finished in-order.
	// Worker production is overriden if wonder value > workerValueThreshold.
	std::vector<int> solveWonderBuild(const DynamicArray2D<int>& costs, std::span<const int> workerValueThreshold, std::span<const int> wonderValue)
	{
		const int numWorkers = costs.dim.x;
		const int numTasks = costs.dim.y;

		std::vector<int> workerTimes(numWorkers);
		std::vector<int> assignedWorkers(numTasks);

		for (const int taskI : range(numTasks))
		{
			int bestWorkerI = -1;
			int bestFinishT = INT_MAX;

			for (const int workerI : range(numWorkers))
			{
				const int cost = costs[{ workerI, taskI }];
				if (cost < INT_MAX)
				{
					const int finishT = workerTimes[workerI] + cost;
					if (wonderValue[taskI] > workerValueThreshold[workerI] && finishT < bestFinishT)
					{
						bestWorkerI = workerI;
						bestFinishT = finishT;
					}
				}
			}

			assignedWorkers[taskI] = bestWorkerI;
			workerTimes[bestWorkerI] = bestFinishT;
		}

		return assignedWorkers;
	}

	struct BuildChoiceEvaluation
	{
		CityBuildChoice choice;
		int value{};
	};

	struct CityChoice
	{
		size_t cityI{};
		int progress{};
		int cost{};
	};

	int getProductionChoiceCost(const GlobalInfoData& infos, const ProductionChoice& choice)
	{
		if (std::holds_alternative<EBuildingClass>(choice))
			return infos.buildings[infos.buildingClasses[std::get<EBuildingClass>(choice)].activeType].productionCost;
		if (std::holds_alternative<EProject>(choice))
			return infos.projects[std::get<EProject>(choice)].productionCost;
		return 1;
	}

	struct ProductionDecisionMethod
	{
		enum EKind
		{
			Regular, // Granary, barracks...
			WonderSolve, // World wonder buildings and projects. Stuff we can only build in a limited quantity and has negligable city effect.
			NationalSelection, // Things that we can build once anywhere we want with no time limit and mainly for their city effect.
		};

		EKind kind = Regular;
		int limit = INT_MAX;
	};

	ProductionDecisionMethod getProductionDecisionMethod(const GlobalInfoData& infos, const CivState& civState, ProductionChoice choice)
	{
		if (std::holds_alternative<EBuildingClass>(choice))
		{
			const auto& info = infos.buildingClasses[std::get<EBuildingClass>(choice)];

			if (info.isWorldWonder)
				return { ProductionDecisionMethod::WonderSolve, 1 };
			if (info.isNationalWonder)
				return { ProductionDecisionMethod::NationalSelection, 1 };

			return {};
		}
		if (std::holds_alternative<EProject>(choice))
		{
			const EProject project = std::get<EProject>(choice);
			const auto& info = infos.projects[project];
			// Let's assume that projects are either world projects or team projects.
			// And if we are allowed to build a world project, we can build at least one.
			return { ProductionDecisionMethod::WonderSolve, std::max(1, std::min(info.maxGlobalInstances, info.maxTeamInstances) - civState.projectCounts[project]) };
		}
		return {};
	}

	bool isVictoryProduction(const GlobalInfoData& infos, ProductionChoice choice)
	{
		if (const auto* const project = std::get_if<EProject>(&choice))
			return infos.projects[*project].optVictoryPrereq != EVictory::None;
		return false;
	}

	std::vector<BuildChoiceEvaluation> solveWonderBuildForCities(
		const GlobalInfoData& infos,
		const LeaderInfo& leader,
		const CivState& civState,
		std::span<const City> myCities,
		std::span<const CityAnalysis> cityAnalyses,
		const std::map<ProductionChoice, std::vector<CityChoice>>& worldWonderProductionsCityChoices,
		const std::vector<BuildChoiceEvaluation>& bestBuildChoicesPreWorldSolve
	)
	{
		if (myCities.empty())
			return bestBuildChoicesPreWorldSolve;

		std::vector<ProductionChoice> orderedWorldWonders;

		const bool isVictoryAtHand = civState.projectCounts[EProject::ApolloProgram] > 0 || worldWonderProductionsCityChoices.contains(EProject::ApolloProgram);

		// Multiply by limits.
		for (const ProductionChoice& choice : worldWonderProductionsCityChoices | std::views::keys)
			if (!isVictoryAtHand || isVictoryProduction(infos, choice))
				orderedWorldWonders.insert(orderedWorldWonders.end(), std::min<size_t>(myCities.size(), getProductionDecisionMethod(infos, civState, choice).limit), choice);

		// For now, just order by cost.
		std::ranges::stable_sort(orderedWorldWonders, std::less(), [&](const ProductionChoice& choice) {
			return getProductionChoiceCost(infos, choice);
			});

		// How many turns left.
		DynamicArray2D<int> worldWonderCosts(ivec2{ static_cast<int>(myCities.size()), static_cast<int>(orderedWorldWonders.size()) }, INT_MAX);
		for (const size_t wonderI : range(orderedWorldWonders.size()))
			for (const CityChoice& cityChoice : worldWonderProductionsCityChoices.at(orderedWorldWonders[wonderI]))
				worldWonderCosts[{ static_cast<int>(cityChoice.cityI), static_cast<int>(wonderI) }] = cdiv(cityChoice.cost - cityChoice.progress, static_cast<unsigned int>(std::max(1, myCities[cityChoice.cityI].optInspectableCityInfo->yieldRates[EYield::Production])));


		const std::vector<int> workerValueThreshold(std::from_range, bestBuildChoicesPreWorldSolve | std::views::transform(&BuildChoiceEvaluation::value));

		const ProductionEvaluator evaluator(
			infos,
			leader,
			// We'll just take the value from the capital for now.
			myCities[0],
			cityAnalyses[0]
		);

		const std::vector<int> wonderValue(std::from_range, orderedWorldWonders | std::views::transform([&](const ProductionChoice& choice) {
			return evaluator.computeProductionChoiceBaseScore(choice);
			}));

		const std::vector<int> citySelections = solveWonderBuild(worldWonderCosts, workerValueThreshold, wonderValue);
		// [wonderI] = cityI
		// Pick minimum.

		std::vector<int> wonderSelections(myCities.size(), INT_MAX);

		for (const size_t wonderI : range(orderedWorldWonders.size()))
			if (const int cityI = citySelections[wonderI]; cityI >= 0)
				wonderSelections[cityI] = std::min(wonderSelections[cityI], static_cast<int>(wonderI));

		std::vector<BuildChoiceEvaluation> finalBuildChoiceSelections = bestBuildChoicesPreWorldSolve;
		for (const size_t cityI : range(myCities.size()))
			if (const int wonderI = wonderSelections[cityI]; wonderI < INT_MAX)
			{
				const ProductionChoice choice = orderedWorldWonders[wonderSelections[cityI]];
				const CityChoice cityChoice = *std::ranges::find(worldWonderProductionsCityChoices.at(choice), cityI, &CityChoice::cityI);
				const CityBuildChoice cityBuildChoice{ choice, cityChoice.progress, cityChoice.cost };
				const int value = evaluator.computeProductionChoiceScore(cityBuildChoice);
				finalBuildChoiceSelections[cityI] = { cityBuildChoice, value };
			}

		return finalBuildChoiceSelections;
	}

	
}

std::vector<CityBuildingProductionRecomendation> mybot::computeCityBuildingProductionRecomendations(
    IGame& game,
	const GlobalInfoData& infos,
	const CivState& civState,
	const Player& activePlayer,
    std::span<const City> myCities,
    std::span<const CityAnalysis> cityAnalyses
)
{
	/// First, gather up choices. Clear production first to get a clean result.
	for (const City& city : myCities)
		game.tryChangeProduction(city.coord, std::monostate());
	std::vector<std::vector<CityBuildChoice>> buildChoices;
	buildChoices.reserve(myCities.size());
	for (const City& city : myCities)
	{
		auto choices = game.getCityProductionChoices(city.coord);
		// Filter out units.
		std::erase_if(choices, [](const CityBuildChoice& choice) {
			return !(std::holds_alternative<EBuildingClass>(choice) || std::holds_alternative<EProject>(choice) || std::holds_alternative<EProcess>(choice));
			});
		// Filter out wonders.
		if constexpr (!kEnableNonVictoryLimitedBuildings)
		{
			std::erase_if(choices, [&](const CityBuildChoice& choice) {
				return !isVictoryProduction(infos, choice) && getProductionDecisionMethod(infos, civState, choice).kind != ProductionDecisionMethod::Regular;
				});
		}
		buildChoices.push_back(std::move(choices));
	}

	/// Find which cities can build the wonder solve buildings.
	

	std::map<ProductionChoice, std::vector<CityChoice>> worldWonderProductionsCityChoices;
	for (const size_t i : range(myCities.size()))
	{
		for (const CityBuildChoice& buildChoice : buildChoices[i])
		{
			const ProductionDecisionMethod method = getProductionDecisionMethod(infos, civState, buildChoice);
			if (method.kind == ProductionDecisionMethod::WonderSolve)
				worldWonderProductionsCityChoices[buildChoice].emplace_back(i, buildChoice.progress, buildChoice.cost);
		}
	}

	// We would like to build wonders in an order, as quick as possible, using multiple cities.
	// This is kind of like the  https://en.wikipedia.org/wiki/Linear_bottleneck_assignment_problem or https://en.wikipedia.org/wiki/Unrelated-machines_scheduling.
	// Except we'd prefer an order.
	// For now, we'll just use a greedy algorithm.
	// But which cities to build wonders in. Top N producers? Should cities build their basic buildings first?
	// We could decide basic buildings first and then give the solver basic values and wonder values, then it can decide whether to override basic building production.
	
	/// Evaluate buildings.
	// For national wonders, we'll pick the best place to build them.
	// For world wonders, ignore city value, for now.
	

	std::vector<std::vector<BuildChoiceEvaluation>> buildEvaluations;
	
	const LeaderInfo& leader = infos.leaders[activePlayer.leader];

	for (const size_t i : range(myCities.size()))
	{
		const ProductionEvaluator evaluator(
			infos,
			leader,
			myCities[i],
			cityAnalyses[i]
		);

		buildEvaluations.emplace_back(std::from_range, buildChoices[i] | std::views::transform([&](const CityBuildChoice& choice) {
			return BuildChoiceEvaluation{ choice, evaluator.computeProductionChoiceScore(choice) };
			}));
	}

	/// Determine best places to build national wonders.
	struct CitySelection
	{
		size_t cityI{};
		int value{};
	};
	std::map<ProductionChoice, CitySelection> bestCitiesByProductionChoice;
	for (const size_t i : range(myCities.size()))
		for (const BuildChoiceEvaluation& eval : buildEvaluations[i])
		{
			const auto [it, isNew] = bestCitiesByProductionChoice.emplace(eval.choice, CitySelection());
			if (isNew || it->second.value < eval.value)
				it->second = { i, eval.value };
		}
	
	/// Assign regular buildings and national selection.
	std::vector<BuildChoiceEvaluation> bestBuildChoicesPreWorldSolve;
	bestBuildChoicesPreWorldSolve.resize(myCities.size());
	for (const size_t i : range(myCities.size()))
	{
		BuildChoiceEvaluation best{ CityBuildChoice(std::monostate()), 0 };
		for (const BuildChoiceEvaluation& eval : buildEvaluations[i])
		{
			if (eval.value > best.value)
			{
				const ProductionDecisionMethod method = getProductionDecisionMethod(infos, civState, eval.choice);
				if (method.kind == ProductionDecisionMethod::Regular
					|| (method.kind == ProductionDecisionMethod::NationalSelection && bestCitiesByProductionChoice.at(eval.choice).cityI == i))
				{
					best = eval;
				}
			}
		}

		bestBuildChoicesPreWorldSolve[i] = best;
	}

	/// Now, solve for world wonders.
	const std::vector<BuildChoiceEvaluation> bestBuildChoices = solveWonderBuildForCities(infos, leader, civState, myCities, cityAnalyses, worldWonderProductionsCityChoices, bestBuildChoicesPreWorldSolve);

	return { std::from_range, bestBuildChoices  | std::views::transform([&](const BuildChoiceEvaluation& choiceEval) {
		return CityBuildingProductionRecomendation{
			.choice = choiceEval.choice,
			.finalAssignmentValue = choiceEval.value * 2,
		};
	}) };
}
