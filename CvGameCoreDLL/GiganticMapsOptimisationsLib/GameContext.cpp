#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "GameContext.h"
#include "PlotDangerCache.h"
#include "TeamNeighbourTracker.h"
#include "UnitPathingUtil.h"
#include "Util.h"
#include "VectorisedPathfinder.h"
#include "VectorisedPathfinderMap.h"
#include "VectorisedPathStep.h"
#include "FoundValueV2/FoundValueTesting.h"
#include "FoundValueV2/FoundValueSystem.h"
//#include "PathDistanceJPS.h"
//#include "PathDistanceRSR.h"
#include "PathDistanceSimple.h"
#include "LivePathfinder.h"

#include <CommonStuff/WorkStealingParallelFor.h>
#include <CommonStuff/ParallelExt.h>
#include <CommonStuff/ProfilerMarkers.h>
#include <CommonStuff/StatisticsRegistry.h>

#include "../CvMap.h"
#include "../CvGameCoreDLL.h"
#include "../CvDLLFAStarIFaceBase.h"
#include "../FAStarNode.h"
#include "../CvPlayerAI.h"
#include "../CvGameCoreUtils.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../CvInfos.h"

#include <iostream>
#include <chrono>
#include <fstream>

using namespace GiganticMapsOptimisationsLib;

static constexpr bool kEnableVerificationAgainstFAStar = false;
// If true, path costs must equal.
// Else, only movement points.
// We can't really check for matching path costs because of the combination of movement point tracking and plot cost adjusts causing the G cost field to become inconsistent with itself.
// `Pathing Quirks.md` has one case with 6 MP difference.
// TODO: Implement more reliable verification path finder/FAStar replacement.
static constexpr int kVerifyPathCostAccumulatedMPTolerance = 20;
// Path verification error with turn limit and 3 units (pathCost flaw) - Autosave turn 819 AD-1800
// Don't know how to solve this one. It's probably up to A* visit order.
static constexpr int kVerifyPathMaxTurnsTolerance = 1;
// With MOVE_NO_ENEMY_TERRITORY path queries from AI_exploreRange, logging takes up most of the time!
static constexpr bool kEnableLogging = false;
static constexpr bool kEnableSlowPathingLogging = false;



static constexpr bool kForceResetPathfinderOnEveryEvent = false;

//static constexpr bool kUseBlockwiseAStar = false;

static constexpr bool kEnableFoundValueComputationPerformanceTest = false;

static constexpr bool kEnablePathDistanceSelfTest = false;
//static constexpr bool kEnableFastPathDistance = true;

//static constexpr bool kEnableReversePathing = false; // Not useful at the moment.
//static constexpr int kPathSpliceLength = 20;
//static constexpr int kReversePathingThreshold = kPathSpliceLength * 2;

// Enable this when `parallelForEachN` starts to cause non-determinism...
static constexpr bool kEnableSyncChecksumLogging = false;

static constexpr bool kEnablePerTurnStatLogging = false;
static constexpr bool kEnablePerTurnGameProgressLogging = true;
static constexpr bool kEnableTurnTimeLogging = true;
static constexpr bool kEnableTurnTimeCSVLogging = false;

// Only verify "used" found values.
static constexpr bool kEnableLiteFoundValueVerification = false;
// Only verify path step function around final path plots.
// And, if no path is found, check internal FAStar.
static constexpr bool kEnableLitePathVerification = false;

using heck::StatisticsRegistry;
using heck::CountingStatistic;
using heck::TimingStatistic;

static auto& gStat_Turns = StatisticsRegistry::getInstance().grab<CountingStatistic>("Turns");
static auto& gStat_PathRequests = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests");
static auto& gStat_OtherFlagsPathRequests = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (other flags)");
static auto& gStat_MultiUnitPathRequests = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (multi-unit)");
static auto& gStat_SiegeMoveThroughEnemyPathRequests = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (siege move through enemy)");
static auto& gStat_OtherUnsupportedPathRequests = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (other unsupported)");
static auto& gStat_FAStar = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (FAStar)");
static auto& gStat_ReachabilityLiteVerification = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path requests (lite verify reachability)");
static auto& gStat_PathStepDistance = StatisticsRegistry::getInstance().grab<TimingStatistic>("Path step distance");

struct GameContext::Internals
{
	/// Pathfinding
	//TurnCache<std::unordered_map<uint64_t, int>> stepDistanceTurnCache;
	std::unique_ptr<PathDistanceSimple> simplePathDistance;
	
	
	std::unique_ptr<TeamNeighbourTracker> teamNeighbourTracker;
	std::unique_ptr<StolenCityRadiusPlotsTracker> stolenCityRadiusPlotsTracker;
	//FoundValueCacheSystem foundValueCacheSystem;

	struct PathfindingSystem
	{
		VectorisedPathfinderMap vectorisedPathfinderMap;
		std::unique_ptr<IVectorisedPathfinder> vectorisedPathfinder;
		bool vectorisedPathfinderAStarPendingReset = true;
		// We'll use a separate instance.
		// All pathing requests will go through this, if verification is enabled or if the fast pathfinder cannot handle the pathing request.
		// Any pathing request *not* handled by this will instead reset this FAStar instance, to prepare for the next pathing request.
		// In any case, all results are emplaced into the original global FAStar instance.
		// This way, the original FAStar instance can be kept permanently in !reuse state without making performance worse, as this internal FAStar will keep track of reusable pathing state.
		// Also: Fixes must be enabled for verification if you allow FAStar reuse.
		const CvDLLFAStarIFaceBase::DeleterFunc fastarDeleter = gDLL->getFAStarIFace()->getFAStarDeleter();
		FAStar* internalFAStar = gDLL->getFAStarIFace()->create();
		LivePathfinder livePathfinder{ CivMapGeometry(GC.getMapINLINE()) };
		uint64_t nextPathSerial = 0;
		std::atomic_bool internalFAStarForceResetDeferred{};

		PathfindingSystem()
		{
			const CvMap& map = GC.getMapINLINE();
			gDLL->getFAStarIFace()->Initialize(internalFAStar,
				map.getGridWidthINLINE(), map.getGridHeightINLINE(), map.isWrapXINLINE(), map.isWrapYINLINE(),
				pathDestValid, pathHeuristic, pathCost, pathValid, pathAdd, nullptr, nullptr
			);
		}

		~PathfindingSystem()
		{
			fastarDeleter(internalFAStar);
		}

