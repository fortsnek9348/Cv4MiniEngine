#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_GroupUpdateRecord.h"
#include "AIParallelUnitUpdate_DryRunGroupState.h"

#include <syncstream>
#include <iostream>

using namespace GiganticMapsOptimisationsLib;

static bool anyUnit(const DryRunGroupState& group, auto f)
{
	return std::ranges::any_of(group.units, f);
}

static bool allUnitsOrEmpty(const DryRunGroupState& group, auto f)
{
	return std::ranges::all_of(group.units, f);
}

//static bool hasAnyUnitCanCarryCargo(const DryRunGroupState& group)
//{
//	return anyUnit(group, [](const DryRunUnitState& unit) { return unit.liveUnit.specialCargo() != NO_SPECIALUNIT || unit.liveUnit.domainCargo() != NO_DOMAIN; });
//}

DryRunGroupState::DryRunGroupState(const CvSelectionGroupAI& group)
	: liveGroup(&group)
	, activityType(group.getActivityType())
	, missionTimer(group.getMissionTimer())
	, m_eMissionAIType(group.AI_getMissionAIType())
	, m_iMissionAICoord(group.AI_getMissionAIPlot() ? (ivec2)getPlotCoord(*group.AI_getMissionAIPlot()) : ivec2(INVALID_PLOT_COORD))
	, m_missionAIUnit(group.AI_getMissionAIUnit() ? group.AI_getMissionAIUnit()->getIDInfo() : IDInfo())
	, m_bGroupAttack(group.AI_isGroupAttack())
	, forceUpdate(group.isForceUpdate())
	, automateType(group.getAutomateType())
	, m_missionQueue(buildMissionQueue())
{
	units.reserve(group.getNumUnits());
	for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
		units.emplace_back(static_cast<const CvUnitAI&>(*::getUnit(node->m_data)));
}

bool DryRunGroupState::hasHeadMissionQueueNode() const
{
	return !m_missionQueue.empty();
}

const MissionData* DryRunGroupState::headMissionQueueNode() const
{
	return m_missionQueue.empty() ? nullptr : &m_missionQueue.front();
}

TeamTypes DryRunGroupState::getTeam() const
{
	return liveGroup->getTeam();
}

PlayerTypes DryRunGroupState::getOwner() const
{
	return liveGroup->getOwner();
}

const CvPlot* DryRunGroupState::plot() const
{
	if (units.empty())
		return nullptr;
	else
		return units.front().plot();
}

// Return false if serial fallback should be used.
bool DryRunGroupState::setBlockading(bool bStart)
{
	return !bStart && !anyUnit(*this, &DryRunUnitState::isBlockading);
}

// CvSelectionGroup::setMissionTimer(int iNewValue)
void DryRunGroupState::setMissionTimer(int iNewValue)
{
	missionTimer = iNewValue;
}

bool DryRunGroupState::isBusy() const
{
	return getNumUnits() != 0 && (missionTimer > 0 || anyUnit(*this, &DryRunUnitState::isCombat));
}

bool DryRunGroupState::isCargoBusy() const
{
	// Not implemented.
	FAssert(liveGroup->getCargo() == 0);
	return false;
}

/// CvSelectionGroup::setActivityType
void DryRunGroupState::setActivityType(ActivityTypes eNewValue, GroupUpdateRecord& record)
{
	FAssert(getOwner() != NO_PLAYER);

	ActivityTypes eOldActivity = activityType;

	if (eOldActivity != eNewValue)
	{
		const CvPlot* pPlot = plot();

		if (eOldActivity == ACTIVITY_INTERCEPT)
		{
			record.setUseSerialFallback(std::string(__func__) + " eOldActivity == ACTIVITY_INTERCEPT");
			return;
		}

		if (!setBlockading(false))
		{
			record.setUseSerialFallback(std::string(__func__) + " !setBlockading");
			return;
		}

		activityType = eNewValue;

		if (activityType == ACTIVITY_INTERCEPT)
		{
			record.setUseSerialFallback(std::string(__func__) + " activityType == ACTIVITY_INTERCEPT");
			return;
		}

		if (activityType != ACTIVITY_MISSION)
		{
			//if (activityType != ACTIVITY_INTERCEPT)
			//	record.entityNotifyIdle = true;

			if (getTeam() == GC.getGameINLINE().getActiveTeam() && pPlot)
				record.setIsPlotSetDirtyFlag();
		}

		if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
			record.setDirtyPlotListButtonsAndSelectionButtons();
	}
}

/// CvSelectionGroup::clearMissionQueue
void DryRunGroupState::clearMissionQueue(GroupUpdateRecord& record)
{
	FAssert(getOwner() != NO_PLAYER);

	/// CvSelectionGroup::deactivateHeadMission()
	if (hasHeadMissionQueueNode())
	{
		if (activityType == ACTIVITY_MISSION)
			setActivityType(ACTIVITY_AWAKE, record);

		setMissionTimer(0);

		// Not for AI
		//if (getOwner() == GC.getGameINLINE().getActivePlayer())
		//{
		//	if (IsSelected())
		//	{
		//		gDLL->getInterfaceIFace()->changeCycleSelectionCounter(1);
		//	}
		//}
	}

	//for (const MissionData& mission : m_missionQueue)
	//	record.invalidateMissionConflictAvoidance(mission.eMissionType);
	m_missionQueue.clear();

	// Not for AI
	//if ((getOwner() == GC.getGameINLINE().getActivePlayer()) && IsSelected())
	//{
	//	gDLL->getInterfaceIFace()->setDirty(Waypoints_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	//}
}


/// CvSelectionGroupAI::AI_setMissionAI
void DryRunGroupState::AI_setMissionAI([[maybe_unused]] GroupUpdateRecord& record, MissionAITypes eNewMissionAI, const CvPlot* pNewPlot, const CvUnit* pNewUnit)
{
	m_eMissionAIType = eNewMissionAI;

	if (pNewPlot)
		m_iMissionAICoord = getPlotCoord(*pNewPlot);
	else
		m_iMissionAICoord = INVALID_PLOT_COORD;

	if (pNewUnit)
		m_missionAIUnit = pNewUnit->getIDInfo();
	else
		m_missionAIUnit.reset();
}


size_t DryRunGroupState::getLengthMissionQueue() const
{
	return m_missionQueue.size();
}

/// CvSelectionGroup::insertAtEndMissionQueue
// The big one.
void DryRunGroupState::insertAtEndMissionQueue(MissionData mission, bool bStart, GroupUpdateRecord& record, IPathFinder& pathFinder)
{
	FAssert(getOwner() != NO_PLAYER);

	m_missionQueue.push_back(mission);

	// We commit the pathing dependency here.
	//record.pathingMapReadDependency |= pathingMapReadDependency;

	if ((getLengthMissionQueue() == 1) && bStart)
	{
		activateHeadMission(pathFinder, record);
	}

	// Not for AI.
	//if ((getOwner() == GC.getGameINLINE().getActivePlayer()) && IsSelected())
	//{
	//	gDLL->getInterfaceIFace()->setDirty(Waypoints_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	//}
}

/// CvSelectionGroup::activateHeadMission
void DryRunGroupState::activateHeadMission(IPathFinder& pathFinder, GroupUpdateRecord& record)
{
	FAssert(getOwner() != NO_PLAYER);

	if (hasHeadMissionQueueNode() && !isBusy())
		startMission(record, pathFinder);
}

	
	
bool DryRunGroupState::isWaiting() const
{
	return ((activityType == ACTIVITY_HOLD) ||
		(activityType == ACTIVITY_SLEEP) ||
		(activityType == ACTIVITY_HEAL) ||
		(activityType == ACTIVITY_SENTRY) ||
		(activityType == ACTIVITY_PATROL) ||
		(activityType == ACTIVITY_PLUNDER) ||
		(activityType == ACTIVITY_INTERCEPT));
}

