#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "GameContext.h"
#include "PlotDangerCache.h"
#include "UnitPathingUtil.h"
#include "Util.h"

#include "../CvGameAI.h"
#include "../CvGlobals.h"
#include "../CvPlayerAI.h"
#include "../CvPlot.h"
#include "../CvTeamAI.h"
#include "../CvRandom.h"
#include "../CvGameCoreUtils.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../CvDLLFAStarIFaceBase.h"
#include "../FAStarNode.h"

#include <bitset>
#include <queue>
#include <unordered_map>

// CvUnitAI::AI_explore

using namespace GiganticMapsOptimisationsLib;

namespace
{
	// Beyond this distance, give up on goodies.
	static constexpr int kOptimisticGoodyGuessMaxDistance = 20;

	using i16vec2 = heck::i16vec2;

	struct ExploreInternals
	{
		const CvUnit& unit;

		const CvPlayerAI& player = GET_PLAYER(unit.getOwnerINLINE());
		const TeamTypes teamI = player.getTeam();
		const bool bNoContact = GC.getGameINLINE().countCivTeamsAlive() > GET_TEAM(teamI).getHasMetCivCount(true);
		const int unitX = unit.getX_INLINE();
		const int unitY = unit.getY_INLINE();
		
		// The value without distance-based stuff.
		int computePlotBaseValue(CvPlot& plot) const
		{
			CvPlot* const pLoopPlot = &plot;

			int iValue = 0;

			if (pLoopPlot->isRevealedGoody(teamI))
			{
				iValue += 100000;
			}

			if (iValue > 0 || GC.getGameINLINE().getSorenRandNum(4, "AI make explore faster ;)") == 0)
			{
				if (!(pLoopPlot->isRevealed(teamI, false)))
				{
					iValue += 10000;
				}
				// XXX is this too slow?
				for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
				{
					//PROFILE("AI_explore 2");

					const CvPlot* const pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

					if (pAdjacentPlot != nullptr)
					{
						if (!(pAdjacentPlot->isRevealed(teamI, false)))
						{
							iValue += 1000;
						}
						else if (bNoContact)
						{
							if (pAdjacentPlot->getRevealedTeam(teamI, false) != pAdjacentPlot->getTeam())
							{
								iValue += 100;
							}
						}
					}
				}

				if (iValue > 0)
				{
					if (!(pLoopPlot->isVisibleEnemyUnit(&unit)))
					{
						if (player.AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, unit.getGroup(), 3) == 0)
						{
							if (!unit.atPlot(pLoopPlot) /*&& generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)*/)
							{
								//iValue += GC.getGameINLINE().getSorenRandNum(250 * abs(xDistance(unit.getX_INLINE(), pLoopPlot->getX_INLINE())) + abs(yDistance(unit.getY_INLINE(), pLoopPlot->getY_INLINE())), "AI explore");

								if (pLoopPlot->isAdjacentToLand())
								{
									iValue += 10000;
								}

								if (pLoopPlot->isOwned())
								{
									iValue += 5000;
								}

								//iValue /= 3 + std::max(1, iPathTurns);
							}
						}
					}
				}
			}

			return iValue;
		}

		// The best possible plot base value you can get from a search starting at this plot.
		// Not strictly, though, the large goody bonus will only be included for nearby plots.
		int guessOptimisticBaseValueForSearch(const CvPlot& plot) const
		{
			const int plotX = plot.getX_INLINE();
			const int plotY = plot.getX_INLINE();
			const int stepDist = stepDistance(unitX, unitY, plotX, plotY);

			int iValue = 0;

			if (stepDist <= kOptimisticGoodyGuessMaxDistance)
			{
				iValue += 100000;
			}

			//if (!(pLoopPlot->isRevealed(teamI, false)))
			{
				iValue += 10000;
			}
			// XXX is this too slow?
			//for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
			{
				//PROFILE("AI_explore 2");

				//const CvPlot* const pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

				//if (pAdjacentPlot != nullptr)
				{
					//if (!(pAdjacentPlot->isRevealed(teamI, false)))
					{
						iValue += 1000 * NUM_DIRECTION_TYPES;
					}
					//else if (bNoContact)
					//{
					//	if (pAdjacentPlot->getRevealedTeam(teamI, false) != pAdjacentPlot->getTeam())
					//	{
					//		iValue += 100;
					//	}
					//}
				}
			}

			//if (iValue > 0)
			{
				//if (!(pLoopPlot->isVisibleEnemyUnit(&unit)))
				{
					//if (player.AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, unit.getGroup(), 3) == 0)
					{
						//if (!unit.atPlot(pLoopPlot) /*&& generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns)*/)
						{
							//iValue += GC.getGameINLINE().getSorenRandNum(250 * abs(xDistance(unit.getX_INLINE(), pLoopPlot->getX_INLINE())) + abs(yDistance(unit.getY_INLINE(), pLoopPlot->getY_INLINE())), "AI explore");

							//if (pLoopPlot->isAdjacentToLand())
							{
								iValue += 10000;
							}

							//if (pLoopPlot->isOwned())
							{
								iValue += 5000;
							}

							//iValue /= 3 + std::max(1, iPathTurns);
						}
					}
				}
			}

			return iValue;
		}

