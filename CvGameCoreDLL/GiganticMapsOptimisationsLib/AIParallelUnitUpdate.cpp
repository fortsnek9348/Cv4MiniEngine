#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "GameContext.h"
#include "AIParallelUnitUpdate_GroupUpdateRecord.h"
#include "AIParallelUnitUpdate_ConflictMap.h"
#include "AIParallelUnitUpdate_UnitPostUpdateQueue.h"
#include "Util.h"
#include "VectorisedPathfinder.h"
#include "VectorisedPathfinderGraph.h"
#include "TeamPathingPlotPropsCache.h"

#include <CommonStuff/CompilerMacros.h>
#include <CommonStuff/WorkStealingParallelFor.h>
#include <CommonStuff/ParallelExt.h>
#include <CommonStuff/ProfilerMarkers.h>
#include "CommonStuff/StatisticsRegistry.h"

#include "../CvPlayerAI.h"
#include "../CvTeamAI.h"
#include "../CvGameCoreUtils.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../CvDLLInterfaceIFaceBase.h"
#include "../CvInfos.h"

#include <array>
#include <mutex>
#include <iostream>
#include <syncstream>
#include <chrono>

using namespace GiganticMapsOptimisationsLib;

static constexpr bool kEnableParallelUnitUpdate = true;
static constexpr bool kDoDryRunsInSerial = false;
//static constexpr bool kForceEmptyMIS = false;
static constexpr bool kCheckMISResults = false;
static constexpr bool kEnableLogging = false;
static constexpr bool kEnableSerialLogging = true;
static constexpr int kSerialLoggingMinMilliseconds = 50;
static constexpr bool kDumpMISStats = false;
static constexpr bool kDumpTiming = false;

static constexpr int kMaxParallelMISPasses = 100;
static constexpr int kMaxParallelPasses = 6;

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::duration<double, std::milli>;

// Parallel implementation of void CvPlayerAI::AI_unitUpdate().
// Or as parallel as we can get.
// Unit updates are naturally very serial.
// There are all kinds of dependencies:
//     Active mission checks at a plot
//     Pathing: Team plot props, heuristic
//     Selection group split/join
//     Access to engine interfaces.
//
// So how do we do it?
//
// Baby steps:
//     Do a dry run of unit updates.
//     Predict which paths a unit will request. Cache them?
//     When it comes to the serial update:
//         Either you can do full path validation: Check that all team plot props and the heuristic are the same.
//         Or: Just check the path is still valid.
//     Only start with some unit AI types at first. Leave other stuff to the fallback.
// Pathing invalidation:
//     Determine which units can move without invalidating any paths at all. This excludes animals as animals won't stack.
//     Heuristic is never invalidated! Except by worker missions. Defer them to the end.
// Optimisations:
//     Reorder units to put most serial dependencies at the end.
//     Cache unit AI decisions, and just reuse those decisions as long as they are still valid (without checking all the stuff that they depend on).
//     Cache all possible unit AI decisions. That way, you might be able to deduplicate decision processing, and pick any valid decision during the serial update.
// Vanilla group order:
//     Vanilla AI orders groups by priority first, and keeps scanning the list of groups until nothing more can be done.
// Dominant AI procedures:
//     3.82 of 36.79% CvUnitAI::AI_anyAttack
//     3.77 of 36.79% CvUnitAI::AI_workerMove (AI_improveBonus)
//     1.42 of 36.79% CvUnitAI::AI_exploreMove
//     0.87 of 36.79% CvUnitAI::AI_attackMove
//     4.33 of 36.79% CvUnitAI::AI_patrol
//     4.33 of 36.79% CvUnitAI::AI_barbAttackMove
//     0.31 of 36.79% CvUnitAI::AI_exploreSeaMove
//     0.28 of 36.79% CvUnitAI::AI_reserveMove
//     0.26 of 36.79% CvUnitAI::AI_cityDefenseMove
//     0.23 of 36.79% CvUnitAI::AI_attackCityMove
// The True Solution(TM):
//     In parallel, loop through all units and generate a list of read/write dependencies super sets.
//     In parallel, group units to avoid conflicts (see: physics engine islands).
//     In serial, process each group in parallel. Check that all updates stayed within their dependencies list (else, defer unit to serial fallback).
//     Drawbacks:
//         Things like religion spread obviously depend on all cities religions. So it's impossible to do missionaries this way in parallel.
//         Solution: Out of all possible decisions, assign them in parallel. The assigned decisions should remain valid as they shouldn't depend on eachother. If they do, then we have problems.
// Separation of data:
//     Maintain all modified state locally, and then apply it to the game at the end.

// https://box2d.org/posts/2023/10/simulation-islands/

namespace
{
	// Using noinline in case it makes profiling more accurate.

	HECK_NOINLINE void doSeparation(CvPlayerAI& player)
	{
		CLLNode<int>* pCurrUnitNode = player.headGroupCycleNode();

		while (pCurrUnitNode)
		{
			CvSelectionGroup* const pLoopSelectionGroup = player.getSelectionGroup(pCurrUnitNode->m_data);
			pCurrUnitNode = player.nextGroupCycleNode(pCurrUnitNode);

			if (pLoopSelectionGroup->AI_isForceSeparate())
			{
				// do not split groups that are in the midst of attacking
				if (pLoopSelectionGroup->isForceUpdate() || !pLoopSelectionGroup->AI_isGroupAttack())
				{
					pLoopSelectionGroup->AI_separate();	// pointers could become invalid...
				}
			}
		}
	}

	constexpr int kNumPriorities = 17 + 1;

	HECK_NOINLINE std::array<std::vector<int>, kNumPriorities> doPriorityBucketing(const CvPlayerAI& player)
	{
		std::array<std::vector<int>, kNumPriorities> priorityBuckets;

		CLLNode<int>* pCurrUnitNode = player.headGroupCycleNode();

		while (pCurrUnitNode)
		{
			CvSelectionGroup* const pLoopSelectionGroup = player.getSelectionGroup(pCurrUnitNode->m_data);
			int vanillaPriority = player.AI_movementPriority(pLoopSelectionGroup);
			if (vanillaPriority >= 9)
				++vanillaPriority;
			// Do animals separately from other units to reduce pathing conflicts.
			if (pLoopSelectionGroup->getHeadUnitAI() == UNITAI_ANIMAL)
				vanillaPriority = 9;
			priorityBuckets[vanillaPriority].push_back(pCurrUnitNode->m_data);
			pCurrUnitNode = player.nextGroupCycleNode(pCurrUnitNode);
		}

		return priorityBuckets;
	}

	// CvUnitAI::AI_anyAttack (->AI_rangeAttack)
	//     Simple local search for enemy targets.
	//     Ends with MISSION_RANGE_ATTACK or MISSION_MOVE_TO.
	//     Dry run: Path to all possibilities.
	// CvUnitAI::AI_workerMove (->AI_improveBonus, AI_connectPlot)
	//     Might AI_retreatToCity.
	//     Might MISSION_ROUTE_TO.
	//     Global search for bonuses to improve. Dependant on AI_plotTargetMissionAIs.
	//     Connects cities. Local search.
	//     AI_improveCity, local search.
	//     AI_improveLocalPlot, local search.
	//     Global search in AI_fortTerritory. Search for new fort locations.
	//     Global search: AI_nextCityToImprove
	//     Global search: AI_routeTerritory. Dependant on AI_plotTargetMissionAIs.
	//     Global search: AI_connectBonus
	//     Global search: AI_irrigateTerritory. Dependant on AI_plotTargetMissionAIs.
	// CvUnitAI::AI_exploreMove
	//     Local AI_cityAttack/AI_anyAttack/AI_pillageRange.
	//     Local AI_goody, AI_patrol, AI_safety.
	//     Local AI_exploreRange. Dependant on AI_plotTargetMissionAIs.
	//     Global AI_explore search. Dependant on AI_plotTargetMissionAIs(range 3).
	//	   Global AI_travelToUpgradeCity.
	//     Global AI_retreatToCity.
	// CvUnitAI::AI_attackMove
	//     Global AI_guardCity. Dependant on AI_plotTargetMissionAIs().
	//     Local AI_heal.
	//     Global player unit loop in AI_group.
	//     Group merging.
	//     Air lift global loop in AI_guardCityAirlift.
	// CvUnitAI::AI_patrol
	//     Local
	// CvUnitAI::AI_barbAttackMove
	//     Local
	//     Global all cities in AI_targetCity.
	// CvUnitAI::AI_exploreSeaMove
	//     Local AI_anyAttack.
	//     AI_setUnitAIType
	//     Local explore.
	//     Local pillage.
	//     Global AI_explore. Dependant on AI_plotTargetMissionAIs.
	//     Global AI_pillage.
	//     AI_travelToUpgradeCity
	//     AI_patrol, AI_safety
	//     Global AI_retreatToCity
	// CvUnitAI::AI_reserveMove
	//     AI_guardCitySite. Dependant on AI_plotTargetMissionAIs().
	//     Local attacks.
	//     Global AI_protect.
	//     Local AI_defend, AI_safety
	//     Global AI_retreatToCity
	// CvUnitAI::AI_cityDefenseMove
	//     Local AI_leaveAttack
	//     Grouping.
	//     Local AI_guardCity.
	//     Global AI_guardCity.Dependant on AI_plotTargetMissionAIs().
	//     Local AI_safety
	//     Global AI_retreatToCity
	// CvUnitAI::AI_attackCityMove
	//     Local AI_guardCity
	//     Group local merging
	//     Local attack
	//     Global barb hunting
	//     Global AI_moveToStagingCity
	//     Local pillage
	//     Global AI_guardCity.Dependant on AI_plotTargetMissionAIs().
	//     Local AI_safety
	//     Global AI_retreatToCity