/// bool CvSelectionGroup::canStartMission(int iMission, int iData1, int iData2, CvPlot* pPlot, bool bTestVisible, bool bUseCache)
// bTestVisible = false
// bUseCache = false
bool DryRunGroupState::canStartMission(int iMission, int iData1, int iData2, const CvPlot* pPlot, GroupUpdateRecord& record)
{
	//cache isBusy
	//if (bUseCache)
	//{
	//	if (m_bIsBusyCache)
	//	{
	//		return false;
	//	}
	//}
	//else
	{
		if (isBusy())
		{
			return false;
		}
	}

	if (pPlot == nullptr)
	{
		pPlot = plot();
	}

	for (DryRunUnitState& unit : units)
	{
		switch (iMission)
		{
		case MISSION_MOVE_TO:
			if (!(pPlot->at(iData1, iData2)))
			{
				return true;
			}
			break;

		case MISSION_ROUTE_TO:
			if (!(pPlot->at(iData1, iData2)) || (getBestBuildRoute(pPlot) != NO_ROUTE))
			{
				return true;
			}
			break;

		case MISSION_MOVE_TO_UNIT:
		{
			FAssertMsg(iData1 != NO_PLAYER, "iData1 should be a valid Player");
			if (const CvUnit* const pTargetUnit = GET_PLAYER((PlayerTypes)iData1).getUnit(iData2))
			{
				record.addDependencyOnUnitMovement(getOwner(), pTargetUnit);
				if (!pTargetUnit->atPlot(pPlot))
					return true;
			}
			break;
		}

		case MISSION_SKIP:
			if (unit.canHold())
			{
				return true;
			}
			break;

		case MISSION_SLEEP:
			if (unit.canSleep(*this))
			{
				return true;
			}
			break;

		case MISSION_FORTIFY:
			if (unit.canFortify(*this))
			{
				return true;
			}
			break;

		case MISSION_HEAL:
			if (unit.canHeal(*this, pPlot, record))
			{
				return true;
			}
			break;

		case MISSION_SENTRY:
			if (unit.canSentry(*this, pPlot))
			{
				return true;
			}
			break;


		case MISSION_BEGIN_COMBAT:
		case MISSION_END_COMBAT:
		case MISSION_AIRSTRIKE:
		case MISSION_SURRENDER:
		case MISSION_IDLE:
		case MISSION_DIE:
		case MISSION_DAMAGE:
		case MISSION_MULTI_SELECT:
		case MISSION_MULTI_DESELECT:
			break;

			// Not implemented for parallel unit update.
		case MISSION_AIRPATROL:
		case MISSION_SEAPATROL:
		case MISSION_AIRLIFT:
		case MISSION_NUKE:
		case MISSION_RECON:
		case MISSION_PARADROP:
		case MISSION_AIRBOMB:
		case MISSION_BOMBARD:
		case MISSION_RANGE_ATTACK:
		case MISSION_PLUNDER:
		case MISSION_PILLAGE:
		case MISSION_SABOTAGE:
		case MISSION_DESTROY:
		case MISSION_STEAL_PLANS:
		case MISSION_FOUND:
		case MISSION_SPREAD:
		case MISSION_SPREAD_CORPORATION:
		case MISSION_JOIN:
		case MISSION_CONSTRUCT:
		case MISSION_DISCOVER:
		case MISSION_HURRY:
		case MISSION_TRADE:
		case MISSION_GREAT_WORK:
		case MISSION_INFILTRATE:
		case MISSION_GOLDEN_AGE:
		case MISSION_BUILD:
		case MISSION_LEAD:
		case MISSION_ESPIONAGE:
		case MISSION_DIE_ANIMATION:
		default:
			record.setUseSerialFallback(std::string(__func__) + " Mission unimplemented");
			return false;
		}
	}

	return false;
}


/// void CvSelectionGroup::startMission()
void DryRunGroupState::startMission(GroupUpdateRecord& record, IPathFinder& pathFinder)
{
	bool bDelete;
	bool bNuke;

	FAssert(!isBusy());
	FAssert(getOwner() != NO_PLAYER);
	FAssert(hasHeadMissionQueueNode());

	if (!GC.getGameINLINE().isMPOption(MPOPTION_SIMULTANEOUS_TURNS))
	{
		if (!GET_PLAYER(getOwner()).isTurnActive())
		{
			// Not for AI.
			//if (getOwner() == GC.getGameINLINE().getActivePlayer())
			//{
			//	if (IsSelected())
			//	{
			//		gDLL->getInterfaceIFace()->changeCycleSelectionCounter(1);
			//	}
			//}

			record.setUseSerialFallback(std::string(__func__) + " !GET_PLAYER(getOwner()).isTurnActive()");
			return;
		}
	}

	if (canAllMove())
	{
		setActivityType(ACTIVITY_MISSION, record);
	}
	else
	{
		setActivityType(ACTIVITY_HOLD, record);
	}

	bDelete = false;
	[[maybe_unused]] bool bAction = false;
	bNuke = false;
	[[maybe_unused]] bool bNotify = false;

	if (!canStartMission(headMissionQueueNode()->eMissionType, headMissionQueueNode()->iData1, headMissionQueueNode()->iData2, plot(), record))
	{
		bDelete = true;
	}
	else
	{
		FAssertMsg(GET_PLAYER(getOwner()).isTurnActive() || GET_PLAYER(getOwner()).isHuman(), "It's expected that either the turn is active for this player or the player is human");

		switch (headMissionQueueNode()->eMissionType)
		{
		case MISSION_MOVE_TO:
		case MISSION_ROUTE_TO:
		case MISSION_MOVE_TO_UNIT:
			break;

		case MISSION_SKIP:
			setActivityType(ACTIVITY_HOLD, record);
			bDelete = true;
			break;

		case MISSION_SLEEP:
			setActivityType(ACTIVITY_SLEEP, record);
			bNotify = true;
			bDelete = true;
			break;

		case MISSION_FORTIFY:
			setActivityType(ACTIVITY_SLEEP, record);
			bNotify = true;
			bDelete = true;
			break;

		case MISSION_PLUNDER:
			setActivityType(ACTIVITY_PLUNDER, record);
			bNotify = true;
			bDelete = true;
			break;

		case MISSION_AIRPATROL:
			setActivityType(ACTIVITY_INTERCEPT, record);
			bDelete = true;
			break;

		case MISSION_SEAPATROL:
			setActivityType(ACTIVITY_PATROL, record);
			bDelete = true;
			break;

		case MISSION_HEAL:
			setActivityType(ACTIVITY_HEAL, record);
			bNotify = true;
			bDelete = true;
			break;

		case MISSION_SENTRY:
			setActivityType(ACTIVITY_SENTRY, record);
			bNotify = true;
			bDelete = true;
			break;

		case MISSION_AIRLIFT:
		case MISSION_NUKE:
		case MISSION_RECON:
		case MISSION_PARADROP:
		case MISSION_AIRBOMB:
		case MISSION_BOMBARD:
		case MISSION_RANGE_ATTACK:
		case MISSION_PILLAGE:
		case MISSION_SABOTAGE:
		case MISSION_DESTROY:
		case MISSION_STEAL_PLANS:
		case MISSION_FOUND:
		case MISSION_SPREAD:
		case MISSION_SPREAD_CORPORATION:
		case MISSION_JOIN:
		case MISSION_CONSTRUCT:
		case MISSION_DISCOVER:
		case MISSION_HURRY:
		case MISSION_TRADE:
		case MISSION_GREAT_WORK:
		case MISSION_INFILTRATE:
		case MISSION_GOLDEN_AGE:
		case MISSION_BUILD:
		case MISSION_LEAD:
		case MISSION_ESPIONAGE:
		case MISSION_DIE_ANIMATION:
			break;

		default:
			FAssert(false);
			break;
		}

		//if (bNotify)
		//{
		//	NotifyEntity(headMissionQueueNode()->m_data.eMissionType);
		//}

		for (DryRunUnitState& unit : units)
		{
			if (unit.canMove())
			{
				switch (headMissionQueueNode()->eMissionType)
				{
				case MISSION_MOVE_TO:
				case MISSION_ROUTE_TO:
				case MISSION_MOVE_TO_UNIT:
				case MISSION_SKIP:
				case MISSION_SLEEP:
				case MISSION_FORTIFY:
				case MISSION_AIRPATROL:
				case MISSION_SEAPATROL:
				case MISSION_HEAL:
				case MISSION_SENTRY:
					break;

					// Not implemented for parallel unit update.
				case MISSION_AIRLIFT:
				case MISSION_NUKE:
				case MISSION_RECON:
				case MISSION_PARADROP:
				case MISSION_AIRBOMB:
				case MISSION_BOMBARD:
				case MISSION_RANGE_ATTACK:
				case MISSION_PILLAGE:
				case MISSION_PLUNDER:
				case MISSION_SABOTAGE:
				case MISSION_DESTROY:
				case MISSION_STEAL_PLANS:
				case MISSION_FOUND:
				case MISSION_SPREAD:
				case MISSION_SPREAD_CORPORATION:
				case MISSION_JOIN:
				case MISSION_CONSTRUCT:
				case MISSION_DISCOVER:
				case MISSION_HURRY:
				case MISSION_TRADE:
				case MISSION_GREAT_WORK:
				case MISSION_INFILTRATE:
				case MISSION_GOLDEN_AGE:
				case MISSION_BUILD:
				case MISSION_LEAD:
				case MISSION_ESPIONAGE:
				case MISSION_DIE_ANIMATION:
				default:
					record.setUseSerialFallback(std::string(__func__) + " Mission not implemented");
					break;
				}

				if (getNumUnits() == 0)
				{
					break;
				}

				if (headMissionQueueNode() == nullptr)
				{
					break;
				}
			}
		}
	}

	if ((getNumUnits() > 0) && (headMissionQueueNode() != nullptr))
	{
		//if (bAction)
		//{
		//	if (isHuman())
		//	{
		//		if (plot()->isVisibleToWatchingHuman())
		//		{
		//			updateMissionTimer();
		//		}
		//	}
		//}

		if (bNuke)
		{
			//setMissionTimer(GC.getMissionInfo(MISSION_NUKE).getTime());
			record.setUseSerialFallback(std::string(__func__) + " Nuke");
		}

		if (!isBusy())
		{
			if (bDelete)
			{
				//if (getOwner() == GC.getGameINLINE().getActivePlayer())
				//{
				//	if (IsSelected())
				//	{
				//		gDLL->getInterfaceIFace()->changeCycleSelectionCounter((GET_PLAYER(getOwner()).isOption(PLAYEROPTION_QUICK_MOVES)) ? 1 : 2);
				//	}
				//}
				//
				//deleteMissionQueueNode(headMissionQueueNode());
				record.setUseSerialFallback(std::string(__func__) + " bDelete");
			}
			else if (activityType == ACTIVITY_MISSION)
			{
				continueMission(record, pathFinder, 0);
			}
		}
	}
}

