#include "MilitaryAdvisor.h"
#include "MapUnitLookup.h"

#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/EnumDefs.h>
#include <PlayerBotGameBinding/EnumStrings.h>

#include <CommonStuff/div.h>
#include <CommonStuff/RangeTransformUtil.h>

#include <map>
#include <set>
#include <ranges>
#include <generator>
#include <functional>
#include <algorithm>
#include <iostream>

using namespace mybot;

// Ideas:
// https://www.gamedeveloper.com/design/designing-ai-algorithms-for-turn-based-strategy-games
// https://github.com/mzetkowski/tbsf-unity-docs?tab=readme-ov-file#7-ai-system
// https://en.wikipedia.org/wiki/Weapon-target_assignment_problem
// https://gamedev.stackexchange.com/questions/206216/ai-for-global-decision-making-in-4x-games

// Power = CvCity::processBuilding + CvUnit::init + CvPlayer::changeTotalPopulation + CvTeam::processTech
// 

// This is a resource assignment problem. X units to Y cities/enemies.
// To solve generally, you generate all X*Y combinations of task assignments and sort them.
// Let's try not to do that.
//
// Tasks:
// - City defence
//     - Quick solution: Check defenders on city plot.
// - Settler escort
//     - Quick solution: Check escort on settler plot.
// - Anti-barb
//     - Quick solution: Ignore units that are too far away to help.
// - Scouting
//     - Quick solution: Only scouts and explorers, and if one city before barbs, the initial warrior.
//

namespace
{
	// Bigger values reduce the effect of distance.
	constexpr int kDistancePenaltyDivisionOffset = 3;
	constexpr int kFortifyBonus = 25;
	constexpr int kNumFortifyTurns = 5;

	// Priority order
	enum class ETask
	{
		Scouting,
		FutureSettlerEscort,
		MilitaryDefence,
		SettlerEscort,
		AntiBarb,
		CityDefence,
	};

	struct TaskEvalContext
	{
		const IGame& game;
		MapGeometry geom{};
		const Unit& unit;
		const GlobalInfoData& infos;
	};

	struct UnitPreference
	{
		EUnitClass cheapMilitaryPolice{};
		EUnitClass cityDefender{};
		EUnitClass attacker{};
		EUnitClass scout{};
	};

	int guessUnitTurns(const MapGeometry& geom, const Unit& unit, ivec2 target)
	{
		return mybot::computeDistance(geom, unit.coord, target, mybot::EDistanceMetric::Step);
	}

	int evalDistanceScaledScore(const TaskEvalContext& ctx, ivec2 target, int taskKindScore)
	{
		return (taskKindScore + kDistancePenaltyDivisionOffset) / (kDistancePenaltyDivisionOffset + guessUnitTurns(ctx.geom, ctx.unit, target));
	}

	

	struct CityDefenceTask
	{
		const City* city{};

