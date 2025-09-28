#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_GroupUpdateRecord.h"
#include "AIParallelUnitUpdate_UnitPostUpdateQueue.h"
#include "GameContext.h"

/// Here, we apply the final results of parallel unit update.
/// But the more complex stuff is deferred to UnitPostUpdateQueue.

void CvSelectionGroupAI::applyParallelUnitUpdateResult(const GiganticMapsOptimisationsLib::DryRunGroupState& result)
{
	m_eActivityType = result.activityType;
	m_iMissionTimer = result.missionTimer;
	m_eMissionAIType = result.m_eMissionAIType;
	m_iMissionAIX = result.m_iMissionAICoord.x;
	m_iMissionAIY = result.m_iMissionAICoord.y;
	m_missionAIUnit = result.m_missionAIUnit;
	m_bGroupAttack = result.m_bGroupAttack;
	m_bForceUpdate = result.forceUpdate;
	m_eAutomateType = result.automateType;
	m_missionQueue.clear();
	for (const MissionData& data : result.m_missionQueue)
		m_missionQueue.insertAtEnd(data);
}

void CvUnitAI::applyParallelUnitUpdateResult(const GiganticMapsOptimisationsLib::DryRunUnitState& result,
	GiganticMapsOptimisationsLib::UnitPostUpdateQueue& postUpdateQueue)
{
	m_iMoves = result.m_iMoves;
	//m_iX = result.coord.x;
	//m_iY = result.coord.y;

	

	const int iX = result.coord.x;
	const int iY = result.coord.y;

	if (!at(iX, iY))
	{
		constexpr bool bUpdate = true;
		constexpr bool bGroup = true;
		constexpr bool bCheckPlotVisible = true;
		bool bShow = true && GC.getMapINLINE().plotINLINE(iX, iY)->isVisibleToWatchingHuman();

		// Now we need to apply the effect of setXY, which can be done in parallel if we're here.
		CvCity* pOldCity;
		CvCity* pNewCity;
		CvCity* pWorkingCity;
		[[maybe_unused]] CvUnit* pTransportUnit;
		CvPlot* pLoopPlot;

		//ActivityTypes eOldActivityType;
		int iI;

		// OOS!! Temporary for Out-of-Sync madness debugging...
		//if (GC.getLogging())
		//{
		//	if (gDLL->getChtLvl() > 0)
		//	{
		//		char szOut[1024];
		//		sprintf(szOut, "Player %d Unit %d (%S's %S) moving from %d:%d to %d:%d\n", getOwnerINLINE(), getID(), GET_PLAYER(getOwnerINLINE()).getNameKey(), getName().GetCString(), getX_INLINE(), getY_INLINE(), iX, iY);
		//		gDLL->messageControlLog(szOut);
		//	}
		//}

		FAssert(!at(iX, iY));
		FAssert(!isFighting());
		FAssert((iX == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getX_INLINE() == iX));
		FAssert((iY == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getY_INLINE() == iY));

		const int oldPositionX = m_iX;
		const int oldPositionY = m_iY;

		//if (getGroup() != nullptr)
		//{
		//	eOldActivityType = getGroup()->getActivityType();
		//}
		//else
		//{
		//	eOldActivityType = NO_ACTIVITY;
		//}

		// No blockading changes allowed!
		FAssert(!isBlockading());
		//setBlockading(false);

		static_assert(bGroup);
		FAssert(!isCargo());
		//if (!bGroup || isCargo())
		//{
		//	joinGroup(nullptr, true);
		//	bShow = false;
		//}

		CvPlot* const pNewPlot = GC.getMapINLINE().plotINLINE(iX, iY);

		if (pNewPlot)
		{
			pTransportUnit = getTransportUnit();

			// No load/unload allowed!
			FAssert(!pTransportUnit || pTransportUnit->atPlot(pNewPlot));
			//if (pTransportUnit != nullptr)
			//{
			//	if (!(pTransportUnit->atPlot(pNewPlot)))
			//	{
			//		setTransportUnit(nullptr);
			//	}
			//}

			//if (canFight())
			//{
			//	CLinkList<IDInfo> oldUnits;
			//	oldUnits.clear();
			//	
			//	pUnitNode = pNewPlot->headUnitNode();
			//	
			//	while (pUnitNode != nullptr)
			//	{
			//		oldUnits.insertAtEnd(pUnitNode->m_data);
			//		pUnitNode = pNewPlot->nextUnitNode(pUnitNode);
			//	}
			//	
			//	pUnitNode = oldUnits.head();
			//	
			//	while (pUnitNode != nullptr)
			//	{
			//		pLoopUnit = ::getUnit(pUnitNode->m_data);
			//		pUnitNode = oldUnits.next(pUnitNode);
			//	
			//		if (pLoopUnit != nullptr)
			//		{
			//			if (isEnemy(pLoopUnit->getTeam(), pNewPlot) || pLoopUnit->isEnemy(getTeam()))
			//			{
			//				// No unit killing or capturing allowed!
			//				FAssert(pLoopUnit->canCoexistWithEnemyUnit(getTeam()));
			//				// if (!pLoopUnit->canCoexistWithEnemyUnit(getTeam()))
			//				// {
			//				// 	if (NO_UNITCLASS == pLoopUnit->getUnitInfo().getUnitCaptureClassType() && pLoopUnit->canDefend(pNewPlot))
			//				// 	{
			//				// 		pLoopUnit->jumpToNearestValidPlot(); // can kill unit
			//				// 	}
			//				// 	else
			//				// 	{
			//				// 		if (!m_pUnitInfo->isHiddenNationality() && !pLoopUnit->getUnitInfo().isHiddenNationality())
			//				// 		{
			//				// 			GET_TEAM(pLoopUnit->getTeam()).changeWarWeariness(getTeam(), *pNewPlot, GC.getDefineINT("WW_UNIT_CAPTURED"));
			//				// 			GET_TEAM(getTeam()).changeWarWeariness(pLoopUnit->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_UNIT"));
			//				// 			GET_TEAM(getTeam()).AI_changeWarSuccess(pLoopUnit->getTeam(), GC.getDefineINT("WAR_SUCCESS_UNIT_CAPTURING"));
			//				// 		}
			//				// 	
			//				// 		if (!isNoCapture())
			//				// 		{
			//				// 			pLoopUnit->setCapturingPlayer(getOwnerINLINE());
			//				// 		}
			//				// 	
			//				// 		pLoopUnit->kill(false, getOwnerINLINE());
			//				// 	}
			//				// }
			//			}
			//		}
			//	}
			//}

			// No goodies allowed!
			FAssert(!pNewPlot->isGoody(getTeam()));
			//if (pNewPlot->isGoody(getTeam()))
			//{
			//	GET_PLAYER(getOwnerINLINE()).doGoody(pNewPlot, this);
			//}
		}

		CvPlot* const pOldPlot = plot();

		FAssert(pOldPlot && pNewPlot && pOldPlot->getArea() == pNewPlot->getArea());

		if (pOldPlot)
		{
			//pOldPlot->removeUnit(this, bUpdate && !hasCargo());
			postUpdateQueue.plotRemoveUnit(pOldPlot, this, bUpdate && !hasCargo());

			//pOldPlot->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);
			postUpdateQueue.changeAdjacentSight(pOldPlot, visibilityRange(), false, this, true);

			// We're not changing area.
			//CvArea& area = *pOldPlot->area();
			//
			//area.changeUnitsPerPlayer(getOwnerINLINE(), -1);
			//area.changePower(getOwnerINLINE(), -(m_pUnitInfo->getPowerValue()));
			//
			//if (AI_getUnitAIType() != NO_UNITAI)
			//{
			//	area.changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), -1);
			//}
			//
			//if (isAnimal())
			//{
			//	area.changeAnimalsPerPlayer(getOwnerINLINE(), -1);
			//}

			if (pOldPlot->getTeam() != getTeam() && (pOldPlot->getTeam() == NO_TEAM || !GET_TEAM(pOldPlot->getTeam()).isVassal(getTeam())))
			{
				//GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(-1);
				postUpdateQueue.changeNumOutsideUnits(-1);
			}

			// This can be done in parallel.
			setLastMoveTurn(GC.getGameINLINE().getTurnSlice());

			pOldCity = pOldPlot->getPlotCity();

			if (pOldCity != nullptr)
			{
				if (isMilitaryHappiness())
				{
					//pOldCity->changeMilitaryHappinessUnits(-1);
					postUpdateQueue.changeMilitaryHappinessUnits(pOldCity, -1);
				}
			}

			pWorkingCity = pOldPlot->getWorkingCity();

			if (pWorkingCity != nullptr)
			{
				if (canSiege(pWorkingCity->getTeam()))
				{
					//pWorkingCity->AI_setAssignWorkDirty(true);
					postUpdateQueue.onSiegingUnitMoveFromToWorkingPlot(pWorkingCity, pOldPlot);
				}
			}

			if (pOldPlot->isWater())
			{
				for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					pLoopPlot = plotDirection(pOldPlot->getX_INLINE(), pOldPlot->getY_INLINE(), ((DirectionTypes)iI));

					if (pLoopPlot != nullptr)
					{
						if (pLoopPlot->isWater())
						{
							pWorkingCity = pLoopPlot->getWorkingCity();

							if (pWorkingCity != nullptr)
							{
								if (canSiege(pWorkingCity->getTeam()))
								{
									// pWorkingCity->AI_setAssignWorkDirty(true);
									postUpdateQueue.onSiegingUnitMoveFromToWorkingPlot(pWorkingCity, pLoopPlot);
								}
							}
						}
					}
				}
			}

			if (pOldPlot->isActiveVisible(true))
			{
				//pNewPlot->updateMinimapColor();
				postUpdateQueue.deferUpdateMinimapColor(pOldPlot);
			}

			if (pOldPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
			{
				// gDLL->getInterfaceIFace()->verifyPlotListColumn();
				// 
				// gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
				postUpdateQueue.deferDirtyPlotList(pOldPlot);
			}
		}

		if (pNewPlot)
		{
			m_iX = pNewPlot->getX_INLINE();
			m_iY = pNewPlot->getY_INLINE();
		}
		else
		{
			m_iX = INVALID_PLOT_COORD;
			m_iY = INVALID_PLOT_COORD;
		}

		FAssertMsg(plot() == pNewPlot, "plot is expected to equal pNewPlot");

		if (pNewPlot)
		{
			pNewCity = pNewPlot->getPlotCity();

			// We are not capturing cities.
			FAssert(!(pNewCity && isEnemy(pNewCity->getTeam()) && !canCoexistWithEnemyUnit(pNewCity->getTeam()) && canFight()));
			//if (pNewCity)
			//{
			//	if (isEnemy(pNewCity->getTeam()) && !canCoexistWithEnemyUnit(pNewCity->getTeam()) && canFight())
			//	{
			//		GET_TEAM(getTeam()).changeWarWeariness(pNewCity->getTeam(), *pNewPlot, GC.getDefineINT("WW_CAPTURED_CITY"));
			//		GET_TEAM(getTeam()).AI_changeWarSuccess(pNewCity->getTeam(), GC.getDefineINT("WAR_SUCCESS_CITY_CAPTURING"));
			//
			//		PlayerTypes eNewOwner = GET_PLAYER(getOwnerINLINE()).pickConqueredCityOwner(*pNewCity);
			//
			//		if (NO_PLAYER != eNewOwner)
			//		{
			//			GET_PLAYER(eNewOwner).acquireCity(pNewCity, true, false, true); // will delete the pointer
			//			pNewCity = nullptr;
			//		}
			//	}
			//}

			//update facing direction
			// This can be done in parallel.
			if (pOldPlot)
			{
				DirectionTypes newDirection = estimateDirection(pOldPlot, pNewPlot);
				if (newDirection != NO_DIRECTION)
					m_eFacingDirection = newDirection;
			}

			// We are not cargo.
			FAssert(!isCargo());
			//if (isCargo())
			//{
			//	if (eOldActivityType != ACTIVITY_MISSION)
			//	{
			//		getGroup()->setActivityType(eOldActivityType);
			//	}
			//}

			// This can be done in parallel.
			setFortifyTurns(0);

			//pNewPlot->changeAdjacentSight(getTeam(), visibilityRange(), true, this, true); // needs to be here so that the square is considered visible when we move into it...
			postUpdateQueue.changeAdjacentSight(pNewPlot, visibilityRange(), true, this, true);

			//pNewPlot->addUnit(this, bUpdate && !hasCargo());
			postUpdateQueue.plotAddUnit(pNewPlot, this, bUpdate && !hasCargo());

			// Areas are the same. We don't need to update these stats.
			//CvArea& area = *pNewPlot->area();
			//area.changeUnitsPerPlayer(getOwnerINLINE(), 1);
			//area.changePower(getOwnerINLINE(), m_pUnitInfo->getPowerValue());
			//
			//if (AI_getUnitAIType() != NO_UNITAI)
			//{
			//	area.changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
			//}
			//
			//if (isAnimal())
			//{
			//	area.changeAnimalsPerPlayer(getOwnerINLINE(), 1);
			//}

			if (pNewPlot->getTeam() != getTeam() && (pNewPlot->getTeam() == NO_TEAM || !GET_TEAM(pNewPlot->getTeam()).isVassal(getTeam())))
			{
				//GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(1);
				postUpdateQueue.changeNumOutsideUnits(1);
			}

			if (shouldLoadOnMove(pNewPlot))
			{
				//load();

				// There should be no loading in the parallel update.
				std::abort();
			}

			for (iI = 0; iI < MAX_CIV_TEAMS; iI++)
			{
				if (GET_TEAM((TeamTypes)iI).isAlive())
				{
					if (!isInvisible(((TeamTypes)iI), false))
					{
						if (pNewPlot->isVisible((TeamTypes)iI, false))
						{
							//GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
							postUpdateQueue.deferTeamMeet((TeamTypes)iI);
						}
					}
				}
			}

			pNewCity = pNewPlot->getPlotCity();

			if (pNewCity != nullptr)
			{
				if (isMilitaryHappiness())
				{
					//pNewCity->changeMilitaryHappinessUnits(1);
					postUpdateQueue.changeMilitaryHappinessUnits(pNewCity, 1);
				}
			}

			pWorkingCity = pNewPlot->getWorkingCity();

			if (pWorkingCity != nullptr)
			{
				if (canSiege(pWorkingCity->getTeam()))
				{
					//pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pNewPlot));
					postUpdateQueue.onSiegingUnitMoveFromToWorkingPlot(pWorkingCity, pNewPlot);
				}
			}

			if (pNewPlot->isWater())
			{
				for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					pLoopPlot = plotDirection(pNewPlot->getX_INLINE(), pNewPlot->getY_INLINE(), ((DirectionTypes)iI));

					if (pLoopPlot != nullptr)
					{
						if (pLoopPlot->isWater())
						{
							pWorkingCity = pLoopPlot->getWorkingCity();

							if (pWorkingCity != nullptr)
							{
								if (canSiege(pWorkingCity->getTeam()))
								{
									//pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pLoopPlot));
									postUpdateQueue.onSiegingUnitMoveFromToWorkingPlot(pWorkingCity, pLoopPlot);
								}
							}
						}
					}
				}
			}

			if (pNewPlot->isActiveVisible(true))
			{
				//pNewPlot->updateMinimapColor();
				postUpdateQueue.deferUpdateMinimapColor(pNewPlot);
			}

			if (GC.IsGraphicsInitialized())
			{
				//override bShow if check plot visible
				if (bCheckPlotVisible && pNewPlot->isVisibleToWatchingHuman())
					bShow = true;

				if (bShow)
				{
					//QueueMove(pNewPlot);
					postUpdateQueue.deferEntityQueueMove(this, pNewPlot);
				}
				else
				{
					//SetPosition(pNewPlot);
					postUpdateQueue.deferEntitySetPosition(this, pNewPlot);
				}
			}

			// Do this later.
			if (pNewPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
			{
				//gDLL->getInterfaceIFace()->verifyPlotListColumn();
				//
				//gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
				postUpdateQueue.deferDirtyPlotList(pNewPlot);
			}
		}

		if (pOldPlot != nullptr)
		{
			// TODO: Not implemented yet.
			if (hasCargo())
				std::abort();
			//if (hasCargo())
			//{
			//	pUnitNode = pOldPlot->headUnitNode();
			//
			//	while (pUnitNode != nullptr)
			//	{
			//		pLoopUnit = ::getUnit(pUnitNode->m_data);
			//		pUnitNode = pOldPlot->nextUnitNode(pUnitNode);
			//
			//		if (pLoopUnit->getTransportUnit() == this)
			//		{
			//			pLoopUnit->setXY(iX, iY, bGroup, false);
			//		}
			//	}
			//}
		}

		if (bUpdate && hasCargo())
		{
			if (pOldPlot != nullptr)
			{
				//pOldPlot->updateCenterUnit();
				//pOldPlot->setFlagDirty(true);
				postUpdateQueue.deferUpdateCenterUnitAndFlagDirty(pOldPlot);
			}

			if (pNewPlot != nullptr)
			{
				//pNewPlot->updateCenterUnit();
				//pNewPlot->setFlagDirty(true);
				postUpdateQueue.deferUpdateCenterUnitAndFlagDirty(pNewPlot);
			}
		}

		FAssert(pOldPlot != pNewPlot);

		// This probably doesn't need to be done for AI, except possibly if the unit isn't in the cycle.
		//GET_PLAYER(getOwnerINLINE()).updateGroupCycle(this);
		postUpdateQueue.deferUpdateGroupCycle(this);

		// Do this later.
		//setInfoBarDirty(true);
		postUpdateQueue.deferSetInfoBarDirty(this);

		// This unit isn't selected because it's AI.
		//if (IsSelected())
		//{
		//	if (isFound())
		//	{
		//		gDLL->getInterfaceIFace()->setDirty(GlobeLayer_DIRTY_BIT, true);
		//		gDLL->getEngineIFace()->updateFoundingBorder();
		//	}
		//
		//	gDLL->getInterfaceIFace()->setDirty(ColoredPlots_DIRTY_BIT, true);
		//}

		// Do this later.
		if (pNewPlot != nullptr)
		{
			//gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
			postUpdateQueue.deferUpdateEnemyGlow(this);
		}

		// NO PYTHON ALLOWED!
		//CvEventReporter::getInstance().unitSetXY(pNewPlot, this);

		gGlobals.enhancedDLLGameContext->onUnitMoved(*this, { oldPositionX, oldPositionY }, { m_iX, m_iY });
	}
}
#endif