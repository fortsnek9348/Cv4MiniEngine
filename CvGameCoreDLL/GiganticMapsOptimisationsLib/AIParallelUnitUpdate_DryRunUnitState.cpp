#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_GroupUpdateRecord.h"
#include "AIParallelUnitUpdate_DryRunGroupState.h"
#include "AIParallelUnitUpdate_DryRunUnitState.h"

#include <syncstream>
#include <iostream>

using namespace GiganticMapsOptimisationsLib;

bool DryRunUnitState::canMoveInto(GroupUpdateRecord& record, const DryRunGroupState& group, const CvPlot* pPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad) const
{
	// If this unit is an animal, then we take dependency on the unit list of the target plot.
	//if (liveUnit.isAnimal())
	//	record.addDependencyOnUnitMovementAt(plot);
	// There might be other dependencies, this is a big function.
	//return liveUnit.canMoveInto(plot, attack, bDeclareWar);

	FAssertMsg(pPlot != nullptr, "Plot is not assigned a valid value");

	if (atPlot(pPlot))
	{
		return false;
	}

	if (pPlot->isImpassable())
	{
		if (!liveUnit.canMoveImpassable())
		{
			return false;
		}
	}

	const CvUnitInfo& unitInfo = liveUnit.getUnitInfo();

	// Cannot move around in unrevealed land freely
	if (unitInfo.isNoRevealMap())// && willRevealByMove(pPlot))
	{
		record.setUseSerialFallback(std::string(__func__) + " isNoRevealMap");
		return false;
	}

	if (GC.getUSE_SPIES_NO_ENTER_BORDERS())
	{
		if (liveUnit.isSpy() && NO_PLAYER != pPlot->getOwnerINLINE())
		{
			if (!GET_PLAYER(getOwner()).canSpiesEnterBorders(pPlot->getOwnerINLINE()))
			{
				return false;
			}
		}
	}

	CvArea *pPlotArea = pPlot->area();
	TeamTypes ePlotTeam = pPlot->getTeam();
	bool bCanEnterArea = liveUnit.canEnterArea(ePlotTeam, pPlotArea);
	if (bCanEnterArea)
	{
		if (pPlot->getFeatureType() != NO_FEATURE)
		{
			if (unitInfo.getFeatureImpassable(pPlot->getFeatureType()))
			{
				TechTypes eTech = (TechTypes)unitInfo.getFeaturePassableTech(pPlot->getFeatureType());
				if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
				{
					if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
					{
						return false;
					}
				}
			}
		}
		else
		{
			if (unitInfo.getTerrainImpassable(pPlot->getTerrainType()))
			{
				TechTypes eTech = (TechTypes)unitInfo.getTerrainPassableTech(pPlot->getTerrainType());
				if (NO_TECH == eTech || !GET_TEAM(getTeam()).isHasTech(eTech))
				{
					if (DOMAIN_SEA != getDomainType() || pPlot->getTeam() != getTeam())  // sea units can enter impassable in own cultural borders
					{
						//if (bIgnoreLoad || !liveUnit.canLoad(pPlot))
						//{
						//	return false;
						//}
						if (bIgnoreLoad || !canLoad(record, pPlot))
							return false;
						//else
						//{
						//	record.setUseSerialFallback(std::string(__func__) + " getTerrainImpassable canLoad");
						//	return false;
						//}
					}
				}
			}
		}
	}

	switch (getDomainType())
	{
	case DOMAIN_SEA:
		if (!pPlot->isWater() && !canMoveAllTerrain())
		{
			//if (!pPlot->isCoastalLand() || !pPlot->isFriendlyCity(liveUnit, true))
			//{
			//	return false;
			//}
			if (!pPlot->isCoastalLand())
				return false;
			else
			{
				record.setUseSerialFallback(std::string(__func__) + " isFriendlyCity not implemented");
				return false;
			}
		}
		break;

	case DOMAIN_AIR:
		if (!bAttack)
		{
			record.setUseSerialFallback(std::string(__func__) + " DOMAIN_AIR not implemented.");
			return false;
		}

		break;

	case DOMAIN_LAND:
		if (pPlot->isWater() && !canMoveAllTerrain())
		{
			if (!pPlot->isCity() || 0 == GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES"))
			{
				// We're always AI.
				//if (bIgnoreLoad || !isHuman() || plot()->isWater() || !canLoad(pPlot))
				{
					return false;
				}
			}
		}
		break;

	case DOMAIN_IMMOBILE:
		return false;
		break;

	default:
		FAssert(false);
		break;
	}

	if (liveUnit.isAnimal())
	{
		if (pPlot->isOwned())
		{
			return false;
		}

		if (!bAttack)
		{
			if (pPlot->getBonusType() != NO_BONUS)
			{
				return false;
			}

			if (pPlot->getImprovementType() != NO_IMPROVEMENT)
			{
				return false;
			}

			record.addDependencyOnOurUnitMovementAt(pPlot);
			if (pPlot->getNumUnits() > 0)
			{
				return false;
			}
		}
	}

	if (isNoCapture())
	{
		if (!bAttack)
		{
			if (pPlot->isEnemyCity(liveUnit))
			{
				return false;
			}
		}
	}

	if (bAttack)
	{
		if (isMadeAttack() && !liveUnit.isBlitz())
		{
			return false;
		}
	}

	if (getDomainType() == DOMAIN_AIR)
	{
		if (bAttack)
		{
			//if (!liveUnit.canAirStrike(pPlot))
			//{
			//	return false;
			//}
			record.setUseSerialFallback(std::string(__func__) + " DOMAIN_AIR canAirStrike dependent on plot visibility.");
			return false;
		}
	}
	else
	{
		if (liveUnit.canAttack())
		{
			if (bAttack || !canCoexistWithEnemyUnit(NO_TEAM))
			{
				// Always AI.
				//if (!isHuman() || (pPlot->isVisible(getTeam(), false)))
				{
					if (record.isVisibleEnemyUnit(pPlot, liveUnit) != bAttack)
					{
						//FAssertMsg(isHuman() || (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack)), "hopefully not an issue, but tracking how often this is the case when we dont want to really declare war");
						//if (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwner()) != bAttack && !(bAttack && pPlot->getPlotCity() && !isNoCapture())))
						if (!bDeclareWar)
						{
							return false;
						}
						else
						{
							//record.setUseSerialFallback(std::string(__func__) + " bDeclareWar not implemented.");

							record.addDependencyOnInvisibilitySightAt(pPlot);
							return pPlot->isVisibleOtherUnit(getOwner()) != bAttack && !(bAttack && pPlot->getPlotCity() && !isNoCapture());
						}
					}
				}
			}

			if (bAttack)
			{
				if (CvUnit* const pDefender = pPlot->getBestDefender(NO_PLAYER, getOwner(), &liveUnit, true))
				{
					if (!liveUnit.canAttack(*pDefender))
					{
						return false;
					}
				}

				// Let's not even try that...
				//if (record.isVisibleEnemyUnit(pPlot, liveUnit))
				//{
				//	record.setUseSerialFallback(std::string(__func__) + " getBestDefender not implemented.");
				//	return false;
				//}
			}
		}
		else
		{
			if (bAttack)
			{
				return false;
			}

			if (!canCoexistWithEnemyUnit(NO_TEAM))
			{
				// We're always AI.
				//if (!isHuman() || pPlot->isVisible(getTeam(), false))
				{
					if (pPlot->isEnemyCity(liveUnit))
					{
						return false;
					}

					if (record.isVisibleEnemyUnit(pPlot, liveUnit))
					{
						return false;
					}
				}
			}
		}

		// We're always AI.
		//if (isHuman())
		//{
		//	ePlotTeam = pPlot->getRevealedTeam(getTeam(), false);
		//	bCanEnterArea = canEnterArea(ePlotTeam, pPlotArea);
		//}

		if (!bCanEnterArea)
		{
			FAssert(ePlotTeam != NO_TEAM);

			if (!(GET_TEAM(getTeam()).canDeclareWar(ePlotTeam)))
			{
				return false;
			}

			// We're always AI.
			//if (isHuman())
			//{
			//	if (!bDeclareWar)
			//	{
			//		return false;
			//	}
			//}
			//else
			{
				if (GET_TEAM(getTeam()).AI_isSneakAttackReady(ePlotTeam))
				{
					if (!(group.AI_isDeclareWar(pPlot)))
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool DryRunUnitState::canLoad(GroupUpdateRecord& record, const CvPlot* plot) const
{
	record.addDependencyOnOurUnitMovementAt(plot);

	CLLNode<IDInfo>* pUnitNode = plot->headUnitNode();
	while (pUnitNode)
	{
		CvUnit* const pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = plot->nextUnitNode(pUnitNode);

		// If units don't move to/from this plot, then this can only change if units are transferred between transports.
		if (liveUnit.canLoadUnit(pLoopUnit, plot))
		{
			return true;
		}
	}

	return false;
}

/// void CvUnit::setBlockading(bool bNewValue)
void DryRunUnitState::setBlockading(GroupUpdateRecord& record, bool bNewValue)
{
	if (bNewValue != liveUnit.isBlockading())
		record.setUseSerialFallback("setBlockading");
}

bool DryRunUnitState::build(GroupUpdateRecord& record, BuildTypes eBuild) const
{
	bool bFinished;

	FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

	if (!canBuild(plot(), eBuild))
	{
		return false;
	}

	bFinished = false;

	// Worker builds not implemented.
	// TODO: Incrementing build turns is easy, but finishing the build is not. Need to deterministically determine which workers put in the turns.
	record.setUseSerialFallback("build");

	return bFinished;
}

/// CvUnit::canSleep, which depends on modified group state.
bool DryRunUnitState::canSleep(const DryRunGroupState& group) const
{
	if (isFortifyable())
	{
		return false;
	}

	if (isWaiting(group))
	{
		return false;
	}

	return true;
}

/// CvUnit::canFortify, which depends on modified group state.
bool DryRunUnitState::canFortify(const DryRunGroupState& group) const
{
	if (!isFortifyable())
	{
		return false;
	}

	if (isWaiting(group))
	{
		return false;
	}

	return true;
}

// Returns true iff the plot provides a heal rate depending only on city and plot owner. Unit-invariant.
// This is always true, pretty much. Every tile has a heal rate in vanilla.
bool DryRunUnitState::hasUnitInvariantHealRate(const CvPlot* pPlot) const
{
	CvCity* pCity;
	int iTotalHeal;

	pCity = pPlot->getPlotCity();

	iTotalHeal = 0;

	if (pPlot->isCity(true, getTeam()))
	{
		iTotalHeal += GC.getDefineINT("CITY_HEAL_RATE") + (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) ? getExtraFriendlyHeal() : getExtraNeutralHeal());
		if (pCity && !pCity->isOccupation())
		{
			iTotalHeal += pCity->getHealRate();
		}
	}
	else
	{
		if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()))
		{
			if (isEnemy(pPlot->getTeam(), pPlot))
			{
				iTotalHeal += (GC.getDefineINT("ENEMY_HEAL_RATE") + getExtraEnemyHeal());
			}
			else
			{
				iTotalHeal += (GC.getDefineINT("NEUTRAL_HEAL_RATE") + getExtraNeutralHeal());
			}
		}
		else
		{
			iTotalHeal += (GC.getDefineINT("FRIENDLY_HEAL_RATE") + getExtraFriendlyHeal());
		}
	}

	return iTotalHeal > 0;
}

/// bool CvUnit::canHeal(const CvPlot* pPlot) const
bool DryRunUnitState::canHeal(const DryRunGroupState& group, const CvPlot* pPlot, GroupUpdateRecord& record) const
{
	if (!isHurt())
	{
		return false;
	}

	if (isWaiting(group))
	{
		return false;
	}

	if (!hasUnitInvariantHealRate(pPlot))
	{
		record.setUseSerialFallback("canHeal !hasUnitInvariantHealRate");
		return false;
	}

	return true;
}

/// bool CvUnit::canSentry(const CvPlot* pPlot) const
bool DryRunUnitState::canSentry(const DryRunGroupState& group, const CvPlot* pPlot) const
{
	if (!canDefend(pPlot))
	{
		return false;
	}

	if (isWaiting(group))
	{
		return false;
	}

	return true;
}

/// CvUnit::isWaiting, which depends on modified group state.
bool DryRunUnitState::isWaiting(const DryRunGroupState& group) const
{
	return group.isWaiting();
}

void DryRunUnitState::move(GroupUpdateRecord& record, [[maybe_unused]] const DryRunGroupState& group, const CvPlot* pPlot, bool bShow)
{
	FAssert(canMoveOrAttackInto(record, group, pPlot) || isMadeAttack());

	[[maybe_unused]] const CvPlot* pOldPlot = plot();

	changeMoves(pPlot->movementCost(&liveUnit, plot()));

	setXY(record, pPlot->getX_INLINE(), pPlot->getY_INLINE(), true, true, bShow && pPlot->isVisibleToWatchingHuman(), bShow);

	//change feature
	FeatureTypes featureType = pPlot->getFeatureType();
	if (featureType != NO_FEATURE)
	{
		const TCHAR* const featureString = GC.getFeatureInfo(featureType).getOnUnitChangeTo();
		if (featureString && featureString[0])
		{
			// Not implemented.
			record.setUseSerialFallback("GC.getFeatureInfo(featureType).getOnUnitChangeTo()");
			return;
		}
	}

	// Not for AI.
	//if (getOwner() == GC.getGameINLINE().getActivePlayer())
	//{
	//	if (!(pPlot->isOwned()))
	//	{
	//		//spawn birds if trees present - JW
	//		if (featureType != NO_FEATURE)
	//		{
	//			if (GC.getASyncRand().get(100) < GC.getFeatureInfo(featureType).getEffectProbability())
	//			{
	//				EffectTypes eEffect = (EffectTypes)GC.getInfoTypeForString(GC.getFeatureInfo(featureType).getEffectType());
	//				gDLL->getEngineIFace()->TriggerEffect(eEffect, pPlot->getPoint(), (float)(GC.getASyncRand().get(360)));
	//				gDLL->getInterfaceIFace()->playGeneralSound("AS3D_UN_BIRDS_SCATTER", pPlot->getPoint());
	//			}
	//		}
	//	}
	//}

	// Remember to do this later!
	//CvEventReporter::getInstance().unitMove(pPlot, this, pOldPlot);
}

void DryRunUnitState::setXY(GroupUpdateRecord& record, int iX, int iY, bool bGroup, bool bUpdate, [[maybe_unused]] bool bShow, [[maybe_unused]] bool bCheckPlotVisible)
{
	// NOTE: Call from `move` has bGroup = true, bUpdate = true, bShow = ?, bCheckPlotVisible = true

	// We assume the unit will only move along its initially computed path.
	// If it doesn't, fallback.
	//if (!std::ranges::contains(pathingChoice.thisTurnMoves, i16vec2(ivec2(iX, iY))))
	//	record.useSerialFallback = true;

	FAssert(!at(iX, iY));
	FAssert(!liveUnit.isFighting());
	FAssert((iX == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getX_INLINE() == iX));
	FAssert((iY == INVALID_PLOT_COORD) || (GC.getMapINLINE().plotINLINE(iX, iY)->getY_INLINE() == iY));

	setBlockading(record, false);

	if (!bGroup || isCargo())
	{
		//joinGroup(nullptr, true);
		record.setUseSerialFallback("setXY !bGroup || isCargo()");
		bShow = false;
	}

	const CvPlot* const pNewPlot = GC.getMapINLINE().plotINLINE(iX, iY);

	if (pNewPlot)
	{
		//pTransportUnit = getTransportUnit();
		//
		//if (pTransportUnit != nullptr)
		//{
		//	if (!(pTransportUnit->atPlot(pNewPlot)))
		//	{
		//		setTransportUnit(nullptr);
		//	}
		//}
		// Can just check isCargo instead, which is already checked, but do it again for completeness.
		if (isCargo())
			record.setUseSerialFallback("setXY isCargo()");

		if (canFight())
		{
			for (auto* pUnitNode = pNewPlot->headUnitNode(); pUnitNode; pUnitNode = pNewPlot->nextUnitNode(pUnitNode))
			{
				if (const CvUnit* const pLoopUnit = ::getUnit(pUnitNode->m_data))
				{
					if (isEnemy(pLoopUnit->getTeam(), pNewPlot) || pLoopUnit->isEnemy(getTeam()))
					{
						if (!pLoopUnit->canCoexistWithEnemyUnit(getTeam()))
						{
							// Enemy kill
							// Use fallback.
							record.setUseSerialFallback("setXY !canCoexistWithEnemyUnit");
							break;
						}
					}
				}
			}
		}

		if (pNewPlot->isGoody(getTeam()))
		{
			// GET_PLAYER(getOwner()).doGoody(pNewPlot, this);
			// Not implemented.
			record.setUseSerialFallback("setXY isGoody");
		}
	}

	const CvPlot* const pOldPlot = plot();

	// Should be all trivially parallelisable. No need to record memory dependencies.
	//if (pOldPlot)
	//{
	//	pOldPlot->removeUnit(this, bUpdate && !hasCargo()); // Use mutex
	//
	//	pOldPlot->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true); // Should just be a bunch of atomic ops
	//
	//	// fortsnek: Local var
	//	CvArea& area = *pOldPlot->area();
	//
	//	area.changeUnitsPerPlayer(getOwnerINLINE(), -1); // atomics
	//	area.changePower(getOwnerINLINE(), -(m_pUnitInfo->getPowerValue())); // atomics
	//
	//	if (AI_getUnitAIType() != NO_UNITAI)
	//	{
	//		area.changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), -1); // atomics
	//	}
	//
	//	if (isAnimal())
	//	{
	//		area.changeAnimalsPerPlayer(getOwnerINLINE(), -1); // atomics
	//	}
	//
	//	if (pOldPlot->getTeam() != getTeam() && (pOldPlot->getTeam() == NO_TEAM || !GET_TEAM(pOldPlot->getTeam()).isVassal(getTeam())))
	//	{
	//		GET_PLAYER(getOwner()).changeNumOutsideUnits(-1); // atomics
	//	}
	//
	//	setLastMoveTurn(GC.getGameINLINE().getTurnSlice()); // no data race
	//
	//	pOldCity = pOldPlot->getPlotCity();
	//
	//	if (pOldCity != nullptr)
	//	{
	//		if (isMilitaryHappiness())
	//		{
	//			pOldCity->changeMilitaryHappinessUnits(-1); // atomics
	//		}
	//	}
	//
	//	pWorkingCity = pOldPlot->getWorkingCity();
	//
	//	if (pWorkingCity != nullptr)
	//	{
	//		if (canSiege(pWorkingCity->getTeam()))
	//		{
	//			pWorkingCity->AI_setAssignWorkDirty(true); // post-processing
	//		}
	//	}
	//
	//	if (pOldPlot->isWater())
	//	{
	//		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	//		{
	//			pLoopPlot = plotDirection(pOldPlot->getX_INLINE(), pOldPlot->getY_INLINE(), ((DirectionTypes)iI));
	//
	//			if (pLoopPlot != nullptr)
	//			{
	//				if (pLoopPlot->isWater())
	//				{
	//					pWorkingCity = pLoopPlot->getWorkingCity();
	//
	//					if (pWorkingCity != nullptr)
	//					{
	//						if (canSiege(pWorkingCity->getTeam()))
	//						{
	//							pWorkingCity->AI_setAssignWorkDirty(true); // post-processing
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}
	//
	//	if (pOldPlot->isActiveVisible(true))
	//	{
	//		pOldPlot->updateMinimapColor(); // post-processing
	//	}
	//
	//	if (pOldPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
	//	{
	//		gDLL->getInterfaceIFace()->verifyPlotListColumn(); // post-processing
	//
	//		gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true); // post-processing
	//	}
	//}

	if (pNewPlot)
	{
		coord = getPlotCoord(*pNewPlot);
	}
	else
	{
		record.setUseSerialFallback("setXY Oh dear. We're being removed from the map.");
	}

	FAssertMsg(plot() == pNewPlot, "plot is expected to equal pNewPlot");

	if (pNewPlot)
	{
		if (const CvCity* pNewCity = pNewPlot->getPlotCity())
		{
			if (isEnemy(pNewCity->getTeam()) && !canCoexistWithEnemyUnit(pNewCity->getTeam()) && canFight())
			{
				record.setUseSerialFallback("setXY City capture not implemented.");
			}
		}

		// No data race
		//update facing direction
		//if (pOldPlot)
		//{
		//	DirectionTypes newDirection = estimateDirection(pOldPlot, pNewPlot);
		//	if (newDirection != NO_DIRECTION)
		//		m_eFacingDirection = newDirection;
		//}

		// Not implemented.
		//update cargo mission animations
		if (isCargo())
		{
			//if (eOldActivityType != ACTIVITY_MISSION)
			//{
			//	getGroup()->setActivityType(eOldActivityType);
			//}
			record.setUseSerialFallback("setXY isCargo() 2.");
		}

		// No data race.
		//setFortifyTurns(0);

		// Atomics
		//pNewPlot->changeAdjacentSight(getTeam(), visibilityRange(), true, this, true); // needs to be here so that the square is considered visible when we move into it...
		// Mutex
		//pNewPlot->addUnit(this, bUpdate && !hasCargo());

		// Atomics
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
		//
		//if (pNewPlot->getTeam() != getTeam() && (pNewPlot->getTeam() == NO_TEAM || !GET_TEAM(pNewPlot->getTeam()).isVassal(getTeam())))
		//{
		//	GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(1);
		//}

		if (shouldLoadOnMove(pNewPlot))
		{
			//load();
			// Not implemented.

			std::wosyncstream(std::wclog) << getFullPlotDescription(*pOldPlot) << L' ' << getFullPlotDescription(*pNewPlot) << std::endl;
			record.setUseSerialFallback("setXY shouldLoadOnMove.");
		}

		// Flag this for later.
		//for (iI = 0; iI < MAX_CIV_TEAMS; iI++)
		//{
		//	if (GET_TEAM((TeamTypes)iI).isAlive())
		//	{
		//		if (!isInvisible(((TeamTypes)iI), false))
		//		{
		//			if (pNewPlot->isVisible((TeamTypes)iI, false))
		//			{
		//				GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
		//			}
		//		}
		//	}
		//}

		// Atomics
		//if (const CvCity* const pNewCity = pNewPlot->getPlotCity())
		//{
		//	if (isMilitaryHappiness())
		//	{
		//		pNewCity->changeMilitaryHappinessUnits(1);
		//	}
		//}

		// Flag these for later.
		//if (const CvCity* pWorkingCity = pNewPlot->getWorkingCity())
		//{
		//	if (canSiege(pWorkingCity->getTeam()))
		//	{
		//		pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pNewPlot));
		//	}
		//}
		//
		//if (pNewPlot->isWater())
		//{
		//	for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		//	{
		//		pLoopPlot = plotDirection(pNewPlot->getX_INLINE(), pNewPlot->getY_INLINE(), ((DirectionTypes)iI));
		//
		//		if (pLoopPlot != nullptr)
		//		{
		//			if (pLoopPlot->isWater())
		//			{
		//				pWorkingCity = pLoopPlot->getWorkingCity();
		//
		//				if (pWorkingCity != nullptr)
		//				{
		//					if (canSiege(pWorkingCity->getTeam()))
		//					{
		//						pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pLoopPlot));
		//					}
		//				}
		//			}
		//		}
		//	}
		//}

		// Flag for later.
		//if (pNewPlot->isActiveVisible(true))
		//{
		//	pNewPlot->updateMinimapColor();
		//}

		// Flag for later.
		//if (GC.IsGraphicsInitialized())
		//{
		//	//override bShow if check plot visible
		//	if (bCheckPlotVisible && pNewPlot->isVisibleToWatchingHuman())
		//		bShow = true;
		//
		//	if (bShow)
		//	{
		//		QueueMove(pNewPlot);
		//	}
		//	else
		//	{
		//		SetPosition(pNewPlot);
		//	}
		//}

		// Flag for later.
		//if (pNewPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
		//{
		//	gDLL->getInterfaceIFace()->verifyPlotListColumn();
		//
		//	gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
		//}
	}

	if (pOldPlot)
	{
		if (hasCargo())
		{
			//pUnitNode = pOldPlot->headUnitNode();
			//
			//while (pUnitNode)
			//{
			//	pLoopUnit = ::getUnit(pUnitNode->m_data);
			//	pUnitNode = pOldPlot->nextUnitNode(pUnitNode);
			//
			//	if (pLoopUnit->getTransportUnit() == this)
			//	{
			//		pLoopUnit->setXY(iX, iY, bGroup, false);
			//	}
			//}

			// Not implemented.
			record.setUseSerialFallback("setXY hasCargo() 3");
		}
	}

	if (bUpdate && hasCargo())
	{
		// Do this later.
		//if (pOldPlot)
		//{
		//	pOldPlot->updateCenterUnit();
		//	pOldPlot->setFlagDirty(true);
		//}
		//
		//if (pNewPlot)
		//{
		//	pNewPlot->updateCenterUnit();
		//	pNewPlot->setFlagDirty(true);
		//}
	}

	// Rare, so take advantage to avoid doing the area stats updates.
	if (pOldPlot->getArea() != pNewPlot->getArea())
		record.setUseSerialFallback("setXY area change");

	FAssert(pOldPlot != pNewPlot);

	// Mutex this.
	//GET_PLAYER(getOwner()).updateGroupCycle(this);

	// Atomic.
	//setInfoBarDirty(true);

	// Not for AI.
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

	// Not used in TUI?
	//update glow
	//if (pNewPlot != nullptr)
	//{
	//	gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
	//}

	// Ripping this out. Python != performance, and the event handler doesn't do anything anyway, it seems.
	// report event to Python, along with some other key state
	//CvEventReporter::getInstance().unitSetXY(pNewPlot, this);
}

bool DryRunUnitState::AI_update(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder)
{
	FAssertMsg(canMove(), "canMove is expected to be true");
	//FAssertMsg(isGroupHead(), "isGroupHead is expected to be true"); // XXX is this a good idea???

	// NO PYTHON. NONE.
	// allow python to handle it
	//CyUnit* pyUnit = new CyUnit(this);
	//CyArgsList argsList;
	//argsList.add(gDLL->getPythonIFace()->makePythonObject(pyUnit));	// pass in unit class
	//long lResult = 0;
	//gDLL->getPythonIFace()->callFunction(PYGameModule, "AI_unitUpdate", argsList.makeFunctionArgs(), &lResult);
	//delete pyUnit;	// python fxn must not hold on to this pointer
	//if (lResult == 1)
	//{
	//	return false;
	//}

	if (getDomainType() == DOMAIN_LAND)
	{
		if (plot()->isWater() && !canMoveAllTerrain())
		{
			group.pushSkipMission(record, pathFinder);
			return false;
		}
		else
		{
			if ([[maybe_unused]] const CvUnit* const pTransportUnit = getTransportUnit())
			{
				//if (pTransportUnit->getGroup()->hasMoved() || (pTransportUnit->getGroup()->headMissionQueueNode() != nullptr))
				//{
				//	group.pushMission(PathingChoice(), MISSION_SKIP);
				//	return false;
				//}

				// Group dependency. Not sure about this...
				record.setUseSerialFallback("AI_update getTransportUnit");
				return false;
			}
		}
	}

	if (AI_afterAttack(record))
	{
		return false;
	}

	if (group.isAutomated())
	{
		//switch (group.getAutomateType())
		//{
		//case AUTOMATE_BUILD:
		//	if (AI_getUnitAIType() == UNITAI_WORKER)
		//	{
		//		AI_workerMove(pathFinder);
		//	}
		//	else if (AI_getUnitAIType() == UNITAI_WORKER_SEA)
		//	{
		//		AI_workerSeaMove();
		//	}
		//	else
		//	{
		//		FAssert(false);
		//	}
		//	break;
		//
		//case AUTOMATE_NETWORK:
		//	AI_networkAutomated();
		//	// XXX else wake up???
		//	break;
		//
		//case AUTOMATE_CITY:
		//	AI_cityAutomated();
		//	// XXX else wake up???
		//	break;
		//
		//case AUTOMATE_EXPLORE:
		//	switch (getDomainType())
		//	{
		//	case DOMAIN_SEA:
		//		AI_exploreSeaMove();
		//		break;
		//
		//	case DOMAIN_AIR:
		//		// if we are cargo (on a carrier), hold if the carrier is not done moving yet
		//		pTransportUnit = getTransportUnit();
		//		if (pTransportUnit != nullptr)
		//		{
		//			if (pTransportUnit->isAutomated() && pTransportUnit->canMove() && pTransportUnit->getGroup()->getActivityType() != ACTIVITY_HOLD)
		//			{
		//				getGroup()->pushMission(MISSION_SKIP);
		//				break;
		//			}
		//		}
		//		break;
		//
		//	case DOMAIN_LAND:
		//		AI_exploreMove();
		//		break;
		//
		//	default:
		//		FAssert(false);
		//		break;
		//	}
		//
		//	// if we have air cargo (we are a carrier), and we done moving, explore with the aircraft as well
		//	if (hasCargo() && domainCargo() == DOMAIN_AIR && (!canMove() || getGroup()->getActivityType() == ACTIVITY_HOLD))
		//	{
		//		std::vector<CvUnit*> aCargoUnits;
		//		getCargoUnits(aCargoUnits);
		//		for (unsigned int i = 0; i < aCargoUnits.size() && isAutomated(); ++i)
		//		{
		//			CvUnit* pCargoUnit = aCargoUnits[i];
		//			if (pCargoUnit->getDomainType() == DOMAIN_AIR)
		//			{
		//				if (pCargoUnit->canMove())
		//				{
		//					pCargoUnit->getGroup()->setAutomateType(AUTOMATE_EXPLORE);
		//					pCargoUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
		//				}
		//			}
		//		}
		//	}
		//	break;
		//
		//case AUTOMATE_RELIGION:
		//	if (AI_getUnitAIType() == UNITAI_MISSIONARY)
		//	{
		//		AI_missionaryMove();
		//	}
		//	break;
		//
		//default:
		//	FAssert(false);
		//	break;
		//}

		// Does AI even use automate?
		record.setUseSerialFallback("AI_update automate");

		// if no longer automated, then we want to bail
		return !group.liveGroup->isAutomated();
	}
	else
	{
		switch (AI_getUnitAIType())
		{
		case UNITAI_UNKNOWN:
			group.pushSkipMission(record, pathFinder);
			break;

		case UNITAI_ANIMAL:
			AI_animalMove(record, pathFinder, group);
			break;

		//case UNITAI_SETTLE:
		//	AI_settleMove();
		//	break;
		//
		//case UNITAI_WORKER:
		//	AI_workerMove();
		//	break;
		//
		case UNITAI_ATTACK:
			if (isBarbarian())
			{
				AI_barbAttackMove(record, pathFinder, group);
			}
			else
			{
				//AI_attackMove();
				record.setUseSerialFallback("UNITAI_ATTACK AI_attackMove");
			}
			break;
			
		//case UNITAI_ATTACK_CITY:
		//	AI_attackCityMove();
		//	break;
		//
		//case UNITAI_COLLATERAL:
		//	AI_collateralMove();
		//	break;
		//
		//case UNITAI_PILLAGE:
		//	AI_pillageMove();
		//	break;
		//
		//case UNITAI_RESERVE:
		//	AI_reserveMove();
		//	break;
		//
		//case UNITAI_COUNTER:
		//	AI_counterMove();
		//	break;
		//
		//case UNITAI_PARADROP:
		//	AI_paratrooperMove();
		//	break;
		//
		case UNITAI_CITY_DEFENSE:
			AI_cityDefenseMove(record, pathFinder, group);
			break;
			
		//case UNITAI_CITY_COUNTER:
		//case UNITAI_CITY_SPECIAL:
		//	AI_cityDefenseExtraMove();
		//	break;
		//
		//case UNITAI_EXPLORE:
		//	AI_exploreMove();
		//	break;
		//
		//case UNITAI_MISSIONARY:
		//	AI_missionaryMove();
		//	break;
		//
		//case UNITAI_PROPHET:
		//	AI_prophetMove();
		//	break;
		//
		//case UNITAI_ARTIST:
		//	AI_artistMove();
		//	break;
		//
		//case UNITAI_SCIENTIST:
		//	AI_scientistMove();
		//	break;
		//
		//case UNITAI_GENERAL:
		//	AI_generalMove();
		//	break;
		//
		//case UNITAI_MERCHANT:
		//	AI_merchantMove();
		//	break;
		//
		//case UNITAI_ENGINEER:
		//	AI_engineerMove();
		//	break;
		//
		//case UNITAI_SPY:
		//	AI_spyMove();
		//	break;
		//
		//case UNITAI_ICBM:
		//	AI_ICBMMove();
		//	break;
		//
		//case UNITAI_WORKER_SEA:
		//	AI_workerSeaMove();
		//	break;
		//
		case UNITAI_ATTACK_SEA:
			if (isBarbarian())
			{
				AI_barbAttackSeaMove(record, pathFinder, group);
			}
			else
			{
				//AI_attackSeaMove();
				record.setUseSerialFallback("UNITAI_ATTACK_SEA AI_attackSeaMove");
			}
			break;
			
		//case UNITAI_RESERVE_SEA:
		//	AI_reserveSeaMove();
		//	break;
		//
		//case UNITAI_ESCORT_SEA:
		//	AI_escortSeaMove();
		//	break;
		//
		//case UNITAI_EXPLORE_SEA:
		//	AI_exploreSeaMove();
		//	break;
		//
		//case UNITAI_ASSAULT_SEA:
		//	AI_assaultSeaMove();
		//	break;
		//
		//case UNITAI_SETTLER_SEA:
		//	AI_settlerSeaMove();
		//	break;
		//
		//case UNITAI_MISSIONARY_SEA:
		//	AI_missionarySeaMove();
		//	break;
		//
		//case UNITAI_SPY_SEA:
		//	AI_spySeaMove();
		//	break;
		//
		//case UNITAI_CARRIER_SEA:
		//	AI_carrierSeaMove();
		//	break;
		//
		//case UNITAI_MISSILE_CARRIER_SEA:
		//	AI_missileCarrierSeaMove();
		//	break;
		//
		//case UNITAI_PIRATE_SEA:
		//	AI_pirateSeaMove();
		//	break;
		//
		//case UNITAI_ATTACK_AIR:
		//	AI_attackAirMove();
		//	break;
		//
		//case UNITAI_DEFENSE_AIR:
		//	AI_defenseAirMove();
		//	break;
		//
		//case UNITAI_CARRIER_AIR:
		//	AI_carrierAirMove();
		//	break;
		//
		//case UNITAI_MISSILE_AIR:
		//	AI_missileAirMove();
		//	break;
		//
		case UNITAI_ATTACK_CITY_LEMMING:
			AI_attackCityLemmingMove(record, pathFinder, group);
			break;

		default:
		{
			//FAssert(false);
			record.setUseSerialFallback(std::string("AI_update unimplemented UnitAI ") + toString(AI_getUnitAIType()));
			break;
		}
		}
	}

	return false;
}

int DryRunUnitState::AI_finalOddsThreshold(GroupUpdateRecord& record, const CvPlot* pPlot, int iOddsThreshold) const
{
	struct Evil : CvUnitAI
	{
		static auto get()
		{
			return &Evil::AI_finalOddsThreshold;
		}
	};
	const int result = (liveUnit.*Evil::get())(pPlot, iOddsThreshold);

	// Take dependency on visible enemy units.
	(void)record.getNumVisiblePotentialEnemyDefenders(pPlot, liveUnit);

	return result;
}

bool DryRunUnitState::canRangeStrike() const
{
	// Depends on moves...

	if (getDomainType() == DOMAIN_AIR)
	{
		return false;
	}

	if (liveUnit.airRange() <= 0)
	{
		return false;
	}

	if (liveUnit.airBaseCombatStr() <= 0)
	{
		return false;
	}

	if (!canFight())
	{
		return false;
	}

	if (isMadeAttack() && !liveUnit.isBlitz())
	{
		return false;
	}

	if (!canMove() && getMoves() > 0)
	{
		return false;
	}

	return true;
}

int DryRunUnitState::AI_searchRange(int iRange) const
{
	struct Evil : CvUnitAI
	{
		static auto get()
		{
			return &Evil::AI_searchRange;
		}
	};
	return (liveUnit.*Evil::get())(iRange);
}

int DryRunUnitState::healTurns(GroupUpdateRecord& record, const CvPlot* pPlot) const
{
	int iHeal;
	int iTurns;

	if (!isHurt())
	{
		return 0;
	}

	iHeal = healRate(record, pPlot);

	if (iHeal > 0)
	{
		iTurns = (liveUnit.getDamage() / iHeal);

		if ((liveUnit.getDamage() % iHeal) != 0)
		{
			iTurns++;
		}

		return iTurns;
	}
	else
	{
		return INT_MAX;
	}
}

int DryRunUnitState::healRate([[maybe_unused]] GroupUpdateRecord& record, const CvPlot* pPlot) const
{
	CLLNode<IDInfo>* pUnitNode;
	const CvCity* pCity;
	const CvUnit* pLoopUnit;
	const CvPlot* pLoopPlot;
	int iTotalHeal;
	int iHeal;
	int iBestHeal;
	int iI;

	pCity = pPlot->getPlotCity();

	iTotalHeal = 0;

	if (pPlot->isCity(true, getTeam()))
	{
		iTotalHeal += GC.getDefineINT("CITY_HEAL_RATE") + (GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()) ? getExtraFriendlyHeal() : getExtraNeutralHeal());
		if (pCity && !pCity->isOccupation())
		{
			iTotalHeal += pCity->getHealRate();
		}
	}
	else
	{
		if (!GET_TEAM(getTeam()).isFriendlyTerritory(pPlot->getTeam()))
		{
			if (isEnemy(pPlot->getTeam(), pPlot))
			{
				iTotalHeal += (GC.getDefineINT("ENEMY_HEAL_RATE") + getExtraEnemyHeal());
			}
			else
			{
				iTotalHeal += (GC.getDefineINT("NEUTRAL_HEAL_RATE") + getExtraNeutralHeal());
			}
		}
		else
		{
			iTotalHeal += (GC.getDefineINT("FRIENDLY_HEAL_RATE") + getExtraFriendlyHeal());
		}
	}

	// XXX optimize this (save it?)
	iBestHeal = 0;

	// NOTE: Even though this depends on our own unit movement, we don't really need to take a dependency because nothing stops healers from moving away, anyway.

	pUnitNode = pPlot->headUnitNode();

	while (pUnitNode != nullptr)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTeam() == getTeam()) // XXX what about alliances?
		{
			iHeal = pLoopUnit->getSameTileHeal();

			if (iHeal > iBestHeal)
			{
				iBestHeal = iHeal;
			}
		}
	}

	for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), ((DirectionTypes)iI));

		if (pLoopPlot != nullptr)
		{
			if (pLoopPlot->area() == pPlot->area())
			{
				pUnitNode = pLoopPlot->headUnitNode();

				while (pUnitNode != nullptr)
				{
					pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getTeam() == getTeam()) // XXX what about alliances?
					{
						iHeal = pLoopUnit->getAdjacentTileHeal();

						if (iHeal > iBestHeal)
						{
							iBestHeal = iHeal;
						}
					}
				}
			}
		}
	}

	iTotalHeal += iBestHeal;
	// XXX

	return iTotalHeal;
}

bool DryRunUnitState::isAlwaysHeal() const
{
	return liveUnit.isAlwaysHeal();
}

bool DryRunUnitState::canBombard(const CvPlot* pPlot) const
{
	if (liveUnit.bombardRate() <= 0)
	{
		return false;
	}

	if (isMadeAttack())
	{
		return false;
	}

	if (isCargo())
	{
		return false;
	}

	if (liveUnit.bombardTarget(pPlot) == nullptr)
	{
		return false;
	}

	return true;
}

int DryRunUnitState::AI_getPlotDefendersNeeded(const CvPlot* pPlot, int iExtra) const
{
	struct Hack : CvUnitAI
	{
		static auto get()
		{
			return &Hack::AI_getPlotDefendersNeeded;
		}
	};
	return (liveUnit.*Hack::get())(pPlot, iExtra);
}

#endif