	// All game state is read-only here.
	// We determine what the unit will do as-if it was the first unit to move.
	// Then cache all probable decisions and paths.
	// Worst case is when unit missions collide, against AI_plotTargetMissionAIs checks. Let's hope that doesn't happen often.
	// We also determine what groups are likely to invalidate:
	//     Mission conflict detection
	//     Pathing team plot props
	//     Pathing heuristic
	// And which groups are likely to split/join.
	// When it comes to global search mission conflicts, we should probably count up the units that want to do the same global search,
	//  or maybe have some fixed number, then record multiple decision choices in the dry run to choose from in case of mission conflict.

	// Idea 2
	// "Unit SIMD".
	// Group units based on which AI proc they end up in.
	// Then process groups serially.
	// For each group, you're basically entering a single AI proc with multiple units. This makes things incredibly simple!
	// Assuming the AI proc is a global search with mission conflict avoidance:
	//     Assert: Movement of these units does not invalidate our own pathing.
	//            (Watch out for units on a war march. City capturing. And killing enemy units (changes plot danger))
	//     Enumerate targets into an array, avoiding mission conflicts from OTHER units (if there are mission conflicts with our own units, then abort all those missions).
	//     While there are units and targets:
	//         For each unit, in parallel:
	//             Our target list = targets sorted by distance (or whatever your value estimate is)
	//             best value = inf
	//             best target = null
	//             For each target in our target list:
	//                 If best possible value of remaining targets is greater or equal than best value:
	//                     break
	//                 Path to it.
	//                 If pathed and value is better:
	//                     best value = value
	//                     best target = target
	//         Reset target assignment for units that conflict with earlier missions.
    //         Remove assigned targets from target array
	//         Remove assigned units from unit array.
	//     Push missions (Track invalidation. If any unit's pathing got invalidated, redo that and the remaining units in serial.)
	//     Return [true/false per unit]
	// Could do multiple AI procs in parallel, if they don't invalidate each other's paths nor cause mission conflicts.

	struct DryRunRecordingPathFinder : IPathFinder
	{
		VectorisedPathfinderMap& pathfinderMap;
		IVectorisedPathfinder& pathfinder;

		struct Start
		{
			i16vec2 coord{};
			int mp = 0;

			friend bool operator==(Start, Start) = default;
		};

		std::optional<Start> activeStart;
		std::optional<uint32_t> activeFlags;

		struct PathInfo
		{
			const CvPlot* nextStepPlot{};
			const CvPlot* pathEndTurnPlot{};
			int lastNodeMP = 0;
			int lastNodeTurns = INT_MAX;
		} pathInfo{};

		i16aabb2 accVisitSet;
		uint32_t accTeamCostPlotPropsReadMask = 0;
		uint32_t accTeamValidityPlotPropsReadMask = 0;
		const ivec2 mapDim = CivMapGeometry(GC.getMapINLINE()).dim;

		explicit DryRunRecordingPathFinder(IVectorisedPathfinder& pathfinder, VectorisedPathfinderMap& map) : pathfinderMap(map), pathfinder(pathfinder)
		{
			// BUG FIX: Reset pathfinder before use.
			(void)pathfinder.reset({});
		}

		void flush()
		{
			// Accumulate the visit set and reset the astar instance.
			// TODO: This won't work well if wrapped!
			const i16aabb2 activeVisitSet = pathfinder.reset(pathfinder.getTeamValidityPlotPropsReadMask() & VectorisedPathfinderMap::getOwnUnitMovementTeamPlotPropsInvalidationMask());
			accVisitSet.combine(activeVisitSet);
		}

		virtual bool findPath(ivec2 start, ivec2 goal, unsigned int flags, int maxTurns, const CvSelectionGroup* group, int remainingMP) override
		{
			const Start newStart{ start, remainingMP };
			if (newStart != activeStart || flags != activeFlags)
			{
				flush();
				activeStart = newStart;
				activeFlags = flags;

				pathfinder.setConfig(group, flags);
				pathfinder.start(start, remainingMP);

				//static std::atomic_uint64_t pathSerial{};


				//if (flags & MOVE_SAFE_TERRITORY)
				//	__debugbreak();

				// Convenient place to do this.
				accTeamCostPlotPropsReadMask |= pathfinder.getTeamCostPlotPropsReadMask();
				accTeamValidityPlotPropsReadMask |= pathfinder.getTeamValidityPlotPropsReadMask();
			}

			const int pathCostLimit = calcPathCostUpperBound(*group, maxTurns);

			//const int goalPlotI = toPlotIndex(goal, mapDim);

			CvMap& map = GC.getMapINLINE();

			pathfinder.setHeuristicGoal(goal);
			pathfinder.updateOpenSetHeuristicValues();

			// TODO: This search does not account for "attacking dest" costs, like GameContext does.
			if (pathfinder.search(goal, pathCostLimit, false))
			{
				const std::vector<i16vec2> path = pathfinder.reconstructPath(start, goal);

				const auto plot = [&](i16vec2 coord) {
					//const ivec2 coord = toPlotCoord(plotI, mapDim);
					return map.plot(coord.x, coord.y);
					};

				const auto [endMP, endTurns] = VectorisedPathStepFunction::splitMPTurns(pathfinder.getUnitStateAt(goal));

				pathInfo = {
					.nextStepPlot = plot(path[std::min<size_t>(1, path.size() - 1)]),
					.lastNodeMP = endMP,
					.lastNodeTurns = endTurns,
				};

				for (const i16vec2 coord : path)
				{
					const auto [mp, turns] = VectorisedPathStepFunction::splitMPTurns(pathfinder.getUnitStateAt(coord));
					if (turns > 1)
						break;
					pathInfo.pathEndTurnPlot = plot(coord);
				}

				return true;
			}
			else
			{
				const CvPlot* const startPlot = map.plot(start.x, start.y);
				pathInfo = {
					.nextStepPlot = startPlot,
					.pathEndTurnPlot = startPlot,
				};
				return false;
			}
		}
		virtual const CvPlot* getNextStepPlot() const override
		{
			return pathInfo.nextStepPlot;
		}
		virtual const CvPlot* getPathEndTurnPlot() const override
		{
			return pathInfo.pathEndTurnPlot;
		}
		virtual int getLastNodeMP() const override
		{
			return pathInfo.lastNodeMP;
		}
		virtual int getLastNodeTurns() const override
		{
			return pathInfo.lastNodeTurns;
		}
	};