int DryRunGroupState::getNumUnits() const
{
	return (int)units.size();
}

DomainTypes DryRunGroupState::getDomainType() const
{
	if (const DryRunUnitState* const unit = getHeadUnit())
		return unit->liveUnit.getDomainType();
	else
		return NO_DOMAIN;
}

// bool CvSelectionGroup::canAllMove() const
bool DryRunGroupState::canAllMove() const
{
	return getNumUnits() > 0 && allUnitsOrEmpty(*this, &DryRunUnitState::canMove);
}

bool DryRunGroupState::canAnyMove() const
{
	return anyUnit(*this, &DryRunUnitState::canMove);
}

UnitAITypes DryRunGroupState::getHeadUnitAI() const
{
	return liveGroup->getHeadUnitAI();
}

MissionAITypes DryRunGroupState::AI_getMissionAIType() const
{
	return m_eMissionAIType;
}

// CvSelectionGroup::getBestBuildRoute
RouteTypes DryRunGroupState::getBestBuildRoute(const CvPlot* pPlot, BuildTypes* peBestBuild) const
{
	return liveGroup->getBestBuildRoute(pPlot, peBestBuild);
}

bool DryRunGroupState::atPlot(const CvPlot* plot) const
{
	return getHeadUnit()->atPlot(plot);
}

bool DryRunGroupState::at(int x, int y) const
{
	return getHeadUnit()->at(x, y);
}

/// CvUnit* CvSelectionGroupAI::AI_getBestGroupAttacker(const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce, bool bNoBlitz) const
DryRunUnitState* DryRunGroupState::AI_getBestGroupAttacker(GroupUpdateRecord& record, const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce, bool bNoBlitz)
{
	//record.dependsOnPathPlotsEnemyUnits = true;
	//return liveGroup->AI_getBestGroupAttacker(pPlot, bPotentialEnemy, iUnitOdds, bForce, bNoBlitz);

	DryRunUnitState* pBestUnit{};
	int iPossibleTargets;
	int iValue;
	int iBestValue;
	int iOdds;
	int iBestOdds;

	iBestValue = 0;
	iBestOdds = 0;
	pBestUnit = nullptr;

	constexpr bool bIsHuman = false;

	for (DryRunUnitState& pLoopUnit : units)
	{
		if (!pLoopUnit.liveUnit.isDead())
		{
			bool bCanAttack = false;
			if (pLoopUnit.getDomainType() == DOMAIN_AIR)
			{
				bCanAttack = pLoopUnit.liveUnit.canAirAttack();
			}
			else
			{
				bCanAttack = pLoopUnit.liveUnit.canAttack();

				if (bCanAttack && bNoBlitz && pLoopUnit.liveUnit.isBlitz() && pLoopUnit.liveUnit.isMadeAttack())
				{
					bCanAttack = false;
				}
			}

			if (bCanAttack)
			{
				if (bForce || pLoopUnit.canMove())
				{
					if (bForce || pLoopUnit.canMoveInto(record, *this, pPlot, /*bAttack*/ true, /*bDeclareWar*/ bPotentialEnemy))
					{
						iOdds = pLoopUnit.liveUnit.AI_attackOdds(pPlot, bPotentialEnemy);

						iValue = iOdds;
						FAssertMsg(iValue > 0, "iValue is expected to be greater than 0");

						if (pLoopUnit.liveUnit.collateralDamage() > 0)
						{
							iPossibleTargets = std::min((record.getNumVisibleEnemyDefenders(pPlot, pLoopUnit.liveUnit) - 1), pLoopUnit.liveUnit.collateralDamageMaxUnits());

							if (iPossibleTargets > 0)
							{
								iValue *= (100 + ((pLoopUnit.liveUnit.collateralDamage() * iPossibleTargets) / 5));
								iValue /= 100;
							}
						}

						// if non-human, prefer the last unit that has the best value (so as to avoid splitting the group)
						if (iValue > iBestValue || (!bIsHuman && iValue > 0 && iValue == iBestValue))
						{
							iBestValue = iValue;
							iBestOdds = iOdds;
							pBestUnit = &pLoopUnit;
						}
					}
				}
			}
		}
	}

	iUnitOdds = iBestOdds;
	return pBestUnit;
		
}

static bool calc_pathDestValid(DryRunGroupState& group, unsigned int flags, ivec2 goal, GroupUpdateRecord& record)
{
	const CvPlot* pToPlot;

	pToPlot = GC.getMapINLINE().plotSorenINLINE(goal.x, goal.y);
	FAssert(pToPlot != nullptr);

	if (group.atPlot(pToPlot))
	{
		return true;
	}

	if (group.getDomainType() == DOMAIN_IMMOBILE)
	{
		return false;
	}

	constexpr bool bAIControl = true;

	if constexpr (bAIControl)
	{
		if (!(flags & MOVE_IGNORE_DANGER))
		{
			if (!(group.canFight()) && !(group.liveGroup->alwaysInvisible()))
			{
				// NOTE: Could use the path danger cache if necessary.
				if (GET_PLAYER(group.getOwner()).AI_getPlotDanger(pToPlot) > 0)
				{
					return false;
				}
			}
		}

		if (group.getDomainType() == DOMAIN_LAND)
		{
			const int iGroupAreaID = group.getArea();
			if (pToPlot->getArea() != iGroupAreaID)
			{
				if (!(pToPlot->isAdjacentToArea(iGroupAreaID)))
				{
					return false;
				}
			}
		}
	}

	if constexpr (bAIControl)// || pToPlot->isRevealed(pSelectionGroup->getHeadTeam(), false))
	{
		if (group.isAmphibPlot(pToPlot))
		{
			for (const DryRunUnitState& pLoopUnit1 : group.units)
			{
				if (pLoopUnit1.hasCargo() && pLoopUnit1.liveUnit.domainCargo() == DOMAIN_LAND)
				{
					record.setUseSerialFallback(std::string(__func__) + " Movement of non-empty transport.");
					return false;
				}
			}

			return false;
		}
		else
		{
			if (!group.canMoveOrAttackInto(record, pToPlot, group.AI_isDeclareWar(pToPlot) || (flags & MOVE_DECLARE_WAR)))
			{
				return false;
			}
		}
	}

	return true;
}

