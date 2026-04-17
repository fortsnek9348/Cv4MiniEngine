#include "MilitaryAdvisor.h"

#include <PlayerBotGameBinding/Infos.h>
#include <PlayerBotGameBinding/EnumDefs.h>

#include <CommonStuff/RangeTransformUtil.h>

#include <map>
#include <set>
#include <ranges>
#include <generator>
#include <functional>

using namespace mybot;

// Ideas:
// https://www.gamedeveloper.com/design/designing-ai-algorithms-for-turn-based-strategy-games
// https://github.com/mzetkowski/tbsf-unity-docs?tab=readme-ov-file#7-ai-system
// https://en.wikipedia.org/wiki/Weapon-target_assignment_problem
// https://gamedev.stackexchange.com/questions/206216/ai-for-global-decision-making-in-4x-games

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
		SettlerEscort,
		AntiBarb,
		CityDefence,
	};

	struct TaskEvalContext
	{
		const cvbot::IGame& game;
		MapGeometry geom{};
		const cvbot::Unit& unit;
		const cvbot::GlobalInfoData& infos;
	};

	int guessUnitTurns(const MapGeometry& geom, const cvbot::Unit& unit, ivec2 target)
	{
		return mybot::computeDistance(geom, unit.coord, target, mybot::EDistanceMetric::Step);
	}

	int evalDistanceScaledScore(const TaskEvalContext& ctx, ivec2 target, int taskKindScore)
	{
		return (taskKindScore + kDistancePenaltyDivisionOffset) / (kDistancePenaltyDivisionOffset + guessUnitTurns(ctx.geom, ctx.unit, target));
	}

	

	struct CityDefenceTask
	{
		const cvbot::City* city{};

		static constexpr int kScoreScale = 100;

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			int score = ctx.unit.strength;
			const cvbot::UnitInfo& unitInfo = ctx.infos.units[ctx.infos.unitClasses[ctx.unit.klass].activeType];
			const int fortifyBonus = unitInfo.bNoDefensiveBonus ? 0 : kFortifyBonus;// +(unit.numFortifyTurns - kNumFortifyTurns) * 5 / 5; // Bias to units that are already fortified.
			int modifier = unitInfo.cityDefenceModifier + fortifyBonus;
			score = score * (modifier + 100) / 100;
			return score * kScoreScale;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, city->coord, taskKindScore);
		}

		void execute(cvbot::IGame& game, const cvbot::Unit& unit) const
		{
			game.startMission(std::array{ unit.id }, cvbot::EMission::MoveTo, city->coord.x, city->coord.y);
			if (game.getUnitCoord(unit.id) == city->coord)
				game.startMission(std::array{ unit.id }, cvbot::EMission::Fortify, -1, -1);
		}

		friend bool operator==(CityDefenceTask, CityDefenceTask) = default;

		friend std::strong_ordering operator<=>(CityDefenceTask a, CityDefenceTask b)
		{
			return a.city->coord <=> b.city->coord;
		}
	};

	struct SettlerEscortTask
	{
		const cvbot::Unit* settler{};

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			int score = CityDefenceTask::evaluateTaskKindScore(ctx) / 10;
			// Bias if can keep up with settler.
			score += ctx.unit.maxMoves >= 2 * cvbot::kMoveDenominator;
			return score;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, settler->coord, taskKindScore);
		}

		void execute(cvbot::IGame& game, const cvbot::Unit& unit) const
		{
			// Only escorting if we can keep up... need to group these units.
			game.startMission(std::array{ unit.id }, cvbot::EMission::MoveTo, settler->coord.x, settler->coord.y);
			if (game.getUnitCoord(unit.id) ==  settler->coord)
				game.startMission(std::array{ unit.id }, cvbot::EMission::Fortify, -1, -1);
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

		void execute(cvbot::IGame& game, const cvbot::Unit& unit) const
		{
			// Only escorting if we can keep up... need to group these units.
			game.startMission(std::array{ unit.id }, cvbot::EMission::MoveTo, target.x, target.y);
			if (game.getUnitCoord(unit.id) == target)
				game.startMission(std::array{ unit.id }, cvbot::EMission::Fortify, target.x, target.y);
		}

		friend bool operator==(FutureSettlerEscortTask, FutureSettlerEscortTask) = default;

		friend std::strong_ordering operator<=>(FutureSettlerEscortTask, FutureSettlerEscortTask) = default;
	};

	struct AntiBarbTask
	{
		std::span<const cvbot::Unit* const> barbs{};

		static constexpr int kScoreScale = 20;

		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			const cvbot::UnitInfo& unitInfo = ctx.infos.units[ctx.infos.unitClasses[ctx.unit.klass].activeType];
			if (unitInfo.canAttack)
				return ctx.unit.strength * kScoreScale;
			else
				return 0;
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			return evalDistanceScaledScore(ctx, barbs[0]->coord, taskKindScore);
		}

		void execute(cvbot::IGame& game, const cvbot::Unit& unit) const
		{
			game.startMission(std::array{ unit.id }, cvbot::EMission::MoveTo, barbs[0]->coord.x, barbs[0]->coord.y);
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
		static int evaluateTaskKindScore(const TaskEvalContext& ctx)
		{
			return ctx.unit.strength * (ctx.unit.maxMoves > cvbot::kMoveDenominator ? 2 : 1);
		}

		int evaluateScore(const TaskEvalContext& ctx, int taskKindScore) const
		{
			// Check if can automate?
			return taskKindScore + (ctx.game.getAutomation(std::array{ ctx.unit.id }) == cvbot::EAutomation::Explore) * 2;
		}

		void execute(cvbot::IGame& game, const cvbot::Unit& unit) const
		{
			game.tryAutomate(std::array{ unit.id }, cvbot::EAutomation::Explore);
		}

		friend std::strong_ordering operator<=>(ScoutingTask, ScoutingTask) = default;
	};

	using Task = std::variant<ScoutingTask, FutureSettlerEscortTask, SettlerEscortTask, AntiBarbTask, CityDefenceTask>;

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
		cvbot::IGame& game;
		const cvbot::Unit& unit;

		void operator()(const auto& task) const
		{
			return task.execute(game, unit);
		}
	};

	struct TaskAssignment
	{
		Task task{};
		const cvbot::Unit* unit{};
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
		std::map<const cvbot::Unit*, TaskAssignment> assignments;
		std::vector<const cvbot::Unit*> spareUnits;
		std::vector<Task> unassignedTasks;
	};

	UnitAssignmentResult assignUnitTasks(const cvbot::IGame& game, cvbot::MapGeometry mapGeom, const cvbot::GlobalInfoData& infos, std::span<const cvbot::Unit> myUnits, std::span<const Task> tasks)
	{
		std::vector<TaskAssignment> combinations;
		combinations.reserve(100);

		for (const cvbot::Unit& unit : myUnits)
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
						task,
						&unit,
						taskScore
					);
				}
			}
		}

		std::ranges::sort(combinations, std::greater());

		std::set<Task> satisfiedTasks;
		std::map<const cvbot::Unit*, TaskAssignment> assignments;

		for (const TaskAssignment& candidate : combinations)
		{
			if (assignments.contains(candidate.unit))
				continue;
			if (!satisfiedTasks.insert(candidate.task).second)
				continue;
			assignments[candidate.unit] = candidate;
		}

		std::vector<const cvbot::Unit*> spareUnits;
		for (const cvbot::Unit& unit : myUnits)
			if (!assignments.contains(&unit))
				spareUnits.push_back(&unit);

		std::vector<Task> unassignedTasks;
		for (const Task& task : tasks)
			if (!satisfiedTasks.contains(task))
				unassignedTasks.push_back(task);

		return {
			std::move(assignments),
			std::move(spareUnits),
			std::move(unassignedTasks),
		};
	}

	struct MapUnitLookup
	{
		std::map<cvbot::i16vec2, std::vector<const cvbot::Unit*>> byCoord;

		explicit MapUnitLookup(std::span<const cvbot::Unit> units)
		{
			for (const cvbot::Unit& unit : units)
				byCoord[unit.coord].push_back(&unit);
		}

		std::span<const cvbot::Unit* const> getUnitsAt(cvbot::i16vec2 coord) const
		{
			if (const auto it = byCoord.find(coord); it != byCoord.end())
				return it->second;
			else
				return {};
		}
	};
}
void MilitaryAdvisor::update(
	std::span<const cvbot::Unit> myUnits,
	std::span<const cvbot::Unit> enemyUnits,
	std::optional<ivec2> futureSettlerEscortTarget,
	std::span<const cvbot::City> myCities,
	cvbot::MapGeometry mapGeom,
	[[maybe_unused]] cvbot::Span2D<const cvbot::Plot> plots,
	[[maybe_unused]] const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
	[[maybe_unused]] std::span<const cvbot::TechState> techs,
	const cvbot::GlobalInfoData& infos,
	//const cvbot::GameSetup& setup,
	const cvbot::GlobalInfo& globalInfo,
	cvbot::IGame& game
)
{
	for (const cvbot::Unit& unit : enemyUnits)
	{
		const cvbot::UnitInfo& unitInfo = infos.units[infos.unitClasses[unit.klass].defaultType]; // We'll assume it's the right info
		if (unitInfo.isAnimal || unitInfo.domain != cvbot::EDomain::Land)
			continue;

		barbsHaveEnteredTerritory |= unit.owner == cvbot::kBarbarianPlayer && plots[unit.coord.y, unit.coord.x].owner != cvbot::kNoPlayer && plots[unit.coord.y, unit.coord.x].owner != cvbot::kBarbarianPlayer;
	}

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
		const int culturePerTurn = infos.buildings[cvbot::EBuildingType::Palace].obsoleteSafeCommerceChanges[cvbot::ECommerce::Culture];
		const int numCapitals = globalInfo.numAliveCivPlayers;
		const int landPerCapital = game.getTurnNumber() * culturePerTurn >= cultureThreshold ? kRing2Land : kRing1Land;
		const int capitalLandCoveragePercent = 90; // Take into account some coastal starts.
		const int totalLandFromCapitals = numCapitals * landPerCapital * capitalLandCoveragePercent / 100;

		const int totalLand = globalInfo.globalDemographics[cvbot::GlobalInfo::Land].rivalAverage * (globalInfo.numAliveCivPlayers - 1) + globalInfo.globalDemographics[cvbot::GlobalInfo::Land].value;
		const int remainingLandFromSecondCities = std::max(0, totalLand - totalLandFromCapitals);
		const int numSecondCities = (remainingLandFromSecondCities + kRing1Land / 2) / kRing1Land;

		const int guessTotalCities = numCapitals + numSecondCities;
		return guessTotalCities > globalInfo.numAliveCivPlayers * 2;
		};
	
	canBarbsEnterTerritory = canBarbsEnterTerritory || barbsHaveEnteredTerritory || isItTime();

	MapUnitLookup enemyUnitMapLookup(enemyUnits);
	
	std::vector<Task> tasks;
	if (canBarbsEnterTerritory)
		for (const cvbot::City& city : myCities)
			tasks.emplace_back(CityDefenceTask{ &city });
	for (const auto& [coord, stack] : enemyUnitMapLookup.byCoord)
	{
		const cvbot::UnitInfo& unitInfo = infos.units[infos.unitClasses[stack[0]->klass].defaultType]; 
		if (unitInfo.isAnimal || unitInfo.domain != cvbot::EDomain::Land)
			continue;

		if (stack[0]->owner == cvbot::kBarbarianPlayer && !canBarbsEnterTerritory)
			continue;

		tasks.emplace_back(AntiBarbTask{ stack });
	}
	for (const cvbot::Unit& unit : myUnits)
		if (unit.klass == cvbot::EUnitClass::Settler)
			tasks.emplace_back(SettlerEscortTask{ &unit });
	if (futureSettlerEscortTarget)
		tasks.emplace_back(FutureSettlerEscortTask{ *futureSettlerEscortTarget });
	
	tasks.emplace_back(ScoutingTask{});

	const UnitAssignmentResult result = assignUnitTasks(game, mapGeom, infos, myUnits, tasks);

	for (const auto& assignment : result.assignments)
		std::visit(TaskExecutor{ game, *assignment.second.unit }, assignment.second.task);

	cityDefenceDemand = static_cast<int>(std::ranges::count_if(result.unassignedTasks, [](const Task& task) {
		return std::holds_alternative<CityDefenceTask>(task) || std::holds_alternative<FutureSettlerEscortTask>(task);
		}));
	antiBarbDemand = static_cast<int>(std::ranges::count_if(result.unassignedTasks, [](const Task& task) {
		return std::holds_alternative<AntiBarbTask>(task);
		}));
}