		bool generatePath(const CvSelectionGroup& group, const CvPlot& start, const CvPlot& goal, unsigned int flags, int maxTurns, bool reuse)
		{
			const auto commonTimingScope = gStat_PathRequests.scope();

			std::optional<TimingStatistic::Scope> optSpecificTimingScope;

			const ivec2 startCoord = getPlotCoord(start);
			const ivec2 goalCoord = getPlotCoord(goal);
			[[maybe_unused]] const int straightLineStepDistance = ::stepDistance(startCoord.x, startCoord.y, goalCoord.x, goalCoord.y);
			auto& fastarIFace = *gDLL->getFAStarIFace();

			const auto thisPathSerial = nextPathSerial;
			++nextPathSerial;

			//if (thisPathSerial == 1315)
			//	__debugbreak();

			// For when you want to check if a verification error will still happen even if A* state is fully reset.
			//if (thisPathSerial >= 44155)
			//	fastarIFace.ForceReset(internalFAStar);

			bool useFastPathFinder = true;

			useFastPathFinder &= !GET_PLAYER(group.getOwnerINLINE()).isHuman(); // Human pathing not implemented
			useFastPathFinder &= group.AI_isControlled(); // Condition used in `pathValid`.
			//useFastPathFinder &= group.getDomainType() == DOMAIN_LAND;
			//useFastPathFinder &= straightLineStepDistance >= 3 || maxTurns < 100;

			// Don't support land units starting from inside transports.
			if (group.getDomainType() == DOMAIN_LAND && group.plot()->isWater())
				useFastPathFinder = false;

			bool logThisPath = kEnableLogging;// || thisPathSerial >= 4409;

			const auto hasSiegeUnit = [&] {
				for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
					if (VectorisedPathStepFunction::isSiegeUnit(*::getUnit(node->m_data)))
						return true;
				return false;
				};

			if (useFastPathFinder)
			{
				if ((flags & ~(MOVE_IGNORE_DANGER | MOVE_SAFE_TERRITORY | MOVE_NO_ENEMY_TERRITORY | MOVE_THROUGH_ENEMY)) != 0)
				{
					optSpecificTimingScope.emplace(gStat_OtherFlagsPathRequests);
					//std::clog << "WARNING: Pathing request will use FAStar due to pathing flags." << std::endl;
					//logThisPath = true;
					useFastPathFinder = false;
				}
				else if ((flags & MOVE_THROUGH_ENEMY) != 0 && hasSiegeUnit())
				{
					optSpecificTimingScope.emplace(gStat_SiegeMoveThroughEnemyPathRequests);
					//std::clog << "WARNING: Pathing request will use FAStar because MOVE_THROUGH_ENEMY and the head unit is a siege unit." << std::endl;
					//logThisPath = true;
					useFastPathFinder = false;
				}
				else if (group.getNumUnits() != 1)
				{
					optSpecificTimingScope.emplace(gStat_MultiUnitPathRequests);
					//std::clog << "WARNING: Pathing request will use FAStar due to multiple units." << std::endl;
					//logThisPath = true;

					//useFastPathFinder = false;

					//fastarIFace.ForceReset(internalFAStar);
				}
			}
			else
				optSpecificTimingScope.emplace(gStat_OtherUnsupportedPathRequests);


			const CvUnit& headUnit = *group.getHeadUnit();

			//reuse = false;

			const auto logPathingRequestInfo = [&] {
				std::wclog << L"Generating path " << thisPathSerial << " from " << getBasicPlotDescription(start) << L" to " << getBasicPlotDescription(goal) << " with maxTurns " << maxTurns;
				std::wclog << " for head unit " << headUnit.getIDInfo().iID << L" \"" << headUnit.getName().c_str() << L"\" ("
					<< headUnit.movesLeft() << L" mp, " << group.getNumUnits() << L" units in group)";
				if (flags & MOVE_NO_ENEMY_TERRITORY)
					std::wclog << L" MOVE_NO_ENEMY_TERRITORY";

				std::wclog << std::endl;
				};

			if (logThisPath)
			{
				logPathingRequestInfo();
			}

			if constexpr (kForceResetPathfinderOnEveryEvent)
				if (internalFAStarForceResetDeferred.exchange(false, std::memory_order_relaxed))
					fastarIFace.ForceReset(internalFAStar);

			// This specific condition in pathCost that makes it goal-dependent.
			const bool isAttackingDest = group.AI_isControlled() && goal.getNumUnits() != 0 && [&group, &goal] {
				// Check if any units meets the conditions.
				for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
				{
					const CvUnit& unit = *::getUnit(node->m_data);
					if (goal.isVisibleEnemyDefender(&unit) && unit.canFight() && unit.canAttack())
						return true;
				}
				return false;
				}();

			// Okay, let's get verification under control. Reset FAStar for every path if costs are goal-dependent.
			if (isAttackingDest)
				fastarIFace.ForceReset(internalFAStar);

			bool isFAStarReset = fastarIFace.isReset(internalFAStar, startCoord.x, startCoord.y, false, flags, reuse);

			if (isFAStarReset)
			{
				if (logThisPath)
					std::clog << "\tPathfinder reset (reuse=" << reuse << ").\n";
				vectorisedPathfinderAStarPendingReset = true;

				if constexpr (kEnableLitePathVerification)
					livePathfinder.reset(startCoord);
			}

			const bool useInternalFAStar = kEnableVerificationAgainstFAStar || !useFastPathFinder;
			bool foundUsingFAStar = false;
			if (!useInternalFAStar)
			{
				// At least start the pathfinding operation, to get correct reset feedback afterwards.
				fastarIFace.SetData(internalFAStar, &group);
				(void)fastarIFace.startGeneratePath(internalFAStar, startCoord.x, startCoord.y, goalCoord.x, goalCoord.y, false, flags, reuse);
			}
			else
				foundUsingFAStar = generatePathUsingFAStar(*internalFAStar, group, start, goal, flags, reuse, logThisPath);

			const CivMapGeometry mapSizeInfo(GC.getMapINLINE());

			// Reset
			// We have to do this for all paths to ensure that mVectorisedPathfinderAStar and the internal FAStar get the same initial start state for all paths.
			// TODO: Really need to make initialisation faster, as this will clear out a meg or two of memory.
			if (!vectorisedPathfinder) [[unlikely]]
			{
				//mVectorisedPathfinder = kUseBlockwiseAStar ? createBlockAStarVectorisedPathfinder() : createCoordPlotwiseAStarVectorisedPathfinder();
				//mVectorisedPathfinder = createPlotwiseAStarVectorisedPathfinder();
				//mVectorisedPathfinder = createBlockAStarVectorisedPathfinder();
				vectorisedPathfinder = createCoordPlotwiseAStarVectorisedPathfinder(vectorisedPathfinderMap);
			}

			if (vectorisedPathfinderAStarPendingReset)
			{
				if (logThisPath)
					std::clog << "\tVectorised pathfinder reset.\n";
				vectorisedPathfinder->reset(0);

				// pathAdd takes the minimum (unless MOVE_MAX_MOVES).
				int startMP = INT_MAX;
				for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
				{
					const CvUnit& unit = *::getUnit(node->m_data);
					startMP = std::min(startMP, unit.movesLeft());
				}

				vectorisedPathfinder->start(startCoord, startMP | (1 << VectorisedPathStepFunction::kUnitStateTurnsShift));
				vectorisedPathfinderAStarPendingReset = false;
			}


			if (!useFastPathFinder)
			{
				//if constexpr (kEnableLogging)
				//{
				//	std::clog << "Using FAStar result for: "
				//		<< "num units = " << group.getNumUnits()
				//		<< ", domain = " << (int)group.getDomainType()
				//		<< ", step distance = " << straightLineStepDistance
				//		<< ", maxTurns = " << maxTurns << '\n';
				//}

				emplacePathIntoFAStar(GC.getPathFinder(), foundUsingFAStar ? reconstructPathFromFAStar(*internalFAStar) : std::vector<PathNode>());
				return foundUsingFAStar;
			}




			// Start generating path. Check dest valid.
			if (!calc_pathDestValid(group, flags, goalCoord))
			{
				if constexpr (kEnableVerificationAgainstFAStar)
					if (foundUsingFAStar)
						std::abort();
				emplacePathIntoFAStar(GC.getPathFinder(), {});
				return false;
			}

			// Update caches.
			vectorisedPathfinderMap.update(group.getTeam());

			//if (thisPathSerial == 2941)
			//	mVectorisedPathfinderMap.verify(unit.getTeam());



			// Generate the path (the G cost field).

			vectorisedPathfinder->setConfig(&group, flags);

			if (isAttackingDest)
				vectorisedPathfinder->setHeuristicGoalTo3x3Around(goalCoord);
			else
				vectorisedPathfinder->setHeuristicGoal(goalCoord);

			vectorisedPathfinder->updateOpenSetHeuristicValues();

			const int pathCostLimit = calcPathCostUpperBound(group, maxTurns);

			const auto t0 = kEnableLogging ? Clock::now() : Clock::time_point();

			const bool isDebugPathingRequest = false;//thisPathSerial == 108;

			const bool isPathFound = vectorisedPathfinder->search(goalCoord, pathCostLimit, isDebugPathingRequest)
				&& (VectorisedPathStepFunction::splitMPTurns(vectorisedPathfinder->getUnitStateAt(goalCoord)).second <= maxTurns);

			const auto t1 = kEnableLogging ? Clock::now() : Clock::time_point();

			if (logThisPath)
				std::clog << "\tVectorised path finder took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
			else if (kEnableSlowPathingLogging && t1 - t0 >= std::chrono::duration<int, std::milli>(100))
			{
				logPathingRequestInfo();
				std::clog << "\tVectorised path finder took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
			}

			//if constexpr (kEnableLogging)
			//	mVectorisedPathfinderAStar->dumpStats(std::clog);

			//if (thisPathSerial == 108)
			//	mVectorisedPathfinder->debugDump();

			std::optional<ivec2> optAdjOverrideCoord;

			int vectorisedPathfinderGoalCost = vectorisedPathfinder->getGCostAt(goalCoord);

			if (isPathFound && isAttackingDest)
			{
				// Okay, the current graph implementation ignores the goal-dependent cost.
				// So to get an accurate path, do the last step here by checking each adjacent plot.
				// Avoid too much extra pathing by limiting the cost.

				// First, compute a correct cost using any valid adjacent plot.
				int32_t adjPathCostLimit = INT32_MAX;
				for (const ivec2 adjD : kAdj)
				{
					const ivec2 coord = goalCoord + adjD;
					if (mapSizeInfo.isValidCoord(coord))
					{
						const ivec2 wrappedCoord = mapSizeInfo.wrapCoord(coord);
						const int32_t adjPathCost = vectorisedPathfinder->getGCostAt(wrappedCoord);
						if (adjPathCost < INT32_MAX)
						{
							const auto [fromMP, fromTurns] = VectorisedPathStepFunction::splitMPTurns(vectorisedPathfinder->getUnitStateAt(wrappedCoord));
							if (calc_pathValid(group, startCoord, flags, fromTurns, fromMP, wrappedCoord, goalCoord))
							{
								adjPathCostLimit = adjPathCost + calc_pathCost(group, flags, fromTurns, fromMP, wrappedCoord, goalCoord, goalCoord);
								break;
							}
						}
					}
				}

				// A path was found, so there has to be at least one valid adjacent plot.
				if (adjPathCostLimit >= INT32_MAX)
					std::abort();

				// Now path to each adjacent plot, up to the cost limit.
				// The heuristic was changed to be valid for all adjacent plots.
				for (const ivec2 adjD : kAdj)
				{
					const ivec2 coord = goalCoord + adjD;
					if (mapSizeInfo.isValidCoord(coord))
					{
						const ivec2 wrappedCoord = mapSizeInfo.wrapCoord(coord);
						vectorisedPathfinder->search(wrappedCoord, adjPathCostLimit, isDebugPathingRequest);
					}
				}

				// Find the best adj plot.
				int32_t bestPathCost = INT32_MAX;
				for (const ivec2 adjD : kAdj)
				{
					const ivec2 coord = goalCoord + adjD;
					if (mapSizeInfo.isValidCoord(coord))
					{
						const ivec2 wrappedCoord = mapSizeInfo.wrapCoord(coord);
						const int32_t adjPathCost = vectorisedPathfinder->getGCostAt(wrappedCoord);
						if (adjPathCost < INT32_MAX)
						{
							const auto [fromMP, fromTurns] = VectorisedPathStepFunction::splitMPTurns(vectorisedPathfinder->getUnitStateAt(wrappedCoord));
							if (calc_pathValid(group, startCoord, flags, fromTurns, fromMP, wrappedCoord, goalCoord))
							{
								const int32_t pathCost = adjPathCost + calc_pathCost(group, flags, fromTurns, fromMP, wrappedCoord, goalCoord, goalCoord);
								//std::clog << adjD << ": " << adjPathCost << ' ' << pathCost << std::endl;
								if (pathCost < bestPathCost)
								{
									bestPathCost = pathCost;
									optAdjOverrideCoord = wrappedCoord;
								}
							}
						}
					}
				}

				if (!optAdjOverrideCoord)
					std::abort();

				vectorisedPathfinderGoalCost = bestPathCost;

				// Okay. Now we finally have a proper path taking into account goal-dependent costs without putting goal-dependent costs in the A* visit set.
			}

			const ivec2 pathReconstructionGoalCoord = optAdjOverrideCoord.value_or(goalCoord);
			std::vector<i16vec2> pathPlotCoords = isPathFound ? vectorisedPathfinder->reconstructPath(startCoord, pathReconstructionGoalCoord) : std::vector<i16vec2>();

			std::vector<PathNode> pathNodes;
			pathNodes.reserve(pathPlotCoords.size());
			for (const ivec2 coord : pathPlotCoords)
			{
				const auto [fromMP, fromTurns] = VectorisedPathStepFunction::splitMPTurns(vectorisedPathfinder->getUnitStateAt(coord));
				pathNodes.emplace_back(coord, fromTurns, fromMP);
			}



			if (optAdjOverrideCoord)
			{
				const auto [fromMP, fromTurns] = VectorisedPathStepFunction::splitMPTurns(vectorisedPathfinder->getUnitStateAt(*optAdjOverrideCoord));
				const auto [goalMP, goalTurns] = calc_pathAdd(group, flags, fromTurns, fromMP, *optAdjOverrideCoord, goalCoord);
				pathNodes.emplace_back(goalCoord, goalTurns, goalMP);
			}

			// Verify.
			if constexpr (kEnableVerificationAgainstFAStar)
			{
				//const ivec2 verificationGoalCoord = optAdjOverrideCoord.value_or(goalCoord);
				//const int verificationGoalPlotI = toPlotIndex(verificationGoalCoord, mapSizeInfo.dim);
				const int32_t pathCost = vectorisedPathfinderGoalCost; //mVectorisedPathfinder->getGCostAt(verificationGoalCoord);

				if (foundUsingFAStar != isPathFound)
				{
					// Could be turn limit.
					if (maxTurns < INT_MAX && foundUsingFAStar && !isPathFound)
					{
						FAStarNode* const current = fastarIFace.GetLastNode(internalFAStar);
						const int groundTruthTurns = current->m_iData2;
						if (groundTruthTurns + kVerifyPathMaxTurnsTolerance <= maxTurns)
						{
							std::clog << "Verification found that whether or not a path is found is not correct for thisPathSerial = " << thisPathSerial << std::endl;
							std::clog << "FAStar turns = " << groundTruthTurns << std::endl;
							std::clog << "FAStar cost = " << current->m_iKnownCost << std::endl;
							std::clog << "maxTurns = " << maxTurns << std::endl;
							std::clog << "pathCostLimit = " << pathCostLimit << std::endl;
							logPathingRequestInfo();
							vectorisedPathfinder->debugDump();
							std::abort();
						}
					}
					else
					{
						//(void)doPersistentPathSearch(group, startCoord, goalCoord, flags, maxTurns, isReversePathing, false);
						std::clog << "Verification found that whether or not a path is found is not correct for thisPathSerial = " << thisPathSerial << std::endl;
						//mVectorisedPathfinder->debugDump();
						std::abort();
					}
				}

				if (isPathFound)
				{
					// There is a path. Check cost.
					//const int fastarCost = fastarIFace.GetLastNode(internalFAStar)->m_iKnownCost;
					FAStarNode* const fastarGoalNode = fastarIFace.getNode(internalFAStar, goalCoord.x, goalCoord.y);
					const int fastarCost = fastarGoalNode->m_iKnownCost;
					if (std::abs(fastarCost - pathCost) > kVerifyPathCostAccumulatedMPTolerance * PATH_MOVEMENT_WEIGHT)
					{
						std::clog << "Significant path cost mismatch: " << fastarCost << " != " << pathCost << std::endl;
						if (optAdjOverrideCoord)
							std::clog << "\tNOTE: isAttackingDest = true. Path cost mismatch is likely if FAStar state is reused for different goals." << std::endl;

						const auto dumpPathCompare = [&]/*(ivec2 lastPlotCoord)*/ {
							CvMap& map = GC.getMapINLINE();

							std::clog << "Verification FAStar path (from goal):\n";
							FAStarNode* current = fastarGoalNode;
							while (current)
							{
								std::clog << ivec2(current->m_iX, current->m_iY)
									<< " cost=" << current->m_iKnownCost
									<< " turns:mp=" << current->m_iData2 << ':' << current->m_iData1 << std::flush;
								std::wclog << L' ' << getFullPlotDescription(*map.plot(current->m_iX, current->m_iY)) << std::endl;
								current = current->m_pParent;
							}

							std::clog << "Our path:\n";
							//const std::vector<i16vec2> path = mVectorisedPathfinder->reconstructPath(startCoord, lastPlotCoord);
							for (const auto [coord, turns, mp] : pathNodes | std::views::reverse)
							{
								//const auto [mp, turns] = VectorisedPathStepFunction::splitMPTurns(mVectorisedPathfinder->getUnitStateAt(coord));
								std::clog << coord
									<< " cost=" << vectorisedPathfinder->getGCostAt(coord)
									<< " turns:mp=" << turns << ':' << mp
									<< std::flush;
								std::wclog << L' ' << getFullPlotDescription(*map.plot(coord.x, coord.y)) << std::endl;
							}
							};

						// It might be because of goal-dependent costs, and that FAStar state is reused despite that (the other FAStar bug).
						// So, path to the plot adjacent to the goal plot, and then compute the cost from that to the goal using goal-dependent costs.
						// And then compare costs.
						//if (isAttackingDest)
						//{
						//	std::clog << "Checking for difference due to goal-dependent costs..." << std::endl;
						//	const FAStarNode* const adjFAStarNode = fastarIFace.GetLastNode(internalFAStar)->m_pParent;
						//	const ivec2 adjCoord(adjFAStarNode->m_iX, adjFAStarNode->m_iY);
						//	const int adjPlotI = toPlotIndex(adjCoord, mapSizeInfo.dim);
						//	if (!mVectorisedPathfinderAStar->search(adjPlotI, graph))
						//	{
						//		std::clog << "Failed to path to adjacent plot." << std::endl;
						//		std::abort();
						//	}
						//	const int fastAdjCost = mVectorisedPathfinderAStar->getGCost(adjPlotI);
						//	const int fastarAdjCost = adjFAStarNode->m_iKnownCost;
						//
						//	if (fastarAdjCost != fastAdjCost)
						//	{
						//		std::clog << "Cost to goal-adjacent plot is different: " << fastarAdjCost << " != " << fastAdjCost << std::endl;
						//		std::abort();
						//	}
						//
						//	const auto [fromMP, fromTurns] = VectorisedPathfinderGraph::splitMPTurns(mVectorisedPathfinderAStar->getGState(adjPlotI));
						//
						//	const int costToGoalWithGoalDependentCost = calc_pathCost(group, flags, fromTurns, fromMP, adjCoord, goalCoord, goalCoord);
						//	const int costToGoalWithoutGoalDependentCost = calc_pathCost(group, flags, fromTurns, fromMP, adjCoord, goalCoord, INVALID_PLOT_COORD);
						//
						//	const int finalCostWithGoalDependentCost = fastAdjCost + costToGoalWithGoalDependentCost;
						//	const int finalCostWithoutGoalDependentCost = fastAdjCost + costToGoalWithoutGoalDependentCost;
						//
						//	if (fastarCost != finalCostWithGoalDependentCost && fastarCost != finalCostWithoutGoalDependentCost)
						//	{
						//		std::clog << "Cost to goal-adjacent plot is different." << std::endl;
						//		std::clog << "FAStar cost: " << fastarCost << std::endl;
						//		std::clog << "Fast pathfinder cost with goal dependent cost: " << finalCostWithGoalDependentCost << std::endl;
						//		std::clog << "Fast pathfinder cost without goal dependent cost: " << finalCostWithoutGoalDependentCost << std::endl;
						//		std::clog << "Path dump for goal-adjacent plot:" << std::endl;
						//		dumpPathCompare(adjCoord);
						//		std::abort();
						//	}
						//
						//	std::clog << "Goal-dependent cost matches (";
						//	if (fastarCost == finalCostWithGoalDependentCost)
						//		std::clog << "cost includes goal-dependent cost";
						//	else
						//		std::clog << "cost is bugged and does not include the goal-dependent cost";
						//	std::clog << ")" << std::endl;
						//}
						//else
						{
							std::clog << "Path dump for goal plot:" << std::endl;
							//dumpPathCompare(verificationGoalCoord);
							dumpPathCompare();
							vectorisedPathfinderMap.verify(group.getTeam());


							//const auto [mp, turns] = VectorisedPathStepFunction::splitMPTurns(mVectorisedPathfinder->getUnitStateAt(verificationGoalCoord));
							//const PathNode finalPathNode = pathNodes.empty() ? PathNode() : pathNodes.back();
							//const int mp = finalPathNode.mp;
							//const int turns = finalPathNode.turns;
							//const int fastarMP = fastarGoalNode->m_iData1;
							//const int fastarTurns = fastarGoalNode->m_iData2;
							// TODO: Now that pathing supports multi-unit, how do you calculate MP per turns...
							//const int ourAccMP = (turns - 1) * unit.maxMoves() + ((turns == 1 ? unit.movesLeft() : unit.maxMoves()) - mp);
							//const int fastarAccMP = (fastarTurns - 1) * unit.maxMoves() + ((fastarTurns == 1 ? unit.movesLeft() : unit.maxMoves()) - fastarMP);
							//if (std::abs(ourAccMP - fastarAccMP) <= kVerifyPathCostAccumulatedMPTolerance)
							//	std::clog << "Path cost does not match, but difference in acc MP is allowed by kVerifyPathCostAccumulatedMPTolerance." << std::endl;
							//else
							{
								//std::clog << "Path cost does not match and acc MP is not within tolerance, but mVectorisedPathfinderMap was verified successfully." << std::endl;
								std::clog << "Path cost does not match, but vectorisedPathfinder was verified successfully." << std::endl;
								std::clog << "This could be a false positive and the pathfinder is functioning as intended with Civ4's astar quirks, or it could be something else." << std::endl;
								std::clog << "Multi-unit pathfinding may make this worse." << std::endl;
								// Because path reconstruction relies on a sensible cost function.
								// It will follow the lowest costs just fine, but the unit states it follows might not make sense at all.
								// Eg: Two consecutive path nodes with the same MP.
								// Both FAStar and the vectorised pathfinder may exhibit this "cost-moves" inconsistency.
								// For multi-unit pathing, when following parent pointers, sometimes the difference in cost does not match the difference in MP.
								std::clog << "See HTML dump." << std::endl;

								logPathingRequestInfo();
								vectorisedPathfinder->debugDump();
								std::abort();

								// See Pathing Quirks.md
							}
						}
					}
					else
					{
						if constexpr (kEnableLogging)
							std::clog << "Path cost matches." << std::endl;
					}
				}
			}

			//if (thisPathSerial == 3000)
			//{
			//	mVectorisedPathfinder->debugDump();
			//}


			if (isPathFound)
			{
				if constexpr (kEnableLitePathVerification)
				{
					heck::parallelForEachN(pathNodes.size(), [&pathNodes, this](size_t i) {
						vectorisedPathfinder->liteVerifyAt(pathNodes[i].coord,
							pathNodes[i].mp | (pathNodes[i].turns << VectorisedPathStepFunction::kUnitStateTurnsShift));
						});
				}


				// Emplace the path into the FAStar instance.
				emplacePathIntoFAStar(GC.getPathFinder(), pathNodes);
				return true;
			}
			else
			{
				if constexpr (kEnableLitePathVerification)
				{
					const auto reachabilityScope = gStat_ReachabilityLiteVerification.scope();

					//if (generatePathUsingFAStar(*internalFAStar, group, start, goal, flags, reuse, logThisPath))
					if (livePathfinder.search(group, flags, goalCoord, pathCostLimit))
					{
						if (*livePathfinder.getGStateTurns(goalCoord) <= maxTurns)
						{
							std::clog << "Pathing lite verification failed. LivePathfinder found a path when the vectorised path finder couldn't." << std::endl;
							std::clog << "maxTurns = " << maxTurns << std::endl;
							std::clog << "getGStateTurns = " << *livePathfinder.getGStateTurns(goalCoord) << std::endl;
							vectorisedPathfinderMap.verify(group.getTeam());
							std::abort();
						}
					}
				}
				emplacePathIntoFAStar(GC.getPathFinder(), {});
				//if constexpr (kEnableLogging)
				//	std::clog << "No path found." << std::endl;
				return false;
			}
		}
	};

