#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_GroupUpdateRecord.h"
#include "AIParallelUnitUpdate_DryRunUnitState.h"
#include "AIParallelUnitUpdate_DryRunGroupState.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>
#include <syncstream>

using namespace GiganticMapsOptimisationsLib;

// Returns true if a mission was pushed...
bool DryRunUnitState::AI_afterAttack(GroupUpdateRecord& record)
{
	if (!isMadeAttack())
	{
		return false;
	}

	if (!canFight())
	{
		return false;
	}

	record.setUseSerialFallback(std::string(__func__) + " Bunch of stuff not implemented");

	return false;
}

void DryRunUnitState::AI_animalMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	if (record.getRand(100, "Animal Attack") < GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAnimalAttackProb())
		if (AI_anyAttack(record, pathFinder, group, 1, 0))
			return;

	if (AI_heal(record, pathFinder, group))
		return;

	if (AI_patrol(record, pathFinder, group))
		return;

	group.pushSkipMission(record, pathFinder);
}


// Returns true if a mission was pushed...
bool DryRunUnitState::AI_anyAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iOddsThreshold, int iMinStack, bool bFollow)
{
	const CvPlot* pLoopPlot;
	const CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	FAssert(canMove());

	if (AI_rangeAttack(record, pathFinder, group, iRange))
	{
		return true;
	}

	if (bFollow)
	{
		iSearchRange = 1;
	}
	else
	{
		iSearchRange = AI_searchRange(iRange);
	}

	//getGroup()->resetPath();

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (record.isVisibleEnemyUnit(pLoopPlot, liveUnit) || (pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getTeam())))
					{
						if (record.getNumVisibleEnemyDefenders(pLoopPlot, liveUnit) >= iMinStack)
						{
							if (!atPlot(pLoopPlot) && ((bFollow) ? canMoveInto(record, group, pLoopPlot, true) : (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange))))
							{
								iValue = group.AI_attackOdds(pLoopPlot, true);

								if (iValue >= AI_finalOddsThreshold(record, pLoopPlot, iOddsThreshold))
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = ((bFollow) ? pLoopPlot : pathFinder.getPathEndTurnPlot());
										FAssert(!atPlot(pBestPlot));
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot), ((bFollow) ? MOVE_DIRECT_ATTACK : 0));
		return true;
	}

	return false;
}

void DryRunUnitState::AI_attackCityLemmingMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	if (AI_cityAttack(record, pathFinder, group, 1, 80))
	{
		return;
	}

	if (AI_bombardCity(record))
	{
		return;
	}

	if (AI_cityAttack(record, pathFinder, group, 1, 40))
	{
		return;
	}

	if (AI_targetCity(record, pathFinder, group, MOVE_THROUGH_ENEMY))
	{
		return;
	}

	if (AI_anyAttack(record, pathFinder, group, 1, 70))
	{
		return;
	}

	if (AI_anyAttack(record, pathFinder, group, 1, 0))
	{
		return;
	}

	group.pushSkipMission(record, pathFinder);
}

void DryRunUnitState::AI_barbAttackMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	if (AI_guardCity(record, pathFinder, group, false, true, 1))
	{
		return;
	}

	if (plot()->isGoody())
	{
		//if (plot()->plotCount(PUF_isUnitAIType, UNITAI_ATTACK, -1, getOwner()) == 1)
		//{
		//	group.pushSkipMission(record, pathFinder);
		//	return;
		//}
		record.setUseSerialFallback(std::string(__func__) + " isGoody");
		return;
	}

	if (record.getRand(2, "AI Barb") == 0)
	{
		if (AI_pillageRange(record, pathFinder, group, 1))
		{
			return;
		}
	}

	if (AI_anyAttack(record, pathFinder, group, 1, 20))
	{
		return;
	}

	if (GC.getGameINLINE().isOption(GAMEOPTION_RAGING_BARBARIANS))
	{
		if (AI_pillageRange(record, pathFinder, group, 4))
		{
			return;
		}

		if (AI_cityAttack(record, pathFinder, group, 3, 10))
		{
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
		{
			if (AI_targetCity(record, pathFinder, group))
			{
				return;
			}
		}
	}
	else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 3))
	{
		if (AI_cityAttack(record, pathFinder, group, 1, 15))
		{
			return;
		}

		if (AI_pillageRange(record, pathFinder, group, 3))
		{
			return;
		}

		if (AI_cityAttack(record, pathFinder, group, 2, 10))
		{
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
		{
			if (AI_targetCity(record, pathFinder, group))
			{
				return;
			}
		}
	}
	else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 2))
	{
		if (AI_pillageRange(record, pathFinder, group, 2))
		{
			return;
		}

		if (AI_cityAttack(record, pathFinder, group, 1, 10))
		{
			return;
		}
	}
	
	if (AI_load(record, group, pathFinder, UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 1))
	{
		return;
	}
	
	if (AI_heal(record, pathFinder, group))
	{
		return;
	}
	
	if (AI_guardCity(record, pathFinder, group, false, true, 2))
	{
		return;
	}
	
	if (AI_patrol(record, pathFinder, group))
	{
		return;
	}
	
	if (AI_retreatToCity(record, group, pathFinder))
	{
		return;
	}
	
	//if (AI_safety())
	//{
	//	return;
	//}

	record.setUseSerialFallback(std::string(__func__) + " unimplemented stuff");
	group.pushSkipMission(record, pathFinder);
}

void DryRunUnitState::AI_barbAttackSeaMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	if (record.getRand(2, "AI Barb") == 0)
	{
		if (AI_pillageRange(record, pathFinder, group, 1))
		{
			return;
		}
	}

	if (AI_anyAttack(record, pathFinder, group, 2, 25))
	{
		return;
	}

	if (AI_pillageRange(record, pathFinder, group, 4))
	{
		return;
	}

	if (AI_heal(record, pathFinder, group))
	{
		return;
	}

	if (AI_patrol(record, pathFinder, group))
	{
		return;
	}

	if (AI_retreatToCity(record, group, pathFinder))
	{
		return;
	}

	//if (AI_safety())
	//{
	//	return;
	//}

	record.setUseSerialFallback(std::string(__func__) + " AI_safety not implemented.");

	group.pushSkipMission(record, pathFinder);
	return;
}

bool DryRunUnitState::AI_bombardCity(GroupUpdateRecord& record)
{
	if (canBombard(plot()))
	{
		record.setUseSerialFallback("AI_bombardCity canBombard");
		return true;
	}

	return false;
}