/// bool generatePath(const CvPlot* pFromPlot, const CvPlot* pToPlot, PathingArg pathingArg = 0, bool bReuse = false, int* piPathTurns = nullptr) const;	// Exposed to Python
bool DryRunGroupState::generatePathFromTo(GroupUpdateRecord& record, IPathFinder& pathFinder, const CvPlot* pFromPlot, const CvPlot* pToPlot,
	PathingArg pathingArg, [[maybe_unused]] bool bReuse, int* piPathTurns)
{
	if (piPathTurns)
		*piPathTurns = INT_MAX;

	// TODO: All these checks are the same as in GameContext.

	if (getNumUnits() != 1)
	{
		// Not implemented in path finder.
		record.setUseSerialFallback(std::string(__func__) + " getNumUnits() != 1");
		return false;
	}

	if (pathingArg.iFlags & MOVE_SAFE_TERRITORY)
	{
		// TODO: This flag is invalidated by change in TeamPathingPlotPropsCache::kPlotPropsBitIndex_PathValid_NotSafeTerritory, which is not implemented.
		record.setUseSerialFallback(std::string(__func__) + " pathingArg.iFlags & MOVE_SAFE_TERRITORY");
		return false;
	}

	if (pathingArg.iFlags & ~(MOVE_SAFE_TERRITORY | MOVE_NO_ENEMY_TERRITORY | MOVE_THROUGH_ENEMY))
	{
		// Flags unsupported by the pathfinder.
		record.setUseSerialFallback(std::string(__func__) + " unsupported pathingArg.iFlags");
		return false;
	}

	if ((pathingArg.iFlags & MOVE_THROUGH_ENEMY) != 0 && getHeadUnit()->liveUnit.combatLimit() < 100)
	{
		record.setUseSerialFallback(std::string(__func__) + " MOVE_THROUGH_ENEMY with siege");
		return false;
	}

	if (getDomainType() != DOMAIN_LAND && getDomainType() != DOMAIN_SEA)
	{
		// Not implemented for pathing.
		record.setUseSerialFallback(std::string(__func__) + " getDomainType() != DOMAIN_LAND");
		return false;
	}

	// Have to check pathDestValid first.
	if (!calc_pathDestValid(*this, pathingArg.iFlags, getPlotCoord(*pToPlot), record))
		return false;

	const bool found = pathFinder.findPath(
		getPlotCoord(*pFromPlot), getPlotCoord(*pToPlot), pathingArg.iFlags, pathingArg.iMaxTurns,
		getHeadUnit()->liveUnit.getGroup(), getHeadUnit()->movesLeft()
	);

	if (found && piPathTurns)
		*piPathTurns = pathFinder.getLastNodeTurns();

	return found;
}

bool DryRunGroupState::generatePath(GroupUpdateRecord& record, IPathFinder& pathFinder, const CvPlot* pToPlot, PathingArg pathingArg, bool bReuse, int* piPathTurns)
{
	return generatePathFromTo(record, pathFinder, plot(), pToPlot, pathingArg, bReuse, piPathTurns);
}

int DryRunGroupState::getX() const
{
	return plot()->getX();
}

int DryRunGroupState::getY() const
{
	return plot()->getY();
}

// Returns true if attack was made...
/// bool CvSelectionGroup::groupAttack(int iX, int iY, int iFlags, bool& bFailedAlreadyFighting)
bool DryRunGroupState::groupAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags, bool& bFailedAlreadyFighting)
{
	const CvPlot* pDestPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	if (iFlags & MOVE_THROUGH_ENEMY)
	{
		if (generatePath(record, pathFinder, pDestPlot, iFlags))
		{
			pDestPlot = pathFinder.getNextStepPlot();
		}
	}

	//if (liveGroup->getID() == 49111846)
	//	__debugbreak();

	FAssertMsg(pDestPlot != nullptr, "DestPlot is not assigned a valid value");

	//bool bStack = false; //(group.isHuman() && ((getDomainType() == DOMAIN_AIR) || GET_PLAYER(getOwner()).isOption(PLAYEROPTION_STACK_ATTACK)));

	bool bAttack = false;
	bFailedAlreadyFighting = false;

	if (getNumUnits() > 0)
	{
		if ((getDomainType() == DOMAIN_AIR) || (stepDistance(getX(), getY(), pDestPlot->getX_INLINE(), pDestPlot->getY_INLINE()) == 1))
		{
			if ((iFlags & MOVE_DIRECT_ATTACK) || (getDomainType() == DOMAIN_AIR) || (iFlags & MOVE_THROUGH_ENEMY)
				|| (generatePath(record, pathFinder, pDestPlot, PathingArg(iFlags, 2)) && (pathFinder.getNextStepPlot() == pDestPlot)))
			{
				int iAttackOdds{};
				DryRunUnitState* const pBestAttackUnit = AI_getBestGroupAttacker(record, pDestPlot, true, iAttackOdds);
		
				if (pBestAttackUnit)
				{
					// if there are no defenders, do not attack
					if (!pDestPlot->getBestDefender(NO_PLAYER, getOwner(), &pBestAttackUnit->liveUnit, true))
					{
						return false;
					}
						
					// Not implemented for parallel unit update.
					record.setUseSerialFallback(std::string(__func__) + " Attacking not implemented");
				}
			}
		}
	}

	// Instead of all that, just check whether any attack is possible.
	//for (const DryRunUnitState& unit : units)
	//{
	//	// Condition from AI_getBestGroupAttacker.
	//	constexpr bool bNoBlitz = false;
	//	bool bCanAttack = false;
	//	if (unit.getDomainType() == DOMAIN_AIR)
	//	{
	//		bCanAttack = unit.liveUnit.canAirAttack();
	//	}
	//	else
	//	{
	//		bCanAttack = unit.liveUnit.canAttack();
	//
	//		if (bCanAttack && bNoBlitz && unit.liveUnit.isBlitz() && unit.liveUnit.isMadeAttack())
	//		{
	//			bCanAttack = false;
	//		}
	//	}
	//
	//	if (bCanAttack && record.isVisibleEnemyUnit(pDestPlot, unit.liveUnit))
	//	{
	//		// Not implemented for parallel unit update.
	//		record.setUseSerialFallback(std::string(__func__) + " Attacking not implemented");
	//	}
	//}

	return bAttack;
}

/// bool CvSelectionGroup::canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const
bool DryRunGroupState::canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const
{
	return liveGroup->canEnterArea(eTeam, pArea, bIgnoreRightOfPassage);
}