	std::unique_ptr<PathfindingSystem> pathfindingSystem;

	/// Found Value System V2
	std::unique_ptr<FoundValueSystem::System> foundValueSystem;

	std::optional<Clock::time_point> optLastTurnTime;

	Internals()
	{
		//renderMap(true).saveAsPPM("GameContext init map dump.ppm");
		//renderMap(false).saveAsPPM("GameContext init map domain dump.ppm");

		if constexpr (kEnableFoundValueComputationPerformanceTest)
			FoundValueSystem::testPerformance();

		if constexpr (kEnablePathDistanceSelfTest)
		{
			PathDistanceSimple(GC.getMapINLINE()).verify();
			//RectangularSymmetryReductionMap(GC.getMapINLINE()).verify();
			//JumpPointSearchMap(GC.getMapINLINE()).verify();
		}

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Fast_CvMap_calculatePathDistance])
			simplePathDistance = std::make_unique<PathDistanceSimple>(GC.getMapINLINE());

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Fast_CvTeamAI_AI_calculateAdjacentLandPlots])
			teamNeighbourTracker = std::make_unique<TeamNeighbourTracker>();

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Fast_CvPlayerAI_AI_calculateStolenCityRadiusPlots])
			stolenCityRadiusPlotsTracker = std::make_unique<StolenCityRadiusPlotsTracker>();

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::VectorisedPathfinder])
			pathfindingSystem = std::make_unique<PathfindingSystem>();

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::VectorisedFoundValues])
			foundValueSystem = std::make_unique<FoundValueSystem::System>();

		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
			gDLL->getFAStarIFace()->enableVerificationFixes();
		else if constexpr (kEnableVerificationAgainstFAStar)
			std::abort();

		initialCacheUpdate();
	}

	HECK_NOINLINE void initialCacheUpdate()
	{
		if (pathfindingSystem)
			for (const TeamTypes teamI : range<TeamTypes>(MAX_TEAMS))
				pathfindingSystem->vectorisedPathfinderMap.update(teamI);

		if (foundValueSystem)
		{
			heck::emitProfilerMarker(L"Begin Found Value System initial update");
			const auto t0 = Clock::now();
			for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
				foundValueSystem->update(playerI);
			const auto t1 = Clock::now();
			heck::emitProfilerMarker(L"End Found Value System initial update");
			std::clog << "Found Value System initial update: " << Milliseconds(t1 - t0) << std::endl;
		}
	}

	void onPlotChange(EGamePlotChangeEvent e, const CvPlot& plot, int oldValue, int newValue)
	{
		// NOTE: Needs to be thread-safe for HasUnit, InvisibilitySight, RevealedByTeam

		switch (e)
		{
		case EGamePlotChangeEvent::Owner:
		{
			const TeamTypes oldOwnerTeamI = oldValue != NO_PLAYER ? GET_PLAYER(static_cast<PlayerTypes>(oldValue)).getTeam() : NO_TEAM;
			if (teamNeighbourTracker)
				teamNeighbourTracker->onPlotOwnerChanged(plot, oldOwnerTeamI);
			if (stolenCityRadiusPlotsTracker)
				stolenCityRadiusPlotsTracker->onPlotOwnerChanged(plot, static_cast<PlayerTypes>(oldValue));
			break;
		}
		//case EGamePlotChangeEvent::CityAfterCreated:
		//	if (stolenCityRadiusPlotsTracker)
		//		stolenCityRadiusPlotsTracker->onCityCreate(*plot.getPlotCity());
		//	break;
		//case EGamePlotChangeEvent::CityBeforeDestroyed:
		//	if (stolenCityRadiusPlotsTracker)
		//		stolenCityRadiusPlotsTracker->onCityDestroy(*plot.getPlotCity());
		//	break;
		default:
			break;
		}

		if (pathfindingSystem)
		{
			pathfindingSystem->vectorisedPathfinderMap.onPlotEvent(e, plot, oldValue, newValue);
			if constexpr (kForceResetPathfinderOnEveryEvent)
			{
				pathfindingSystem->internalFAStarForceResetDeferred.store(true, std::memory_order_relaxed);
				//gDLL->getFAStarIFace()->ForceReset(internalFAStar);
			}
		}

		// Found value cache (V1)
		/*
		switch (e)
		{
		case EGamePlotChangeEvent::Owner:
			foundValueCacheSystem.onPlotOwnerChanged(plot, (PlayerTypes)oldValue);

			if (plot.isCity())
			{
				// City owner changed. This invalidates per-player city lists too.
				foundValueCacheSystem.onAfterCityDestroyed(getPlotCoord(plot), (PlayerTypes)oldValue);
				foundValueCacheSystem.onCityCreated(*plot.getPlotCity());
			}

			break;
		case EGamePlotChangeEvent::RevealedByTeam:
			for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
				if (GET_PLAYER(playerI).getTeam() == (TeamTypes)newValue)
					foundValueCacheSystem.onPlotRevealed(playerI, plot);
			break;
	
		case EGamePlotChangeEvent::CityAfterCreated:
			foundValueCacheSystem.onCityCreated(*plot.getPlotCity());
			break;
		case EGamePlotChangeEvent::CityAfterDestroyed:
			foundValueCacheSystem.onAfterCityDestroyed(getPlotCoord(plot), (PlayerTypes)oldValue);
			break;
		default:
			break;
		}
		*/

		// Found value cache (V2)
		if (foundValueSystem)
		{
			switch (e)
			{
			case EGamePlotChangeEvent::Feature:
			case EGamePlotChangeEvent::Terrain:
				foundValueSystem->onPlotTerrainOrFeatureChanged(plot);
				break;
			case EGamePlotChangeEvent::GetYield:
				foundValueSystem->onPlotGetYieldChanged(plot);
				break;
			case EGamePlotChangeEvent::CityRadiusCount:
				foundValueSystem->onCityRadiusCountChanged(plot);
				break;
			case EGamePlotChangeEvent::Bonus:
				foundValueSystem->onPlotBonusChanged(plot, static_cast<BonusTypes>(oldValue), static_cast<BonusTypes>(newValue));
				break;
			case EGamePlotChangeEvent::Owner:
				foundValueSystem->onPlotOwnerChanged(plot, static_cast<PlayerTypes>(oldValue), static_cast<PlayerTypes>(newValue));
				break;
			case EGamePlotChangeEvent::CityAfterCreated:
				foundValueSystem->onAfterCityCreated(plot, static_cast<PlayerTypes>(newValue));
				break;
			case EGamePlotChangeEvent::CityAfterDestroyed:
				foundValueSystem->onAfterCityDestroyed(plot, static_cast<PlayerTypes>(oldValue));
				break;
			case EGamePlotChangeEvent::RevealedByTeam:
				for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
					if (GET_PLAYER(playerI).getTeam() == static_cast<TeamTypes>(newValue))
						foundValueSystem->onPlayerPlotRevealedChanged(plot, playerI);
				break;

			default:
				break;
			}
		}

		//if constexpr (kEnableLogging)
		//{
		//	if (e == EGamePlotChangeEvent::RouteType)
		//		std::clog << "Route Change at " << getPlotCoord(plot) << ". New type = " << newValue << std::endl;
		//	if (e == EGamePlotChangeEvent::Feature)
		//		std::clog << "Feature Change at " << getPlotCoord(plot) << ". New type = " << newValue << std::endl;
		//}
	}

	void onPlotCultureChange(const CvPlot& plot, PlayerTypes culturePlayerI, [[maybe_unused]] int oldValue, [[maybe_unused]] int newValue)
	{
		if (foundValueSystem)
			foundValueSystem->onPlayerPlotCultureChanged(plot, culturePlayerI);
	}

	void onUnitChange(EGameUnitChangeEvent e, const CvUnit& unit)
	{
		if (pathfindingSystem)
		{
			pathfindingSystem->vectorisedPathfinderMap.onUnitEvent(e, unit);

			if constexpr (kForceResetPathfinderOnEveryEvent)
			{
				pathfindingSystem->internalFAStarForceResetDeferred.store(true, std::memory_order_relaxed);
				//gDLL->getFAStarIFace()->ForceReset(internalFAStar);
			}
		}
	}

	// Thread-safe
	void onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to)
	{
		if (pathfindingSystem)
		{
			pathfindingSystem->vectorisedPathfinderMap.onUnitMoved(unit, from, to);

			if constexpr (kForceResetPathfinderOnEveryEvent)
			{
				pathfindingSystem->internalFAStarForceResetDeferred.store(true, std::memory_order_relaxed);
				//gDLL->getFAStarIFace()->ForceReset(internalFAStar);
			}
		}

		if (foundValueSystem)
			foundValueSystem->onUnitMoved(unit, from, to);
	}

	void onTeamChange([[maybe_unused]] EGameTeamChangeEvent e, TeamTypes teamA, int valueB)
	{
		if (pathfindingSystem)
			pathfindingSystem->vectorisedPathfinderMap.onTeamEvent(e, teamA, valueB);

		if (foundValueSystem)
		{
			if (e == EGameTeamChangeEvent::TeamTech)
				foundValueSystem->onTeamTech(teamA, static_cast<TechTypes>(valueB));
			else if (e == EGameTeamChangeEvent::ForceRevealBonusChanged)
				foundValueSystem->onTeamForceRevealBonusChanged(teamA, static_cast<BonusTypes>(valueB));
		}
	}
	
	std::optional<int> guessPathTurnsZeroBased(const CvSelectionGroup& group, unsigned int pathingFlags, const CvPlot& dest)
	{
		const ivec2 startCoord = getPlotCoord(*group.plot());
		const ivec2 destCoord = getPlotCoord(dest);

		const CvUnit& testUnit = *group.getHeadUnit();

		// If no path possible...
		if (!calc_pathDestValid(group, pathingFlags, destCoord))
			return std::nullopt;

		if (!pathfindingSystem)
			return 0;

		const VectorisedPathfinderMap::LandmarkDistanceFieldWithBias heuristicInstance = pathfindingSystem->vectorisedPathfinderMap.tryGetMultiLandmarkDistanceField(group);

		if (heuristicInstance.distanceField)
		{
			// Use the same heuristic evaluation as pathfinding.
			const VectorisedPathfinderMap::MultiLandmarkDistanceVector startVec = (*heuristicInstance.distanceField)[startCoord];
			const VectorisedPathfinderMap::MultiLandmarkDistanceVector goalVec = (*heuristicInstance.distanceField)[destCoord];
			const int totalBias = testUnit.maxMoves() * PATH_MOVEMENT_WEIGHT + heuristicInstance.bias;
			const int32_t pathCostLowerBound = hmax(goalVec - startVec) - totalBias;
			// Now estimate the lower bound turn count from the lower bound path cost.
			return calcPathCostLowerBoundZeroBasedTurns(testUnit, pathCostLowerBound);
		}
		else
		{
			const int dist = ::stepDistance(startCoord.x, startCoord.y, destCoord.x, destCoord.y);
			return calcZeroBasedMinTurnsForStepDistance(testUnit, dist);
		}
	}

	static bool generatePathUsingFAStar(FAStar& fastar, const CvSelectionGroup& group, const CvPlot& start, const CvPlot& goal, unsigned int flags, bool reuse,
		bool enableLogging)
	{
		const auto timingScope = gStat_FAStar.scope();

		const ivec2 startCoord = getPlotCoord(start);
		const ivec2 goalCoord = getPlotCoord(goal);

		gDLL->getFAStarIFace()->SetData(&fastar, &group);

		const auto t0 = Clock::now();
		// Fallback.
		const bool result = gDLL->getFAStarIFace()->GeneratePath(&fastar, start.getX(), start.getY(), goal.getX(), goal.getY(), false, flags, reuse);
		const auto t1 = Clock::now();
		//const double nodesPerMicroSec = Microseconds(t1 - t0).count() / numVisited;
		//if (t1 - t0 >= Milliseconds(1))
		{
			if (enableLogging || (kEnableSlowPathingLogging && t1 - t0 >= Milliseconds(100)))
			{
				int minMoves = INT_MAX;
				int maxMoves = INT_MIN;
				for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
				{
					const CvUnit* const unit = getUnit(node->m_data);
					minMoves = std::min(minMoves, unit->maxMoves());
					maxMoves = std::max(maxMoves, unit->maxMoves());
				}
				std::wclog <<L"\tFAStar for " << GET_PLAYER(group.getOwnerINLINE()).getName() << L' ' << getUnit(group.headUnitNode()->m_data)->getName()
					<< L" start=" << startCoord
					<< L" goal=" << goalCoord
					<< L" result=" << result
					<< L" flags=" << flags
					<< L" numUnits=" << group.getNumUnits()
					<< L" minMoves=" << minMoves
					<< L" maxMoves=" << maxMoves
					<< L" domain=" << group.getDomainType()
					<< L' ' << Milliseconds(t1 - t0)
					<< std::endl;
			}
		}

		return result;
	}

	struct PathNode
	{
		ivec2 coord{};
		int turns = 0;
		int mp = 0;
	};

	static std::vector<PathNode> reconstructPathFromFAStar(FAStar& fastar)
	{
		auto& fastarIFace = *gDLL->getFAStarIFace();

		std::vector<PathNode> path;
		path.reserve(100);
		FAStarNode* node = fastarIFace.GetLastNode(&fastar);
		path.emplace_back(ivec2(node->m_iX, node->m_iY), node->m_iData2, node->m_iData1);
		while (node = node->m_pParent, node)
			path.emplace_back(ivec2(node->m_iX, node->m_iY), node->m_iData2, node->m_iData1);
		std::ranges::reverse(path);
		return path;
	}

	static void emplacePathIntoFAStar(FAStar& fastar, std::span<const PathNode> path)
	{
		auto& fastarIFace = *gDLL->getFAStarIFace();

		if (path.empty())
			fastarIFace.setPath(&fastar, nullptr);
		else
		{
			// Emplace the path into the FAStar instance.
			FAStarNode* parent = nullptr;
			for (const PathNode pathNode : path)
			{
				FAStarNode* const node = fastarIFace.getNode(&fastar, pathNode.coord.x, pathNode.coord.y);
				node->m_pParent = parent;
				node->m_iData1 = pathNode.mp;
				node->m_iData2 = pathNode.turns;
				parent = node;
			}

			fastarIFace.setPath(&fastar, fastarIFace.getNode(&fastar, path.back().coord.x, path.back().coord.y));
		}

		fastarIFace.ForceReset(&GC.getPathFinder());
	}

	

	//CvPlot* tryFindNextPathStep(const CvSelectionGroup& group, const CvPlot& dest)
	//{
	//	return nullptr;
	//}

	

	std::optional<int> findPathStepDistance(const CvPlot& start, const CvPlot& goal)
	{
		const auto timingScope = gStat_PathStepDistance.scope();

		if (start.area() != goal.area())
			return std::nullopt;

		return simplePathDistance->findPathLength(getPlotCoord(start), getPlotCoord(goal));
	}
};