		static constexpr int kScoreScale = 100;

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			int score = ctx.unit.strength;
			const UnitInfo& unitInfo = ctx.infos.units[ctx.infos.unitClasses[ctx.unit.klass].activeType];
			const int fortifyBonus = unitInfo.bNoDefensiveBonus ? 0 : kFortifyBonus;// +(unit.numFortifyTurns - kNumFortifyTurns) * 5 / 5; // Bias to units that are already fortified.
			int modifier = unitInfo.cityDefenceModifier + fortifyBonus;
			score = score * (modifier + 100) / 100;
			return score * kScoreScale;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, city->coord, taskKindScore);
		}

		void execute(IGame& game, const Unit& unit) const
		{
			game.startMission(std::array{ unit.id }, EMission::MoveTo, city->coord.x, city->coord.y);
			if (game.getUnitCoord(unit.id) == city->coord)
				game.startMission(std::array{ unit.id }, EMission::Fortify, -1, -1);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.cityDefender,
				.target = city->coord,
				.turns = 0,
				.urgency = EUnitProductionUrgency::UndefendedCity,
			};
		}

		friend bool operator==(CityDefenceTask, CityDefenceTask) = default;

		friend std::strong_ordering operator<=>(CityDefenceTask a, CityDefenceTask b)
		{
			return a.city->coord <=> b.city->coord;
		}
	};

	struct MilitaryPolice
	{
		const City* city{};

		static constexpr int kScoreScale = 100;

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			return CityDefenceTask::evaluateTaskKindScore(ctx);
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, city->coord, taskKindScore);
		}

		void execute(IGame& game, const Unit& unit) const
		{
			return CityDefenceTask{ city }.execute(game, unit);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.cheapMilitaryPolice,
				.target = city->coord,
				.turns = 0,
				.urgency = EUnitProductionUrgency::UndefendedCity,
			};
		}

		friend bool operator==(MilitaryPolice, MilitaryPolice) = default;

		friend std::strong_ordering operator<=>(MilitaryPolice a, MilitaryPolice b)
		{
			return a.city->coord <=> b.city->coord;
		}
	};

	struct SettlerEscortTask
	{
		const Unit* settler{};

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			int score = CityDefenceTask::evaluateTaskKindScore(ctx) / 10;
			// Bias if can keep up with settler.
			score += ctx.unit.maxMoves >= 2 * kMoveDenominator;
			return score;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, settler->coord, taskKindScore);
		}

		void execute(IGame& game, const Unit& unit) const
		{
			// Only escorting if we can keep up... need to group these units.
			game.startMission(std::array{ unit.id }, EMission::MoveTo, settler->coord.x, settler->coord.y);
			if (game.getUnitCoord(unit.id) ==  settler->coord)
				game.startMission(std::array{ unit.id }, EMission::Fortify, -1, -1);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.cityDefender,
				.target = settler->coord,
				.turns = 0,
				.urgency = EUnitProductionUrgency::Escort,
			};
		}

		friend bool operator==(SettlerEscortTask, SettlerEscortTask) = default;

		friend std::strong_ordering operator<=>(SettlerEscortTask a, SettlerEscortTask b)
		{
			return a.settler->id <=> b.settler->id;
		}
	};

	struct FutureSettlerEscortTask
	{
		ivec2 target{};

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			return SettlerEscortTask::evaluateTaskKindScore(ctx) / 2;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, target, taskKindScore);
		}

		void execute(IGame& game, const Unit& unit) const
		{
			// Only escorting if we can keep up... need to group these units.
			game.startMission(std::array{ unit.id }, EMission::MoveTo, target.x, target.y);
			if (game.getUnitCoord(unit.id) == target)
				game.startMission(std::array{ unit.id }, EMission::Fortify, target.x, target.y);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.cityDefender,
				.target = static_cast<i16vec2>(target),
				.turns = 5,
				.urgency = EUnitProductionUrgency::Escort,
			};
		}

		friend bool operator==(FutureSettlerEscortTask, FutureSettlerEscortTask) = default;

		friend std::strong_ordering operator<=>(FutureSettlerEscortTask, FutureSettlerEscortTask) = default;
	};

	struct AntiBarbTask
	{
		std::span<const Unit* const> barbs{};

		static constexpr int kScoreScale = 20;

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			const UnitInfo& unitInfo = ctx.infos.units[ctx.infos.unitClasses[ctx.unit.klass].activeType];
			if (unitInfo.canAttack)
				return ctx.unit.strength * kScoreScale;
			else
				return 0;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, barbs[0]->coord, taskKindScore);
		}

		void execute(IGame& game, const Unit& unit) const
		{
			game.startMission(std::array{ unit.id }, EMission::MoveTo, barbs[0]->coord.x, barbs[0]->coord.y);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.attacker,
				.target = barbs[0]->coord,
				.turns = 0,
				.count = static_cast<int>(barbs.size()),
				.urgency = EUnitProductionUrgency::TerritoryDefence,
			};
		}

		friend bool operator==(AntiBarbTask a, AntiBarbTask b)
		{
			return a.barbs[0]->coord == b.barbs[0]->coord;
		}

		friend std::strong_ordering operator<=>(AntiBarbTask a, AntiBarbTask b)
		{
			return a.barbs[0]->coord <=> b.barbs[0]->coord;
		}
	};

	struct ScoutingTask
	{
		i16vec2 taget{};

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			return ctx.unit.strength * (ctx.unit.maxMoves > kMoveDenominator ? 2 : 1);
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			// Check if can automate?
			return taskKindScore + (ctx.game.getAutomation(std::array{ ctx.unit.id }) == EAutomation::Explore) * 2;
		}

		void execute(IGame& game, const Unit& unit) const
		{
			game.tryAutomate(std::array{ unit.id }, EAutomation::Explore);
		}

		UnitProductionDemand makeDemand(const UnitPreference& unitPreference) const
		{
			return UnitProductionDemand{
				.klass = unitPreference.scout,
				.target = taget,
				.turns = 0,
				.urgency = EUnitProductionUrgency::NonCombat,
			};
		}

		friend std::strong_ordering operator<=>(ScoutingTask, ScoutingTask) = default;
	};

	using Task = std::variant<ScoutingTask, FutureSettlerEscortTask, MilitaryPolice, SettlerEscortTask, AntiBarbTask, CityDefenceTask>;

	struct TaskKindScoreEval
	{
		const TaskEvalContext& ctx;

		int operator()(const auto& task) const
		{
			
			return task.evaluateTaskKindScore(ctx);
		}
	};

	struct TaskScoreEval
	{
		const TaskEvalContext& ctx;
		int taskKindScore{};

		int operator()(const auto& task) const
		{
			return task.evaluateScore(ctx, taskKindScore);
		}
	};

	struct TaskExecutor
	{
		IGame& game;
		const Unit& unit;

		void operator()(const auto& task) const
		{
			return task.execute(game, unit);
		}
	};

	struct TaskAssignment
	{
		const Task* task{};
		const Unit* unit{};
		int score{};

		friend std::strong_ordering operator<=>(const TaskAssignment& a, const TaskAssignment& b)
		{
			if (const auto c = a.score <=> b.score; c != 0)
				return c;
			if (const auto c = a.unit->id <=> b.unit->id; c != 0)
				return c;
			
			return a.task <=> b.task;
		}
	};

	struct UnitAssignmentResult
	{
		std::map<const Unit*, TaskAssignment> assignments;
		std::vector<const Unit*> spareUnits;
		std::vector<const Task*> unassignedTasks;
	};

	UnitAssignmentResult assignUnitTasks(const IGame& game, MapGeometry mapGeom, const GlobalInfoData& infos, std::span<const Unit> myUnits, std::span<const Task> tasks)
	{
		std::vector<TaskAssignment> combinations;
		combinations.reserve(100);

		for (const Unit& unit : myUnits)
		{
			const TaskEvalContext ctx{
				game,
				mapGeom,
				unit,
				infos,
			};
			for (const Task& task : tasks)
			{
				const int taskKindScore = std::visit(TaskKindScoreEval{ ctx }, task);
				const int taskScore = std::visit(TaskScoreEval{ ctx, taskKindScore }, task);
				if (taskScore > 0)
				{
					combinations.emplace_back(
						&task,
						&unit,
						taskScore
					);
				}
			}
		}

		std::ranges::sort(combinations, std::greater());

		std::set<const Task*> satisfiedTasks;
		std::map<const Unit*, TaskAssignment> assignments;

		for (const TaskAssignment& candidate : combinations)
		{
			if (assignments.contains(candidate.unit))
				continue;
			if (!satisfiedTasks.insert(candidate.task).second)
				continue;
			assignments[candidate.unit] = candidate;
		}

		std::vector<const Unit*> spareUnits;
		for (const Unit& unit : myUnits)
			if (!assignments.contains(&unit))
				spareUnits.push_back(&unit);

		std::vector<const Task*> unassignedTasks;
		for (const Task& task : tasks)
			if (!satisfiedTasks.contains(&task))
				unassignedTasks.push_back(&task);

		return {
			std::move(assignments),
			std::move(spareUnits),
			std::move(unassignedTasks),
		};
	}

	// Return the power ratio to use to decide ShowOfForce unit demand.
	int computePowerRatioPercentForShowOfForce(EPlayer activePlayerI, std::span<const std::optional<Player>> players)
	{
		int myPower{};
		int sumPower{};
		int numPowerKnown{};
		int maxPower{};
		for (const size_t i : range(players.size()))
		{
			if (!players[i] || !players[i]->optVisibleDemographics)
				continue;

			const int power = players[i]->optVisibleDemographics->power;

			if (i == activePlayerI)
			{
				myPower = power;
				continue;
			}

			if (players[i]->optVisibleDemographics)
			{
				sumPower += power;
				maxPower = std::max(maxPower, power);
				++numPowerKnown;
			}
		}

		if (numPowerKnown <= 0)
			return 100;
		else
			return heck::rdiv(myPower * 100, static_cast<unsigned int>(std::max(1, maxPower)));
			//return heck::rdiv(myPower * numPowerKnown * 100, static_cast<unsigned int>(std::max(1, sumPower)));
	}
}

