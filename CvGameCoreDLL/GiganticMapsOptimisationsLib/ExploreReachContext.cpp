#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "ExploreReachContext.h"
#include "Util.h"
#include "UnitPathingUtil.h"

#include "../CvDLLUtilityIFaceBase.h"
#include "../CvDLLFAStarIFaceBase.h"
#include "../FAStarNode.h"
#include "../CvMap.h"
#include "../CvGameAI.h"
#include "../CvGlobals.h"
#include "../CvUnit.h"
#include "../CvPlayerAI.h"
#include "../CvGameCoreUtils.h"

using namespace GiganticMapsOptimisationsLib;

namespace
{
	struct UnitState
	{
		unsigned int mp : 12 {};
		unsigned int turns : 20 {};
	};

	struct PathCostOutput
	{
		int cost = INT_MAX;
		UnitState unitState{};
	};
}

static PathCostOutput calcSingleUnitPathCost(const CvUnit& unit, UnitState fromState, ivec2 parentCoord, ivec2 toCoord)
{
	CvPlot* pFromPlot;
	CvPlot* pToPlot;
	int iWorstCost;
	int iCost;
	int iWorstMovesLeft;
	int iMovesLeft;
	int iWorstMax;
	int iMax;

	pFromPlot = GC.getMapINLINE().plotSorenINLINE(parentCoord.x, parentCoord.y);
	FAssert(pFromPlot != nullptr);
	pToPlot = GC.getMapINLINE().plotSorenINLINE(toCoord.x, toCoord.y);
	FAssert(pToPlot != nullptr);

	iWorstCost = INT_MAX;
	iWorstMovesLeft = INT_MAX;
	iWorstMax = INT_MAX;

	const CvUnit* const pLoopUnit = &unit;
	FAssertMsg(pLoopUnit->getDomainType() != DOMAIN_AIR, "pLoopUnit->getDomainType() is not expected to be equal with DOMAIN_AIR");

	if (fromState.mp > 0)
	{
		iMax = fromState.mp;
	}
	else
	{
		iMax = pLoopUnit->maxMoves();
	}

	iCost = pToPlot->movementCost(pLoopUnit, pFromPlot);

	iMovesLeft = std::max(0, (iMax - iCost));

	if (iMovesLeft <= iWorstMovesLeft)
	{
		if ((iMovesLeft < iWorstMovesLeft) || (iMax <= iWorstMax))
		{
			if (iMovesLeft == 0)
			{
				iCost = (PATH_MOVEMENT_WEIGHT * iMax);

				if (pToPlot->getTeam() != pLoopUnit->getTeam())
				{
					iCost += PATH_TERRITORY_WEIGHT;
				}

				// Damage caused by features (mods)
				//if (0 != GC.getPATH_DAMAGE_WEIGHT())
			}
			else
			{
				iCost = (PATH_MOVEMENT_WEIGHT * iCost);
			}

			if (pLoopUnit->canFight())
			{
				if (iMovesLeft == 0)
				{
					iCost += (PATH_DEFENSE_WEIGHT * std::max(0, (200 - ((pLoopUnit->noDefensiveBonus()) ? 0 : pToPlot->defenseModifier(pLoopUnit->getTeam(), false)))));
				}

				//if (unit.AI_isControlled())
				{
					if (pLoopUnit->canAttack())
					{
						if (gDLL->getFAStarIFace()->IsPathDest(&GC.getPathFinder(), pToPlot->getX_INLINE(), pToPlot->getY_INLINE()))
						{
							if (pToPlot->isVisibleEnemyDefender(pLoopUnit))
							{
								iCost += (PATH_DEFENSE_WEIGHT * std::max(0, (200 - ((pLoopUnit->noDefensiveBonus()) ? 0 : pFromPlot->defenseModifier(pLoopUnit->getTeam(), false)))));

								if (!(pFromPlot->isCity()))
								{
									iCost += PATH_CITY_WEIGHT;
								}

								if (pFromPlot->isRiverCrossing(directionXY(pFromPlot, pToPlot)))
								{
									if (!(pLoopUnit->isRiver()))
									{
										iCost += (PATH_RIVER_WEIGHT * -(GC.getRIVER_ATTACK_MODIFIER()));
										iCost += (PATH_MOVEMENT_WEIGHT * iMovesLeft);
									}
								}
							}
						}
					}
				}
			}

			if (iCost < iWorstCost)
			{
				iWorstCost = iCost;
				iWorstMovesLeft = iMovesLeft;
				iWorstMax = iMax;
			}
		}
	}

	FAssert(iWorstCost != INT_MAX);

	iWorstCost += PATH_STEP_WEIGHT;

	if ((pFromPlot->getX_INLINE() != pToPlot->getX_INLINE()) && (pFromPlot->getY_INLINE() != pToPlot->getY_INLINE()))
	{
		iWorstCost += PATH_STRAIGHT_WEIGHT;
	}

	return { iWorstCost, {.mp = (unsigned int)iMovesLeft, .turns = unsigned(fromState.turns + (fromState.mp == 0)) } };
}