GameContext::GameContext() : mInternals(std::make_unique<Internals>())
{
}

GameContext::~GameContext() noexcept = default;

void GameContext::onGameTurn()
{
	if (mInternals->pathfindingSystem)
		mInternals->pathfindingSystem->vectorisedPathfinderMap.onGameTurn();

	//if constexpr (kEnableFullFoundValueSystemVerifyEveryTurn)
	//	mInternals->foundValueCacheSystem.verify();

	heck::emitProfilerMarker(std::format(L"Turn start {}", gGlobals.getGame().getGameTurn()).c_str());

	if constexpr (kEnableSyncChecksumLogging)
	{
		// Inc the turn slice to get each checksum.
		CvGame& game = gGlobals.getGame();
		const int turn = game.getGameTurn();
		const int slice = game.getTurnSlice();
		std::clog << "TURN " << turn << " SLICE " << slice << '\n';
		std::clog << "SYNC CHECKSUM +0: " << game.calculateSyncChecksum() << '\n'; game.changeTurnSlice(1);
		std::clog << "SYNC CHECKSUM +1: " << game.calculateSyncChecksum() << '\n'; game.changeTurnSlice(1);
		std::clog << "SYNC CHECKSUM +2: " << game.calculateSyncChecksum() << '\n'; game.changeTurnSlice(1);
		std::clog << "SYNC CHECKSUM +3: " << game.calculateSyncChecksum() << '\n';
		game.changeTurnSlice(-3);
	}

	gStat_Turns.inc();
	if constexpr (kEnablePerTurnStatLogging)
		std::clog << StatisticsRegistry::getInstance().buildAllStatsString() << std::flush;

	if constexpr (kEnablePerTurnGameProgressLogging)
	{
		std::clog << gGlobals.getGameINLINE().getNumCities() << " cities (" << (gGlobals.getGameINLINE().getNumCities() - gGlobals.getGameINLINE().getNumCivCities()) << " barbarian)" << std::endl;
		const CvArea* const biggestArea = gGlobals.getMapINLINE().findBiggestArea(false);
		std::clog << biggestArea->getNumOwnedTiles() << '/' << biggestArea->getNumTiles() << " plots owned on biggest land area ("
			<< (biggestArea->getNumOwnedTiles() * 100 + biggestArea->getNumTiles() / 2) / biggestArea->getNumTiles() << "%)" << std::endl;
	}

	if constexpr (kEnableTurnTimeLogging)
	{
		const auto t1 = Clock::now();
		if (mInternals->optLastTurnTime)
		{
			const auto dt = t1 - *mInternals->optLastTurnTime;
			std::clog << Seconds(dt) << " since the last turn.\n";

			if constexpr (kEnableTurnTimeCSVLogging)
				std::ofstream("GameContext onGameTurn turn times.csv", std::ios::app) << gGlobals.getGame().getGameTurn() << ',' << Seconds(dt).count() << '\n';
		}
		mInternals->optLastTurnTime = t1;
	}
}