	//i16vec2 fakeUnitDependencyPlotCoord(const CvUnit* unit)
	//{
	//	return getPlotCoord(*unit->plot());
	//}
	//
	//i16vec2 fakeUnitDependencyPlotCoord(IDInfo id)
	//{
	//	return fakeUnitDependencyPlotCoord(::getUnit(id));
	//}
	//
	//i16vec2 fakeGroupDependencyPlotCoord(int id)
	//{
	//	return i16vec2(static_cast<int16_t>(id), 1000);
	//}

	//static constexpr uint32_t kUnitMovementFakePlotPropsBit = uint32_t(1) << 31;
	//static constexpr uint32_t kPlotInvisibilitySightDependencyBit = uint32_t(1) << 30;
	//static constexpr uint32_t kCityWorkersDependencyBit = uint32_t(1) << 29;

	enum class ELinearMemorySpace
	{
		PlotMisc, // toMemoryPlotAddress. Don't need the regional conflict map for this as there are no plot rects.
		MissionTargetUnit, // target unit id, mask is the missions
		GroupData, // group id
		Num,
	};

	struct LinearMemoryAddress
	{
		ELinearMemorySpace space{};
		uint32_t index{};

		friend auto operator<=>(LinearMemoryAddress, LinearMemoryAddress) = default;
	};

	uint32_t toMemoryPlotAddress(i16vec2 v)
	{
		return (v.y << 16) | v.x;
	}

	uint32_t toMemoryPlotAddress(IDInfo idInfo)
	{
		return (idInfo.eOwner << 24) ^ idInfo.iID;
	}

	template<size_t k>
		requires(k <= 32)
	std::bitset<32> extendMemoryAccessMask(std::bitset<k> b)
	{
		return std::bitset<32>(b.to_ullong());
	}

	struct PathingMemoryAccess
	{
		i16vec2 coord{};
		std::bitset<32> mask{};
	};

	struct LinearMemoryAccess
	{
		LinearMemoryAddress address{};
		std::bitset<32> mask{};
	};

	//std::pair<int, int> coordToPair(ivec2 v)
	//{
	//	return { v.x, v.y };
	//}

	struct GroupUpdateTask
	{
		GroupDryRunResult dryRunResult;
		i16aabb2 pathingMemoryReadRect{};
		uint32_t pathingTeamCostPlotPropsReadMask = 0;
		uint32_t pathingTeamValidityPlotPropsReadMask = 0;
		uint32_t movementTeamPlotPropsWriteMask = 0;
		std::vector<PathingMemoryAccess> pathingWrites;
		std::vector<PathingMemoryAccess> pathingReads; // Only potential conflicts, not everything!
		std::vector<LinearMemoryAccess> linearWrites;
		std::vector<LinearMemoryAccess> linearReads;
	};

	

	IDInfo getIDInfoIfNonNull(const CvUnit* unit)
	{
		return unit ? unit->getIDInfo() : IDInfo();
	}

	// f(coord, mask)
	void enumerateGroupWrites(const GroupUpdateTask& task, auto emitPathing, auto emitLinear)
	{
		const auto& state = task.dryRunResult.dryRunGroupState;
		const CvSelectionGroupAI& liveGroup = *state.liveGroup;

		const i16vec2 startCoord = getPlotCoord(*task.dryRunResult.dryRunGroupState.liveGroup->plot());
		const i16vec2 endCoord = task.dryRunResult.dryRunGroupState.units.front().coord;

		// Implicitly handled writes.
		const bool missionTypeChanged = liveGroup.getMissionType(0) != state.getMissionType(0);
		const bool missionAIChanged = liveGroup.AI_getMissionAIType() != state.AI_getMissionAIType();
		const bool missionXYChanged = (liveGroup.AI_getMissionAIPlot() ? (ivec2)getPlotCoord(*liveGroup.AI_getMissionAIPlot()) : ivec2(INVALID_PLOT_COORD)) != state.m_iMissionAICoord;
		const bool activityChanged = liveGroup.getActivityType() != state.activityType;
		const bool moved = startCoord != endCoord;

		/// Starting plot
		emitPathing(PathingMemoryAccess{
			.coord = startCoord,
			.mask = task.movementTeamPlotPropsWriteMask,
			});

		// Plot misc
		{
			LinearMemoryAccess writes{
				.address{ ELinearMemorySpace::PlotMisc, toMemoryPlotAddress(startCoord) },
			};
			if (moved)
				writes.mask[GroupUpdateRecord::PlotMiscAccess::kOurUnitMovement] = true;
			// Note that NO_MISSIONAI is filtered out by GroupUpdateRecord.
			if ((moved || missionAIChanged) && liveGroup.AI_getMissionAIType() != NO_MISSIONAI)
				writes.mask[(int)GroupUpdateRecord::PlotMiscAccess::kFirstGroupMissionAIAtThisPlot + (int)liveGroup.AI_getMissionAIType()] = true;
			//if ((moved || activityChanged) && liveGroup.getActivityType() != NO_ACTIVITY)
			//	writes.mask[GroupUpdateRecord::PlotMiscAccess::kGroupActivitiesAtThisPlot] = true;
			emitLinear(writes);
		}
		
		/// Ending plot
		emitPathing(PathingMemoryAccess{
			.coord = endCoord,
			.mask = task.movementTeamPlotPropsWriteMask,
			});
		// Plot misc
		{
			LinearMemoryAccess writes{
				.address{ ELinearMemorySpace::PlotMisc, toMemoryPlotAddress(endCoord) },
			};
			if (moved)
				writes.mask[GroupUpdateRecord::PlotMiscAccess::kOurUnitMovement] = true;
			if ((moved || missionAIChanged) && state.AI_getMissionAIType() != NO_MISSIONAI)
				writes.mask[(int)GroupUpdateRecord::PlotMiscAccess::kFirstGroupMissionAIAtThisPlot + (int)state.AI_getMissionAIType()] = true;
			//if (moved || activityChanged)
			//	writes.mask[GroupUpdateRecord::PlotMiscAccess::kGroupActivitiesAtThisPlot] = true;
			emitLinear(writes);
		}

		/// Mission target unit.
		const std::pair<IDInfo, MissionAITypes> oldUnitTargeting(getIDInfoIfNonNull(liveGroup.AI_getMissionAIUnit()), liveGroup.AI_getMissionAIType());
		const std::pair<IDInfo, MissionAITypes> newUnitTargeting(state.m_missionAIUnit, state.AI_getMissionAIType());
		if (oldUnitTargeting != newUnitTargeting)
		{
			// Note that NO_MISSIONAI is filtered out by GroupUpdateRecord.
			if (oldUnitTargeting.first != IDInfo() && oldUnitTargeting.second != NO_MISSIONAI)
			{
				LinearMemoryAccess writes{
					.address{ ELinearMemorySpace::MissionTargetUnit, toMemoryPlotAddress(oldUnitTargeting.first) },
					};
				writes.mask[oldUnitTargeting.second] = true;
				emitLinear(writes);
			}
			if (newUnitTargeting.first != IDInfo() && newUnitTargeting.second != NO_MISSIONAI)
			{
				LinearMemoryAccess writes{
					.address{ ELinearMemorySpace::MissionTargetUnit, toMemoryPlotAddress(newUnitTargeting.first) },
				};
				writes.mask[newUnitTargeting.second] = true;
				emitLinear(writes);
			}
		}

		/// Unit writes.
		//for (const auto& invalidation : result.unitUpdateRecord.plotUnitMovementDependencies)
		//	conflictGraph.markPotentialConflictPlot(invalidation);
		// Misc plot writes
		//for (const auto [coord, mask] : task.dryRunResult.unitUpdateRecord.plotMissionAccessWrites)
		//	f(MemoryAccess{ .coord = coord, .missionMask = mask });
		//// Unit writes
		//for (const auto [targetUnit, mask] : task.dryRunResult.unitUpdateRecord.unitGeneralMissionWrites)
		//	f(MemoryAccess{ .coord = fakeUnitDependencyPlotCoord(targetUnit), .missionMask = mask });

		//memAccess = {};
		//
		//// This unit's implicit writes.
		//
		//if (state.m_eMissionAIType != liveGroup.AI_getMissionAIType())
		//{
		//	if (state.m_eMissionAIType != NO_MISSIONAI)
		//		memAccess.missionMask[state.m_eMissionAIType] = true;
		//	if (liveGroup.AI_getMissionAIType() != NO_MISSIONAI)
		//		memAccess.missionMask[liveGroup.AI_getMissionAIType()] = true;
		//}
		//if (state.activityType != liveGroup.getActivityType())
		//	memAccess.missionMask[UnitUpdateRecord::kGroupActivity] = true;
		//if ((state.m_missionQueue.empty() ? NO_MISSION : state.m_missionQueue.front().eMissionType) != liveGroup.getMissionType(0))
		//	memAccess.missionMask[UnitUpdateRecord::kGroupMission] = true;
		//if (startCoord != endCoord)
		//	memAccess.missionMask[UnitUpdateRecord::kThisUnitMovement] = true;
		//
		//if (memAccess.missionMask.any())
		//{
		//	for (const auto& unit : state.units)
		//	{
		//		memAccess.coord = fakeUnitDependencyPlotCoord(unit.liveUnit.getIDInfo());
		//		f(memAccess);
		//	}
		//}

		/// Group writes
		{
			LinearMemoryAccess writes{
				.address{ ELinearMemorySpace::GroupData, static_cast<uint32_t>(liveGroup.getID()) },
			};
			if (missionTypeChanged)
				writes.mask[GroupUpdateRecord::SelectionGroupAccess::kMissionType] = true;
			if (missionAIChanged)
				writes.mask[GroupUpdateRecord::SelectionGroupAccess::kMissionAI] = true;
			if (activityChanged)
				writes.mask[GroupUpdateRecord::SelectionGroupAccess::kActivity] = true;
			if (missionXYChanged)
				writes.mask[GroupUpdateRecord::SelectionGroupAccess::kMissionXY] = true;
			emitLinear(writes);
		}
	}