static bool calcSingleUnitPathValid(const CvUnit& unit, unsigned int flags, UnitState fromState, ivec2 parentCoord, ivec2 toCoord)
{
	CvPlot* pFromPlot;
	CvPlot* pToPlot;
	bool bAIControl;

	pFromPlot = GC.getMapINLINE().plotSorenINLINE(parentCoord.x, parentCoord.y);
	FAssert(pFromPlot != nullptr);
	pToPlot = GC.getMapINLINE().plotSorenINLINE(toCoord.x, toCoord.y);
	
	if (!pToPlot)
		return false;

	// XXX might want to take this out...
	if (unit.getDomainType() == DOMAIN_SEA)
	{
		if (pFromPlot->isWater() && pToPlot->isWater())
		{
			if (!(GC.getMapINLINE().plotINLINE(parentCoord.x, toCoord.y)->isWater()) && !(GC.getMapINLINE().plotINLINE(toCoord.x, parentCoord.y)->isWater()))
			{
				return false;
			}
		}
	}

	if (unit.atPlot(pFromPlot))
	{
		return true;
	}

	if (flags & MOVE_SAFE_TERRITORY)
	{
		if (!(pFromPlot->isRevealed(unit.getTeam(), false)))
		{
			return false;
		}

		if (pFromPlot->isOwned())
		{
			if (pFromPlot->getTeam() != unit.getTeam())
			{
				return false;
			}
		}
	}

	if (flags & MOVE_NO_ENEMY_TERRITORY)
	{
		if (pFromPlot->isOwned())
		{
			if (atWar(pFromPlot->getTeam(), unit.getTeam()))
			{
				return false;
			}
		}
	}

	bAIControl = true; // unit.AI_isControlled();

	if (bAIControl)
	{
		if ((fromState.turns > 1) || (fromState.mp == 0))
		{
			if (!(flags & MOVE_IGNORE_DANGER))
			{
				if (!(unit.canFight()) && !(unit.alwaysInvisible()))
				{
					//#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
					//					if (gGlobals.enhancedDLLGameContext->hasPotentialPlotDanger(unit.getTeam(), *pFromPlot))
					//#endif
					if (GET_PLAYER(unit.getOwnerINLINE()).AI_getPlotDanger(pFromPlot) > 0)
					{
						return false;
					}
				}
			}
		}
	}

	if (bAIControl || pFromPlot->isRevealed(unit.getTeam(), false))
	{
		if (flags & MOVE_THROUGH_ENEMY)
		{
			if (!(unit.canMoveOrAttackInto(pFromPlot)))
			{
				return false;
			}
		}
		else
		{
			if (!(unit.canMoveThrough(pFromPlot)))
			{
				return false;
			}
		}
	}

	return true;
}