void GameContext::onPlotChange(EGamePlotChangeEvent e, const CvPlot& plot, int oldValue, int newValue)
{
	mInternals->onPlotChange(e, plot, oldValue, newValue);
}

void GameContext::onPlotCultureChange(const CvPlot& plot, PlayerTypes culturePlayerI, int oldValue, int newValue)
{
	mInternals->onPlotCultureChange(plot, culturePlayerI, oldValue, newValue);
}

void GameContext::onUnitChange(EGameUnitChangeEvent e, const CvUnit& unit)
{
	mInternals->onUnitChange(e, unit);
}

void GameContext::onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to)
{
	mInternals->onUnitMoved(unit, from, to);
	
}

void GameContext::onTeamChange(EGameTeamChangeEvent e, TeamTypes teamA, int valueB)
{
	mInternals->onTeamChange(e, teamA, valueB);
}

void GameContext::onPlayerAliveChanged(PlayerTypes player, bool alive)
{
	if (mInternals->foundValueSystem)
		mInternals->foundValueSystem->onPlayerAliveChanged(player, alive);
}

void GameContext::onCapitalChanged(PlayerTypes player, [[maybe_unused]] ivec2 from, ivec2 to)
{
	if (mInternals->foundValueSystem)
		mInternals->foundValueSystem->onPlayerCapitalChanged(player, GC.getMapINLINE().plotSorenINLINE(to.x, to.y)->getPlotCity());
}