// Returns true if group was bumped...
/// bool CvSelectionGroup::groupDeclareWar(CvPlot* pPlot, bool bForce)
bool DryRunGroupState::groupDeclareWar(GroupUpdateRecord& record, const CvPlot* pPlot, bool bForce) const
{
	CvTeamAI& kTeam = GET_TEAM(getTeam());
	TeamTypes ePlotTeam = pPlot->getTeam();

	if (!liveGroup->AI_isDeclareWar(pPlot))
	{
		return false;
	}

	int iNumUnits = getNumUnits();

	if (bForce || !canEnterArea(ePlotTeam, pPlot->area(), true))
	{
		if (ePlotTeam != NO_TEAM && kTeam.AI_isSneakAttackReady(ePlotTeam))
		{
			// There's a minor dependency on isHasMet here. But if we say that team meets happen after all moves are done, then that's fine. 
			if (kTeam.canDeclareWar(ePlotTeam))
			{
				record.setUseSerialFallback(std::string(__func__) + " Sneak attack declare war");
				//kTeam.declareWar(ePlotTeam, true, NO_WARPLAN);
			}
		}
	}

	return (iNumUnits != getNumUnits());
}

/// bool CvSelectionGroup::isAmphibPlot(const CvPlot* pPlot) const
bool DryRunGroupState::isAmphibPlot(const CvPlot* pPlot) const
{
	return liveGroup->isAmphibPlot(pPlot);
}

// Returns true if attempted an amphib landing...
/// bool CvSelectionGroup::groupAmphibMove(CvPlot* pPlot, int iFlags)
bool DryRunGroupState::groupAmphibMove(GroupUpdateRecord& record, const CvPlot* pPlot, [[maybe_unused]] int iFlags)
{
	bool bLanding = false;

	FAssert(getOwner() != NO_PLAYER);

	if (groupDeclareWar(record, pPlot))
		return true;

	if (isAmphibPlot(pPlot))
		record.setUseSerialFallback(std::string(__func__) + " isAmphibPlot");

	return bLanding;
}

/// void CvSelectionGroup::groupMove(CvPlot* pPlot, bool bCombat, CvUnit* pCombatUnit, bool bEndMove)
void DryRunGroupState::groupMove(GroupUpdateRecord& record, [[maybe_unused]] IPathFinder& pathFinder, const CvPlot* pPlot, bool bCombat, const CvUnit* pCombatUnit, [[maybe_unused]] bool bEndMove)
{
	for (DryRunUnitState& unit : units)
	{
		const bool unitCanMove = unit.canMove();
		const bool isEnemyCity = pPlot->isEnemyCity(unit.liveUnit);
		const bool unitCanMoveOrAttackInto = unit.canMoveOrAttackInto(record, *this, pPlot);
		const bool unitCanMoveInto = unit.canMoveInto(record, *this, pPlot);
		if ((unitCanMove && (bCombat && (!unit.isNoCapture() || !isEnemyCity) ? unitCanMoveOrAttackInto : unitCanMoveInto))
			|| &unit.liveUnit == pCombatUnit)
		{
			unit.move(record, *this, pPlot, true);
		}
		else
		{
			// Group ops. Not implemented.
			record.setUseSerialFallback(std::string(__func__) + " splitting the group");
			//pLoopUnit->joinGroup(nullptr, true);
			//pLoopUnit->ExecuteMove(((float)(GC.getMissionInfo(MISSION_MOVE_TO).getTime() * gDLL->getMillisecsPerTurn())) / 1000.0f, false);
			// Not implemented.
		}
	}

	//execute move
	// Entity stuff. Not needed.
	//if (bEndMove || !canAllMove())
	//{
	//	pUnitNode = group.headUnitNode();
	//	while (pUnitNode)
	//	{
	//		const CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
	//		pUnitNode = group.nextUnitNode(pUnitNode);
	//
	//		pLoopUnit->ExecuteMove(((float)(GC.getMissionInfo(MISSION_MOVE_TO).getTime() * gDLL->getMillisecsPerTurn())) / 1000.0f, false);
	//	}
	//}
}

/// bool CvSelectionGroup::groupPathTo(int iX, int iY, int iFlags)
bool DryRunGroupState::groupPathTo(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags)
{
	const CvPlot* pDestPlot;
	const CvPlot* pPathPlot;

	if (at(iX, iY))
	{
		return false; // XXX is this necessary?
	}

	FAssert(!isBusy());
	FAssert(getOwner() != NO_PLAYER);
	FAssert(headMissionQueueNode() != nullptr);

	pDestPlot = GC.getMapINLINE().plotINLINE(iX, iY);
	FAssertMsg(pDestPlot != nullptr, "DestPlot is not assigned a valid value");

	FAssertMsg(canAllMove(), "canAllMove is expected to be true");

	if (getDomainType() == DOMAIN_AIR)
	{
		//if (!canMoveInto(pDestPlot))
		//{
		//	return false;
		//}

		// Not implemented.
		record.setUseSerialFallback(std::string(__func__) + " getDomainType() == DOMAIN_AIR");

		pPathPlot = pDestPlot;
	}
	else
	{
		if (!generatePath(record, pathFinder, pDestPlot, iFlags))
		{
			return false;
		}

		pPathPlot = pathFinder.getNextStepPlot();

		if (groupAmphibMove(record, pPathPlot, iFlags))
		{
			return false;
		}
	}

	bool bForce = false;
	MissionAITypes eMissionAI = AI_getMissionAIType();
	if (eMissionAI == MISSIONAI_BLOCKADE || eMissionAI == MISSIONAI_PILLAGE)
	{
		bForce = true;
	}

	if (groupDeclareWar(record, pPathPlot, bForce))
	{
		return false;
	}

	bool bEndMove = false;
	if (pPathPlot == pDestPlot)
		bEndMove = true;

	groupMove(record, pathFinder, pPathPlot, iFlags & MOVE_THROUGH_ENEMY, nullptr, bEndMove);

	return true;
}

bool DryRunGroupState::groupRoadTo(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags)
{
	[[maybe_unused]] RouteTypes eBestRoute;
	BuildTypes eBestBuild;

	if (!liveGroup->AI_isControlled() || !at(iX, iY) || (getLengthMissionQueue() == 1))
	{
		const CvPlot* const pPlot = plot();

		eBestRoute = getBestBuildRoute(pPlot, &eBestBuild);

		if (eBestBuild != NO_BUILD)
		{
			groupBuild(record, eBestBuild);
			return true;
		}
	}

	return groupPathTo(record, pathFinder, iX, iY, iFlags);
}

bool DryRunGroupState::groupBuild(GroupUpdateRecord& record, BuildTypes eBuild)
{
	bool bContinue;

	FAssert(getOwner() != NO_PLAYER);
	FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

	bContinue = false;

	const CvPlot* const pPlot = plot();

	ImprovementTypes eImprovement = (ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement();
	if (eImprovement != NO_IMPROVEMENT)
	{
		if (liveGroup->AI_isControlled())
		{
			if (GET_PLAYER(getOwner()).isOption(PLAYEROPTION_SAFE_AUTOMATION))
			{
				if ((pPlot->getImprovementType() != NO_IMPROVEMENT) && (pPlot->getImprovementType() != (ImprovementTypes)(GC.getDefineINT("RUINS_IMPROVEMENT"))))
				{
					BonusTypes eBonus = (BonusTypes)pPlot->getNonObsoleteBonusType(GET_PLAYER(getOwner()).getTeam());
					if ((eBonus == NO_BONUS) || !GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eBonus))
					{
						if (GC.getImprovementInfo(eImprovement).getImprovementPillage() != NO_IMPROVEMENT)
						{
							return false;
						}
					}
				}
			}
		}
	}

	for (const DryRunUnitState& unit : units)
	{
		FAssertMsg(unit.atPlot(pPlot), "pLoopUnit is expected to be at pPlot");

		if (unit.canBuild(pPlot, eBuild))
		{
			bContinue = true;

			if (unit.build(record, eBuild))
			{
				bContinue = false;
				break;
			}
		}
	}

	return bContinue;
}