	// f(coord, mask)
	void enumerateGroupReads(const GroupUpdateTask& task, auto emitLinear, auto emitPathingRegion)
	{
		// Pathing reads
		// Afaik, we should only need validity bits. Moving units should not change path costs.
		emitPathingRegion(task.pathingMemoryReadRect, task.pathingTeamValidityPlotPropsReadMask);

		const GroupUpdateRecord& record = task.dryRunResult.groupUpdateRecord;

		/// Misc plot reads
		for (const auto& [coord, mask] : record.getPlotMissionAccessReads())
			emitLinear(LinearMemoryAccess{ .address{ ELinearMemorySpace::PlotMisc, toMemoryPlotAddress(coord) }, .mask = extendMemoryAccessMask(mask) });
		/// Mission target unit reads.
		for (const auto& [targetUnitIdInfo, missionAIMask] : record.getTargetUnitMissionReads())
			emitLinear(LinearMemoryAccess{ .address{ ELinearMemorySpace::MissionTargetUnit, toMemoryPlotAddress(targetUnitIdInfo) }, .mask = extendMemoryAccessMask(missionAIMask) });
		// Unit reads
		//for (const auto& [unitIDInfo, mask] : task.dryRunResult.unitUpdateRecord.unitGeneralMissionReads)
		//	doPlot(MemoryAccess{ .coord = fakeUnitDependencyPlotCoord(unitIDInfo), .missionMask = mask });
		/// Group reads
		for (const auto& [groupId, mask] : record.getGroupAccesses())
			emitLinear(LinearMemoryAccess{ .address{ ELinearMemorySpace::GroupData, static_cast<uint32_t>(groupId) }, .mask = extendMemoryAccessMask(mask) });
	}

	void sortAndMergeMemoryAccessList(std::vector<PathingMemoryAccess>& v)
	{
		std::ranges::sort(v, std::less<>(), [](PathingMemoryAccess m) { return toMemoryPlotAddress(m.coord); });

		// Merge masks
		if (!v.empty())
		{
			size_t outI = 0;
			for (const size_t inI : range(size_t(1), v.size()))
			{
				if (v[inI].coord == v[outI].coord)
					v[outI].mask |= v[inI].mask;
				else
					v[++outI] = v[inI];
			}
			v.resize(outI + 1);
		}
	}

	void sortAndMergeMemoryAccessList(std::vector<LinearMemoryAccess>& v)
	{
		std::ranges::sort(v, std::less<>(), &LinearMemoryAccess::address);

		// Merge masks
		if (!v.empty())
		{
			size_t outI = 0;
			for (const size_t inI : range(size_t(1), v.size()))
			{
				if (v[inI].address == v[outI].address)
					v[outI].mask |= v[inI].mask;
				else
					v[++outI] = v[inI];
			}
			v.resize(outI + 1);
		}
	}

	// Inputs must be ordered and deduplicated by MemoryAddress!
	bool hasMemoryAccessConflict(std::span<const PathingMemoryAccess> A, std::span<const PathingMemoryAccess> B)
	{
		size_t a = 0;
		size_t b = 0;

		while (a < A.size() && b < B.size())
		{
			if (toMemoryPlotAddress(A[a].coord) < toMemoryPlotAddress(B[b].coord))
				++a;
			else if (toMemoryPlotAddress(A[a].coord) > toMemoryPlotAddress(B[b].coord))
				++b;
			else
			{
				if ((A[a].mask & B[b].mask).any())
					return true;
				++a;
				++b;
			}
		}

		return false;
	}

	// Inputs must be ordered and deduplicated by MemoryAddress!
	bool hasMemoryAccessConflict(std::span<const LinearMemoryAccess> A, std::span<const LinearMemoryAccess> B)
	{
		size_t a = 0;
		size_t b = 0;

		while (a < A.size() && b < B.size())
		{
			if (A[a].address < B[b].address)
				++a;
			else if (A[a].address > B[b].address)
				++b;
			else
			{
				if ((A[a].mask & B[b].mask).any())
					return true;
				++a;
				++b;
			}
		}

		return false;
	}

	bool calcActualGroupConflict(const GroupUpdateTask& A, const GroupUpdateTask& B)
	{
		// There's a bunch of conflicts that have a false write-write hazard. Should probably mask them off. Are all writes safe to do in any order as long as there's no reads?
		return //hasMemoryAccessConflict(A.writes, B.writes)
			false
			|| hasMemoryAccessConflict(A.pathingWrites, B.pathingReads)
			|| hasMemoryAccessConflict(B.pathingWrites, A.pathingReads)
			|| hasMemoryAccessConflict(A.linearWrites, B.linearReads)
			|| hasMemoryAccessConflict(B.linearWrites, A.linearReads)
			;
	}

	struct ThreadLocalData
	{
		std::unique_ptr<IVectorisedPathfinder> astar;
		//std::unique_ptr<IVectorisedPathfinder> astar = createPlotwiseAStarVectorisedPathfinder();
		//std::vector<int> potentialConflicts;
	};

	struct ConflictsContainer
	{
		RegionalConflictMap pathingRegionalConflictMap;
		LinearConflictMap linearConflictMaps[(int)ELinearMemorySpace::Num];

		explicit ConflictsContainer(const CvMap& map, const CvPlayer& player)
			: pathingRegionalConflictMap(map)
			, linearConflictMaps{
				LinearConflictMap(player.getNumSelectionGroups() * 2), // plot misc
				LinearConflictMap(player.getNumSelectionGroups()), // target units
				LinearConflictMap(player.getNumSelectionGroups()), // group data
			}
		{
		}
	};

	// Conflict generation:
	// In parallel, for each group: Mark all writes in a bitmap.
	// In parallel, for each group: Add all reads and writes to conflicts and tasks if the corresponding write mask bit is set.
	//                              For regional conflicts (pathing), writes can be quickly looked up given a read rect.