void MilitaryKnowledge::update(
	const GlobalInfoData& infos,
	EPlayer activePlayerI,
	Span2D<const Plot> plots,
	std::span<const Unit> allUnits,
	std::span<const Unit> enemyUnits
)
{
	barbUnitsSeenBits.resize(EUnitType::Num);
	rivalUnitsSeenBits.resize(EUnitType::Num);
	for (const Unit& unit : allUnits)
	{
		if (unit.owner != activePlayerI)
		{
			auto& bits = unit.owner == kBarbarianPlayer ? barbUnitsSeenBits : rivalUnitsSeenBits;
			const EUnitType type = infos.unitClasses[unit.klass].defaultType; // TODO: Use actual type
			if (!bits[type])
			{
				bits[type] = true;
				(unit.owner == kBarbarianPlayer ? barbUnitsSeen : rivalUnitsSeen).push_back(type);
			}
		}
	}

	for (const Unit& unit : enemyUnits)
	{
		const UnitInfo& unitInfo = infos.units[infos.unitClasses[unit.klass].defaultType]; // We'll assume it's the right info

		if (unitInfo.isAnimal || unitInfo.domain != EDomain::Land)
			continue;

		barbsHaveEnteredTerritory |= unit.owner == kBarbarianPlayer && plots[unit.coord].owner != kNoPlayer && plots[unit.coord].owner != kBarbarianPlayer;
	}
}