void GameContext::onGlobalAreaCityCountChanged(const CvArea& area, int from, int to)
{
	if (mInternals->foundValueSystem)
		mInternals->foundValueSystem->onGlobalAreaCityCountChanged(area, from, to);
}

void GameContext::onPlayerAreaCityCountChanged(const CvArea& area, PlayerTypes playerI, int from, int to)
{
	if (mInternals->foundValueSystem)
		mInternals->foundValueSystem->onPlayerAreaCityCountChanged(area, playerI, from, to);
}
void GameContext::onIsAICitySiteChanged([[maybe_unused]] const CvPlot& plot, PlayerTypes playerI)
{
	if (mInternals->foundValueSystem)
		mInternals->foundValueSystem->onPlayerAICitySitesChanged(playerI);
}

void GameContext::onChangePlayerCityRadiusCount(const CvPlot& plot, PlayerTypes playerI, int from, int to)
{
	if (mInternals->stolenCityRadiusPlotsTracker)
		mInternals->stolenCityRadiusPlotsTracker->onChangePlayerCityRadiusCount(plot, playerI, from, to);
}

//void GameContext::onPlotGetYieldChanged(const CvPlot& plot)
//{
//	//mInternals->foundValueCacheSystem.onPlotGetYieldChanged(plot);
//}
//void GameContext::onEventChangedPlotExtraYield(const CvPlot& plot)
//{
//	mInternals->foundValueCacheSystem.onEventChangedPlotExtraYield(plot);
//}
//void GameContext::onEventChangedPlotBonus(const CvPlot& plot)
//{
//	//mInternals->foundValueCacheSystem.onEventChangedPlotBonus(plot);
//}