	HECK_NOINLINE void generateUnitUpdateRecords(const CvPlayerAI& player, std::span<const int> groupIds, std::span<GroupUpdateTask> groupUpdateTasks,
		VectorisedPathfinderMap& pathfinderMap,
		ConflictsContainer& conflictsContainer,
		std::span<ThreadLocalData> threadLocalData)
	{
		// A seed used to seed all the group update tasks.
		const uint32_t updateProcessSeed = GC.getGameINLINE().getSorenRand().get(UINT16_MAX, "generateUnitUpdateRecords");

		/// Generate the update records.
		// And while we're at it, start on the conflict graph by populating with the writes.
		// Use work stealing as unit update performance can vary wildly.
		heck::parallelWorkStealingForEachN(groupIds.size(), [&player, &pathfinderMap, &groupUpdateTasks, &conflictsContainer, groupIds, threadLocalData, updateProcessSeed](size_t threadI, size_t taskI) {
			//static std::mutex mutex; const std::lock_guard lock(mutex);
			const CvSelectionGroupAI* const liveGroup = static_cast<const CvSelectionGroupAI*>(player.getSelectionGroup(groupIds[taskI]));

			GroupUpdateTask& task = groupUpdateTasks[taskI];

			if (!liveGroup || !liveGroup->plot())
				task.dryRunResult.setUseSerialFallback("No group/plot!");

			// If a previous parallel update already marked this for serial update, then don't bother dry running again.
			if (task.dryRunResult.getSerialFallbackReason())//|| task.dryRunResult.parallelMISUpdateDone)
				return;

			DryRunRecordingPathFinder pathFinder(*threadLocalData[threadI].astar, pathfinderMap);

			task.dryRunResult = dryrun_CvSelectionGroupAI_AI_update(*liveGroup, pathFinder, updateProcessSeed);
			

			// If the return value is true, the group loop exits, which is not very compatible with a parallel unit update.
			if (task.dryRunResult.returnValue)
				task.dryRunResult.setUseSerialFallback("task.dryRunResult.returnValue");
			//task.dryRunResult.useSerialFallback |= analyseOnly;

			if (task.dryRunResult.getSerialFallbackReason())
				return;

			pathFinder.flush();
			task.pathingMemoryReadRect.combine(pathFinder.accVisitSet.cast<int16_t>());
			task.pathingTeamCostPlotPropsReadMask = pathFinder.accTeamCostPlotPropsReadMask;
			task.pathingTeamValidityPlotPropsReadMask = pathFinder.accTeamValidityPlotPropsReadMask;
			task.movementTeamPlotPropsWriteMask = VectorisedPathfinderMap::getOwnUnitMovementTeamPlotPropsInvalidationMask(*liveGroup);

			// Mark potential conflict plots.
			enumerateGroupWrites(task,
				[&](PathingMemoryAccess memAccess) {
				task.pathingWrites.push_back(memAccess);
				conflictsContainer.pathingRegionalConflictMap.markPotentialConflictPlot(memAccess.coord);
			},
				[&](LinearMemoryAccess memAccess) {
				task.linearWrites.push_back(memAccess);
				conflictsContainer.linearConflictMaps[(int)memAccess.address.space].markPotentialConflictIndex(memAccess.address.index);
			});

			sortAndMergeMemoryAccessList(task.pathingWrites);
			sortAndMergeMemoryAccessList(task.linearWrites);

			}, kDoDryRunsInSerial);
	}

	HECK_NOINLINE void buildConflictGraph(std::span<GroupUpdateTask> groupUpdateTasks, ConflictsContainer& conflictsContainer)
	{
		/// Build the rest of the conflict graph.
		heck::parallelForEachN(static_cast<int>(groupUpdateTasks.size()), [&](int taskI) {
			GroupUpdateTask& task = groupUpdateTasks[taskI];

			/// Writes
			for (const PathingMemoryAccess& access : task.pathingWrites)
				conflictsContainer.pathingRegionalConflictMap.addPotentialConflictAtPlot(access.coord, taskI);
			for (const LinearMemoryAccess& access : task.linearWrites)
				conflictsContainer.linearConflictMaps[(int)access.address.space].addPotentialConflict(access.address.index, taskI);

			/// Reads
			enumerateGroupReads(task,
				[&](LinearMemoryAccess access) {
				LinearConflictMap& map = conflictsContainer.linearConflictMaps[(int)access.address.space];
				if (map.isPotentialConflictIndex(access.address.index))
					task.linearReads.push_back(access);
				map.addPotentialConflict(access.address.index, taskI);
			},
				[&](i16aabb2 rect, std::bitset<32> pathingMask) {
					// Don't take a dependency if your read mask will not be invalidated by any other unit.
					// This is critical for barbs attacking cities (AI_targetCity), or they'll take thousands of dependencies because of how many animals their pathing overlaps.
					if ((pathingMask & std::bitset<32>(VectorisedPathfinderMap::getOwnUnitMovementTeamPlotPropsInvalidationMask())).any())
					{
						size_t totalPlots = 0;

						conflictsContainer.pathingRegionalConflictMap.enumeratePotentialConflictsPlotsInRect(rect, [&, pathingMask](ivec2 coord) {
							task.pathingReads.push_back(PathingMemoryAccess{ .coord = coord, .mask = pathingMask });
							conflictsContainer.pathingRegionalConflictMap.addPotentialConflictAtPlot(coord, taskI);
							++totalPlots;
							});
						if (totalPlots >= 1000)
						{
							std::osyncstream(std::clog) << "Huge pathing rect for UnitAI "
								<< toString(task.dryRunResult.dryRunGroupState.liveGroup->getHeadUnitAI())
								<< ": " << rect.area() << " plots"
								<< ", " << totalPlots << " potential conflicts"
								<< ", pathingMask=" << pathingMask
								<< '\n';
						}
					}
			});

			sortAndMergeMemoryAccessList(task.pathingReads);
			sortAndMergeMemoryAccessList(task.linearReads);
		});
	}

	struct [[nodiscard]] ParallelUpdateRemainderStats
	{
		size_t numInsideMIS = 0;
		size_t numOutsideMIS = 0;
		size_t numSerialFallback = 0;
	};


	// For a task, gather up the potential conflicts by looking them up in the conflict maps.
	static void gatherPotentialConflicts(std::vector<int>& potentialConflicts, int taskI, const GroupUpdateTask& taskA, const ConflictsContainer& conflictsContainer)
	{
		const auto appendPotentialConflict = [&, taskI](int adjTaskI) {
			if (adjTaskI != taskI) // Don't conflict with self
				if (potentialConflicts.empty() || potentialConflicts.back() != adjTaskI) // Avoid trivial duplicates
					potentialConflicts.push_back(adjTaskI);
			};

		for (const PathingMemoryAccess& access : taskA.pathingWrites)
			conflictsContainer.pathingRegionalConflictMap.enumeratePotentialConflictsAtPlot(access.coord, appendPotentialConflict);
		for (const PathingMemoryAccess& access : taskA.pathingReads)
			conflictsContainer.pathingRegionalConflictMap.enumeratePotentialConflictsAtPlot(access.coord, appendPotentialConflict);

		for (const LinearMemoryAccess& access : taskA.linearWrites)
			conflictsContainer.linearConflictMaps[(int)access.address.space].enumeratePotentialConflicts(access.address.index, appendPotentialConflict);
		for (const LinearMemoryAccess& access : taskA.linearReads)
			conflictsContainer.linearConflictMaps[(int)access.address.space].enumeratePotentialConflicts(access.address.index, appendPotentialConflict);
	}