static bool calcSingleUnitPathDestValid(const CvUnit& unit, unsigned int flags, ivec2 toCoord)
{
	CvPlot* pToPlot;
	bool bAIControl;
	bool bValid;

	pToPlot = GC.getMapINLINE().plotSorenINLINE(toCoord.x, toCoord.y);
	FAssert(pToPlot != nullptr);

	if (unit.atPlot(pToPlot))
	{
		return true;
	}

	if (unit.getDomainType() == DOMAIN_IMMOBILE)
	{
		return false;
	}

	bAIControl = true; //unit.AI_isControlled();

	if (bAIControl)
	{
		if (!(flags & MOVE_IGNORE_DANGER))
		{
			if (!(unit.canFight()) && !(unit.alwaysInvisible()))
			{
				//#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
				//				if (gGlobals.enhancedDLLGameContext->hasPotentialPlotDanger(unit.getTeam(), *pToPlot))
				//#endif
				if (GET_PLAYER(unit.getOwnerINLINE()).AI_getPlotDanger(pToPlot) > 0)
				{
					return false;
				}
			}
		}

		if (unit.getDomainType() == DOMAIN_LAND)
		{
			int iGroupAreaID = unit.getArea();
			if (pToPlot->getArea() != iGroupAreaID)
			{
				if (!(pToPlot->isAdjacentToArea(iGroupAreaID)))
				{
					return false;
				}
			}
		}
	}

	if (bAIControl || pToPlot->isRevealed(unit.getTeam(), false))
	{
		if (unit.getGroup()->isAmphibPlot(pToPlot))
		{
			if ((unit.getCargo() > 0) && (unit.domainCargo() == DOMAIN_LAND))
			{
				bValid = false;

				auto* pUnitNode2 = unit.plot()->headUnitNode();

				while (pUnitNode2 != nullptr)
				{
					CvUnit* const pLoopUnit2 = ::getUnit(pUnitNode2->m_data);
					pUnitNode2 = unit.plot()->nextUnitNode(pUnitNode2);

					if (pLoopUnit2->getTransportUnit() == &unit)
					{
						if (pLoopUnit2->isGroupHead())
						{
							if (pLoopUnit2->getGroup()->canMoveOrAttackInto(pToPlot, (getSingleUnit_AI_isDeclareWar(unit, pToPlot) || (flags & MOVE_DECLARE_WAR))))
							{
								bValid = true;
								break;
							}
						}
					}
				}

				if (bValid)
				{
					return true;
				}
			}

			return false;
		}
		else
		{
			if (!(unit.canMoveOrAttackInto(pToPlot, (getSingleUnit_AI_isDeclareWar(unit, pToPlot) || (flags & MOVE_DECLARE_WAR)))))
			{
				return false;
			}
		}
	}

	return true;
}

struct ExploreReachContext::Internals
{
	struct NodeState
	{
		int gcost = INT_MAX;
		UnitState unitState{};
	};

	// For some reason, clang-cl wants members initialised explicitly.
	TwoLevelLookup<NodeState, NodeState{ .gcost = INT_MAX, .unitState{} }> visitSet;
	std::vector<heck::i16vec2> reachableSet;