		GameContext::ExploreSearchResult search() const
		{
			// An estimate.
			const int stepsPerTurn = unit.baseMoves();

			VisitSet visitSet;

			// 32-bit element, to get _two_ elements per block! Thanks, MSVC.
			std::queue<i16vec2> workQueue;
			workQueue.emplace(ivec2(unitX, unitY));
			visitSet.set(unitX, unitY);

			const CvMap& map = GC.getMapINLINE();
			CvGame& game = GC.getGameINLINE();
			FAStar& fastar = GC.getPathFinder();
			CvDLLFAStarIFaceBase& fastarInterface = *gDLL->getFAStarIFace();

			// Init FAStar for validity checks.
			fastarInterface.SetData(&fastar, unit.getGroup());
			(void)fastarInterface.startGeneratePath(&fastar, unitX, unitY, unitX, unitY, false, MOVE_NO_ENEMY_TERRITORY, true);
			

			CvPlot* bestPlot{};
			int bestValue = 0;

			while (!workQueue.empty())
			{
				const i16vec2 coord = workQueue.front();
				workQueue.pop();

				CvPlot* const plot = map.plot(coord.x, coord.y);

				const int maxStepValue = 250 * abs(xDistance(unitX, coord.x)) + abs(yDistance(unitY, coord.y));
				const int turns = stepDistance(unitX, unitY, coord.x, coord.y) / stepsPerTurn;

				if (const int baseValue = computePlotBaseValue(*plot))
				{
					const int stepValue = game.getSorenRandNum(maxStepValue, "AI explore");
					const int finalValue = (baseValue + stepValue) / (3 + std::max(1, turns));

					if (finalValue > bestValue)
					{
						bestPlot = plot;
						bestValue = finalValue;
					}
				}

				for (const DirectionTypes adjI : range(NUM_DIRECTION_TYPES))
				{
					if (CvPlot* const adjPlot = plotDirection(coord.x, coord.y, adjI))
					{
						const heck::ivec2 adjCoord{ adjPlot->getX_INLINE(), adjPlot->getY_INLINE() };
						if (!visitSet.test(adjCoord.x, adjCoord.y))
						{
							// Full validity check.
							FAStarNode* const parent = fastarInterface.getNode(&fastar, coord.x, coord.y);
							FAStarNode* const to = fastarInterface.getNode(&fastar, adjCoord.x, adjCoord.y);
							parent->m_iData1 = 0;

							if (pathValid(parent, to, 0, unit.getGroup(), &fastar)
								// Need to check this too, as `pathValid` only checks parent for most stuff.
								&& pathDestValid(adjCoord.x, adjCoord.y, unit.getGroup(), &fastar))
							{
								// Okay, the heuristic: we assume that the best possible paths coming out of this plot can only have increasing distances from the unit.
								// So this won't be accurate if nearby plots are unreachable without going around the long way (nearby non-convexity).
								const int optimisticBaseValue = guessOptimisticBaseValueForSearch(*adjPlot);
								if (optimisticBaseValue + maxStepValue > bestValue * (3 + std::max(1, turns)))
								{
									workQueue.emplace(ivec2(adjCoord.x, adjCoord.y));
									visitSet.set(adjCoord.x, adjCoord.y);
								}
							}
						}
					}
				}
			}

			if (!bestPlot)
				return {};

			GameContext::ExploreSearchResult result{
				.endMissionPlot = bestPlot,
				.target = bestPlot,
			};
			if (bestPlot->isRevealedGoody(teamI))
			{
				// Mission stops on the first turn.
				if (unit.generatePath(bestPlot, MOVE_NO_ENEMY_TERRITORY, true))
					result.endMissionPlot = unit.getPathEndTurnPlot();
				else
				{
					// Non-critical, but this really shouldn't happen.
					std::abort();
				}
			}

			return result;
		}
	};
}

GameContext::ExploreSearchResult GameContext::findExplorationTarget(const CvUnit& unit) const
{
	return ExploreInternals{ .unit = unit }.search();
}

//std::vector<LongRangeAStar::i16vec2> GameContext::findExploreRangeTargets(const CvUnit& unit, int maxStepDist, int maxTurns)
//{
//	// Definitely not for unlimited exploration.
//	if (maxTurns > 1000)
//		std::abort();
//	assert(maxTurns > 0); // We're one-based now, like FAStar.
//
//	NoGoalUnitPathingAStarInterface astarInterface(*this, unit, MOVE_NO_ENEMY_TERRITORY/*, maxTurns*/);
//	
//
//	const hecktui::ivec2 startCoord{ unit.getX_INLINE(), unit.getY_INLINE() };
//
//	//FAStar& fastar = GC.getPathFinder();
//	//CvDLLFAStarIFaceBase& fastarInterface = *gDLL->getFAStarIFace();
//	//fastarInterface.SetData(&fastar, unit.getGroup());
//	//(void)fastarInterface.startGeneratePath(&fastar, startCoord.x, startCoord.y, startCoord.x, startCoord.y, false, MOVE_NO_ENEMY_TERRITORY, true);
//
//	TemporaryUnitPathingState pathingState(unit);
//
//	std::vector<LongRangeAStar::i16vec2> coords = pathingState.visitAllReachable(astarInterface, INT_MAX, maxTurns);
//	std::erase_if(coords, [&](LongRangeAStar::i16vec2 coord) {
//		return !isPathGoalValid(*unit.getGroup(), MOVE_NO_ENEMY_TERRITORY, coord);
//		});
//
//	return coords;
//}

#endif