	HECK_NOINLINE ParallelUpdateRemainderStats parallelMIS(std::span<GroupUpdateTask> groupUpdateTasks, const ConflictsContainer& conflictsContainer)
	{
		/// Do parallel MIS to generate a parallel subset.
		// <https://en.wikipedia.org/wiki/Maximal_independent_set#Random-permutation_parallel_algorithm_[Blelloch's_Algorithm]>
		// The algorithm works by iterating over the nodes multiple times.
		// A node is added to the MIS if all its neighbours have greater index.
		// Then that node an all neighbours are removed from the graph.
		
		// Hope this doesn't cause unnecessary cacheline sharing.
		ParallelUpdateRemainderStats stats{};

		bool earlyExit = false;

		for (const int stepFromZero : range(kMaxParallelMISPasses))
		{
			std::atomic_bool changed = true;
			changed.store(false, std::memory_order_relaxed);
			heck::parallelForEachN(static_cast<int>(groupUpdateTasks.size()), [&groupUpdateTasks, &conflictsContainer, &changed, &stats, stepFromOne = static_cast<uint8_t>(stepFromZero + 1)] HECK_NOINLINE_PRE (int taskI) HECK_NOINLINE_POST {

				GroupUpdateTask& taskA = groupUpdateTasks[taskI];

				if (taskA.dryRunResult.getSerialFallbackReason())// || taskA.dryRunResult.parallelMISUpdateDone)
				{
					if (stepFromOne == 1)
						std::atomic_ref(stats.numSerialFallback).fetch_add(1, std::memory_order_relaxed);
					return;
				}

				const auto wasRemoved = [stepFromOne, &groupUpdateTasks](int adjTaskI) {
					const auto& adjResult = groupUpdateTasks[adjTaskI].dryRunResult;
					const int removedStep = std::atomic_ref(adjResult.parallelMISRemovalStep).load(std::memory_order_relaxed);
					return 0 < removedStep && removedStep < stepFromOne
						// Or, it was already selected in a previous parallel update.
						// Or, selected in this parallel update, but in which case, this group either conflicts or will not be selected.
						//|| std::atomic_ref(adjResult.parallelMISSelect).load(std::memory_order_relaxed);
						;
					};

				if (wasRemoved(taskI))
					return;

				// The MIS step: Gather up all the groups whose neighbours are all greater than their ID.

				static thread_local std::vector<int> tlsPotentialConflicts;
				auto& potentialConflicts = tlsPotentialConflicts;
				potentialConflicts.clear();
				gatherPotentialConflicts(potentialConflicts, taskI, taskA, conflictsContainer);

				//std::ranges::sort(potentialConflicts);
				//potentialConflicts.erase(std::ranges::unique(potentialConflicts).begin(), potentialConflicts.end());

				const auto hasEdgeTo = [&taskA, &groupUpdateTasks](int adjTaskI) {
					//assert(calcActualGroupConflict(taskA, groupUpdateTasks[adjTaskI]) == calcActualGroupConflict(groupUpdateTasks[adjTaskI], taskA));
					return calcActualGroupConflict(taskA, groupUpdateTasks[adjTaskI]);
					};

				const auto getKey = [&](int taskI) {
					//return std::tuple{ -groupUpdateTasks[taskI].pathingRect.area(), taskI };
					return taskI;
					};

				const bool takeMe = std::ranges::all_of(potentialConflicts, [taskI, hasEdgeTo, wasRemoved, getKey](int adjTaskI) {
					if (getKey(taskI) < getKey(adjTaskI))
						return true;
					if (wasRemoved(adjTaskI))
						return true;
					if (!hasEdgeTo(adjTaskI))
						return true;
					return false;
					});

				//if (taskI == 49 && stepFromOne == 10)
				//	__debugbreak();

				if (takeMe)
				{
					std::atomic_ref(taskA.dryRunResult.parallelMISSelect).store(true, std::memory_order_relaxed);
					std::atomic_ref(stats.numInsideMIS).fetch_add(1, std::memory_order_relaxed);

					// Remove this and neighbours.
					std::atomic_ref(taskA.dryRunResult.parallelMISRemovalStep).store(stepFromOne, std::memory_order_relaxed);

					for (const int adjTaskI : potentialConflicts)
					{
						// Because takeMe is true, we've already tested the edges where adjTaskI <= taskI.
						if (!wasRemoved(adjTaskI) && hasEdgeTo(adjTaskI))
							std::atomic_ref(groupUpdateTasks[adjTaskI].dryRunResult.parallelMISRemovalStep).store(stepFromOne, std::memory_order_relaxed);
					}

					if (!changed.load(std::memory_order_relaxed))
						changed.store(true, std::memory_order_relaxed);
				}

				});

			if (!changed)
			{
				earlyExit = true;
				if constexpr (kEnableLogging)
					std::clog << "Parallel MIS !changed exit at iteration " << stepFromZero << std::endl;
				break;
			}
		}
		if constexpr (kEnableLogging)
		{
			if (!earlyExit)
				std::clog << "Parallel MIS exit at max iterations." << std::endl;
		}

		stats.numOutsideMIS = groupUpdateTasks.size() - stats.numInsideMIS - stats.numSerialFallback;
		return stats;
	}

	struct SerialFallback
	{
		int groupId = 0;
		std::string reason;
	};

	struct [[nodiscard]] ParallelUpdateRemainder
	{
		ParallelUpdateRemainderStats stats;
		std::unique_ptr<int[]> parallelConflicts;
		std::unique_ptr<SerialFallback[]> serialFallbacks;
	};

	HECK_NOINLINE ParallelUpdateRemainder applyGroupUpdates(PlayerTypes playerI, std::span<const GroupUpdateTask> groupUpdateTasks, ParallelUpdateRemainderStats stats)
	{
		/// Apply the results of the parallel subset.
		ParallelUpdateRemainder remainder{
			.stats = stats,
			.parallelConflicts = std::make_unique_for_overwrite<int[]>(stats.numOutsideMIS),
			.serialFallbacks = std::make_unique_for_overwrite<SerialFallback[]>(stats.numSerialFallback),
		};

		std::atomic_size_t nextParallelConflictI{};
		std::atomic_size_t nextSerialFallbackI{};

		UnitPostUpdateQueue postUpdateQueue(playerI);
		heck::parallelForEachN(groupUpdateTasks.size(), [&](size_t taskI) {
			const GroupUpdateTask& task = groupUpdateTasks[taskI];
			if (task.dryRunResult.parallelMISSelect)// && !task.dryRunResult.parallelMISUpdateDone)
			{
				// Reset MIS stuff here.
				//task.dryRunResult.parallelMISSelect = false;
				//task.dryRunResult.parallelMISRemovalStep = 0;
				//task.dryRunResult.parallelMISUpdateDone = true;
				// HACK: const_cast. The pointer is const to avoid accidents in the dry run, and reusing this pointer should be faster than looking up by group id.
				//       Same for the unit.
				const_cast<CvSelectionGroupAI&>(*task.dryRunResult.dryRunGroupState.liveGroup).applyParallelUnitUpdateResult(task.dryRunResult.dryRunGroupState);
				for (const DryRunUnitState& unitState : task.dryRunResult.dryRunGroupState.units)
					const_cast<CvUnitAI&>(unitState.liveUnit).applyParallelUnitUpdateResult(unitState, postUpdateQueue);
			}
			else if (task.dryRunResult.getSerialFallbackReason())
			{
				if (task.dryRunResult.dryRunGroupState.liveGroup)
				{
					remainder.serialFallbacks[nextSerialFallbackI.fetch_add(1, std::memory_order_relaxed)] = {
						.groupId = task.dryRunResult.dryRunGroupState.liveGroup->getID(),
						.reason = task.dryRunResult.getSerialFallbackReason(),
					};
				}
			}
			else
				remainder.parallelConflicts[nextParallelConflictI.fetch_add(1, std::memory_order_relaxed)] = task.dryRunResult.dryRunGroupState.liveGroup->getID();
			});

		postUpdateQueue.doPostUpdateStuff();

		if (nextParallelConflictI != stats.numOutsideMIS)
			std::abort();
		if (nextSerialFallbackI > stats.numSerialFallback)
			std::abort();
		remainder.stats.numSerialFallback = nextSerialFallbackI;

		return remainder;
	}