std::optional<int> GameContext::guessPathTurnsOneBased(const CvSelectionGroup& group, const CvPlot& dest, unsigned int pathingFlags, int)
{
	std::optional<int> n = mInternals->guessPathTurnsZeroBased(group, pathingFlags, dest);
	if (n)
		++*n;
	return n;
}

//void GameContext::onUnitPathfinderReset()
//{
//	if constexpr (kEnableVerification)
//	{
//		gDLL->getFAStarIFace()->ForceReset(mInternals->verificationFAStar);
//		mInternals->mVectorisedPathfinderAStar = nullptr;
//	}
//}

bool GameContext::generatePath(const CvSelectionGroup& group, const CvPlot& start, const CvPlot& goal, unsigned int flags, int maxTurns, bool reuse)
{
	if (mInternals->pathfindingSystem)
		return mInternals->pathfindingSystem->generatePath(group, start, goal, flags, maxTurns, reuse);
	else
	{
		gDLL->getFAStarIFace()->SetData(&GC.getPathFinder(), &group);
		return gDLL->getFAStarIFace()->GeneratePath(&GC.getPathFinder(), start.getX_INLINE(), start.getY_INLINE(), goal.getX_INLINE(), goal.getY_INLINE(), false, flags, reuse);
	}
}

void GameContext::resetPathfinder()
{
	//std::clog << __FUNCTION__ << std::endl;
	if (mInternals->pathfindingSystem)
		gDLL->getFAStarIFace()->ForceReset(mInternals->pathfindingSystem->internalFAStar);
}