void DryRunUnitState::AI_cityDefenseMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	const bool bDanger = record.AI_getPlotDanger(getOwner(), plot(), 3) > 0;

	if (bDanger)
	{
		//if (AI_leaveAttack(1, 70, 175))
		//{
		//	return;
		//}
		//
		//if (AI_chokeDefend())
		//{
		//	return;
		//}
		record.setUseSerialFallback("AI_cityDefenseMove danger");
		return;
	}

	if (AI_guardCityBestDefender(record, pathFinder, group))
	{
		return;
	}

	if (!bDanger)
	{
		if (plot()->getOwner() == getOwner())
		{
			if (AI_load(record, group, pathFinder, UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY, 1))
			{
				return;
			}
		}
	}

	if (AI_guardCityMinDefender(record, pathFinder, group, true))
	{
		return;
	}
	
	if (AI_guardCity(record, pathFinder, group, true))
	{
		return;
	}

	if (!bDanger)
	{
		if (AI_group(record, pathFinder, group, UNITAI_SETTLE, /*iMaxGroup*/ 1, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
		{
			return;
		}
	
		if (AI_group(record, pathFinder, group, UNITAI_SETTLE, /*iMaxGroup*/ 2, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
		{
			return;
		}
	
		if (plot()->getOwner() == getOwner())
		{
			if (AI_load(record, group, pathFinder, UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY))
			{
				return;
			}
		}
	}

	AreaAITypes eAreaAI = area()->getAreaAIType(getTeam());
	if ((eAreaAI == AREAAI_ASSAULT) || (eAreaAI == AREAAI_ASSAULT_MASSING) || (eAreaAI == AREAAI_ASSAULT_ASSIST))
	{
		if (AI_load(record, group, pathFinder, UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, 0, MOVE_SAFE_TERRITORY))
		{
			return;
		}
	}
	
	if ((liveUnit.AI_getBirthmark() % 4) == 0)
	{
		if (AI_guardFort(record, pathFinder, group))
		{
			return;
		}
	}
	
	if (AI_guardCityAirlift(record, group))
	{
		return;
	}
	
	if (AI_guardCity(record, pathFinder, group, false, true, 1))
	{
		return;
	}
	
	if (plot()->getOwner() == getOwner())
	{
		if (AI_load(record, group, pathFinder, UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY))
		{
			// will enter here if in danger
			return;
		}
	}
	
	if (AI_guardCity(record, pathFinder, group, false, true))
	{
		return;
	}
	if (!isBarbarian() && ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || (area()->getAreaAIType(getTeam()) == AREAAI_MASSING)))
	{
		if (AI_group(record, pathFinder, group, UNITAI_ATTACK_CITY, -1, 2, 4, /*bIgnoreFaster*/ true))
		{
			return;
		}
	}
	
	if (area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
	{
		if (AI_load(record, group, pathFinder, UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, 1, 2, -1, 1, MOVE_SAFE_TERRITORY))
		{
			// does this ever occur? the previous settler check is less strict, this one should never be true (I think)
			FAssertMsg(false, "unexpected settler load (non-fatal)");
			return;
		}
	}
	
	if (AI_retreatToCity(record, group, pathFinder))
	{
		return;
	}
	
	if (AI_safety(record, pathFinder, group))
	{
		return;
	}

	group.pushSkipMission(record, pathFinder);
}

// Returns true if a mission was pushed...
bool DryRunUnitState::AI_connectPlot(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvPlot* pPlot, int iRange)
{
	FAssert(canBuildRoute());

	if (!record.isVisibleEnemyUnit(pPlot, liveUnit))
	{
		if (record.read_AI_plotTargetMissionAIs(getOwner(), pPlot, MISSIONAI_BUILD, group.liveGroup, iRange) == 0)
		{
			if (group.generatePath(record, pathFinder, pPlot, MOVE_SAFE_TERRITORY, true))
			{
				int iLoop{};
				for (const CvCity* pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
				{
					if (!(pPlot->isConnectedTo(pLoopCity)))
					{
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
						if (plot()->isSamePlotGroup(getOwner(), pLoopCity->plot()))
#else
						if (plot()->getPlotGroup(getOwner()) == pLoopCity->plot()->getPlotGroup(getOwner()))
#endif
						{
							group.pushPathingMission(record, pathFinder, MISSION_ROUTE_TO, getPlotCoord(*pPlot), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
							return true;
						}
					}
				}

				for (const CvCity* pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
				{
					if (!(pPlot->isConnectedTo(pLoopCity)))
					{
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

						if (!record.isVisibleEnemyUnit(pLoopCity->plot(), liveUnit))
						{
							if (group.generatePath(record, pathFinder, pLoopCity->plot(), MOVE_SAFE_TERRITORY, true))
							{
								if (atPlot(pPlot)) // need to test before moving...
								{
									group.pushPathingMission(record, pathFinder, MISSION_ROUTE_TO, getPlotCoord(*pLoopCity->plot()), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
								}
								else
								{
									group.pushPathingMission(record, pathFinder, MISSION_ROUTE_TO, getPlotCoord(*pLoopCity->plot()), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
									group.pushPathingMission(record, pathFinder, MISSION_ROUTE_TO, getPlotCoord(*pPlot), MOVE_SAFE_TERRITORY, (group.getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
								}

								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool DryRunUnitState::AI_guardCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bLeave, bool bSearch, int iMaxPath)
{
	CLLNode<IDInfo>* pUnitNode;
	// Not const because of CvCityAI::AI_isDefended.
	CvCity* pCity;
	CvCity* pLoopCity;
	const CvUnit* pLoopUnit;
	const CvPlot* pPlot;
	const CvPlot* pBestPlot;
	const CvPlot* pBestGuardPlot;
	bool bDefend;
	int iExtra;
	int iCount;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	FAssert(getDomainType() == DOMAIN_LAND);
	FAssert(canDefend());

	pPlot = plot();
	pCity = pPlot->getPlotCity();

	if (pCity && pCity->getOwner() == getOwner()) // XXX check for other team?
	{
		if (bLeave && !record.CvCityAI_AI_isDanger(*pCity))
		{
			iExtra = 1;
		}
		else
		{
			iExtra = (bSearch ? 0 : -record.AI_getPlotDanger(getOwner(), pPlot, 2));
		}

		bDefend = false;


		record.addDependencyOnOurUnitMovementAt(pPlot);
		if (pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, getOwner()) == 1) // XXX check for other team's units?
		{
			bDefend = true;
		}
		else if (!record.CvCityAI_AI_isDefended(*pCity, (AI_isCityAIType() ? -1 : 0) + iExtra)) // XXX check for other team's units?
		{
			if (AI_isCityAIType())
			{
				bDefend = true;
			}
			else
			{
				iCount = 0;

				// As above
				//record.addDependencyOnUnitMovementAt(pPlot);

				pUnitNode = pPlot->headUnitNode();

				while (pUnitNode)
				{
					pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getOwner() == getOwner())
					{
						if (pLoopUnit->isGroupHead())
						{
							if (!(pLoopUnit->isCargo()))
							{
								if (pLoopUnit->canDefend())
								{
									if (!(pLoopUnit->AI_isCityAIType()))
									{
										if (!(pLoopUnit->isHurt()))
										{
											//record.markPlotMissionRead(getPlotCoord(*pPlot), GroupUpdateRecord::kGroupActivity);
											record.addReadDependencyOnGroup(pLoopUnit->getGroup(), GroupUpdateRecord::SelectionGroupAccess::kActivity);
											if (pLoopUnit->isWaiting())
											{
												FAssert(pLoopUnit != &liveUnit);
												iCount++;
											}
										}
									}
									else
									{
										record.addReadDependencyOnGroup(pLoopUnit->getGroup(), GroupUpdateRecord::SelectionGroupAccess::kMissionType);
										// NOTE: This could be our own unit! Keep this in mind for other plop unit loops...
										if ((pLoopUnit == &liveUnit ? group.getMissionType(0) : pLoopUnit->getGroup()->getMissionType(0)) != MISSION_SKIP)
										{
											iCount++;
										}
									}
								}
							}
						}
					}
				}

				if (!(record.CvCityAI_AI_isDefended(*pCity, iCount + iExtra))) // XXX check for other team's units?
				{
					bDefend = true;
				}
			}
		}

		if (bDefend)
		{
			//CvSelectionGroup* pOldGroup = getGroup();
			//CvUnit* pEjectedUnit = getGroup()->AI_ejectBestDefender(pPlot);
			//...
			record.setUseSerialFallback(std::string(__func__) + " AI_ejectBestDefender");
			return true;
		}
	}

	if (bSearch)
	{
		iBestValue = INT_MAX;
		pBestPlot = nullptr;
		pBestGuardPlot = nullptr;

		// TODO: If this looks takes too much pathing dependency, then we should probably sort by distance.
		for (pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				record.addDependencyOnOurUnitMovementAt(pLoopCity->plot());
				if (!(pLoopCity->AI_isDefended((!AI_isCityAIType()) ? pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwner(), NO_TEAM, PUF_isNotCityAIType) : 0)))	// XXX check for other team's units?
				{
					if (!record.isVisibleEnemyUnit(pLoopCity->plot(), liveUnit))
					{
						if ((GC.getGame().getGameTurn() - pLoopCity->getGameTurnAcquired() < 10) || record.read_AI_plotTargetMissionAIs(getOwner(), pLoopCity->plot(), MISSIONAI_GUARD_CITY, group.liveGroup) < 2)
						{
							if (!atPlot(pLoopCity->plot()) && group.generatePath(record, pathFinder, pLoopCity->plot(), PathingArg(0, std::min(iMaxPath, iBestValue)), true, &iPathTurns))
							{
								if (iPathTurns <= iMaxPath)
								{
									iValue = iPathTurns;

									if (iValue < iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = pathFinder.getPathEndTurnPlot();
										pBestGuardPlot = pLoopCity->plot();
										FAssert(!atPlot(pBestPlot));
									}
								}
							}
						}
					}
				}

				if (pBestPlot != nullptr)
				{
					break;
				}
			}
		}

		if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
		{
			FAssert(!atPlot(pBestPlot));

			//CvSelectionGroup* pOldGroup = getGroup();
			//CvUnit* pEjectedUnit = getGroup()->AI_ejectBestDefender(pPlot);
			//
			//if (pEjectedUnit != nullptr)
			//{
			//	pEjectedUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
			//	if (pEjectedUnit->getGroup() == pOldGroup || pEjectedUnit == this)
			//	{
			//		return true;
			//	}
			//	else
			//	{
			//		return false;
			//	}
			//}
			//else
			//{
			//	//This unit is not suited for defense, skip the mission
			//	//to protect this city but encourage others to defend instead.
			//	if (atPlot(pBestGuardPlot))
			//	{
			//		getGroup()->pushMission(MISSION_SKIP);
			//	}
			//	else
			//	{
			//		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
			//	}
			//	return true;
			//}

			if (group.has_AI_ejectBestDefender(pPlot))
				record.setUseSerialFallback(std::string(__func__) + " AI_ejectBestDefender");
			else
			{
				//This unit is not suited for defense, skip the mission
				//to protect this city but encourage others to defend instead.
				if (atPlot(pBestGuardPlot))
				{
					group.pushSkipMission(record, pathFinder);
				}
				else
				{
					group.pushGenericMission(record, pathFinder, MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				}
				return true;
			}
		}
	}

	return false;
}

bool DryRunUnitState::AI_improveBonus(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iMinValue, const CvPlot** ppBestPlot, BuildTypes* peBestBuild, int* piBestValue)
{
	const CvPlot* pBestPlot = nullptr;
	ImprovementTypes eImprovement;
	BuildTypes eBuild;
	BuildTypes eBestBuild = NO_BUILD;
	BuildTypes eBestTempBuild;
	BonusTypes eNonObsoleteBonus;
	int iValue;
	int iBestValue = 0;
	int iBestTempBuildValue;
	int iBestResourceValue = 0;
	bool bBestBuildIsRoute = false;

	const bool bCanRoute = canBuildRoute();

	for (const int iI : range(GC.getMapINLINE().numPlotsINLINE()))
	{
		const CvPlot* const pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->getOwner() == getOwner() && AI_plotValid(pLoopPlot))
		{
			bool bCanImprove = (pLoopPlot->area() == area());
			if (!bCanImprove)
			{
				if (DOMAIN_SEA == getDomainType() && pLoopPlot->isWater() && plot()->isAdjacentToArea(pLoopPlot->area()))
				{
					bCanImprove = true;
				}
			}

			if (bCanImprove)
			{
				eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

				if (eNonObsoleteBonus != NO_BONUS)
				{
					const bool  bIsConnected = pLoopPlot->isConnectedToCapital(getOwner());
					if ((pLoopPlot->getWorkingCity() != nullptr) || (bIsConnected || bCanRoute))
					{
						eImprovement = pLoopPlot->getImprovementType();

						bool bDoImprove = false;

						if (eImprovement == NO_IMPROVEMENT)
						{
							bDoImprove = true;
						}
						else if (GC.getImprovementInfo(eImprovement).isActsAsCity() || GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
						{
							bDoImprove = false;
						}
						else if (eImprovement == (ImprovementTypes)(GC.getDefineINT("RUINS_IMPROVEMENT")))
						{
							bDoImprove = true;
						}
						else if (!GET_PLAYER(getOwner()).isOption(PLAYEROPTION_SAFE_AUTOMATION))
						{
							bDoImprove = true;
						}

						iBestTempBuildValue = INT_MAX;
						eBestTempBuild = NO_BUILD;

						if (bDoImprove)
						{
							for (const int iJ : range(GC.getNumBuildInfos()))
							{
								eBuild = ((BuildTypes)iJ);

								if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
								{
									if (GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isImprovementBonusTrade(eNonObsoleteBonus) || (!pLoopPlot->isCityRadius() && GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isActsAsCity()))
									{
										if (canBuild(pLoopPlot, eBuild))
										{
											if ((pLoopPlot->getFeatureType() == NO_FEATURE) || !GC.getBuildInfo(eBuild).isFeatureRemove(pLoopPlot->getFeatureType()) || !GET_PLAYER(getOwner()).isOption(PLAYEROPTION_LEAVE_FORESTS))
											{
												iValue = 10000;

												iValue /= (GC.getBuildInfo(eBuild).getTime() + 1);

												// XXX feature production???

												if (iValue < iBestTempBuildValue)
												{
													iBestTempBuildValue = iValue;
													eBestTempBuild = eBuild;
												}
											}
										}
									}
								}
							}
						}
						if (eBestTempBuild == NO_BUILD)
						{
							bDoImprove = false;
						}

						if ((eBestTempBuild != NO_BUILD) || (bCanRoute && !bIsConnected))
						{
							int iPathTurns{};
							if (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns))
							{
								iValue = GET_PLAYER(getOwner()).AI_bonusVal(eNonObsoleteBonus);

								if (bDoImprove)
								{
									eImprovement = (ImprovementTypes)GC.getBuildInfo(eBestTempBuild).getImprovement();
									FAssert(eImprovement != NO_IMPROVEMENT);
									//iValue += (GC.getImprovementInfo((ImprovementTypes) GC.getBuildInfo(eBestTempBuild).getImprovement()))
									iValue += 5 * pLoopPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, getOwner(), false);
									iValue += 5 * pLoopPlot->calculateNatureYield(YIELD_FOOD, getTeam(), (pLoopPlot->getFeatureType() == NO_FEATURE) ? true : GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()));
								}

								iValue += std::max(0, 100 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

								if (GET_PLAYER(getOwner()).getNumTradeableBonuses(eNonObsoleteBonus) == 0)
								{
									iValue *= 2;
								}

								int iMaxWorkers = 1;
								if ((eBestTempBuild != NO_BUILD) && (!GC.getBuildInfo(eBestTempBuild).isKill()))
								{
									//allow teaming.
									iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBestTempBuild);
									if (pathFinder.getLastNodeMP() == 0)
									{
										iMaxWorkers = std::min((iMaxWorkers + 1) / 2, 1 + GET_PLAYER(getOwner()).AI_baseBonusVal(eNonObsoleteBonus) / 20);
									}
								}

								if ((!bDoImprove || (pLoopPlot->getBuildTurnsLeft(eBestTempBuild, 0, 0) > (iPathTurns * 2 - 1)))
									// Do this last to avoid unnecessary dependency.
									&& (record.read_AI_plotTargetMissionAIs(getOwner(), pLoopPlot, MISSIONAI_BUILD, group.liveGroup) < iMaxWorkers))
								{
									if (bDoImprove)
									{
										iValue *= 1000;

										if (atPlot(pLoopPlot))
										{
											iValue *= 3;
										}

										iValue /= (iPathTurns + 1);

										if (pLoopPlot->isCityRadius())
										{
											iValue *= 2;
										}

										if (iValue > iBestValue)
										{
											iBestValue = iValue;
											eBestBuild = eBestTempBuild;
											pBestPlot = pLoopPlot;
											bBestBuildIsRoute = false;
											iBestResourceValue = iValue;
										}
									}
									else
									{
										FAssert(bCanRoute && !bIsConnected);
										eImprovement = pLoopPlot->getImprovementType();
										if ((eImprovement != NO_IMPROVEMENT) && (GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus)))
										{
											iValue *= 1000;
											iValue /= (iPathTurns + 1);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestBuild = NO_BUILD;
												pBestPlot = pLoopPlot;
												bBestBuildIsRoute = true;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((iBestValue < iMinValue) && (nullptr != ppBestPlot))
	{
		FAssert(nullptr != peBestBuild);
		FAssert(nullptr != piBestValue);

		*ppBestPlot = pBestPlot;
		*peBestBuild = eBestBuild;
		*piBestValue = iBestResourceValue;
	}

	if (pBestPlot != nullptr)
	{
		if (eBestBuild != NO_BUILD)
		{
			FAssertMsg(!bBestBuildIsRoute, "BestBuild should not be a route");
			FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");


			MissionTypes eBestMission = MISSION_MOVE_TO;

			if ((pBestPlot->getWorkingCity() == nullptr) || !pBestPlot->getWorkingCity()->isConnectedToCapital())
			{
				eBestMission = MISSION_ROUTE_TO;
			}
			else
			{
				int iDistance = stepDistance(coord.x, coord.y, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
				int iPathTurns{};
				if (group.generatePath(record, pathFinder, pBestPlot, 0, false, &iPathTurns))
				{
					if (iPathTurns >= iDistance)
					{
						eBestMission = MISSION_ROUTE_TO;
					}
				}
			}

			eBestBuild = AI_betterPlotBuild(record, pBestPlot, eBestBuild);
			group.pushPathingMission(record, pathFinder, eBestMission, getPlotCoord(*pBestPlot), 0, false, false, MISSIONAI_BUILD, pBestPlot);
			group.pushGenericMission(record, pathFinder, MISSION_BUILD, eBestBuild, -1, 0, (group.getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

			return true;
		}
		else if (bBestBuildIsRoute)
		{
			if (AI_connectPlot(record, pathFinder, group, pBestPlot))
			{
				return true;
			}
			/*else
			{
				// the plot may be connected, but not connected to capital, if capital is not on same area, or if civ has no capital (like barbarians)
				FAssertMsg(false, "Expected that a route could be built to eBestPlot");
			}*/
		}
		else
		{
			FAssert(false);
		}
	}

	return false;
}

//void DryRunUnitState::AI_workerMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
//{
//	CvCity* pCity;
//	bool bCanRoute;
//	bool bNextCity;
//
//	bCanRoute = canBuildRoute();
//	bNextCity = false;
//
//	// XXX could be trouble...
//	if (plot()->getOwner() != getOwner())
//	{
//		if (AI_retreatToCity(record, group, pathFinder))
//		{
//			return;
//		}
//	}
//
//	//if (!isHuman())
//	{
//		if (plot()->getOwner() == getOwner())
//		{
//			if (AI_load(record, group, pathFinder, UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 2, -1, -1, 0, MOVE_SAFE_TERRITORY))
//			{
//				return;
//			}
//		}
//	}
//
//	if (!(group.canDefend()))
//	{
//		//bool AI_isPlotThreatened(IPathFinder& pathFinder, PlayerTypes defender, const CvPlot* pPlot, int iRange, bool bTestMoves) const;
//		if (GET_PLAYER(getOwner()).AI_isPlotThreatened(plot(), 2))
//		{
//			if (AI_retreatToCity(record, group, pathFinder)) // XXX maybe not do this??? could be working productively somewhere else...
//			{
//				return;
//			}
//		}
//	}
//
//	if (bCanRoute)
//	{
//		if (plot()->getOwner() == getOwner()) // XXX team???
//		{
//			BonusTypes eNonObsoleteBonus = plot()->getNonObsoleteBonusType(getTeam());
//			if (NO_BONUS != eNonObsoleteBonus)
//			{
//				if (!(plot()->isConnectedToCapital()))
//				{
//					ImprovementTypes eImprovement = plot()->getImprovementType();
//					if (NO_IMPROVEMENT != eImprovement && GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
//					{
//						if (AI_connectPlot(record, pathFinder, group, plot()))
//						{
//							return;
//						}
//					}
//				}
//			}
//		}
//	}
//
//	CvPlot* pBestBonusPlot = nullptr;
//	BuildTypes eBestBonusBuild = NO_BUILD;
//	int iBestBonusValue = 0;
//
//	if (AI_improveBonus(record, pathFinder, group, 25, &pBestBonusPlot, &eBestBonusBuild, &iBestBonusValue))
//	{
//		return;
//	}
//
//	if (bCanRoute && !isBarbarian())
//	{
//		if (AI_connectCity(record, pathFinder, group))
//		{
//			return;
//		}
//	}
//
//	pCity = nullptr;
//
//	if (plot()->getOwner() == getOwner())
//	{
//		pCity = plot()->getPlotCity();
//		if (pCity == nullptr)
//		{
//			pCity = plot()->getWorkingCity();
//		}
//	}
//
//
//	//	if (pCity != nullptr)
//	//	{
//	//		bool bMoreBuilds = false;
//	//		for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
//	//		{
//	//			CvPlot* pLoopPlot = plotCity(getX_INLINE(), getY_INLINE(), iI);
//	//			if ((iI != CITY_HOME_PLOT) && (pLoopPlot != nullptr))
//	//			{
//	//				if (pLoopPlot->getWorkingCity() == pCity)
//	//				{
//	//					if (pLoopPlot->isBeingWorked())
//	//					{
//	//						if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT)
//	//						{
//	//							if (pCity->AI_getBestBuildValue(iI) > 0)
//	//							{
//	//								ImprovementTypes eImprovement;
//	//								eImprovement = (ImprovementTypes)GC.getBuildInfo((BuildTypes)pCity->AI_getBestBuild(iI)).getImprovement();
//	//								if (eImprovement != NO_IMPROVEMENT)
//	//								{
//	//									bMoreBuilds = true;
//	//									break;
//	//								}
//	//							}
//	//						}
//	//					}
//	//				}
//	//			}
//	//		}
//	//		
//	//		if (bMoreBuilds)
//	//		{
//	//			if (AI_improveCity(pCity))
//	//			{
//	//				return;
//	//			}
//	//		}
//	//	}
//	if (pCity != nullptr)
//	{
//
//		if ((pCity->AI_getWorkersNeeded() > 0) && (plot()->isCity() ||
//			(record.addDependencyOnCityWorkersHave(pCity), pCity->AI_getWorkersNeeded() < ((1 + pCity->AI_getWorkersHave() * 2) / 3))))
//		{
//			if (AI_improveCity(record, pathFinder, group, pCity))
//			{
//				return;
//			}
//		}
//	}
//
//	if (AI_improveLocalPlot(2, pCity))
//	{
//		return;
//	}
//
//	bool bBuildFort = false;
//
//	if (GC.getGame().getSorenRandNum(5, "AI Worker build Fort with Priority"))
//	{
//		bool bCanal = ((100 * area()->getNumCities()) / std::max(1, GC.getGame().getNumCities()) < 85);
//		CvPlayerAI& kPlayer = GET_PLAYER(getOwner());
//		bool bAirbase = false;
//		bAirbase = (kPlayer.AI_totalUnitAIs(UNITAI_PARADROP) || kPlayer.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kPlayer.AI_totalUnitAIs(UNITAI_MISSILE_AIR));
//
//		if (bCanal || bAirbase)
//		{
//			if (AI_fortTerritory(bCanal, bAirbase))
//			{
//				return;
//			}
//		}
//		bBuildFort = true;
//	}
//
//
//	if (bCanRoute && isBarbarian())
//	{
//		if (AI_connectCity())
//		{
//			return;
//		}
//	}
//
//	if ((pCity == nullptr) || (pCity->AI_getWorkersNeeded() == 0) || ((pCity->AI_getWorkersHave() > (pCity->AI_getWorkersNeeded() + 1))))
//	{
//		if ((pBestBonusPlot != nullptr) && (iBestBonusValue >= 15))
//		{
//			if (AI_improvePlot(pBestBonusPlot, eBestBonusBuild))
//			{
//				return;
//			}
//		}
//
//		//		if (pCity == nullptr)
//		//		{
//		//			pCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), getOwner()); // XXX do team???
//		//		}
//
//		if (AI_nextCityToImprove(pCity))
//		{
//			return;
//		}
//
//		bNextCity = true;
//	}
//
//	if (pBestBonusPlot != nullptr)
//	{
//		if (AI_improvePlot(pBestBonusPlot, eBestBonusBuild))
//		{
//			return;
//		}
//	}
//
//	if (pCity != nullptr)
//	{
//		if (AI_improveCity(pCity))
//		{
//			return;
//		}
//	}
//
//	if (!bNextCity)
//	{
//		if (AI_nextCityToImprove(pCity))
//		{
//			return;
//		}
//	}
//
//	if (bCanRoute)
//	{
//		if (AI_routeTerritory(true))
//		{
//			return;
//		}
//
//		if (AI_connectBonus(false))
//		{
//			return;
//		}
//
//		if (AI_routeCity())
//		{
//			return;
//		}
//	}
//
//	if (AI_irrigateTerritory())
//	{
//		return;
//	}
//
//	if (!bBuildFort)
//	{
//		bool bCanal = ((100 * area()->getNumCities()) / std::max(1, GC.getGame().getNumCities()) < 85);
//		CvPlayerAI& kPlayer = GET_PLAYER(getOwner());
//		bool bAirbase = false;
//		bAirbase = (kPlayer.AI_totalUnitAIs(UNITAI_PARADROP) || kPlayer.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kPlayer.AI_totalUnitAIs(UNITAI_MISSILE_AIR));
//
//		if (bCanal || bAirbase)
//		{
//			if (AI_fortTerritory(bCanal, bAirbase))
//			{
//				return;
//			}
//		}
//	}
//
//	if (bCanRoute)
//	{
//		if (AI_routeTerritory())
//		{
//			return;
//		}
//	}
//
//	//if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0))
//	{
//		//if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn()))
//		{
//			if (AI_nextCityToImproveAirlift())
//			{
//				return;
//			}
//		}
//		//if (!isHuman())
//		{
//			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY))
//			{
//				return;
//			}
//		}
//	}
//
//	if (AI_improveLocalPlot(3, nullptr))
//	{
//		return;
//	}
//
//	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER))
//	{
//		if (GC.getGameINLINE().getElapsedGameTurns() > 10)
//		{
//			if (GET_PLAYER(getOwner()).AI_totalUnitAIs(UNITAI_WORKER) > GET_PLAYER(getOwner()).getNumCities())
//			{
//				if (GET_PLAYER(getOwner()).calculateUnitCost() > 0)
//				{
//					scrap();
//					return;
//				}
//			}
//		}
//	}
//
//	if (AI_retreatToCity(false, true))
//	{
//		return;
//	}
//
//	if (AI_retreatToCity(result, group, pathFinder))
//	{
//		return;
//	}
//
//	if (AI_safety())
//	{
//		return;
//	}
//
//	getGroup()->pushMission(MISSION_SKIP);
//	return;
//}

bool DryRunUnitState::AI_rangeAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange)
{
	FAssert(canMove());

	if (!canRangeStrike())
	{
		return false;
	}

	int iSearchRange = AI_searchRange(iRange);
		
	CvPlot* pBestPlot = nullptr;
		
	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			CvPlot* pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);
		
			if (pLoopPlot != nullptr)
			{
				if (record.isVisibleEnemyUnit(pLoopPlot, liveUnit) || (pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getTeam())))
				{
					//if (!atPlot(pLoopPlot) && canRangeStrikeAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
					//{
					//	int iValue = group.AI_attackOdds(pLoopPlot, true);
					//
					//	if (iValue > iBestValue)
					//	{
					//		iBestValue = iValue;
					//		pBestPlot = pLoopPlot;
					//	}
					//}

					record.setUseSerialFallback(std::string(__func__) + " Not implemented");
					return true;
				}
			}
		}
	}
		
	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_RANGE_ATTACK, getPlotCoord(*pBestPlot), 0);
		return true;
	}

	return false;
}

// Have to override this to use our RNG.
static int AI_targetCityValue(const CvPlayerAI& player, GroupUpdateRecord& record, const CvCity* pCity, bool bRandomize, bool bIgnoreAttackers = false, bool bOverestimate = false)
{
	const CvCity* pNearestCity;
	const CvPlot* pLoopPlot;
	int iValue;
	int iI;

	FAssertMsg(pCity != nullptr, "City is not assigned a valid value");

	iValue = 1;

	iValue += ((pCity->getPopulation() * (50 + pCity->calculateCulturePercent(player.getID()))) / 100);

	if (pCity->getDefenseDamage() > 0)
	{
		iValue += ((pCity->getDefenseDamage() / 30) + 1);
	}

	if (pCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()))
	{
		iValue++;
	}

	if (pCity->hasActiveWorldWonder())
	{
		iValue += 1 + pCity->getNumWorldWonders();
	}

	if (pCity->isHolyCity())
	{
		iValue += 2;
	}

	if (pCity->isEverOwned(player.getID()))
	{
		iValue += 3;
	}
	if (!bIgnoreAttackers)
	{
		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			record.addDependencyOnOurUnitMovementAt(plotDirection(pCity->plot()->getX_INLINE(), pCity->plot()->getY_INLINE(), ((DirectionTypes)iI)));
		iValue += player.AI_adjacentPotentialAttackers(pCity->plot());
	}

	for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
	{
		pLoopPlot = plotCity(pCity->getX_INLINE(), pCity->getY_INLINE(), iI);

		if (pLoopPlot != nullptr)
		{
			if (pLoopPlot->getBonusType(player.getTeam()) != NO_BONUS)
			{
				iValue++;
			}

			if (pLoopPlot->getOwner() == player.getID())
			{
				iValue++;
			}

			if (pLoopPlot->isAdjacentPlayer(player.getID(), true))
			{
				iValue++;
			}
		}
	}

	pNearestCity = GC.getMapINLINE().findCity(pCity->getX_INLINE(), pCity->getY_INLINE(), player.getID());

	if (pNearestCity != nullptr)
	{
		if (bOverestimate)
			iValue += std::max(1, ((GC.getMapINLINE().maxStepDistance() * 2) - ::stepDistance(pNearestCity->getX_INLINE(), pNearestCity->getY_INLINE(), pCity->getX_INLINE(), pCity->getY_INLINE())));
		else
			iValue += std::max(1, ((GC.getMapINLINE().maxStepDistance() * 2) - GC.getMapINLINE().calculatePathDistance(pNearestCity->plot(), pCity->plot())));
	}

	if (bRandomize)
	{
		iValue += record.getRand(static_cast<unsigned short>(((pCity->getPopulation() / 2) + 1)), "AI Target City Value");
	}

	return iValue;
}

bool DryRunUnitState::AI_targetCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iFlags)
{
	const CvCity* pTargetCity;
	const CvCity* pBestCity;
	const CvPlot* pAdjacentPlot;
	const CvPlot* pBestPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	iBestValue = 0;
	pBestCity = nullptr;

	pTargetCity = area()->getTargetCity(getOwner());

	if (pTargetCity != nullptr)
	{
		if (AI_potentialEnemy(pTargetCity->getTeam(), pTargetCity->plot()))
		{
			if (!atPlot(pTargetCity->plot()) && group.generatePath(record, pathFinder, pTargetCity->plot(), iFlags, true))
			{
				pBestCity = pTargetCity;
			}
		}
	}

	//std::osyncstream(std::clog) << std::string(__func__) + ": " << std::this_thread::get_id() << ' ' << group.liveGroup->getID() << std::endl;

	if (pBestCity == nullptr)
	{
		std::vector<const CvCity*> cityList;
		cityList.reserve(100);
		for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
			if (GET_PLAYER((PlayerTypes)iI).isAlive())
				for (const CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
					if (AI_plotValid(pLoopCity->plot()) && AI_potentialEnemy(GET_PLAYER((PlayerTypes)iI).getTeam(), pLoopCity->plot()))
						cityList.push_back(pLoopCity);
		std::ranges::sort(cityList, std::less<>(), [coord = coord](const CvCity* city) {
			return ::stepDistance(coord.x, coord.y, city->getX_INLINE(), city->getY_INLINE());
			});
		for (const CvCity* const pLoopCity : cityList)
		{
			if (record.getSerialFallbackReason())
			{
				group.pushSkipMission(record, pathFinder);
				return true;
			}

			const auto computeCityBaseValue = [&](bool overestimate) {
				iValue = 0;
				if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) //lemming?
					iValue = AI_targetCityValue(GET_PLAYER(getOwner()), record, pLoopCity, false, false, overestimate);
				else
					iValue = AI_targetCityValue(GET_PLAYER(getOwner()), record, pLoopCity, true, true, overestimate);

				iValue *= 1000;

				if ((area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
				{
					if (pLoopCity->calculateCulturePercent(getOwner()) < 75)
					{
						iValue /= 2;
					}
				}
				};

			// Else, compute the value first, and then the max turns.
			if (!atPlot(pLoopCity->plot()))
			{
				// Compute max turns from an overestimate of value, then use the vanilla iValue afterwards.
				computeCityBaseValue(true);
				// iValue / (4 + iPathTurns*iPathTurns) - 1 >= iBestValue
				// iPathTurns^2 <= iValue/(iBestValue+1)-4
				// if iValue/(iBestValue+1)-4 > 0: iPathTurns <= sqrt(iValue/(iBestValue+1)-4)
				const int maxPathTurns = (int)std::sqrt(std::max(1.0, double(iValue) / (iBestValue + 1) - 4)) + 1;

				if (group.generatePath(record, pathFinder, pLoopCity->plot(), PathingArg(iFlags, maxPathTurns), true, &iPathTurns))
				{
					computeCityBaseValue(false);
					iValue /= (4 + iPathTurns * iPathTurns);

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestCity = pLoopCity;
					}
				}
			}
		}
	}

	if (pBestCity)
	{
		iBestValue = 0;
		pBestPlot = nullptr;

		if (0 == (iFlags & MOVE_THROUGH_ENEMY))
		{
			for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			{
				pAdjacentPlot = plotDirection(pBestCity->getX_INLINE(), pBestCity->getY_INLINE(), ((DirectionTypes)iI));

				if (pAdjacentPlot)
				{
					if (AI_plotValid(pAdjacentPlot))
					{
						if (!record.isVisibleEnemyUnit(pAdjacentPlot, liveUnit))
						{
							if (group.generatePath(record, pathFinder, pAdjacentPlot, iFlags, true, &iPathTurns))
							{
								iValue = std::max(0, (pAdjacentPlot->defenseModifier(getTeam(), false) + 100));

								if (!(pAdjacentPlot->isRiverCrossing(directionXY(pAdjacentPlot, pBestCity->plot()))))
								{
									iValue += (12 * -(GC.getRIVER_ATTACK_MODIFIER()));
								}

								if (!isEnemy(pAdjacentPlot->getTeam(), pAdjacentPlot))
								{
									iValue += 100;
								}

								iValue = std::max(1, iValue);

								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = pathFinder.getPathEndTurnPlot();
								}
							}
						}
					}
				}
			}
		}


		else
		{
			pBestPlot = pBestCity->plot();
		}

		if (pBestPlot != nullptr)
		{
			FAssert(!(pBestCity->at(pBestPlot)) || 0 != (iFlags & MOVE_THROUGH_ENEMY)); // no suicide missions...
			if (!atPlot(pBestPlot))
			{
				group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot), iFlags);
				return true;
			}
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool DryRunUnitState::AI_retreatToCity(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder, bool bPrimary, bool bAirlift, int iMaxPath)
{
	const CvPlot* pBestPlot = nullptr;
	int iPathTurns{};
	int iBestValue = INT_MAX;
	const int iCurrentDanger = GET_PLAYER(getOwner()).AI_getPlotDanger(plot());

	const CvCity* const pCity = plot()->getPlotCity();


	if (0 == iCurrentDanger)
	{
		if (pCity != nullptr)
		{
			if (pCity->getOwner() == getOwner())
			{
				if (!bPrimary || GET_PLAYER(getOwner()).AI_isPrimaryArea(pCity->area()))
				{
					if (!bAirlift || (pCity->getMaxAirlift() > 0))
					{
						// NOTE: As this is our city, it is always visible, so we don't take a dependence on this visibility.
						if (!(pCity->plot()->isVisibleEnemyUnit(&liveUnit)))
						{
							group.pushSkipMission(record, pathFinder);
							return true;
						}
					}
				}
			}
		}
	}

	int iPass{};

	for (iPass = 0; iPass < 4; iPass++)
	{
		int iLoop{};
		for (const CvCity* pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				if (!bPrimary || GET_PLAYER(getOwner()).AI_isPrimaryArea(pLoopCity->area()))
				{
					if (!bAirlift || (pLoopCity->getMaxAirlift() > 0))
					{
						// NOTE: As this is our city, it is always visible, so we don't take a dependence on this visibility.
						if (!(pLoopCity->plot()->isVisibleEnemyUnit(&liveUnit)))
						{
							if (!atPlot(pLoopCity->plot()))
							{
								if (group.generatePath(record, pathFinder, pLoopCity->plot(), ((iPass > 1) ? MOVE_IGNORE_DANGER : 0), true, &iPathTurns))
								{
									if (iPathTurns <= ((iPass == 2) ? 1 : iMaxPath))
									{
										if ((iPass > 0) || (group.canFight() || GET_PLAYER(getOwner()).AI_getPlotDanger(pLoopCity->plot()) < iCurrentDanger))
										{
											int iValue = iPathTurns;

											if (AI_getUnitAIType() == UNITAI_SETTLER_SEA)
											{
												iValue *= 1 + std::max(0, GET_PLAYER(getOwner()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLE)
													- GET_PLAYER(getOwner()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLER_SEA));
											}

											if (iValue < iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = pathFinder.getPathEndTurnPlot();
												FAssert(!atPlot(pBestPlot));
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (pBestPlot != nullptr)
		{
			break;
		}
		else if (iPass == 0)
		{
			if (pCity != nullptr)
			{
				if (pCity->getOwner() == getOwner())
				{
					if (!bPrimary || GET_PLAYER(getOwner()).AI_isPrimaryArea(pCity->area()))
					{
						if (!bAirlift || (pCity->getMaxAirlift() > 0))
						{
							// NOTE: This is our own city, so we'll always have visibility.
							if (!(pCity->plot()->isVisibleEnemyUnit(&liveUnit)))
							{
								group.pushSkipMission(record, pathFinder);
								return true;
							}
						}
					}
				}
			}
		}

		if (group.alwaysInvisible())
		{
			break;
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot), ((iPass > 0) ? MOVE_IGNORE_DANGER : 0));
		return true;
	}

	if (pCity != nullptr)
	{
		if (pCity->getTeam() == getTeam())
		{
			group.pushSkipMission(record, pathFinder);
			return true;
		}
	}

	return false;
}

// Returns true if we loaded onto a transport or a mission was pushed...
bool DryRunUnitState::AI_load(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder,
	UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI,
	int iMinCargo, int iMinCargoSpace, int iMaxCargoSpace, int iMaxCargoOurUnitAI, [[maybe_unused]] int iFlags, [[maybe_unused]] int iMaxPath)
{
	CvUnit* pLoopUnit;
	[[maybe_unused]] CvUnit* pBestUnit;
	[[maybe_unused]] int iBestValue;
	int iLoop;

	// XXX what to do about groups???
	/*if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}*/

	if (hasCargo())
	{
		return false;
	}

	if (isCargo())
	{
		group.pushSkipMission(record, pathFinder);
		return true;
	}

	if ((getDomainType() == DOMAIN_LAND) && !canMoveAllTerrain())
	{
		if (area()->getNumAIUnits(getOwner(), eUnitAI) == 0)
		{
			return false;
		}
	}

	// do not load transports if we are already in a land war
	AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
	bool bLandWar = ((eAreaAIType == AREAAI_OFFENSIVE) || (eAreaAIType == AREAAI_DEFENSIVE) || (eAreaAIType == AREAAI_MASSING));
	if (!isBarbarian() && bLandWar && (eMissionAI != MISSIONAI_LOAD_SETTLER))
	{
		return false;
	}

	iBestValue = INT_MAX;
	pBestUnit = nullptr;

	constexpr MissionAITypes aeLoadMissionAI[]{
		MISSIONAI_LOAD_ASSAULT,
		MISSIONAI_LOAD_SETTLER,
		MISSIONAI_LOAD_SPECIAL,
		MISSIONAI_ATTACK_SPY
	};

	for (pLoopUnit = GET_PLAYER(getOwner()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwner()).nextUnit(&iLoop))
	{
		if (pLoopUnit != &liveUnit)
		{
			if (AI_plotValid(pLoopUnit->plot()))
			{
				if (canLoadUnitInto(pLoopUnit, pLoopUnit->plot()))
				{
					// special case ASSAULT_SEA UnitAI, so that, if a unit is marked escort, but can load units, it will load them
					// transport units might have been built as escort, this most commonly happens with galleons
					UnitAITypes eLoopUnitAI = pLoopUnit->AI_getUnitAIType();
					if (eLoopUnitAI == eUnitAI)// || (eUnitAI == UNITAI_ASSAULT_SEA && eLoopUnitAI == UNITAI_ESCORT_SEA))
					{
						int iCargoSpaceAvailable = pLoopUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());
						// Do this later.
						//iCargoSpaceAvailable -= record.read_AI_unitTargetMissionAIs(getOwner(), pLoopUnit, aeLoadMissionAI, &group.liveGroup);
						if (iCargoSpaceAvailable > 0)
						{
							if ((eTransportedUnitAI == NO_UNITAI) || (pLoopUnit->getUnitAICargo(eTransportedUnitAI) > 0))
							{
								if ((iMinCargo == -1) || (pLoopUnit->getCargo() >= iMinCargo))
								{
									if ((iMinCargoSpace == -1) || (pLoopUnit->cargoSpaceAvailable() >= iMinCargoSpace))
									{
										if ((iMaxCargoSpace == -1) || (pLoopUnit->cargoSpaceAvailable() <= iMaxCargoSpace))
										{
											if ((iMaxCargoOurUnitAI == -1) || (pLoopUnit->getUnitAICargo(AI_getUnitAIType()) <= iMaxCargoOurUnitAI))
											{
												if (group.getHeadUnitAI() != UNITAI_CITY_DEFENSE || !plot()->isCity() || (plot()->getTeam() != getTeam()))
												{
													iCargoSpaceAvailable -= record.read_AI_unitTargetMissionAIs(getOwner(), pLoopUnit, aeLoadMissionAI, group.liveGroup);
													if (iCargoSpaceAvailable > 0)
													{
														if (isVisibleEnemyUnitAt(pLoopUnit, record))
														{
															//CvPlot* pUnitTargetPlot = pLoopUnit->getGroup()->AI_getMissionAIPlot();
															//if ((pUnitTargetPlot == nullptr) || (pUnitTargetPlot->getTeam() == getTeam()) || (!pUnitTargetPlot->isOwned() || !isPotentialEnemy(pUnitTargetPlot->getTeam(), pUnitTargetPlot)))
															{
																// if generatePath...
																record.setUseSerialFallback(std::string(__func__) + " Will load or split group in the end, which is not implemented.");
																return true;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	//if (pBestUnit != nullptr)
	//{
	//	...
	//}

	return false;
}

bool DryRunUnitState::isVisibleEnemyUnitAt(const CvUnit* targetUnit, GroupUpdateRecord& record)
{
	assert(targetUnit->getOwner() == getOwner());
	if (targetUnit->getOwner() != getOwner())
		record.setUseSerialFallback(std::string(__func__) + " Should be one of our units.");

	const CvPlot* const plot = targetUnit->plot();

	// If this isn't one of our cities, then visibility could come from one of our units, so we'll need to take a dependency.
	if (!(plot->isCity() && plot->getOwner() == getOwner()))
		record.addDependencyOnUnitMovement(getOwner(), targetUnit);

	return plot->isVisibleEnemyUnit(&liveUnit);
}

BuildTypes DryRunUnitState::AI_betterPlotBuild(GroupUpdateRecord& record, const CvPlot* pPlot, BuildTypes eBuild) const
{
	const CvBuildInfo& kOriginalBuildInfo = GC.getBuildInfo(eBuild);

	if (kOriginalBuildInfo.getRoute() != NO_ROUTE)
	{
		return eBuild;
	}

	//int iWorkersNeeded = AI_calculatePlotWorkersNeeded(pPlot, eBuild);

	if ((pPlot->getBonusType() == NO_BONUS) && (pPlot->getWorkingCity() != nullptr))
	{
		// We have dependency on AI_getWorkersHave.
		// iWorkersNeeded = std::max(1, std::min(iWorkersNeeded, pPlot->getWorkingCity()->AI_getWorkersHave()));
		record.addDependencyOnCityWorkersHave(pPlot->getWorkingCity());
	}

	struct Evil : CvUnitAI
	{
		static auto get()
		{
			return &Evil::AI_betterPlotBuild;
		}
	};
	return (liveUnit.*Evil::get())(pPlot, eBuild);
}

// Returns true if a mission was pushed...
bool DryRunUnitState::AI_connectCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	CvCity* pLoopCity;
	int iLoop;

	// XXX how do we make sure that we can build roads???

	pLoopCity = plot()->getWorkingCity();
	if (pLoopCity != nullptr)
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (!(pLoopCity->isConnectedToCapital()))
			{
				if (AI_connectPlot(record, pathFinder, group, pLoopCity->plot(), 1))
				{
					return true;
				}
			}
		}
	}

	for (pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (!(pLoopCity->isConnectedToCapital()))
			{
				if (AI_connectPlot(record, pathFinder, group, pLoopCity->plot(), 1))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool DryRunUnitState::AI_improveCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvCity* pCity)
{
	const CvPlot* pBestPlot = nullptr;
	BuildTypes eBestBuild = NO_BUILD;
	MissionTypes eMission = NO_MISSION;

	if (AI_bestCityBuild(record, pathFinder, group, pCity, &pBestPlot, &eBestBuild, nullptr, &liveUnit))
	{
		FAssertMsg(pBestPlot != nullptr, "BestPlot is not assigned a valid value");
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");
		if ((plot()->getWorkingCity() != pCity) || (GC.getBuildInfo(eBestBuild).getRoute() != NO_ROUTE))
		{
			eMission = MISSION_ROUTE_TO;
		}
		else
		{
			eMission = MISSION_MOVE_TO;
			if (nullptr != pBestPlot && group.generatePath(record, pathFinder, pBestPlot) && (pathFinder.getLastNodeTurns() == 1) && (pathFinder.getLastNodeMP() == 0))
			{
				if (pBestPlot->getRouteType() != NO_ROUTE)
				{
					eMission = MISSION_ROUTE_TO;
				}
			}
			else if (plot()->getRouteType() == NO_ROUTE)
			{
				int iPlotMoveCost = 0;
				iPlotMoveCost = ((plot()->getFeatureType() == NO_FEATURE) ? GC.getTerrainInfo(plot()->getTerrainType()).getMovementCost() : GC.getFeatureInfo(plot()->getFeatureType()).getMovementCost());

				if (plot()->isHills())
				{
					iPlotMoveCost += GC.getHILLS_EXTRA_MOVEMENT();
				}
				if (iPlotMoveCost > 1)
				{
					eMission = MISSION_ROUTE_TO;
				}
			}
		}

		eBestBuild = AI_betterPlotBuild(record, pBestPlot, eBestBuild);

		group.pushPathingMission(record, pathFinder, eMission, getPlotCoord(*pBestPlot), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		group.pushGenericMission(record, pathFinder, MISSION_BUILD, eBestBuild, -1, 0, (group.getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

		return true;
	}

	return false;
}

bool DryRunUnitState::AI_bestCityBuild(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvCity* pCity, const CvPlot** ppBestPlot, BuildTypes* peBestBuild, const CvPlot* pIgnorePlot, const CvUnit* pUnit)
{
	BuildTypes eBuild;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = nullptr;


	for (int iPass = 0; iPass < 2; iPass++)
	{
		for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
		{
			CvPlot* pLoopPlot = plotCity(pCity->getX_INLINE(), pCity->getY_INLINE(), iI);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot != pIgnorePlot)
					{
						if ((pLoopPlot->getImprovementType() == NO_IMPROVEMENT) || !(GET_PLAYER(getOwner()).isOption(PLAYEROPTION_SAFE_AUTOMATION) && !(pLoopPlot->getImprovementType() == (GC.getDefineINT("RUINS_IMPROVEMENT")))))
						{
							iValue = pCity->AI_getBestBuildValue(iI);

							if (iValue > iBestValue)
							{
								eBuild = pCity->AI_getBestBuild(iI);
								FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

								if (eBuild != NO_BUILD)
								{
									if (0 == iPass)
									{
										iBestValue = iValue;
										pBestPlot = pLoopPlot;
										eBestBuild = eBuild;
									}
									else if (canBuild(pLoopPlot, eBuild))
									{
										if (!record.isVisibleEnemyUnit(pLoopPlot, liveUnit))
										{
											int iPathTurns;
											if (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns))
											{
												// XXX take advantage of range (warning... this could lead to some units doing nothing...)
												int iMaxWorkers = 1;
												if (pathFinder.getLastNodeMP() == 0)
												{
													iPathTurns++;
												}
												else if (iPathTurns <= 1)
												{
													iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBuild);
												}
												if (pUnit != nullptr)
												{
													if (pUnit->plot()->isCity() && iPathTurns == 1 && pathFinder.getLastNodeMP() > 0)
													{
														iMaxWorkers += 10;
													}
												}
												if (record.read_AI_plotTargetMissionAIs(getOwner(), pLoopPlot, MISSIONAI_BUILD, group.liveGroup) < iMaxWorkers)
												{
													//XXX this could be improved greatly by
													//looking at the real build time and other factors
													//when deciding whether to stack.
													iValue /= iPathTurns;

													iBestValue = iValue;
													pBestPlot = pLoopPlot;
													eBestBuild = eBuild;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (0 == iPass)
		{
			if (eBestBuild != NO_BUILD)
			{
				FAssert(pBestPlot != nullptr);
				int iPathTurns;
				if ((group.generatePath(record, pathFinder, pBestPlot, 0, true, &iPathTurns)) && canBuild(pBestPlot, eBestBuild)
					&& !record.isVisibleEnemyUnit(pBestPlot, liveUnit))
				{
					int iMaxWorkers = 1;
					if (pUnit != nullptr)
					{
						if (pUnit->plot()->isCity())
						{
							iMaxWorkers += 10;
						}
					}
					if (pathFinder.getLastNodeMP() == 0)
					{
						iPathTurns++;
					}
					else if (iPathTurns <= 1)
					{
						iMaxWorkers = AI_calculatePlotWorkersNeeded(pBestPlot, eBestBuild);
					}
					int iWorkerCount = record.read_AI_plotTargetMissionAIs(getOwner(), pBestPlot, MISSIONAI_BUILD, group.liveGroup);
					if (iWorkerCount < iMaxWorkers)
					{
						//Good to go.
						break;
					}
				}
				eBestBuild = NO_BUILD;
				iBestValue = 0;
			}
		}
	}

	if (NO_BUILD != eBestBuild)
	{
		FAssert(nullptr != pBestPlot);
		if (ppBestPlot != nullptr)
		{
			*ppBestPlot = pBestPlot;
		}
		if (peBestBuild != nullptr)
		{
			*peBestBuild = eBestBuild;
		}
	}


	return (NO_BUILD != eBestBuild);
}

bool DryRunUnitState::AI_heal(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iDamagePercent, int iMaxPath)
{
	std::vector<DryRunUnitState*> aeDamagedUnits;
	[[maybe_unused]] int iTotalDamage;
	[[maybe_unused]] int iTotalHitpoints;
	int iHurtUnitCount;
	[[maybe_unused]] bool bRetreat;

	if (plot()->getFeatureType() != NO_FEATURE)
	{
		if (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() != 0)
		{
			//Pass through
			//(actively seeking a safe spot may result in unit getting stuck)
			return false;
		}
	}

	if (iDamagePercent == 0)
	{
		iDamagePercent = 10;
	}

	bRetreat = false;

	if (group.getNumUnits() == 1)
	{
		if (liveUnit.getDamage() > 0)
		{

			if (plot()->isCity() || (healTurns(record, plot()) == 1))
			{
				if (!(isAlwaysHeal()))
				{
					group.pushGenericMission(record, pathFinder, MISSION_HEAL);
					return true;
				}
			}
		}
		return false;
	}

	iMaxPath = std::min(iMaxPath, 2);

	iTotalDamage = 0;
	iTotalHitpoints = 0;
	iHurtUnitCount = 0;
	for (DryRunUnitState& pLoopUnit : group.units)
	{
		int iDamageThreshold = (pLoopUnit.liveUnit.maxHitPoints() * iDamagePercent) / 100;

		if (NO_UNIT != liveUnit.getLeaderUnitType())
		{
			iDamageThreshold /= 2;
		}

		if (pLoopUnit.liveUnit.getDamage() > 0)
		{
			iHurtUnitCount++;
		}
		iTotalDamage += pLoopUnit.liveUnit.getDamage();
		iTotalHitpoints += pLoopUnit.liveUnit.maxHitPoints();


		if (pLoopUnit.liveUnit.getDamage() > iDamageThreshold)
		{
			bRetreat = true;

			if (!(pLoopUnit.hasMoved()))
			{
				if (!(pLoopUnit.isAlwaysHeal()))
				{
					if (pLoopUnit.healTurns(record, pLoopUnit.plot()) <= iMaxPath)
					{
						aeDamagedUnits.push_back(&pLoopUnit);
					}
				}
			}
		}
	}
	if (iHurtUnitCount == 0)
	{
		return false;
	}

	bool bPushedMission = false;
	if (plot()->isCity() && (plot()->getOwner() == getOwner()))
	{
		FAssertMsg(((int)aeDamagedUnits.size()) <= iHurtUnitCount, "damaged units array is larger than our hurt unit count");

		for (unsigned int iI = 0; iI < aeDamagedUnits.size(); iI++)
		{
			DryRunUnitState& pUnitToHeal = *aeDamagedUnits[iI];
			//pUnitToHeal.joinGroup(nullptr);
			//pUnitToHeal.group.pushGenericMission(record, pathFinder, MISSION_HEAL);
			// Not implemented.
			record.setUseSerialFallback(std::string(__func__) + " Group split");

			// note, removing the head unit from a group will force the group to be completely split if non-human
			if (&pUnitToHeal == this)
			{
				bPushedMission = true;
			}

			iHurtUnitCount--;
		}
	}

	if ((iHurtUnitCount * 2) > group.getNumUnits())
	{
		FAssertMsg(group.getNumUnits() > 0, "group now has zero units");

		if (AI_moveIntoCity(record, pathFinder, group, 2))
		{
			return true;
		}
		else if (healRate(record, plot()) > 10)
		{
			group.pushGenericMission(record, pathFinder, MISSION_HEAL);
			return true;
		}
	}

	return bPushedMission;
}

bool DryRunUnitState::AI_moveIntoCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange)
{
	const CvPlot* pLoopPlot;
	const CvPlot* pBestPlot;
	int iSearchRange = iRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	FAssert(canMove());

	iBestValue = 0;
	pBestPlot = nullptr;

	if (plot()->isCity())
	{
		return false;
	}

	iSearchRange = AI_searchRange(iRange);

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot) && (!isEnemy(pLoopPlot->getTeam(), pLoopPlot)))
				{
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true)))
					{
						if (canMoveInto(record, group, pLoopPlot, false) && (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= 1)))
						{
							iValue = 1;
							if (pLoopPlot->getPlotCity() != nullptr)
							{
								iValue += pLoopPlot->getPlotCity()->getPopulation();
							}

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pathFinder.getPathEndTurnPlot();
								FAssert(!atPlot(pBestPlot));
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot));
		return true;
	}

	return false;
}

bool DryRunUnitState::AI_patrol(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	const CvPlot* pAdjacentPlot;
	const CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		pAdjacentPlot = plotDirection(coord.x, coord.y, ((DirectionTypes)iI));

		if (pAdjacentPlot != nullptr)
		{
			if (AI_plotValid(pAdjacentPlot))
			{
				if (!record.isVisibleEnemyUnit(pAdjacentPlot, liveUnit))
				{
					if (group.generatePath(record, pathFinder, pAdjacentPlot, 0, true))
					{
						if (!canMoveOrAttackInto(record, group, pathFinder.getNextStepPlot()))
						{
							std::osyncstream(std::clog)
								<< "Pathfinder is wrong. Cannot move onto the next plot!"
								<< " From " << getPlotCoord(*plot())
								<< " To " << (pathFinder.getNextStepPlot() ? getPlotCoord(*pathFinder.getNextStepPlot()) : i16vec2(-1))
								<< std::endl;
						}
						assert(canMoveOrAttackInto(record, group, pathFinder.getNextStepPlot()));
						iValue = (1 + record.getRand(10000, "AI Patrol"));

						if (isBarbarian())
						{
							if (!(pAdjacentPlot->isOwned()))
							{
								iValue += 20000;
							}

							if (!(pAdjacentPlot->isAdjacentOwned()))
							{
								iValue += 10000;
							}
						}
						else
						{
							if (pAdjacentPlot->isRevealedGoody(getTeam()))
							{
								iValue += 100000;
							}

							if (pAdjacentPlot->getOwner() == getOwner())
							{
								iValue += 10000;
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pathFinder.getPathEndTurnPlot();
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot));
		return true;
	}

	return false;
}

bool DryRunUnitState::AI_pillageRange(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iBonusValueThreshold)
{
	const CvPlot* pLoopPlot;
	const CvPlot* pBestPlot;
	const CvPlot* pBestPillagePlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	iSearchRange = AI_searchRange(iRange);

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestPillagePlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot) && !(pLoopPlot->isBarbarian())) // Interesting, barbarian plots will never be pillaged by AI?
				{
					if (liveUnit.potentialWarAction(pLoopPlot))
					{
						if (const CvCity* pWorkingCity = pLoopPlot->getWorkingCity())
						{
							if (pWorkingCity != area()->getTargetCity(getOwner()) && canPillage(pLoopPlot))
							{
								if (!record.isVisibleEnemyUnit(pLoopPlot, liveUnit))
								{
									//if (GET_PLAYER(getOwner()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup()) == 0)
									if (record.read_AI_plotTargetMissionAIs(getOwner(), pLoopPlot, MISSIONAI_PILLAGE, group.liveGroup) == 0)
									{
										if (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns))
										{
											if (pathFinder.getLastNodeMP() == 0)
											{
												iPathTurns++;
											}

											if (iPathTurns <= iRange)
											{
												iValue = AI_pillageValue(record, pLoopPlot, iBonusValueThreshold);

												iValue *= 1000;

												iValue /= (iPathTurns + 1);

												// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
												// (because declaring war will pop us some unknown distance away)
												if (!isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam())
												{
													iValue /= 10;
												}

												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													pBestPlot = pathFinder.getPathEndTurnPlot();
													pBestPillagePlot = pLoopPlot;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestPillagePlot != nullptr))
	{
		if (atPlot(pBestPillagePlot) && !isEnemy(pBestPillagePlot->getTeam()))
		{
			//getGroup()->groupDeclareWar(pBestPillagePlot, true);
			// rather than declare war, just find something else to do, since we may already be deep in enemy territory
			return false;
		}

		if (atPlot(pBestPillagePlot))
		{
			if (isEnemy(pBestPillagePlot->getTeam()))
			{
				group.pushGenericMission(record, pathFinder, MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot), 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		}
	}

	return false;
}

int DryRunUnitState::AI_pillageValue(GroupUpdateRecord& record, const CvPlot* pPlot, int iBonusValueThreshold) const
{
	const CvPlot* pAdjacentPlot;
	ImprovementTypes eImprovement;
	BonusTypes eNonObsoleteBonus;
	int iValue;
	int iTempValue;
	int iBonusValue;
	int iI;

	//FAssert(canPillage(pPlot) || canAirBombAt(plot(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) || (getGroup()->getCargo() > 0));

	if (!(pPlot->isOwned()))
	{
		return 0;
	}

	iBonusValue = 0;
	eNonObsoleteBonus = pPlot->getNonObsoleteBonusType(pPlot->getTeam());
	if (eNonObsoleteBonus != NO_BONUS)
	{
		iBonusValue = (GET_PLAYER(pPlot->getOwner()).AI_bonusVal(eNonObsoleteBonus));
	}

	if (iBonusValueThreshold > 0)
	{
		if (eNonObsoleteBonus == NO_BONUS)
		{
			return 0;
		}
		else if (iBonusValue < iBonusValueThreshold)
		{
			return 0;
		}
	}

	iValue = 0;

	if (getDomainType() != DOMAIN_AIR)
	{
		if (pPlot->isRoute())
		{
			iValue++;
			if (eNonObsoleteBonus != NO_BONUS)
			{
				iValue += iBonusValue * 4;
			}

			for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			{
				pAdjacentPlot = plotDirection(coord.x, coord.y, ((DirectionTypes)iI));

				if (pAdjacentPlot != nullptr && pAdjacentPlot->getTeam() == pPlot->getTeam())
				{
					if (pAdjacentPlot->isCity())
					{
						iValue += 10;
					}

					if (!(pAdjacentPlot->isRoute()))
					{
						if (!(pAdjacentPlot->isWater()) && !(pAdjacentPlot->isImpassable()))
						{
							iValue += 2;
						}
					}
				}
			}
		}
	}

	if (pPlot->getImprovementDuration() > ((pPlot->isWater()) ? 20 : 5))
	{
		eImprovement = pPlot->getImprovementType();
	}
	else
	{
		record.setUseSerialFallback(std::string(__func__) + " Dependency on plot reveal state.");
		return 0;
	}

	if (eImprovement != NO_IMPROVEMENT)
	{
		if (pPlot->getWorkingCity() != nullptr)
		{
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, pPlot->getOwner()) * 5);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_PRODUCTION, pPlot->getOwner()) * 4);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_COMMERCE, pPlot->getOwner()) * 3);
		}

		if (getDomainType() != DOMAIN_AIR)
		{
			iValue += GC.getImprovementInfo(eImprovement).getPillageGold();
		}

		if (eNonObsoleteBonus != NO_BONUS)
		{
			if (GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
			{
				iTempValue = iBonusValue * 4;

				if (pPlot->isConnectedToCapital() && (pPlot->getPlotGroupConnectedBonus(pPlot->getOwner(), eNonObsoleteBonus) == 1))
				{
					iTempValue *= 2;
				}

				iValue += iTempValue;
			}
		}
	}

	return iValue;
}

bool DryRunUnitState::AI_follow(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	if (AI_followBombard(record, pathFinder, group))
	{
		return true;
	}

	if (AI_cityAttack(record, pathFinder, group, 1, 65, true))
	{
		return true;
	}

	if (isEnemy(plot()->getTeam()))
	{
		if (canPillage(plot()))
		{
			group.pushGenericMission(record, pathFinder, MISSION_PILLAGE);
			return true;
		}
	}

	if (AI_anyAttack(record, pathFinder, group, 1, 70, 2, true))
	{
		return true;
	}

	if (liveUnit.isFound())
	{
		if (area()->getBestFoundValue(getOwner()) > 0)
		{
			record.setUseSerialFallback(std::string(__func__) + " founding not implemented");
			return true;

			//if (AI_foundRange(FOUND_RANGE, true))
			//{
			//	return true;
			//}
		}
	}

	return false;
}

// Returns true if a mission was pushed or we should wait for another unit to bombard...
bool DryRunUnitState::AI_followBombard(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	CvPlot* pAdjacentPlot1;
	CvPlot* pAdjacentPlot2;
	int iI, iJ;

	if (canBombard(plot()))
	{
		group.pushGenericMission(record, pathFinder, MISSION_BOMBARD);
		return true;
	}

	if (getDomainType() == DOMAIN_LAND)
	{
		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			pAdjacentPlot1 = plotDirection(coord.x, coord.y, ((DirectionTypes)iI));

			if (pAdjacentPlot1 && pAdjacentPlot1->isCity())
			{
				if (AI_potentialEnemy(pAdjacentPlot1->getTeam(), pAdjacentPlot1))
				{
					for (iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
					{
						pAdjacentPlot2 = plotDirection(pAdjacentPlot1->getX_INLINE(), pAdjacentPlot1->getY_INLINE(), ((DirectionTypes)iJ));

						if (pAdjacentPlot2 != nullptr)
						{
							record.setUseSerialFallback(std::string(__func__) + " Depending on our own unit lists here.");
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool DryRunUnitState::AI_cityAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iOddsThreshold, bool bFollow)
{
	const CvPlot* pLoopPlot;
	const CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	FAssert(canMove());

	if (bFollow)
	{
		iSearchRange = 1;
	}
	else
	{
		iSearchRange = AI_searchRange(iRange);
	}

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true, getTeam()) && record.isVisibleEnemyUnit(pLoopPlot, liveUnit)))
					{
						if (AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
						{
							if (!atPlot(pLoopPlot) && ((bFollow) ? canMoveInto(record, group, pLoopPlot, true) : (group.generatePath(record, pathFinder, pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange))))
							{
								iValue = group.AI_attackOdds(pLoopPlot, true);

								if (iValue >= AI_finalOddsThreshold(record, pLoopPlot, iOddsThreshold))
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = ((bFollow) ? pLoopPlot : pathFinder.getPathEndTurnPlot());
										FAssert(!atPlot(pBestPlot));
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		group.pushPathingMission(record, pathFinder, MISSION_MOVE_TO, getPlotCoord(*pBestPlot), ((bFollow) ? MOVE_DIRECT_ATTACK : 0));
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool DryRunUnitState::AI_guardCityBestDefender(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	const CvCity* pCity;
	const CvPlot* pPlot;

	pPlot = plot();
	pCity = pPlot->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwner() == getOwner())
		{
			record.addDependencyOnOurUnitMovementAt(pPlot);
			if (pPlot->getBestDefender(getOwner()) == &liveUnit)
			{
				group.pushGenericMission(record, pathFinder, MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
		}
	}

	return false;
}

bool DryRunUnitState::AI_guardCityMinDefender(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bSearch)
{
	CvCity* pPlotCity = plot()->getPlotCity();
	if ((pPlotCity != nullptr) && (pPlotCity->getOwner() == getOwner()))
	{
		record.addDependencyOnOurUnitMovementAt(pPlotCity->plot());
		int iCityDefenderCount = pPlotCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwner());
		if ((iCityDefenderCount - 1) < pPlotCity->AI_minDefenders())
		{
			if ((iCityDefenderCount <= 2) || (record.getRand(5, "AI shuffle defender") != 0))
			{
				group.pushGenericMission(record, pathFinder, MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
		}
	}

	if (bSearch)
	{
		int iBestValue = 0;
		const CvPlot* pBestPlot = nullptr;
		const CvPlot* pBestGuardPlot = nullptr;

		const CvCity* pLoopCity;
		int iLoop;

		int iCurrentTurn = GC.getGame().getGameTurn();
		for (pLoopCity = GET_PLAYER(getOwner()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwner()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				record.addDependencyOnOurUnitMovementAt(pLoopCity->plot());
				int iDefendersHave = pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwner());
				int iDefendersNeed = pLoopCity->AI_minDefenders();
				if (iDefendersHave < iDefendersNeed)
				{
					//if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
					if (!record.isVisibleEnemyUnit(pLoopCity->plot(), liveUnit))
					{
						//iDefendersHave += GET_PLAYER(getOwner()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_CITY, getGroup());
						iDefendersHave += record.read_AI_plotTargetMissionAIs(getOwner(), pLoopCity->plot(), MISSIONAI_GUARD_CITY, group.liveGroup);
						if (iDefendersHave < iDefendersNeed + 1)
						{
							int iPathTurns;
							if (!atPlot(pLoopCity->plot()) && group.generatePath(record, pathFinder, pLoopCity->plot(), 0, true, &iPathTurns))
							{
								if (iPathTurns <= 10)
								{
									int iValue = (iDefendersNeed - iDefendersHave) * 20;
									iValue += 2 * std::min(15, iCurrentTurn - pLoopCity->getGameTurnAcquired());
									if (pLoopCity->isOccupation())
									{
										iValue += 5;
									}
									iValue -= iPathTurns;

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = pathFinder.getPathEndTurnPlot();
										pBestGuardPlot = pLoopCity->plot();
									}
								}
							}
						}
					}
				}
			}
		}
		if (pBestPlot != nullptr)
		{
			if (atPlot(pBestGuardPlot))
			{
				FAssert(pBestGuardPlot == pBestPlot);
				group.pushGenericMission(record, pathFinder, MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
			FAssert(!atPlot(pBestPlot));
			group.pushGenericMission(record, pathFinder, MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

bool DryRunUnitState::AI_canGroupWithAIType(UnitAITypes eUnitAI) const
{
	if (eUnitAI != AI_getUnitAIType())
	{
		switch (eUnitAI)
		{
		case (UNITAI_ATTACK_CITY):
			if (plot()->isCity() && (GC.getGame().getGameTurn() - plot()->getPlotCity()->getGameTurnAcquired()) <= 1)
			{
				return false;
			}
			break;
		default:
			break;
		}
	}

	return true;
}

bool DryRunUnitState::AI_allowGroup(GroupUpdateRecord& record, DryRunGroupState& group, const CvUnit* pUnit, UnitAITypes eUnitAI) const
{
	CvSelectionGroup* pGroup = pUnit->getGroup();
	CvPlot* pPlot = pUnit->plot();

	if (pUnit == &liveUnit)
	{
		return false;
	}

	if (!pUnit->isGroupHead())
	{
		return false;
	}

	if (pGroup == liveUnit.getGroup())
	{
		return false;
	}

	if (pUnit->isCargo())
	{
		return false;
	}

	if (pUnit->AI_getUnitAIType() != eUnitAI)
	{
		return false;
	}

	record.addReadDependencyOnGroup(pGroup, GroupUpdateRecord::SelectionGroupAccess::kMissionAI);
	switch (pGroup->AI_getMissionAIType())
	{
	case MISSIONAI_GUARD_CITY:
		// do not join groups that are guarding cities
		// intentional fallthrough
	case MISSIONAI_LOAD_SETTLER:
	case MISSIONAI_LOAD_ASSAULT:
	case MISSIONAI_LOAD_SPECIAL:
		// do not join groups that are loading into transports (we might not fit and get stuck in loop forever)
		return false;
		break;
	default:
		break;
	}

	record.addReadDependencyOnGroup(pGroup, GroupUpdateRecord::SelectionGroupAccess::kActivity);
	if (pGroup->getActivityType() == ACTIVITY_HEAL)
	{
		// do not attempt to join groups which are healing this turn
		// (healing is cleared every turn for automated groups, so we know we pushed a heal this turn)
		return false;
	}

	if (!canJoinGroup(record, pPlot, pGroup))
	{
		return false;
	}

	if (eUnitAI == UNITAI_SETTLE)
	{
		if (record.AI_getPlotDanger(getOwner(), pPlot, 3) > 0)
		{
			return false;
		}
	}
	else if (eUnitAI == UNITAI_ASSAULT_SEA)
	{
		if (!pGroup->hasCargo())
		{
			return false;
		}
	}

	if (group.getHeadUnitAI() == UNITAI_CITY_DEFENSE)
	{
		if (plot()->isCity() && (plot()->getTeam() == getTeam()) &&
			(record.addDependencyOnOurUnitMovementAt(plot()), plot()->getBestDefender(getOwner())->getGroup() == liveUnit.getGroup()))
		{
			return false;
		}
	}

	if (plot()->getOwnerINLINE() == getOwner())
	{
		record.addReadDependencyOnGroup(pGroup, GroupUpdateRecord::SelectionGroupAccess::kMissionXY);
		CvPlot* pTargetPlot = pGroup->AI_getMissionAIPlot();

		if (pTargetPlot != nullptr)
		{
			if (pTargetPlot->isOwned())
			{
				if (isPotentialEnemy(pTargetPlot->getTeam(), pTargetPlot))
				{
					//Do not join groups which have debarked on an offensive mission
					return false;
				}
			}
		}
	}

	if (pUnit->getInvisibleType() != NO_INVISIBLE)
	{
		if (liveUnit.getInvisibleType() == NO_INVISIBLE)
		{
			return false;
		}
	}

	return true;
}

bool DryRunUnitState::canJoinGroup(GroupUpdateRecord& record, const CvPlot* pPlot, CvSelectionGroup* pSelectionGroup) const
{
	CvUnit* pHeadUnit;

	// do not allow someone to join a group that is about to be split apart
	// this prevents a case of a never-ending turn
	if (pSelectionGroup->AI_isForceSeparate())
	{
		return false;
	}

	if (pSelectionGroup->getOwnerINLINE() == NO_PLAYER)
	{
		pHeadUnit = pSelectionGroup->getHeadUnit();

		if (pHeadUnit != nullptr)
		{
			if (pHeadUnit->getOwnerINLINE() != getOwner())
			{
				return false;
			}
		}
	}
	else
	{
		if (pSelectionGroup->getOwnerINLINE() != getOwner())
		{
			return false;
		}
	}

	if (pSelectionGroup->getNumUnits() > 0)
	{
		record.addDependencyOnUnitMovement(getOwner(), pSelectionGroup->getHeadUnit());
		if (!(pSelectionGroup->atPlot(pPlot)))
		{
			return false;
		}

		if (pSelectionGroup->getDomainType() != getDomainType())
		{
			return false;
		}
	}

	return true;
}

bool DryRunUnitState::AI_group(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group,
	UnitAITypes eUnitAI, int iMaxGroup, int iMaxOwnUnitAI, int iMinUnitAI, bool bIgnoreFaster, bool bIgnoreOwnUnitType, bool bStackOfDoom, int iMaxPath, bool bAllowRegrouping)
{
	const CvUnit* pBestUnit;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	// if we are on a transport, then do not regroup
	if (isCargo())
	{
		return false;
	}

	if (!bAllowRegrouping)
	{
		if (group.getNumUnits() > 1)
		{
			return false;
		}
	}

	if ((getDomainType() == DOMAIN_LAND) && !canMoveAllTerrain())
	{
		if (area()->getNumAIUnits(getOwner(), eUnitAI) == 0)
		{
			return false;
		}
	}

	if (!AI_canGroupWithAIType(eUnitAI))
	{
		return false;
	}

	iBestValue = INT_MAX;
	pBestUnit = nullptr;

	const auto processUnit = [&](const CvUnit* pLoopUnit) {
		// Start with all the conditions that don't depend on unit position.
		if (AI_allowGroup(record, group, pLoopUnit, eUnitAI))
		{
			if (!bIgnoreOwnUnitType || (pLoopUnit->getUnitType() != liveUnit.getUnitType()))
			{
				if (!bIgnoreFaster || (pLoopUnit->getGroup()->baseMoves() <= baseMoves()))
				{
					CvSelectionGroup* pLoopGroup = pLoopUnit->getGroup();
					if ((iMaxOwnUnitAI == -1) || (pLoopGroup->countNumUnitAIType(AI_getUnitAIType()) <= (iMaxOwnUnitAI + ((bStackOfDoom) ? AI_stackOfDoomExtra() : 0))))
					{
						if ((iMinUnitAI == -1) || (pLoopGroup->countNumUnitAIType(eUnitAI) >= iMinUnitAI))
						{
							if ((iMaxGroup == -1) || ((pLoopGroup->getNumUnits()
								+ record.read_AI_unitTargetMissionAIs(getOwner(), pLoopUnit, std::array{ MISSIONAI_GROUP }, liveUnit.getGroup())) <= (iMaxGroup + ((bStackOfDoom) ? AI_stackOfDoomExtra() : 0))))
							{
								// Reading unit position now.
								const CvPlot* pPlot = pLoopUnit->plot();
								if (AI_plotValid(pPlot))
								{
									if (iMaxPath > 0 || pPlot == plot())
									{
										if (!isEnemy(pPlot->getTeam()))
										{
											if (!record.isVisibleEnemyUnit(pPlot, liveUnit))
											{
												if (group.generatePath(record, pathFinder, pPlot, PathingArg(0, iMaxPath), true, &iPathTurns))
												{
													// Okay, take the dependency on unit position, but only if the unit can possibly be within iMaxPath turns *after* it has moved.
													// So just add a few extra turns and hope for the best.
													if (iPathTurns - 2 <= iMaxPath)
													{
														record.addDependencyOnUnitMovement(getOwner(), pLoopUnit);

														if (iPathTurns <= iMaxPath)
														{
															iValue = 1000 * (iPathTurns + 1);
															iValue *= 4 + pLoopGroup->getCargo();
															iValue /= pLoopGroup->getNumUnits();


															if (iValue < iBestValue)
															{
																iBestValue = iValue;
																pBestUnit = pLoopUnit;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		};

	for (const CvUnit* pLoopUnit = GET_PLAYER(getOwner()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwner()).nextUnit(&iLoop))
		processUnit(pLoopUnit);

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			record.setUseSerialFallback("AI_group joinGroup");
			return true;
		}
		else
		{
			group.pushGenericMission(record, pathFinder, MISSION_MOVE_TO_UNIT, pBestUnit->getOwner(), pBestUnit->getID(), 0, false, false, MISSIONAI_GROUP, nullptr, pBestUnit);
			return true;
		}
	}

	return false;
}

bool DryRunUnitState::AI_guardFort(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bSearch)
{
	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		ImprovementTypes eImprovement = plot()->getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			if (GC.getImprovementInfo(eImprovement).isActsAsCity())
			{
				if (plot()->plotCount(PUF_isCityAIType, -1, -1, getOwnerINLINE()) <= AI_getPlotDefendersNeeded(plot(), 0))
				{
					group.pushGenericMission(record, pathFinder, MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, plot());
					return true;
				}
			}
		}
	}

	if (!bSearch)
	{
		return false;
	}

	int iBestValue = 0;
	const CvPlot* pBestPlot = nullptr;
	const CvPlot* pBestGuardPlot = nullptr;

	const CvMap& map = GC.getMapINLINE();
	std::mutex mutex;
	std::vector<int> fortPlotIndices;

	heck::parallelForEachN(map.numPlotsINLINE(), [&record, &group, &map, &mutex, &fortPlotIndices, this](int iI) {
		const CvPlot* pLoopPlot = map.plotByIndexINLINE(iI);
		if (AI_plotValid(pLoopPlot) && !atPlot(pLoopPlot))
		{
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
			{
				ImprovementTypes eImprovement = pLoopPlot->getImprovementType();
				if (eImprovement != NO_IMPROVEMENT)
				{
					if (GC.getImprovementInfo(eImprovement).isActsAsCity())
					{
						int iValue = AI_getPlotDefendersNeeded(pLoopPlot, 0);

						if (iValue > 0)
						{
							//if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							if (!record.isVisibleEnemyUnit(pLoopPlot, liveUnit))
							{
								//if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) < iValue)
								if (record.read_AI_plotTargetMissionAIs(getOwnerINLINE(), pLoopPlot, MISSIONAI_GUARD_BONUS, group.liveGroup) < iValue)
								{
									const std::lock_guard lock(mutex);
									fortPlotIndices.push_back(iI);
								}
							}
						}
					}
				}
			}
		}
		});

	// Sort by distance.
	std::ranges::sort(fortPlotIndices, std::less<>(), [&map, this](int iI) {
		const CvPlot& plot = *map.plotByIndexINLINE(iI);
		return std::pair(::stepDistance(coord.x, coord.y, plot.getX_INLINE(), plot.getY_INLINE()), iI);
		});

	for (const int iI : fortPlotIndices)
	{
		const CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		// iBestValue < AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / (iPathTurns + 2)
		// (iPathTurns + 2) < AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue
		// iPathTurns < AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue - 2
		// iPathTurns <= floor(AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue) - 2

		const int baseValue = AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000;
		const int maxTurns = iBestValue > 0 ? baseValue / iBestValue - 2 : INT_MAX;

		int iPathTurns{};
		if (group.generatePath(record, pathFinder, pLoopPlot, PathingArg(0, maxTurns), true, &iPathTurns))
		{
			const int iValue = baseValue / (iPathTurns + 2);

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				pBestPlot = pathFinder.getPathEndTurnPlot();
				pBestGuardPlot = pLoopPlot;
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
	{
		if (atPlot(pBestGuardPlot))
		{
			group.pushGenericMission(record, pathFinder, MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			group.pushGenericMission(record, pathFinder, MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

bool DryRunUnitState::AI_guardCityAirlift(GroupUpdateRecord& record, DryRunGroupState& group)
{
	const CvCity* pCity;
	//CvCity* pLoopCity;
	//CvPlot* pBestPlot;
	//int iValue;
	//int iBestValue;
	//int iLoop;

	if (group.getNumUnits() > 1)
	{
		return false;
	}

	pCity = plot()->getPlotCity();

	if (pCity == nullptr)
	{
		return false;
	}

	if (pCity->getMaxAirlift() == 0)
	{
		return false;
	}

	record.setUseSerialFallback("AI_guardCityAirlift unimplemented stuff.");

	return true;
}

bool DryRunUnitState::AI_safety(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group)
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	CvUnit* pHeadUnit;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iCount;
	int iPass;
	int iDX, iDY;

	iSearchRange = AI_searchRange(1);

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iPass = 0; iPass < 2; iPass++)
	{
		for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
		{
			for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
			{
				pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					if (AI_plotValid(pLoopPlot))
					{
						if (!record.isVisibleEnemyUnit(pLoopPlot, liveUnit))
						{
							if (group.generatePath(record, pathFinder, pLoopPlot, PathingArg((iPass > 0) ? MOVE_IGNORE_DANGER : 0, 2), true, &iPathTurns)
								&& iPathTurns <= 1)
							{
								iCount = 0;

								record.addDependencyOnOurUnitMovementAt(pLoopPlot);

								pUnitNode = pLoopPlot->headUnitNode();

								while (pUnitNode != nullptr)
								{
									pLoopUnit = ::getUnit(pUnitNode->m_data);
									pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

									if (pLoopUnit->getOwnerINLINE() == getOwnerINLINE())
									{
										if (pLoopUnit->canDefend())
										{
											pHeadUnit = pLoopUnit->getGroup()->getHeadUnit();
											FAssert(pHeadUnit != nullptr);
											FAssert(group.getHeadUnit() == this);

											if (pHeadUnit != &liveUnit)
											{
												if (pHeadUnit->isWaiting() || !(pHeadUnit->canMove()))
												{
													FAssert(pLoopUnit != &liveUnit);
													FAssert(pHeadUnit != liveUnit.getGroup()->getHeadUnit());
													iCount++;
												}
											}
										}
									}
								}

								iValue = (iCount * 100);

								iValue += pLoopPlot->defenseModifier(getTeam(), false);

								if (atPlot(pLoopPlot))
								{
									iValue += 50;
								}
								else
								{
									iValue += record.getRand(50, "AI Safety");
								}

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = pLoopPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			group.pushSkipMission(record, pathFinder);
			return true;
		}
		else
		{
			group.pushGenericMission(record, pathFinder, MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((iPass > 0) ? MOVE_IGNORE_DANGER : 0));
			return true;
		}
	}

	return false;
}

#endif