	[[nodiscard]] HECK_NOINLINE bool applySerialUpdates(const CvPlayer& player, std::span<const SerialFallback> serialFallbacks)
	{
		/// Do the rest in serial.
		[[maybe_unused]] size_t serialUnitI = 0;
		[[maybe_unused]] size_t unitI = 0;

		// Doing the serial fallback groups first might early-out ealier.

		for (const auto& [groupId, reason] : serialFallbacks)
		{
			//if (task.dryRunResult.isUseSerialFallback())
			{
				//auto& group = const_cast<CvSelectionGroupAI&>(*task.dryRunResult.dryRunGroupState.liveGroup);
				CvSelectionGroupAI* const group = static_cast<CvSelectionGroupAI*>(player.getSelectionGroup(groupId));
				if (!group) // this can happen
					continue;
				//if constexpr (kEnableSerialLogging)
				//	std::wclog << getFullPlotDescription(*group->plot()) << L": " << toString(group->getHeadUnitAI()) << std::endl;
				const auto t0 = Clock::now();
				//if (group.getID() == 10978618)
				//	__debugbreak();
				const bool result = group->AI_update();
				const auto t1 = Clock::now();
				if constexpr (kEnableSerialLogging)
				{
					const auto duration = t1 - t0;
					heck::StatisticsRegistry::getInstance().grab<heck::TimingStatistic>(std::string("Parallel unit update serial fallback for \"") + reason + '"')
						.addSample(duration);
					if (duration >= std::chrono::duration<int, std::milli>(kSerialLoggingMinMilliseconds))
					{
						std::clog << "Group ID " << group->getID() << " serial update took " << Duration(duration) << " (" << reason << ')' << std::endl;
						std::wclog << L'\t' << getFullPlotDescription(*group->plot()) << L": " << std::flush;
						std::clog << toString(group->getHeadUnitAI()) << std::endl;
					}
				}
				if (result)
				{
					//if constexpr (kEnableSerialLogging)
					//	std::clog << "Early-out on unit " << unitI << " (serial " << serialUnitI << ')' << std::endl;
					return true;
				}
				++serialUnitI;
			}
			++unitI;
		}

		//for (const GroupUpdateTask& task : groupUpdateTasks)
		//{
		//	//if (task.dryRunResult.isUseSerialFallback() || analyseOnly)
		//	if ((!task.dryRunResult.isUseSerialFallback() && !task.dryRunResult.parallelMISSelect) || analyseOnly)
		//	{
		//		if constexpr (kEnableSerialLogging)
		//			std::wclog << getFullPlotDescription(*task.dryRunResult.dryRunGroupState.liveGroup->plot()) << L": " << toString(task.dryRunResult.dryRunGroupState.liveGroup->getHeadUnitAI()) << std::endl;
		//		if (const_cast<CvSelectionGroupAI&>(*task.dryRunResult.dryRunGroupState.liveGroup).AI_update())
		//		{
		//			std::clog << "Early-out on unit " << unitI << " (serial " << serialUnitI << ')' << std::endl;
		//			return true;
		//		}
		//		++serialUnitI;
		//	}
		//	++unitI;
		//}

		return false;
	}

	

	ParallelUpdateRemainder findAndUpdateMIS(CvPlayerAI& player, std::span<const int> groupIds, VectorisedPathfinderMap& pathfinderMap,
		std::span<ThreadLocalData> threadLocalData)
	{
		// 320KB per 1000 units. 1.25MB per 4000 units (the practical cap).
		std::vector<GroupUpdateTask> groupUpdateTasks(groupIds.size());

		ConflictsContainer conflictContainer(GC.getMapINLINE(), player);

		/// Update pathfinder data
		pathfinderMap.update(player.getTeam());

		generateUnitUpdateRecords(player, groupIds, groupUpdateTasks, pathfinderMap, conflictContainer, threadLocalData);

		conflictContainer.pathingRegionalConflictMap.prepareForConflictHashing();

		buildConflictGraph(groupUpdateTasks, conflictContainer);

		//if constexpr (kEnableLogging)
		//{
		//	if (analyseOnly)
		//	{
		//		/// Dump dry run results and memory access info.
		//
		//		for (const GroupUpdateTask& task : groupUpdateTasks)
		//		{
		//			const auto& group = task.dryRunResult.dryRunGroupState;
		//			std::clog << "Group " << group.liveGroup->getID() << std::endl;
		//			std::clog << "useSerialFallback = " << task.dryRunResult.isUseSerialFallback() << std::endl;
		//			std::clog << "Units:" << std::flush;
		//			for (const DryRunUnitState& unit : group.units)
		//				std::wclog << L' ' << unit.liveUnit.getName();
		//			std::wclog << std::flush;
		//			std::clog << std::endl;
		//
		//			CvWString str;
		//			if (!group.m_missionQueue.empty())
		//				getMissionTypeString(str, group.m_missionQueue.front().eMissionType);
		//			std::wclog << L"Mission: " << str << std::endl;
		//			getUnitAIString(str, group.getHeadUnitAI());
		//			std::wclog << L"UnitAI: " << str << std::endl;
		//			for (const MemoryAccess& access : task.reads)
		//				std::clog << "Read : " << access.coord << std::bitset<64>(access.mask) << std::endl;
		//			for (const MemoryAccess& access : task.writes)
		//				std::clog << "Write: " << access.coord << std::bitset<64>(access.mask) << std::endl;
		//		}
		//	}
		//}

		const ParallelUpdateRemainderStats stats = parallelMIS(groupUpdateTasks, conflictContainer);

		/// Debug: Check results.
		if constexpr (kCheckMISResults)
		{
			//std::vector<int> mis;
			//mis.reserve(groupIds.size());
			//for (const int groupId : groupIds)
			//	if (groupUpdateTasks[groupId].dryRunResult.parallelMISSelect)
			//		mis.push_back(groupId);

			std::vector<int> numConflictsWithMIS(groupUpdateTasks.size());
			std::atomic_int numInSerial{};
			std::atomic_int numInMIS{};

			heck::parallelForEachN(static_cast<int>(groupIds.size()), [&] HECK_NOINLINE_PRE (int taskI) HECK_NOINLINE_POST {

				const GroupUpdateTask& taskA = groupUpdateTasks[taskI];

				if (taskA.dryRunResult.getSerialFallbackReason())
				{
					numInSerial.fetch_add(1, std::memory_order_relaxed);
					return;
				}

				if (taskA.dryRunResult.parallelMISSelect)
					numInMIS.fetch_add(1, std::memory_order_relaxed);

				for (const auto& [taskBIndex, taskB] : groupUpdateTasks | std::views::enumerate)
				{
					if (&taskA == &taskB || taskB.dryRunResult.getSerialFallbackReason())
						continue;

					const bool areBothInMIS = taskA.dryRunResult.parallelMISSelect && taskB.dryRunResult.parallelMISSelect;

					if (areBothInMIS || taskB.dryRunResult.parallelMISSelect)
					{
						if (calcActualGroupConflict(taskA, taskB))
						{
							std::vector<int> potentialConflicts;
							gatherPotentialConflicts(potentialConflicts, taskI, taskA, conflictContainer);
							if (!std::ranges::contains(potentialConflicts, taskBIndex))
								std::abort(); // Actual conflict but no potential conflict

							if (areBothInMIS)
								std::abort(); // contradiction with MIS

							if (taskB.dryRunResult.parallelMISSelect)
								++numConflictsWithMIS[taskI];
						}
					}
					// else, don't need to calculate conflict
				}

				});

			// Now, for all grups NOT in the MIS, check that they indeed have conflicts with the MIS.
			const int numOutsideMIS = static_cast<int>(groupUpdateTasks.size() - std::ranges::count(numConflictsWithMIS, 0)); // count non-zero
			if (numOutsideMIS + numInMIS + numInSerial != groupUpdateTasks.size())
				std::abort();

			// Show some examples of conflicts.
			if (numOutsideMIS > 0)
			{
				const auto dumpTask = [&](const GroupUpdateTask& task) {
					const auto& group = task.dryRunResult.dryRunGroupState;
					std::clog << "Group " << group.liveGroup->getID() << std::endl;
					std::clog << "Serial fallback reason = " << task.dryRunResult.getSerialFallbackReason() << std::endl;
					std::clog << "Units:" << std::flush;
					for (const DryRunUnitState& unit : group.units)
						std::wclog << L' ' << unit.liveUnit.getName();
					std::wclog << std::flush;
					std::clog << std::endl;

					CvWString str;
					if (!group.m_missionQueue.empty())
						getMissionTypeString(str, group.m_missionQueue.front().eMissionType);
					std::wclog << L"Mission: " << str << std::endl;
					getUnitAIString(str, group.getHeadUnitAI());
					std::wclog << L"UnitAI: " << str << std::endl;
					for (const PathingMemoryAccess& access : task.pathingReads)
						std::clog << "Pathing Read : coord=" << access.coord << ", mask=" << access.mask << std::endl;
					for (const PathingMemoryAccess& access : task.pathingWrites)
						std::clog << "Pathing Write: coord=" << access.coord << ", mask=" << access.mask << std::endl;
					for (const LinearMemoryAccess& access : task.linearReads)
						std::clog << "Linear Read : space=" << std::to_underlying(access.address.space) << ", index=" << std::hex << access.address.index << std::dec << ", mask=" << access.mask << std::endl;
					for (const LinearMemoryAccess& access : task.linearWrites)
						std::clog << "Linear Write: space=" << std::to_underlying(access.address.space) << ", index=" << std::hex << access.address.index << std::dec << ", mask=" << access.mask << std::endl;
					};

				size_t numOutput = 0;
				for (const size_t taskI : range(groupUpdateTasks.size()))
				{
					if (numConflictsWithMIS[taskI] > 0)
					//if (groupUpdateTasks[taskI].dryRunResult.dryRunGroupState.liveGroup->getID() == 33442205)
					{
						std::clog << "Example conflict outside MIS: " << taskI << " (group " << groupUpdateTasks[taskI].dryRunResult.dryRunGroupState.liveGroup->getID() << "):";
						for (const size_t taskBI : range(groupUpdateTasks.size()))
							if (groupUpdateTasks[taskBI].dryRunResult.parallelMISSelect)
								if (calcActualGroupConflict(groupUpdateTasks[taskI], groupUpdateTasks[taskBI]))
								{
									std::clog << ' ' << groupUpdateTasks[taskBI].dryRunResult.dryRunGroupState.liveGroup->getID();
									
									dumpTask(groupUpdateTasks[taskI]);
									dumpTask(groupUpdateTasks[taskBI]);
								}
						std::clog << std::endl;

						if (++numOutput >= 4)
							break;
					}
				}
			}
		}

		if constexpr (kDumpMISStats)
		{
			int numInSerial{};
			int numInMIS{};

			for (const GroupUpdateTask& task : groupUpdateTasks)
			{
				if (task.dryRunResult.getSerialFallbackReason())
					++numInSerial;

				if (task.dryRunResult.parallelMISSelect)
					++numInMIS;
			}

			// Now, for all grups NOT in the MIS, check that they indeed have conflicts with the MIS.
			const size_t numOutsideMIS = groupUpdateTasks.size() - (numInMIS + numInSerial);

			std::clog << __func__ << ": Groups = " << groupUpdateTasks.size()
				<< ", numInSerial = " << numInSerial
				<< ", numInMIS = " << numInMIS
				<< ", numOutsideMIS = " << numOutsideMIS
				<< std::endl;
		}

		return applyGroupUpdates(player.getID(), groupUpdateTasks, stats);
	}