/// void CvSelectionGroup::continueMission(int iSteps)
void DryRunGroupState::continueMission(GroupUpdateRecord& record, IPathFinder& pathFinder, int iSteps)
{
	CvUnit* pTargetUnit;
	bool bDone;
	bool bAction;

	FAssert(!isBusy());
	FAssert(headMissionQueueNode() != nullptr);
	FAssert(getOwner() != NO_PLAYER);
	FAssert(activityType == ACTIVITY_MISSION);

	if (headMissionQueueNode() == nullptr)
	{
		// just in case...
		setActivityType(ACTIVITY_AWAKE, record);
		return;
	}

	bDone = false;
	bAction = false;

	if (headMissionQueueNode()->iPushTurn == GC.getGameINLINE().getGameTurn() || (headMissionQueueNode()->iFlags & MOVE_THROUGH_ENEMY))
	{
		if (headMissionQueueNode()->eMissionType == MISSION_MOVE_TO)
		{
			bool bFailedAlreadyFighting{};
			if (groupAttack(record, pathFinder, headMissionQueueNode()->iData1, headMissionQueueNode()->iData2, headMissionQueueNode()->iFlags, bFailedAlreadyFighting))
			{
				bDone = true;
			}
		}
	}

	// extra crash protection, should never happen (but a previous bug in groupAttack was causing a nullptr here)
	// while that bug is fixed, no reason to not be a little more careful
	if (headMissionQueueNode() == nullptr)
	{
		setActivityType(ACTIVITY_AWAKE, record);
		return;
	}

	if (!bDone)
	{
		if (getNumUnits() > 0)
		{
			if (canAllMove())
			{
				switch (headMissionQueueNode()->eMissionType)
				{
				case MISSION_MOVE_TO:
					if (getDomainType() == DOMAIN_AIR)
					{
						groupPathTo(record, pathFinder, headMissionQueueNode()->iData1, headMissionQueueNode()->iData2, headMissionQueueNode()->iFlags);
						bDone = true;
					}
					else if (groupPathTo(record, pathFinder, headMissionQueueNode()->iData1, headMissionQueueNode()->iData2, headMissionQueueNode()->iFlags))
					{
						bAction = true;

						if (getNumUnits() > 0)
						{
							if (!canAllMove())
							{
								if (headMissionQueueNode() != nullptr)
								{
									if (groupAmphibMove(record, GC.getMapINLINE().plotINLINE(headMissionQueueNode()->iData1, headMissionQueueNode()->iData2), headMissionQueueNode()->iFlags))
									{
										bAction = false;
										bDone = true;
									}
								}
							}
						}
					}
					else
					{
						bDone = true;
					}
					break;

				case MISSION_ROUTE_TO:
					if (groupRoadTo(record, pathFinder, headMissionQueueNode()->iData1, headMissionQueueNode()->iData2, headMissionQueueNode()->iFlags))
					{
						bAction = true;
					}
					else
					{
						bDone = true;
					}
					break;

				case MISSION_MOVE_TO_UNIT:
					if ((getHeadUnitAI() == UNITAI_CITY_DEFENSE) && plot()->isCity() && (plot()->getTeam() == getTeam()))
					{
						record.addDependencyOnOurUnitMovementAt(plot());
						if (plot()->getBestDefender(getOwner())->getGroup() == liveGroup)
						{
							bAction = false;
							bDone = true;
							break;
						}
					}
					pTargetUnit = GET_PLAYER((PlayerTypes)headMissionQueueNode()->iData1).getUnit(headMissionQueueNode()->iData2);
					if (pTargetUnit != nullptr)
					{
						if (AI_getMissionAIType() != MISSIONAI_SHADOW && AI_getMissionAIType() != MISSIONAI_GROUP)
						{
							if (!plot()->isOwned() || plot()->getOwner() == getOwner())
							{
								if (pTargetUnit->getOwner() == getOwner())
								{
									record.setUseSerialFallback(std::string(__func__) + " If it's our own unit, then we depend on AI_getMissionAIPlot");
								}

								CvPlot* pMissionPlot = pTargetUnit->getGroup()->AI_getMissionAIPlot();
								if (pMissionPlot != nullptr && NO_TEAM != pMissionPlot->getTeam())
								{
									if (pMissionPlot->isOwned() && pTargetUnit->isPotentialEnemy(pMissionPlot->getTeam(), pMissionPlot))
									{
										bAction = false;
										bDone = true;
										break;
									}
								}
							}
						}

						if (groupPathTo(record, pathFinder, pTargetUnit->getX_INLINE(), pTargetUnit->getY_INLINE(), headMissionQueueNode()->iFlags))
						{
							bAction = true;
						}
						else
						{
							bDone = true;
						}
					}
					else
					{
						bDone = true;
					}
					break;

				case MISSION_SKIP:
				case MISSION_SLEEP:
				case MISSION_FORTIFY:
				case MISSION_PLUNDER:
				case MISSION_AIRPATROL:
				case MISSION_SEAPATROL:
				case MISSION_HEAL:
				case MISSION_SENTRY:
					FAssert(false);
					break;

				case MISSION_AIRLIFT:
				case MISSION_NUKE:
				case MISSION_RECON:
				case MISSION_PARADROP:
				case MISSION_AIRBOMB:
				case MISSION_BOMBARD:
				case MISSION_RANGE_ATTACK:
				case MISSION_PILLAGE:
				case MISSION_SABOTAGE:
				case MISSION_DESTROY:
				case MISSION_STEAL_PLANS:
				case MISSION_FOUND:
				case MISSION_SPREAD:
				case MISSION_SPREAD_CORPORATION:
				case MISSION_JOIN:
				case MISSION_CONSTRUCT:
				case MISSION_DISCOVER:
				case MISSION_HURRY:
				case MISSION_TRADE:
				case MISSION_GREAT_WORK:
				case MISSION_INFILTRATE:
				case MISSION_GOLDEN_AGE:
				case MISSION_LEAD:
				case MISSION_ESPIONAGE:
				case MISSION_DIE_ANIMATION:
					break;

				case MISSION_BUILD:
					if (!groupBuild(record, (BuildTypes)(headMissionQueueNode()->iData1)))
					{
						bDone = true;
					}
					break;

				default:
					FAssert(false);
					break;
				}
			}
		}
	}

	if ((getNumUnits() > 0) && (headMissionQueueNode() != nullptr))
	{
		if (!bDone)
		{
			switch (headMissionQueueNode()->eMissionType)
			{
			case MISSION_MOVE_TO:
				if (at(headMissionQueueNode()->iData1, headMissionQueueNode()->iData2))
				{
					bDone = true;
				}
				break;

			case MISSION_ROUTE_TO:
				if (at(headMissionQueueNode()->iData1, headMissionQueueNode()->iData2))
				{
					if (getBestBuildRoute(plot()) == NO_ROUTE)
					{
						bDone = true;
					}
				}
				break;

			case MISSION_MOVE_TO_UNIT:
				pTargetUnit = GET_PLAYER((PlayerTypes)headMissionQueueNode()->iData1).getUnit(headMissionQueueNode()->iData2);
				if ((pTargetUnit == nullptr) || atPlot(pTargetUnit->plot()))
				{
					bDone = true;
				}
				break;

			case MISSION_SKIP:
			case MISSION_SLEEP:
			case MISSION_FORTIFY:
			case MISSION_PLUNDER:
			case MISSION_AIRPATROL:
			case MISSION_SEAPATROL:
			case MISSION_HEAL:
			case MISSION_SENTRY:
				FAssert(false);
				break;

			case MISSION_AIRLIFT:
			case MISSION_NUKE:
			case MISSION_RECON:
			case MISSION_PARADROP:
			case MISSION_AIRBOMB:
			case MISSION_BOMBARD:
			case MISSION_RANGE_ATTACK:
			case MISSION_PILLAGE:
			case MISSION_SABOTAGE:
			case MISSION_DESTROY:
			case MISSION_STEAL_PLANS:
			case MISSION_FOUND:
			case MISSION_SPREAD:
			case MISSION_SPREAD_CORPORATION:
			case MISSION_JOIN:
			case MISSION_CONSTRUCT:
			case MISSION_DISCOVER:
			case MISSION_HURRY:
			case MISSION_TRADE:
			case MISSION_GREAT_WORK:
			case MISSION_INFILTRATE:
			case MISSION_GOLDEN_AGE:
			case MISSION_LEAD:
			case MISSION_ESPIONAGE:
			case MISSION_DIE_ANIMATION:
				bDone = true;
				break;

			case MISSION_BUILD:
				// XXX what happens if two separate worker groups are both building the mine...
				/*if (plot()->getBuildType() != ((BuildTypes)(headMissionQueueNode()->m_data.iData1)))
				{
					bDone = true;
				}*/
				break;

			default:
				FAssert(false);
				break;
			}
		}
	}

	if ((getNumUnits() > 0) && (headMissionQueueNode() != nullptr))
	{
		if (bAction)
		{
			if (bDone || !canAllMove())
			{
				if (plot()->isVisibleToWatchingHuman())
				{
					updateMissionTimer(iSteps);
			
					// Yeah, we can't really show moves if they happen in parallel...
					//if (showMoves())
					//{
					//	if (GC.getGameINLINE().getActivePlayer() != NO_PLAYER)
					//	{
					//		if (getOwner() != GC.getGameINLINE().getActivePlayer())
					//		{
					//			if (plot()->isActiveVisible(false) && !isInvisible(GC.getGameINLINE().getActiveTeam()))
					//			{
					//				gDLL->getInterfaceIFace()->lookAt(plot()->getPoint(), CAMERALOOKAT_NORMAL);
					//			}
					//		}
					//	}
					//}
				}
			}
		}

		if (bDone)
		{
			if (!isBusy())
			{
				//if (getOwner() == GC.getGameINLINE().getActivePlayer())
				//{
				//	if (IsSelected())
				//	{
				//		if ((headMissionQueueNode()->m_data.eMissionType == MISSION_MOVE_TO) ||
				//			(headMissionQueueNode()->m_data.eMissionType == MISSION_ROUTE_TO) ||
				//			(headMissionQueueNode()->m_data.eMissionType == MISSION_MOVE_TO_UNIT))
				//		{
				//			gDLL->getInterfaceIFace()->changeCycleSelectionCounter((GET_PLAYER(getOwner()).isOption(PLAYEROPTION_QUICK_MOVES)) ? 1 : 2);
				//		}
				//	}
				//}

				deleteHeadMissionQueueNode(record, pathFinder);
			}
		}
		else
		{
			if (canAllMove())
			{
				// Stop recursion early.
				if (record.getSerialFallbackReason())
					return;
				continueMission(record, pathFinder, iSteps + 1);
			}
			else if (!isBusy())
			{
				//if (getOwner() == GC.getGameINLINE().getActivePlayer())
				//{
				//	if (IsSelected())
				//	{
				//		gDLL->getInterfaceIFace()->changeCycleSelectionCounter(1);
				//	}
				//}
			}
		}
	}
}