	void visitAllReachable(const CvUnit& unit, uint32_t flags, int maxTurns)
	{
		if (unit.getGroup()->getNumUnits() != 1)
			std::abort();

		const CvMap& map = GC.getMapINLINE();
		const CivMapGeometry mapSizeInfo(map);
		//FAStar& fastar = GC.getPathFinder();
		//CvDLLFAStarIFaceBase& fastarInterface = *gDLL->getFAStarIFace();

		const ivec2 startCoord = getPlotCoord(*unit.plot());

		// Init FAStar for validity checks.
		//fastarInterface.SetData(&fastar, unit.getGroup());
		//(void)fastarInterface.startGeneratePath(&fastar, startCoord.x, startCoord.y, startCoord.x, startCoord.y, false, flags, false);

		struct Entry
		{
			int value = 0;
			i16vec2 coord{};

			bool operator<(Entry b) const
			{
				return value > b.value;
			}
		};

		std::vector<Entry> heap;
		heap.emplace_back(0, startCoord);

		//if (visitSet.cells.empty())
		//	visitSet = DynamicArray2D<NodeState>(CivMapGeometry(map).dim);

		// Use the last reach set to clear the visit set.
		//for (const i16vec2 coord : reachableSet)
		//	visitSet[coord] = {};
		visitSet.clear();

		visitSet[startCoord] = {
			.gcost = 0,
			.unitState{.mp = (unsigned)unit.maxMoves() },
		};

		reachableSet.clear();

		while (!heap.empty())
		{
			const ivec2 fromCoord = heap.front().coord;
			std::ranges::pop_heap(heap, std::less<>());
			heap.pop_back();

			const auto [fromCost, fromState] = visitSet[fromCoord];

			reachableSet.push_back(fromCoord);

			for (const ivec2 adjD : kAdj)
			{
				const ivec2 unwrappedToCoord = fromCoord + adjD;
				if (mapSizeInfo.isValidCoord(unwrappedToCoord))
				{
					const ivec2 toCoord = mapSizeInfo.wrapCoord(unwrappedToCoord);
					if (calcSingleUnitPathValid(unit, flags, fromState, fromCoord, toCoord) &&
						calcSingleUnitPathDestValid(unit, flags, toCoord))
					{
						const auto [cost, toState] = calcSingleUnitPathCost(unit, fromState, fromCoord, toCoord);
						const int newGCost = fromCost + cost;
						if (newGCost < visitSet[toCoord].gcost && static_cast<int>(toState.turns) <= maxTurns) // cast safe as turns is a bit field
						{
							visitSet[toCoord] = { newGCost, toState };
							heap.emplace_back(newGCost, toCoord);
							std::ranges::push_heap(heap, std::less<>());
						}
					}
				}
			}
		}

		
	}

	std::span<const i16vec2> getVisitSet() const
	{
		return reachableSet;
	}

	i16vec2 getEndTurnStopCoord(const CvUnit& unit, [[maybe_unused]] uint32_t flags, ivec2 toCoord) const
	{
		// Must be reachable.
		assert(visitSet[toCoord].gcost < INT_MAX);

		
		while (visitSet[toCoord].unitState.turns > 0)
		{
			int bestCostFromParent = INT_MAX;
			i16vec2 bestParentCoord{};

			for (const ivec2 adjD : kAdj)
			{
				const ivec2 fromCoord = toCoord + adjD;
				const int fromCost = visitSet[fromCoord].gcost;
				assert(fromCost >= 0);
				if (fromCost < INT_MAX)
				{
					const UnitState fromState = visitSet[fromCoord].unitState;
					const auto [cost, toState] = calcSingleUnitPathCost(unit, fromState, fromCoord, toCoord);
					const int newGCost = fromCost + cost;
					assert(newGCost > 0);
					if (newGCost < bestCostFromParent)
					{
						bestCostFromParent = newGCost;
						bestParentCoord = fromCoord;
					}
				}
			}

			toCoord = bestParentCoord;

			// Can't be the start plot.
			assert(visitSet[toCoord].gcost > 0);
		}

		return toCoord;
	}
};

ExploreReachContext::ExploreReachContext() : mInternals(std::make_unique<Internals>())
{
}
ExploreReachContext::ExploreReachContext(ExploreReachContext&&) noexcept = default;
ExploreReachContext& ExploreReachContext::operator=(ExploreReachContext&&) noexcept = default;
ExploreReachContext::~ExploreReachContext() = default;

void ExploreReachContext::visitAllReachable(const CvUnit& unit, uint32_t flags, int maxTurns)
{
	return mInternals->visitAllReachable(unit, flags, maxTurns);
}
std::span<const i16vec2> ExploreReachContext::getVisitSet() const
{
	return mInternals->getVisitSet();
}
bool ExploreReachContext::isVisited(heck::ivec2 p) const
{
	return mInternals->visitSet[p].gcost < INT_MAX;
}
i16vec2 ExploreReachContext::getEndTurnStopCoord(const CvUnit& unit, uint32_t flags, ivec2 coord) const
{
	return mInternals->getEndTurnStopCoord(unit, flags, coord);
}

#endif