MilitaryAnalysis mybot::doMilitaryAnalysis(
	const CivState& civState,
	std::span<const Unit> myUnits,
	std::span<const Unit> enemyUnits,
	std::optional<ivec2> futureSettlerEscortTarget,
	std::span<const City> myCities,
	std::span<const std::optional<Player>> players,
	MapGeometry mapGeom,
	[[maybe_unused]] Span2D<const Plot> plots,
	[[maybe_unused]] const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
	[[maybe_unused]] std::span<const TechState> techs,
	const GlobalInfoData& infos,
	//const GameSetup& setup,
	const GlobalInfo& globalInfo,
	IGame& game,
	const MilitaryKnowledge& militaryKnowledge
)
{
	MilitaryAnalysis analysis;

	const auto isItTime = [&] {
		// Guess whether barbs can enter territory yet.
		// The condition in CvUnitAI is: GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 2)
		// We know countCivPlayersAlive, and can guess getNumCivCities from total land.
		// getNumCivCities = total land / land per city
		// To be more accurate, assume the culture level of capitals is 1 or 2, and assume the culture level of other cities is 1.
		// Underestimation of land per city is safer, but try not to underestimate too much (typical worst case: all capitals coastal).
		constexpr int kRing1Land = 9;
		constexpr int kRing2Land = 21;
		const int cultureThreshold = infos.cultureLevels[2].culture;
		const int culturePerTurn = infos.buildings[EBuildingType::Palace].obsoleteSafeCommerceChanges[ECommerce::Culture];
		const int numCapitals = globalInfo.numAliveCivPlayers;
		const int landPerCapital = game.getTurnNumber() * culturePerTurn >= cultureThreshold ? kRing2Land : kRing1Land;
		const int capitalLandCoveragePercent = 90; // Take into account some coastal starts.
		const int totalLandFromCapitals = numCapitals * landPerCapital * capitalLandCoveragePercent / 100;

		const int totalLand = globalInfo.globalDemographics[GlobalInfo::Land].rivalAverage * (globalInfo.numAliveCivPlayers - 1) + globalInfo.globalDemographics[GlobalInfo::Land].value;
		const int remainingLandFromSecondCities = std::max(0, totalLand - totalLandFromCapitals);
		const int numSecondCities = (remainingLandFromSecondCities + kRing1Land / 2) / kRing1Land;

		const int guessTotalCities = numCapitals + numSecondCities;
		return guessTotalCities > globalInfo.numAliveCivPlayers * 2;
		};
	
	analysis.canBarbsEnterTerritory = militaryKnowledge.barbsHaveEnteredTerritory || isItTime();

	analysis.barbsThreatTurn = analysis.canBarbsEnterTerritory ? 0 : infos.handicap.firstMilitaryBarbCreationTurn;

	const MapUnitLookup enemyUnitMapLookup(enemyUnits);
	const MapUnitLookup myUnitMapLookup(myUnits);
	
	std::vector<Task> tasks;
	if (analysis.canBarbsEnterTerritory)
		for (const City& city : myCities)
		{
			tasks.emplace_back(CityDefenceTask{ &city });
			if (civState.civics[ECivicOptionType::Government] == ECivic::HereditaryRule)
			{
				const int currentMilitaryPolice = static_cast<int>(std::ranges::count_if(myUnitMapLookup.getUnitsAt(city.coord), [&](const Unit* unit) {
					return infos.units[infos.unitClasses[unit->klass].activeType].isMilitaryHappiness;
					}));
				const int targetPop = city.pop + 1;
				const int happinessAtTargetPop = city.optInspectableCityInfo->happiness - (targetPop - city.pop);
				const int targetMilitaryPolice = std::clamp(currentMilitaryPolice - happinessAtTargetPop, 0, 100);
				for ([[maybe_unused]] const int i : range(targetMilitaryPolice))
					tasks.emplace_back(MilitaryPolice{ &city });
			}

		}
	for (const auto& [coord, stack] : enemyUnitMapLookup.byCoord)
	{
		const UnitInfo& unitInfo = infos.units[infos.unitClasses[stack[0]->klass].defaultType]; 
		if (unitInfo.isAnimal || unitInfo.domain != EDomain::Land)
			continue;

		if (stack[0]->owner == kBarbarianPlayer && !analysis.canBarbsEnterTerritory)
			continue;

		tasks.emplace_back(AntiBarbTask{ stack });
	}
	for (const Unit& unit : myUnits)
		if (unit.klass == EUnitClass::Settler)
			tasks.emplace_back(SettlerEscortTask{ &unit });
	if (futureSettlerEscortTarget)
		tasks.emplace_back(FutureSettlerEscortTask{ *futureSettlerEscortTarget });
	
	tasks.emplace_back(ScoutingTask{});

	const UnitAssignmentResult result = assignUnitTasks(game, mapGeom, infos, myUnits, tasks);

	for (const auto& assignment : result.assignments)
		std::visit(TaskExecutor{ game, *assignment.second.unit }, *assignment.second.task);

	const std::vector<EUnitClass> availableUnitClasses = game.getCityProductionChoices(myCities.front().coord)
		| std::views::filter([](const ProductionChoice& choice) { return std::holds_alternative<EUnitClass>(choice); })
		| std::views::transform([](const ProductionChoice& choice) { return std::get<EUnitClass>(choice); })
		| std::ranges::to<std::vector>();

	const auto getUnitStrength = [&](EUnitClass klass, bool onCityDefence) {
		const auto& unitInfo = infos.units[infos.unitClasses[klass].activeType];
		return unitInfo.baseCombatStrength * (100 + onCityDefence * unitInfo.cityDefenceModifier) / 100;
		};

	const UnitPreference unitPreference{
		.cheapMilitaryPolice = std::ranges::min(availableUnitClasses, std::less(), [&](EUnitClass klass) {
			const auto& unitInfo = infos.units[infos.unitClasses[klass].activeType];
			return unitInfo.isMilitaryHappiness ? unitInfo.productionCost : INT_MAX;
		}),
		.cityDefender = std::ranges::max(availableUnitClasses, std::less(), [&](EUnitClass klass) {
			//const auto& unitInfo = infos.units[infos.unitClasses[klass].activeType];
			return getUnitStrength(klass, true);
		}),
		.attacker = std::ranges::max(availableUnitClasses, std::less(), [&](EUnitClass klass) {
			const auto& unitInfo = infos.units[infos.unitClasses[klass].activeType];
			return unitInfo.canAttack ? getUnitStrength(klass, false) : -1;
		}),
		.scout = std::ranges::max(availableUnitClasses, std::less(), [&](EUnitClass klass) {
			const auto& unitInfo = infos.units[infos.unitClasses[klass].activeType];
			// CvUnit::canAutomate condition
			if ((unitInfo.baseCombatStrength <= 0 && unitInfo.domain != EDomain::Sea) || unitInfo.domain == EDomain::Air || unitInfo.domain == EDomain::Immobile)
				return std::pair(-1, -1);
			else
				return std::pair(+unitInfo.moveSteps, unitInfo.baseCombatStrength);
		}),
	};

	

	const auto isMilitaryUnit = [&](const Unit& unit) {
		const UnitInfo& info = infos.units[infos.unitClasses[unit.klass].activeType];
		return info.isMilitaryHappiness;
		};

	const int numMilitaryUnits = static_cast<int>(std::ranges::count_if(myUnits, isMilitaryUnit));

	const int bestAvailableEffectiveStrength = getUnitStrength(unitPreference.cityDefender, true);

	const auto isObsoleteMilitaryUnit = [&](const Unit& unit) {
		//const UnitInfo& info = infos.units[infos.unitClasses[unit.klass].activeType];
		return isMilitaryUnit(unit) && getUnitStrength(unitPreference.cityDefender, true) * 2 < bestAvailableEffectiveStrength;
		};

	std::vector<const Unit*> spareMilitaryUnits = result.spareUnits;
	std::ranges::sort(spareMilitaryUnits, std::greater(), [&](const Unit* unit) {
		return getUnitStrength(unit->klass, true);
		});

	// Scrap units that are too weak to be useful.
	while (spareMilitaryUnits.size() > myCities.size() && isObsoleteMilitaryUnit(*spareMilitaryUnits.back()))
	{
		const Unit& unit = *spareMilitaryUnits.back();
		std::clog << "DELETING UNIT: " << cvbot::kUnitTypeNames[infos.unitClasses[unit.klass].activeType] << ", effective strength = " << getUnitStrength(unit.klass, true) << " vs best possible strength " << bestAvailableEffectiveStrength << '\n';
		game.tryDelete(spareMilitaryUnits.back()->id);
	}


	const int showOfForcePowerRatio = computePowerRatioPercentForShowOfForce(civState.activePlayerI, players);
	analysis.rivalPowerRatioPercent = showOfForcePowerRatio;

	const int targetNumMilitaryUnits = numMilitaryUnits * 100 / showOfForcePowerRatio;
	const int numShowOfForceWanted = std::max(targetNumMilitaryUnits - numMilitaryUnits, 0);

	std::clog << "showOfForcePowerRatio = " << showOfForcePowerRatio << '\n';
	std::clog << "numMilitaryUnits = " << numMilitaryUnits << '\n';
	std::clog << "numShowOfForceWanted = " << numShowOfForceWanted << '\n';

	for (const Task* const task : result.unassignedTasks)
		analysis.unitProductionDemands.push_back(std::visit([&](const auto& typedTask) { return typedTask.makeDemand(unitPreference); }, *task));

	for ([[maybe_unused]] const int i : range(numShowOfForceWanted))
	{
		const UnitProductionDemand demand{
			.klass = unitPreference.attacker,
			.target = myCities.front().coord,
			.turns = 0,
			.urgency = EUnitProductionUrgency::ShowOfForce,
		};
		analysis.unitProductionDemands.push_back(demand);
	}

	return analysis;
}