void DryRunGroupState::updateMissionTimer([[maybe_unused]] int iSteps)
{
	//CvUnit* pTargetUnit;
	//CvPlot* pTargetPlot;
	int iTime;

	// We are always AI, and we never show parallel moves.
	//if (!isHuman() && !showMoves())
	{
		iTime = 0;
	}
	//else if (hasHeadMissionQueueNode())
	//{
	//	iTime = GC.getMissionInfo((MissionTypes)(headMissionQueueNode()->m_data.eMissionType)).getTime();
	//
	//	if ((headMissionQueueNode()->m_data.eMissionType == MISSION_MOVE_TO) ||
	//		(headMissionQueueNode()->m_data.eMissionType == MISSION_ROUTE_TO) ||
	//		(headMissionQueueNode()->m_data.eMissionType == MISSION_MOVE_TO_UNIT))
	//	{
	//		if (headMissionQueueNode()->m_data.eMissionType == MISSION_MOVE_TO_UNIT)
	//		{
	//			pTargetUnit = GET_PLAYER((PlayerTypes)headMissionQueueNode()->m_data.iData1).getUnit(headMissionQueueNode()->m_data.iData2);
	//			if (pTargetUnit != nullptr)
	//			{
	//				pTargetPlot = pTargetUnit->plot();
	//			}
	//			else
	//			{
	//				pTargetPlot = nullptr;
	//			}
	//		}
	//		else
	//		{
	//			pTargetPlot = GC.getMapINLINE().plotINLINE(headMissionQueueNode()->m_data.iData1, headMissionQueueNode()->m_data.iData2);
	//		}
	//
	//		if (atPlot(pTargetPlot))
	//		{
	//			iTime += iSteps;
	//		}
	//		else
	//		{
	//			iTime = std::min(iTime, 2);
	//		}
	//	}
	//
	//	if (isHuman() && (isAutomated() || (GET_PLAYER((GC.getGameINLINE().isNetworkMultiPlayer()) ? getOwnerINLINE() : GC.getGameINLINE().getActivePlayer()).isOption(PLAYEROPTION_QUICK_MOVES))))
	//	{
	//		iTime = std::min(iTime, 1);
	//	}
	//}
	//else
	//{
	//	iTime = 0;
	//}

	setMissionTimer(iTime);
}

/// inline void CvSelectionGroupAI::AI_cancelGroupAttack()
void DryRunGroupState::AI_cancelGroupAttack()
{
	m_bGroupAttack = false;
}

/// inline bool CvSelectionGroupAI::AI_isGroupAttack() const
bool DryRunGroupState::AI_isGroupAttack() const
{
	return m_bGroupAttack;
}

/// CLLNode<MissionData>* CvSelectionGroup::deleteMissionQueueNode(CLLNode<MissionData>* pNode)
void DryRunGroupState::deleteHeadMissionQueueNode(GroupUpdateRecord& record, IPathFinder& pathFinder)
{
	FAssert(getOwner() != NO_PLAYER);

	//if (pNode == headMissionQueueNode())
	{
		deactivateHeadMission(record);
	}

	//pNextMissionNode = m_missionQueue.deleteNode(pNode);
	m_missionQueue.erase(m_missionQueue.begin());

	//if (pNextMissionNode == headMissionQueueNode())
	{
		activateHeadMission(pathFinder, record);
	}

	//if ((getOwner() == GC.getGameINLINE().getActivePlayer()) && IsSelected())
	//{
	//	gDLL->getInterfaceIFace()->setDirty(Waypoints_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(SelectionButtons_DIRTY_BIT, true);
	//	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
	//}

	//return pNextMissionNode;
}

/// void CvSelectionGroup::deactivateHeadMission()
void DryRunGroupState::deactivateHeadMission(GroupUpdateRecord& record)
{
	FAssert(getOwner() != NO_PLAYER);

	if (headMissionQueueNode() != nullptr)
	{
		if (activityType == ACTIVITY_MISSION)
		{
			setActivityType(ACTIVITY_AWAKE, record);
		}

		setMissionTimer(0);

		//if (getOwner() == GC.getGameINLINE().getActivePlayer())
		//{
		//	if (IsSelected())
		//	{
		//		gDLL->getInterfaceIFace()->changeCycleSelectionCounter(1);
		//	}
		//}
	}
}

std::vector<MissionData> DryRunGroupState::buildMissionQueue() const
{
	std::vector<MissionData> missions;
	missions.reserve(liveGroup->getLengthMissionQueue());
	for (auto* node = liveGroup->headMissionQueueNode(); node; node = liveGroup->nextMissionQueueNode(node))
		missions.push_back(node->m_data);
	return missions;
}

void DryRunGroupState::pushPathingMission(GroupUpdateRecord& record, IPathFinder& pathFinder, MissionTypes missionType, ivec2 missionDataCoord, unsigned int flags,
	bool bAppend, bool bManual, MissionAITypes eMissionAI, const CvPlot* pMissionAIPlot, const CvUnit* pMissionAIUnit)
{
	return pushGenericMission(record, pathFinder, missionType, missionDataCoord.x, missionDataCoord.y, flags, bAppend, bManual, eMissionAI, pMissionAIPlot, pMissionAIUnit);
}

