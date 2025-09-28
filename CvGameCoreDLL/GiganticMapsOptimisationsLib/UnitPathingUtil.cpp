#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "UnitPathingUtil.h"

#include "../CvTeamAI.h"
#include "../CvUnit.h"
#include "../CvSelectionGroup.h"
#include "../CvPlayerAI.h"
#include "../CvGameCoreUtils.h"
#include "../CvInfos.h"
#include "../CvTeamAI.h"

using namespace GiganticMapsOptimisationsLib;

bool GiganticMapsOptimisationsLib::calc_pathDestValid(const CvSelectionGroup& group, unsigned int flags, ivec2 goal)
{
	CLLNode<IDInfo>* pUnitNode1;
	CLLNode<IDInfo>* pUnitNode2;
	const CvSelectionGroup* pSelectionGroup;
	CvUnit* pLoopUnit1;
	CvUnit* pLoopUnit2;
	CvPlot* pToPlot;
	bool bAIControl;
	bool bValid;

	pToPlot = GC.getMapINLINE().plotSorenINLINE(goal.x, goal.y);
	FAssert(pToPlot != nullptr);

	pSelectionGroup = &group;

	if (pSelectionGroup->atPlot(pToPlot))
	{
		return true;
	}

	if (pSelectionGroup->getDomainType() == DOMAIN_IMMOBILE)
	{
		return false;
	}

	bAIControl = pSelectionGroup->AI_isControlled();

	if (bAIControl)
	{
		if (!(flags & MOVE_IGNORE_DANGER))
		{
			if (!(pSelectionGroup->canFight()) && !(pSelectionGroup->alwaysInvisible()))
			{
				// NOTE: Could use the path danger cache if necessary.
				if (GET_PLAYER(pSelectionGroup->getHeadOwner()).AI_getPlotDanger(pToPlot) > 0)
				{
					return false;
				}
			}
		}

		if (pSelectionGroup->getDomainType() == DOMAIN_LAND)
		{
			int iGroupAreaID = pSelectionGroup->getArea();
			if (pToPlot->getArea() != iGroupAreaID)
			{
				if (!(pToPlot->isAdjacentToArea(iGroupAreaID)))
				{
					return false;
				}
			}
		}
	}

	if (bAIControl || pToPlot->isRevealed(pSelectionGroup->getHeadTeam(), false))
	{
		if (pSelectionGroup->isAmphibPlot(pToPlot))
		{
			pUnitNode1 = pSelectionGroup->headUnitNode();

			while (pUnitNode1 != nullptr)
			{
				pLoopUnit1 = ::getUnit(pUnitNode1->m_data);
				pUnitNode1 = pSelectionGroup->nextUnitNode(pUnitNode1);

				if ((pLoopUnit1->getCargo() > 0) && (pLoopUnit1->domainCargo() == DOMAIN_LAND))
				{
					bValid = false;

					pUnitNode2 = pLoopUnit1->plot()->headUnitNode();

					while (pUnitNode2 != nullptr)
					{
						pLoopUnit2 = ::getUnit(pUnitNode2->m_data);
						pUnitNode2 = pLoopUnit1->plot()->nextUnitNode(pUnitNode2);

						if (pLoopUnit2->getTransportUnit() == pLoopUnit1)
						{
							if (pLoopUnit2->isGroupHead())
							{
								if (pLoopUnit2->getGroup()->canMoveOrAttackInto(pToPlot, (pSelectionGroup->AI_isDeclareWar(pToPlot) || (flags & MOVE_DECLARE_WAR))))
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
			}

			return false;
		}
		else
		{
			if (!(pSelectionGroup->canMoveOrAttackInto(pToPlot, (pSelectionGroup->AI_isDeclareWar(pToPlot) || (flags & MOVE_DECLARE_WAR)))))
			{
				return false;
			}
		}
	}

	return true;
}

int GiganticMapsOptimisationsLib::calc_pathCost(const CvSelectionGroup& group, [[maybe_unused]] unsigned int flags, [[maybe_unused]] int fromTurns, int fromMP, ivec2 from, ivec2 to, ivec2 goal)
{
	CLLNode<IDInfo>* pUnitNode;
	const CvSelectionGroup* pSelectionGroup;
	const CvUnit* pLoopUnit;
	const CvPlot* pFromPlot;
	const CvPlot* pToPlot;
	int iWorstCost;
	int iCost;
	int iWorstMovesLeft;
	int iMovesLeft;
	int iWorstMax;
	int iMax;

	pFromPlot = GC.getMapINLINE().plotSorenINLINE(from.x, from.y);
	pToPlot = GC.getMapINLINE().plotSorenINLINE(to.x, to.y);

	pSelectionGroup = &group;

	iWorstCost = INT_MAX;
	iWorstMovesLeft = INT_MAX;
	iWorstMax = INT_MAX;

	pUnitNode = pSelectionGroup->headUnitNode();

	while (pUnitNode != nullptr)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pSelectionGroup->nextUnitNode(pUnitNode);
		FAssertMsg(pLoopUnit->getDomainType() != DOMAIN_AIR, "pLoopUnit->getDomainType() is not expected to be equal with DOMAIN_AIR");

		if (fromMP > 0)
		{
			iMax = fromMP;
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
					if (0 != GC.getPATH_DAMAGE_WEIGHT())
					{
						if (pToPlot->getFeatureType() != NO_FEATURE)
						{
							iCost += (GC.getPATH_DAMAGE_WEIGHT() * std::max(0, GC.getFeatureInfo(pToPlot->getFeatureType()).getTurnDamage())) / GC.getMAX_HIT_POINTS();
						}

						if (pToPlot->getExtraMovePathCost() > 0)
						{
							iCost += (PATH_MOVEMENT_WEIGHT * pToPlot->getExtraMovePathCost());
						}
					}
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

					if (pSelectionGroup->AI_isControlled())
					{
						if (pLoopUnit->canAttack())
						{
							//if (gDLL->getFAStarIFace()->IsPathDest(finder, pToPlot->getX_INLINE(), pToPlot->getY_INLINE()))
							if (to == goal)
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
	}

	FAssert(iWorstCost != INT_MAX);

	iWorstCost += PATH_STEP_WEIGHT;

	if ((pFromPlot->getX_INLINE() != pToPlot->getX_INLINE()) && (pFromPlot->getY_INLINE() != pToPlot->getY_INLINE()))
	{
		iWorstCost += PATH_STRAIGHT_WEIGHT;
	}

	FAssert(iWorstCost > 0);

	return iWorstCost;
}

// Copy of CvUnit::canMoveInto, but with unit coord override for parallel unit update.
static bool calc_canMoveInto(const CvUnit& unit, ivec2 coord, const CvPlot* pPlot, bool bAttack, bool bDeclareWar, bool bIgnoreLoad)
{
	FAssertMsg(pPlot != nullptr, "Plot is not assigned a valid value");

	//if (coord != (ivec2)getPlotCoord(*unit.plot()))
	//	std::abort();
	if (coord == (ivec2)getPlotCoord(*pPlot))
	{
		return false;
	}

	if (pPlot->isImpassable())
	{
		if (!unit.canMoveImpassable())
		{
			return false;
		}
	}

	const CvUnitInfo& unitInfo = unit.getUnitInfo();

	// Cannot move around in unrevealed land freely
	if (unitInfo.isNoRevealMap() && unit.willRevealByMove(pPlot))
	{
		return false;
	}

	if (GC.getUSE_SPIES_NO_ENTER_BORDERS())
	{
		if (unit.isSpy() && NO_PLAYER != pPlot->getOwnerINLINE())
		{
			if (!GET_PLAYER(unit.getOwnerINLINE()).canSpiesEnterBorders(pPlot->getOwnerINLINE()))
			{
				return false;
			}
		}
	}

	CvArea *pPlotArea = pPlot->area();
	TeamTypes ePlotTeam = pPlot->getTeam();
	bool bCanEnterArea = unit.canEnterArea(ePlotTeam, pPlotArea);
	if (bCanEnterArea)
	{
		if (pPlot->getFeatureType() != NO_FEATURE)
		{
			if (unitInfo.getFeatureImpassable(pPlot->getFeatureType()))
			{
				TechTypes eTech = (TechTypes)unitInfo.getFeaturePassableTech(pPlot->getFeatureType());
				if (NO_TECH == eTech || !GET_TEAM(unit.getTeam()).isHasTech(eTech))
				{
					if (DOMAIN_SEA != unit.getDomainType() || pPlot->getTeam() != unit.getTeam())  // sea units can enter impassable in own cultural borders
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
				if (NO_TECH == eTech || !GET_TEAM(unit.getTeam()).isHasTech(eTech))
				{
					if (DOMAIN_SEA != unit.getDomainType() || pPlot->getTeam() != unit.getTeam())  // sea units can enter impassable in own cultural borders
					{
						if (bIgnoreLoad || !unit.canLoad(pPlot))
						{
							return false;
						}
					}
				}
			}
		}
	}

	switch (unit.getDomainType())
	{
	case DOMAIN_SEA:
		if (!pPlot->isWater() && !unit.canMoveAllTerrain())
		{
			if (!pPlot->isFriendlyCity(unit, true) || !pPlot->isCoastalLand())
			{
				return false;
			}
		}
		break;

	case DOMAIN_AIR:
		if (!bAttack)
		{
			bool bValid = false;

			if (pPlot->isFriendlyCity(unit, true))
			{
				bValid = true;

				if (unitInfo.getAirUnitCap() > 0)
				{
					if (pPlot->airUnitSpaceAvailable(unit.getTeam()) <= 0)
					{
						bValid = false;
					}
				}
			}

			if (!bValid)
			{
				if (bIgnoreLoad || !unit.canLoad(pPlot))
				{
					return false;
				}
			}
		}

		break;

	case DOMAIN_LAND:
		if (pPlot->isWater() && !unit.canMoveAllTerrain())
		{
			if (!pPlot->isCity() || 0 == GC.getDefineINT("LAND_UNITS_CAN_ATTACK_WATER_CITIES"))
			{
				if (bIgnoreLoad || !unit.isHuman() || GC.getMapINLINE().plot(coord.x, coord.y)->isWater() || !unit.canLoad(pPlot))
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

	if (unit.isAnimal())
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

			if (pPlot->getNumUnits() > 0)
			{
				return false;
			}
		}
	}

	if (unit.isNoCapture())
	{
		if (!bAttack)
		{
			if (pPlot->isEnemyCity(unit))
			{
				return false;
			}
		}
	}

	if (bAttack)
	{
		if (unit.isMadeAttack() && !unit.isBlitz())
		{
			return false;
		}
	}

	if (unit.getDomainType() == DOMAIN_AIR)
	{
		if (bAttack)
		{
			struct Evil : CvUnit
			{
				static auto get()
				{
					return &Evil::canAirStrike;
				}
			};
			if (!(unit.*Evil::get())(pPlot))
			{
				return false;
			}
		}
	}
	else
	{
		if (unit.canAttack())
		{
			if (bAttack || !unit.canCoexistWithEnemyUnit(NO_TEAM))
			{
				if (!unit.isHuman() || (pPlot->isVisible(unit.getTeam(), false)))
				{
					if (pPlot->isVisibleEnemyUnit(&unit) != bAttack)
					{
						//FAssertMsg(isHuman() || (!bDeclareWar || (pPlot->isVisibleOtherUnit(getOwnerINLINE()) != bAttack)), "hopefully not an issue, but tracking how often this is the case when we dont want to really declare war");
						if (!bDeclareWar || (pPlot->isVisibleOtherUnit(unit.getOwnerINLINE()) != bAttack && !(bAttack && pPlot->getPlotCity() && !unit.isNoCapture())))
						{
							return false;
						}
					}
				}
			}

			if (bAttack)
			{
				CvUnit* pDefender = pPlot->getBestDefender(NO_PLAYER, unit.getOwnerINLINE(), &unit, true);
				if (nullptr != pDefender)
				{
					if (!unit.canAttack(*pDefender))
					{
						return false;
					}
				}
			}
		}
		else
		{
			if (bAttack)
			{
				return false;
			}

			if (!unit.canCoexistWithEnemyUnit(NO_TEAM))
			{
				if (!unit.isHuman() || pPlot->isVisible(unit.getTeam(), false))
				{
					if (pPlot->isEnemyCity(unit))
					{
						return false;
					}

					if (pPlot->isVisibleEnemyUnit(&unit))
					{
						return false;
					}
				}
			}
		}

		if (unit.isHuman())
		{
			ePlotTeam = pPlot->getRevealedTeam(unit.getTeam(), false);
			bCanEnterArea = unit.canEnterArea(ePlotTeam, pPlotArea);
		}

		if (!bCanEnterArea)
		{
			FAssert(ePlotTeam != NO_TEAM);

			if (!(GET_TEAM(unit.getTeam()).canDeclareWar(ePlotTeam)))
			{
				return false;
			}

			if (unit.isHuman())
			{
				if (!bDeclareWar)
				{
					return false;
				}
			}
			else
			{
				if (GET_TEAM(unit.getTeam()).AI_isSneakAttackReady(ePlotTeam))
				{
					if (!(unit.getGroup()->AI_isDeclareWar(pPlot)))
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

	//if (GC.getUSE_UNIT_CANNOT_MOVE_INTO_CALLBACK())
	//{
	//	// Python Override
	//	CyArgsList argsList;
	//	argsList.add(unit.getOwnerINLINE());	// Player ID
	//	argsList.add(unit.getID());	// Unit ID
	//	argsList.add(pPlot->getX());	// Plot X
	//	argsList.add(pPlot->getY());	// Plot Y
	//	long lResult = 0;
	//	gDLL->getPythonIFace()->callFunction(PYGameModule, "unitCannotMoveInto", argsList.makeFunctionArgs(), &lResult);
	//
	//	if (lResult != 0)
	//	{
	//		return false;
	//	}
	//}

	return true;
}

static bool calc_groupCanMoveThrough(const CvSelectionGroup& group, ivec2 start, const CvPlot* plot)
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;

	if (group.getNumUnits() > 0)
	{
		pUnitNode = group.headUnitNode();

		while (pUnitNode != nullptr)
		{
			pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = group.nextUnitNode(pUnitNode);

			if (!(calc_canMoveInto(*pLoopUnit, start, plot, false, false, true)))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

static bool calc_groupCanMoveOrAttackInto(const CvSelectionGroup& group, ivec2 start, const CvPlot* plot)
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;

	const bool bDeclareWar = false;

	if (group.getNumUnits() > 0)
	{
		pUnitNode = group.headUnitNode();

		while (pUnitNode != nullptr)
		{
			pLoopUnit = ::getUnit(pUnitNode->m_data);
			pUnitNode = group.nextUnitNode(pUnitNode);

			if (calc_canMoveInto(*pLoopUnit, start, plot, false, bDeclareWar, false) || calc_canMoveInto(*pLoopUnit, start, plot, true, bDeclareWar, false))
			{
				return true;
			}
		}
	}

	return false;
}

bool GiganticMapsOptimisationsLib::calc_pathValid(const CvSelectionGroup& group, ivec2 start, unsigned int flags, int fromTurns, int fromMP, ivec2 from, ivec2 to)
{
	const CvSelectionGroup* pSelectionGroup;
	CvPlot* pFromPlot;
	CvPlot* pToPlot;

	//if (parent == nullptr)
	//{
	//	return true;
	//}

	pFromPlot = GC.getMapINLINE().plotSorenINLINE(from.x, from.y);
	pToPlot = GC.getMapINLINE().plotSorenINLINE(to.x, to.y);

	pSelectionGroup = &group;

	// XXX might want to take this out...
	if (pSelectionGroup->getDomainType() == DOMAIN_SEA)
	{
		if (pFromPlot->isWater() && pToPlot->isWater())
		{
			if (!(GC.getMapINLINE().plotINLINE(from.x, to.y)->isWater()) && !(GC.getMapINLINE().plotINLINE(to.x, from.y)->isWater()))
			{
				return false;
			}
		}
	}

	return calc_pathFromValid(group, start, flags, fromTurns, fromMP, from);
}

bool GiganticMapsOptimisationsLib::calc_pathFromValid(const CvSelectionGroup& group, ivec2 start, unsigned int flags, int fromTurns, int fromMP, ivec2 from)
{
	const CvSelectionGroup* pSelectionGroup;
	CvPlot* pFromPlot;
	bool bAIControl;

	pFromPlot = GC.getMapINLINE().plotSorenINLINE(from.x, from.y);

	pSelectionGroup = &group;

	if (start == from)
	{
		return true;
	}

	if (flags & MOVE_SAFE_TERRITORY)
	{
		if (!(pFromPlot->isRevealed(pSelectionGroup->getHeadTeam(), false)))
		{
			return false;
		}

		if (pFromPlot->isOwned())
		{
			if (pFromPlot->getTeam() != pSelectionGroup->getHeadTeam())
			{
				return false;
			}
		}
	}

	if (flags & MOVE_NO_ENEMY_TERRITORY)
	{
		if (pFromPlot->isOwned())
		{
			if (atWar(pFromPlot->getTeam(), pSelectionGroup->getHeadTeam()))
			{
				return false;
			}
		}
	}

	bAIControl = pSelectionGroup->AI_isControlled();

	if (bAIControl)
	{
		if ((fromTurns > 1) || (fromMP == 0))
		{
			if (!(flags & MOVE_IGNORE_DANGER))
			{
				if (!(pSelectionGroup->canFight()) && !(pSelectionGroup->alwaysInvisible()))
				{
					//#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
					//					if (gGlobals.enhancedDLLGameContext->hasPotentialPlotDanger(pSelectionGroup->getTeam(), *pFromPlot))
					//#endif
					if (GET_PLAYER(pSelectionGroup->getHeadOwner()).AI_getPlotDanger(pFromPlot) > 0)
					{
						return false;
					}
				}
			}
		}
	}

	if (bAIControl || pFromPlot->isRevealed(pSelectionGroup->getHeadTeam(), false))
	{
		if (flags & MOVE_THROUGH_ENEMY)
		{
			if (!(calc_groupCanMoveOrAttackInto(*pSelectionGroup, start, pFromPlot)))
			{
				return false;
			}
		}
		else
		{
			//if (!(pSelectionGroup->canMoveThrough(pFromPlot)))
			if (!calc_groupCanMoveThrough(*pSelectionGroup, start, pFromPlot))
			{
				return false;
			}
		}
	}

	return true;
}

bool GiganticMapsOptimisationsLib::getSingleUnit_AI_isDeclareWar(const CvUnit& unit, const CvPlot* pPlot)
{
	return getSingleUnit_AI_isDeclareWar(unit, pPlot ? pPlot->getTeam() : NO_TEAM);
}

bool GiganticMapsOptimisationsLib::getSingleUnit_AI_isDeclareWar(const CvUnit& unit, TeamTypes plotTeam)
{
	switch (getSingleUnitWarAIType(unit))
	{
	case EUnitWarAIType::False:
	default:
		return false;
	case EUnitWarAIType::True:
		return true;
	case EUnitWarAIType::Limited:
		if (plotTeam != NO_TEAM)
		{
			WarPlanTypes eWarplan = GET_TEAM(unit.getTeam()).AI_getWarPlan(plotTeam);
			if (eWarplan == WARPLAN_LIMITED)
			{
				return true;
			}
		}
		return false;
	}
}

EUnitWarAIType GiganticMapsOptimisationsLib::getSingleUnitWarAIType(const CvUnit& unit)
{
	switch (unit.AI_getUnitAIType())
	{
	case UNITAI_UNKNOWN:
	case UNITAI_ANIMAL:
	case UNITAI_SETTLE:
	case UNITAI_WORKER:
		return EUnitWarAIType::False;
	case UNITAI_ATTACK_CITY:
	case UNITAI_ATTACK_CITY_LEMMING:
		return EUnitWarAIType::True;

	case UNITAI_ATTACK:
	case UNITAI_COLLATERAL:
	case UNITAI_PILLAGE:
		return EUnitWarAIType::Limited;

	case UNITAI_PARADROP:
	case UNITAI_RESERVE:
	case UNITAI_COUNTER:
	case UNITAI_CITY_DEFENSE:
	case UNITAI_CITY_COUNTER:
	case UNITAI_CITY_SPECIAL:
	case UNITAI_EXPLORE:
	case UNITAI_MISSIONARY:
	case UNITAI_PROPHET:
	case UNITAI_ARTIST:
	case UNITAI_SCIENTIST:
	case UNITAI_GENERAL:
	case UNITAI_MERCHANT:
	case UNITAI_ENGINEER:
	case UNITAI_SPY:
	case UNITAI_ICBM:
	case UNITAI_WORKER_SEA:
		return EUnitWarAIType::False;

	case UNITAI_ATTACK_SEA:
	case UNITAI_RESERVE_SEA:
	case UNITAI_ESCORT_SEA:
		return EUnitWarAIType::Limited;
	case UNITAI_EXPLORE_SEA:
		return EUnitWarAIType::False;

	case UNITAI_ASSAULT_SEA:
		if (unit.hasCargo())
		{
			return EUnitWarAIType::True;
		}
		return EUnitWarAIType::False;

	case UNITAI_SETTLER_SEA:
	case UNITAI_MISSIONARY_SEA:
	case UNITAI_SPY_SEA:
	case UNITAI_CARRIER_SEA:
	case UNITAI_MISSILE_CARRIER_SEA:
	case UNITAI_PIRATE_SEA:
	case UNITAI_ATTACK_AIR:
	case UNITAI_DEFENSE_AIR:
	case UNITAI_CARRIER_AIR:
	case UNITAI_MISSILE_AIR:
		return EUnitWarAIType::False;

	default:
		FAssert(false);
		return EUnitWarAIType::False;
	}
}

std::array<int, kNumRouteTypes> GiganticMapsOptimisationsLib::getTeamRouteChanges(TeamTypes teamI)
{
	const CvTeam& team = GET_TEAM(teamI);
	return {
		team.getRouteChange(kRouteType_Road),
		team.getRouteChange(kRouteType_Railroad),
	};
}

std::array<int32_t, heck::ISimdAStarGraph::kCoordVectorLength> GiganticMapsOptimisationsLib::calcRouteCostCombinations(std::array<int, kNumRouteTypes> teamRouteChanges, int maxMP)
{
	const int numRouteTypes = GC.getNumRouteInfos();
	//if ((1 << numRouteTypes) > heck::SimdAStar::kCoordVectorLength)
	//	std::abort();

	std::array<int32_t, heck::ISimdAStarGraph::kCoordVectorLength> costs{};

	//for (const unsigned int mask : range(heck::SimdAStar::kCoordVectorLength))
	//{
	//	int iRouteCost = 0;
	//	int iRouteFlatCost = 0;
	//
	//	for (const auto i : range<RouteTypes>(numRouteTypes))
	//	{
	//		if ((mask >> i) & 1)
	//		{
	//			const int routeChange = teamI != NO_TEAM ? GET_TEAM(teamI).getRouteChange(i) : 0;
	//			iRouteCost = std::max(iRouteCost, GC.getRouteInfo(i).getMovementCost() + routeChange);
	//			iRouteFlatCost = std::max(iRouteFlatCost, GC.getRouteInfo(i).getFlatMovementCost() * maxMP);
	//		}
	//	}
	//
	//	costs[mask] = std::min(iRouteCost, iRouteFlatCost);
	//}

	// Default cost for no routes. Caller will take the minimum with regular cost.
	//costs[0] = INT32_MAX;

	if (numRouteTypes * numRouteTypes > heck::ISimdAStarGraph::kCoordVectorLength)
		std::abort();
	for (const unsigned int mask : range(numRouteTypes * numRouteTypes))
	{
		const RouteTypes routeA = RouteTypes(mask % numRouteTypes);
		const RouteTypes routeB = RouteTypes(mask / numRouteTypes);
		int iRouteCost = 0;
		int iRouteFlatCost = 0;
		for (const RouteTypes route : { routeA, routeB })
		{
			iRouteCost = std::max(iRouteCost, GC.getRouteInfo(route).getMovementCost() + teamRouteChanges[route]);
			iRouteFlatCost = std::max(iRouteFlatCost, GC.getRouteInfo(route).getFlatMovementCost() * (maxMP / MOVE_DENOMINATOR));
		}
		costs[mask] = std::min(iRouteCost, iRouteFlatCost);
	}

	costs.back() = INT32_MAX;

	return costs;
}

static int calcRouteMPCost(const CvUnit& unit, RouteTypes routeI)
{
	const int routeChange = GET_TEAM(unit.getTeam()).getRouteChange(routeI);
	const CvRouteInfo& info = GC.getRouteInfo(routeI);
	const int iRouteCost = info.getMovementCost() + routeChange;
	const int iRouteFlatCost = info.getFlatMovementCost() * unit.baseMoves();
	return std::min(iRouteCost, iRouteFlatCost);
}

static int calcMinPlotMoveCost(const CvUnit& unit)
{
	int minMovementCost = MOVE_DENOMINATOR;

	if (unit.getDomainType() == DOMAIN_LAND)
	{
		for (const RouteTypes routeI : range<RouteTypes>(GC.getNumRouteInfos()))
			minMovementCost = std::min(minMovementCost, calcRouteMPCost(unit, routeI));
	}

	return minMovementCost;
}

static int calcMaxExtraDestPathCost(const CvUnit& unit)
{
	int maxExtraDestPathCost = 0;

	if (unit.canFight())
	{
		if (unit.getGroup()->AI_isControlled() && unit.canAttack())
		{
			maxExtraDestPathCost += 200 * PATH_DEFENSE_WEIGHT;
			maxExtraDestPathCost += PATH_CITY_WEIGHT;
			if (!unit.isRiver())
			{
				maxExtraDestPathCost += PATH_RIVER_WEIGHT * -GC.getRIVER_ATTACK_MODIFIER();
				maxExtraDestPathCost += PATH_MOVEMENT_WEIGHT * unit.maxMoves();
			}
		}
	}

	return maxExtraDestPathCost;
}

static int calcMaxPerTurnPathCost(const CvUnit& unit)
{
	// Ceiling this div. Take into account leftover MP.
	const int maxPlotsPerTurn = heck::cdiv(unit.maxMoves(), (unsigned)calcMinPlotMoveCost(unit));
	// Ignoring getPATH_DAMAGE_WEIGHT.
	int maxExtraPerPlotPathCost = PATH_TERRITORY_WEIGHT;
	int maxExtraPerTurnPathCost = 0;

	if (unit.canFight())
		maxExtraPerTurnPathCost += 200 * PATH_DEFENSE_WEIGHT;

	const int perTurnPathCost = unit.maxMoves() * PATH_MOVEMENT_WEIGHT;
	
	return perTurnPathCost + maxExtraPerTurnPathCost + maxExtraPerPlotPathCost * maxPlotsPerTurn;
}

int GiganticMapsOptimisationsLib::calcPathCostUpperBound(const CvSelectionGroup& group, int numTurns)
{
	// numTurns could be INT_MAX
	int64_t result{};

	for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
	{
		const CvUnit& unit = *::getUnit(node->m_data);
		result = std::max(result, int64_t(calcMaxPerTurnPathCost(unit)) * numTurns + calcMaxExtraDestPathCost(unit));
	}

	return (int)std::min<int64_t>(INT_MAX, result);
}

int GiganticMapsOptimisationsLib::calcPathCostLowerBoundZeroBasedTurns(const CvUnit& unit, int pathCostLowerBound)
{
	return std::max(0, pathCostLowerBound - calcMaxExtraDestPathCost(unit)) / calcMaxPerTurnPathCost(unit);
}


int GiganticMapsOptimisationsLib::calcZeroBasedMinTurnsForStepDistance(const CvUnit& unit, int dist)
{
	return dist * calcMinPlotMoveCost(unit) / unit.maxMoves();
}

std::pair<int, int> GiganticMapsOptimisationsLib::calc_pathAdd(const CvSelectionGroup& group, [[maybe_unused]] unsigned int flags, int fromTurns, int fromMP, ivec2 from, ivec2 to)
{
	const CvSelectionGroup* pSelectionGroup = &group;
	FAssert(pSelectionGroup->getNumUnits() > 0);

	int iTurns = 1;
	int iMoves = INT_MAX;

	//if (data == ASNC_INITIALADD)
	//{
	//	bool bMaxMoves = (gDLL->getFAStarIFace()->GetInfo(finder) & MOVE_MAX_MOVES);
	//	if (bMaxMoves)
	//	{
	//		iMoves = 0;
	//	}
	//
	//	for (CLLNode<IDInfo>* pUnitNode = pSelectionGroup->headUnitNode(); pUnitNode != nullptr; pUnitNode = pSelectionGroup->nextUnitNode(pUnitNode))
	//	{
	//		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
	//		if (bMaxMoves)
	//		{
	//			iMoves = std::max(iMoves, pLoopUnit->maxMoves());
	//		}
	//		else
	//		{
	//			iMoves = std::min(iMoves, pLoopUnit->movesLeft());
	//		}
	//	}
	//}
	//else
	{
		CvPlot* pFromPlot = GC.getMapINLINE().plotSorenINLINE(from.x, from.y);
		FAssertMsg(pFromPlot != nullptr, "FromPlot is not assigned a valid value");
		CvPlot* pToPlot = GC.getMapINLINE().plotSorenINLINE(to.x, to.y);
		FAssertMsg(pToPlot != nullptr, "ToPlot is not assigned a valid value");

		int iStartMoves = fromMP;
		iTurns = fromTurns;
		if (iStartMoves == 0)
		{
			iTurns++;
		}

		for (CLLNode<IDInfo>* pUnitNode = pSelectionGroup->headUnitNode(); pUnitNode != nullptr; pUnitNode = pSelectionGroup->nextUnitNode(pUnitNode))
		{
			CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);

			int iUnitMoves = (iStartMoves == 0 ? pLoopUnit->maxMoves() : iStartMoves);
			iUnitMoves -= pToPlot->movementCost(pLoopUnit, pFromPlot);
			iUnitMoves = std::max(iUnitMoves, 0);

			iMoves = std::min(iMoves, iUnitMoves);
		}
	}

	FAssertMsg(iMoves >= 0, "iMoves is expected to be non-negative (invalid Index)");

	return { iMoves, iTurns };
}

#endif