	[[nodiscard]] bool runUpdatesOnGroupList(CvPlayerAI& player, std::span<const int> groupIds, VectorisedPathfinderMap& pathfinderMap)
	{
		// Do the equivelent of:
		//
		// for (const int selGroupId : bucket)
		// {
		// 	if (CvSelectionGroup* const pLoopSelectionGroup = player.getSelectionGroup(selGroupId))  // group might have been killed by a previous group update
		// 	{
		// 		if (pLoopSelectionGroup->AI_update())
		// 		{
		// 			return;
		// 		}
		// 	}
		// }
		//
		// ...but in parallel!

		// Some AI proces use bonus values. Precalculate them.
		for (const auto i : heck::range<BonusTypes>(GC.getNumBonusInfos()))
			(void)player.AI_bonusVal(i);

		//if (analyseOnly)
		//{
		//	std::clog << __FUNCTION__ ": Player " << (int)player.getID() << std::endl;
		//	std::clog << __FUNCTION__ ": num groups = " << groupIds.size() << std::endl;
		//}

		const auto t0 = Clock::now();

		// TODO: This is a lot of memory for barbarians. Approaching 1MB.
		//std::vector<GroupUpdateTask> groupUpdateTasks(groupIds.size());

		// An A* for each thread.
		// This is a ~5.3MB init per thread.
		// TODO: This should be part of GameContext.
		static std::vector<ThreadLocalData> gThreadLocalData;
		gThreadLocalData.resize(heck::getParallelWorkStealingNumThreads());

		//ConflictMap conflictMap(GC.getMapINLINE());

		for (ThreadLocalData& data : gThreadLocalData)
			data.astar = createCoordPlotwiseAStarVectorisedPathfinder(pathfinderMap);

		ParallelUpdateRemainder remainder;

		std::vector<SerialFallback> accSerialFallback;

		for ([[maybe_unused]] const int parallelUpdateI : range(kMaxParallelPasses))
		{
			if (groupIds.size() <= 1)
				break;

			/// Update pathfinder data
			pathfinderMap.update(player.getTeam());

			remainder = findAndUpdateMIS(player, groupIds, pathfinderMap, gThreadLocalData);
			std::ranges::sort(std::span(remainder.parallelConflicts.get(), remainder.stats.numOutsideMIS));
			groupIds = { remainder.parallelConflicts.get(), remainder.stats.numOutsideMIS };
			accSerialFallback.append_range(std::span(remainder.serialFallbacks.get(), remainder.stats.numSerialFallback));
		}

		

		const auto t1 = Clock::now();

		// The remainding groups we didn't run through parallel update
		accSerialFallback.append_range(groupIds | std::views::transform([](int groupId) {
			return SerialFallback{ groupId, "Exceeded kMaxParallelPasses" };
			}));
		std::ranges::sort(accSerialFallback, std::less<>(), &SerialFallback::groupId);

		// Don't need to update pathfinder data here as GameContext will do it for the serial AI code.

		const bool result = applySerialUpdates(player, accSerialFallback);

		const auto t2 = Clock::now();

		if constexpr (kDumpTiming)
		{
			using D = std::chrono::duration<double, std::milli>;
			std::clog << "Parallel: " << D(t1 - t0) << ", serial: " << D(t2 - t1) << std::endl;
		}

		return result;
	}
}

// CvPlayerAI::AI_unitUpdate()
void GameContext::doAIParallelUnitUpdate(CvPlayerAI& player)
{
	// No humans allowed.
	if (player.isHuman())
		std::abort();

	if (player.hasBusyUnit())
		return;
	
	doSeparation(player);

	const bool parallelUpdateEnabledForThisPlayer = player.getID() == BARBARIAN_PLAYER && kEnableParallelUnitUpdate;

	if (parallelUpdateEnabledForThisPlayer)
	{
		// Update these variables now so that they are not lazily computed during the parallel update.
		// This is a small mechanical change as this depends on plot danger, visible enemy units, that can change depending on unit update order.
		int itIndex{};
		for (CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
			(void)static_cast<CvCityAI&>(*city).AI_neededFloatingDefenders();
	}

	const std::array<std::vector<int>, kNumPriorities> priorityBuckets = doPriorityBucketing(player);

	//PlayerUnitsDryRun dryRun;
	//
	//for (const auto& bucket : priorityBuckets)
	//{
	//	std::for_each(std::execution::par, bucket.begin(), bucket.end(), [&](int selGroupId) {
	//		if (CvSelectionGroup* const pLoopSelectionGroup = player.getSelectionGroup(selGroupId))  // group might have been killed by a previous group update
	//			dryRun.CvSelectionGroupAI_AI_update(pLoopSelectionGroup);
	//		});
	//}

	//heck::emitProfilerMarker(std::format(L"Player {} start units update", (int)player.getID()).c_str());

	const auto t0 = Clock::now();

	[[maybe_unused]] size_t numUnits = 0;

	for (const auto& bucket : priorityBuckets)
	{
		if (bucket.empty())
			continue;

		if (parallelUpdateEnabledForThisPlayer)
		{
			//constexpr bool enableAnalysis = false;
			if (runUpdatesOnGroupList(player, bucket, getVectorisedPathfinderMap()))
				break;
		}
		else
		{
			bool earlyOut = false;
			for (const int selGroupId : bucket)
			{
				if (CvSelectionGroup* const pLoopSelectionGroup = player.getSelectionGroup(selGroupId))  // group might have been killed by a previous group update
				{
					if (pLoopSelectionGroup->AI_update())
					{
						//if constexpr (kEnableSerialLogging)
						//	std::clog << "Early-out on unit " << numUnits << std::endl;
						earlyOut = true;
						break;
					}
				}
		
				++numUnits;
			}
			if (earlyOut)
				break;
		}
	}

	const auto t1 = Clock::now();

	//heck::emitProfilerMarker(std::format(L"Player {} end units update", (int)player.getID()).c_str());

	if constexpr (kDumpTiming)
		std::clog << "Player " << (int)player.getID() << " units took " << std::chrono::duration<double, std::milli>(t1 - t0) << " to update." << std::endl;
}

#endif