void DryRunGroupState::pushGenericMission(GroupUpdateRecord& record, IPathFinder& pathFinder, MissionTypes missionType, int iData1, int iData2, unsigned int flags,
	bool bAppend, [[maybe_unused]] bool bManual, MissionAITypes eMissionAI, const CvPlot* pMissionAIPlot, const CvUnit* pMissionAIUnit)
{
	MissionData mission{};

	FAssert(getOwner() != NO_PLAYER);

	// Should be AI only.
	assert(!bManual);

	if (!bAppend)
	{
		if (isBusy())
		{
			record.setUseSerialFallback(std::string(__func__) + " Busy");
		}

		clearMissionQueue(record);
	}

	//if (bManual)
	//{
	//	setAutomateType(NO_AUTOMATE);
	//}

	mission.eMissionType = missionType;
	mission.iData1 = iData1;
	mission.iData2 = iData2;
	mission.iFlags = flags;
	mission.iPushTurn = GC.getGameINLINE().getGameTurn();

	AI_setMissionAI(record, eMissionAI, pMissionAIPlot, pMissionAIUnit);

	insertAtEndMissionQueue(mission, !bAppend, record, pathFinder);

	//if (bManual)
	//{
	//	if (getOwner() == GC.getGameINLINE().getActivePlayer())
	//	{
	//		if (isBusy() && GC.getMissionInfo(eMission).isSound())
	//		{
	//			playActionSound();
	//		}
	//
	//		gDLL->getInterfaceIFace()->setHasMovedUnit(true);
	//	}
	//
	//	CvEventReporter::getInstance().selectionGroupPushMission(this, eMission);
	//
	//	doDelayedDeath();
	//}
}

void DryRunGroupState::pushSkipMission(GroupUpdateRecord& record, IPathFinder& pathFinder)
{
	return pushGenericMission(record, pathFinder, MISSION_SKIP);
}

void GroupDryRunResult::setUseSerialFallback(const char* reason)
{
	groupUpdateRecord.setUseSerialFallback(reason);
}

const char* GroupDryRunResult::getSerialFallbackReason() const
{
	return groupUpdateRecord.getSerialFallbackReason();
}
	
GroupDryRunResult GiganticMapsOptimisationsLib::dryrun_CvSelectionGroupAI_AI_update(const CvSelectionGroupAI& liveGroup, IPathFinder& pathFinder, uint32_t updateProcessingSeed)
{
	FAssert(liveGroup.getOwner() != NO_PLAYER);

	GroupDryRunResult dryRunResult{
		.dryRunGroupState = DryRunGroupState{ liveGroup },
		.groupUpdateRecord = GroupUpdateRecord{ liveGroup.getOwner(), liveGroup.getID(), updateProcessingSeed },
	};

	DryRunGroupState& group = dryRunResult.dryRunGroupState;

	if (!liveGroup.AI_isControlled())
	{
		dryRunResult.returnValue = false;
		return dryRunResult;
	}

	if (group.getNumUnits() == 0)
	{
		dryRunResult.returnValue = false;
		return dryRunResult;
	}

	// Cargo not implemented.
	// Being cargo not implemented.
	if (liveGroup.getCargo() > 0 || anyUnit(group, &DryRunUnitState::isCargo))
	{
		dryRunResult.returnValue = false;
		return dryRunResult;
	}

	if (group.isForceUpdate()) // Almost always true.
	{
		group.clearMissionQueue(dryRunResult.groupUpdateRecord); // XXX ???
		group.setActivityType(ACTIVITY_AWAKE, dryRunResult.groupUpdateRecord);
		group.setForceUpdate(false);

		// if we are in the middle of attacking with a stack, cancel it
		group.AI_cancelGroupAttack();
	}

	for (const auto& unit : group.units)
	{
		if (unit.liveUnit.getNumSeeInvisibleTypes() != 0)
		{
			// TODO: Movement of this unit invalidates invisibility sight, which is not implemented.
			dryRunResult.setUseSerialFallback("unit.liveUnit.getNumSeeInvisibleTypes() != 0");
			return dryRunResult;
		}
	}

	FAssert(!(GET_PLAYER(liveGroup.getOwner()).isAutoMoves()));

	bool bDead;
	bool bFollow;
	int iTempHack = 0; // XXX

	bool isGroupAttack = group.AI_isGroupAttack();

	if (isGroupAttack)
	{
		dryRunResult.setUseSerialFallback("isGroupAttack");
		return dryRunResult;
	}

	// Eliminate this dependency from group.readyToMove/isCargoBusy.
	//if (hasAnyUnitCanCarryCargo(group))
	//{
	//	dryRunrecord.useSerialFallback = true;
	//	return dryRunResult;
	//}

	bDead = false;

	bool bFailedAlreadyFighting = false;
	while ((isGroupAttack && !bFailedAlreadyFighting) || group.readyToMove())
	{
		// Early out.
		if (dryRunResult.getSerialFallbackReason())
		{
			dryRunResult.returnValue = false;
			return dryRunResult;
		}

		iTempHack++;
		if (iTempHack > 100)
		{
			dryRunResult.setUseSerialFallback("iTempHack > 100");
			return dryRunResult;
		}

		// if we want to force the group to attack, force another attack
		//if (isGroupAttack)
		//{
		//	isGroupAttack = false;
		//
		//	groupAttack(m_iGroupAttackX, m_iGroupAttackY, MOVE_DIRECT_ATTACK, bFailedAlreadyFighting);
		//}
		// else pick AI action
		//else
		{
			DryRunUnitState* const pHeadUnit = group.getHeadUnit();

			if (pHeadUnit == nullptr || pHeadUnit->isDelayedDeath())
			{
				dryRunResult.setUseSerialFallback("pHeadUnit == nullptr || pHeadUnit->isDelayedDeath()");
				return dryRunResult;
			}

			if (pHeadUnit->AI_update(dryRunResult.groupUpdateRecord, group, pathFinder))
			{
				// AI_update returns true when we should abort the loop and wait until next slice
				break;
			}
		}

		// group.doDelayedDeath
		if (anyUnit(group, &DryRunUnitState::isDelayedDeath) || group.getNumUnits() == 0)
		{
			dryRunResult.setUseSerialFallback("anyUnit(group, &DryRunUnitState::isDelayedDeath) || group.getNumUnits() == 0");
			return dryRunResult;
		}

		// if no longer group attacking, and force separate is true, then bail, decide what to do after group is split up
		// (UnitAI of head unit may have changed)
		if (!isGroupAttack && liveGroup.AI_isForceSeparate())
		{
			dryRunResult.setUseSerialFallback("!isGroupAttack && liveGroup.AI_isForceSeparate()");
			return dryRunResult;
		}
	}

	if (!bDead)
	{
		if (!liveGroup.isHuman())
		{
			bFollow = false;

			// if we not group attacking, then check for follow action
			if (!isGroupAttack)
			{
				for (DryRunUnitState& unit : group.units)
				{
					if (group.readyToMove(true))
					{
						if (unit.canMove())
						{
							if (unit.AI_follow(dryRunResult.groupUpdateRecord, pathFinder, group))
							{
								bFollow = true;
								break;
							}
						}
					}
					else
						break;
				}
			}

			//if (doDelayedDeath())
			if (anyUnit(group, &DryRunUnitState::isDelayedDeath) || group.getNumUnits() == 0)
				bDead = true;

			if (!bDead)
			{
				if (!bFollow && group.readyToMove(true))
				{
					group.pushSkipMission(dryRunResult.groupUpdateRecord, pathFinder);
				}
			}
		}
	}

	if (bDead)
	{
		dryRunResult.returnValue = true;
		return dryRunResult;
	}

	if (dryRunResult.groupUpdateRecord.getSerialFallbackReason())
		dryRunResult.setUseSerialFallback("GroupUpdateRecord");
	dryRunResult.returnValue = group.isBusy() || group.isCargoBusy();
	return dryRunResult;
}

bool DryRunGroupState::has_AI_ejectBestDefender(const CvPlot*) const
{
	CLLNode<IDInfo>* pEntityNode;
	CvUnit* pLoopUnit;

	pEntityNode = liveGroup->headUnitNode();

	while (pEntityNode != nullptr)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		pEntityNode = liveGroup->nextUnitNode(pEntityNode);

		if (!pLoopUnit->noDefensiveBonus())
			return true;
	}

	return false;
}

#endif