//CvPlot* GameContext::tryFindNextPathStep(const CvSelectionGroup& group, const CvPlot& dest)
//{
//	return mInternals->tryFindNextPathStep(group, dest);
//}

std::optional<int> GameContext::findPathStepDistance(const CvPlot& start, const CvPlot& goal)
{
	return mInternals->findPathStepDistance(start, goal);
}

int GameContext::getAdjacentLandPlotsCount(TeamTypes plotTeamI, TeamTypes adjTeamI) const
{
	return mInternals->teamNeighbourTracker->getAdjacentLandPlotsCount(plotTeamI, adjTeamI);
}

int GameContext::getStolenCityRadiusPlots(PlayerTypes cityPlayerI, PlayerTypes neighbourPlayerI) const
{
	return mInternals->stolenCityRadiusPlotsTracker->getStolenCityRadiusPlots(cityPlayerI, neighbourPlayerI);
}

//void GameContext::enumerateHighestCityPlanningValues(
//	int minFoundValue,
//	PlayerTypes playerI,
//	bool revealedOnly,
//	const std::function<int(const CvArea&)>& getAreaValueScale,
//	const std::function<bool(FoundValueSearchResult result)>& callback
//)
//{
//	mInternals->foundValueCacheSystem.enumerateHighestCityPlanningValues(minFoundValue, playerI, revealedOnly, getAreaValueScale, callback);
//}
//int GameContext::grabSpecificPlotFoundValue(PlayerTypes playerI, ivec2 plotCoord)
//{
//	return mInternals->foundValueCacheSystem.grabSpecificPlotFoundValue(playerI, plotCoord);
//}
//bool GameContext::isAreaBestFoundValueAtLeast(const CvArea& area, PlayerTypes playerI, int iMinValue) const
//{
//	return mInternals->foundValueCacheSystem.isAreaBestFoundValueAtLeast(area, playerI, iMinValue);
//}

void GameContext::prepareForFoundValueQuerying(PlayerTypes playerI)
{
	static auto& stat = StatisticsRegistry::getInstance().grab<TimingStatistic>(__func__);
	const auto timing = stat.scope();
	mInternals->foundValueSystem->update(playerI);
}

std::vector<int> GameContext::computeRevealedFoundValuesRow(PlayerTypes playerI, int y) const
{
	static auto& stat = StatisticsRegistry::getInstance().grab<TimingStatistic>(__func__);
	const auto timing = stat.scope();
	return mInternals->foundValueSystem->computeFoundValuesRow(playerI, FoundValueSystem::System::EFilter::Revealed, y, -1);
}

std::vector<int> GameContext::computeFoundValuesRow(PlayerTypes playerI, int y, int iMinRivalRange) const
{
	static auto& stat = StatisticsRegistry::getInstance().grab<TimingStatistic>(__func__);
	const auto timing = stat.scope();
	return mInternals->foundValueSystem->computeFoundValuesRow(playerI, FoundValueSystem::System::EFilter::None, y, iMinRivalRange);
}

void GameContext::notifyFoundValueUsed([[maybe_unused]] PlayerTypes player, [[maybe_unused]] ivec2 coord, [[maybe_unused]] int iMinRivalRange, [[maybe_unused]] int value) const
{
	if constexpr (kEnableLiteFoundValueVerification)
	{
		if (mInternals->foundValueSystem)
		{
			const int liveValue = GET_PLAYER(player).AI_foundValue(coord.x, coord.y, iMinRivalRange);
			if (liveValue != value)
			{
				std::clog << "Lite found value verification error." << std::endl;
				std::clog << "Player = " << +player << std::endl;
				std::clog << "Coord = " << coord << std::endl;
				std::clog << "iMinRivalRange = " << iMinRivalRange << std::endl;
				std::clog << "Given value = " << value << std::endl;
				std::clog << "Live  value = " << liveValue << std::endl;

				std::clog << "Performing found value system update..." << std::endl;
				mInternals->foundValueSystem->update(player);
				std::clog << "Performing found value system verification..." << std::endl;
				mInternals->foundValueSystem->verify(player);

				std::clog << "Done found value system verification. Dumping eval." << std::endl;

				mInternals->foundValueSystem->dumpEvaluationAtAndAbort(player, coord, iMinRivalRange);
			}
		}
	}
}

VectorisedPathfinderMap& GameContext::getVectorisedPathfinderMap()
{
	assert(mInternals->pathfindingSystem);
	return mInternals->pathfindingSystem->vectorisedPathfinderMap;
}


#endif