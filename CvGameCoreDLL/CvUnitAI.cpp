// unitAI.cpp

#include "CvGameCoreDLL.h"
#include "CvUnitAI.h"
#include "CvMap.h"
#include "CvArea.h"
#include "CvPlot.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvTeamAI.h"
#include "CvPlayerAI.h"
#include "CvGameCoreUtils.h"
#include "CvRandom.h"
#include "CyUnit.h"
#include "CyArgsList.h"
#include "CvDLLPythonIFaceBase.h"
#include "CvInfos.h"
#include "FProfiler.h"
#include "FAStarNode.h"

// interface uses
#include "CvDLLInterfaceIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"

#include <iostream>

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
#include <algorithm>
#include <fstream>
#include <chrono>
#include "GiganticMapsOptimisationsLib/GameContext.h"
#include "GiganticMapsOptimisationsLib/ExploreReachContext.h"
#include "GiganticMapsOptimisationsLib/UnitPathingUtil.h"
#include "GiganticMapsOptimisationsLib/WholeMapSimpleDijkstra.h"
#include <CommonStuff/ParallelExt.h>
#endif

#define FOUND_RANGE				(7)

// Public Functions...

CvUnitAI::CvUnitAI()
{
	AI_reset();
}


CvUnitAI::~CvUnitAI()
{
	AI_uninit();
}


void CvUnitAI::AI_init(UnitAITypes eUnitAI)
{
	AI_reset(eUnitAI);

	//--------------------------------
	// Init other game data
	AI_setBirthmark(GC.getGameINLINE().getSorenRandNum(10000, "AI Unit Birthmark"));

	FAssertMsg(AI_getUnitAIType() != NO_UNITAI, "AI_getUnitAIType() is not expected to be equal with NO_UNITAI");
	area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
	GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), 1);
}


void CvUnitAI::AI_uninit()
{
}


void CvUnitAI::AI_reset(UnitAITypes eUnitAI)
{
	AI_uninit();

	m_iBirthmark = 0;

	m_eUnitAIType = eUnitAI;
	
	m_iAutomatedAbortTurn = -1;
}

// AI_update returns true when we should abort the loop and wait until next slice
bool CvUnitAI::AI_update()
{
	PROFILE_FUNC();

	CvUnit* pTransportUnit;

	FAssertMsg(canMove(), "canMove is expected to be true");
	FAssertMsg(isGroupHead(), "isGroupHead is expected to be true"); // XXX is this a good idea???

	// allow python to handle it
	CyUnit* pyUnit = new CyUnit(this);
	CyArgsList argsList;
	argsList.add(gDLL->getPythonIFace()->makePythonObject(pyUnit));	// pass in unit class
	long lResult=0;
	gDLL->getPythonIFace()->callFunction(PYGameModule, "AI_unitUpdate", argsList.makeFunctionArgs(), &lResult);
	delete pyUnit;	// python fxn must not hold on to this pointer
	if (lResult == 1)
	{
		return false;
	}

	if (getDomainType() == DOMAIN_LAND)
	{
		if (plot()->isWater() && !canMoveAllTerrain())
		{
			getGroup()->pushMission(MISSION_SKIP);
			return false;
		}
		else
		{
			pTransportUnit = getTransportUnit();

			if (pTransportUnit != nullptr)
			{
				if (pTransportUnit->getGroup()->hasMoved() || (pTransportUnit->getGroup()->headMissionQueueNode() != nullptr))
				{
					getGroup()->pushMission(MISSION_SKIP);
					return false;
				}
			}
		}
	}

	if (AI_afterAttack())
	{
		return false;
	}

	if (getGroup()->isAutomated())
	{
		//const auto t0 = std::chrono::steady_clock::now();
		const AutomateTypes automateType = getGroup()->getAutomateType();
		switch (automateType)
		{
		case AUTOMATE_BUILD:
			if (AI_getUnitAIType() == UNITAI_WORKER)
			{
				AI_workerMove();
			}
			else if (AI_getUnitAIType() == UNITAI_WORKER_SEA)
			{
				AI_workerSeaMove();
			}
			else
			{
				FAssert(false);
			}
			break;

		case AUTOMATE_NETWORK:
			AI_networkAutomated();
			// XXX else wake up???
			break;

		case AUTOMATE_CITY:
			AI_cityAutomated();
			// XXX else wake up???
			break;

		case AUTOMATE_EXPLORE:
			switch (getDomainType())
			{
			case DOMAIN_SEA:
				AI_exploreSeaMove();
				break;

			case DOMAIN_AIR:
				// if we are cargo (on a carrier), hold if the carrier is not done moving yet
				pTransportUnit = getTransportUnit();
				if (pTransportUnit != nullptr)
				{
					if (pTransportUnit->isAutomated() && pTransportUnit->canMove() && pTransportUnit->getGroup()->getActivityType() != ACTIVITY_HOLD)
					{
						getGroup()->pushMission(MISSION_SKIP);
						break;
					}
				}
				break;

			case DOMAIN_LAND:
				AI_exploreMove();
				break;

			default:
				FAssert(false);
				break;
			}
			
			// if we have air cargo (we are a carrier), and we done moving, explore with the aircraft as well
			if (hasCargo() && domainCargo() == DOMAIN_AIR && (!canMove() || getGroup()->getActivityType() == ACTIVITY_HOLD))
			{
				std::vector<CvUnit*> aCargoUnits;
				getCargoUnits(aCargoUnits);
				for (unsigned int i = 0; i < aCargoUnits.size() && isAutomated(); ++i)
				{
					CvUnit* pCargoUnit = aCargoUnits[i];
					if (pCargoUnit->getDomainType() == DOMAIN_AIR)
					{
						if (pCargoUnit->canMove())
						{
							pCargoUnit->getGroup()->setAutomateType(AUTOMATE_EXPLORE);
							pCargoUnit->getGroup()->setActivityType(ACTIVITY_AWAKE);
						}
					}
				}
			}
			break;

		case AUTOMATE_RELIGION:
			if (AI_getUnitAIType() == UNITAI_MISSIONARY)
			{
				AI_missionaryMove();
			}
			break;

		default:
			FAssert(false);
			break;
		}

		//const auto t1 = std::chrono::steady_clock::now();
		//std::wclog << L"Updating unit with automation " << GC.getAutomateInfo(automateType).getType() << " took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
	
		// if no longer automated, then we want to bail
		return !getGroup()->isAutomated();
	}
	else
	{
		//const auto t0 = std::chrono::steady_clock::now();
		const UnitAITypes unitAIType = AI_getUnitAIType();
		switch (unitAIType)
		{
		case UNITAI_UNKNOWN:
			getGroup()->pushMission(MISSION_SKIP);
			break;

		case UNITAI_ANIMAL:
			AI_animalMove();
			break;

		case UNITAI_SETTLE:
			AI_settleMove();
			break;

		case UNITAI_WORKER:
			AI_workerMove();
			break;

		case UNITAI_ATTACK:
			if (isBarbarian())
			{
				AI_barbAttackMove();
			}
			else
			{
				AI_attackMove();
			}
			break;

		case UNITAI_ATTACK_CITY:
			AI_attackCityMove();
			break;

		case UNITAI_COLLATERAL:
			AI_collateralMove();
			break;

		case UNITAI_PILLAGE:
			AI_pillageMove();
			break;

		case UNITAI_RESERVE:
			AI_reserveMove();
			break;

		case UNITAI_COUNTER:
			AI_counterMove();
			break;

		case UNITAI_PARADROP:
			AI_paratrooperMove();
			break;

		case UNITAI_CITY_DEFENSE:
			AI_cityDefenseMove();
			break;

		case UNITAI_CITY_COUNTER:
		case UNITAI_CITY_SPECIAL:
			AI_cityDefenseExtraMove();
			break;

		case UNITAI_EXPLORE:
			AI_exploreMove();
			break;

		case UNITAI_MISSIONARY:
			AI_missionaryMove();
			break;

		case UNITAI_PROPHET:
			AI_prophetMove();
			break;

		case UNITAI_ARTIST:
			AI_artistMove();
			break;

		case UNITAI_SCIENTIST:
			AI_scientistMove();
			break;

		case UNITAI_GENERAL:
			AI_generalMove();
			break;

		case UNITAI_MERCHANT:
			AI_merchantMove();
			break;

		case UNITAI_ENGINEER:
			AI_engineerMove();
			break;

		case UNITAI_SPY:
			AI_spyMove();
			break;

		case UNITAI_ICBM:
			AI_ICBMMove();
			break;

		case UNITAI_WORKER_SEA:
			AI_workerSeaMove();
			break;

		case UNITAI_ATTACK_SEA:
			if (isBarbarian())
			{
				AI_barbAttackSeaMove();
			}
			else
			{
				AI_attackSeaMove();
			}
			break;

		case UNITAI_RESERVE_SEA:
			AI_reserveSeaMove();
			break;

		case UNITAI_ESCORT_SEA:
			AI_escortSeaMove();
			break;

		case UNITAI_EXPLORE_SEA:
			AI_exploreSeaMove();
			break;

		case UNITAI_ASSAULT_SEA:
			AI_assaultSeaMove();
			break;

		case UNITAI_SETTLER_SEA:
			AI_settlerSeaMove();
			break;

		case UNITAI_MISSIONARY_SEA:
			AI_missionarySeaMove();
			break;

		case UNITAI_SPY_SEA:
			AI_spySeaMove();
			break;

		case UNITAI_CARRIER_SEA:
			AI_carrierSeaMove();
			break;

		case UNITAI_MISSILE_CARRIER_SEA:
			AI_missileCarrierSeaMove();
			break;

		case UNITAI_PIRATE_SEA:
			AI_pirateSeaMove();
			break;

		case UNITAI_ATTACK_AIR:
			AI_attackAirMove();
			break;

		case UNITAI_DEFENSE_AIR:
			AI_defenseAirMove();
			break;

		case UNITAI_CARRIER_AIR:
			AI_carrierAirMove();
			break;

		case UNITAI_MISSILE_AIR:
			AI_missileAirMove();
			break;

		case UNITAI_ATTACK_CITY_LEMMING:
			AI_attackCityLemmingMove();
			break;

		default:
			FAssert(false);
			break;
		}


		//const auto t1 = std::chrono::steady_clock::now();
		//CvWString str;
		//getUnitAIString(str, unitAIType);
		//std::wclog << L"Updating unit with UnitAI " << str << " took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
	}

	return false;
}


// Returns true if took an action or should wait to move later...
bool CvUnitAI::AI_follow()
{
	if (AI_followBombard())
	{
		return true;
	}

	if (AI_cityAttack(1, 65, true))
	{
		return true;
	}

	if (isEnemy(plot()->getTeam()))
	{
		if (canPillage(plot()))
		{
			getGroup()->pushMission(MISSION_PILLAGE);
			return true;
		}
	}

	if (AI_anyAttack(1, 70, 2, true))
	{
		return true;
	}

	if (isFound())
	{
		if (area()->getBestFoundValue(getOwnerINLINE()) > 0)
		{
			if (AI_foundRange(FOUND_RANGE, true))
			{
				return true;
			}
		}
	}

	return false;
}


// XXX what if a unit gets stuck b/c of it's UnitAIType???
// XXX is this function costing us a lot? (it's recursive...)
void CvUnitAI::AI_upgrade()
{
	PROFILE_FUNC();

	FAssertMsg(!isHuman(), "isHuman did not return false as expected");
	FAssertMsg(AI_getUnitAIType() != NO_UNITAI, "AI_getUnitAIType() is not expected to be equal with NO_UNITAI");

	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	UnitAITypes eUnitAI = AI_getUnitAIType();
	CvArea* pArea = area();

	int iCurrentValue = kPlayer.AI_unitValue(getUnitType(), eUnitAI, pArea);
	
	for (int iPass = 0; iPass < 2; iPass++)
	{
		int iBestValue = 0;
		UnitTypes eBestUnit = NO_UNIT;

		for (int iI = 0; iI < GC.getNumUnitInfos(); iI++)
		{
			if ((iPass > 0) || GC.getUnitInfo((UnitTypes)iI).getUnitAIType(AI_getUnitAIType()))
			{
				int iNewValue = kPlayer.AI_unitValue(((UnitTypes)iI), eUnitAI, pArea);
				if ((iPass == 0 || iNewValue > 0) && iNewValue > iCurrentValue)
				{
					if (canUpgrade((UnitTypes)iI))
					{
						int iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Upgrade"));

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							eBestUnit = ((UnitTypes)iI);
						}
					}
				}
			}
		}

		if (eBestUnit != NO_UNIT)
		{
			upgrade(eBestUnit);
			doDelayedDeath();
			return;
		}
	}
}


void CvUnitAI::AI_promote()
{
	PROFILE_FUNC();

	PromotionTypes eBestPromotion;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	eBestPromotion = NO_PROMOTION;

	for (iI = 0; iI < GC.getNumPromotionInfos(); iI++)
	{
		if (canPromote((PromotionTypes)iI, -1))
		{
			iValue = AI_promotionValue((PromotionTypes)iI);

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				eBestPromotion = ((PromotionTypes)iI);
			}
		}
	}

	if (eBestPromotion != NO_PROMOTION)
	{
		promote(eBestPromotion, -1);
		AI_promote();
	}
}


int CvUnitAI::AI_groupFirstVal()
{
	switch (AI_getUnitAIType())
	{
	case UNITAI_UNKNOWN:
	case UNITAI_ANIMAL:
		FAssert(false);
		break;

	case UNITAI_SETTLE:
		return 21;
		break;

	case UNITAI_WORKER:
		return 20;
		break;

	case UNITAI_ATTACK:
		if (collateralDamage() > 0)
		{
			return 17;
		}
		else if (withdrawalProbability() > 0)
		{
			return 15;
		}
		else
		{
			return 13;
		}
		break;

	case UNITAI_ATTACK_CITY:
		if (bombardRate() > 0)
		{
			return 19;
		}
		else if (collateralDamage() > 0)
		{
			return 18;
		}
		else if (withdrawalProbability() > 0)
		{
			return 16;
		}
		else
		{
			return 14;
		}
		break;

	case UNITAI_COLLATERAL:
		return 7;
		break;

	case UNITAI_PILLAGE:
		return 12;
		break;

	case UNITAI_RESERVE:
		return 6;
		break;

	case UNITAI_COUNTER:
		return 5;
		break;

	case UNITAI_CITY_DEFENSE:
		return 3;
		break;

	case UNITAI_CITY_COUNTER:
		return 2;
		break;

	case UNITAI_CITY_SPECIAL:
		return 1;
		break;

	case UNITAI_PARADROP:
		return 4;
		break;

	case UNITAI_EXPLORE:
		return 8;
		break;

	case UNITAI_MISSIONARY:
		return 10;
		break;

	case UNITAI_PROPHET:
	case UNITAI_ARTIST:
	case UNITAI_SCIENTIST:
	case UNITAI_GENERAL:
	case UNITAI_MERCHANT:
	case UNITAI_ENGINEER:
		return 11;
		break;

	case UNITAI_SPY:
		return 9;
		break;

	case UNITAI_ICBM:
		break;

	case UNITAI_WORKER_SEA:
		return 8;
		break;

	case UNITAI_ATTACK_SEA:
		return 3;
		break;

	case UNITAI_RESERVE_SEA:
		return 2;
		break;

	case UNITAI_ESCORT_SEA:
		return 1;
		break;

	case UNITAI_EXPLORE_SEA:
		return 5;
		break;

	case UNITAI_ASSAULT_SEA:
		return 11;
		break;

	case UNITAI_SETTLER_SEA:
		return 9;
		break;

	case UNITAI_MISSIONARY_SEA:
		return 9;
		break;

	case UNITAI_SPY_SEA:
		return 10;
		break;

	case UNITAI_CARRIER_SEA:
		return 7;
		break;

	case UNITAI_MISSILE_CARRIER_SEA:
		return 6;
		break;

	case UNITAI_PIRATE_SEA:
		return 4;
		break;

	case UNITAI_ATTACK_AIR:
	case UNITAI_DEFENSE_AIR:
	case UNITAI_CARRIER_AIR:
	case UNITAI_MISSILE_AIR:
		break;

	case UNITAI_ATTACK_CITY_LEMMING:
		return 1;
		break;

	default:
		FAssert(false);
		break;
	}

	return 0;
}


int CvUnitAI::AI_groupSecondVal()
{
	return ((getDomainType() == DOMAIN_AIR) ? airBaseCombatStr() : baseCombatStr());
}


// Returns attack odds out of 100 (the higher, the better...)
// Withdrawal odds included in returned value
int CvUnitAI::AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const
{
	CvUnit* pDefender;
	int iOurStrength;
	int iTheirStrength;
	int iOurFirepower;
	int iTheirFirepower;
	int iBaseOdds;
	int iStrengthFactor;
	int iDamageToUs;
	int iDamageToThem;
	int iNeededRoundsUs;
	int iNeededRoundsThem;
	int iHitLimitThem;

	pDefender = pPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, !bPotentialEnemy, bPotentialEnemy);

	if (pDefender == nullptr)
	{
		return 100;
	}

	iOurStrength = ((getDomainType() == DOMAIN_AIR) ? airCurrCombatStr(nullptr) : currCombatStr(nullptr, nullptr));
	iOurFirepower = ((getDomainType() == DOMAIN_AIR) ? iOurStrength : currFirepower(nullptr, nullptr));

	if (iOurStrength == 0)
	{
		return 1;
	}

	iTheirStrength = pDefender->currCombatStr(pPlot, this);
	iTheirFirepower = pDefender->currFirepower(pPlot, this);


	FAssert((iOurStrength + iTheirStrength) > 0);
	FAssert((iOurFirepower + iTheirFirepower) > 0);

	iBaseOdds = (100 * iOurStrength) / (iOurStrength + iTheirStrength);
	if (iBaseOdds == 0)
	{
		return 1;
	}

	iStrengthFactor = ((iOurFirepower + iTheirFirepower + 1) / 2);

	iDamageToUs = std::max(1,((GC.getDefineINT("COMBAT_DAMAGE") * (iTheirFirepower + iStrengthFactor)) / (iOurFirepower + iStrengthFactor)));
	iDamageToThem = std::max(1,((GC.getDefineINT("COMBAT_DAMAGE") * (iOurFirepower + iStrengthFactor)) / (iTheirFirepower + iStrengthFactor)));

	iHitLimitThem = pDefender->maxHitPoints() - combatLimit();

	iNeededRoundsUs = (std::max(0, pDefender->currHitPoints() - iHitLimitThem) + iDamageToThem - 1 ) / iDamageToThem;
	iNeededRoundsThem = (std::max(0, currHitPoints()) + iDamageToUs - 1 ) / iDamageToUs;

	if (getDomainType() != DOMAIN_AIR)
	{
		iNeededRoundsUs = std::max(1, iNeededRoundsUs - (iBaseOdds * ((pDefender->immuneToFirstStrikes() ? 0 : (firstStrikes() + chanceFirstStrikes()/2))) / 100));
		iNeededRoundsThem = std::max(1, iNeededRoundsThem - ((100 - iBaseOdds) * ((immuneToFirstStrikes() ? 0 : (pDefender->firstStrikes() + pDefender->chanceFirstStrikes()/2))) / 100));
	}

	int iRoundsDiff = iNeededRoundsUs - iNeededRoundsThem;
	if (iRoundsDiff > 0)
	{
		iTheirStrength *= (1 + iRoundsDiff);
	}
	else
	{
		iOurStrength *= (1 - iRoundsDiff);
	}

	int iOdds = (((iOurStrength * 100) / (iOurStrength + iTheirStrength)));
	iOdds += ((100 - iOdds) * withdrawalProbability()) / 100;
	iOdds += GET_PLAYER(getOwnerINLINE()).AI_getAttackOddsChange();

	return std::max(1, std::min(iOdds, 99));
}


// Returns true if the unit found a build for this city...
bool CvUnitAI::AI_bestCityBuild(CvCity* pCity, CvPlot** ppBestPlot, BuildTypes* peBestBuild, CvPlot* pIgnorePlot, CvUnit* pUnit)
{
	PROFILE_FUNC();

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
						if ((pLoopPlot->getImprovementType() == NO_IMPROVEMENT) || !(GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION) && !(pLoopPlot->getImprovementType() == (GC.getDefineINT("RUINS_IMPROVEMENT")))))
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
										if (!(pLoopPlot->isVisibleEnemyUnit(this)))
										{
											int iPathTurns;
											if (generatePath(pLoopPlot, 0, true, &iPathTurns))
											{
												// XXX take advantage of range (warning... this could lead to some units doing nothing...)
												int iMaxWorkers = 1;
												if (getPathLastNode()->m_iData1 == 0)
												{
													iPathTurns++;
												}
												else if (iPathTurns <= 1)
												{
													iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBuild);										
												}
												if (pUnit != nullptr)
												{
													if (pUnit->plot()->isCity() && iPathTurns == 1 && getPathLastNode()->m_iData1 > 0)
													{
														iMaxWorkers += 10;
													}
												}	
												if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers)
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
				if ((generatePath(pBestPlot, 0, true, &iPathTurns)) && canBuild(pBestPlot, eBestBuild)
					&& !(pBestPlot->isVisibleEnemyUnit(this)))
				{
					int iMaxWorkers = 1;
					if (pUnit != nullptr)
					{
						if (pUnit->plot()->isCity())
						{
							iMaxWorkers += 10;
						}
					}	
					if (getPathLastNode()->m_iData1 == 0)
					{
						iPathTurns++;
					}
					else if (iPathTurns <= 1)
					{
						iMaxWorkers = AI_calculatePlotWorkersNeeded(pBestPlot, eBestBuild);										
					}
					int iWorkerCount = GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pBestPlot, MISSIONAI_BUILD, getGroup());
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


bool CvUnitAI::AI_isCityAIType() const
{
	return ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		      (AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
					(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
						(AI_getUnitAIType() == UNITAI_RESERVE));
}


int CvUnitAI::AI_getBirthmark() const
{
	return m_iBirthmark;
}


void CvUnitAI::AI_setBirthmark(int iNewValue)
{
	m_iBirthmark = iNewValue;
	if (AI_getUnitAIType() == UNITAI_EXPLORE_SEA)
	{
		if (GC.getGame().circumnavigationAvailable())
		{
			m_iBirthmark -= m_iBirthmark % 4;
			int iExplorerCount = GET_PLAYER(getOwnerINLINE()).AI_getNumAIUnits(UNITAI_EXPLORE_SEA);
			iExplorerCount += getOwnerINLINE() % 4;
			if (GC.getMap().isWrapX())
			{
				if ((iExplorerCount % 2) == 1)
				{
					m_iBirthmark += 1;
				}
			}
			if (GC.getMap().isWrapY())
			{
				if (!GC.getMap().isWrapX())
				{
					iExplorerCount *= 2;
				}
					
				if (((iExplorerCount >> 1) % 2) == 1)
				{
					m_iBirthmark += 2;
				}
			}
		}
	}
}


UnitAITypes CvUnitAI::AI_getUnitAIType() const
{
	return m_eUnitAIType;
}


// XXX make sure this gets called...
void CvUnitAI::AI_setUnitAIType(UnitAITypes eNewValue)
{
	FAssertMsg(eNewValue != NO_UNITAI, "NewValue is not assigned a valid value");

	if (AI_getUnitAIType() != eNewValue)
	{
		area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), -1);
		GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), -1);

		m_eUnitAIType = eNewValue;

		area()->changeNumAIUnits(getOwnerINLINE(), AI_getUnitAIType(), 1);
		GET_PLAYER(getOwnerINLINE()).AI_changeNumAIUnits(AI_getUnitAIType(), 1);

		joinGroup(nullptr);
	}
}

int CvUnitAI::AI_sacrificeValue(const CvPlot* pPlot) const
{
    int iValue;
    int iCollateralDamageValue = 0;
    if (pPlot != nullptr)
    {
        int iPossibleTargets = std::min((pPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits());

        if (iPossibleTargets > 0)
        {
            iCollateralDamageValue = collateralDamage();
            iCollateralDamageValue += std::max(0, iCollateralDamageValue - 100);
            iCollateralDamageValue *= iPossibleTargets;
            iCollateralDamageValue /= 5;
        }
    }

	if (getDomainType() == DOMAIN_AIR) 
	{
		iValue = 128 * (100 + currInterceptionProbability());
		if (m_pUnitInfo->getNukeRange() != -1)
		{
			iValue += 25000;
		}
		iValue /= std::max(1, (1 + m_pUnitInfo->getProductionCost()));
		iValue *= (maxHitPoints() - getDamage());
		iValue /= 100;
	} 
	else 
	{
		iValue  = 128 * (currEffectiveStr(pPlot, ((pPlot == nullptr) ? nullptr : this)));
		iValue *= (100 + iCollateralDamageValue);
		iValue /= (100 + cityDefenseModifier());
		iValue *= (100 + withdrawalProbability());		
		iValue /= std::max(1, (1 + m_pUnitInfo->getProductionCost()));
		iValue /= (10 + getExperience());

		if (m_pUnitInfo->getCombatLimit() < 100)
		{
			iValue *= 150;
			iValue /= 100;
		}
	}

    return iValue;
}

// Protected Functions...

void CvUnitAI::AI_animalMove()
{
	PROFILE_FUNC();

	if (GC.getGameINLINE().getSorenRandNum(100, "Animal Attack") < GC.getHandicapInfo(GC.getGameINLINE().getHandicapType()).getAnimalAttackProb())
	{
		if (AI_anyAttack(1, 0))
		{
			return;
		}
	}

	if (AI_heal())
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_settleMove()
{
	PROFILE_FUNC();

	if (GET_PLAYER(getOwnerINLINE()).getNumCities() == 0)
	{
		if (canFound(plot()))
		{
			getGroup()->pushMission(MISSION_FOUND);
			return;
		}
	}
	
	int iDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3);
	
	if (iDanger > 0)
	{
		if ((plot()->getOwnerINLINE() == getOwnerINLINE()) || (iDanger > 2))
		{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
			if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
				getGroup()->resetPath();
#endif
			joinGroup(nullptr);
			if (AI_retreatToCity())
			{
				return;
			}
			if (AI_safety())
			{
				return;
			}
			getGroup()->pushMission(MISSION_SKIP);
		}
	}

	int iAreaBestFoundValue = 0;
	int iOtherBestFoundValue = 0;

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++)
	{
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (pCitySitePlot->getArea() == getArea())
		{
			if (plot() == pCitySitePlot)
			{
				if (canFound(plot()))
				{
					getGroup()->pushMission(MISSION_FOUND);
					return;					
				}
			}
			iAreaBestFoundValue = std::max(iAreaBestFoundValue, pCitySitePlot->getCitySiteFoundValue(getOwnerINLINE()));

		}
		else
		{
			iOtherBestFoundValue = std::max(iOtherBestFoundValue, pCitySitePlot->getCitySiteFoundValue(getOwnerINLINE()));
		}
	}
	
	if ((iAreaBestFoundValue == 0) && (iOtherBestFoundValue == 0))
	{
		if ((GC.getGame().getGameTurn() - getGameTurnCreated()) > 20)
		{
			if (nullptr != getTransportUnit())
			{
				getTransportUnit()->unloadAll();
			}

			if (nullptr == getTransportUnit())
			{
				//may seem wasteful, but settlers confuse the AI.
				scrap();
				return;
			}
		}
	}
	
	if ((iOtherBestFoundValue * 100) > (iAreaBestFoundValue * 110))
	{
		if (plot()->getOwnerINLINE() == getOwnerINLINE())
		{
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, 0, MOVE_SAFE_TERRITORY))
			{
				return;
			}
		}
	}
	
	if ((iAreaBestFoundValue > 0) && plot()->isBestAdjacentFound(getOwnerINLINE()))
	{
		if (canFound(plot()))
		{
			getGroup()->pushMission(MISSION_FOUND);
			return;
		}
	}

	if (!GC.getGameINLINE().isOption(GAMEOPTION_ALWAYS_PEACE) && !GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI) && !getGroup()->canDefend())
	{
		if (AI_retreatToCity())
		{
			return;
		}
	}

	if (plot()->isCity() && (plot()->getOwnerINLINE() == getOwnerINLINE()))
	{
		if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot()) > 0) 
			&& (GC.getGameINLINE().getMaxCityElimination() > 0))
		{
			if (getGroup()->getNumUnits() < 3)
			{
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
	}

	if (iAreaBestFoundValue > 0)
	{
		if (AI_found())
		{
			return;
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, 0, MOVE_NO_ENEMY_TERRITORY))
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_workerMove()
{
	PROFILE_FUNC();
	
	CvCity* pCity;
	bool bCanRoute;
	bool bNextCity;

	bCanRoute = canBuildRoute();
	bNextCity = false;

	// XXX could be trouble...
	if (plot()->getOwnerINLINE() != getOwnerINLINE())
	{
		if (AI_retreatToCity())
		{
			return;
		}
	}

	if (!isHuman())
	{
		if (plot()->getOwnerINLINE() == getOwnerINLINE())
		{
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 2, -1, -1, 0, MOVE_SAFE_TERRITORY))
			{
				return;
			}
		}
	}

    if (!(getGroup()->canDefend()))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_isPlotThreatened(plot(), 2))
		{
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	if (bCanRoute)
	{
		if (plot()->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
		{
			BonusTypes eNonObsoleteBonus = plot()->getNonObsoleteBonusType(getTeam());
			if (NO_BONUS != eNonObsoleteBonus)
			{
				if (!(plot()->isConnectedToCapital()))
				{
					ImprovementTypes eImprovement = plot()->getImprovementType();
					if (NO_IMPROVEMENT != eImprovement && GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
					{
						if (AI_connectPlot(plot()))
						{
							return;
						}
					}
				}
			}
		}
	}
	
	CvPlot* pBestBonusPlot = nullptr;
	BuildTypes eBestBonusBuild = NO_BUILD;
	int iBestBonusValue = 0; 

    if (AI_improveBonus(25, &pBestBonusPlot, &eBestBonusBuild, &iBestBonusValue))
	{
		return;
	}

	if (bCanRoute && !isBarbarian())
	{
		if (AI_connectCity())
		{
			return;
		}
	}

	pCity = nullptr;

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		pCity = plot()->getPlotCity();
		if (pCity == nullptr)
		{
			pCity = plot()->getWorkingCity();
		}
	}
	
	
//	if (pCity != nullptr)
//	{
//		bool bMoreBuilds = false;
//		for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
//		{
//			CvPlot* pLoopPlot = plotCity(getX_INLINE(), getY_INLINE(), iI);
//			if ((iI != CITY_HOME_PLOT) && (pLoopPlot != nullptr))
//			{
//				if (pLoopPlot->getWorkingCity() == pCity)
//				{
//					if (pLoopPlot->isBeingWorked())
//					{
//						if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT)
//						{
//							if (pCity->AI_getBestBuildValue(iI) > 0)
//							{
//								ImprovementTypes eImprovement;
//								eImprovement = (ImprovementTypes)GC.getBuildInfo((BuildTypes)pCity->AI_getBestBuild(iI)).getImprovement();
//								if (eImprovement != NO_IMPROVEMENT)
//								{
//									bMoreBuilds = true;
//									break;
//								}
//							}
//						}
//					}
//				}
//			}
//		}
//		
//		if (bMoreBuilds)
//		{
//			if (AI_improveCity(pCity))
//			{
//				return;
//			}
//		}
//	}
	if (pCity != nullptr)
	{
		if ((pCity->AI_getWorkersNeeded() > 0) && (plot()->isCity() || (pCity->AI_getWorkersNeeded() < ((1 + pCity->AI_getWorkersHave() * 2) / 3))))
		{
			if (AI_improveCity(pCity))
			{
				return;
			}
		}
	}
	
	if (AI_improveLocalPlot(2, pCity))
	{
		return;		
	}
	
	bool bBuildFort = false;
	
	if (GC.getGame().getSorenRandNum(5, "AI Worker build Fort with Priority"))
	{
		bool bCanal = ((100 * area()->getNumCities()) / std::max(1, GC.getGame().getNumCities()) < 85);
		CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
		bool bAirbase = false;
		bAirbase = (kPlayer.AI_totalUnitAIs(UNITAI_PARADROP) || kPlayer.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kPlayer.AI_totalUnitAIs(UNITAI_MISSILE_AIR));
		
		if (bCanal || bAirbase)
		{
			if (AI_fortTerritory(bCanal, bAirbase))
			{
				return;
			}
		}
		bBuildFort = true;
	}

	
	if (bCanRoute && isBarbarian())
	{
		if (AI_connectCity())
		{
			return;
		}
	}

	if ((pCity == nullptr) || (pCity->AI_getWorkersNeeded() == 0) || ((pCity->AI_getWorkersHave() > (pCity->AI_getWorkersNeeded() + 1))))
	{
		if ((pBestBonusPlot != nullptr) && (iBestBonusValue >= 15))
		{
			if (AI_improvePlot(pBestBonusPlot, eBestBonusBuild))
			{
				return;
			}
		}

//		if (pCity == nullptr)
//		{
//			pCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), getOwnerINLINE()); // XXX do team???
//		}

		if (AI_nextCityToImprove(pCity))
		{
			return;
		}

		bNextCity = true;
	}

	if (pBestBonusPlot != nullptr)
	{
		if (AI_improvePlot(pBestBonusPlot, eBestBonusBuild))
		{
			return;
		}
	}
		
	if (pCity != nullptr)
	{
		if (AI_improveCity(pCity))
		{
			return;
		}
	}

	if (!bNextCity)
	{
		if (AI_nextCityToImprove(pCity))
		{
			return;
		}
	}

	if (bCanRoute)
	{
		if (AI_routeTerritory(true))
		{
			return;
		}

		if (AI_connectBonus(false))
		{
			return;
		}

		if (AI_routeCity())
		{
			return;
		}
	}

	if (AI_irrigateTerritory())
	{
		return;
	}
	
	if (!bBuildFort)
	{
		bool bCanal = ((100 * area()->getNumCities()) / std::max(1, GC.getGame().getNumCities()) < 85);
		CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
		bool bAirbase = false;
		bAirbase = (kPlayer.AI_totalUnitAIs(UNITAI_PARADROP) || kPlayer.AI_totalUnitAIs(UNITAI_ATTACK_AIR) || kPlayer.AI_totalUnitAIs(UNITAI_MISSILE_AIR));
		
		if (bCanal || bAirbase)
		{
			if (AI_fortTerritory(bCanal, bAirbase))
			{
				return;
			}
		}
	}

	if (bCanRoute)
	{
		if (AI_routeTerritory())
		{
			return;
		}
	}

	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0))
	{
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn()))
		{
			if (AI_nextCityToImproveAirlift())
			{
				return;
			}
		}
		if (!isHuman())
		{
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY))
			{
				return;
			}
		}
	}
	
	if (AI_improveLocalPlot(3, nullptr))
	{
		return;		
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER))
	{			
		if (GC.getGameINLINE().getElapsedGameTurns() > 10)
		{
			if (GET_PLAYER(getOwnerINLINE()).AI_totalUnitAIs(UNITAI_WORKER) > GET_PLAYER(getOwnerINLINE()).getNumCities())
			{
				if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0)
				{
					scrap();
					return;
				}
			}
		}
	}

	if (AI_retreatToCity(false, true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_barbAttackMove()
{
	PROFILE_FUNC();

	if (AI_guardCity(false, true, 1))
	{
		return;
	}

	if (plot()->isGoody())
	{
		if (plot()->plotCount(PUF_isUnitAIType, UNITAI_ATTACK, -1, getOwnerINLINE()) == 1)
		{
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (GC.getGameINLINE().getSorenRandNum(2, "AI Barb") == 0)
	{
		if (AI_pillageRange(1))
		{
			return;
		}
	}

	if (AI_anyAttack(1, 20))
	{
		return;
	}	

	if (GC.getGameINLINE().isOption(GAMEOPTION_RAGING_BARBARIANS))
	{
		if (AI_pillageRange(4))
		{
			return;
		}

		if (AI_cityAttack(3, 10))
		{
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
		{
			if (AI_targetCity())
			{
				return;
			}
		}
	}
	else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 3))
	{
		if (AI_cityAttack(1, 15))
		{
			return;
		}

		if (AI_pillageRange(3))
		{
			return;
		}

		if (AI_cityAttack(2, 10))
		{
			return;
		}

		if (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE)
		{
			if (AI_targetCity())
			{
				return;
			}
		}
	}
	else if (GC.getGameINLINE().getNumCivCities() > (GC.getGameINLINE().countCivPlayersAlive() * 2))
	{
		if (AI_pillageRange(2))
		{
			return;
		}

		if (AI_cityAttack(1, 10))
		{
			return;
		}
	}
	
	if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 1))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}

	if (AI_guardCity(false, true, 2))
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_attackMove()
{
	PROFILE_FUNC();
	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3) > 0);
	{
		PROFILE("CvUnitAI::AI_attackMove() 1");

		if (AI_guardCity(true))
		{
			return;
		}

		if (AI_heal(30, 1))
		{
			return;
		}
		
		if (!bDanger)
		{
			if (AI_group(UNITAI_SETTLE, 1, -1, -1, false, false, false, 3, true))
			{
				return;
			}

			if (AI_group(UNITAI_SETTLE, 2, -1, -1, false, false, false, 3, true))
			{
				return;
			}
		}

		if (AI_guardCityAirlift())
		{
			return;
		}

		if (AI_guardCity(false, true, 1))
		{
			return;
		}

		//join any city attacks in progress
		if (plot()->getOwnerINLINE() != getOwnerINLINE())
		{
			if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 1, true, true))
			{
				return;
			}
		}
		
		AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
        if (plot()->isCity())
        {
            if (plot()->getOwnerINLINE() == getOwnerINLINE())
            {
                if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST))
                {
                    if (AI_offensiveAirlift())
                    {
                        return;
                    }
                }
            }
        }
		
		if (bDanger)
		{
			if (AI_cityAttack(1, 55))
			{
				return;
			}

			if (AI_anyAttack(1, 65))
			{
				return;
			}

			if (collateralDamage() > 0)
			{
				if (AI_anyAttack(1, 45, 3))
				{
					return;
				}
			}
		}

		if (!noDefensiveBonus())
		{
			if (AI_guardCity(false, false))
			{
				return;
			}
		}
		
		if (!bDanger)
		{
			if (plot()->getOwnerINLINE() == getOwnerINLINE())
			{
				if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY, 3))
				{
					return;
				}

				if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, 2, -1, -1, MOVE_SAFE_TERRITORY, 4))
				{
					return;
				}

				if (GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_GROUP) > 0)
				{
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
		}
		
		if (bDanger)
		{
			if (AI_pillageRange(1, 20))
			{
				return;
			}

			if (AI_cityAttack(1, 35))
			{
				return;
			}

			if (AI_anyAttack(1, 45))
			{
				return;
			}

			if (AI_pillageRange(3, 20))
			{
				return;
			}
			
			if (AI_choke(1))
			{
				return;
			}
		}

		if (AI_goody(3))
		{
			return;
		}

		if (AI_anyAttack(1, 70))
		{
			return;
		}
	}

	{
		PROFILE("CvUnitAI::AI_attackMove() 2");

		if (bDanger)
		{
			if (AI_cityAttack(4, 30))
			{
				return;
			}

			if (AI_anyAttack(2, 40))
			{
				return;
			}
		}

		if (!isEnemy(plot()->getTeam()))
		{
			if (AI_heal())
			{
				return;
			}
		}

		if ((GET_PLAYER(getOwnerINLINE()).AI_getNumAIUnits(UNITAI_CITY_DEFENSE) > 0) || (GET_TEAM(getTeam()).getAtWarCount(true) > 0))
		{
				if (AI_group(UNITAI_ATTACK_CITY, /*iMaxGroup*/ 1, /*iMaxOwnUnitAI*/ 1, -1, true, true, true, /*iMaxPath*/ 5))
				{
					return;
				}

			if (AI_group(UNITAI_ATTACK, /*iMaxGroup*/ 1, /*iMaxOwnUnitAI*/ 1, -1, true, true, false, /*iMaxPath*/ 4))
			{
				return;
			}
			
			if ((getMoves() > 1) && GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_FASTMOVERS))
			{
				if (AI_group(UNITAI_ATTACK, /*iMaxGroup*/ 4, /*iMaxOwnUnitAI*/ 1, -1, true, false, false, /*iMaxPath*/ 3))
				{
					return;
				}
			}
		}

		if (area()->getAreaAIType(getTeam()) != AREAAI_OFFENSIVE)
		{
			if (area()->getCitiesPerPlayer(BARBARIAN_PLAYER) > 0)
			{
				if (AI_targetBarbCity())
				{
					return;
				}
			}
		}
		else
		{
			if (getGroup()->getNumUnits() > 1)
			{
				if (AI_targetCity())
				{
					return;
				}
			}
		}

		if (AI_guardCity(false, true, 3))
		{
			return;
		}

		if ((GET_PLAYER(getOwnerINLINE()).getNumCities() > 1) && (getGroup()->getNumUnits() == 1))
		{
			if (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE)
			{
				if (area()->getNumUnrevealedTiles(getTeam()) > 0)
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_areaMissionAIs(area(), MISSIONAI_EXPLORE, getGroup()) < (GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(area()) + 1))
					{
						if (AI_exploreRange(3))
						{
							return;
						}

						if (AI_explore())
						{
							return;
						}
					}
				}
			}
		}

		if (AI_protect(35))
		{
			return;
		}

		if (AI_offensiveAirlift())
		{
			return;
		}

		if (AI_defend())
		{
			return;
		}

		if (AI_travelToUpgradeCity())
		{
			return;
		}

		if (AI_patrol())
		{
			return;
		}

		if (AI_retreatToCity())
		{
			return;
		}

		if (AI_safety())
		{
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_paratrooperMove()
{
	PROFILE_FUNC();

	{
		PROFILE("CvUnitAI::AI_paratrooperMove()");
		bool bHostile = (plot()->isOwned() && isPotentialEnemy(plot()->getTeam()));
		if (!bHostile)
		{
			if (AI_guardCity(true))
			{
				return;
			}
			
			if (plot()->getTeam() == getTeam())
			{
				if (plot()->isCity())
				{
					if (AI_heal(30, 1))
					{
						return;
					}
				}
				
				AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
				bool bLandWar = ((eAreaAIType == AREAAI_OFFENSIVE) || (eAreaAIType == AREAAI_DEFENSIVE) || (eAreaAIType == AREAAI_MASSING));		
				if (!bLandWar)
				{
					if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, 0, MOVE_SAFE_TERRITORY, 4))
					{
						return;
					}
				}
			}

			if (AI_guardCity(false, true, 1))
			{
				return;
			}
		}

		if (AI_cityAttack(1, 45))
		{
			return;
		}
		
		if (AI_anyAttack(1, 55))
		{
			return;
		}
		
		if (!bHostile)
		{
			if (AI_paradrop(getDropRange()))
			{
				return;
			}
		
			if (AI_offensiveAirlift())
			{
				return;
			}
		
			if (AI_moveToStagingCity())
			{
				return;
			}
			
			if (AI_guardFort(true))
			{
				return;
			}
	
			if (AI_guardCityAirlift())
			{
				return;
			}
		}

		if (collateralDamage() > 0)
		{
			if (AI_anyAttack(1, 45, 3))
			{
				return;
			}
		}

		if (AI_pillageRange(1, 15))
		{
			return;
		}
		
		if (bHostile)
		{
			if (AI_choke(1))
			{
				return;
			}
		}
		
		if (AI_heal())
		{
			return;
		}

		if (AI_retreatToCity())
		{
			return;
		}

		if (AI_protect(35))
		{
			return;
		}

		if (AI_safety())
		{
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_attackCityMove()
{
	PROFILE_FUNC();

	bool bIgnoreFaster = false;
	if (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_FASTMOVERS))
	{
		if (area()->getAreaAIType(getTeam()) != AREAAI_ASSAULT)
		{
			bIgnoreFaster = true;
		}
	}

	// force heal if we in our own city and damaged
	// can we remove this or call AI_heal here?
	if ((getGroup()->getNumUnits() == 1) && (getDamage() > 0) &&
        plot()->getOwnerINLINE() == getOwnerINLINE() && plot()->isCity())
    {
        getGroup()->pushMission(MISSION_HEAL);
		return;
    }

    AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
    bool bLandWar = !isBarbarian() && ((eAreaAIType == AREAAI_OFFENSIVE) || (eAreaAIType == AREAAI_DEFENSIVE) || (eAreaAIType == AREAAI_MASSING));

	if (plot()->isCity())
	{
		if ((GC.getGame().getGameTurn() - plot()->getPlotCity()->getGameTurnAcquired()) <= 1)
		{
			CvSelectionGroup* pOldGroup = getGroup();

			pOldGroup->AI_seperateNonAI(UNITAI_ATTACK_CITY);

			if (pOldGroup != getGroup())
			{
				return;
			}
		}

		if (plot()->getOwnerINLINE() == getOwnerINLINE())
		{
		    if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST))
		    {
		        if (AI_offensiveAirlift())
		        {
		            return;
		        }
		    }
		}
	}

	if (AI_guardCity(false, false, 1))
	{
		return;
	}

	if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 0, true, true, bIgnoreFaster))
	{
		return;
	}

	bool bCity = plot()->isCity();

	if (!bCity)
	{
		if (AI_bombardCity())
		{
			return;
		}

		//stack attack
		if (getGroup()->getNumUnits() > 1)
		{
			if (AI_stackAttackCity(1, 250, true))
			{
				return;
			}
		}

		//stack attack
		if (getGroup()->getNumUnits() > 1)
		{
			if (AI_stackAttackCity(1, 110, true))
			{
				return;
			}
		}
	}

	if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 2, true, true, bIgnoreFaster))
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	bool bHuntBarbs = false;
	if (area()->getCitiesPerPlayer(BARBARIAN_PLAYER) > 0)
	{
		if ((area()->getAreaAIType(getTeam()) != AREAAI_OFFENSIVE) && (area()->getAreaAIType(getTeam()) != AREAAI_DEFENSIVE))
		{
			bHuntBarbs = true;
		}
	}
	bool bReadyToAttack = ((getGroup()->getNumUnits() >= (bHuntBarbs ? 3 : AI_stackOfDoomExtra())));
	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (!bLandWar)
		{
			if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, NO_UNITAI, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4))
			{
				return;
			}
		}

		if (!bReadyToAttack)
		{
			int iTargetCount = GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_GROUP);
			if ((iTargetCount * 5) > getGroup()->getNumUnits())
			{
				if (AI_moveToStagingCity())
				{
					return;
				}
			}
		}
	}

	if (collateralDamage() > 0)
	{
		if (AI_anyAttack(1, 45, 3))
		{
			return;
		}
	}

	if (AI_anyAttack(1, 60))
	{
		return;
	}

	bool bAtWar = isEnemy(plot()->getTeam());

	if (bAtWar && (getGroup()->getNumUnits() <= 2))
	{
		if (AI_pillageRange(3, 11))
		{
			return;
		}

		if (AI_pillageRange(1))
		{
			return;
		}
	}

	if (AI_heal(50, 3))
	{
		return;
	}

	if (!isEnemy(plot()->getTeam()))
	{
		if (AI_heal())
		{
			return;
		}

		if ((getGroup()->getNumUnits() == 1) && (getTeam() != plot()->getTeam()))
		{
			if (AI_retreatToCity())
			{
				return;
			}
		}
	}

	if (!bReadyToAttack && !noDefensiveBonus())
	{
		if (AI_guardCity(false, false))
		{
			return;
		}
	}

	//XXX more sophisticated logic for attacking is long overdue here
//	if ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) ||
//		  (atWar(getTeam(), plot()->getTeam())) ||
//			((area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE) && (getGroup()->getNumUnits() >= AI_stackOfDoomExtra())))
	if (bReadyToAttack)
	{
		if (bHuntBarbs && AI_targetBarbCity())
		{
			return;
		}
		else if (bLandWar)
		{
			if (AI_targetCity())
			{
				return;
			}
			CvTeamAI& kTeam = GET_TEAM(getTeam());
			if (kTeam.getAnyWarPlanCount(true) > 0)
			{
				CvCity* pTargetCity = area()->getTargetCity(getOwnerINLINE());

				if (pTargetCity != nullptr)
				{
					if (AI_solveBlockageProblem(pTargetCity->plot(), (kTeam.getAtWarCount(true) == 0)))
					{
						return;
					}
				}
			}
		}
	}
	else
	{
		if ((bombardRate() > 0) && noDefensiveBonus())
		{
			if (AI_group(UNITAI_ATTACK_CITY, -1, -1, -1, bIgnoreFaster, true, true, /*iMaxPath*/ 10, /*bAllowRegrouping*/ true))
			{
				return;
			}
		}
		else
		{
			if (AI_group(UNITAI_ATTACK_CITY, AI_stackOfDoomExtra() * 2, -1, -1, bIgnoreFaster, true, true, /*iMaxPath*/ 10, /*bAllowRegrouping*/ false))
			{
				return;
			}
		}
	}

	if (AI_moveToStagingCity())
	{
		return;
	}

	if (AI_offensiveAirlift())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_attackCityLemmingMove()
{
	if (AI_cityAttack(1, 80)) 
	{ 
		return; 
	} 

	if (AI_bombardCity())
	{ 
		return; 
	} 

	if (AI_cityAttack(1, 40)) 
	{ 
		return; 
	} 

	if (AI_targetCity(MOVE_THROUGH_ENEMY)) 
	{ 
		return; 
	} 

	if (AI_anyAttack(1, 70)) 
	{ 
		return; 
	} 

	if (AI_anyAttack(1, 0)) 
	{ 
		return; 
	} 

	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_collateralMove()
{
	PROFILE_FUNC();
	
	if (AI_leaveAttack(1, 20, 100))
	{
		return;
	}

	if (AI_guardCity(false, true, 1))
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	if (AI_cityAttack(1, 35))
	{
		return;
	}

	if (AI_anyAttack(1, 45, 3))
	{
		return;
	}

	if (AI_anyAttack(1, 55, 2))
	{
		return;
	}

	if (AI_anyAttack(1, 35, 3))
	{
		return;
	}

	if (AI_anyAttack(1, 30, 4))
	{
		return;
	}

	if (AI_anyAttack(1, 20, 5))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}

	if (!noDefensiveBonus())
	{
		if (AI_guardCity(false, false))
		{
			return;
		}
	}

	if (AI_anyAttack(2, 55, 3))
	{
		return;
	}

	if (AI_cityAttack(2, 50))
	{
		return;
	}

	if (AI_anyAttack(2, 60))
	{
		return;
	}

	if (AI_protect(50))
	{
		return;
	}

	if (AI_guardCity(false, true, 3))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_pillageMove()
{
	PROFILE_FUNC();

	if (AI_guardCity(false, true, 1))
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	//join any city attacks in progress
	if (plot()->getOwnerINLINE() != getOwnerINLINE())
	{
		if (AI_groupMergeRange(UNITAI_ATTACK_CITY, 1, true, true))
		{
			return;
		}
	}
	
	if (AI_cityAttack(1, 55))
	{
		return;
	}

	if (AI_anyAttack(1, 65))
	{
		return;
	}

	if (!noDefensiveBonus())
	{
		if (AI_guardCity(false, false))
		{
			return;
		}
	}
	
	if (AI_pillageRange(3, 11))
	{
		return;
	}
	
	if (AI_choke(1))
	{
		return;
	}

	if (AI_pillageRange(1))
	{
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4))
		{
			return;
		}
	}

	if (AI_heal(50, 3))
	{
		return;
	}

	if (!isEnemy(plot()->getTeam()))
	{
		if (AI_heal())
		{
			return;
		}
	}

	if (AI_group(UNITAI_PILLAGE, /*iMaxGroup*/ 1, /*iMaxOwnUnitAI*/ 1, -1, /*bIgnoreFaster*/ true, false, false, /*iMaxPath*/ 3))
	{
		return;
	}

	if ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || isEnemy(plot()->getTeam()))
	{
		if (AI_pillage(20))
		{
			return;
		}
	}

	if (AI_heal())
	{
		return;
	}

	if (AI_guardCity(false, true, 3))
	{
		return;
	}

	if (AI_offensiveAirlift())
	{
		return;
	}

	if (AI_travelToUpgradeCity())
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_reserveMove()
{
	PROFILE_FUNC();
	
	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3) > 0);


	if (bDanger && AI_leaveAttack(2, 55, 130))
	{
		return;
	}
	
	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY))
		{
			return;
		}
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_WORKER, -1, -1, 1, -1, MOVE_SAFE_TERRITORY))
		{
			return;
		}
	}
	
	if (!bDanger)
	{
		if (AI_group(UNITAI_SETTLE, 2, -1, -1, false, false, false, 3, true))
		{
			return;
		}
	}
	
	if (AI_guardCity(true))
	{
		return;
	}
	
	if (!noDefensiveBonus())
	{
		if (AI_guardFort(false))
		{
			return;
		}
	}
		
	if (AI_guardCityAirlift())
	{
		return;
	}

	if (AI_guardCity(false, true, 1))
	{
		return;
	}
	
	if (AI_guardCitySite())
	{
		return;
	}
	
	if (!noDefensiveBonus())
	{
		if (AI_guardFort(true))
		{
			return;
		}
		
		if (AI_guardBonus(15))
		{
			return;
		}
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	if (bDanger)
	{
		if (AI_cityAttack(1, 55))
		{
			return;
		}

		if (AI_anyAttack(1, 60))
		{
			return;
		}
	}

	if (!noDefensiveBonus())
	{
		if (AI_guardCity(false, false))
		{
			return;
		}
	}
	
	if (bDanger)
	{
		if (AI_cityAttack(3, 45))
		{
			return;
		}

		if (AI_anyAttack(3, 50))
		{
			return;
		}
	}

	if (AI_protect(45))
	{
		return;
	}

	if (AI_guardCity(false, true, 3))
	{
		return;
	}

	if (AI_defend())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_counterMove()
{
	PROFILE_FUNC();

	if (AI_guardCity(false, true, 1))
	{
		return;
	}
	
	if (getSameTileHeal() > 0)
	{
		if (!canAttack())
		{
			if (AI_shadow(UNITAI_ATTACK_CITY))
			{
				return;
			}
		}
	}
	
    AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
    if (plot()->isCity())
    {
        if (plot()->getOwnerINLINE() == getOwnerINLINE())
        {
            if ((eAreaAIType == AREAAI_ASSAULT) || (eAreaAIType == AREAAI_ASSAULT_ASSIST))
            {
                if (AI_offensiveAirlift())
                {
                    return;
                }
            }
        }
    }
    
	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3) > 0);

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4))
		{
			return;
		}

		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK, -1, -1, -1, -1, MOVE_SAFE_TERRITORY, 4))
		{
			return;
		}
	}

	if (!noDefensiveBonus())
	{
		if (AI_guardCity(false, false))
		{
			return;
		}
	}

	if (bDanger)
	{
		if (AI_cityAttack(1, 30))
		{
			return;
		}

		if (AI_anyAttack(1, 40))
		{
			return;
		}
	}

			if (AI_group(UNITAI_ATTACK_CITY, /*iMaxGroup*/ -1, 2, -1, false, /*bIgnoreOwnUnitType*/ true, /*bStackOfDoom*/ true, /*iMaxPath*/ 6))
			{
				return;
			}
	
	bool bFastMovers = (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_FASTMOVERS));

	if (AI_group(UNITAI_ATTACK, /*iMaxGroup*/ 2, -1, -1, bFastMovers, /*bIgnoreOwnUnitType*/ true, /*bStackOfDoom*/ true, /*iMaxPath*/ 5))
	{
		return;
	}
	
	if (AI_group(UNITAI_ATTACK_CITY, /*iMaxGroup*/ -1, 2, -1, false, /*bIgnoreOwnUnitType*/ true, /*bStackOfDoom*/ true, /*iMaxPath*/ 6))
	{
		return;
	}
	
	if (AI_guardCity(false, true, 3))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}

	if (AI_offensiveAirlift())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityDefenseMove()
{
	PROFILE_FUNC();
	
	bool bDanger = (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3) > 0);
	
	if (bDanger)
	{
		if (AI_leaveAttack(1, 70, 175))
		{
			return;
		}

		if (AI_chokeDefend())
		{
			return;
		}
	}

	if (AI_guardCityBestDefender())
	{
		return;
	}
	
	if (!bDanger)
	{
		if (plot()->getOwnerINLINE() == getOwnerINLINE())
		{
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY, 1))
			{
				return;
			}
		}
	}
	
	if (AI_guardCityMinDefender(true))
	{
		return;
	}

	if (AI_guardCity(true))
	{
		return;
	}
	
	if (!bDanger)
	{
		if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 1, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
		{
			return;
		}

		if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 2, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
		{
			return;
		}

		if (plot()->getOwnerINLINE() == getOwnerINLINE())
		{
			if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, -1, -1, 1, -1, MOVE_SAFE_TERRITORY))
			{
				return;
			}
		}
	}
	
	AreaAITypes eAreaAI = area()->getAreaAIType(getTeam());
	if ((eAreaAI == AREAAI_ASSAULT) || (eAreaAI == AREAAI_ASSAULT_MASSING) || (eAreaAI == AREAAI_ASSAULT_ASSIST))
	{
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, -1, -1, -1, 0, MOVE_SAFE_TERRITORY))
		{
			return;
		}		
	}
	
	if ((AI_getBirthmark() % 4) == 0)
	{
		if (AI_guardFort())
		{
			return;
		}
	}

	if (AI_guardCityAirlift())
	{
		return;
	}

	if (AI_guardCity(false, true, 1))
	{
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY))
		{
			// will enter here if in danger
			return;
		}
	}

	if (AI_guardCity(false, true))
	{
		return;
	}
	if (!isBarbarian() && ((area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE) || (area()->getAreaAIType(getTeam()) == AREAAI_MASSING)))
	{
			if (AI_group(UNITAI_ATTACK_CITY, -1, 2, 4, /*bIgnoreFaster*/ true))
			{
				return;
			}
		}
	
	if (area()->getAreaAIType(getTeam()) == AREAAI_ASSAULT)
	{
		if (AI_load(UNITAI_ASSAULT_SEA, MISSIONAI_LOAD_ASSAULT, UNITAI_ATTACK_CITY, 1, 2, -1, 1, MOVE_SAFE_TERRITORY))
		{
			// does this ever occur? the previous settler check is less strict, this one should never be true (I think)
			FAssertMsg(false, "unexpected settler load (non-fatal)");
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityDefenseExtraMove()
{
	PROFILE_FUNC();

	CvCity* pCity;

	if (AI_leaveAttack(2, 55, 150))
	{
		return;
	}
	
	if (AI_chokeDefend())
	{
		return;
	}

	if (AI_guardCityBestDefender())
	{
		return;
	}

	if (AI_guardCity(true))
	{
		return;
	}

	if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 1, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
	{
		return;
	}

	if (AI_group(UNITAI_SETTLE, /*iMaxGroup*/ 2, -1, -1, false, false, false, /*iMaxPath*/ 2, /*bAllowRegrouping*/ true))
	{
		return;
	}

	pCity = plot()->getPlotCity();

	if ((pCity != nullptr) && (pCity->getOwnerINLINE() == getOwnerINLINE())) // XXX check for other team?
	{
		if (plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isUnitAIType, AI_getUnitAIType()) == 1)
		{
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}

	if (AI_guardCityAirlift())
	{
		return;
	}

	if (AI_guardCity(false, true, 1))
	{
		return;
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		if (AI_load(UNITAI_SETTLER_SEA, MISSIONAI_LOAD_SETTLER, UNITAI_SETTLE, 3, -1, -1, -1, MOVE_SAFE_TERRITORY, 3))
		{
			return;
		}
	}

	if (AI_guardCity(false, true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_exploreMove()
{
	PROFILE_FUNC();

	if (!isHuman() && canAttack())
	{
		if (AI_cityAttack(1, 60))
		{
			return;
		}

		if (AI_anyAttack(1, 70))
		{
			return;
		}
	}

	if (getDamage() > 0)
	{
		if ((plot()->getFeatureType() == NO_FEATURE) || (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() == 0))
		{
			getGroup()->pushMission(MISSION_HEAL);
			return;
		}
	}

	if (!isHuman())
	{
		if (AI_pillageRange(1))
		{
			return;
		}

		if (AI_cityAttack(3, 80))
		{
			return;
		}
	}

	if (AI_goody(4))
	{
		return;
	}

	if (AI_exploreRange(3))
	{
		return;
	}

	if (!isHuman())
	{
		if (AI_pillageRange(3))
		{
			return;
		}
	}

	if (AI_explore())
	{
		return;
	}

	if (!isHuman())
	{
		if (AI_pillage())
		{
			return;
		}
	}

	if (!isHuman())
	{
		if (AI_travelToUpgradeCity())
		{
			return;
		}
	}

	if (!isHuman() && (AI_getUnitAIType() == UNITAI_EXPLORE))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(area(), UNITAI_EXPLORE) > GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(area()))
		{
			if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0)
			{
				scrap();
				return;
			}
		}
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missionaryMove()
{
	PROFILE_FUNC();

	if (AI_spreadReligion())
	{
		return;
	}

	if (AI_spreadCorporation())
	{
		return;
	}
	
	if (!isHuman() || (isAutomated() && GET_TEAM(getTeam()).getAtWarCount(true) == 0))
	{
		if (!isHuman() || (getGameTurnCreated() < GC.getGame().getGameTurn()))
		{
			if (AI_spreadReligionAirlift())
			{
				return;
			}
			if (AI_spreadCorporationAirlift())
			{
				return;
			}
		}
		
		if (!isHuman())
		{
			if (AI_load(UNITAI_MISSIONARY_SEA, MISSIONAI_LOAD_SPECIAL, NO_UNITAI, -1, -1, -1, 0, MOVE_SAFE_TERRITORY))
			{
				return;
			}

			if (AI_load(UNITAI_MISSIONARY_SEA, MISSIONAI_LOAD_SPECIAL, NO_UNITAI, -1, -1, -1, 0, MOVE_NO_ENEMY_TERRITORY))
			{
				return;
			}
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_prophetMove()
{
	PROFILE_FUNC();

	if (AI_construct(1))
	{
		return;
	}

	if (AI_discover(true, true))
	{
		return;
	}

	if (AI_construct(3))
	{
		return;
	}
	
	int iGoldenAgeValue = (GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge()));
	int iDiscoverValue = std::max(1, getDiscoverResearch(NO_TECH));

	if (((iGoldenAgeValue * 100) / iDiscoverValue) > 60)
	{
        if (AI_goldenAge())
        {
            return;
        }

        if (iDiscoverValue > iGoldenAgeValue)
        {
            if (AI_discover())
            {
                return;
            }
            if (GET_PLAYER(getOwnerINLINE()).getUnitClassCount(getUnitClassType()) > 1)
            {
                if (AI_join())
                {
                    return;
                }
            }
        }
	}
	else
	{
		if (AI_discover())
		{
			return;
		}

		if (AI_join())
		{
			return;
		}
	}

	if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) ||
		  (getGameTurnCreated() < (GC.getGameINLINE().getGameTurn() - 25)))
	{
		if (AI_discover())
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_artistMove()
{
	PROFILE_FUNC();
	
	if (AI_artistCultureVictoryMove())
	{
	    return;
	}

	if (AI_construct())
	{
		return;
	}

	if (AI_discover(true, true))
	{
		return;
	}

	if (AI_greatWork())
	{
		return;
	}

	int iGoldenAgeValue = (GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge()));
	int iDiscoverValue = std::max(1, getDiscoverResearch(NO_TECH));

	if (((iGoldenAgeValue * 100) / iDiscoverValue) > 60)
	{
        if (AI_goldenAge())
        {
            return;
        }

        if (iDiscoverValue > iGoldenAgeValue)
        {
            if (AI_discover())
            {
                return;
            }
            if (GET_PLAYER(getOwnerINLINE()).getUnitClassCount(getUnitClassType()) > 1)
            {
                if (AI_join())
                {
                    return;
                }
            }
        }
	}
	else
	{
		if (AI_discover())
		{
			return;
		}

		if (AI_join())
		{
			return;
		}
	}

	if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) ||
		  (getGameTurnCreated() < (GC.getGameINLINE().getGameTurn() - 25)))
	{
		if (AI_discover())
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_scientistMove()
{
	PROFILE_FUNC();

	if (AI_discover(true, true))
	{
		return;
	}

	if (AI_construct(INT_MAX, 1))
	{
		return;
	}
	if (std::to_underlying(GET_PLAYER(getOwnerINLINE()).getCurrentEra()) < 3)
	{
		if (AI_join(2))
		{
			return;
		}		
	}
	
	if (GET_PLAYER(getOwnerINLINE()).getCurrentEra() <= (GC.getNumEraInfos() / 2))
	{
		if (AI_construct())
		{
			return;
		}
	}

	int iGoldenAgeValue = (GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge()));
	int iDiscoverValue = std::max(1, getDiscoverResearch(NO_TECH));

	if (((iGoldenAgeValue * 100) / iDiscoverValue) > 60)
	{
        if (AI_goldenAge())
        {
            return;
        }

        if (iDiscoverValue > iGoldenAgeValue)
        {
            if (AI_discover())
            {
                return;
            }
            if (GET_PLAYER(getOwnerINLINE()).getUnitClassCount(getUnitClassType()) > 1)
            {
                if (AI_join())
                {
                    return;
                }
            }
        }
	}
	else
	{
		if (AI_discover())
		{
			return;
		}

		if (AI_join())
		{
			return;
		}
	}

	if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) ||
		  (getGameTurnCreated() < (GC.getGameINLINE().getGameTurn() - 25)))
	{
		if (AI_discover())
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_generalMove()
{
	PROFILE_FUNC();

	std::vector<UnitAITypes> aeUnitAITypes;
	int iDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2);
	
	bool bOffenseWar = (area()->getAreaAIType(getTeam()) == AREAAI_OFFENSIVE);
	
	
	if (iDanger > 0)
	{
		aeUnitAITypes.clear();
		aeUnitAITypes.push_back(UNITAI_ATTACK);
		aeUnitAITypes.push_back(UNITAI_COUNTER);
		if (AI_lead(aeUnitAITypes))
		{
			return;
		}
	}
	
	if (AI_construct(1))
	{
		return;
	}
	if (AI_join(1))
	{
		return;
	}
	
	if (bOffenseWar && (GC.getGameINLINE().getSorenRandNum(2, "AI General Lead") == 0))
	{
		aeUnitAITypes.clear();
		aeUnitAITypes.push_back(UNITAI_ATTACK_CITY);
		if (AI_lead(aeUnitAITypes))
		{
			return;
		}
	}
	
	
	if (AI_join(2))
	{
		return;
	}
	
	if (AI_construct(2))
	{
		return;
	}
	if (AI_join(4))
	{
		return;
	}
	
	if (GC.getGameINLINE().getSorenRandNum(3, "AI General Construct") == 0)
	{
		if (AI_construct())
		{
			return;
		}
	}

	if (AI_join())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_merchantMove()
{
	PROFILE_FUNC();

	if (AI_construct())
	{
		return;
	}

	if (AI_discover(true, true))
	{
		return;
	}

	int iGoldenAgeValue = (GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge()));
	int iDiscoverValue = std::max(1, getDiscoverResearch(NO_TECH));

	if (AI_trade(iGoldenAgeValue * 2))
	{
	    return;
	}

	if (((iGoldenAgeValue * 100) / iDiscoverValue) > 60)
	{
        if (AI_goldenAge())
        {
            return;
        }

        if (AI_trade(iGoldenAgeValue))
        {
            return;
        }

        if (iDiscoverValue > iGoldenAgeValue)
        {
            if (AI_discover())
            {
                return;
            }
            if (GET_PLAYER(getOwnerINLINE()).getUnitClassCount(getUnitClassType()) > 1)
            {
                if (AI_join())
                {
                    return;
                }
            }
        }
	}
	else
	{
		if (AI_discover())
		{
			return;
		}

		if (AI_join())
		{
			return;
		}
	}

	if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) ||
		  (getGameTurnCreated() < (GC.getGameINLINE().getGameTurn() - 25)))
	{
		if (AI_discover())
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_engineerMove()
{
	PROFILE_FUNC();

	if (AI_construct())
	{
		return;
	}

	if (AI_switchHurry())
	{
		return;
	}

	if (AI_hurry())
	{
		return;
	}

	if (AI_discover(true, true))
	{
		return;
	}

	int iGoldenAgeValue = (GET_PLAYER(getOwnerINLINE()).AI_calculateGoldenAgeValue() / (GET_PLAYER(getOwnerINLINE()).unitsRequiredForGoldenAge()));
	int iDiscoverValue = std::max(1, getDiscoverResearch(NO_TECH));

	if (((iGoldenAgeValue * 100) / iDiscoverValue) > 60)
	{
        if (AI_goldenAge())
        {
            return;
        }

        if (iDiscoverValue > iGoldenAgeValue)
        {
            if (AI_discover())
            {
                return;
            }
            if (GET_PLAYER(getOwnerINLINE()).getUnitClassCount(getUnitClassType()) > 1)
            {
                if (AI_join())
                {
                    return;
                }
            }
        }
	}
	else
	{
		if (AI_discover())
		{
			return;
		}

		if (AI_join())
		{
			return;
		}
	}

	if ((GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) ||
		  (getGameTurnCreated() < (GC.getGameINLINE().getGameTurn() - 25)))
	{
		if (AI_discover())
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_spyMove()
{
	CvTeamAI& kTeam = GET_TEAM(getTeam());
	int iEspionageChance = 0;
	if (plot()->isOwned() && (plot()->getTeam() != getTeam()))
	{
		switch (kTeam.AI_getAttitude(plot()->getTeam()))
		{
		case ATTITUDE_FURIOUS:
			iEspionageChance = 100;
			break;

		case ATTITUDE_ANNOYED:
			iEspionageChance = 50;
			break;

		case ATTITUDE_CAUTIOUS:
			iEspionageChance = 0;
			break;

		case ATTITUDE_PLEASED:
			iEspionageChance = 0;
			break;

		case ATTITUDE_FRIENDLY:
			iEspionageChance = 0;
			break;

		default:
			FAssert(false);
			break;
		}
		
		WarPlanTypes eWarPlan = kTeam.AI_getWarPlan(plot()->getTeam());
		if (eWarPlan != NO_WARPLAN)
		{
			if (eWarPlan == WARPLAN_LIMITED)
			{
				iEspionageChance += 50;
			}
			else
			{
				iEspionageChance += 10;
			}
		}
		
		if (plot()->isCity() && plot()->getTeam() != getTeam())
		{
			if (getFortifyTurns() >= GC.getDefineINT("MAX_FORTIFY_TURNS"))
			{
				if (AI_espionageSpy())
				{
					return;
				}
			}
			if (GC.getGame().getSorenRandNum(100, "AI Spy Skip Turn") > 5)
			{
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
		
		if (GC.getGameINLINE().getSorenRandNum(100, "AI Spy Espionage") < iEspionageChance)
		{
			if (AI_espionageSpy())
			{
				return;
			}
		}
	}
	
	if (plot()->getTeam() == getTeam())
	{
		if (kTeam.getAnyWarPlanCount(true) == 0)
		{
			if (AI_guardSpy(0))
			{
				return;			
			}
		}
		
		if (GC.getGame().getSorenRandNum(100, "AI Spy pillage improvement") < 25)
		{
			if (AI_bonusOffenseSpy(3))
			{
				return;
			}
		}
		else
		{
			if (AI_cityOffenseSpy(10))
			{
				return;
			}
		}
	}
	
	if (iEspionageChance > 0 && (plot()->isCity() || (plot()->getNonObsoleteBonusType(getTeam()) != NO_BONUS)))
	{
		if (GC.getGame().getSorenRandNum(7, "AI Spy Skip Turn") > 0)
		{
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}
	

	if (GC.getGame().getSorenRandNum(4, "AI Spy Choose Movement"))
	{
		if (AI_reconSpy(3))
		{
			return;
		}
	}
	else
	{
		if (AI_cityOffenseSpy(10))
		{
			return;
		}
	}
	
//	int iInfiltrateChance = 25;
//	if (plot()->getPlotCity() != nullptr)
//	{
//		if (plot()->getTeam() != getTeam())
//		{
//			iInfiltrateChance += 50;
//		}
//	}

//	if (GC.getGameINLINE().getSorenRandNum(100, "AI Spy Infiltrate") < iInfiltrateChance)
//	{
//		if (AI_infiltrate())
//		{
//			return;
//		}
//	}
	


	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_ICBMMove()
{
//	CvCity* pCity = plot()->getPlotCity();

//	if (pCity != nullptr)
//	{
//		if (pCity->AI_isDanger())
//		{
//			if (!(pCity->AI_isDefended()))
//			{
//				if (AI_airCarrier())
//				{
//					return;
//				}
//			}
//		}
//	}
	
	if (airRange() > 0)
	{
		if (AI_nukeRange(airRange()))
		{
			return;
		}
	}
	else if (AI_nuke())
	{
		return;
	}

	if (isCargo())
	{
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}
	
	if (airRange() > 0)
	{
		if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA, 2, true))
		{
			return;
		}
		
		if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA, 1, false))
		{
			return;
		}
		
		if (AI_getBirthmark() % 3 == 0)
		{
			if (AI_missileLoad(UNITAI_ATTACK_SEA, 0, false))
			{
				return;
			}
		}
		
		if (AI_airOffensiveCity())
		{
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_workerSeaMove()
{
	PROFILE_FUNC();

	CvCity* pCity;
	
	int iI;

	if (!(getGroup()->canDefend()))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot()) > 0)
		{
			if (AI_retreatToCity())
			{
				return;
			}
		}
	}

	if (AI_improveBonus(20))
	{
		return;
	}

	if (AI_improveBonus(10))
	{
		return;
	}

	if (AI_improveBonus())
	{
		return;
	}
	
	if (isHuman())
	{
		FAssert(isAutomated());
		if (plot()->getBonusType() != NO_BONUS)
		{
			if ((plot()->getOwnerINLINE() == getOwnerINLINE()) || (!plot()->isOwned()))
			{
				getGroup()->pushMission(MISSION_SKIP);
				return;
			}
		}
		
		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			CvPlot* pLoopPlot = plotDirection(getX_INLINE(), getY_INLINE(), (DirectionTypes)iI);
			if (pLoopPlot != nullptr)
			{
				if (pLoopPlot->getBonusType() != NO_BONUS)
				{
					if (pLoopPlot->isValidDomainForLocation(*this))
					{
						getGroup()->pushMission(MISSION_SKIP);
						return;						
					}					
				}
			}
		}
	}
	
	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_WORKER_SEA))
	{
		pCity = plot()->getPlotCity();

		if (pCity != nullptr)
		{
			if (pCity->getOwnerINLINE() == getOwnerINLINE())
			{
				if (pCity->AI_neededSeaWorkers() == 0)
				{
					if (GC.getGameINLINE().getElapsedGameTurns() > 10)
					{
						if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0)
						{
							scrap();
							return;
						}
					}
				}
				else
				{
					//Probably icelocked since it can't perform actions.
					scrap();
					return;
				}	
			}
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_barbAttackSeaMove()
{
	PROFILE_FUNC();

	if (GC.getGameINLINE().getSorenRandNum(2, "AI Barb") == 0)
	{
		if (AI_pillageRange(1))
		{
			return;
		}
	}

	if (AI_anyAttack(2, 25))
	{
		return;
	}

	if (AI_pillageRange(4))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_pirateSeaMove()
{
	PROFILE_FUNC();

	CvArea* pWaterArea;

	if (plot()->isCity())
	{
		if (AI_heal())
		{
			return;
		}
	}
	if (plot()->isOwned() && (plot()->getTeam() == getTeam()))
	{
		if (AI_anyAttack(2, 30))
		{
			return;			
		}
		
		if (AI_protect(30))
		{
			return;
		}
		
		if (((AI_getBirthmark() / 8) % 2) == 0)
		{
			if (AI_group(UNITAI_PIRATE_SEA, 1, 0))
			{
				return;
			}
		}
	}
	else
	{
		if (AI_anyAttack(2, 51))
		{
			return;
		}
	}

	
	if (GC.getGame().getSorenRandNum(10, "AI Pirate Explore") == 0)
	{
		pWaterArea = plot()->waterArea();

		if (pWaterArea != nullptr)
		{
			if (pWaterArea->getNumUnrevealedTiles(getTeam()) > 0)
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_areaMissionAIs(pWaterArea, MISSIONAI_EXPLORE, getGroup()) < (GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(pWaterArea)))
				{
					if (AI_exploreRange(2))
					{
						return;
					}
				}
			}
		}
	}

	if (GC.getGame().getSorenRandNum(11, "AI Pirate Pillage") == 0)
	{
		if (AI_pillageRange(1))
		{
			return;
		}
	}
	
	//Includes heal and retreat to sea routines.
	if (AI_pirateBlockade())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_attackSeaMove()
{
	PROFILE_FUNC();

	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}
	
	if (AI_anyAttack(1, 35))
	{
		return;
	}
	
	if (AI_anyAttack(2, 40))
	{
		return;
	}
	
	if (AI_seaBombardRange(6))
	{
		return;
	}
	
	if (AI_heal(50, 3))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}
	
	if (AI_group(UNITAI_CARRIER_SEA, /*iMaxGroup*/ 4, 1, -1, true, false, false, /*iMaxPath*/ 5))
	{
		return;
	}
	
	if (AI_group(UNITAI_ATTACK_SEA, /*iMaxGroup*/ 1, -1, -1, true, false, false, /*iMaxPath*/ 3))
	{
		return;
	}
	
	if (!plot()->isOwned() || !isEnemy(plot()->getTeam()))
	{
		if (AI_shadow(UNITAI_ASSAULT_SEA, 4, 34))
		{
			return;
		}
		
		if (AI_shadow(UNITAI_CARRIER_SEA, 4, 51))
		{
			return;
		}
		if (AI_group(UNITAI_ASSAULT_SEA, -1, 4, -1, false, false, false))
		{
			return;
		}
	}
	
	if (AI_group(UNITAI_CARRIER_SEA, -1, 1, -1, false, false, false))
	{
		return;
	}
	
	if (plot()->isOwned() && (isEnemy(plot()->getTeam())))
	{
		if (AI_blockade())
		{
			return;
		}
	}

	if (AI_pillageRange(4))
	{
		return;
	}
	
	if (AI_protect(35))
	{
		return;
	}

	if (AI_travelToUpgradeCity())
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_reserveSeaMove()
{
	PROFILE_FUNC();

	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	if (AI_guardBonus(30))
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	if (AI_anyAttack(1, 55))
	{
		return;
	}
	
	if (AI_seaBombardRange(6))
	{
		return;
	}
	
	if (AI_protect(40))
	{
		return;
	}
	
	if (AI_shadow(UNITAI_SETTLER_SEA, 1, -1, true))
	{
		return;
	}
	
	if (AI_group(UNITAI_RESERVE_SEA, 1))
	{
		return;
	}
	
	if (bombardRate() > 0)
	{
		if (AI_shadow(UNITAI_ASSAULT_SEA, 2, 30, true))
		{
			return;
		}
	}

	if (AI_heal(50, 3))
	{
		return;
	}

	if (AI_anyAttack(3, 45))
	{
		return;
	}

	if (AI_heal())
	{
		return;
	}

	if (!isNeverInvisible())
	{
		if (AI_anyAttack(5, 35))
		{
			return;
		}
	}

	if (AI_travelToUpgradeCity())
	{
		return;
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_escortSeaMove()
{
	PROFILE_FUNC();

//	// if we have cargo, possibly convert to UNITAI_ASSAULT_SEA (this will most often happen with galleons)
//	// note, this should not happen when we are not the group head, so escort galleons are fine joining a group, just not as head
//	if (hasCargo() && (getUnitAICargo(UNITAI_ATTACK_CITY) > 0 || getUnitAICargo(UNITAI_ATTACK) > 0))
//	{
//		// non-zero AI_unitValue means that UNITAI_ASSAULT_SEA is valid for this unit (that is the check used everywhere)
//		if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_ASSAULT_SEA, nullptr) > 0)
//		{
//			// save old group, so we can merge it back in
//			CvSelectionGroup* pOldGroup = getGroup();
//
//			// this will remove this unit from the current group
//			AI_setUnitAIType(UNITAI_ASSAULT_SEA);
//
//			// merge back the rest of the group into the new group
//			CvSelectionGroup* pNewGroup = getGroup();
//			if (pOldGroup != pNewGroup)
//			{
//				pOldGroup->mergeIntoGroup(pNewGroup);
//			}
//
//			// perform assault sea action
//			AI_assaultSeaMove();
//			return;
//		}
//	}

	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	if (AI_heal(30, 1))
	{
		return;
	}

	if (AI_anyAttack(1, 55))
	{
		return;
	}
	
	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 0, -1, /*bIgnoreFaster*/ true))
	{
		return;
	}
		
	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 0, -1, /*bIgnoreFaster*/ true, false, false, /*iMaxPath*/ 3))
	{
		return;
	}

	if (AI_heal(50, 3))
	{
		return;
	}

	if (AI_pillageRange(2))
	{
		return;
	}
	
	if (AI_group(UNITAI_MISSILE_CARRIER_SEA, 1, 1, true))
	{
		return;
	}

	if (AI_group(UNITAI_ASSAULT_SEA, 1, /*iMaxOwnUnitAI*/ 0, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true))
	{
		return;
	}
	
	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 2, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true))
	{
		return;
	}
	
	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 2, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true))
	{
		return;
	}
	
	if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ 4, /*iMinUnitAI*/ -1, /*bIgnoreFaster*/ true))
	{
		return;
	}	

	if (AI_heal())
	{
		return;
	}

	if (AI_travelToUpgradeCity())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_exploreSeaMove()
{
	PROFILE_FUNC();

	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	CvArea* pWaterArea = plot()->waterArea();

	if (!isHuman())
	{
		if (AI_anyAttack(1, 60))
		{
			return;
		}
	}
	
	if (!isHuman() && !isBarbarian()) //XXX move some of this into a function? maybe useful elsewhere
	{
		//Obsolete?
		int iValue = GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), AI_getUnitAIType(), area());
		int iBestValue = GET_PLAYER(getOwnerINLINE()).AI_bestAreaUnitAIValue(AI_getUnitAIType(), area());
		
		if (iValue < iBestValue)
		{
			//Transform
			if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_WORKER_SEA, area()) > 0)
			{
				AI_setUnitAIType(UNITAI_WORKER_SEA);
				return;
			}
			
			if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_PIRATE_SEA, area()) > 0)
			{
				AI_setUnitAIType(UNITAI_PIRATE_SEA);
				return;
			}
			
			if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_MISSIONARY_SEA, area()) > 0)
			{
				AI_setUnitAIType(UNITAI_MISSIONARY_SEA);
				return;
			}
			
			if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_RESERVE_SEA, area()) > 0)
			{
				AI_setUnitAIType(UNITAI_RESERVE_SEA);
				return;
			}
			scrap();
		}		
	}

	if (getDamage() > 0)
	{
		if ((plot()->getFeatureType() == NO_FEATURE) || (GC.getFeatureInfo(plot()->getFeatureType()).getTurnDamage() == 0))
		{
			getGroup()->pushMission(MISSION_HEAL);
			return;
		}
	}

	if (!isHuman())
	{
		if (AI_pillageRange(1))
		{
			return;
		}
	}

	if (AI_exploreRange(4))
	{
		return;
	}

	if (!isHuman())
	{
		if (AI_pillageRange(4))
		{
			return;
		}
	}

	if (AI_explore())
	{
		return;
	}

	if (!isHuman())
	{
		if (AI_pillage())
		{
			return;
		}
	}
	
	if (!isHuman())
	{
		if (AI_travelToUpgradeCity())
		{
			return;
		}
	}

	if (!(isHuman()) && (AI_getUnitAIType() == UNITAI_EXPLORE_SEA))
	{
		pWaterArea = plot()->waterArea();

		if (pWaterArea != nullptr)
		{
			if (GET_PLAYER(getOwnerINLINE()).AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_EXPLORE_SEA) > GET_PLAYER(getOwnerINLINE()).AI_neededExplorers(pWaterArea))
			{
				if (GET_PLAYER(getOwnerINLINE()).calculateUnitCost() > 0)
				{
					scrap();
					return;
				}
			}
		}
	}

	if (AI_patrol())
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_assaultSeaMove()
{
	PROFILE_FUNC();

	FAssert(AI_getUnitAIType() == UNITAI_ASSAULT_SEA);

	bool bEmpty = !getGroup()->hasCargo();
	if (bEmpty)
	{
		if (AI_anyAttack(1, 65))
		{
			return;
		}
		if (AI_anyAttack(1, 40))
		{
			return;
		}
	}
	bool bAttack = false;
	bool bLandWar = false;
	bool bIsCity = plot()->isCity();
	int iTargetStackSize = std::max(4, 1 + (AI_stackOfDoomExtra()));
	int iCargo = getGroup()->getCargo();
	bool bFull = getGroup()->AI_isFull();
	
	AreaAITypes eAreaAIType = area()->getAreaAIType(getTeam());
	bLandWar = !isBarbarian() && ((eAreaAIType == AREAAI_OFFENSIVE) || (eAreaAIType == AREAAI_DEFENSIVE) || (eAreaAIType == AREAAI_MASSING));
	
	if (bIsCity)
	{
		if (eAreaAIType == AREAAI_ASSAULT)
		{
			if (iCargo >= iTargetStackSize)
			{
				bAttack = true;
			}
		}
		if (!bAttack)
		{
			if (eAreaAIType == AREAAI_ASSAULT && iCargo > 0)
			{
				int iAttackers = GET_PLAYER(getOwnerINLINE()).AI_enemyTargetMissionAIs(MISSIONAI_ASSAULT, getGroup());
				if (iAttackers >= iTargetStackSize)
				{
					if (bFull)
					{
						//Join the attack
						bAttack = true;
					}
				}
			}
			else if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2))
			{		
				if (getGroup()->hasCargo())
				{
					getGroup()->unloadAll();
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
			else if (bLandWar)
			{
				if (bFull && (eAreaAIType != AREAAI_DEFENSIVE))
				{
					if (AI_assaultSeaTransport(false))
					{
						return;
					}
				}
				else if (iCargo > 0)
				{
					getGroup()->unloadAll();
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
			else if (GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_ASSAULT) > 0)
			{
				if (!bFull)
				{
					getGroup()->pushMission(MISSION_SKIP);
					return;
				}
			}
			else
			{
				if (!bLandWar && iCargo > 0)
				{
					if (AI_group(UNITAI_ASSAULT_SEA))
					{
						return;
					}
				}
			}
		}
	}
	
	if (plot()->getTeam() == getTeam())
	{
		if ((iCargo == 0) && getGroup()->getNumUnits() > 1)
		{
			getGroup()->AI_makeForceSeparate();
		}
	}
	
	if (!bIsCity)
	{
	 	if (plot()->isOwned() && isEnemy(plot()->getTeam()))
		{
			if (iCargo == 0)
			{
				// if we just made a dropoff, bombard the city if we can
				if ((getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA) + getGroup()->countNumUnitAIType(UNITAI_RESERVE_SEA)) > 0)
				{
					bool bMissionPushed = false;
					
					if (AI_seaBombardRange(1))
					{
						bMissionPushed = true;
					}

					CvSelectionGroup* pOldGroup = getGroup();

						//Release any Warships to finish the job.
						getGroup()->AI_seperateAI(UNITAI_ATTACK_SEA);
						getGroup()->AI_seperateAI(UNITAI_RESERVE_SEA);

						// fortsnek: Clang complained about enum comparison here. Casting to int to preserve bug.
					if (pOldGroup == getGroup() && (int)getUnitType() == (int)UNITAI_ASSAULT_SEA)
					{
						if (AI_retreatToCity(true))
						{
							bMissionPushed = true;
						}
					}

					if (bMissionPushed)
					{
						return;
					}
				}
			}	
		}
	
		if ((iCargo > iTargetStackSize) || bFull)
		{
			bAttack = true;		
		}
	}
	
	if (isBarbarian())
	{
		if (getGroup()->isFull())
		{
			if (AI_assaultSeaTransport(false))
			{
				return;
			}
		}
		else
		{
			if (AI_pickup(UNITAI_ATTACK_CITY))
			{
				return;
			}

			if (AI_pickup(UNITAI_ATTACK))
			{
				return;
			}
		}
	}
	else
	{
		bool bAttackBarbarian = false;

		if (GET_TEAM(getTeam()).getAtWarCount(true) == 0)
		{
			bAttackBarbarian = true;
		}
		
		if (bAttack)
		{
			FAssert(getGroup()->hasCargo());
			if (AI_assaultSeaTransport(bAttackBarbarian))
			{
				return;
			}
		}
	}
	
	if (bFull)
	{
		if (AI_group(UNITAI_ASSAULT_SEA, -1, /*iMaxOwnUnitAI*/ -1, -1, true))
		{
			return;
		}
	}
	else
	{
		if (AI_pickup(UNITAI_ATTACK_CITY))
		{
			return;
		}

		if (AI_pickup(UNITAI_ATTACK))
		{
			return;
		}
		
		if (AI_pickup(UNITAI_COUNTER))
		{
			return;
		}
	}
		
	// if we are in a city, and at/preparing land war, and we have cargo, unload, even if full, since AI_assaultSeaTransport already had its chance
	if (bIsCity && bLandWar && getGroup()->hasCargo())
	{
		getGroup()->unloadAll();
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (AI_retreatToCity(true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_settlerSeaMove()
{
	PROFILE_FUNC();
	
	bool bEmpty = !getGroup()->hasCargo();
	if (bEmpty)
	{
		if (AI_anyAttack(1, 65))
		{
			return;
		}
		if (AI_anyAttack(1, 40))
		{
			return;
		}		
	}
	
	int iSettlerCount = getUnitAICargo(UNITAI_SETTLE);
	int iWorkerCount = getUnitAICargo(UNITAI_WORKER);

	if ((iSettlerCount > 0) && (isFull() ||
			((getUnitAICargo(UNITAI_CITY_DEFENSE) > 0) &&
			 (getUnitAICargo(UNITAI_WORKER) > 0) &&
			 (GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SETTLER) == 0))))
	{
		if (AI_settlerSeaTransport())
		{
			return;
		}
	}
	else if ((getTeam() != plot()->getTeam()) && bEmpty)
	{
		if (AI_pillageRange(3))
		{
			return;
		}
	}
	
	if (plot()->isCity() && !hasCargo())
	{
		AreaAITypes eAreaAI = area()->getAreaAIType(getTeam());
		if ((eAreaAI == AREAAI_ASSAULT) || (eAreaAI == AREAAI_ASSAULT_MASSING))
		{
			CvArea* pWaterArea = plot()->waterArea();
			FAssert(pWaterArea != nullptr);
			if (pWaterArea != nullptr)
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_SETTLER_SEA) > 1)
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_unitValue(getUnitType(), UNITAI_ASSAULT_SEA, pWaterArea) > 0)
					{
						AI_setUnitAIType(UNITAI_ASSAULT_SEA);
						AI_assaultSeaMove();
						return;
					}
				}
			}
		}
	}
	
	if ((iWorkerCount > 0)
		&& GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_LOAD_SETTLER) == 0)
	{
		if (isFull() || (iSettlerCount == 0))
		{
			if (AI_settlerSeaFerry())
			{
				return;
			}
		}
	}
	
	if (AI_pickup(UNITAI_SETTLE))
	{
		return;
	}
	
	if ((GC.getGame().getGameTurn() - getGameTurnCreated()) < 8)
	{
		if ((plot()->getPlotCity() == nullptr) || GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(plot()->area(), UNITAI_SETTLE) == 0)
		{
			if (AI_explore())
			{
				return;
			}
		}
	}
	

	if (AI_pickup(UNITAI_WORKER))
	{
		return;
	}
		
	if (AI_retreatToCity(true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missionarySeaMove()
{
	PROFILE_FUNC();

	if (getUnitAICargo(UNITAI_MISSIONARY) > 0)
	{
		if (AI_specialSeaTransportMissionary())
		{
			return;
		}
	}
	else if (!(getGroup()->hasCargo()))
	{
		if (AI_pillageRange(4))
		{
			return;
		}
	}

	if (AI_pickup(UNITAI_MISSIONARY))
	{
		return;
	}
	
	if (AI_explore())
	{
		return;
	}

	if (AI_retreatToCity(true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_spySeaMove()
{
	PROFILE_FUNC();

	CvCity* pCity;

	if (getUnitAICargo(UNITAI_SPY) > 0)
	{
		if (AI_specialSeaTransportSpy())
		{
			return;
		}

		pCity = plot()->getPlotCity();

		if (pCity != nullptr)
		{
			if (pCity->getOwnerINLINE() == getOwnerINLINE())
			{
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, pCity->plot());
				return;
			}
		}
	}
	else if (!(getGroup()->hasCargo()))
	{
		if (AI_pillageRange(5))
		{
			return;
		}
	}

	if (AI_pickup(UNITAI_SPY))
	{
		return;
	}

	if (AI_retreatToCity(true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_carrierSeaMove()
{
	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	if (AI_heal(50))
	{
		return;
	}

	if (!isEnemy(plot()->getTeam()))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(this, MISSIONAI_GROUP) > 0)
		{
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
	}
	else
	{
		if (AI_seaBombardRange(1))
		{
			return;
		}
	}
	
	if (AI_group(UNITAI_CARRIER_SEA, -1, /*iMaxOwnUnitAI*/ 1))
	{
		return;
	}

	if (getGroup()->countNumUnitAIType(UNITAI_ATTACK_SEA) + getGroup()->countNumUnitAIType(UNITAI_ESCORT_SEA) == 0)
	{
		if (plot()->isCity() && plot()->getOwnerINLINE() == getOwnerINLINE())
		{
			getGroup()->pushMission(MISSION_SKIP);
			return;
		}
		if (AI_retreatToCity())
		{
			return;
		}
	}
		
	if (getCargo() > 0)
	{
		if (AI_carrierSeaTransport())
		{
			return;
		}

		if (AI_blockade())
		{
			return;
		}

		if (AI_shadow(UNITAI_ASSAULT_SEA))
		{
			return;
		}
	}
	
	if (AI_travelToUpgradeCity())
	{
		return;
	}

	if (AI_retreatToCity(true))
	{
		return;
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missileCarrierSeaMove()
{
	bool bIsStealth = (getInvisibleType() != NO_INVISIBLE);

	if (AI_seaRetreatFromCityDanger())
	{
		return;
	}

	if (plot()->isCity() && plot()->getTeam() == getTeam())
	{
		if (AI_heal())
		{
			return;
		}
	}
	
	if (((plot()->getTeam() != getTeam()) && getGroup()->hasCargo()) || getGroup()->AI_isFull())
	{
		if (bIsStealth)
		{
			if (AI_carrierSeaTransport())
			{
				return;
			}
		}
		else
		{
			if (AI_shadow(UNITAI_ASSAULT_SEA, 1, 50))
			{
				return;
			}
			
			if (AI_carrierSeaTransport())
			{
				return;
			}
		}
	}
//	if (AI_pickup(UNITAI_ICBM))
//	{
//		return;
//	}
//	
//	if (AI_pickup(UNITAI_MISSILE_AIR))
//	{
//		return;
//	}
	if (AI_retreatToCity())
	{
		return;
	}
	
	getGroup()->pushMission(MISSION_SKIP);
}


void CvUnitAI::AI_attackAirMove()
{
	if (AI_airRetreatFromCityDanger())
	{
		return;
	}

	if (AI_airAttackDamagedSkip())
	{
		return;
	}

	if (getDamage() > 0)
	{
		if (AI_airBombPlots())
		{
			return;
		}
		if (AI_airStrike())
		{
			return;
		}
	}

	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvArea* pArea = area();
	int iAttackValue = kPlayer.AI_unitValue(getUnitType(), UNITAI_ATTACK_AIR, pArea);
	int iCarrierValue = kPlayer.AI_unitValue(getUnitType(), UNITAI_CARRIER_AIR, pArea);
	if (iCarrierValue > 0)
	{
		int iCarriers = kPlayer.AI_totalUnitAIs(UNITAI_CARRIER_SEA);
		if (iCarriers > 0)
		{
			UnitTypes eBestCarrierUnit = NO_UNIT;  
			kPlayer.AI_bestAreaUnitAIValue(UNITAI_CARRIER_SEA, nullptr, &eBestCarrierUnit);
			if (eBestCarrierUnit != NO_UNIT)
			{
				int iCarrierAirNeeded = iCarriers * GC.getUnitInfo(eBestCarrierUnit).getCargoSpace();
				if (kPlayer.AI_totalUnitAIs(UNITAI_CARRIER_AIR) < iCarrierAirNeeded)
				{
					AI_setUnitAIType(UNITAI_CARRIER_AIR);
					getGroup()->pushMission(MISSION_SKIP);
					return;		
				}
			}
		}
	}
	
	int iDefenseValue = kPlayer.AI_unitValue(getUnitType(), UNITAI_DEFENSE_AIR, pArea);
	if (iDefenseValue > iAttackValue)
	{
		if (kPlayer.AI_bestAreaUnitAIValue(UNITAI_ATTACK_AIR, pArea) > iAttackValue)
		{
			AI_setUnitAIType(UNITAI_DEFENSE_AIR);
			getGroup()->pushMission(MISSION_SKIP);
			return;	
		}
	}


	if (AI_airBombDefenses())
	{
		return;
	}

	if (GC.getGameINLINE().getSorenRandNum(4, "AI Air Attack Move") == 0)
	{
		if (AI_airBombPlots())
		{
			return;
		}
	}

	if (AI_airStrike())
	{
		return;
	}
	
	if (canAirAttack())
	{
		if (AI_airOffensiveCity())
		{
			return;
		}
	}
	
	if (canRecon(plot()))
	{
		if (AI_exploreAir())
		{
			return;
		}
	}

	if (canAirDefend())
	{
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_defenseAirMove()
{
	if (AI_airRetreatFromCityDanger())
	{
		return;
	}

	if (AI_airAttackDamagedSkip())
	{
		return;
	}
	
	if ((GC.getGameINLINE().getSorenRandNum(2, "AI Air Defense Move") == 0))
	{
		CvCity* pCity = plot()->getPlotCity();

		if ((pCity != nullptr) && pCity->AI_isDanger())
		{
			if (AI_airStrike())
			{
				return;
			}
		}
		else
		{
			if (AI_airBombDefenses())
			{
				return;
			}

			if (AI_airStrike())
			{
				return;
			}
			
			if (AI_getBirthmark() % 2 == 0)
			{
				if (AI_airBombPlots())
				{
					return;
				}
			}
		}

		if (AI_travelToUpgradeCity())
		{
			return;
		}
	}

	bool bNoWar = (GET_TEAM(getTeam()).getAtWarCount(false) == 0);
	
	if (canRecon(plot()))
	{
		if (GC.getGame().getSorenRandNum(bNoWar ? 2 : 4, "AI defensive air recon") == 0)
		{
			if (AI_exploreAir())
			{
				return;
			}
		}
	}
	
	if (AI_airDefensiveCity())
	{
		return;
	}

	if (canAirDefend())
	{
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_carrierAirMove()
{
	// XXX maybe protect land troops?

	if (getDamage() > 0)
	{
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (isCargo())
	{
		int iRand = GC.getGameINLINE().getSorenRandNum(3, "AI Air Carrier Move");
		
		if (iRand == 2 && canAirDefend())
		{
			getGroup()->pushMission(MISSION_AIRPATROL);
			return;
		}
		else if (AI_airBombDefenses())
		{
			return;
		}
		else if (iRand == 1)
		{
			if (AI_airBombPlots())
			{
				return;
			}
			
			if (AI_airStrike())
			{
				return;
			}
		}
		else
		{
			if (AI_airStrike())
			{
				return;
			}
			
			if (AI_airBombPlots())
			{
				return;
			}
		}

		if (AI_travelToUpgradeCity())
		{
			return;
		}

		if (canAirDefend())
		{
			getGroup()->pushMission(MISSION_AIRPATROL);
			return;
		}
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}

	if (AI_airCarrier())
	{
		return;
	}

	if (AI_airDefensiveCity())
	{
		return;
	}

	if (canAirDefend())
	{
		getGroup()->pushMission(MISSION_AIRPATROL);
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_missileAirMove()
{
	CvCity* pCity = plot()->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->AI_isDanger())
		{
			if (!(pCity->AI_isDefended()))
			{
				if (AI_airOffensiveCity())
				{
					return;
				}
			}
		}
	}
	
	if (isCargo())
	{
		int iRand = GC.getGameINLINE().getSorenRandNum(3, "AI Air Missile plot bombing");
		if (iRand != 0)
		{
			if (AI_airBombPlots())
			{
				return;
			}
		}

		iRand = GC.getGameINLINE().getSorenRandNum(3, "AI Air Missile Carrier Move");
		if (iRand == 0)
		{
			if (AI_airBombDefenses())
			{
				return;
			}
			
			if (AI_airStrike())
			{
				return;
			}
		}
		else
		{	
			if (AI_airStrike())
			{
				return;
			}
			
			if (AI_airBombDefenses())
			{
				return;
			}
		}
		
		if (AI_airBombPlots())
		{
			return;
		}
		
		getGroup()->pushMission(MISSION_SKIP);
		return;
	}
	
	if (AI_airStrike())
	{
		return;
	}
	
	if (AI_missileLoad(UNITAI_MISSILE_CARRIER_SEA))
	{
		return;
	}
	
	if (AI_missileLoad(UNITAI_RESERVE_SEA, 1))
	{
		return;
	}
	
	if (AI_missileLoad(UNITAI_ATTACK_SEA, 1))
	{
		return;
	}

	if (AI_airBombDefenses())
	{
		return;
	}

	if (!isCargo())
	{
		if (AI_airOffensiveCity())
		{
			return;
		}
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_networkAutomated()
{
	FAssertMsg(canBuildRoute(), "canBuildRoute is expected to be true");

	if (!(getGroup()->canDefend()))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot()) > 0)
		{
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	if (AI_improveBonus(20))
	{
		return;
	}

	if (AI_improveBonus(10))
	{
		return;
	}

	if (AI_connectBonus())
	{
		return;
	}

	if (AI_connectCity())
	{
		return;
	}

	if (AI_improveBonus())
	{
		return;
	}

	if (AI_routeTerritory(true))
	{
		return;
	}

	if (AI_connectBonus(false))
	{
		return;
	}

	if (AI_routeCity())
	{
		return;
	}

	if (AI_routeTerritory())
	{
		return;
	}
	
	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


void CvUnitAI::AI_cityAutomated()
{
	CvCity* pCity;

	if (!(getGroup()->canDefend()))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot()) > 0)
		{
			if (AI_retreatToCity()) // XXX maybe not do this??? could be working productively somewhere else...
			{
				return;
			}
		}
	}

	pCity = nullptr;

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		pCity = plot()->getWorkingCity();
	}

	if (pCity == nullptr)
	{
		pCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), getOwnerINLINE()); // XXX do team???
	}

	if (pCity != nullptr)
	{
		if (AI_improveCity(pCity))
		{
			return;
		}
	}

	if (AI_retreatToCity())
	{
		return;
	}

	if (AI_safety())
	{
		return;
	}

	getGroup()->pushMission(MISSION_SKIP);
	return;
}


// XXX make sure we include any new UnitAITypes...
int CvUnitAI::AI_promotionValue(PromotionTypes ePromotion)
{
	int iValue;
	int iTemp;
	int iExtra;
	int iI;

	iValue = 0;

	if (GC.getPromotionInfo(ePromotion).isLeader())
	{
		// Don't consume the leader as a regular promotion
		return 0;
	}

	if (GC.getPromotionInfo(ePromotion).isBlitz())
	{
		if ((AI_getUnitAIType() == UNITAI_RESERVE  && baseMoves() > 1) || 
			AI_getUnitAIType() == UNITAI_PARADROP)
		{
			iValue += 10;
		}
		else
		{
			iValue += 2;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isAmphib())
	{
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			  (AI_getUnitAIType() == UNITAI_ATTACK_CITY))
		{
			iValue += 5;
		}
		else
		{
			iValue++;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isRiver())
	{
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			  (AI_getUnitAIType() == UNITAI_ATTACK_CITY))
		{
			iValue += 5;
		}
		else
		{
			iValue++;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isEnemyRoute())
	{
		if (AI_getUnitAIType() == UNITAI_PILLAGE)
		{
			iValue += 40;
		}
		else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			       (AI_getUnitAIType() == UNITAI_ATTACK_CITY))
		{
			iValue += 20;
		}
		else if (AI_getUnitAIType() == UNITAI_PARADROP)
		{
			iValue += 10;
		}
		else
		{
			iValue += 4;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isAlwaysHeal())
	{
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			  (AI_getUnitAIType() == UNITAI_ATTACK_CITY) ||
				(AI_getUnitAIType() == UNITAI_PILLAGE) ||
				(AI_getUnitAIType() == UNITAI_COUNTER) ||
				(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
				(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
				(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
				(AI_getUnitAIType() == UNITAI_PARADROP))
		{
			iValue += 10;
		}
		else
		{
			iValue += 8;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isHillsDoubleMove())
	{
		if (AI_getUnitAIType() == UNITAI_EXPLORE)
		{
			iValue += 20;
		}
		else
		{
			iValue += 10;
		}
	}

	if (GC.getPromotionInfo(ePromotion).isImmuneToFirstStrikes() 
		&& !immuneToFirstStrikes())
	{
		if ((AI_getUnitAIType() == UNITAI_ATTACK_CITY))
		{
			iValue += 12;			
		}
		else if ((AI_getUnitAIType() == UNITAI_ATTACK))
		{
			iValue += 8;
		}
		else
		{
			iValue += 4;
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getVisibilityChange();
	if ((AI_getUnitAIType() == UNITAI_EXPLORE_SEA) || 
		(AI_getUnitAIType() == UNITAI_EXPLORE))
	{
		iValue += (iTemp * 40);
	}
	else if (AI_getUnitAIType() == UNITAI_PIRATE_SEA) 
	{
		iValue += (iTemp * 20);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getMovesChange();
	if ((AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		  (AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		  (AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
			(AI_getUnitAIType() == UNITAI_EXPLORE_SEA) ||
			(AI_getUnitAIType() == UNITAI_ASSAULT_SEA) ||
			(AI_getUnitAIType() == UNITAI_SETTLER_SEA) ||
			(AI_getUnitAIType() == UNITAI_PILLAGE) ||
			(AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_PARADROP))
	{
		iValue += (iTemp * 20);
	}
	else
	{
		iValue += (iTemp * 4);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getMoveDiscountChange();
	if (AI_getUnitAIType() == UNITAI_PILLAGE)
	{
		iValue += (iTemp * 10);
	}
	else
	{
		iValue += (iTemp * 2);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getAirRangeChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_AIR ||
		AI_getUnitAIType() == UNITAI_CARRIER_AIR)
	{
		iValue += (iTemp * 20);
	}
	else if (AI_getUnitAIType() == UNITAI_DEFENSE_AIR)
	{
		iValue += (iTemp * 10);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getInterceptChange();
	if (AI_getUnitAIType() == UNITAI_DEFENSE_AIR)
	{
		iValue += (iTemp * 3);
	}
	else if (AI_getUnitAIType() == UNITAI_CITY_SPECIAL || AI_getUnitAIType() == UNITAI_CARRIER_AIR)
	{
		iValue += (iTemp * 2);
	}
	else
	{
		iValue += (iTemp / 10);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getEvasionChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_AIR || AI_getUnitAIType() == UNITAI_CARRIER_AIR)
	{
		iValue += (iTemp * 3);
	}
	else
	{
		iValue += (iTemp / 10);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getFirstStrikesChange() * 2;
	iTemp += GC.getPromotionInfo(ePromotion).getChanceFirstStrikesChange();
	if ((AI_getUnitAIType() == UNITAI_RESERVE) ||
		  (AI_getUnitAIType() == UNITAI_COUNTER) ||
			(AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
			(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
			(AI_getUnitAIType() == UNITAI_CITY_SPECIAL) ||
			(AI_getUnitAIType() == UNITAI_ATTACK))
	{
		iTemp *= 8;
		iExtra = getExtraChanceFirstStrikes() + getExtraFirstStrikes() * 2;
		iTemp *= 100 + iExtra * 15;
		iTemp /= 100;
		iValue += iTemp;
	}
	else
	{
		iValue += (iTemp * 5);
	}


	iTemp = GC.getPromotionInfo(ePromotion).getWithdrawalChange();
	if (iTemp != 0)
	{
		iExtra = (m_pUnitInfo->getWithdrawalProbability() + (getExtraWithdrawal() * 4));
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if ((AI_getUnitAIType() == UNITAI_ATTACK_CITY))
		{
			iValue += (iTemp * 4) / 3;
		}
		else if ((AI_getUnitAIType() == UNITAI_COLLATERAL) ||
			  (AI_getUnitAIType() == UNITAI_RESERVE) ||
			  (AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
			  getLeaderUnitType() != NO_UNIT)
		{
			iValue += iTemp * 1;
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getCollateralDamageChange();
	if (iTemp != 0)
	{
		iExtra = (getExtraCollateralDamage());//collateral has no strong synergy (not like retreat)
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		
		if (AI_getUnitAIType() == UNITAI_COLLATERAL)
		{
			iValue += (iTemp * 1);
		}
		else if (AI_getUnitAIType() == UNITAI_ATTACK_CITY)
		{
			iValue += ((iTemp * 2) / 3);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getBombardRateChange();
	if (AI_getUnitAIType() == UNITAI_ATTACK_CITY)
	{
		iValue += (iTemp * 2);
	}
	else
	{
		iValue += (iTemp / 8);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getEnemyHealChange();	
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PARADROP) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA))
	{
		iValue += (iTemp / 4);
	}
	else
	{
		iValue += (iTemp / 8);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getNeutralHealChange();
	iValue += (iTemp / 8);

	iTemp = GC.getPromotionInfo(ePromotion).getFriendlyHealChange();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		  (AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		  (AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
	{
		iValue += (iTemp / 4);
	}
	else
	{
		iValue += (iTemp / 8);
	}


    if (getDamage() > 0)
    {
        iTemp = GC.getPromotionInfo(ePromotion).getSameTileHealChange() + getSameTileHeal();
        iExtra = getSameTileHeal();
        
        iTemp *= (100 + iExtra * 5);
        iTemp /= 100;
        
        if (iTemp > 0)
        {
            if (healRate(plot()) < iTemp)
            {
                iValue += iTemp * ((getGroup()->getNumUnits() > 4) ? 4 : 2);
            }
            else
            {
                iValue += (iTemp / 8);
            }
        }

        iTemp = GC.getPromotionInfo(ePromotion).getAdjacentTileHealChange();
        iExtra = getAdjacentTileHeal();
        iTemp *= (100 + iExtra * 5);
        iTemp /= 100;
        if (getSameTileHeal() >= iTemp)
        {
            iValue += (iTemp * ((getGroup()->getNumUnits() > 9) ? 4 : 2));
        }
        else
        {
            iValue += (iTemp / 4);
        }
    }

	// try to use Warlords to create super-medic units
	if (GC.getPromotionInfo(ePromotion).getAdjacentTileHealChange() > 0 || GC.getPromotionInfo(ePromotion).getSameTileHealChange() > 0)
	{
		PromotionTypes eLeader = NO_PROMOTION;
		for (iI = 0; iI < GC.getNumPromotionInfos(); iI++)
		{
			if (GC.getPromotionInfo((PromotionTypes)iI).isLeader())
			{
				eLeader = (PromotionTypes)iI;
			}
		}
		
		if (isHasPromotion(eLeader) && eLeader != NO_PROMOTION)
		{
			iValue += GC.getPromotionInfo(ePromotion).getAdjacentTileHealChange() + GC.getPromotionInfo(ePromotion).getSameTileHealChange();
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getCombatPercent();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		  (AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		  (AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
			(AI_getUnitAIType() == UNITAI_PARADROP) ||
			(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
			(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
			(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
			(AI_getUnitAIType() == UNITAI_CARRIER_SEA) ||
			(AI_getUnitAIType() == UNITAI_ATTACK_AIR) ||
			(AI_getUnitAIType() == UNITAI_CARRIER_AIR))
	{
		iValue += (iTemp * 2);
	}
	else
	{
		iValue += (iTemp * 1);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getCityAttackPercent();
	if (iTemp != 0)
	{
		if (m_pUnitInfo->getUnitAIType(UNITAI_ATTACK) || m_pUnitInfo->getUnitAIType(UNITAI_ATTACK_CITY) || m_pUnitInfo->getUnitAIType(UNITAI_ATTACK_CITY_LEMMING))
		{
			iExtra = (m_pUnitInfo->getCityAttackModifier() + (getExtraCityAttackPercent() * 2));
			iTemp *= (100 + iExtra);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_ATTACK_CITY)
			{
				iValue += (iTemp * 1);
			}
			else 
			{
				iValue -= iTemp / 4;
			}
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getCityDefensePercent();
	if (iTemp != 0)
	{
		if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
			  (AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
		{
			iExtra = m_pUnitInfo->getCityDefenseModifier() + (getExtraCityDefensePercent() * 2);
			iValue += ((iTemp * (100 + iExtra)) / 100);
		}
		else
		{
			iValue += (iTemp / 4);
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getHillsAttackPercent();
	if (iTemp != 0)
	{
		iExtra = getExtraHillsAttackPercent();
		iTemp *= (100 + iExtra * 2);
		iTemp /= 100;
		if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			(AI_getUnitAIType() == UNITAI_COUNTER))
		{
			iValue += (iTemp / 4);
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getHillsDefensePercent();
	if (iTemp != 0)
	{
		iExtra = (m_pUnitInfo->getHillsDefenseModifier() + (getExtraHillsDefensePercent() * 2)); 
		iTemp *= (100 + iExtra);
		iTemp /= 100;
		if (AI_getUnitAIType() == UNITAI_CITY_DEFENSE)
		{
			if (plot()->isCity() && plot()->isHills())
			{
				iValue += (iTemp * 4) / 3;
			}
		}
		else if (AI_getUnitAIType() == UNITAI_COUNTER)
		{
			if (plot()->isHills())
			{
				iValue += (iTemp / 4);
			}
			else
			{
				iValue++;
			}
		}
		else
		{
			iValue += (iTemp / 16);
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getRevoltProtection();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
	{
		if (iTemp > 0)
		{
			PlayerTypes eOwner = plot()->calculateCulturalOwner();
			if (eOwner != NO_PLAYER && GET_PLAYER(eOwner).getTeam() != GET_PLAYER(getOwnerINLINE()).getTeam())
			{
				iValue += (iTemp / 2);
			}
		}
	}

	iTemp = GC.getPromotionInfo(ePromotion).getCollateralDamageProtection();
	if ((AI_getUnitAIType() == UNITAI_CITY_DEFENSE) ||
		(AI_getUnitAIType() == UNITAI_CITY_COUNTER) ||
		(AI_getUnitAIType() == UNITAI_CITY_SPECIAL))
	{
		iValue += (iTemp / 3);
	}
	else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_COUNTER))
	{
		iValue += (iTemp / 4);
	}
	else
	{
		iValue += (iTemp / 8);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getPillageChange();
	if (AI_getUnitAIType() == UNITAI_PILLAGE ||
		AI_getUnitAIType() == UNITAI_ATTACK_SEA ||
		AI_getUnitAIType() == UNITAI_PIRATE_SEA)
	{
		iValue += (iTemp / 4);
	}
	else
	{
		iValue += (iTemp / 16);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getUpgradeDiscount();
	iValue += (iTemp / 16);

	iTemp = GC.getPromotionInfo(ePromotion).getExperiencePercent();
	if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
		(AI_getUnitAIType() == UNITAI_ATTACK_SEA) ||
		(AI_getUnitAIType() == UNITAI_PIRATE_SEA) ||
		(AI_getUnitAIType() == UNITAI_RESERVE_SEA) ||
		(AI_getUnitAIType() == UNITAI_ESCORT_SEA) ||
		(AI_getUnitAIType() == UNITAI_CARRIER_SEA) ||
		(AI_getUnitAIType() == UNITAI_MISSILE_CARRIER_SEA))
	{
		iValue += (iTemp * 1);
	}
	else
	{
		iValue += (iTemp / 2);
	}

	iTemp = GC.getPromotionInfo(ePromotion).getKamikazePercent();
	if (AI_getUnitAIType() == UNITAI_ATTACK_CITY)
	{
		iValue += (iTemp / 16);
	}
	else
	{
		iValue += (iTemp / 64);
	}

	for (iI = 0; iI < GC.getNumTerrainInfos(); iI++)
	{
		iTemp = GC.getPromotionInfo(ePromotion).getTerrainAttackPercent(iI);
		if (iTemp != 0)
		{
			iExtra = getExtraTerrainAttackPercent((TerrainTypes)iI);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
				(AI_getUnitAIType() == UNITAI_COUNTER))
			{
				iValue += (iTemp / 4);
			}
			else
			{
				iValue += (iTemp / 16);
			}
		}

		iTemp = GC.getPromotionInfo(ePromotion).getTerrainDefensePercent(iI);
		if (iTemp != 0)
		{
			iExtra =  getExtraTerrainDefensePercent((TerrainTypes)iI);
			iTemp *= (100 + iExtra);
			iTemp /= 100;
			if (AI_getUnitAIType() == UNITAI_COUNTER)
			{
				if (plot()->getTerrainType() == (TerrainTypes)iI)
				{
					iValue += (iTemp / 4);
				}
				else
				{
					iValue++;
				}
			}
			else
			{
				iValue += (iTemp / 16);
			}
		}

		if (GC.getPromotionInfo(ePromotion).getTerrainDoubleMove(iI))
		{
			if (AI_getUnitAIType() == UNITAI_EXPLORE)
			{
				iValue += 20;
			}
			else if ((AI_getUnitAIType() == UNITAI_ATTACK) || (AI_getUnitAIType() == UNITAI_PILLAGE))
			{
				iValue += 10;
			}
			else
			{
			    iValue += 1;
			}
		}
	}

	for (iI = 0; iI < GC.getNumFeatureInfos(); iI++)
	{
		iTemp = GC.getPromotionInfo(ePromotion).getFeatureAttackPercent(iI);
		if (iTemp != 0)
		{
			iExtra = getExtraFeatureAttackPercent((FeatureTypes)iI);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
				(AI_getUnitAIType() == UNITAI_COUNTER))
			{
				iValue += (iTemp / 4);
			}
			else
			{
				iValue += (iTemp / 16);
			}
		}

		iTemp = GC.getPromotionInfo(ePromotion).getFeatureDefensePercent(iI);;
		if (iTemp != 0)
		{
			iExtra = getExtraFeatureDefensePercent((FeatureTypes)iI);
			iTemp *= (100 + iExtra * 2);
			iTemp /= 100;
			
			if (!noDefensiveBonus())
			{
				if (AI_getUnitAIType() == UNITAI_COUNTER)
				{
					if (plot()->getFeatureType() == (FeatureTypes)iI)
					{
						iValue += (iTemp / 4);
					}
					else
					{
						iValue++;
					}
				}
				else
				{
					iValue += (iTemp / 16);
				}
			}
		}

		if (GC.getPromotionInfo(ePromotion).getFeatureDoubleMove(iI))
		{
			if (AI_getUnitAIType() == UNITAI_EXPLORE)
			{
				iValue += 20;
			}
			else if ((AI_getUnitAIType() == UNITAI_ATTACK) || (AI_getUnitAIType() == UNITAI_PILLAGE))
			{
				iValue += 10;
			}
			else
			{
			    iValue += 1;
			}
		}
	}

    int iOtherCombat = 0; 
    int iSameCombat = 0;
    
    for (iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
    {
        if ((UnitCombatTypes)iI == getUnitCombatType())
        {
            iSameCombat += unitCombatModifier((UnitCombatTypes)iI);
        }
        else
        {
            iOtherCombat += unitCombatModifier((UnitCombatTypes)iI);
        }
    }

	for (iI = 0; iI < GC.getNumUnitCombatInfos(); iI++)
	{
		iTemp = GC.getPromotionInfo(ePromotion).getUnitCombatModifierPercent(iI);
		int iCombatWeight = 0;
        //Fighting their own kind
        if ((UnitCombatTypes)iI == getUnitCombatType())
        {
            if (iSameCombat >= iOtherCombat)
            {
                iCombatWeight = 70;//"axeman takes formation"
            }
            else
            {
                iCombatWeight = 30;
            }
        }
        else
        {
            //fighting other kinds
            if (unitCombatModifier((UnitCombatTypes)iI) > 10)
            {
                iCombatWeight = 70;//"spearman takes formation"
            }
            else
            {
                iCombatWeight = 30;
            }
        }

		iCombatWeight *= GET_PLAYER(getOwnerINLINE()).AI_getUnitCombatWeight((UnitCombatTypes)iI);
		iCombatWeight /= 100;		
		
		if ((AI_getUnitAIType() == UNITAI_COUNTER) || (AI_getUnitAIType() == UNITAI_CITY_COUNTER))
		{
		    iValue += (iTemp * iCombatWeight) / 50;
		}
		else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			       (AI_getUnitAIType() == UNITAI_RESERVE))
		{
			iValue += (iTemp * iCombatWeight) / 100;
		}
		else
		{
			iValue += (iTemp * iCombatWeight) / 200;
		}
	}

	for (iI = 0; iI < NUM_DOMAIN_TYPES; iI++)
	{
		//WTF? why float and cast to int?
		//iTemp = ((int)((GC.getPromotionInfo(ePromotion).getDomainModifierPercent(iI) + getExtraDomainModifier((DomainTypes)iI)) * 100.0f));
		iTemp = GC.getPromotionInfo(ePromotion).getDomainModifierPercent(iI);
		if (AI_getUnitAIType() == UNITAI_COUNTER)
		{
			iValue += (iTemp * 1);
		}
		else if ((AI_getUnitAIType() == UNITAI_ATTACK) ||
			       (AI_getUnitAIType() == UNITAI_RESERVE))
		{
			iValue += (iTemp / 2);
		}
		else
		{
			iValue += (iTemp / 8);
		}
	}

	if (iValue > 0)
	{
		iValue += GC.getGameINLINE().getSorenRandNum(15, "AI Promote");
	}

	return iValue;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_shadow(UnitAITypes eUnitAI, int iMax, int iMaxRatio, bool bWithCargoOnly)
{
	PROFILE_FUNC();

	CvUnit* pLoopUnit;
	CvUnit* pBestUnit;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestUnit = nullptr;

	for(pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop))
	{
		if (pLoopUnit != this)
		{
			if (AI_plotValid(pLoopUnit->plot()))
			{
				if (pLoopUnit->isGroupHead())
				{
					if (!(pLoopUnit->isCargo()))
					{
						if (pLoopUnit->AI_getUnitAIType() == eUnitAI)
						{
							if (pLoopUnit->getGroup()->baseMoves() <= getGroup()->baseMoves())
							{
								if (!bWithCargoOnly || pLoopUnit->getGroup()->hasCargo())
								{
									int iShadowerCount = GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, MISSIONAI_SHADOW, getGroup());
									if (((-1 == iMax) || (iShadowerCount < iMax)) &&
										 ((-1 == iMaxRatio) || (iShadowerCount == 0) || (((100 * iShadowerCount) / std::max(1, pLoopUnit->getGroup()->countNumUnitAIType(eUnitAI))) <= iMaxRatio)))
									{
										if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this)))
										{
											if (generatePath(pLoopUnit->plot(), 0, true, &iPathTurns))
											{
												//if (iPathTurns <= iMaxPath) XXX
												{
													iValue = 1 + pLoopUnit->getGroup()->getCargo();
													iValue *= 1000;
													iValue /= 1 + iPathTurns;

													if (iValue > iBestValue)
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

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_SHADOW, nullptr, pBestUnit);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), 0, false, false, MISSIONAI_SHADOW, nullptr, pBestUnit);
			return true;
		}
	}

	return false;
}


// Returns true if a group was joined or a mission was pushed...
bool CvUnitAI::AI_group(UnitAITypes eUnitAI, int iMaxGroup, int iMaxOwnUnitAI, int iMinUnitAI, bool bIgnoreFaster, bool bIgnoreOwnUnitType, bool bStackOfDoom, int iMaxPath, bool bAllowRegrouping)
{
	PROFILE_FUNC();

	CvUnit* pLoopUnit;
	CvUnit* pBestUnit;
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
		if (getGroup()->getNumUnits() > 1)
		{
			return false;
		}
	}
	
	if ((getDomainType() == DOMAIN_LAND) && !canMoveAllTerrain())
	{
		if (area()->getNumAIUnits(getOwnerINLINE(), eUnitAI) == 0)
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

	for(pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop))
	{
		CvSelectionGroup* pLoopGroup = pLoopUnit->getGroup();
		CvPlot* pPlot = pLoopUnit->plot();
		if (AI_plotValid(pPlot))
		{
			if (iMaxPath > 0 || pPlot == plot())
			{
				if (!isEnemy(pPlot->getTeam()))
				{
					if (AI_allowGroup(pLoopUnit, eUnitAI))
					{
						if ((iMaxGroup == -1) || ((pLoopGroup->getNumUnits() + GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, MISSIONAI_GROUP, getGroup())) <= (iMaxGroup + ((bStackOfDoom) ? AI_stackOfDoomExtra() : 0))))
						{
							if ((iMaxOwnUnitAI == -1) || (pLoopGroup->countNumUnitAIType(AI_getUnitAIType()) <= (iMaxOwnUnitAI + ((bStackOfDoom) ? AI_stackOfDoomExtra() : 0))))
							{
								if ((iMinUnitAI == -1) || (pLoopGroup->countNumUnitAIType(eUnitAI) >= iMinUnitAI))
								{
									if (!bIgnoreFaster || (pLoopUnit->getGroup()->baseMoves() <= baseMoves()))
									{
										if (!bIgnoreOwnUnitType || (pLoopUnit->getUnitType() != getUnitType()))
										{
											if (!(pPlot->isVisibleEnemyUnit(this)))
											{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
												if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_group]
													? generatePath(pPlot, PathingArg(0, iMaxPath), true, &iPathTurns)
													: generatePath(pPlot, 0, true, &iPathTurns))
#else
												if (generatePath(pPlot, 0, true, &iPathTurns))
#endif
												{
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

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
			if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
				getGroup()->resetPath();
#endif
			joinGroup(pBestUnit->getGroup());
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), 0, false, false, MISSIONAI_GROUP, nullptr, pBestUnit);
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_groupMergeRange(UnitAITypes eUnitAI, int iMaxRange, bool bBiggerOnly, bool bAllowRegrouping, bool bIgnoreFaster)
{
	PROFILE_FUNC();


 	// if we are on a transport, then do not regroup
	if (isCargo())
	{
		return false;
	}

   if (!bAllowRegrouping)
	{
		if (getGroup()->getNumUnits() > 1)
		{
			return false;
		}
	}
	
	if ((getDomainType() == DOMAIN_LAND) && !canMoveAllTerrain())
	{
		if (area()->getNumAIUnits(getOwnerINLINE(), eUnitAI) == 0)
		{
			return false;
		}
	}
	
	if (!AI_canGroupWithAIType(eUnitAI))
	{
		return false;
	}
	
	// cached values
	CvPlot* pPlot = plot();
	CvSelectionGroup* pGroup = getGroup();
	
	// best match
	CvUnit* pBestUnit = nullptr;
	int iBestValue = INT_MAX;
	// iterate over plots at each range
	for (int iDX = -(iMaxRange); iDX <= iMaxRange; iDX++)
	{
		for (int iDY = -(iMaxRange); iDY <= iMaxRange; iDY++)
		{
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);
			
			if (pLoopPlot != nullptr && pLoopPlot->getArea() == pPlot->getArea())
			{
				CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
				while (pUnitNode != nullptr)
				{
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);
					
					CvSelectionGroup* pLoopGroup = pLoopUnit->getGroup();

					if (AI_allowGroup(pLoopUnit, eUnitAI))
					{
						if (!bIgnoreFaster || (pLoopUnit->getGroup()->baseMoves() <= baseMoves()))
						{
							if (!bBiggerOnly || (pLoopGroup->getNumUnits() >= pGroup->getNumUnits()))
							{
								int iPathTurns;
								if (generatePath(pLoopPlot, 0, true, &iPathTurns))
								{
									if (iPathTurns <= (iMaxRange + 2))
									{
										int iValue = 1000 * (iPathTurns + 1);
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

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			pGroup->mergeIntoGroup(pBestUnit->getGroup()); 
			return true;
		}
		else
		{
			pGroup->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), 0, false, false, MISSIONAI_GROUP, nullptr, pBestUnit);
			return true;
		}
	}

	return false;
}

// Returns true if we loaded onto a transport or a mission was pushed...
bool CvUnitAI::AI_load(UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI, int iMinCargo, int iMinCargoSpace, int iMaxCargoSpace, int iMaxCargoOurUnitAI, int iFlags, int iMaxPath)
{
	PROFILE_FUNC();

	CvUnit* pLoopUnit;
	CvUnit* pBestUnit;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	// XXX what to do about groups???
	/*if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}*/

	if (getCargo() > 0)
	{
		return false;
	}

	if (isCargo())
	{
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}
	
	if ((getDomainType() == DOMAIN_LAND) && !canMoveAllTerrain())
	{
		if (area()->getNumAIUnits(getOwnerINLINE(), eUnitAI) == 0)
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
	
	const int iLoadMissionAICount = 4;
	MissionAITypes aeLoadMissionAI[iLoadMissionAICount] = {MISSIONAI_LOAD_ASSAULT, MISSIONAI_LOAD_SETTLER, MISSIONAI_LOAD_SPECIAL, MISSIONAI_ATTACK_SPY};

	int iCurrentGroupSize = getGroup()->getNumUnits();

	for(pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop))
	{
		if (pLoopUnit != this)
		{
			if (AI_plotValid(pLoopUnit->plot()))
			{
				if (canLoadUnit(pLoopUnit, pLoopUnit->plot()))
				{
					// special case ASSAULT_SEA UnitAI, so that, if a unit is marked escort, but can load units, it will load them
					// transport units might have been built as escort, this most commonly happens with galleons
					UnitAITypes eLoopUnitAI = pLoopUnit->AI_getUnitAIType();
					if (eLoopUnitAI == eUnitAI)// || (eUnitAI == UNITAI_ASSAULT_SEA && eLoopUnitAI == UNITAI_ESCORT_SEA))
					{
						int iCargoSpaceAvailable = pLoopUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());
						iCargoSpaceAvailable -= GET_PLAYER(getOwnerINLINE()).AI_unitTargetMissionAIs(pLoopUnit, aeLoadMissionAI, iLoadMissionAICount, getGroup());
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
												if (getGroup()->getHeadUnitAI() != UNITAI_CITY_DEFENSE || !plot()->isCity() || (plot()->getTeam() != getTeam()))
												{
													if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this)))
													{
														CvPlot* pUnitTargetPlot = pLoopUnit->getGroup()->AI_getMissionAIPlot();
														if ((pUnitTargetPlot == nullptr) || (pUnitTargetPlot->getTeam() == getTeam()) || (!pUnitTargetPlot->isOwned() || !isPotentialEnemy(pUnitTargetPlot->getTeam(), pUnitTargetPlot)))
														{
															if (generatePath(pLoopUnit->plot(), iFlags, true, &iPathTurns))
															{
																if (iPathTurns <= iMaxPath)
																{
																	// prefer a transport that can hold as much of our group as possible 
																	iValue = (std::max(0, iCurrentGroupSize - iCargoSpaceAvailable) * 5) + iPathTurns;

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
			}
		}
	}

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			getGroup()->setTransportUnit(pBestUnit); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		}
		else
		{
			int iCargoSpaceAvailable = pBestUnit->cargoSpaceAvailable(getSpecialUnitType(), getDomainType());
			FAssertMsg(iCargoSpaceAvailable > 0, "best unit has no space");

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
			if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
				getGroup()->resetPath();
#endif

			// split our group to fit on the transport
			CvSelectionGroup* pOtherGroup = nullptr;
			CvSelectionGroup* pSplitGroup = getGroup()->splitGroup(iCargoSpaceAvailable, this, &pOtherGroup);			
			FAssertMsg(pSplitGroup != nullptr, "splitGroup failed");
			FAssertMsg(m_iGroupID == pSplitGroup->getID(), "splitGroup failed to put unit in the new group");

			if (pSplitGroup != nullptr)
			{
				CvPlot* pOldPlot = pSplitGroup->plot();
				pSplitGroup->pushMission(MISSION_MOVE_TO_UNIT, pBestUnit->getOwnerINLINE(), pBestUnit->getID(), iFlags, false, false, eMissionAI, nullptr, pBestUnit);
				bool bMoved = (pSplitGroup->plot() != pOldPlot);
				if (!bMoved && pOtherGroup != nullptr)
				{
					joinGroup(pOtherGroup);
				}
				return bMoved;
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCityBestDefender()
{
	CvCity* pCity;
	CvPlot* pPlot;

	pPlot = plot();
	pCity = pPlot->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwnerINLINE() == getOwnerINLINE())
		{
			if (pPlot->getBestDefender(getOwnerINLINE()) == this)
			{
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
		}
	}

	return false;
}

bool CvUnitAI::AI_guardCityMinDefender(bool bSearch)
{
	PROFILE_FUNC();
	
	CvCity* pPlotCity = plot()->getPlotCity();
	if ((pPlotCity != nullptr) && (pPlotCity->getOwnerINLINE() == getOwnerINLINE()))
	{
		int iCityDefenderCount = pPlotCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwnerINLINE());
		if ((iCityDefenderCount - 1) < pPlotCity->AI_minDefenders())
		{
			if ((iCityDefenderCount <= 2) || (GC.getGame().getSorenRandNum(5, "AI shuffle defender") != 0))
			{
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
		}
	}
	
	if (bSearch)
	{
		int iBestValue = 0;
		CvPlot* pBestPlot = nullptr;
		CvPlot* pBestGuardPlot = nullptr;
		
		CvCity* pLoopCity;
		int iLoop;

		int iCurrentTurn = GC.getGame().getGameTurn();
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				int iDefendersHave = pLoopCity->plot()->plotCount(PUF_isUnitAIType, UNITAI_CITY_DEFENSE, -1, getOwnerINLINE());
				int iDefendersNeed = pLoopCity->AI_minDefenders();
				if (iDefendersHave < iDefendersNeed)
				{
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
					{
						iDefendersHave += GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_CITY, getGroup());
						if (iDefendersHave < iDefendersNeed + 1)
						{
							int iPathTurns{};
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
							if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_guardCityMinDefender]
								? !atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), PathingArg(0, 10 + 1), true, &iPathTurns)
								: !atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
#else
							if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
#endif
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
										pBestPlot = getPathEndTurnPlot();
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
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				return true;
			}
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
	}
	
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCity(bool bLeave, bool bSearch, int iMaxPath)
{
	PROFILE_FUNC();

	CLLNode<IDInfo>* pUnitNode;
	CvCity* pCity;
	CvCity* pLoopCity;
	CvUnit* pLoopUnit;
	CvPlot* pPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestGuardPlot;
	bool bDefend;
	int iExtra;
	int iCount;
	int iPathTurns{};
	int iValue;
	int iBestValue;
	int iLoop;

	FAssert(getDomainType() == DOMAIN_LAND);
	FAssert(canDefend());

	pPlot = plot();
	pCity = pPlot->getPlotCity();

	if ((pCity != nullptr) && (pCity->getOwnerINLINE() == getOwnerINLINE())) // XXX check for other team?
	{
		if (bLeave && !(pCity->AI_isDanger()))
		{
			iExtra = 1;
		}
		else
		{
			iExtra = (bSearch ? 0 : -GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pPlot, 2));
		}

		bDefend = false;

		if (pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE()) == 1) // XXX check for other team's units?
		{
			bDefend = true;
		}
		else if (!(pCity->AI_isDefended(((AI_isCityAIType()) ? -1 : 0) + iExtra))) // XXX check for other team's units?
		{
			if (AI_isCityAIType())
			{
				bDefend = true;
			}
			else
			{
				iCount = 0;

				pUnitNode = pPlot->headUnitNode();

				while (pUnitNode != nullptr)
				{
					pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getOwnerINLINE() == getOwnerINLINE())
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
											if (pLoopUnit->isWaiting())
											{
												FAssert(pLoopUnit != this);
												iCount++;
											}
										}
									}
									else
									{
										if (pLoopUnit->getGroup()->getMissionType(0) != MISSION_SKIP)
										{
											iCount++;											
										}
									}
								}
							}
						}
					}
				}

				if (!(pCity->AI_isDefended(iCount + iExtra))) // XXX check for other team's units?
				{
					bDefend = true;
				}
			}
		}

		if (bDefend)
		{			
			CvSelectionGroup* pOldGroup = getGroup();
			CvUnit* pEjectedUnit = getGroup()->AI_ejectBestDefender(pPlot);
			
			if (pEjectedUnit != nullptr)
			{
				if (pPlot->plotCount(PUF_isCityAIType, -1, -1, getOwnerINLINE()) == 0)
				{
					if (pEjectedUnit->cityDefenseModifier() > 0)
					{
						pEjectedUnit->AI_setUnitAIType(UNITAI_CITY_DEFENSE);
					}
				}
				pEjectedUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
				if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
				{
					// We have to reset pathing here too (as well as in AI_ejectBestDefender), because pushMission may have just tainted the pathfinder with pEjectedUnit.
					pEjectedUnit->getGroup()->resetPath();
				}
#endif
				if (pEjectedUnit->getGroup() == pOldGroup || pEjectedUnit == this)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				//This unit is not suited for defense, skip the mission
				//to protect this city but encourage others to defend instead.
				getGroup()->pushMission(MISSION_SKIP);
				if (!isHurt())
				{
					finishMoves();
				}
			}
			return true;
		}
	}

	if (bSearch)
	{
		iBestValue = INT_MAX;
		pBestPlot = nullptr;
		pBestGuardPlot = nullptr;

		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				if (!(pLoopCity->AI_isDefended((!AI_isCityAIType()) ? pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isNotCityAIType) : 0)))	// XXX check for other team's units?
				{
					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
					{
						if ((GC.getGame().getGameTurn() - pLoopCity->getGameTurnAcquired() < 10) || GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_CITY, getGroup()) < 2)
						{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
							if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_guardCity]
								? !atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), PathingArg(0, std::min(iMaxPath, iBestValue)), true, &iPathTurns)
								: !atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
#else
							if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
#endif
							{
								if (iPathTurns <= iMaxPath)
								{
									iValue = iPathTurns;

									if (iValue < iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
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
			// split up group if we are going to defend, so rest of group has opportunity to do something else
//			if (getGroup()->getNumUnits() > 1)
//			{
//				getGroup()->AI_separate();	// will change group
//			}
//
//			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
//			return true;

			CvSelectionGroup* pOldGroup = getGroup();
			CvUnit* pEjectedUnit = getGroup()->AI_ejectBestDefender(pPlot);
			
			if (pEjectedUnit != nullptr)
			{
				pEjectedUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
				if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes])
				{
					// We have to reset pathing here too (as well as in AI_ejectBestDefender), because pushMission may have just tainted the pathfinder with pEjectedUnit.
					pEjectedUnit->getGroup()->resetPath();
				}
#endif
				if (pEjectedUnit->getGroup() == pOldGroup || pEjectedUnit == this)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				//This unit is not suited for defense, skip the mission
				//to protect this city but encourage others to defend instead.
				if (atPlot(pBestGuardPlot))
				{
					getGroup()->pushMission(MISSION_SKIP);
				}
				else
				{
					getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, nullptr);
				}
				return true;				
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCityAirlift()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	if (getGroup()->getNumUnits() > 1)
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

	iBestValue = 0;
	pBestPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (pLoopCity != pCity)
		{
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
			{
				if (!(pLoopCity->AI_isDefended((!AI_isCityAIType()) ? pLoopCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isNotCityAIType) : 0)))	// XXX check for other team's units?
				{
					iValue = pLoopCity->getPopulation();

					if (pLoopCity->AI_isDanger())
					{
						iValue *= 2;
					}

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestPlot = pLoopCity->plot();
						FAssert(pLoopCity != pCity);
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardBonus(int iMinValue)
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	CvPlot* pBestGuardPlot;
	[[maybe_unused]] ImprovementTypes eImprovement;
	[[maybe_unused]] BonusTypes eNonObsoleteBonus;
	int iPathTurns;
	int iValue;
	int iBestValue;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestGuardPlot = nullptr;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Parallel_CvUnitAI_AI_guardBonus])
	{
		std::mutex mutex;

		struct Choice
		{
			int plotI{};
			int baseValue{};
		};

		std::vector<Choice> choices;
		const CvMap& map = GC.getMapINLINE();
		heck::parallelForEachN(map.numPlotsINLINE(), [&mutex, &choices, &map, iMinValue, this](int iI) {
			const CvPlot* const pLoopPlot = map.plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
				{
					const BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

					if (eNonObsoleteBonus != NO_BONUS)
					{
						const ImprovementTypes eImprovement = pLoopPlot->getImprovementType();

						if ((eImprovement != NO_IMPROVEMENT) && GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
						{
							int iValue = GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus);

							iValue += std::max(0, 200 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

							if (pLoopPlot->getPlotGroupConnectedBonus(getOwnerINLINE(), eNonObsoleteBonus) == 1)
							{
								iValue *= 2;
							}

							if (iValue > iMinValue)
							{
								if (!(pLoopPlot->isVisibleEnemyUnit(this)))
								{
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) == 0)
									{
										const std::lock_guard lock(mutex);
										choices.push_back({ iI, iValue * 1000 });
										//if (generatePath(pLoopPlot, 0, true, &iPathTurns))
										//{
										//	iValue *= 1000;
										//
										//	iValue /= (iPathTurns + 1);
										//
										//	if (iValue > iBestValue)
										//	{
										//		iBestValue = iValue;
										//		pBestPlot = getPathEndTurnPlot();
										//		pBestGuardPlot = pLoopPlot;
										//	}
										//}
									}
								}
							}
						}
					}
				}
			}
			});

		// Sort by distance.
		std::ranges::sort(choices, std::less<>(), [&map, originX = getX_INLINE(), originY = getY_INLINE()](Choice choice) {
			const CvPlot& pLoopPlot = *map.plotByIndexINLINE(choice.plotI);
			return std::pair(::stepDistance(originX, originY, pLoopPlot.getX_INLINE(), pLoopPlot.getY_INLINE()), choice.plotI);
			});

		int bestPlotI = 0;

		for (const auto [iI, baseValue] : choices)
		{
			CvPlot* const pLoopPlot = map.plotByIndexINLINE(iI);
			// iBestValue <= iValue / (iPathTurns + 1)
			// iPathTurns + 1 <= iValue / iBestValue
			// iPathTurns <= iValue / iBestValue - 1
			// iPathTurns <= floor(iValue / iBestValue) - 1
			const int maxTurns = iBestValue > 0 ? baseValue / iBestValue : INT_MAX;
			if (generatePath(pLoopPlot, PathingArg(0, maxTurns), true, &iPathTurns))
			{
				iValue = baseValue / (iPathTurns + 1);

				if (std::pair(iValue, -iI) > std::pair(iBestValue, -bestPlotI))
				{
					iBestValue = iValue;
					pBestPlot = getPathEndTurnPlot();
					pBestGuardPlot = pLoopPlot;
					bestPlotI = iI;
				}
			}
		}
	}
	else
#endif
	{
		for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			CvPlot* const pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
				{
					eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

					if (eNonObsoleteBonus != NO_BONUS)
					{
						eImprovement = pLoopPlot->getImprovementType();

						if ((eImprovement != NO_IMPROVEMENT) && GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
						{
							iValue = GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus);

							iValue += std::max(0, 200 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

							if (pLoopPlot->getPlotGroupConnectedBonus(getOwnerINLINE(), eNonObsoleteBonus) == 1)
							{
								iValue *= 2;
							}

							if (iValue > iMinValue)
							{
								if (!(pLoopPlot->isVisibleEnemyUnit(this)))
								{
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) == 0)
									{
										if (generatePath(pLoopPlot, 0, true, &iPathTurns))
										{
											iValue *= 1000;

											iValue /= (iPathTurns + 1);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
												pBestGuardPlot = pLoopPlot;
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

	if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
	{
		if (atPlot(pBestGuardPlot))
		{
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
	}

	return false;
}

int CvUnitAI::AI_getPlotDefendersNeeded(const CvPlot* pPlot, int iExtra) const
{
	int iNeeded = iExtra;
	BonusTypes eNonObsoleteBonus = pPlot->getNonObsoleteBonusType(getTeam());
	if (eNonObsoleteBonus != NO_BONUS)
	{
		iNeeded += (GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus) + 10) / 19;
	}

	int iDefense = pPlot->defenseModifier(getTeam(), true);

	iNeeded += (iDefense + 25) / 50;

	if (iNeeded == 0)
	{
		return 0;
	}

	iNeeded += GET_PLAYER(getOwnerINLINE()).AI_getPlotAirbaseValue(pPlot) / 50;

	int iNumHostiles = 0;
	int iNumPlots = 0;

	int iRange = 2;
	for (int iX = -iRange; iX <= iRange; iX++)
	{
		for (int iY = -iRange; iY <= iRange; iY++)
		{
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iX, iY);
			if (pLoopPlot != nullptr)
			{
				iNumHostiles += pLoopPlot->getNumVisibleEnemyDefenders(this);
				if ((pLoopPlot->getTeam() != getTeam()) || pLoopPlot->isCoastalLand())
				{
				    iNumPlots++;
                    if (isEnemy(pLoopPlot->getTeam()))
                    {
                        iNumPlots += 4;
                    }
				}
			}
		}
	}

	if ((iNumHostiles == 0) && (iNumPlots < 4))
	{
		if (iNeeded > 1)
		{
			iNeeded = 1;
		}
		else
		{
			iNeeded = 0;
		}
	}

	return iNeeded;
}

bool CvUnitAI::AI_guardFort(bool bSearch)
{
	PROFILE_FUNC();
	
	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
		ImprovementTypes eImprovement = plot()->getImprovementType();
		if (eImprovement != NO_IMPROVEMENT)
		{
			if (GC.getImprovementInfo(eImprovement).isActsAsCity())
			{
				if (plot()->plotCount(PUF_isCityAIType, -1, -1, getOwnerINLINE()) <= AI_getPlotDefendersNeeded(plot(), 0))
				{
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, plot());
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
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestGuardPlot = nullptr;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Parallel_CvUnitAI_AI_guardFort])
	{
		const CvMap& map = GC.getMapINLINE();
		std::mutex mutex;
		std::vector<int> fortPlotIndices;

		heck::parallelForEachN(map.numPlotsINLINE(), [&map, &mutex, &fortPlotIndices, this](int iI) {
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
								if (!(pLoopPlot->isVisibleEnemyUnit(this)))
								{
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) < iValue)
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
		std::ranges::sort(fortPlotIndices, std::less<>(), [&map, originX = getX_INLINE(), originY = getY_INLINE()](int iI) {
			const CvPlot& plot = *map.plotByIndexINLINE(iI);
			return std::pair(::stepDistance(originX, originY, plot.getX_INLINE(), plot.getY_INLINE()), iI);
			});

		int bestPlotI = 0;

		for (const int iI : fortPlotIndices)
		{
			CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			// iBestValue <= AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / (iPathTurns + 2)
			// (iPathTurns + 2) <= AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue
			// iPathTurns <= AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue - 2
			// iPathTurns <= floor(AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000 / iBestValue) - 2

			const int baseValue = AI_getPlotDefendersNeeded(pLoopPlot, 0) * 1000;
			const int maxTurns = iBestValue > 0 ? baseValue / iBestValue - 2 : INT_MAX;

			int iPathTurns{};
			if (generatePath(pLoopPlot, PathingArg(0, maxTurns), true, &iPathTurns))
			{
				const int iValue = baseValue / (iPathTurns + 2);

				if (std::pair(iValue, -iI) > std::pair(iBestValue, -bestPlotI))
				{
					iBestValue = iValue;
					pBestPlot = getPathEndTurnPlot();
					pBestGuardPlot = pLoopPlot;
					bestPlotI = iI;
				}
			}
		}
	}
	else
#endif
	{
		for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

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
								if (!(pLoopPlot->isVisibleEnemyUnit(this)))
								{
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_BONUS, getGroup()) < iValue)
									{
										int iPathTurns;
										if (generatePath(pLoopPlot, 0, true, &iPathTurns))
										{
											iValue *= 1000;

											iValue /= (iPathTurns + 2);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
												pBestGuardPlot = pLoopPlot;
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

	if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
	{
		if (atPlot(pBestGuardPlot))
		{
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_BONUS, pBestGuardPlot);
			return true;
		}
	}

	return false;
}
// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardCitySite()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestGuardPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestGuardPlot = nullptr;

	for (iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++)
	{
		pLoopPlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_GUARD_CITY, getGroup()) == 0)
		{
			if (generatePath(pLoopPlot, 0, true, &iPathTurns))
			{
				iValue = pLoopPlot->getCitySiteFoundValue(getOwnerINLINE());
				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					pBestPlot = getPathEndTurnPlot();
					pBestGuardPlot = pLoopPlot;
				}
			}
		}
	}
	
	if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
	{
		if (atPlot(pBestGuardPlot))
		{
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_CITY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}



// Returns true if a mission was pushed...
bool CvUnitAI::AI_guardSpy(int iRandomPercent)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestGuardPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestGuardPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
			{
				iValue = 0;
				if (pLoopCity->isProductionUnit())
				{
					if (isLimitedUnitClass((UnitClassTypes)(GC.getUnitInfo(pLoopCity->getProductionUnit()).getUnitClassType())))
					{
						iValue = 4;
					}
				}
				else if (pLoopCity->isProductionBuilding())
				{
					if (isLimitedWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pLoopCity->getProductionBuilding()).getBuildingClassType())))
					{
						iValue = 5;
					}
				}
				else if (pLoopCity->isProductionProject())
				{
					if (isLimitedProject(pLoopCity->getProductionProject()))
					{
						iValue = 6;
					}
				}
				if (iValue > 0)
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GUARD_SPY, getGroup()) == 0)
					{
						int iPathTurns;
						if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
						{
							iValue *= 100 + GC.getGameINLINE().getSorenRandNum(iRandomPercent, "AI Guard Spy");
							//iValue /= 100;
							iValue /= iPathTurns + 1;

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestGuardPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestGuardPlot != nullptr))
	{
		if (atPlot(pBestGuardPlot))
		{
			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_GUARD_SPY, pBestGuardPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GUARD_SPY, pBestGuardPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_destroySpy()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvCity* pBestCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestCity = nullptr;

	for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_getAttitude((PlayerTypes)iI) <= ATTITUDE_ANNOYED)
				{
					for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
					{
						if (AI_plotValid(pLoopCity->plot()))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_ATTACK_SPY, getGroup()) == 0)
							{
								if (generatePath(pLoopCity->plot(), 0, true))
								{
									iValue = (pLoopCity->getPopulation() * 2);

									iValue += pLoopCity->getYieldRate(YIELD_PRODUCTION);

									if (atPlot(pLoopCity->plot()))
									{
										iValue *= 4;
										iValue /= 3;
									}

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestCity = pLoopCity;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestCity != nullptr))
	{
		if (atPlot(pBestCity->plot()))
		{
			if (canDestroy(pBestCity->plot()))
			{
				if (pBestCity->getProduction() > ((pBestCity->getProductionNeeded() * 2) / 3))
				{
					if (pBestCity->isProductionUnit())
					{
						if (isLimitedUnitClass((UnitClassTypes)(GC.getUnitInfo(pBestCity->getProductionUnit()).getUnitClassType())))
						{
							getGroup()->pushMission(MISSION_DESTROY);
							return true;
						}
					}
					else if (pBestCity->isProductionBuilding())
					{
						if (isLimitedWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pBestCity->getProductionBuilding()).getBuildingClassType())))
						{
							getGroup()->pushMission(MISSION_DESTROY);
							return true;
						}
					}
					else if (pBestCity->isProductionProject())
					{
						if (isLimitedProject(pBestCity->getProductionProject()))
						{
							getGroup()->pushMission(MISSION_DESTROY);
							return true;
						}
					}
				}
			}

			getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, pBestCity->plot());
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestCity->plot());
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_sabotageSpy()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestSabotagePlot;
	bool abPlayerAngry[MAX_PLAYERS];
	ImprovementTypes eImprovement;
	BonusTypes eNonObsoleteBonus;
	int iValue;
	int iBestValue;
	int iI;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		abPlayerAngry[iI] = false;

		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_getAttitude((PlayerTypes)iI) <= ATTITUDE_ANNOYED)
				{
					abPlayerAngry[iI] = true;
				}
			}
		}
	}

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestSabotagePlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->isOwned())
			{
				if (pLoopPlot->getTeam() != getTeam())
				{
					if (abPlayerAngry[pLoopPlot->getOwnerINLINE()])
					{
						eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(pLoopPlot->getTeam());

						if (eNonObsoleteBonus != NO_BONUS)
						{
							eImprovement = pLoopPlot->getImprovementType();

							if ((eImprovement != NO_IMPROVEMENT) && GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus))
							{
								if (canSabotage(pLoopPlot))
								{
									iValue = GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus);

									if (pLoopPlot->isConnectedToCapital() && (pLoopPlot->getPlotGroupConnectedBonus(pLoopPlot->getOwnerINLINE(), eNonObsoleteBonus) == 1))
									{
										iValue *= 3;
									}

									if (iValue > 25)
									{
										if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ATTACK_SPY, getGroup()) == 0)
										{
											if (generatePath(pLoopPlot, 0, true))
											{
												if (iValue > iBestValue)
												{
													iBestValue = iValue;
													pBestPlot = getPathEndTurnPlot();
													pBestSabotagePlot = pLoopPlot;
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

	if ((pBestPlot != nullptr) && (pBestSabotagePlot != nullptr))
	{
		if (atPlot(pBestSabotagePlot))
		{
			getGroup()->pushMission(MISSION_SABOTAGE);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestSabotagePlot);
			return true;
		}
	}

	return false;
}


bool CvUnitAI::AI_pickupTargetSpy()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestPickupPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	pCity = plot()->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwnerINLINE() == getOwnerINLINE())
		{
			if (pCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()))
			{
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_ATTACK_SPY, pCity->plot());
				return true;
			}
		}
	}

	iBestValue = INT_MAX;
	pBestPlot = nullptr;
	pBestPickupPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (pLoopCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()))
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
					{
						iValue = iPathTurns;

						if (iValue < iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
							pBestPickupPlot = pLoopCity->plot();
							FAssert(!atPlot(pBestPlot));
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestPickupPlot != nullptr))
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestPickupPlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_chokeDefend()
{
	CvCity* pCity;
	int iPlotDanger;

	FAssert(AI_isCityAIType());

	// XXX what about amphib invasions?

	pCity = plot()->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwnerINLINE() == getOwnerINLINE())
		{
			if (pCity->AI_neededDefenders() > 1)
			{
				if (pCity->AI_isDefended(pCity->plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE(), NO_TEAM, PUF_isNotCityAIType)))
				{
					iPlotDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 3);

					if (iPlotDanger <= 4)
					{
						if (AI_anyAttack(1, 65, std::max(0, (iPlotDanger - 1))))
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_heal(int iDamagePercent, int iMaxPath)
{
	PROFILE_FUNC();

	CLLNode<IDInfo>* pEntityNode;
	std::vector<CvUnit*> aeDamagedUnits;
	CvSelectionGroup* pGroup;
	CvUnit* pLoopUnit;
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
	
	pGroup = getGroup();
	
	if (iDamagePercent == 0)
	{
	    iDamagePercent = 10;	    
	}	

	bRetreat = false;
	
    if (getGroup()->getNumUnits() == 1)
	{
	    if (getDamage() > 0)
        {
        	
            if (plot()->isCity() || (healTurns(plot()) == 1))
            {
                if (!(isAlwaysHeal()))
                {
                    getGroup()->pushMission(MISSION_HEAL);
                    return true;
                }
            }
        }
        return false;
	}
	
	iMaxPath = std::min(iMaxPath, 2);

	pEntityNode = getGroup()->headUnitNode();

    iTotalDamage = 0;
    iTotalHitpoints = 0;
    iHurtUnitCount = 0;
	while (pEntityNode != nullptr)
	{
		pLoopUnit = ::getUnit(pEntityNode->m_data);
		FAssert(pLoopUnit != nullptr);
		pEntityNode = pGroup->nextUnitNode(pEntityNode);

		int iDamageThreshold = (pLoopUnit->maxHitPoints() * iDamagePercent) / 100;

		if (NO_UNIT != getLeaderUnitType())
		{
			iDamageThreshold /= 2;
		}
		
		if (pLoopUnit->getDamage() > 0)
		{
		    iHurtUnitCount++;
		}
		iTotalDamage += pLoopUnit->getDamage();
		iTotalHitpoints += pLoopUnit->maxHitPoints();
		

		if (pLoopUnit->getDamage() > iDamageThreshold)
		{
			bRetreat = true;

			if (!(pLoopUnit->hasMoved()))
			{
				if (!(pLoopUnit->isAlwaysHeal()))
				{
					if (pLoopUnit->healTurns(pLoopUnit->plot()) <= iMaxPath)
					{
					    aeDamagedUnits.push_back(pLoopUnit);
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
    if (plot()->isCity() && (plot()->getOwnerINLINE() == getOwnerINLINE()))
	{
		FAssertMsg(((int) aeDamagedUnits.size()) <= iHurtUnitCount, "damaged units array is larger than our hurt unit count");

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::GeneralFixes] && aeDamagedUnits.size())
			getGroup()->resetPath();
#endif

		for (unsigned int iI = 0; iI < aeDamagedUnits.size(); iI++)
		{
			CvUnit* pUnitToHeal = aeDamagedUnits[iI];
			pUnitToHeal->joinGroup(nullptr);
			pUnitToHeal->getGroup()->pushMission(MISSION_HEAL);

			// note, removing the head unit from a group will force the group to be completely split if non-human
			if (pUnitToHeal == this)
			{
				bPushedMission = true;
			}

			iHurtUnitCount--;
		}
	}
	
	if ((iHurtUnitCount * 2) > pGroup->getNumUnits())
	{
		FAssertMsg(pGroup->getNumUnits() > 0, "group now has zero units");
	
	    if (AI_moveIntoCity(2))
		{
			return true;
		}
		else if (healRate(plot()) > 10)
	    {
            pGroup->pushMission(MISSION_HEAL);
            return true;
	    }
	}
	
	return bPushedMission;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_afterAttack()
{
	if (!isMadeAttack())
	{
		return false;
	}
	
	if (!canFight())
	{
		return false;
	}

	if (isBlitz())
	{
		return false;
	}

	if (getDomainType() == DOMAIN_LAND)
	{
		if (AI_guardCity(false, true, 1))
		{
			return true;
		}
	}

	if (AI_pillageRange(1))
	{
		return true;
	}

	if (AI_retreatToCity(false, false, 1))
	{
		return true;
	}

	if (AI_hide())
	{
		return true;
	}

	if (AI_goody(1))
	{
		return true;
	}

	if (AI_pillageRange(2))
	{
		return true;
	}

	if (AI_defend())
	{
		return true;
	}

	if (AI_safety())
	{
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_goldenAge()
{
	if (canGoldenAge(plot()))
	{
		getGroup()->pushMission(MISSION_GOLDEN_AGE);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_spreadReligion()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestSpreadPlot;
	ReligionTypes eReligion;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iPlayerMultiplierPercent;
	int iLoop;
	int iI;


    bool bCultureVictory = GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_CULTURE2);
	eReligion = NO_RELIGION;

	if (eReligion == NO_RELIGION)
	{
		if (GET_PLAYER(getOwnerINLINE()).getStateReligion() != NO_RELIGION)
		{
			if (m_pUnitInfo->getReligionSpreads(GET_PLAYER(getOwnerINLINE()).getStateReligion()) > 0)
			{
				eReligion = GET_PLAYER(getOwnerINLINE()).getStateReligion();
			}
		}
	}

	if (eReligion == NO_RELIGION)
	{
		for (iI = 0; iI < GC.getNumReligionInfos(); iI++)
		{
			//if (bCultureVictory || GET_TEAM(getTeam()).hasHolyCity((ReligionTypes)iI))
			{
				if (m_pUnitInfo->getReligionSpreads((ReligionTypes)iI) > 0)
				{
					eReligion = ((ReligionTypes)iI);
					break;
				}
			}
		}
	}

	if (eReligion == NO_RELIGION)
	{
		return false;
	}

	bool bHasHolyCity = GET_TEAM(getTeam()).hasHolyCity(eReligion);
	bool bHasAnyHolyCity = bHasHolyCity;
	if (!bHasAnyHolyCity)
	{
		for (iI = 0; !bHasAnyHolyCity && iI < GC.getNumReligionInfos(); iI++)
		{
			bHasAnyHolyCity = GET_TEAM(getTeam()).hasHolyCity((ReligionTypes)iI);
		}
	}

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestSpreadPlot = nullptr;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
		    iPlayerMultiplierPercent = 0;

			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				if (bHasHolyCity)
				{
					iPlayerMultiplierPercent = 100;
					if (!bCultureVictory || ((eReligion == GET_PLAYER(getOwnerINLINE()).getStateReligion()) && bHasHolyCity))
					{
						if (GET_PLAYER((PlayerTypes)iI).getStateReligion() == NO_RELIGION)
						{
							if (0 == (GET_PLAYER((PlayerTypes)iI).getNonStateReligionHappiness()))
							{
								iPlayerMultiplierPercent += 600;
							}
						}
						else if (GET_PLAYER((PlayerTypes)iI).getStateReligion() == eReligion)
						{
							iPlayerMultiplierPercent += 300;
						}
						else
						{
							if (GET_PLAYER((PlayerTypes)iI).hasHolyCity(GET_PLAYER((PlayerTypes)iI).getStateReligion()))
							{
								iPlayerMultiplierPercent += 50;
							}
							else
							{
								iPlayerMultiplierPercent += 300;
							}
						}
						
						int iReligionCount = GET_PLAYER((PlayerTypes)iI).countTotalHasReligion();
						int iCityCount = GET_PLAYER(getOwnerINLINE()).getNumCities();
						//magic formula to produce normalized adjustment factor based on religious infusion
						int iAdjustment = (100 * (iCityCount + 1));
						iAdjustment /= ((iCityCount + 1) + iReligionCount);
						iAdjustment = (((iAdjustment - 25) * 4) / 3);
						
						iAdjustment = (std::max(10, iAdjustment));
						
						iPlayerMultiplierPercent *= iAdjustment;
						iPlayerMultiplierPercent /= 100;

						// if we have a holy city, but not this holy city, then we will likely switch
						// religions soon, so try to spread this religion internally, not externally
						if (bHasAnyHolyCity && !bHasHolyCity)
						{
							iPlayerMultiplierPercent /= 10;
						}
					}
				}
			}
			else if (iI == getOwnerINLINE())
			{
				iPlayerMultiplierPercent = 100;
			}
			else if (bHasHolyCity)
			{
				iPlayerMultiplierPercent = 80;
			}
			
			if (iPlayerMultiplierPercent > 0)
			{
				for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
				{

					if (AI_plotValid(pLoopCity->plot()) && pLoopCity->area() == area())
					{
						if (canSpread(pLoopCity->plot(), eReligion))
						{
							if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
							{
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD, getGroup()) == 0)
								{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
									if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_spreadReligion])
									{
										// Calculate the base value first, and then determine max turns for pathing.
										iValue = (7 + (pLoopCity->getPopulation() * 4));

										bool bOurCity = false;
										if (pLoopCity->getOwnerINLINE() == getOwnerINLINE())
										{
											iValue *= (bCultureVictory ? 16 : 4);
											bOurCity = true;
										}
										else if (pLoopCity->getTeam() == getTeam())
										{
											iValue *= 3;
											bOurCity = true;
										}
										else
										{
											iValue *= iPlayerMultiplierPercent;
											iValue /= 100;
										}

										int iCityReligionCount = pLoopCity->getReligionCount();
										int iReligionCountFactor = iCityReligionCount;

										if (bOurCity)
										{
											// count cities with no religion the same as cities with 2 religions
											// prefer a city with exactly 1 religion already
											if (iCityReligionCount == 0)
											{
												iReligionCountFactor = 2;
											}
											else if (iCityReligionCount == 1)
											{
												iValue *= 2;
											}
										}
										else
										{
											// absolutely prefer cities with zero religions
											if (iCityReligionCount == 0)
											{
												iValue *= 2;
											}

											// not our city, so prefer the lowest number of religions (increment so no divide by zero)
											iReligionCountFactor++;
										}

										iValue /= iReligionCountFactor;



										bool bForceMove = false;
										if (isHuman())
										{
											//If human, prefer to spread to the player where automated from.
											if (plot()->getOwnerINLINE() == pLoopCity->getOwnerINLINE())
											{
												iValue *= 10;
												if (pLoopCity->isRevealed(getTeam(), false))
												{
													bForceMove = true;
												}
											}
										}

										iValue *= 1000;

										// iValue / iPathTurns + 2 > iBestValue
										// iPathTurns < iValue / (iBestValue - 2)

										if (generatePath(pLoopCity->plot(), PathingArg(0, iValue / (iBestValue - 2) + 1), true, &iPathTurns))
										{
											FAssert(iPathTurns > 0);
											iValue /= (iPathTurns + 2);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = bForceMove ? pLoopCity->plot() : getPathEndTurnPlot();
												pBestSpreadPlot = pLoopCity->plot();
											}
										}
									}
									else
#endif
									{
										if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
										{
											iValue = (7 + (pLoopCity->getPopulation() * 4));

											bool bOurCity = false;
											if (pLoopCity->getOwnerINLINE() == getOwnerINLINE())
											{
												iValue *= (bCultureVictory ? 16 : 4);
												bOurCity = true;
											}
											else if (pLoopCity->getTeam() == getTeam())
											{
												iValue *= 3;
												bOurCity = true;
											}
											else
											{
												iValue *= iPlayerMultiplierPercent;
												iValue /= 100;
											}

											int iCityReligionCount = pLoopCity->getReligionCount();
											int iReligionCountFactor = iCityReligionCount;

											if (bOurCity)
											{
												// count cities with no religion the same as cities with 2 religions
												// prefer a city with exactly 1 religion already
												if (iCityReligionCount == 0)
												{
													iReligionCountFactor = 2;
												}
												else if (iCityReligionCount == 1)
												{
													iValue *= 2;
												}
											}
											else
											{
												// absolutely prefer cities with zero religions
												if (iCityReligionCount == 0)
												{
													iValue *= 2;
												}

												// not our city, so prefer the lowest number of religions (increment so no divide by zero)
												iReligionCountFactor++;
											}

											iValue /= iReligionCountFactor;

											FAssert(iPathTurns > 0);

											bool bForceMove = false;
											if (isHuman())
											{
												//If human, prefer to spread to the player where automated from.
												if (plot()->getOwnerINLINE() == pLoopCity->getOwnerINLINE())
												{
													iValue *= 10;
													if (pLoopCity->isRevealed(getTeam(), false))
													{
														bForceMove = true;
													}
												}
											}

											iValue *= 1000;

											iValue /= (iPathTurns + 2);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = bForceMove ? pLoopCity->plot() : getPathEndTurnPlot();
												pBestSpreadPlot = pLoopCity->plot();
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

	if ((pBestPlot != nullptr) && (pBestSpreadPlot != nullptr))
	{
		if (atPlot(pBestSpreadPlot))
		{
			getGroup()->pushMission(MISSION_SPREAD, eReligion);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_spreadCorporation()
{
	PROFILE_FUNC();

	CorporationTypes eCorporation = NO_CORPORATION;	

	int iI;
	for (iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
	{
		if (m_pUnitInfo->getCorporationSpreads((CorporationTypes)iI) > 0)
		{
			eCorporation = ((CorporationTypes)iI);
			break;
		}
	}

	if (NO_CORPORATION == eCorporation)
	{
		return false;
	}
	bool bHasHQ = (GET_TEAM(getTeam()).hasHeadquarters((CorporationTypes)iI));

	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestSpreadPlot = nullptr;

	CvTeam& kTeam = GET_TEAM(getTeam());
	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && (bHasHQ || (getTeam() == kLoopPlayer.getTeam())))
		{
			int iLoopPlayerCorpCount = kLoopPlayer.countCorporations(eCorporation);
			CvTeam& kLoopTeam = GET_TEAM(kLoopPlayer.getTeam());
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); nullptr != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop))
			{
				if (AI_plotValid(pLoopCity->plot()))
				{
					if (canSpreadCorporation(pLoopCity->plot(), eCorporation))
					{
						if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0)
							{
								int iPathTurns;
								if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
								{
									int iValue = (10 + pLoopCity->getPopulation() * 2);

									if (pLoopCity->getOwnerINLINE() == getOwnerINLINE())
									{
										iValue *= 4;
									}
									else if (kLoopTeam.isVassal(getTeam()))
									{
										iValue *= 3;
									}
									else if (kTeam.isVassal(kLoopTeam.getID()))
									{
										if (iLoopPlayerCorpCount == 0)
										{
											iValue *= 10;
										}
										else
										{
											iValue *= 3;
											iValue /= 2;
										}
									}
									else if (pLoopCity->getTeam() == getTeam())
									{
										iValue *= 2;
									}

									if (iLoopPlayerCorpCount == 0)
									{
										//Generally prefer to heavily target one player
										iValue /= 2;
									}

									bool bForceMove = false;
									if (isHuman())
									{
										//If human, prefer to spread to the player where automated from.
										if (plot()->getOwnerINLINE() == pLoopCity->getOwnerINLINE())
										{
											iValue *= 10;
											if (pLoopCity->isRevealed(getTeam(), false))
											{
												bForceMove = true;
											}
										}
									}

									FAssert(iPathTurns > 0);

									iValue *= 1000;

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = bForceMove ? pLoopCity->plot() : getPathEndTurnPlot();
										pBestSpreadPlot = pLoopCity->plot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestSpreadPlot != nullptr))
	{
		if (atPlot(pBestSpreadPlot))
		{
			if (canSpreadCorporation(pBestSpreadPlot, eCorporation))
			{
				getGroup()->pushMission(MISSION_SPREAD_CORPORATION, eCorporation);
			}
			else
			{
				getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_SPREAD_CORPORATION, pBestSpreadPlot);
			}
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD_CORPORATION, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_spreadReligionAirlift()
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	ReligionTypes eReligion;
	int iValue;
	int iBestValue;

	if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity == nullptr)
	{
		return false;
	}

	if (pCity->getMaxAirlift() == 0)
	{
		return false;
	}

	//bool bCultureVictory = GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_CULTURE2);
	eReligion = NO_RELIGION;

	if (eReligion == NO_RELIGION)
	{
		if (GET_PLAYER(getOwnerINLINE()).getStateReligion() != NO_RELIGION)
		{
			if (m_pUnitInfo->getReligionSpreads(GET_PLAYER(getOwnerINLINE()).getStateReligion()) > 0)
			{
				eReligion = GET_PLAYER(getOwnerINLINE()).getStateReligion();
			}
		}
	}

	if (eReligion == NO_RELIGION)
	{
		for (int iI = 0; iI < GC.getNumReligionInfos(); iI++)
		{
			//if (bCultureVictory || GET_TEAM(getTeam()).hasHolyCity((ReligionTypes)iI))
			{
				if (m_pUnitInfo->getReligionSpreads((ReligionTypes)iI) > 0)
				{
					eReligion = ((ReligionTypes)iI);
					break;
				}
			}
		}
	}

	if (eReligion == NO_RELIGION)
	{
		return false;
	}

	iBestValue = 0;
	pBestPlot = nullptr;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && (getTeam() == kLoopPlayer.getTeam()))
		{
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); nullptr != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop))
			{
				if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
				{
					if (canSpread(pLoopCity->plot(), eReligion))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD, getGroup()) == 0)
						{
							iValue = (7 + (pLoopCity->getPopulation() * 4));
							
							int iCityReligionCount = pLoopCity->getReligionCount();
							int iReligionCountFactor = iCityReligionCount;

							// count cities with no religion the same as cities with 2 religions
							// prefer a city with exactly 1 religion already
							if (iCityReligionCount == 0)
							{
								iReligionCountFactor = 2;
							}
							else if (iCityReligionCount == 1)
							{
								iValue *= 2;
							}
							
							iValue /= iReligionCountFactor;
							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD, pBestPlot);
		return true;
	}

	return false;	
}

bool CvUnitAI::AI_spreadCorporationAirlift()
{
	PROFILE_FUNC();
	
	if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}

	CvCity* pCity = plot()->getPlotCity();

	if (pCity == nullptr)
	{
		return false;
	}

	if (pCity->getMaxAirlift() == 0)
	{
		return false;
	}
	
	CorporationTypes eCorporation = NO_CORPORATION;	

	for (int iI = 0; iI < GC.getNumCorporationInfos(); ++iI)
	{
		if (m_pUnitInfo->getCorporationSpreads((CorporationTypes)iI) > 0)
		{
			eCorporation = ((CorporationTypes)iI);
			break;
		}
	}

	if (NO_CORPORATION == eCorporation)
	{
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;

	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iI);
		if (kLoopPlayer.isAlive() && (getTeam() == kLoopPlayer.getTeam()))
		{
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); nullptr != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop))
			{
				if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
				{
					if (canSpreadCorporation(pLoopCity->plot(), eCorporation))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0)
						{
							int iValue = (pLoopCity->getPopulation() * 4);

							if (pLoopCity->getOwnerINLINE() == getOwnerINLINE())
							{
								iValue *= 4;
							}
							else
							{
								iValue *= 3;
							}
							
							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_SPREAD, pBestPlot);
		return true;
	}

	return false;	
}
	
// Returns true if a mission was pushed...
bool CvUnitAI::AI_discover(bool bThisTurnOnly, bool bFirstResearchOnly)
{
	TechTypes eDiscoverTech;
	bool bIsFirstTech;
	int iPercentWasted = 0;

	if (canDiscover(plot()))
	{
		eDiscoverTech = getDiscoveryTech();
		bIsFirstTech = (GET_PLAYER(getOwnerINLINE()).AI_isFirstTech(eDiscoverTech));

        if (bFirstResearchOnly && !bIsFirstTech)
        {
            return false;
        }

		iPercentWasted = (100 - ((getDiscoverResearch(eDiscoverTech) * 100) / getDiscoverResearch(NO_TECH)));
		FAssert(((iPercentWasted >= 0) && (iPercentWasted <= 100)));


        if (getDiscoverResearch(eDiscoverTech) >= GET_TEAM(getTeam()).getResearchLeft(eDiscoverTech))
        {
            if ((iPercentWasted < 51) && bFirstResearchOnly && bIsFirstTech)
            {
                getGroup()->pushMission(MISSION_DISCOVER);
                return true;
            }

            if (iPercentWasted < (bIsFirstTech ? 31 : 11))
            {
                //I need a good way to assess if the tech is actually valuable...
                //but don't have one.
                getGroup()->pushMission(MISSION_DISCOVER);
                return true;
            }
        }
        else if (bThisTurnOnly)
        {
            return false;
        }

        if (iPercentWasted <= 11)
        {
            if (GET_PLAYER(getOwnerINLINE()).getCurrentResearch() == eDiscoverTech)
            {
                getGroup()->pushMission(MISSION_DISCOVER);
                return true;
            }
        }
    }
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_lead(std::vector<UnitAITypes>& aeUnitAITypes)
{
	PROFILE_FUNC();

	FAssertMsg(!isHuman(), "isHuman did not return false as expected");
	FAssertMsg(AI_getUnitAIType() != NO_UNITAI, "AI_getUnitAIType() is not expected to be equal with NO_UNITAI");
	FAssert(NO_PLAYER != getOwnerINLINE());

	CvPlayer& kOwner = GET_PLAYER(getOwnerINLINE());

	bool bNeedLeader = false;
	for (int iI = 0; iI < MAX_CIV_TEAMS; iI++)
	{
		CvTeamAI& kLoopTeam = GET_TEAM((TeamTypes)iI);
		if (isEnemy((TeamTypes)iI))
		{
			if (kLoopTeam.countNumUnitsByArea(area()) > 0)
			{
				bNeedLeader = true;
				break;
			}
		}
	}

	CvUnit* pBestUnit = nullptr;
	CvPlot* pBestPlot = nullptr;

	// AI may use Warlords to create super-medic units
	CvUnit* pBestStrUnit = nullptr;
	CvPlot* pBestStrPlot = nullptr;

	CvUnit* pBestHealUnit = nullptr;
	CvPlot* pBestHealPlot = nullptr;

	if (bNeedLeader)
	{
		int iBestStrength = 0;
		int iBestHealing = 0;
		int iLoop;
		for (CvUnit* pLoopUnit = kOwner.firstUnit(&iLoop); pLoopUnit; pLoopUnit = kOwner.nextUnit(&iLoop))
		{
			for (unsigned int iI = 0; iI < aeUnitAITypes.size(); iI++)
			{
				if (pLoopUnit->AI_getUnitAIType() == aeUnitAITypes[iI] || NO_UNITAI == aeUnitAITypes[iI])
				{
					if (canLead(pLoopUnit->plot(), pLoopUnit->getID()))
					{
						if (AI_plotValid(pLoopUnit->plot()))
						{
							if (!(pLoopUnit->plot()->isVisibleEnemyUnit(this)))
							{
								if (generatePath(pLoopUnit->plot(), 0, true))
								{
									// pick the unit with the highest current strength
									int iCombatStrength = pLoopUnit->currCombatStr(nullptr, nullptr);
									if (iCombatStrength > iBestStrength)
									{
										iBestStrength = iCombatStrength;
										pBestStrUnit = pLoopUnit;
										pBestStrPlot = getPathEndTurnPlot();
									}
									
									// or the unit with the best healing ability
									int iHealing = pLoopUnit->getSameTileHeal() + pLoopUnit->getAdjacentTileHeal();
									if (iHealing > iBestHealing)
									{
										iBestHealing = iHealing;
										pBestHealUnit = pLoopUnit;
										pBestHealPlot = getPathEndTurnPlot();
									}
									
									if (GC.getGame().getSorenRandNum(3, "AI Warlord mash unit") != 0)
									{
										pBestPlot = pBestStrPlot;
										pBestUnit = pBestStrUnit;
									}
									else
									{
										pBestPlot = pBestHealPlot;
										pBestUnit = pBestHealUnit;
									}
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	if (pBestPlot)
	{
		if (atPlot(pBestPlot) && pBestUnit)
		{
			getGroup()->pushMission(MISSION_LEAD, pBestUnit->getID());
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed... 
// iMaxCounts = 1 would mean join a city if there's no existing joined GP of that type.
bool CvUnitAI::AI_join(int iMaxCount)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	SpecialistTypes eBestSpecialist;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;
	int iCount;

	iBestValue = 0;
	pBestPlot = nullptr;
	eBestSpecialist = NO_SPECIALIST;
	iCount = 0;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
			{
				if (generatePath(pLoopCity->plot(), 0, true))
				{
					for (iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
					{
						bool bDoesJoin = false;
						SpecialistTypes eSpecialist = (SpecialistTypes)iI;
						if (m_pUnitInfo->getGreatPeoples(eSpecialist))
						{
							bDoesJoin = true;
						}
						if (bDoesJoin)
						{
							iCount += pLoopCity->getSpecialistCount(eSpecialist);
							if (iCount >= iMaxCount)
							{
								return false;
							}
						}
						
						if (canJoin(pLoopCity->plot(), ((SpecialistTypes)iI)))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopCity->plot(), 2) == 0)
							{
								iValue = pLoopCity->AI_specialistValue(((SpecialistTypes)iI), pLoopCity->AI_avoidGrowth(), false);
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									eBestSpecialist = ((SpecialistTypes)iI);
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (eBestSpecialist != NO_SPECIALIST))
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_JOIN, eBestSpecialist);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed... 
// iMaxCount = 1 would mean construct only if there are no existing buildings 
//   constructed by this GP type.
bool CvUnitAI::AI_construct(int iMaxCount, int iMaxSingleBuildingCount, int iThreshold)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestConstructPlot;
	BuildingTypes eBestBuilding;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;
	int iCount;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestConstructPlot = nullptr;
	eBestBuilding = NO_BUILDING;
	iCount = 0;
	
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_construct])
	{
		// Only path to the city if a possible construct is found.
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()) && pLoopCity->area() == area())
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_CONSTRUCT, getGroup()) == 0)
					{
						bool pathChecked = false;

						for (iI = 0; iI < GC.getNumBuildingClassInfos(); iI++)
						{
							BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(iI);

							if (NO_BUILDING != eBuilding)
							{
								bool bDoesBuild = false;
								if ((m_pUnitInfo->getForceBuildings(eBuilding))
									|| (m_pUnitInfo->getBuildings(eBuilding)))
								{
									bDoesBuild = true;
								}

								if (bDoesBuild && (pLoopCity->getNumBuilding(eBuilding) > 0))
								{
									iCount++;
									if (iCount >= iMaxCount)
									{
										return false;
									}
								}

								if (bDoesBuild && GET_PLAYER(getOwnerINLINE()).getBuildingClassCount((BuildingClassTypes)GC.getBuildingInfo(eBuilding).getBuildingClassType()) < iMaxSingleBuildingCount)
								{
									if (canConstruct(pLoopCity->plot(), eBuilding))
									{
										iValue = pLoopCity->AI_buildingValue(eBuilding);

										if ((iValue > iThreshold) && (iValue > iBestValue))
										{
											if (pathChecked || generatePath(pLoopCity->plot(), 0, true))
											{
												pathChecked = true;
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
												pBestConstructPlot = pLoopCity->plot();
												eBestBuilding = eBuilding;
											}
											else // Can't path to this city, try another one.
												break;
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
	else
#endif
	{
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()) && pLoopCity->area() == area())
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_CONSTRUCT, getGroup()) == 0)
					{
						if (generatePath(pLoopCity->plot(), 0, true))
						{
							for (iI = 0; iI < GC.getNumBuildingClassInfos(); iI++)
							{
								BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(iI);

								if (NO_BUILDING != eBuilding)
								{
									bool bDoesBuild = false;
									if ((m_pUnitInfo->getForceBuildings(eBuilding))
										|| (m_pUnitInfo->getBuildings(eBuilding)))
									{
										bDoesBuild = true;
									}

									if (bDoesBuild && (pLoopCity->getNumBuilding(eBuilding) > 0))
									{
										iCount++;
										if (iCount >= iMaxCount)
										{
											return false;
										}
									}

									if (bDoesBuild && GET_PLAYER(getOwnerINLINE()).getBuildingClassCount((BuildingClassTypes)GC.getBuildingInfo(eBuilding).getBuildingClassType()) < iMaxSingleBuildingCount)
									{
										if (canConstruct(pLoopCity->plot(), eBuilding))
										{
											iValue = pLoopCity->AI_buildingValue(eBuilding);

											if ((iValue > iThreshold) && (iValue > iBestValue))
											{
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
												pBestConstructPlot = pLoopCity->plot();
												eBestBuilding = eBuilding;
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

	if ((pBestPlot != nullptr) && (pBestConstructPlot != nullptr) && (eBestBuilding != NO_BUILDING))
	{
		if (atPlot(pBestConstructPlot))
		{
			getGroup()->pushMission(MISSION_CONSTRUCT, eBestBuilding);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_CONSTRUCT, pBestConstructPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_switchHurry()
{
	CvCity* pCity;
	BuildingTypes eBestBuilding;
	int iValue;
	int iBestValue;
	int iI;

	pCity = plot()->getPlotCity();

	if ((pCity == nullptr) || (pCity->getOwnerINLINE() != getOwnerINLINE()))
	{
		return false;
	}

	iBestValue = 0;
	eBestBuilding = NO_BUILDING;

	for (iI = 0; iI < GC.getNumBuildingClassInfos(); iI++)
	{
		if (isWorldWonderClass((BuildingClassTypes)iI))
		{
			BuildingTypes eBuilding = (BuildingTypes)GC.getCivilizationInfo(getCivilizationType()).getCivilizationBuildings(iI);

			if (NO_BUILDING != eBuilding)
		{
				if (pCity->canConstruct(eBuilding))
			{
					if (pCity->getBuildingProduction(eBuilding) == 0)
				{
						if (getMaxHurryProduction(pCity) >= pCity->getProductionNeeded(eBuilding))
					{
							iValue = pCity->AI_buildingValue(eBuilding);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
								eBestBuilding = eBuilding;
							}
						}
					}
				}
			}
		}
	}

	if (eBestBuilding != NO_BUILDING)
	{
		pCity->pushOrder(ORDER_CONSTRUCT, eBestBuilding, -1, false, false, false);

		if (pCity->getProductionBuilding() == eBestBuilding)
		{
			if (canHurry(plot()))
			{
				getGroup()->pushMission(MISSION_HURRY);
				return true;
			}
		}

		FAssert(false);
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_hurry()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestHurryPlot;
	bool bHurry;
	int iTurnsLeft;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestHurryPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (canHurry(pLoopCity->plot()))
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_HURRY, getGroup()) == 0)
					{
						if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
						{
							bHurry = false;

							if (pLoopCity->isProductionBuilding())
							{
								if (isWorldWonderClass((BuildingClassTypes)(GC.getBuildingInfo(pLoopCity->getProductionBuilding()).getBuildingClassType())))
								{
									bHurry = true;
								}
							}

							if (bHurry)
							{
								iTurnsLeft = pLoopCity->getProductionTurnsLeft();

								iTurnsLeft -= iPathTurns;

								if (iTurnsLeft > 8)
								{
									iValue = iTurnsLeft;

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestHurryPlot = pLoopCity->plot();
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestHurryPlot != nullptr))
	{
		if (atPlot(pBestHurryPlot))
		{
			getGroup()->pushMission(MISSION_HURRY);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_HURRY, pBestHurryPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_greatWork()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestGreatWorkPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestGreatWorkPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (canGreatWork(pLoopCity->plot()))
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_GREAT_WORK, getGroup()) == 0)
					{
						if (generatePath(pLoopCity->plot(), 0, true))
						{
							iValue = pLoopCity->AI_calculateCulturePressure(true);
							iValue -= ((100 * pLoopCity->getCulture(pLoopCity->getOwnerINLINE())) / std::max(1, getGreatWorkCulture(pLoopCity->plot())));
							if (iValue > 0)
							{
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestGreatWorkPlot = pLoopCity->plot();
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestGreatWorkPlot != nullptr))
	{
		if (atPlot(pBestGreatWorkPlot))
		{
			getGroup()->pushMission(MISSION_GREAT_WORK);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_GREAT_WORK, pBestGreatWorkPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_offensiveAirlift()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pTargetCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}

	if (area()->getTargetCity(getOwnerINLINE()) != nullptr)
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

	iBestValue = 0;
	pBestPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (pLoopCity->area() != pCity->area())
		{
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
			{
				pTargetCity = pLoopCity->area()->getTargetCity(getOwnerINLINE());

				if (pTargetCity != nullptr)
				{
					AreaAITypes eAreaAIType = pTargetCity->area()->getAreaAIType(getTeam());
					if (((eAreaAIType == AREAAI_OFFENSIVE) || (eAreaAIType == AREAAI_DEFENSIVE) || (eAreaAIType == AREAAI_MASSING))
						|| pTargetCity->AI_isDanger())
					{
						iValue = 10000;

						iValue *= (GET_PLAYER(getOwnerINLINE()).AI_militaryWeight(pLoopCity->area()) + 10);
						iValue /= (GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), AI_getUnitAIType()) + 10);

						iValue += std::max(1, ((GC.getMapINLINE().maxStepDistance() * 2) - GC.getMapINLINE().calculatePathDistance(pLoopCity->plot(), pTargetCity->plot())));
						
						if (AI_getUnitAIType() == UNITAI_PARADROP)
						{
							CvCity* pNearestEnemyCity = GC.getMapINLINE().findCity(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), NO_PLAYER, NO_TEAM, false, false, getTeam());

							if (pNearestEnemyCity != nullptr)
							{
								int iDistance = plotDistance(pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), pNearestEnemyCity->getX_INLINE(), pNearestEnemyCity->getY_INLINE());
								if (iDistance <= getDropRange())
								{
									iValue *= 5;
								}
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopCity->plot();
							FAssert(pLoopCity != pCity);
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_paradrop(int iRange)
{
	PROFILE_FUNC();

	if (getGroup()->getNumUnits() > 1)
	{
		return false;
	}
	int iParatrooperCount = plot()->plotCount(PUF_isUnitAIType, UNITAI_PARADROP, -1, getOwnerINLINE());
	FAssert(iParatrooperCount > 0);

	CvPlot* pPlot = plot();

	if (!canParadrop(pPlot))
	{
		return false;
	}

	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;

	int iSearchRange = AI_searchRange(iRange);

	for (int iDX = -iSearchRange; iDX <= iSearchRange; ++iDX)
	{
		for (int iDY = -iSearchRange; iDY <= iSearchRange; ++iDY)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
				{
					if (canParadropAt(pPlot, pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
					{
						int iValue = 0;

						PlayerTypes eTargetPlayer = pLoopPlot->getOwnerINLINE();
						FAssert(NO_PLAYER != eTargetPlayer);

						if (NO_BONUS != pLoopPlot->getBonusType())
						{
							iValue += GET_PLAYER(eTargetPlayer).AI_bonusVal(pLoopPlot->getBonusType()) - 10;
						}

						for (int i = -1; i <= 1; ++i)
						{
							for (int j = -1; j <= 1; ++j)
							{
								CvPlot* pAdjacentPlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), i, j);
								if (nullptr != pAdjacentPlot)
								{
									CvCity* pAdjacentCity = pAdjacentPlot->getPlotCity();

									if (nullptr != pAdjacentCity)
									{
										if (pAdjacentCity->getOwnerINLINE() == eTargetPlayer)
										{
											int iAttackerCount = GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pAdjacentPlot, true);
											int iDefenderCount = pAdjacentPlot->getNumVisibleEnemyDefenders(this);
											iValue += 20 * (AI_attackOdds(pAdjacentPlot, true) - ((50 * iDefenderCount) / (iParatrooperCount + iAttackerCount)));
										}
									}
								}
							}
						}
						
						if (iValue > 0)
						{
							iValue += pLoopPlot->defenseModifier(getTeam(), ignoreBuildingDefense());

							CvUnit* pInterceptor = bestInterceptor(pLoopPlot);
							if (nullptr != pInterceptor)
							{
								int iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

								iInterceptProb *= std::max(0, (100 - evasionProbability()));
								iInterceptProb /= 100;

								iValue *= std::max(0, 100 - iInterceptProb / 2);
								iValue /= 100;
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;

							FAssert(pBestPlot != pPlot);
						}
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_PARADROP, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_protect(int iOddsThreshold)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_protect])
	{
		int bestPlotI = 0;

		// Okay, all that matters is enemy units. Find them.
		for (int playerI = 0; playerI < MAX_PLAYERS; ++playerI)
		{
			const CvPlayer& player = GET_PLAYER((PlayerTypes)playerI);

			// Can't do this because of always hostile units.
			//if (!atWar(getTeam(), player.getTeam()))
			//	continue;

			for (auto* node = player.headGroupCycleNode(); node; node = player.nextGroupCycleNode(node))
			{
				CvSelectionGroup* const group = player.getSelectionGroup(node->m_data);
				pLoopPlot = group->plot();
				if (pLoopPlot && pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
				{
					if (pLoopPlot->isVisibleEnemyUnit(this))
					{
						if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true))
						{
							iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

							if (iValue >= AI_finalOddsThreshold(pLoopPlot, iOddsThreshold))
							{
								iI = gGlobals.getMapINLINE().plotNumINLINE(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE());
								if (std::pair(iValue, -iI) > std::pair(iBestValue, -bestPlotI))
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									bestPlotI = iI;
									FAssert(!atPlot(pBestPlot));
								}
							}
						}
					}
				}
			}
		}
	}
	else
#endif
	{
		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
				{
					if (pLoopPlot->isVisibleEnemyUnit(this))
					{
						if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true))
						{
							iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

							if (iValue >= AI_finalOddsThreshold(pLoopPlot, iOddsThreshold))
							{
								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									FAssert(!atPlot(pBestPlot));
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_patrol()
{
	PROFILE_FUNC();

	CvPlot* pAdjacentPlot;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
	{
		pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), ((DirectionTypes)iI));

		if (pAdjacentPlot != nullptr)
		{
			if (AI_plotValid(pAdjacentPlot))
			{
				if (!(pAdjacentPlot->isVisibleEnemyUnit(this)))
				{
					if (generatePath(pAdjacentPlot, 0, true))
					{
						iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Patrol"));

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

							if (pAdjacentPlot->getOwnerINLINE() == getOwnerINLINE())
							{
								iValue += 10000;
							}
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_defend()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	if (AI_defendPlot(plot()))
	{
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}

	iSearchRange = AI_searchRange(1);

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (AI_defendPlot(pLoopPlot))
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								if (iPathTurns <= 1)
								{
									iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Defend"));

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
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_safety()
{
	PROFILE_FUNC();

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
				pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					if (AI_plotValid(pLoopPlot))
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (generatePath(pLoopPlot, ((iPass > 0) ? MOVE_IGNORE_DANGER : 0), true, &iPathTurns))
							{
								if (iPathTurns <= 1)
								{
									iCount = 0;

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
												FAssert(getGroup()->getHeadUnit() == this);

												if (pHeadUnit != this)
												{
													if (pHeadUnit->isWaiting() || !(pHeadUnit->canMove()))
													{
														FAssert(pLoopUnit != this);
														FAssert(pHeadUnit != getGroup()->getHeadUnit());
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
										iValue += GC.getGameINLINE().getSorenRandNum(50, "AI Safety");
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
	}

	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((iPass > 0) ? MOVE_IGNORE_DANGER : 0));
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_hide()
{
	PROFILE_FUNC();

	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	CvUnit* pHeadUnit;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	bool bValid;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iCount;
	int iDX, iDY;
	int iI;

	if (getInvisibleType() == NO_INVISIBLE)
	{
		return false;
	}

	iSearchRange = AI_searchRange(1);

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					bValid = true;

					for (iI = 0; iI < MAX_TEAMS; iI++)
					{
						if (GET_TEAM((TeamTypes)iI).isAlive())
						{
							if (pLoopPlot->isInvisibleVisible(((TeamTypes)iI), getInvisibleType()))
							{
								bValid = false;
								break;
							}
						}
					}

					if (bValid)
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								if (iPathTurns <= 1)
								{
									iCount = 1;

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
												FAssert(getGroup()->getHeadUnit() == this);

												if (pHeadUnit != this)
												{
													if (pHeadUnit->isWaiting() || !(pHeadUnit->canMove()))
													{
														FAssert(pLoopUnit != this);
														FAssert(pHeadUnit != getGroup()->getHeadUnit());
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
										iValue += GC.getGameINLINE().getSorenRandNum(50, "AI Hide");
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
	}

	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_goody(int iRange)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
//	CvPlot* pAdjacentPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;
//	int iI;

	if (isBarbarian())
	{
		return false;
	}

	iSearchRange = AI_searchRange(iRange);

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isRevealedGoody(getTeam()))
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								if (iPathTurns <= iRange)
								{
									iValue = (1 + GC.getGameINLINE().getSorenRandNum(10000, "AI Goody"));

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_explore()
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	CvPlot* pBestExplorePlot;

	pBestPlot = nullptr;
	pBestExplorePlot = nullptr;
	
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Localised_CvUnitAI_AI_explore])
	{
#if 0
		const auto t0 = std::chrono::steady_clock::now();
		struct Entry
		{
			int16_t x{}, y{};
			int baseValue = 0; // Everything before the division.
		};
		std::vector<Entry> entries;
		entries.reserve(1000);
		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			PROFILE("AI_explore 1");

			pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				iValue = 0;

				if (pLoopPlot->isRevealedGoody(getTeam()))
				{
					iValue += 100000;
				}

				if (iValue > 0 || GC.getGameINLINE().getSorenRandNum(4, "AI make explore faster ;)") == 0)
				{
					if (!(pLoopPlot->isRevealed(getTeam(), false)))
					{
						iValue += 10000;
					}
					// XXX is this too slow?
					for (iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
					{
						PROFILE("AI_explore 2");

						pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

						if (pAdjacentPlot != nullptr)
						{
							if (!(pAdjacentPlot->isRevealed(getTeam(), false)))
							{
								iValue += 1000;
							}
							else if (bNoContact)
							{
								if (pAdjacentPlot->getRevealedTeam(getTeam(), false) != pAdjacentPlot->getTeam())
								{
									iValue += 100;
								}
							}
						}
					}

					if (iValue > 0)
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0)
							{
								if (pLoopPlot->isAdjacentToLand())
								{
									iValue += 10000;
								}

								if (pLoopPlot->isOwned())
								{
									iValue += 5000;
								}

								iValue += GC.getGameINLINE().getSorenRandNum(250 * abs(xDistance(getX_INLINE(), pLoopPlot->getX_INLINE())) + abs(yDistance(getY_INLINE(), pLoopPlot->getY_INLINE())), "AI explore");

								if (!atPlot(pLoopPlot))
									entries.emplace_back(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iValue);
							}
						}
					}
				}
			}
		}

		std::ranges::sort(entries, std::less<>(), [x = getX_INLINE(), y = getY_INLINE()](Entry entry) {
			return ::stepDistance(x, y, entry.x, entry.y);
			//return -entry.baseValue;
			});

		int statNumEntriesPassedCheck1 = 0;
		int statNumEntriesPassedCheck2 = 0;
		int statNumEntriesPassedCheck3 = 0;
		int statNumEntriesPassedCheck4 = 0;

		for (const Entry entry : entries)
		{
			iValue = entry.baseValue;
			if (iValue / 4 > iBestValue)
			{
				++statNumEntriesPassedCheck1;
				pLoopPlot = GC.getMapINLINE().plot(entry.x, entry.y);
				const int maxTurns = iBestValue <= 0 ? INT_MAX : (iValue - 3 * iBestValue) / iBestValue + 1;
				//const int maxTurns = INT_MAX;
				if (iValue / (3 + std::max(1, gGlobals.enhancedDLLGameContext->guessPathTurns(*getGroup(), *pLoopPlot, maxTurns).value_or(0))) > iBestValue)
				{
					++statNumEntriesPassedCheck2;
					if (generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns))
					{
						++statNumEntriesPassedCheck3;
						iValue /= 3 + std::max(1, iPathTurns);

						if (iValue > iBestValue)
						{
							++statNumEntriesPassedCheck4;
							iBestValue = iValue;
							pBestPlot = pLoopPlot->isRevealedGoody(getTeam()) ? getPathEndTurnPlot() : pLoopPlot;
							pBestExplorePlot = pLoopPlot;
						}
					}
				}
			}
			//else
			//	break;
		}
		const auto t1 = std::chrono::steady_clock::now();
		std::wclog << __FUNCTIONW__ L" stats: "
			<< GET_PLAYER(getOwnerINLINE()).getName()
			<< L' ' << getName() << L' '
			<< statNumEntriesPassedCheck1 << L", "
			<< statNumEntriesPassedCheck2 << L", "
			<< statNumEntriesPassedCheck3 << L", "
			<< statNumEntriesPassedCheck4 << L", "
			<< std::chrono::duration<double, std::micro>(t1 - t0) << std::endl;

		//if (getDomainType() == DOMAIN_SEA)
		//{
		//	static int counter = 0;
		//	if (/*getOwnerINLINE() == BARBARIAN_PLAYER &&*/ counter++ == 0)
		//	{
		//		const CvPlayerAI& player = GET_PLAYER(getOwnerINLINE());
		//		std::ofstream dump("PlotDangerCache dump.pbm");
		//		dump << "P1\n" << GC.getMapINLINE().getGridWidthINLINE() << ' ' << GC.getMapINLINE().getGridHeightINLINE() << '\n';
		//		for (int y = 0; y < GC.getMapINLINE().getGridHeightINLINE(); ++y)
		//			for (int x = 0; x < GC.getMapINLINE().getGridWidthINLINE(); ++x)
		//				dump << (player.AI_getPlotDanger(GC.getMapINLINE().plot(x, y)) > 0);
		//	}
		//}
#else
		// Implementation 2. The "local exploration search" method.

		//const auto t0 = std::chrono::steady_clock::now();
		const GiganticMapsOptimisationsLib::GameContext::ExploreSearchResult explorationSearchResult = gGlobals.enhancedDLLGameContext->findExplorationTarget(*this);
		//const auto t1 = std::chrono::steady_clock::now();
		pBestPlot = explorationSearchResult.endMissionPlot;
		pBestExplorePlot = explorationSearchResult.target;

		//std::wclog << __FUNCTIONW__ L" stats: "
		//	<< GET_PLAYER(getOwnerINLINE()).getName()
		//	<< L' ' << getName() << L' '
		//	<< L"explore target " << (!pBestExplorePlot ? -1 : ::plotDistance(pBestExplorePlot->getX_INLINE(), pBestExplorePlot->getY_INLINE(), getX_INLINE(), getY_INLINE())) << L" plots away "
		//	<< std::chrono::duration<double, std::micro>(t1 - t0) << std::endl;
#endif
	}
	else
#endif
	{
		CvPlot* pLoopPlot;
		CvPlot* pAdjacentPlot;
		int iPathTurns;
		int iValue;
		int iBestValue;
		int iI, iJ;

		iBestValue = 0;

		bool bNoContact = (GC.getGameINLINE().countCivTeamsAlive() > GET_TEAM(getTeam()).getHasMetCivCount(true));

		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			PROFILE("AI_explore 1");

			pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				iValue = 0;

				if (pLoopPlot->isRevealedGoody(getTeam()))
				{
					iValue += 100000;
				}

				if (iValue > 0 || GC.getGameINLINE().getSorenRandNum(4, "AI make explore faster ;)") == 0)
				{
					if (!(pLoopPlot->isRevealed(getTeam(), false)))
					{
						iValue += 10000;
					}
					// XXX is this too slow?
					for (iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
					{
						PROFILE("AI_explore 2");

						pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

						if (pAdjacentPlot != nullptr)
						{
							if (!(pAdjacentPlot->isRevealed(getTeam(), false)))
							{
								iValue += 1000;
							}
							else if (bNoContact)
							{
								if (pAdjacentPlot->getRevealedTeam(getTeam(), false) != pAdjacentPlot->getTeam())
								{
									iValue += 100;
								}
							}
						}
					}

					if (iValue > 0)
					{
						if (!(pLoopPlot->isVisibleEnemyUnit(this)))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0)
							{
								if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns))
								{
									iValue += GC.getGameINLINE().getSorenRandNum(250 * abs(xDistance(getX_INLINE(), pLoopPlot->getX_INLINE())) + abs(yDistance(getY_INLINE(), pLoopPlot->getY_INLINE())), "AI explore");

									if (pLoopPlot->isAdjacentToLand())
									{
										iValue += 10000;
									}

									if (pLoopPlot->isOwned())
									{
										iValue += 5000;
									}

									iValue /= 3 + std::max(1, iPathTurns);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = pLoopPlot->isRevealedGoody(getTeam()) ? getPathEndTurnPlot() : pLoopPlot;
										pBestExplorePlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestExplorePlot != nullptr))
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_exploreRange(int iRange)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pAdjacentPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestExplorePlot;
	
	int iSearchRange;
	
	iSearchRange = AI_searchRange(iRange);

	pBestPlot = nullptr;
	pBestExplorePlot = nullptr;

	int iImpassableCount = GET_PLAYER(getOwnerINLINE()).AI_unitImpassableCount(getUnitType());

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Localised_CvUnitAI_AI_exploreRange])
	{
		// Okay. Instead, we'll use a pathfinder to find all reachable plots within a range. Much better than trying to path to unreachable plots (in the same CvArea).

		//const std::vector<LongRangeAStar::i16vec2> coordList = pathingState.visitAllReachable(astarInterface, INT_MAX, iRange);
		GiganticMapsOptimisationsLib::ExploreReachContext exploreReachContext;
		exploreReachContext.visitAllReachable(*this, MOVE_NO_ENEMY_TERRITORY, iRange);
		const std::span<const GiganticMapsOptimisationsLib::i16vec2> coordList = exploreReachContext.getVisitSet();
		std::vector<int> valueList(coordList.size());

		//std::clog << __FUNCTION__ ": " << coordList.size() << std::endl;

		for (auto&& [coord, value] : std::views::zip(coordList, valueList))
		{
			if (::stepDistance(getX_INLINE(), getY_INLINE(), coord.x, coord.y) > iSearchRange)
				continue;

			if (!GiganticMapsOptimisationsLib::calc_pathDestValid(*getGroup(), MOVE_NO_ENEMY_TERRITORY, coord))
				continue;
	
			const int iDX = xDistance(coord.x, getX_INLINE());
			const int iDY = yDistance(coord.y, getY_INLINE());

			pLoopPlot = GC.getMapINLINE().plot(coord.x, coord.y);

			if (atPlot(pLoopPlot))
				continue;

			// Verify.
			//if (!generatePath(pLoopPlot, PathingArg(MOVE_NO_ENEMY_TERRITORY, iRange + 1), false, &iPathTurns))
			//	std::abort();

			int iValue = 0;

			if (pLoopPlot->isRevealedGoody(getTeam()))
			{
				iValue += 100000;
			}

			if (!(pLoopPlot->isRevealed(getTeam(), false)))
			{
				iValue += 10000;
			}

			for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			{
				PROFILE("AI_exploreRange 2");

				pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iI));

				if (pAdjacentPlot != nullptr)
				{
					if (!(pAdjacentPlot->isRevealed(getTeam(), false)))
					{
						iValue += 1000;
					}
				}
			}

			if (iValue > 0)
			{
				if (!(pLoopPlot->isVisibleEnemyUnit(this)))
				{
					PROFILE("AI_exploreRange 3");

					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0)
					{
						PROFILE("AI_exploreRange 4");

						if (!atPlot(pLoopPlot) /*&& generatePath(pLoopPlot, PathingArg(MOVE_NO_ENEMY_TERRITORY, iRange), true, &iPathTurns)*/)
						{
							//if (iPathTurns <= iRange)
							{
								iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Explore");

								if (pLoopPlot->isAdjacentToLand())
								{
									iValue += 10000;
								}

								if (pLoopPlot->isOwned())
								{
									iValue += 5000;
								}

								if (!isHuman())
								{
									int iDirectionModifier = 100;

									if (AI_getUnitAIType() == UNITAI_EXPLORE_SEA && iImpassableCount == 0)
									{
										iDirectionModifier += (50 * (abs(iDX) + abs(iDY))) / iSearchRange;
										if (GC.getGame().circumnavigationAvailable())
										{
											if (GC.getMap().isWrapX())
											{
												if ((iDX * ((AI_getBirthmark() % 2 == 0) ? 1 : -1)) > 0)
												{
													iDirectionModifier *= 150 + ((iDX * 100) / iSearchRange);
												}
												else
												{
													iDirectionModifier /= 2;
												}
											}
											if (GC.getMap().isWrapY())
											{
												if ((iDY * (((AI_getBirthmark() >> 1) % 2 == 0) ? 1 : -1)) > 0)
												{
													iDirectionModifier *= 150 + ((iDY * 100) / iSearchRange);
												}
												else
												{
													iDirectionModifier /= 2;
												}
											}
										}
										iValue *= iDirectionModifier;
										iValue /= 100;
									}
								}

								value = iValue;
							}
						}
					}
				}
			}
		}

		if (const size_t bestI = std::ranges::max_element(valueList) - valueList.begin(); bestI < valueList.size() && valueList[bestI] > 0)
		{
			const heck::ivec2 bestCoord = coordList[bestI];
			pLoopPlot = GC.getMapINLINE().plot(bestCoord.x, bestCoord.y);
			if (getDomainType() == DOMAIN_LAND)
			{
				// Need to get the end turn plot.
				const heck::ivec2 stopCoord = exploreReachContext.getEndTurnStopCoord(*this, MOVE_NO_ENEMY_TERRITORY, bestCoord);
				pBestPlot = GC.getMapINLINE().plot(stopCoord.x, stopCoord.y);
			}
			else
			{
				pBestPlot = pLoopPlot;
			}
			pBestExplorePlot = pLoopPlot;
		}
	}
	else
#endif
	{
		int iI;
		int iPathTurns;
		int iValue;
		int iBestValue;
		int iDX, iDY;
		iBestValue = 0;
		for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
		{
			for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
			{
				PROFILE("AI_exploreRange 1");

				pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					if (AI_plotValid(pLoopPlot))
					{
						iValue = 0;

						if (pLoopPlot->isRevealedGoody(getTeam()))
						{
							iValue += 100000;
						}

						if (!(pLoopPlot->isRevealed(getTeam(), false)))
						{
							iValue += 10000;
						}

						for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
						{
							PROFILE("AI_exploreRange 2");

							pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iI));

							if (pAdjacentPlot != nullptr)
							{
								if (!(pAdjacentPlot->isRevealed(getTeam(), false)))
								{
									iValue += 1000;
								}
							}
						}

						if (iValue > 0)
						{
							if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							{
								PROFILE("AI_exploreRange 3");

								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_EXPLORE, getGroup(), 3) == 0)
								{
									PROFILE("AI_exploreRange 4");

									if (!atPlot(pLoopPlot) && generatePath(pLoopPlot, MOVE_NO_ENEMY_TERRITORY, true, &iPathTurns))
									{
										if (iPathTurns <= iRange)
										{
											iValue += GC.getGameINLINE().getSorenRandNum(10000, "AI Explore");

											if (pLoopPlot->isAdjacentToLand())
											{
												iValue += 10000;
											}

											if (pLoopPlot->isOwned())
											{
												iValue += 5000;
											}

											if (!isHuman())
											{
												int iDirectionModifier = 100;

												if (AI_getUnitAIType() == UNITAI_EXPLORE_SEA && iImpassableCount == 0)
												{
													iDirectionModifier += (50 * (abs(iDX) + abs(iDY))) / iSearchRange;
													if (GC.getGame().circumnavigationAvailable())
													{
														if (GC.getMap().isWrapX())
														{
															if ((iDX * ((AI_getBirthmark() % 2 == 0) ? 1 : -1)) > 0)
															{
																iDirectionModifier *= 150 + ((iDX * 100) / iSearchRange);
															}
															else
															{
																iDirectionModifier /= 2;
															}
														}
														if (GC.getMap().isWrapY())
														{
															if ((iDY * (((AI_getBirthmark() >> 1) % 2 == 0) ? 1 : -1)) > 0)
															{
																iDirectionModifier *= 150 + ((iDY * 100) / iSearchRange);
															}
															else
															{
																iDirectionModifier /= 2;
															}
														}
													}
													iValue *= iDirectionModifier;
													iValue /= 100;
												}
											}

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												if (getDomainType() == DOMAIN_LAND)
												{
													pBestPlot = getPathEndTurnPlot();
												}
												else
												{
													pBestPlot = pLoopPlot;
												}
												pBestExplorePlot = pLoopPlot;
											}
										}
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
										//std::clog << __FUNCTION__ " failed to path!" << std::endl;
										// Yep, that's happening.
#endif
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestExplorePlot != nullptr))
	{
		PROFILE("AI_exploreRange 5");

		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_NO_ENEMY_TERRITORY, false, false, MISSIONAI_EXPLORE, pBestExplorePlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_targetCity(int iFlags)
{
	PROFILE_FUNC();

	CvCity* pTargetCity;
	CvCity* pBestCity;
	CvPlot* pAdjacentPlot;
	CvPlot* pBestPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	iBestValue = 0;
	pBestCity = nullptr;

	pTargetCity = area()->getTargetCity(getOwnerINLINE());

	if (pTargetCity != nullptr)
	{
		if (AI_potentialEnemy(pTargetCity->getTeam(), pTargetCity->plot()))
		{
			if (!atPlot(pTargetCity->plot()) && generatePath(pTargetCity->plot(), iFlags, true))
			{
				pBestCity = pTargetCity;
			}
		}
	}

	//std::clog << __FUNCTION__": " << getID() << std::endl;

	if (pBestCity == nullptr)
	{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_targetCity])
		{
			std::vector<CvCity*> cityList;
			cityList.reserve(100);
			for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
				if (GET_PLAYER((PlayerTypes)iI).isAlive())
					for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
						if (AI_plotValid(pLoopCity->plot()) && AI_potentialEnemy(GET_PLAYER((PlayerTypes)iI).getTeam(), pLoopCity->plot()))
							cityList.push_back(pLoopCity);
			std::ranges::sort(cityList, std::less<>(), [this](const CvCity* city) {
				return std::pair(::stepDistance(getX_INLINE(), getY_INLINE(), city->getX_INLINE(), city->getY_INLINE()), city->getID());
				});
			for (CvCity* const pLoopCity : cityList)
			{
				const auto computeCityBaseValue = [&](bool overestimate) {
					iValue = 0;
					if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) //lemming?
					{
						iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, false, false, overestimate);
					}
					else
					{
						iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, true, true, overestimate);
					}

					iValue *= 1000;

					if ((area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
					{
						if (pLoopCity->calculateCulturePercent(getOwnerINLINE()) < 75)
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

					if (generatePath(pLoopCity->plot(), PathingArg(iFlags, maxPathTurns), true, &iPathTurns))
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
		else
#endif
		{
			for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
			{
				if (GET_PLAYER((PlayerTypes)iI).isAlive())
				{
					for (CvCity* pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
					{
						if (AI_plotValid(pLoopCity->plot()) && AI_potentialEnemy(GET_PLAYER((PlayerTypes)iI).getTeam(), pLoopCity->plot()))
						{
							if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), iFlags, true, &iPathTurns))
							{
								iValue = 0;
								if (AI_getUnitAIType() == UNITAI_ATTACK_CITY) //lemming?
								{
									iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, false, false);
								}
								else
								{
									iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, true, true);
								}

								iValue *= 1000;

								if ((area()->getAreaAIType(getTeam()) == AREAAI_DEFENSIVE))
								{
									if (pLoopCity->calculateCulturePercent(getOwnerINLINE()) < 75)
									{
										iValue /= 2;
									}
								}

								iValue /= (4 + iPathTurns*iPathTurns);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestCity = pLoopCity;
								}
							}
						}
					}
				}
			}
		}
	}

	if (pBestCity != nullptr)
	{
		iBestValue = 0;
		pBestPlot = nullptr;

		if (0 == (iFlags & MOVE_THROUGH_ENEMY))
		{
			for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
			{
				pAdjacentPlot = plotDirection(pBestCity->getX_INLINE(), pBestCity->getY_INLINE(), ((DirectionTypes)iI));

				if (pAdjacentPlot != nullptr)
				{
					if (AI_plotValid(pAdjacentPlot))
					{
						if (!(pAdjacentPlot->isVisibleEnemyUnit(this)))
						{
							if (generatePath(pAdjacentPlot, iFlags, true, &iPathTurns))
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
									pBestPlot = getPathEndTurnPlot();
								}
							}
						}
					}
				}
			}
		}


		else
		{
			pBestPlot =  pBestCity->plot();
		}

		if (pBestPlot != nullptr)
		{
			FAssert(!(pBestCity->at(pBestPlot)) || 0 != (iFlags & MOVE_THROUGH_ENEMY)); // no suicide missions...
			if (!atPlot(pBestPlot))
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), iFlags);
				return true;
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_targetBarbCity()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvCity* pBestCity;
	CvPlot* pAdjacentPlot;
	CvPlot* pBestPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	if (isBarbarian())
	{
		return false;
	}

	iBestValue = 0;
	pBestCity = nullptr;

	for (pLoopCity = GET_PLAYER(BARBARIAN_PLAYER).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(BARBARIAN_PLAYER).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (pLoopCity->isRevealed(getTeam(), false))
			{
				if (!atPlot(pLoopCity->plot()) &&
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
				(gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_targetBarbCity]
					? generatePath(pLoopCity->plot(), PathingArg(0, 10), true, &iPathTurns)
					: generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
#else
				generatePath(pLoopCity->plot(), 0, true, &iPathTurns)
#endif
					)
				{
					if (iPathTurns < 10)
					{
						iValue = GET_PLAYER(getOwnerINLINE()).AI_targetCityValue(pLoopCity, false);

						iValue *= 1000;

						iValue /= (iPathTurns + 1);

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestCity = pLoopCity;
						}
					}
				}
			}
		}
	}

	if (pBestCity != nullptr)
	{
		iBestValue = 0;
		pBestPlot = nullptr;

		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			pAdjacentPlot = plotDirection(pBestCity->getX_INLINE(), pBestCity->getY_INLINE(), ((DirectionTypes)iI));

			if (pAdjacentPlot != nullptr)
			{
				if (AI_plotValid(pAdjacentPlot))
				{
					if (!(pAdjacentPlot->isVisibleEnemyUnit(this)))
					{
						if (generatePath(pAdjacentPlot, 0, true, &iPathTurns))
						{
							iValue = std::max(0, (pAdjacentPlot->defenseModifier(getTeam(), false) + 100));

							if (!(pAdjacentPlot->isRiverCrossing(directionXY(pAdjacentPlot, pBestCity->plot()))))
							{
								iValue += (10 * -(GC.getRIVER_ATTACK_MODIFIER()));
							}

							iValue = std::max(1, iValue);

							iValue *= 1000;

							iValue /= (iPathTurns + 1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
							}
						}
					}
				}
			}
		}

		if (pBestPlot != nullptr)
		{
			FAssert(!(pBestCity->at(pBestPlot))); // no suicide missions...
			if (atPlot(pBestPlot))
			{
				getGroup()->pushMission(MISSION_SKIP);
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
				return true;
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_bombardCity()
{
	CvCity* pBombardCity;

	if (canBombard(plot()))
	{
		pBombardCity = bombardTarget(plot());
		FAssertMsg(pBombardCity != nullptr, "BombardCity is not assigned a valid value");

		// do not bombard cities with no defenders
		int iDefenderStrength = pBombardCity->plot()->AI_sumStrength(NO_PLAYER, getOwnerINLINE(), DOMAIN_LAND, /*bDefensiveBonuses*/ true, /*bTestAtWar*/ true, false);
		if (iDefenderStrength == 0)
		{
			return false;
		}
		
		// do not bombard cities if we have overwelming odds
		int iAttackOdds = getGroup()->AI_attackOdds(pBombardCity->plot(), /*bPotentialEnemy*/ true);
		if (iAttackOdds > 95)
		{
			return false;
		}
		
		// could also do a compare stacks call here if we wanted, the downside of that is that we may just have a lot more units
		// we may not want to suffer high casualties just to save a turn
		//getGroup()->AI_compareStacks(pBombardCity->plot(), /*bPotentialEnemy*/ true, /*bCheckCanAttack*/ true, /*bCheckCanMove*/ true);
		//int iOurStrength = pBombardCity->plot()->AI_sumStrength(getOwnerINLINE(), NO_PLAYER, DOMAIN_LAND, false, false, false)
		
		if (pBombardCity->getDefenseDamage() < ((GC.getMAX_CITY_DEFENSE_DAMAGE() * 3) / 4))
		{
			getGroup()->pushMission(MISSION_BOMBARD);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_cityAttack(int iRange, int iOddsThreshold, bool bFollow)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
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
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true, getTeam()) && pLoopPlot->isVisibleEnemyUnit(this)))
					{
						if (AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
						{
							if (!atPlot(pLoopPlot) && ((bFollow) ? canMoveInto(pLoopPlot, true) : (generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange))))
							{
								iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

								if (iValue >= AI_finalOddsThreshold(pLoopPlot, iOddsThreshold))
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = ((bFollow) ? pLoopPlot : getPathEndTurnPlot());
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((bFollow) ? MOVE_DIRECT_ATTACK : 0));
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_anyAttack(int iRange, int iOddsThreshold, int iMinStack, bool bFollow)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	FAssert(canMove());

	if (AI_rangeAttack(iRange))
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
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isVisibleEnemyUnit(this) || (pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getTeam())))
					{
						if (!atPlot(pLoopPlot) && ((bFollow) ? canMoveInto(pLoopPlot, true) : (generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange))))
						{
							if (pLoopPlot->getNumVisibleEnemyDefenders(this) >= iMinStack)
							{
								iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

								if (iValue >= AI_finalOddsThreshold(pLoopPlot, iOddsThreshold))
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = ((bFollow) ? pLoopPlot : getPathEndTurnPlot());
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((bFollow) ? MOVE_DIRECT_ATTACK : 0));
		return true;
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_rangeAttack(int iRange)
{
	PROFILE_FUNC();

	FAssert(canMove());

	if (!canRangeStrike())
	{
		return false;
	}

	int iSearchRange = AI_searchRange(iRange);

	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;

	for (int iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (int iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (pLoopPlot->isVisibleEnemyUnit(this) || (pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getTeam())))
				{
					if (!atPlot(pLoopPlot) && canRangeStrikeAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
					{
						int iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

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

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_RANGE_ATTACK, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0);
		return true;
	}

	return false;
}

bool CvUnitAI::AI_leaveAttack(int iRange, int iOddsThreshold, int iStrengthThreshold)
{
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvCity* pCity;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	FAssert(canMove());

	iSearchRange = iRange;
	
	iBestValue = 0;
	pBestPlot = nullptr;
	
	
	pCity = plot()->getPlotCity();
	
	if ((pCity != nullptr) && (pCity->getOwnerINLINE() == getOwnerINLINE()))
	{
		int iOurStrength = GET_PLAYER(getOwnerINLINE()).AI_getOurPlotStrength(plot(), 0, false, false);
    	int iEnemyStrength = GET_PLAYER(getOwnerINLINE()).AI_getEnemyPlotStrength(plot(), 2, false, false);
		if (iEnemyStrength > 0)
		{
    		if (((iOurStrength * 100) / iEnemyStrength) < iStrengthThreshold)
    		{
    			return false;    		    		
    		}
    		if (plot()->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE()) <= getGroup()->getNumUnits())
    		{
    			return false;    		
    		}
		}
	}

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isVisibleEnemyUnit(this) || (pLoopPlot->isCity() && AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot)))
					{
						if (!atPlot(pLoopPlot) && (generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange)))
						{
							if (pLoopPlot->getNumVisibleEnemyDefenders(this) > 0)
							{
								iValue = getGroup()->AI_attackOdds(pLoopPlot, true);

								if (iValue >= AI_finalOddsThreshold(pLoopPlot, iOddsThreshold))
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0);
		return true;
	}

	return false;
	
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_blockade()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestBlockadePlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestBlockadePlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (potentialWarAction(pLoopPlot))
			{
				pCity = pLoopPlot->getWorkingCity();

				if (pCity != nullptr)
				{
					if (pCity->isCoastal(GC.getMIN_WATER_SIZE_FOR_OCEAN()))
					{
						if (!(pCity->isBarbarian()))
						{
							FAssert(isEnemy(pCity->getTeam()) || GET_TEAM(getTeam()).AI_getWarPlan(pCity->getTeam()) != NO_WARPLAN);

							if (!(pLoopPlot->isVisibleEnemyUnit(this)) && canPlunder(pLoopPlot))
							{
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BLOCKADE, getGroup(), 2) == 0)
								{
									if (generatePath(pLoopPlot, 0, true, &iPathTurns))
									{
										iValue = 1;

										iValue += std::min(pCity->getPopulation(), pCity->countNumWaterPlots());

										iValue += GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pCity->plot());

										iValue += (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCity->plot(), MISSIONAI_ASSAULT, getGroup(), 2) * 3);

										if (canBombard(pLoopPlot))
										{
											iValue *= 2;
										}

										iValue *= 1000;

										iValue /= (iPathTurns + 1);
										
										if (iPathTurns == 1)
										{
											//Prefer to have movement remaining to Bombard + Plunder
											iValue *= 1 + std::min(2, getPathLastNode()->m_iData1);
										}

										// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
										// (because declaring war will pop us some unknown distance away)
										if (!isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam())
										{
											iValue /= 10;
										}

										if (iValue > iBestValue)
										{
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestBlockadePlot = pLoopPlot;
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

	if ((pBestPlot != nullptr) && (pBestBlockadePlot != nullptr))
	{
		FAssert(canPlunder(pBestBlockadePlot));
		if (atPlot(pBestBlockadePlot) && !isEnemy(pBestBlockadePlot->getTeam(), pBestBlockadePlot))
		{
			getGroup()->groupDeclareWar(pBestBlockadePlot, true);
		}
		
		if (atPlot(pBestBlockadePlot))
		{
			if (canBombard(plot()))
			{
				getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			}

			getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_pirateBlockade()
{
	PROFILE_FUNC();
	
	std::vector<int> aiDeathZone(GC.getMapINLINE().numPlotsINLINE(), 0);

	const auto doPlotDeathZoneCounting = [&aiDeathZone, this](int iI) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);
		if (AI_plotValid(pLoopPlot) || (pLoopPlot->isCity() && pLoopPlot->isAdjacentToArea(area())))
		{
			if (pLoopPlot->isOwned() && (pLoopPlot->getTeam() != getTeam()))
			{
				int iBestHostileMoves = 0;
				CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
				while (pUnitNode != nullptr)
				{
					CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);
					if (isEnemy(pLoopUnit->getTeam(), pLoopUnit->plot()))
					{
						if (pLoopUnit->getDomainType() == DOMAIN_SEA && !pLoopUnit->isInvisible(getTeam(), false))
						{
							if (pLoopUnit->canAttack())
							{
								if (pLoopUnit->currEffectiveStr(nullptr, nullptr, nullptr) > currEffectiveStr(pLoopPlot, pLoopUnit, nullptr))
								{
									iBestHostileMoves = std::max(iBestHostileMoves, pLoopUnit->getMoves());
								}
							}
						}
					}
				}
				if (iBestHostileMoves > 0)
				{
					for (int iX = -iBestHostileMoves; iX <= iBestHostileMoves; iX++)
					{
						for (int iY = -iBestHostileMoves; iY <= iBestHostileMoves; iY++)
						{
							CvPlot * pRangePlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iX, iY);
							if (pRangePlot != nullptr)
							{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
								std::atomic_ref(aiDeathZone[GC.getMap().plotNumINLINE(pRangePlot->getX_INLINE(), pRangePlot->getY_INLINE())])
									.fetch_add(1, std::memory_order_relaxed);
#else
								aiDeathZone[GC.getMap().plotNumINLINE(pRangePlot->getX_INLINE(), pRangePlot->getY_INLINE())]++;
#endif
							}
						}
					}
				}
			}
		}
		};

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Parallel_CvUnitAI_AI_pirateBlockade])
		heck::parallelForEachN(GC.getMapINLINE().numPlotsINLINE(), doPlotDeathZoneCounting);
	else
		heck::serialForEachN(GC.getMapINLINE().numPlotsINLINE(), doPlotDeathZoneCounting);
#else
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		doPlotDeathZoneCounting(iI);
#endif
	
	bool bIsInDanger = aiDeathZone[GC.getMap().plotNumINLINE(getX_INLINE(), getY_INLINE())] > 0;
	
	if (!bIsInDanger)
	{
		if (getDamage() > 0)
		{
			if (!plot()->isOwned() && !plot()->isAdjacentOwned())
			{
				if (AI_retreatToCity(false, false, 1 + getDamage() / 20))
				{
					return true;
				}
				getGroup()->pushMission(MISSION_SKIP);
				return true;
			}
		}
	}
	
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestBlockadePlot = nullptr;
	bool bBestIsForceMove = false;
	bool bBestIsMove = false;
	
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Parallel_CvUnitAI_AI_pirateBlockade])
	{
		// The trivial parallelisation of the above, by evaluating plots in parallel and pathing to them in distance order, was not good enough.
		// It seems the pathing was fast, but there was a lot of pathing.
		// So to combat that, we'll path to the whole map first, then evaluate plots.
		// If only the pathing could be done in parallel though...

		struct Evaluation
		{
			int iPopulationValue = 0;
			int baseValue = 0;
		};

		CvMap& map = GC.getMapINLINE();

		GiganticMapsOptimisationsLib::DeterministicParallelPlotHashRNG rng0(GC.getGameINLINE().getSorenRand(), "AI Pirate Blockade DeterministicParallelPlotHashRNG");

		const heck::DynamicArray2D<uint8_t> plotTargetMissionsMap = GiganticMapsOptimisationsLib::buildAIPlotTargetMap(GET_PLAYER(getOwnerINLINE()), std::array{ MISSIONAI_BLOCKADE }, getGroup(), 3);

		const auto evaluatePlot = [&map, bIsInDanger, &rng0, &plotTargetMissionsMap, this](int iI) -> std::optional<Evaluation> {
			const CvPlot* const pLoopPlot = map.plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				if (!(pLoopPlot->isVisibleEnemyUnit(this)) && canPlunder(pLoopPlot))
				{
					if (rng0.get(4, iI, "AI Pirate Blockade") == 0)
					{
						//if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BLOCKADE, getGroup(), 3) == 0)
						if (plotTargetMissionsMap[{ pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE() }] == 0)
						{
							int iBlockadedCount = 0;
							int iPopulationValue = 0;
							int iRange = GC.getDefineINT("SHIP_BLOCKADE_RANGE") - 1;
							for (int iX = -iRange; iX <= iRange; iX++)
							{
								for (int iY = -iRange; iY <= iRange; iY++)
								{
									CvPlot* pRangePlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iX, iY);
									if (pRangePlot != nullptr)
									{
										bool bPlotBlockaded = false;
										if (pRangePlot->isWater() && pRangePlot->isOwned() && isEnemy(pRangePlot->getTeam(), pLoopPlot))
										{
											bPlotBlockaded = true;
											iBlockadedCount += pRangePlot->getBlockadedCount(pRangePlot->getTeam());
										}

										if (!bPlotBlockaded)
										{
											CvCity* pPlotCity = pRangePlot->getPlotCity();
											if (pPlotCity != nullptr)
											{
												if (isEnemy(pPlotCity->getTeam(), pLoopPlot))
												{
													int iCityValue = 3 + pPlotCity->getPopulation();
													iCityValue *= (atWar(getTeam(), pPlotCity->getTeam()) ? 1 : 3);
													if (GET_PLAYER(pPlotCity->getOwnerINLINE()).isNoForeignTrade())
													{
														iCityValue /= 2;
													}
													iPopulationValue += iCityValue;

												}
											}
										}
									}
								}
							}

							// If iPopulationValue is zero, final iValue should also be zero, unless bIsInDanger.
							if (iPopulationValue > 0 || bIsInDanger)
							{
								return Evaluation{
									.iPopulationValue = iPopulationValue,
									.baseValue = iPopulationValue * 1000 / (16 + iBlockadedCount),
								};
							}
						}
					}
				}
			}

			return std::nullopt;
			};



		// This is for privateers, so we can just do fast uniform cost pathfinding across the whole map.
		// This may be a tad inefficient sometimes, but for now, privateers live to visit the whole map.
		GiganticMapsOptimisationsLib::WholeMapSimpleDijkstra wholeMapSimpleDijkstra;
		wholeMapSimpleDijkstra.visitAllReachable(*getGroup(), { getX_INLINE(), getY_INLINE() }, 0);

		struct Choice
		{
			int finalValue = 0;
			int plotI = -1;
			bool bBestIsForceMove = false;
			bool bBestIsMove = false;
			int dummy = 0;

			bool operator<(Choice b) const
			{
				return std::pair(finalValue, -plotI) < std::pair(b.finalValue, -b.plotI);
			}
		};
		static_assert(sizeof(Choice) == 16);

		std::atomic<Choice> bestChoice{};
		// It is lock-free on MSVC, but this is false because whatever.
		//static_assert(std::atomic<Choice>::is_always_lock_free);

		// As pathAdd would calculate it (except starting plot).
		const auto getTurnNumberAtSteps = [mpRemaining = movesLeft(), mpPerTurn = maxMoves(), mpPerStep = GC.getMOVE_DENOMINATOR()](int steps) {
			// When movesLeft first hits 0, turns is still 1. Then increments after.
			const int endMPUnwrapped = mpRemaining - steps * mpPerStep;
			return 1 - heck::fdiv(endMPUnwrapped, static_cast<unsigned int>(mpPerTurn));
			};

		const auto getMPAtSteps = [mpRemaining = movesLeft(), mpPerTurn = maxMoves(), mpPerStep = GC.getMOVE_DENOMINATOR(), getTurnNumberAtSteps](int steps) {
			// When movesLeft first hits 0, turns is still 1. Then increments after.
			const int endMPUnwrapped = mpRemaining - steps * mpPerStep;
			return endMPUnwrapped + (getTurnNumberAtSteps(steps) - 1) * mpPerTurn;
			};

		GiganticMapsOptimisationsLib::DeterministicParallelPlotHashRNG rng1(GC.getGameINLINE().getSorenRand(), "AI Pirate Retreat DeterministicParallelPlotHashRNG");
		GiganticMapsOptimisationsLib::DeterministicParallelPlotHashRNG rng2(GC.getGameINLINE().getSorenRand(), "AI Pirate Retreat 2 DeterministicParallelPlotHashRNG");

		// Then enumerate the results in parallel. No more pathfinding is needed, except for mission exec.
		heck::parallelForEachN(GC.getMapINLINE().numPlotsINLINE(), [&bestChoice, &map = GC.getMapINLINE(), &evaluatePlot,
			&wholeMapSimpleDijkstra, &aiDeathZone, bIsInDanger, getTurnNumberAtSteps, getMPAtSteps, &rng1, &rng2, &thisUnit = *this](int iI) {

				const std::optional<Evaluation> entry = evaluatePlot(iI);
				if (!entry)
					return;

				const auto [iPopulationValue, baseValue] = *entry;

				const CvPlot* const pLoopPlot = map.plotByIndexINLINE(iI);

				const int pathSteps = wholeMapSimpleDijkstra.getPathStepDistance(GiganticMapsOptimisationsLib::getPlotCoord(*pLoopPlot));
				const int iPathTurns = std::max(1, getTurnNumberAtSteps(pathSteps));
				const int endingMP = getMPAtSteps(pathSteps);

				int iValue = baseValue;

				bool bMove = iPathTurns == 1 && endingMP > 0;
				if (thisUnit.atPlot(pLoopPlot))
				{
					iValue *= 3;
				}
				else if (bMove)
				{
					iValue *= 2;
				}

				int iDeath = aiDeathZone[iI];

				bool bForceMove = false;
				if (iDeath)
				{
					iValue /= 10;
				}
				else if (bIsInDanger && (iPathTurns <= 2) && (0 == iPopulationValue))
				{
					if (endingMP == 0)
					{
						if (!pLoopPlot->isAdjacentOwned())
						{
							int iRand = rng1.get(2500, iI, "AI Pirate Retreat");
							iValue += iRand;
							if (iRand > 1000)
							{
								iValue += rng2.get(2500, iI, "AI Pirate Retreat");
								bForceMove = true;
							}
						}
					}
				}

				if (!bForceMove)
				{
					iValue /= iPathTurns + 1;
				}

				heck::atomicMaxRelaxed(bestChoice, Choice{
					iValue,
					iI,
					bForceMove,
					bMove,
					});
			});

		if (const Choice localChoice = bestChoice.load(std::memory_order_relaxed); localChoice.plotI >= 0)
		{
			pBestBlockadePlot = map.plotByIndexINLINE(localChoice.plotI);
			bBestIsForceMove = localChoice.bBestIsForceMove;
			bBestIsMove = localChoice.bBestIsMove;
			if (bBestIsForceMove)
				pBestPlot = pBestBlockadePlot;
			else if (generatePath(pBestBlockadePlot, 0, true))
				pBestPlot = getPathEndTurnPlot();
			else
			{
				// Oh dear... Just go with something.
				FAssertMsg(false, __FUNCTION__" found a path to the best plot using WholeMapSimpleDijkstra, but the general pathfinder could not.");
				pBestPlot = pBestBlockadePlot;
			}

		}
	}
	else
#endif
	{
		int iPathTurns;
		int iValue;
		int iI;
		int iBestValue = 0;

		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot))
			{
				if (!(pLoopPlot->isVisibleEnemyUnit(this)) && canPlunder(pLoopPlot))
				{
					if (GC.getGame().getSorenRandNum(4, "AI Pirate Blockade") == 0)
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BLOCKADE, getGroup(), 3) == 0)
						{
							if (generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								int iBlockadedCount = 0;
								int iPopulationValue = 0;
								int iRange = GC.getDefineINT("SHIP_BLOCKADE_RANGE") - 1;
								for (int iX = -iRange; iX <= iRange; iX++)
								{
									for (int iY = -iRange; iY <= iRange; iY++)
									{
										CvPlot* pRangePlot = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iX, iY);
										if (pRangePlot != nullptr)
										{
											bool bPlotBlockaded = false;
											if (pRangePlot->isWater() && pRangePlot->isOwned() && isEnemy(pRangePlot->getTeam(), pLoopPlot))
											{
												bPlotBlockaded = true;
												iBlockadedCount += pRangePlot->getBlockadedCount(pRangePlot->getTeam());
											}

											if (!bPlotBlockaded)
											{
												CvCity* pPlotCity = pRangePlot->getPlotCity();
												if (pPlotCity != nullptr)
												{
													if (isEnemy(pPlotCity->getTeam(), pLoopPlot))
													{
														int iCityValue = 3 + pPlotCity->getPopulation();
														iCityValue *= (atWar(getTeam(), pPlotCity->getTeam()) ? 1 : 3);
														if (GET_PLAYER(pPlotCity->getOwnerINLINE()).isNoForeignTrade())
														{
															iCityValue /= 2;
														}
														iPopulationValue += iCityValue;

													}
												}
											}
										}
									}
								}
								iValue = iPopulationValue;

								iValue *= 1000;

								iValue /= 16 + iBlockadedCount;

								bool bMove = ((getPathLastNode()->m_iData2 == 1) && getPathLastNode()->m_iData1 > 0);
								if (atPlot(pLoopPlot))
								{
									iValue *= 3;
								}
								else if (bMove)
								{
									iValue *= 2;
								}

								int iDeath = aiDeathZone[GC.getMap().plotNumINLINE(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE())];

								bool bForceMove = false;
								if (iDeath)
								{
									iValue /= 10;
								}
								else if (bIsInDanger && (iPathTurns <= 2) && (0 == iPopulationValue))
								{
									if (getPathLastNode()->m_iData1 == 0)
									{
										if (!pLoopPlot->isAdjacentOwned())
										{
											int iRand = GC.getGame().getSorenRandNum(2500, "AI Pirate Retreat");
											iValue += iRand;
											if (iRand > 1000)
											{
												iValue += GC.getGame().getSorenRandNum(2500, "AI Pirate Retreat");
												bForceMove = true;
											}
										}
									}
								}

								if (!bForceMove)
								{
									iValue /= iPathTurns + 1;
								}

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = bForceMove ? pLoopPlot : getPathEndTurnPlot();
									pBestBlockadePlot = pLoopPlot;
									bBestIsForceMove = bForceMove;
									bBestIsMove = bMove;
								}
							}
						}
					}
				}
			}
		}
	}

	//std::wclog << __FUNCTIONW__ L": " << GiganticMapsOptimisationsLib::getFullPlotDescription(*plot())
	//	<< L" going to " << (pBestBlockadePlot ? heck::ivec2(GiganticMapsOptimisationsLib::getPlotCoord(*pBestBlockadePlot)) : heck::ivec2(-1))
	//	<< L" dt=" << std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
	//	<< L'\n';


	if ((pBestPlot != nullptr) && (pBestBlockadePlot != nullptr))
	{
		FAssert(canPlunder(pBestBlockadePlot));

		if (atPlot(pBestBlockadePlot))
		{
			getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			if (bBestIsForceMove)
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
				if (bBestIsMove)
				{
					getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BLOCKADE, pBestBlockadePlot);
				}
				return true;
			}
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_seaBombardRange(int iMaxRange)
{
	PROFILE_FUNC();
	
	// cached values
	CvPlayerAI& kPlayer = GET_PLAYER(getOwnerINLINE());
	CvPlot* pPlot = plot();
	CvSelectionGroup* pGroup = getGroup();
	
	// can any unit in this group bombard?
	bool bHasBombardUnit = false;
	bool bBombardUnitCanBombardNow = false;
	CLLNode<IDInfo>* pUnitNode = pGroup->headUnitNode();
	while (pUnitNode != nullptr && !bBombardUnitCanBombardNow)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pGroup->nextUnitNode(pUnitNode);

		if (pLoopUnit->bombardRate() > 0)
		{
			bHasBombardUnit = true;

			if (pLoopUnit->canMove() && !pLoopUnit->isMadeAttack())
			{
				bBombardUnitCanBombardNow = true;
			}
		}
	}

	if (!bHasBombardUnit)
	{
		return false;
	}

	// best match
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestBombardPlot = nullptr;
	int iBestValue = 0;
	
	// iterate over plots at each range
	for (int iDX = -(iMaxRange); iDX <= iMaxRange; iDX++)
	{
		for (int iDY = -(iMaxRange); iDY <= iMaxRange; iDY++)
		{
			CvPlot* pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);
			
			if (pLoopPlot != nullptr && AI_plotValid(pLoopPlot))
			{
				CvCity* pBombardCity = bombardTarget(pLoopPlot);

				if (pBombardCity != nullptr && isEnemy(pBombardCity->getTeam(), pLoopPlot) && pBombardCity->getDefenseDamage() < GC.getMAX_CITY_DEFENSE_DAMAGE())
				{
					int iPathTurns;
					if (generatePath(pLoopPlot, 0, true, &iPathTurns))
					{
						int iValue = (AI_getUnitAIType() == UNITAI_ASSAULT_SEA) ? 0 : 1; 
						
						iValue += (kPlayer.AI_plotTargetMissionAIs(pBombardCity->plot(), MISSIONAI_ASSAULT, nullptr, 2) * 3);
						iValue += (kPlayer.AI_adjacentPotentialAttackers(pBombardCity->plot(), true));
						
						if (iValue > 0)
						{
							iValue *= 1000;

							iValue /= (iPathTurns + 1);
							
							if (iPathTurns == 1)
							{
								//Prefer to have movement remaining to Bombard + Plunder
								iValue *= 1 + std::min(2, getPathLastNode()->m_iData1);
							}

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestBombardPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}
	
	if ((pBestPlot != nullptr) && (pBestBombardPlot != nullptr))
	{
		if (atPlot(pBestBombardPlot))
		{
			// if we are at the plot from which to bombard, and we have a unit that can bombard this turn, do it
			if (bBombardUnitCanBombardNow && pGroup->canBombard(pBestBombardPlot))
			{
				getGroup()->pushMission(MISSION_BOMBARD, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);

				// if city bombarded enough, wake up any units that were waiting to bombard this city
				CvCity* pBombardCity = bombardTarget(pBestBombardPlot); // is nullptr if city cannot be bombarded any more
				if (pBombardCity == nullptr || pBombardCity->getDefenseDamage() < ((GC.getMAX_CITY_DEFENSE_DAMAGE()*5)/6))
				{
					kPlayer.AI_wakePlotTargetMissionAIs(pBestBombardPlot, MISSIONAI_BLOCKADE, getGroup());
				}
			}
			// otherwise, skip until next turn, when we will surely bombard
			else if (canPlunder(pBestBombardPlot))
			{
				getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);
			}
			else
			{
				getGroup()->pushMission(MISSION_SKIP);
			}

			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BLOCKADE, pBestBombardPlot);
			return true;
		}
	}

	return false;
}



// Returns true if a mission was pushed...
bool CvUnitAI::AI_pillage(int iBonusValueThreshold)
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	CvPlot* pBestPillagePlot;

	pBestPlot = nullptr;
	pBestPillagePlot = nullptr;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_pillage])
	{
		struct Choice
		{
			int value = 0;
			int movePlotI = 0;
			int pillagePlotI = -1;
			int dummy = 0; // padding for atomic, if needed

			bool operator<(Choice b) const
			{
				return std::pair(value, -pillagePlotI) < std::pair(b.value, -b.pillagePlotI);
			}
		};

		const CvMap& map = GC.getMapINLINE();
		const CvCity* const areaTargetCity = area()->getTargetCity(getOwnerINLINE());

		if (!m_pUnitInfo->isPillage()) // Just in case...
			return false;

		// Let's try to do this a Smart way.
		// Valid targets include all improvements around potential enemy cities (that are not barbarian).
		// So, because value is divided by turns, sort cities by distance.
		// I expect that the best final value will come early and all other cities will early-out because maxTurns is too low.
		// This gets you a mostly *localised* AI procedure.
		// In order to avoid duplicate plot checks, check that the plot is assigned to the city.

		// TODO: Verify against firaxis implementation. Should yield exact same result.

		// This only needs to be a superset of possible target city teams. ~std::bitset<MAX_TEAMS>() is a valid value.
		std::bitset<MAX_TEAMS> targetCityTeams{};
		for (const auto teamI : heck::range<TeamTypes>(MAX_TEAMS))
		{
			// Team filter in potentialWarAction (and no barbarians)
			// Assuming not always hostile.
			targetCityTeams[teamI] = atWar(teamI, getTeam()) || (GiganticMapsOptimisationsLib::getSingleUnit_AI_isDeclareWar(*getGroup()->getHeadUnit(), teamI) && GET_TEAM(getTeam()).AI_getWarPlan(teamI) != NO_WARPLAN);
		}
		if (isAlwaysHostile(nullptr))
		{
			// Everybody except us.
			targetCityTeams = {};
			targetCityTeams = ~targetCityTeams;
		}
		targetCityTeams[BARBARIAN_TEAM] = false;

		std::vector<const CvCity*> citySearchList;

		for (const auto targetPlayerI : heck::range<PlayerTypes>(MAX_TEAMS))
		{
			const CvPlayer& player = GET_PLAYER(targetPlayerI);
			if (player.getNumCities() > 0 && targetCityTeams[player.getTeam()])
			{
				int it{};
				for (const CvCity* city = player.firstCity(&it); city; city = player.nextCity(&it))
					citySearchList.push_back(city);
			}
		}

		std::ranges::sort(citySearchList, std::less<>(), [originX = getX_INLINE(), originY = getY_INLINE()](const CvCity* city) {
			// Stable sorting. Every city has a unique sort key.
			return std::tuple(::stepDistance(originX, originY, city->getX_INLINE(), city->getY_INLINE()), city->getOwnerINLINE(), city->getID());
			});

		const CvPlayer& unitPlayer = GET_PLAYER(getOwnerINLINE());

		Choice bestChoice{};

		for (const CvCity* const targetCity : citySearchList)
		{
			if (targetCity == areaTargetCity)
				continue;

			for (const int workingPlotI : heck::range(NUM_CITY_PLOTS))
			{
				if (const CvPlot* const pLoopPlot = plotCity(targetCity->getX_INLINE(), targetCity->getY_INLINE(), workingPlotI);
					pLoopPlot && pLoopPlot->getWorkingCity() == targetCity)
				{
					if (AI_plotValid(pLoopPlot) && !pLoopPlot->isBarbarian())
					{
						if (potentialWarAction(pLoopPlot) && canPillage(pLoopPlot))
						{
							if (!pLoopPlot->isVisibleEnemyUnit(this))
							{
								if (unitPlayer.AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup(), 1) == 0)
								{
									// iValue = floor(floor(AI_pillageValue * 1000 / (iPathTurns + 1)) / penalty)
									// iBestValue < floor(floor(AI_pillageValue * 1000 / (iPathTurns + 1)) / penalty)
									// iBestValue < floor(AI_pillageValue * 1000 / (iPathTurns + 1)) / penalty
									// iBestValue * penalty < floor(AI_pillageValue * 1000 / (iPathTurns + 1))
									// iBestValue * penalty < AI_pillageValue * 1000 / (iPathTurns + 1)
									// iBestValue * penalty * (iPathTurns + 1) < AI_pillageValue * 1000
									// iPathTurns + 1 <= AI_pillageValue * 1000 / (iBestValue * penalty)
									// iPathTurns <= floor(AI_pillageValue * 1000 / (iBestValue * penalty) - 1)
									const int pillageValue = AI_pillageValue(pLoopPlot, iBonusValueThreshold);
									const bool hasPenalty = !isEnemy(pLoopPlot->getTeam()) && plot()->getTeam() == pLoopPlot->getTeam();
									constexpr int kPenalty = 10;
									const int maxTurns = bestChoice.value > 0 ? pillageValue * 1000 / (bestChoice.value * (hasPenalty ? kPenalty : 1)) - 1 : INT_MAX;

									int iPathTurns{};
									if (generatePath(pLoopPlot, PathingArg(0, maxTurns), true, &iPathTurns))
									{
										int iValue = pillageValue;

										iValue *= 1000;

										iValue /= (iPathTurns + 1);

										// if not at war with this plot owner, then devalue plot if we already inside this owner's borders
										// (because declaring war will pop us some unknown distance away)
										if (hasPenalty)
											iValue /= kPenalty;

										const CvPlot& movePlot = *getPathEndTurnPlot();

										bestChoice = std::max(bestChoice, Choice{
											.value = iValue,
											.movePlotI = map.plotNumINLINE(movePlot.getX_INLINE(), movePlot.getY_INLINE()),
											.pillagePlotI = map.plotNumINLINE(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()),
											});
									}
								}
							}
						}
					}
				}
			}
		}

		{
			const Choice localChoice = bestChoice;
			if (localChoice.pillagePlotI >= 0)
			{
				pBestPlot = map.plotByIndexINLINE(localChoice.movePlotI);
				pBestPillagePlot = map.plotByIndexINLINE(localChoice.pillagePlotI);
			}
		}
	}
	else
#endif
	{
		int iI;
		CvPlot* pLoopPlot;
		int iPathTurns;
		int iValue;
		int iBestValue;
		iBestValue = 0;
		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (AI_plotValid(pLoopPlot) && !(pLoopPlot->isBarbarian()))
			{
				if (potentialWarAction(pLoopPlot))
				{
					CvCity * pWorkingCity = pLoopPlot->getWorkingCity();

					if (pWorkingCity != nullptr)
					{
						if (!(pWorkingCity == area()->getTargetCity(getOwnerINLINE())) && canPillage(pLoopPlot))
						{
							if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							{
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup(), 1) == 0)
								{
									if (generatePath(pLoopPlot, 0, true, &iPathTurns))
									{
										iValue = AI_pillageValue(pLoopPlot, iBonusValueThreshold);

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
											pBestPlot = getPathEndTurnPlot();
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
				getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_canPillage(CvPlot& kPlot) const
{
	if (isEnemy(kPlot.getTeam(), &kPlot))
	{
		return true;
	}

	if (!kPlot.isOwned())
	{
		bool bPillageUnowned = true;

		for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS && bPillageUnowned; ++iPlayer)
		{
			int iIndx;
			CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
			if (!isEnemy(kLoopPlayer.getTeam(), &kPlot))
			{
				for (CvCity* pCity = kLoopPlayer.firstCity(&iIndx); nullptr != pCity; pCity = kLoopPlayer.nextCity(&iIndx))
				{
#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
					if (kPlot.getPlotGroup((PlayerTypes)iPlayer) == pCity->plot()->getPlotGroup((PlayerTypes)iPlayer))
#else
					if (kPlot.isSamePlotGroup((PlayerTypes)iPlayer, pCity->plot()))
#endif
					{
						bPillageUnowned = false;
						break;
					}

				}
			}
		}

		if (bPillageUnowned)
		{
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_pillageRange(int iRange, int iBonusValueThreshold)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestPillagePlot;
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
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot) && !(pLoopPlot->isBarbarian()))
				{
					if (potentialWarAction(pLoopPlot))
					{
                        CvCity * pWorkingCity = pLoopPlot->getWorkingCity();

                        if (pWorkingCity != nullptr)
                        {
                            if (!(pWorkingCity == area()->getTargetCity(getOwnerINLINE())) && canPillage(pLoopPlot))
                            {
                                if (!(pLoopPlot->isVisibleEnemyUnit(this)))
                                {
                                    if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_PILLAGE, getGroup()) == 0)
                                    {
                                        if (generatePath(pLoopPlot, 0, true, &iPathTurns))
                                        {
                                            if (getPathLastNode()->m_iData1 == 0)
                                            {
                                                iPathTurns++;
                                            }

                                            if (iPathTurns <= iRange)
                                            {
                                                iValue = AI_pillageValue(pLoopPlot, iBonusValueThreshold);

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
                                                    pBestPlot = getPathEndTurnPlot();
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
				getGroup()->pushMission(MISSION_PILLAGE, -1, -1, 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_PILLAGE, pBestPillagePlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_found()
{
	PROFILE_FUNC();
//
//	CvPlot* pLoopPlot;
//	CvPlot* pBestPlot;
//	CvPlot* pBestFoundPlot;
//	int iPathTurns;
//	int iValue;
//	int iBestValue;
//	int iI;
//
//	iBestValue = 0;
//	pBestPlot = nullptr;
//	pBestFoundPlot = nullptr;
//
//	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
//	{
//		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);
//
//		if (AI_plotValid(pLoopPlot) && (pLoopPlot != plot() || GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopPlot, 1) <= pLoopPlot->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE())))
//		{
//			if (canFound(pLoopPlot))
//			{
//				iValue = pLoopPlot->getFoundValue(getOwnerINLINE());
//
//				if (iValue > 0)
//				{
//					if (!(pLoopPlot->isVisibleEnemyUnit(this)))
//					{
//						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_FOUND, getGroup(), 3) == 0)
//						{
//							if (generatePath(pLoopPlot, MOVE_SAFE_TERRITORY, true, &iPathTurns))
//							{
//								iValue *= 1000;
//
//								iValue /= (iPathTurns + 1);
//
//								if (iValue > iBestValue)
//								{
//									iBestValue = iValue;
//									pBestPlot = getPathEndTurnPlot();
//									pBestFoundPlot = pLoopPlot;
//								}
//							}
//						}
//					}
//				}
//			}
//		}
//	}

	int iPathTurns;
	int iValue;
	int iBestFoundValue = 0;
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestFoundPlot = nullptr;

	for (int iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++)
	{
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (pCitySitePlot->getArea() == getArea())
		{
			if (canFound(pCitySitePlot))
			{
				if (!(pCitySitePlot->isVisibleEnemyUnit(this)))
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup()) == 0)
					{
						if (getGroup()->canDefend() || GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_GUARD_CITY) > 0)
						{
							if (generatePath(pCitySitePlot, MOVE_SAFE_TERRITORY, true, &iPathTurns))
							{
								iValue = pCitySitePlot->getCitySiteFoundValue(getOwnerINLINE());
								iValue *= 1000;
								iValue /= (iPathTurns + 1);
								if (iValue > iBestFoundValue)
								{
									iBestFoundValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestFoundPlot = pCitySitePlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestFoundPlot != nullptr))
	{
		if (atPlot(pBestFoundPlot))
		{
			getGroup()->pushMission(MISSION_FOUND, -1, -1, 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_foundRange(int iRange, bool bFollow)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestFoundPlot;
	int iSearchRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	iSearchRange = AI_searchRange(iRange);

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestFoundPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot) && (pLoopPlot != plot() || GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopPlot, 1) <= pLoopPlot->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE())))
				{
					if (canFound(pLoopPlot))
					{
//#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
						iValue = pLoopPlot->getFoundValue(getOwnerINLINE());
//#else
//						iValue = pLoopPlot->grabLazyFoundValue(getOwnerINLINE());
//#endif

						if (iValue > iBestValue)
						{
							if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							{
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_FOUND, getGroup(), 3) == 0)
								{
									if (generatePath(pLoopPlot, MOVE_SAFE_TERRITORY, true, &iPathTurns))
									{
										if (iPathTurns <= iRange)
										{
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestFoundPlot = pLoopPlot;
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

	if ((pBestPlot != nullptr) && (pBestFoundPlot != nullptr))
	{
		if (atPlot(pBestFoundPlot))
		{
			getGroup()->pushMission(MISSION_FOUND, -1, -1, 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
		else if (!bFollow)
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_assaultSeaTransport(bool bBarbarian)
{
	PROFILE_FUNC();

	bool bIsAttackCity = (getUnitAICargo(UNITAI_ATTACK_CITY) > 0);
	
	FAssert(getGroup()->hasCargo());
	//FAssert(bIsAttackCity || getGroup()->getUnitAICargo(UNITAI_ATTACK) > 0);

	if (!canCargoAllMove())
	{
		return false;
	}

	std::vector<CvUnit*> aGroupCargo;
	CLLNode<IDInfo>* pUnitNode = plot()->headUnitNode();
	while (pUnitNode != nullptr)
	{
		CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = plot()->nextUnitNode(pUnitNode);
		CvUnit* pTransport = pLoopUnit->getTransportUnit();
		if (pTransport != nullptr && pTransport->getGroup() == getGroup())
		{
			aGroupCargo.push_back(pLoopUnit);
		}
	}

	int iCargo = getGroup()->getCargo();
	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestAssaultPlot = nullptr;

	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCoastalLand())
		{
			if (pLoopPlot->isOwned())
			{
				if (((bBarbarian || !pLoopPlot->isBarbarian())) || GET_PLAYER(getOwnerINLINE()).isMinorCiv())
				{
					if (isPotentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
					{
						int iTargetCities = pLoopPlot->area()->getCitiesPerPlayer(pLoopPlot->getOwnerINLINE());
						if (iTargetCities > 0)
						{
							bool bCanCargoAllUnload = true;
							int iVisibleEnemyDefenders = pLoopPlot->getNumVisibleEnemyDefenders(this);
							if (iVisibleEnemyDefenders > 0)
							{
								for (unsigned int i = 0; i < aGroupCargo.size(); ++i)
								{
									CvUnit* pAttacker = aGroupCargo[i];
									CvUnit* pDefender = pLoopPlot->getBestDefender(NO_PLAYER, pAttacker->getOwnerINLINE(), pAttacker, true);
									if (pDefender == nullptr || !pAttacker->canAttack(*pDefender))
									{
										bCanCargoAllUnload = false;
										break;
									}
								}
							}

							if (bCanCargoAllUnload)
							{
								int iPathTurns;
								if (generatePath(pLoopPlot, 0, true, &iPathTurns))
								{
									int iValue = 1;

									if (!bIsAttackCity)
									{
										iValue += (AI_pillageValue(pLoopPlot, 15) * 10);
									}

									int iAssaultsHere = GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ASSAULT, getGroup());

									iValue += (iAssaultsHere * 100);

									CvCity* pCity = pLoopPlot->getPlotCity();

									if (pCity == nullptr)
									{
										for (int iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
										{
											CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iJ));

											if (pAdjacentPlot != nullptr)
											{
												pCity = pAdjacentPlot->getPlotCity();

												if (pCity != nullptr)
												{
													if (pCity->getOwnerINLINE() == pLoopPlot->getOwnerINLINE())
													{
														break;
													}
													else
													{
														pCity = nullptr;
													}
												}
											}
										}
									}

									if (pCity != nullptr)
									{
										FAssert(isPotentialEnemy(pCity->getTeam(), pLoopPlot));

										if (!(pLoopPlot->isRiverCrossing(directionXY(pLoopPlot, pCity->plot()))))
										{
											iValue += (50 * -(GC.getRIVER_ATTACK_MODIFIER()));
										}

										iValue += 15 * (pLoopPlot->defenseModifier(getTeam(), false));
										iValue += 1000;
										iValue += (GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pCity->plot()) * 200);

										if (iPathTurns == 1)
										{
											iValue += GC.getGameINLINE().getSorenRandNum(50, "AI Assault");
										}
									}

									FAssert(iPathTurns > 0);

									if (iPathTurns == 1)
									{
										if (pCity != nullptr)
										{
											if (pCity->area()->getNumCities() > 1)
											{
												iValue *= 2;
											}
										}
									}

									iValue *= 1000;

									if (iTargetCities <= iAssaultsHere)
									{
										iValue /= 2;
									}

									if (iTargetCities == 1)
									{
										if (iCargo > 7)
										{
											iValue *= 3;
											iValue /= iCargo - 4;
										}
									}

									if (pLoopPlot->isCity())
									{
										if (iVisibleEnemyDefenders * 3 > iCargo)
										{
											iValue /= 10;
										}
										else
										{
											iValue *= iCargo;
											iValue /= std::max(1, (iVisibleEnemyDefenders * 3));
										}
									}
									else
									{
										if (0 == iVisibleEnemyDefenders)
										{
											iValue *= 4;
											iValue /= 3;
										}
										else
										{
											iValue /= iVisibleEnemyDefenders;
										}
									}

									// if more than 3 turns to get there, then put some randomness into our preference of distance
									// +/- 33%
									if (iPathTurns > 3)
									{
										int iPathAdjustment = GC.getGameINLINE().getSorenRandNum(67, "AI Assault Target");

										iPathTurns *= 66 + iPathAdjustment;
										iPathTurns /= 100;
									}

									iValue /= (iPathTurns + 1);

									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = getPathEndTurnPlot();
										pBestAssaultPlot = pLoopPlot;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestAssaultPlot != nullptr))
	{
		FAssert(!(pBestPlot->isImpassable()));

		if ((pBestPlot == pBestAssaultPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestAssaultPlot->getX_INLINE(), pBestAssaultPlot->getY_INLINE()) == 1))
		{
			if (atPlot(pBestAssaultPlot))
			{
				getGroup()->unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestAssaultPlot->getX_INLINE(), pBestAssaultPlot->getY_INLINE(), 0, false, false, MISSIONAI_ASSAULT, pBestAssaultPlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ASSAULT, pBestAssaultPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_settlerSeaTransport()
{
	PROFILE_FUNC();

	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	CvPlot* pLoopPlot;
	CvPlot* pPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestFoundPlot;
	[[maybe_unused]] CvArea* pWaterArea;
	bool bValid;
	int iValue;
	int iBestValue;
	int iI;

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_SETTLE) > 0);

	if (!canCargoAllMove())
	{
		return false;
	}

	//New logic should allow some new tricks like 
	//unloading settlers when a better site opens up locally
	//and delivering settlers
	//to inland sites

	pWaterArea = plot()->waterArea();
	FAssertMsg(pWaterArea != nullptr, "Ship out of water?");

	[[maybe_unused]] CvUnit* pSettlerUnit = nullptr;
	pPlot = plot();
	pUnitNode = pPlot->headUnitNode();

	while (pUnitNode != nullptr)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTransportUnit() == this)
		{
			if (pLoopUnit->AI_getUnitAIType() == UNITAI_SETTLE)
			{
				pSettlerUnit = pLoopUnit;
				break;
			}
		}
	}
	
	FAssert(pSettlerUnit != nullptr);
	
	int iAreaBestFoundValue = 0;
	[[maybe_unused]] CvPlot* pAreaBestPlot = nullptr;

	int iOtherAreaBestFoundValue = 0;
	[[maybe_unused]] CvPlot* pOtherAreaBestPlot = nullptr;

	for (iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++)
	{
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup()) == 0)
		{
			iValue = pCitySitePlot->getCitySiteFoundValue(getOwnerINLINE());
			if (pCitySitePlot->getArea() == getArea())
			{
				if (iValue > iAreaBestFoundValue)
				{
					iAreaBestFoundValue = iValue;
					pAreaBestPlot = pCitySitePlot;
				}
			}
			else
			{
				if (iValue > iOtherAreaBestFoundValue)
				{
					iOtherAreaBestFoundValue = iValue;
					pOtherAreaBestPlot = pCitySitePlot;
				}
			}
		}
	}
	if ((0 == iAreaBestFoundValue) && (0 == iOtherAreaBestFoundValue))
	{
		return false;
	}
	
	if (iAreaBestFoundValue > iOtherAreaBestFoundValue)
	{
		//let the settler walk.
		unloadAll();
		return true;
	}
	
	iBestValue = 0;
	pBestPlot = nullptr;
	pBestFoundPlot = nullptr;

	for (iI = 0; iI < GET_PLAYER(getOwnerINLINE()).AI_getNumCitySites(); iI++)
	{
		CvPlot* pCitySitePlot = GET_PLAYER(getOwnerINLINE()).AI_getCitySite(iI);
		if (!(pCitySitePlot->isVisibleEnemyUnit(this)))
		{
			if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCitySitePlot, MISSIONAI_FOUND, getGroup(), 4) == 0)
			{
				int iPathTurns;
				if (generatePath(pCitySitePlot, 0, true, &iPathTurns))
				{
					iValue = pCitySitePlot->getCitySiteFoundValue(getOwnerINLINE());
					iValue *= 1000;
					iValue /= (2 + iPathTurns);
					
					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestPlot = getPathEndTurnPlot();
						pBestFoundPlot = pCitySitePlot;
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestFoundPlot != nullptr))
	{
		FAssert(!(pBestPlot->isImpassable()));

		if ((pBestPlot == pBestFoundPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE()) == 1))
		{
			if (atPlot(pBestFoundPlot))
			{
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}

	//Try original logic
	//(sometimes new logic breaks)
	pPlot = plot();

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestFoundPlot = nullptr;
	
	int iMinFoundValue = GET_PLAYER(getOwnerINLINE()).AI_getMinFoundValue();

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCoastalLand())
		{
//#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
			iValue = pLoopPlot->getFoundValue(getOwnerINLINE());
//#else
//			iValue = pLoopPlot->grabLazyFoundValue(getOwnerINLINE());
//#endif

			if ((iValue > iBestValue) && (iValue >= iMinFoundValue))
			{
				bValid = false;

				pUnitNode = pPlot->headUnitNode();

				while (pUnitNode != nullptr)
				{
					pLoopUnit = ::getUnit(pUnitNode->m_data);
					pUnitNode = pPlot->nextUnitNode(pUnitNode);

					if (pLoopUnit->getTransportUnit() == this)
					{
						if (pLoopUnit->canFound(pLoopPlot))
						{
							bValid = true;
							break;
						}
					}
				}

				if (bValid)
				{
					if (!(pLoopPlot->isVisibleEnemyUnit(this)))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_FOUND, getGroup(), 4) == 0)
						{
							if (generatePath(pLoopPlot, 0, true))
							{
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestFoundPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}
	
	if ((pBestPlot != nullptr) && (pBestFoundPlot != nullptr))
	{
		FAssert(!(pBestPlot->isImpassable()));

		if ((pBestPlot == pBestFoundPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE()) == 1))
		{
			if (atPlot(pBestFoundPlot))
			{
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestFoundPlot->getX_INLINE(), pBestFoundPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestFoundPlot);
			return true;
		}
	}
	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_settlerSeaFerry()
{
	PROFILE_FUNC();

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_WORKER) > 0);

	if (!canCargoAllMove())
	{
		return false;
	}

	[[maybe_unused]] CvArea* pWaterArea = plot()->waterArea();
	FAssertMsg(pWaterArea != nullptr, "Ship out of water?");

	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;
	
	CvCity* pLoopCity;
	int iLoop;
	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		int iValue = pLoopCity->AI_getWorkersNeeded();
		if (iValue > 0)
		{
			iValue -= GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_FOUND, getGroup());
			if (iValue > 0)
			{
				int iPathTurns;
				if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
				{
					iValue += std::max(0, (GET_PLAYER(getOwnerINLINE()).AI_neededWorkers(pLoopCity->area()) - GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_WORKER)));
					iValue *= 1000;
					iValue /= 4 + iPathTurns;
					if (atPlot(pLoopCity->plot()))
					{
						iValue += 100;
					}
					else
					{
						iValue += GC.getGame().getSorenRandNum(100, "AI settler sea ferry");
					}
					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestPlot = pLoopCity->plot();							
					}
				}
			}
		}
	}
	
	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_FOUND, pBestPlot);
			return true;
		}
	}
	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_specialSeaTransportMissionary()
{
	PROFILE_FUNC();

	CLLNode<IDInfo>* pUnitNode;
	CvCity* pCity;
	CvUnit* pMissionaryUnit;
	CvUnit* pLoopUnit;
	CvPlot* pLoopPlot;
	CvPlot* pPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestSpreadPlot;
	int iPathTurns;
	int iValue;
	int iCorpValue;
	int iBestValue;
	int iI, iJ;
	bool bExecutive = false;

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_MISSIONARY) > 0);

	if (!canCargoAllMove())
	{
		return false;
	}

	pPlot = plot();

	pMissionaryUnit = nullptr;

	pUnitNode = pPlot->headUnitNode();

	while (pUnitNode != nullptr)
	{
		pLoopUnit = ::getUnit(pUnitNode->m_data);
		pUnitNode = pPlot->nextUnitNode(pUnitNode);

		if (pLoopUnit->getTransportUnit() == this)
		{
			if (pLoopUnit->AI_getUnitAIType() == UNITAI_MISSIONARY)
			{
				pMissionaryUnit = pLoopUnit;
				break;
			}
		}
	}

	if (pMissionaryUnit == nullptr)
	{
		return false;
	}

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestSpreadPlot = nullptr;

	// XXX what about non-coastal cities?
	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCoastalLand())
		{
			pCity = pLoopPlot->getPlotCity();

			if (pCity != nullptr)
			{
				iValue = 0;
				iCorpValue = 0;

				for (iJ = 0; iJ < GC.getNumReligionInfos(); iJ++)
				{
					if (pMissionaryUnit->canSpread(pLoopPlot, ((ReligionTypes)iJ)))
					{
						if (GET_PLAYER(getOwnerINLINE()).getStateReligion() == ((ReligionTypes)iJ))
						{
							iValue += 3;
						}

						if (GET_PLAYER(getOwnerINLINE()).hasHolyCity((ReligionTypes)iJ))
						{
							iValue++;
						}
					}
				}

				for (iJ = 0; iJ < GC.getNumCorporationInfos(); iJ++)
				{
					if (pMissionaryUnit->canSpreadCorporation(pLoopPlot, ((CorporationTypes)iJ)))
					{
						if (GET_PLAYER(getOwnerINLINE()).hasHeadquarters((CorporationTypes)iJ))
						{
							iCorpValue += 3;
						}
					}
				}

				if (iValue > 0)
				{
					if (!(pLoopPlot->isVisibleEnemyUnit(this)))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_SPREAD, getGroup()) == 0)
						{
							if (generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								iValue *= pCity->getPopulation();

								if (pCity->getOwnerINLINE() == getOwnerINLINE())
								{
									iValue *= 4;
								}
								else if (pCity->getTeam() == getTeam())
								{
									iValue *= 3;
								}

								if (pCity->getReligionCount() == 0)
								{
									iValue *= 2;
								}

								iValue /= (pCity->getReligionCount() + 1);

								FAssert(iPathTurns > 0);

								if (iPathTurns == 1)
								{
									iValue *= 2;
								}

								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestSpreadPlot = pLoopPlot;
									bExecutive = false;
								}
							}
						}
					}
				}

				if (iCorpValue > 0)
				{
					if (!(pLoopPlot->isVisibleEnemyUnit(this)))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_SPREAD_CORPORATION, getGroup()) == 0)
						{
							if (generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								iCorpValue *= pCity->getPopulation();

								FAssert(iPathTurns > 0);

								if (iPathTurns == 1)
								{
									iValue *= 2;
								}

								iCorpValue *= 1000;

								iCorpValue /= (iPathTurns + 1);

								if (iCorpValue > iBestValue)
								{
									iBestValue = iCorpValue;
									pBestPlot = getPathEndTurnPlot();
									pBestSpreadPlot = pLoopPlot;
									bExecutive = true;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestSpreadPlot != nullptr))
	{
		FAssert(!(pBestPlot->isImpassable()) || canMoveImpassable());

		if ((pBestPlot == pBestSpreadPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestSpreadPlot->getX_INLINE(), pBestSpreadPlot->getY_INLINE()) == 1))
		{
			if (atPlot(pBestSpreadPlot))
			{
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestSpreadPlot->getX_INLINE(), pBestSpreadPlot->getY_INLINE(), 0, false, false, bExecutive ? MISSIONAI_SPREAD_CORPORATION : MISSIONAI_SPREAD, pBestSpreadPlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, bExecutive ? MISSIONAI_SPREAD_CORPORATION : MISSIONAI_SPREAD, pBestSpreadPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_specialSeaTransportSpy()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	CvPlot* pBestSpyPlot;
	PlayerTypes eBestPlayer;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iI;

	FAssert(getCargo() > 0);
	FAssert(getUnitAICargo(UNITAI_SPY) > 0);

	if (!canCargoAllMove())
	{
		return false;
	}

	iBestValue = 0;
	eBestPlayer = NO_PLAYER;

	for (iI = 0; iI < MAX_CIV_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_getAttitude((PlayerTypes)iI) <= ATTITUDE_ANNOYED)
				{
					iValue = GET_PLAYER((PlayerTypes)iI).getTotalPopulation();

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						eBestPlayer = ((PlayerTypes)iI);
					}
				}
			}
		}
	}

	if (eBestPlayer == NO_PLAYER)
	{
		return false;
	}

	pBestPlot = nullptr;
	pBestSpyPlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCoastalLand())
		{
			if (pLoopPlot->getOwnerINLINE() == eBestPlayer)
			{
				iValue = pLoopPlot->area()->getCitiesPerPlayer(eBestPlayer);

				if (iValue > 0)
				{
					if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ATTACK_SPY, getGroup(), 4) == 0)
					{
						if (generatePath(pLoopPlot, 0, true, &iPathTurns))
						{
							iValue *= 1000;

							iValue /= (iPathTurns + 1);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = getPathEndTurnPlot();
								pBestSpyPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestSpyPlot != nullptr))
	{
		FAssert(!(pBestPlot->isImpassable()));

		if ((pBestPlot == pBestSpyPlot) || (stepDistance(pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), pBestSpyPlot->getX_INLINE(), pBestSpyPlot->getY_INLINE()) == 1))
		{
			if (atPlot(pBestSpyPlot))
			{
				unloadAll(); // XXX is this dangerous (not pushing a mission...) XXX air units?
				return true;
			}
			else
			{
				getGroup()->pushMission(MISSION_MOVE_TO, pBestSpyPlot->getX_INLINE(), pBestSpyPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestSpyPlot);
				return true;
			}
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_ATTACK_SPY, pBestSpyPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_carrierSeaTransport()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pLoopPlotAir;
	CvPlot* pBestPlot;
	CvPlot* pBestCarrierPlot;
	int iMaxAirRange;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iDX, iDY;
	int iI;

	// XXX maybe protect land troops?

	iMaxAirRange = 0;

	std::vector<CvUnit*> aCargoUnits;
	getCargoUnits(aCargoUnits);
	for (unsigned int i = 0; i < aCargoUnits.size(); ++i)
	{
		iMaxAirRange = std::max(iMaxAirRange, aCargoUnits[i]->airRange());
	}

	if (iMaxAirRange == 0)
	{
		return false;
	}

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestCarrierPlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->isAdjacentToLand())
			{
				iValue = 0;

				for (iDX = -(iMaxAirRange); iDX <= iMaxAirRange; iDX++)
				{
					for (iDY = -(iMaxAirRange); iDY <= iMaxAirRange; iDY++)
					{
						pLoopPlotAir = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iDX, iDY);

						if (pLoopPlotAir != nullptr)
						{
							if (plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pLoopPlotAir->getX_INLINE(), pLoopPlotAir->getY_INLINE()) <= iMaxAirRange)
							{
								if (!(pLoopPlotAir->isBarbarian()))
								{
									if (potentialWarAction(pLoopPlotAir))
									{
										if (pLoopPlotAir->isCity())
										{
											iValue++;
										}

										if (pLoopPlotAir->getImprovementType() != NO_IMPROVEMENT)
										{
											iValue ++;
										}
									}
								}
							}
						}
					}
				}

				if (iValue > 0)
				{
					if (!(pLoopPlot->isVisibleEnemyUnit(this)))
					{
						bool bStealth = (getInvisibleType() != NO_INVISIBLE);
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_CARRIER, getGroup(), bStealth ? 5 : 3) <= (bStealth ? 0 : 3))
						{
							if (generatePath(pLoopPlot, 0, true, &iPathTurns))
							{
								iValue *= 1000;
								
								for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++)
								{
									CvPlot* pDirectionPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), (DirectionTypes)iDirection);
									if (pDirectionPlot != nullptr)
									{
										if (pDirectionPlot->isCity() && isEnemy(pDirectionPlot->getTeam(), pLoopPlot))
										{
											iValue /= 2;
											break;
										}
									}
								}

								iValue += (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ASSAULT, getGroup(), 2) * 2000);

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = getPathEndTurnPlot();
									pBestCarrierPlot = pLoopPlot;
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestCarrierPlot != nullptr))
	{
		if (atPlot(pBestCarrierPlot))
		{
			if (getGroup()->hasCargo())
			{
				CvPlot* pPlot = plot();

				int iNumUnits = pPlot->getNumUnits();

				for (int i = 0; i < iNumUnits; ++i)
				{
					bool bDone = true;
					CLLNode<IDInfo>* pUnitNode = pPlot->headUnitNode();
					while (pUnitNode != nullptr)
					{
						CvUnit* pCargoUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pPlot->nextUnitNode(pUnitNode);
					
						if (pCargoUnit->isCargo())
						{
							FAssert(pCargoUnit->getTransportUnit() != nullptr);
							if (pCargoUnit->getOwnerINLINE() == getOwnerINLINE() && (pCargoUnit->getTransportUnit()->getGroup() == getGroup()) && (pCargoUnit->getDomainType() == DOMAIN_AIR))
							{
								if (pCargoUnit->canMove() && pCargoUnit->isGroupHead())
								{
									// careful, this might kill the cargo group
									if (pCargoUnit->getGroup()->AI_update())
									{
										bDone = false;
										break;
									}
								}
							}
						}
					}

					if (bDone)
					{
						break;
					}
				}
			}

			if (canPlunder(pBestCarrierPlot))
			{
				getGroup()->pushMission(MISSION_PLUNDER, -1, -1, 0, false, false, MISSIONAI_CARRIER, pBestCarrierPlot);
			}
			else
			{
				getGroup()->pushMission(MISSION_SKIP);
			}
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_CARRIER, pBestCarrierPlot);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectPlot(CvPlot* pPlot, int iRange)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	int iLoop;

	FAssert(canBuildRoute());

	if (!(pPlot->isVisibleEnemyUnit(this)))
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pPlot, MISSIONAI_BUILD, getGroup(), iRange) == 0)
		{
			if (generatePath(pPlot, MOVE_SAFE_TERRITORY, true))
			{
				for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
				{
					if (!(pPlot->isConnectedTo(pLoopCity)))
					{
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
						if (plot()->getPlotGroup(getOwnerINLINE()) == pLoopCity->plot()->getPlotGroup(getOwnerINLINE()))
#else
						if (plot()->isSamePlotGroup(getOwnerINLINE(), pLoopCity->plot()))
#endif
						{
							getGroup()->pushMission(MISSION_ROUTE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
							return true;
						}
					}
				}

				for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
				{
					if (!(pPlot->isConnectedTo(pLoopCity)))
					{
						FAssertMsg(pPlot->getPlotCity() != pLoopCity, "pPlot->getPlotCity() is not expected to be equal with pLoopCity");

						if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
						{
							if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true))
							{
								if (atPlot(pPlot)) // need to test before moving...
								{
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
								}
								else
								{
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
									getGroup()->pushMission(MISSION_ROUTE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
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


// Returns true if a mission was pushed...
bool CvUnitAI::AI_improveCity(CvCity* pCity)
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	BuildTypes eBestBuild;
	MissionTypes eMission;

	if (AI_bestCityBuild(pCity, &pBestPlot, &eBestBuild, nullptr, this))
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
			if (nullptr != pBestPlot && generatePath(pBestPlot) && (getPathLastNode()->m_iData2 == 1) && (getPathLastNode()->m_iData1 == 0))
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
		
		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(eMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

		return true;
	}

	return false;
}

bool CvUnitAI::AI_improveLocalPlot(int iRange, CvCity* pIgnoreCity)
{
	
	int iX, iY;
	
	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;
	BuildTypes eBestBuild = NO_BUILD;
	
	for (iX = -iRange; iX <= iRange; iX++)
	{
		for (iY = -iRange; iY <= iRange; iY++)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			if ((pLoopPlot != nullptr) && (pLoopPlot->isCityRadius()))
			{
				CvCity* pCity = pLoopPlot->getWorkingCity();
				if ((nullptr != pCity) && (pCity->getOwnerINLINE() == getOwnerINLINE()))
				{
					if ((nullptr == pIgnoreCity) || (pCity != pIgnoreCity))
					{
						if (AI_plotValid(pLoopPlot))
						{
							int iIndex = pCity->getCityPlotIndex(pLoopPlot);
							if (iIndex != CITY_HOME_PLOT)
							{
								if (((nullptr == pIgnoreCity) || ((pCity->AI_getWorkersNeeded() > 0) && (pCity->AI_getWorkersHave() < (1 + pCity->AI_getWorkersNeeded() * 2 / 3)))) && (pCity->AI_getBestBuild(iIndex) != NO_BUILD))
								{
									if (canBuild(pLoopPlot, pCity->AI_getBestBuild(iIndex)))
									{
										bool bAllowed = true;

										if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION))
										{
											if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT && pLoopPlot->getImprovementType() != GC.getDefineINT("RUINS_IMPROVEMENT"))
											{
												bAllowed = false;
											}
										}

										if (bAllowed)
										{
											if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT && GC.getBuildInfo(pCity->AI_getBestBuild(iIndex)).getImprovement() != NO_IMPROVEMENT)
											{
												bAllowed = false;
											}
										}

										if (bAllowed)
										{
											int iValue = pCity->AI_getBestBuildValue(iIndex);
											int iPathTurns;
											if (generatePath(pLoopPlot, 0, true, &iPathTurns))
											{
												int iMaxWorkers = 1;
												if (plot() == pLoopPlot)
												{
													iValue *= 3;
													iValue /= 2;
												}
												else if (getPathLastNode()->m_iData1 == 0)
												{
													iPathTurns++;
												}
												else if (iPathTurns <= 1)
												{
													iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, pCity->AI_getBestBuild(iIndex));											
												}

												if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers)
												{
													iValue *= 1000;
													iValue /= 1 + iPathTurns;

													if (iValue > iBestValue)
													{
														iBestValue = iValue;
														pBestPlot = pLoopPlot;
														eBestBuild = pCity->AI_getBestBuild(iIndex);											
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
	
	if (pBestPlot != nullptr)
	{
	    FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
	    FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		FAssert(pBestPlot->getWorkingCity() != nullptr);
		if (nullptr != pBestPlot->getWorkingCity())
		{
			pBestPlot->getWorkingCity()->AI_changeWorkersHave(+1);

			if (plot()->getWorkingCity() != nullptr)
			{
				plot()->getWorkingCity()->AI_changeWorkersHave(-1);
			}
		}
		MissionTypes eMission = MISSION_MOVE_TO;

		int iPathTurns;
		if (generatePath(pBestPlot, 0, true, &iPathTurns) && (getPathLastNode()->m_iData2 == 1) && (getPathLastNode()->m_iData1 == 0))
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
		
		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(eMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);
		return true;
	}
	
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_nextCityToImprove(CvCity* pCity)
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvPlot* pPlot;
	CvPlot* pBestPlot;
	BuildTypes eBuild;
	BuildTypes eBestBuild;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	eBestBuild = NO_BUILD;
	pBestPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (pLoopCity != pCity)
		{
			//iValue = pLoopCity->AI_totalBestBuildValue(area());
			int iWorkersNeeded = pLoopCity->AI_getWorkersNeeded();
			int iWorkersHave = pLoopCity->AI_getWorkersHave();
			
			iValue = std::max(0, iWorkersNeeded - iWorkersHave) * 100;
			iValue += iWorkersNeeded * 10;
			iValue *= (iWorkersNeeded + 1);
			iValue /= (iWorkersHave + 1);

			if (iValue > 0)
			{
				if (AI_bestCityBuild(pLoopCity, &pPlot, &eBuild, nullptr, this))
				{
					FAssert(pPlot != nullptr);
					FAssert(eBuild != NO_BUILD);

					iValue *= 1000;

					if (pLoopCity->isCapital())
					{
					    iValue *= 2;
					}

					generatePath(pPlot, 0, true, &iPathTurns);
					iValue /= (iPathTurns + 1);

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						eBestBuild = eBuild;
						pBestPlot = pPlot;
						FAssert(!atPlot(pBestPlot) || nullptr == pCity || pCity->AI_getWorkersNeeded() == 0 || pCity->AI_getWorkersHave() > pCity->AI_getWorkersNeeded() + 1);
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
	    FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
	    FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");
	    if (plot()->getWorkingCity() != nullptr)
	    {
			plot()->getWorkingCity()->AI_changeWorkersHave(-1);
	    }

		FAssert(pBestPlot->getWorkingCity() != nullptr || GC.getBuildInfo(eBestBuild).getImprovement() == NO_IMPROVEMENT);
		if (nullptr != pBestPlot->getWorkingCity())
		{
			pBestPlot->getWorkingCity()->AI_changeWorkersHave(+1);
		}
		
		eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_nextCityToImproveAirlift()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	if (getGroup()->getNumUnits() > 1)
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

	iBestValue = 0;
	pBestPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (pLoopCity != pCity)
		{
			if (canAirliftAt(pCity->plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
			{
				iValue = pLoopCity->AI_totalBestBuildValue(pLoopCity->area());

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					pBestPlot = pLoopCity->plot();
					FAssert(pLoopCity != pCity);
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_AIRLIFT, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_irrigateTerritory()
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	ImprovementTypes eImprovement;
	BuildTypes eBuild;
	BuildTypes eBestBuild;
	BuildTypes eBestTempBuild;
	BonusTypes eNonObsoleteBonus;
	bool bValid;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iBestTempBuildValue;
	int iI, iJ;

	iBestValue = 0;
	eBestBuild = NO_BUILD;
	pBestPlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				if (pLoopPlot->getWorkingCity() == nullptr)
				{
					eImprovement = pLoopPlot->getImprovementType();

					if ((eImprovement == NO_IMPROVEMENT) || !(GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION) && !(eImprovement == (GC.getDefineINT("RUINS_IMPROVEMENT")))))
					{
						if ((eImprovement == NO_IMPROVEMENT) || !(GC.getImprovementInfo(eImprovement).isCarriesIrrigation()))
						{
							eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

							if ((eImprovement == NO_IMPROVEMENT) || (eNonObsoleteBonus == NO_BONUS) || !(GC.getImprovementInfo(eImprovement).isImprovementBonusTrade(eNonObsoleteBonus)))
							{
								if (pLoopPlot->isIrrigationAvailable(true))
								{
									iBestTempBuildValue = INT_MAX;
									eBestTempBuild = NO_BUILD;

									for (iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
									{
										eBuild = ((BuildTypes)iJ);
										FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

										if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
										{
											if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).isCarriesIrrigation())
											{
												if (canBuild(pLoopPlot, eBuild))
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

									if (eBestTempBuild != NO_BUILD)
									{
										bValid = true;

										if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS))
										{
											if (pLoopPlot->getFeatureType() != NO_FEATURE)
											{
												if (GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()))
												{
													if (GC.getFeatureInfo(pLoopPlot->getFeatureType()).getYieldChange(YIELD_PRODUCTION) > 0)
													{
														bValid = false;
													}
												}
											}
										}

										if (bValid)
										{
											if (!(pLoopPlot->isVisibleEnemyUnit(this)))
											{
												if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 1) == 0)
												{
													if (generatePath(pLoopPlot, 0, true, &iPathTurns)) // XXX should this actually be at the top of the loop? (with saved paths and all...)
													{
														iValue = 10000;

														iValue /= (iPathTurns + 1);

														if (iValue > iBestValue)
														{
															iBestValue = iValue;
															eBestBuild = eBestTempBuild;
															pBestPlot = pLoopPlot;
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

	if (pBestPlot != nullptr)
	{
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

		return true;
	}

	return false;
}

bool CvUnitAI::AI_fortTerritory(bool bCanal, bool bAirbase)
{
	int iBestValue = 0;
	BuildTypes eBestBuild = NO_BUILD;
	CvPlot* pBestPlot = nullptr;

	CvPlayerAI& kOwner = GET_PLAYER(getOwnerINLINE());
	for (int iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT)
				{
					int iValue = 0;
					iValue += bCanal ? kOwner.AI_getPlotCanalValue(pLoopPlot) : 0;
					iValue += bAirbase ? kOwner.AI_getPlotAirbaseValue(pLoopPlot) : 0;

					if (iValue > 0)
					{
						int iBestTempBuildValue = INT_MAX;
						BuildTypes eBestTempBuild = NO_BUILD;

						for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
						{
							BuildTypes eBuild = ((BuildTypes)iJ);
							FAssertMsg(eBuild < GC.getNumBuildInfos(), "Invalid Build");

							if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
							{
								if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).isActsAsCity())
								{
								    if (GC.getImprovementInfo((ImprovementTypes)(GC.getBuildInfo(eBuild).getImprovement())).getDefenseModifier() > 0)
								    {
                                        if (canBuild(pLoopPlot, eBuild))
                                        {
                                            iValue = 10000;

                                            iValue /= (GC.getBuildInfo(eBuild).getTime() + 1);

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

						if (eBestTempBuild != NO_BUILD)
						{
							if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							{
								bool bValid = true;

								if (GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS))
								{
									if (pLoopPlot->getFeatureType() != NO_FEATURE)
									{
										if (GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()))
										{
											if (GC.getFeatureInfo(pLoopPlot->getFeatureType()).getYieldChange(YIELD_PRODUCTION) > 0)
											{
												bValid = false;
											}
										}
									}
								}

								if (bValid)
								{
									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 3) == 0)
									{
										int iPathTurns;
										if (generatePath(pLoopPlot, 0, true, &iPathTurns))
										{
											iValue *= 1000;
											iValue /= (iPathTurns + 1);

											if (iValue > iBestValue)
											{
												iBestValue = iValue;
												eBestBuild = eBestTempBuild;
												pBestPlot = pLoopPlot;
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

	if (pBestPlot != nullptr)
	{
		FAssertMsg(eBestBuild != NO_BUILD, "BestBuild is not assigned a valid value");
		FAssertMsg(eBestBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");

		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
		getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

		return true;
	}
	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_improveBonus(int iMinValue, CvPlot** ppBestPlot, BuildTypes* peBestBuild, int* piBestValue)
{
	PROFILE_FUNC();

	CvPlot* pBestPlot;
	BuildTypes eBestBuild;
	int iBestValue;
	int iBestResourceValue;
	bool bBestBuildIsRoute = false;

	iBestValue = 0;
	iBestResourceValue = 0;
	eBestBuild = NO_BUILD;
	pBestPlot = nullptr;

	const bool bCanRoute = canBuildRoute();

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Parallel_CvUnitAI_AI_improveBonus])
	{
		// We'll do this with a parallel loop over the map to gather improvable plots, then sort them by estimated final value.
		struct PlotInfo
		{
			CvPlot* plot{};
			BuildTypes eBestBuild{}; // eBestBuild
			bool bDoImprove{};
			bool bBestBuildIsRoute{};
			int baseValue{}; // The value before path turns is factored in
			int valueUpperBound{};
		};
		std::vector<PlotInfo> plotList;
		std::mutex mutex;
		heck::parallelForEachN(GC.getMapINLINE().numPlotsINLINE(), [&plotList, &mutex, &map = GC.getMapINLINE(),
			ownerId = getOwnerINLINE(), teamId = getTeam(), domainType = getDomainType(), areaPtr = area(),
			bCanRoute, unitPlot = plot(), safeAutomation = GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION), this](int iI) {

				[[maybe_unused]] int iPathTurns{};
				int iValue{};

				CvPlot* const pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

				if (pLoopPlot->getOwnerINLINE() == ownerId && AI_plotValid(pLoopPlot))
				{
					bool bCanImprove = (pLoopPlot->area() == areaPtr);
					if (!bCanImprove)
					{
						if (DOMAIN_SEA == domainType && pLoopPlot->isWater() && unitPlot->isAdjacentToArea(pLoopPlot->area()))
						{
							bCanImprove = true;
						}
					}

					if (bCanImprove)
					{
						const BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(teamId);

						if (eNonObsoleteBonus != NO_BONUS)
						{
							const bool bIsConnected = pLoopPlot->isConnectedToCapital(ownerId);
							if ((pLoopPlot->getWorkingCity() != nullptr) || (bIsConnected || bCanRoute))
							{
								const ImprovementTypes eImprovement = pLoopPlot->getImprovementType();

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
								else if (!safeAutomation)
								{
									bDoImprove = true;
								}

								BuildTypes eBestTempBuild = NO_BUILD;

								if (bDoImprove)
								{
									int iBestTempBuildValue = INT_MAX;
									for (int iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
									{
										const BuildTypes eBuild = ((BuildTypes)iJ);

										if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
										{
											if (GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isImprovementBonusTrade(eNonObsoleteBonus) || (!pLoopPlot->isCityRadius() && GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isActsAsCity()))
											{
												if (canBuild(pLoopPlot, eBuild))
												{
													if ((pLoopPlot->getFeatureType() == NO_FEATURE) || !GC.getBuildInfo(eBuild).isFeatureRemove(pLoopPlot->getFeatureType()) || !GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS))
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
									iValue = GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus);

									if (bDoImprove)
									{
										const ImprovementTypes eImprovement2 = (ImprovementTypes)GC.getBuildInfo(eBestTempBuild).getImprovement();
										FAssert(eImprovement2 != NO_IMPROVEMENT);
										//iValue += (GC.getImprovementInfo((ImprovementTypes) GC.getBuildInfo(eBestTempBuild).getImprovement()))
										iValue += 5 * pLoopPlot->calculateImprovementYieldChange(eImprovement2, YIELD_FOOD, getOwnerINLINE(), false);
										iValue += 5 * pLoopPlot->calculateNatureYield(YIELD_FOOD, getTeam(), (pLoopPlot->getFeatureType() == NO_FEATURE) ? true : GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()));
									}

									iValue += std::max(0, 100 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

									if (GET_PLAYER(getOwnerINLINE()).getNumTradeableBonuses(eNonObsoleteBonus) == 0)
									{
										iValue *= 2;
									}

									int iMaxWorkers = 1;
									if ((eBestTempBuild != NO_BUILD) && (!GC.getBuildInfo(eBestTempBuild).isKill()))
									{
										//allow teaming.
										iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBestTempBuild);

										// For value upperbound, we assume this doesn't happen.
										//if (getPathLastNode()->m_iData1 == 0)
										//{
										//	iMaxWorkers = std::min((iMaxWorkers + 1) / 2, 1 + GET_PLAYER(getOwnerINLINE()).AI_baseBonusVal(eNonObsoleteBonus) / 20);
										//}
									}

									const int baseValue = iValue;

									if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers)
									{
										const std::optional<int> iPathTurnsLowerBound = gGlobals.enhancedDLLGameContext->guessPathTurnsOneBased(*getGroup(), *pLoopPlot, 0, INT32_MAX);
										if (!iPathTurnsLowerBound)
											return;

										if (!bDoImprove || pLoopPlot->getBuildTurnsLeft(eBestTempBuild, 0, 0) > (*iPathTurnsLowerBound * 2 - 1))
										{
											if (bDoImprove)
											{
												iValue *= 1000;

												if (atPlot(pLoopPlot))
												{
													iValue *= 3;
												}

												iValue /= (*iPathTurnsLowerBound + 1);

												if (pLoopPlot->isCityRadius())
												{
													iValue *= 2;
												}

												std::lock_guard lock(mutex);
												plotList.push_back({
													.plot = pLoopPlot,
													.eBestBuild = eBestTempBuild,
													.bDoImprove = bDoImprove,
													.bBestBuildIsRoute = false,
													.baseValue = baseValue,
													.valueUpperBound = iValue,
													});
											}
											else
											{
												FAssert(bCanRoute && !bIsConnected);
												const ImprovementTypes eImprovement2 = pLoopPlot->getImprovementType();
												if ((eImprovement2 != NO_IMPROVEMENT) && (GC.getImprovementInfo(eImprovement2).isImprovementBonusTrade(eNonObsoleteBonus)))
												{
													iValue *= 1000;
													iValue /= (*iPathTurnsLowerBound + 1);

													std::lock_guard lock(mutex);
													plotList.push_back({
														.plot = pLoopPlot,
														.eBestBuild = eBestTempBuild,
														.bDoImprove = bDoImprove,
														.bBestBuildIsRoute = false,
														.baseValue = baseValue,
														.valueUpperBound = iValue,
														});
												}
											}
										}
									}
								}
							}
						}
					}
				}
			});

		std::ranges::sort(plotList, std::greater<>(), [](const PlotInfo& x) {
			// Deterministic ordering.
			return std::tuple(x.valueUpperBound, x.plot->getY_INLINE(), x.plot->getX_INLINE());
			});

		for (const PlotInfo& plotInfo : plotList)
		{
			if (plotInfo.valueUpperBound <= iBestValue)
				break;

			CvPlot* const pLoopPlot = plotInfo.plot;
			int iPathTurns = 0;

			if (generatePath(pLoopPlot, 0, true, &iPathTurns))
			{
				const BonusTypes eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

				int iMaxWorkers = 1;
				if ((plotInfo.eBestBuild != NO_BUILD) && (!GC.getBuildInfo(plotInfo.eBestBuild).isKill()))
				{
					//allow teaming.
					iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, plotInfo.eBestBuild);

					if (getPathLastNode()->m_iData1 == 0)
					{
						iMaxWorkers = std::min((iMaxWorkers + 1) / 2, 1 + GET_PLAYER(getOwnerINLINE()).AI_baseBonusVal(eNonObsoleteBonus) / 20);
					}
				}

				if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers)
				{
					if (!plotInfo.bDoImprove || (pLoopPlot->getBuildTurnsLeft(plotInfo.eBestBuild, 0, 0) > (iPathTurns * 2 - 1)))
					{
						int iValue = plotInfo.baseValue;
						if (plotInfo.bDoImprove)
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
								eBestBuild = plotInfo.eBestBuild;
								pBestPlot = pLoopPlot;
								bBestBuildIsRoute = false;
								iBestResourceValue = iValue;
							}
						}
						else
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
	else
#endif
	{
		int iI{};
		for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
		{
			CvPlot* pLoopPlot;
			ImprovementTypes eImprovement;
			BuildTypes eBuild;
			BuildTypes eBestTempBuild;
			BonusTypes eNonObsoleteBonus;
			int iPathTurns;
			int iValue;
			int iBestTempBuildValue;
			int iJ;
			bool bIsConnected;

			pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE() && AI_plotValid(pLoopPlot))
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
						bIsConnected = pLoopPlot->isConnectedToCapital(getOwnerINLINE());
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
							else if (!GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_SAFE_AUTOMATION))
							{
								bDoImprove = true;
							}

							iBestTempBuildValue = INT_MAX;
							eBestTempBuild = NO_BUILD;

							if (bDoImprove)
							{
								for (iJ = 0; iJ < GC.getNumBuildInfos(); iJ++)
								{
									eBuild = ((BuildTypes)iJ);

									if (GC.getBuildInfo(eBuild).getImprovement() != NO_IMPROVEMENT)
									{
										if (GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isImprovementBonusTrade(eNonObsoleteBonus) || (!pLoopPlot->isCityRadius() && GC.getImprovementInfo((ImprovementTypes)GC.getBuildInfo(eBuild).getImprovement()).isActsAsCity()))
										{
											if (canBuild(pLoopPlot, eBuild))
											{
												if ((pLoopPlot->getFeatureType() == NO_FEATURE) || !GC.getBuildInfo(eBuild).isFeatureRemove(pLoopPlot->getFeatureType()) || !GET_PLAYER(getOwnerINLINE()).isOption(PLAYEROPTION_LEAVE_FORESTS))
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
								if (generatePath(pLoopPlot, 0, true, &iPathTurns))
								{
									iValue = GET_PLAYER(getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus);

									if (bDoImprove)
									{
										eImprovement = (ImprovementTypes)GC.getBuildInfo(eBestTempBuild).getImprovement();
										FAssert(eImprovement != NO_IMPROVEMENT);
										//iValue += (GC.getImprovementInfo((ImprovementTypes) GC.getBuildInfo(eBestTempBuild).getImprovement()))
										iValue += 5 * pLoopPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, getOwnerINLINE(), false);
										iValue += 5 * pLoopPlot->calculateNatureYield(YIELD_FOOD, getTeam(), (pLoopPlot->getFeatureType() == NO_FEATURE) ? true : GC.getBuildInfo(eBestTempBuild).isFeatureRemove(pLoopPlot->getFeatureType()));
									}

									iValue += std::max(0, 100 * GC.getBonusInfo(eNonObsoleteBonus).getAIObjective());

									if (GET_PLAYER(getOwnerINLINE()).getNumTradeableBonuses(eNonObsoleteBonus) == 0)
									{
										iValue *= 2;
									}

									int iMaxWorkers = 1;
									if ((eBestTempBuild != NO_BUILD) && (!GC.getBuildInfo(eBestTempBuild).isKill()))
									{
										//allow teaming.
										iMaxWorkers = AI_calculatePlotWorkersNeeded(pLoopPlot, eBestTempBuild);
										if (getPathLastNode()->m_iData1 == 0)
										{
											iMaxWorkers = std::min((iMaxWorkers + 1) / 2, 1 + GET_PLAYER(getOwnerINLINE()).AI_baseBonusVal(eNonObsoleteBonus) / 20);
										}
									}

									if ((GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup()) < iMaxWorkers)
										&& (!bDoImprove || (pLoopPlot->getBuildTurnsLeft(eBestTempBuild, 0, 0) > (iPathTurns * 2 - 1))))
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
				int iDistance = stepDistance(getX_INLINE(), getY_INLINE(), pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
				int iPathTurns;
				if (generatePath(pBestPlot, 0, false, &iPathTurns))
				{
					if (iPathTurns >= iDistance)
					{
						eBestMission = MISSION_ROUTE_TO;
					}
				}
			}

			eBestBuild = AI_betterPlotBuild(pBestPlot, eBestBuild);
			getGroup()->pushMission(eBestMission, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pBestPlot);
			getGroup()->pushMission(MISSION_BUILD, eBestBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBestPlot);

			return true;
		}
		else if (bBestBuildIsRoute)
		{
			if (AI_connectPlot(pBestPlot))
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

//returns true if a mission is pushed
//if eBuild is NO_BUILD, assumes a route is desired.
bool CvUnitAI::AI_improvePlot(CvPlot* pPlot, BuildTypes eBuild)
{
	FAssert(pPlot != nullptr);
	
	if (eBuild != NO_BUILD)
	{
		FAssertMsg(eBuild < GC.getNumBuildInfos(), "BestBuild is assigned a corrupt value");
		
		eBuild = AI_betterPlotBuild(pPlot, eBuild);
		if (!atPlot(pPlot))
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX_INLINE(), pPlot->getY_INLINE(), 0, false, false, MISSIONAI_BUILD, pPlot);
		}
		getGroup()->pushMission(MISSION_BUILD, eBuild, -1, 0, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);

		return true;
	}
	else if (canBuildRoute())
	{
		if (AI_connectPlot(pPlot))
		{
			return true;
		}
	}
	
	return false;
	
}

BuildTypes CvUnitAI::AI_betterPlotBuild(const CvPlot* pPlot, BuildTypes eBuild) const
{
	FAssert(pPlot != nullptr);
	FAssert(eBuild != NO_BUILD);
	bool bBuildRoute = false;
	bool bClearFeature = false;
	
	FeatureTypes eFeature = pPlot->getFeatureType();
	
	CvBuildInfo& kOriginalBuildInfo = GC.getBuildInfo(eBuild);
	
	if (kOriginalBuildInfo.getRoute() != NO_ROUTE)
	{
		return eBuild;
	}
	
	int iWorkersNeeded = AI_calculatePlotWorkersNeeded(pPlot, eBuild);
	
	if ((pPlot->getBonusType() == NO_BONUS) && (pPlot->getWorkingCity() != nullptr))
	{
		iWorkersNeeded = std::max(1, std::min(iWorkersNeeded, pPlot->getWorkingCity()->AI_getWorkersHave()));
	}
	
	if (eFeature != NO_FEATURE)
	{
		CvFeatureInfo& kFeatureInfo = GC.getFeatureInfo(eFeature);
		if (kOriginalBuildInfo.isFeatureRemove(eFeature))
		{
			if ((kOriginalBuildInfo.getImprovement() == NO_IMPROVEMENT) || (!pPlot->isBeingWorked() || (kFeatureInfo.getYieldChange(YIELD_FOOD) + kFeatureInfo.getYieldChange(YIELD_PRODUCTION)) <= 0))
			{
				bClearFeature = true;
			}
		}
		
		if ((kFeatureInfo.getMovementCost() > 1) && (iWorkersNeeded > 1))
		{
			bBuildRoute = true;
		}
	}
	
	if (pPlot->getBonusType() != NO_BONUS)
	{
		bBuildRoute = true;
	}
	else if (pPlot->isHills())
	{
		if ((GC.getHILLS_EXTRA_MOVEMENT() > 0) && (iWorkersNeeded > 1))
		{
			bBuildRoute = true;
		}
	}
	
	if (pPlot->getRouteType() != NO_ROUTE)
	{
		bBuildRoute = false;
	}
	
	BuildTypes eBestBuild = NO_BUILD;
	int iBestValue = 0;
	for (int iBuild = 0; iBuild < GC.getNumBuildInfos(); iBuild++)
	{
		BuildTypes eBuild2 = ((BuildTypes)iBuild);
		CvBuildInfo& kBuildInfo = GC.getBuildInfo(eBuild2);
		
		
		RouteTypes eRoute = (RouteTypes)kBuildInfo.getRoute();
		if ((bBuildRoute && (eRoute != NO_ROUTE)) || (bClearFeature && kBuildInfo.isFeatureRemove(eFeature)))
		{
			if (canBuild(pPlot, eBuild2))
			{
				int iValue = 10000;
				
				if (bBuildRoute && (eRoute != NO_ROUTE))
				{
					iValue *= (1 + GC.getRouteInfo(eRoute).getValue());
					iValue /= 2;
					
					if (pPlot->getBonusType() != NO_BONUS)
					{
						iValue *= 2;
					}
					
					if (pPlot->getWorkingCity() != nullptr)
					{
						iValue *= 2 + iWorkersNeeded + ((pPlot->isHills() && (iWorkersNeeded > 1)) ? 2 * GC.getHILLS_EXTRA_MOVEMENT() : 0);
						iValue /= 3;
					}
					ImprovementTypes eImprovement = (ImprovementTypes)kOriginalBuildInfo.getImprovement();
					if (eImprovement != NO_IMPROVEMENT)
					{
						int iRouteMultiplier = ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_FOOD)) * 100);
						iRouteMultiplier += ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_PRODUCTION)) * 100);
						iRouteMultiplier += ((GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eRoute, YIELD_COMMERCE)) * 60);
						iValue *= 100 + iRouteMultiplier;
						iValue /= 100;
					}
					
					int iPlotGroupId = -1;
					for (int iDirection = 0; iDirection < NUM_DIRECTION_TYPES; iDirection++)
					{
						CvPlot* pLoopPlot = plotDirection(pPlot->getX_INLINE(), pPlot->getY_INLINE(), (DirectionTypes)iDirection);
						if (pLoopPlot != nullptr)
						{
							if (pPlot->isRiver() || (pLoopPlot->getRouteType() != NO_ROUTE))
							{
#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
								CvPlotGroup* pLoopGroup = pLoopPlot->getPlotGroup(getOwnerINLINE());
								if (pLoopGroup != nullptr)
								{
									// fortsnek: This firaxis code is probably wrong.
									if (pLoopGroup->getID() != -1)
									{
										if (pLoopGroup->getID() != iPlotGroupId)
										{
											//This plot bridges plot groups, so route it.
											iValue *= 4;
											break;
										}
										else
										{
											iPlotGroupId = pLoopGroup->getID();
										}
									}
								}
#else
								if (const std::optional<int> plotGroupId = pLoopPlot->tryGetPlotGroupId(getOwnerINLINE()))
								{
									if (*plotGroupId != iPlotGroupId)
									{
										//This plot bridges plot groups, so route it.
										iValue *= 4;
										break;
									}
								}
#endif
							}
						}
					}	
				}

				iValue /= (kBuildInfo.getTime() + 1);

				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					eBestBuild = eBuild2;
				}
			}
		}
	}
	
	if (eBestBuild == NO_BUILD)
	{
		return eBuild;
	}
	return eBestBuild;	
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectBonus(bool bTestTrade)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	BonusTypes eNonObsoleteBonus;
	int iI;

	// XXX how do we make sure that we can build roads???

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				eNonObsoleteBonus = pLoopPlot->getNonObsoleteBonusType(getTeam());

				if (eNonObsoleteBonus != NO_BONUS)
				{
					if (!(pLoopPlot->isConnectedToCapital()))
					{
						if (!bTestTrade || ((pLoopPlot->getImprovementType() != NO_IMPROVEMENT) && (GC.getImprovementInfo(pLoopPlot->getImprovementType()).isImprovementBonusTrade(eNonObsoleteBonus))))
						{
							if (AI_connectPlot(pLoopPlot))
							{
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


// Returns true if a mission was pushed...
bool CvUnitAI::AI_connectCity()
{
	PROFILE_FUNC();

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
                if (AI_connectPlot(pLoopCity->plot(), 1))
                {
                    return true;
                }
            }
        }
    }

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			if (!(pLoopCity->isConnectedToCapital()))
			{
				if (AI_connectPlot(pLoopCity->plot(), 1))
				{
					return true;
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_routeCity()
{
	PROFILE_FUNC();

	CvCity* pRouteToCity;
	CvCity* pLoopCity;
	int iLoop;

	FAssert(canBuildRoute());

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			pRouteToCity = pLoopCity->AI_getRouteToCity();

			if (pRouteToCity != nullptr)
			{
				if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
				{
					if (generatePath(pLoopCity->plot(), MOVE_SAFE_TERRITORY, true))
					{
						if (!(pRouteToCity->plot()->isVisibleEnemyUnit(this)))
						{
							if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pRouteToCity->plot(), MISSIONAI_BUILD, getGroup()) == 0)
							{
								if (generatePath(pRouteToCity->plot(), MOVE_SAFE_TERRITORY, true))
								{
									getGroup()->pushMission(MISSION_ROUTE_TO, pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pRouteToCity->plot());
									getGroup()->pushMission(MISSION_ROUTE_TO, pRouteToCity->getX_INLINE(), pRouteToCity->getY_INLINE(), MOVE_SAFE_TERRITORY, (getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pRouteToCity->plot());

									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_routeTerritory(bool bImprovementOnly)
{
	PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	ImprovementTypes eImprovement;
	RouteTypes eBestRoute;
	bool bValid;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iI, iJ;

	// XXX how do we make sure that we can build roads???

	FAssert(canBuildRoute());

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (AI_plotValid(pLoopPlot))
		{
			if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE()) // XXX team???
			{
				eBestRoute = GET_PLAYER(getOwnerINLINE()).getBestRoute(pLoopPlot);

				if (eBestRoute != NO_ROUTE)
				{
					if (eBestRoute != pLoopPlot->getRouteType())
					{
						if (bImprovementOnly)
						{
							bValid = false;

							eImprovement = pLoopPlot->getImprovementType();

							if (eImprovement != NO_IMPROVEMENT)
							{
								for (iJ = 0; iJ < NUM_YIELD_TYPES; iJ++)
								{
									if (GC.getImprovementInfo(eImprovement).getRouteYieldChanges(eBestRoute, iJ) > 0)
									{
										bValid = true;
										break;
									}
								}
							}
						}
						else
						{
							bValid = true;
						}

						if (bValid)
						{
							if (!(pLoopPlot->isVisibleEnemyUnit(this)))
							{
								if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_BUILD, getGroup(), 1) == 0)
								{
									if (generatePath(pLoopPlot, MOVE_SAFE_TERRITORY, true, &iPathTurns))
									{
										iValue = 10000;

										iValue /= (iPathTurns + 1);

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
		}
	}

	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_ROUTE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pBestPlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_travelToUpgradeCity()
{
	// is there a city which can upgrade us?
	CvCity* pUpgradeCity = getUpgradeCity(/*bSearch*/ true);
	if (pUpgradeCity != nullptr)
	{
		// cache some stuff
		CvPlot* pPlot = plot();
		bool bSeaUnit = (getDomainType() == DOMAIN_SEA);
		bool bCanAirliftUnit = (getDomainType() == DOMAIN_LAND);
		bool bShouldSkipToUpgrade = (getDomainType() != DOMAIN_AIR);

		// if we at the upgrade city, stop, wait to get upgraded
		if (pUpgradeCity->plot() == pPlot)
		{
			if (!bShouldSkipToUpgrade)
			{
				return false;
			}
			
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}

		if (DOMAIN_AIR == getDomainType())
		{
			FAssert(!atPlot(pUpgradeCity->plot()));
			getGroup()->pushMission(MISSION_MOVE_TO, pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE());
			return true;
		}

		// find the closest city
		CvCity* pClosestCity = pPlot->getPlotCity();
		bool bAtClosestCity = (pClosestCity != nullptr);
		if (pClosestCity == nullptr)
		{
			pClosestCity = pPlot->getWorkingCity();
		}
		if (pClosestCity == nullptr)
		{
			pClosestCity = GC.getMapINLINE().findCity(getX_INLINE(), getY_INLINE(), NO_PLAYER, getTeam(), true, bSeaUnit);
		}

		// can we path to the upgrade city?
		int iUpgradeCityPathTurns;
		CvPlot* pThisTurnPlot = nullptr;
		bool bCanPathToUpgradeCity = generatePath(pUpgradeCity->plot(), 0, true, &iUpgradeCityPathTurns);
		if (bCanPathToUpgradeCity)
		{
			pThisTurnPlot = getPathEndTurnPlot();
		}
		
		// if we close to upgrade city, head there 
		if (nullptr != pThisTurnPlot && nullptr != pClosestCity && (pClosestCity == pUpgradeCity || iUpgradeCityPathTurns < 4))
		{
			FAssert(!atPlot(pThisTurnPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlot->getX_INLINE(), pThisTurnPlot->getY_INLINE());
			return true;
		}
		
		// check for better airlift choice
		if (bCanAirliftUnit && nullptr != pClosestCity && pClosestCity->getMaxAirlift() > 0)
		{
			// if we at the closest city, then do the airlift, or wait
			if (bAtClosestCity)
			{
				// can we do the airlift this turn?
				if (canAirliftAt(pClosestCity->plot(), pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE()))
				{
					getGroup()->pushMission(MISSION_AIRLIFT, pUpgradeCity->getX_INLINE(), pUpgradeCity->getY_INLINE());
					return true;
				}
				// wait to do it next turn
				else
				{
					getGroup()->pushMission(MISSION_SKIP);
					return true;
				}
			}
			
			int iClosestCityPathTurns;
			CvPlot* pThisTurnPlotForAirlift = nullptr;
			bool bCanPathToClosestCity = generatePath(pClosestCity->plot(), 0, true, &iClosestCityPathTurns);
			if (bCanPathToClosestCity)
			{
				pThisTurnPlotForAirlift = getPathEndTurnPlot();
			}
			
			// is the closest city closer pathing? If so, move toward closest city
			if (nullptr != pThisTurnPlotForAirlift && (!bCanPathToUpgradeCity || iClosestCityPathTurns < iUpgradeCityPathTurns))
			{
				FAssert(!atPlot(pThisTurnPlotForAirlift));
				getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlotForAirlift->getX_INLINE(), pThisTurnPlotForAirlift->getY_INLINE());
				return true;
			}
		}
		
		// did not have better airlift choice, go ahead and path to the upgrade city
		if (nullptr != pThisTurnPlot)
		{
			FAssert(!atPlot(pThisTurnPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pThisTurnPlot->getX_INLINE(), pThisTurnPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}

// Returns true if a mission was pushed...
bool CvUnitAI::AI_retreatToCity(bool bPrimary, bool bAirlift, int iMaxPath)
{
	PROFILE_FUNC();

	CvCity* pCity;
	
	CvPlot* pBestPlot = nullptr;
	int iPathTurns;
	int iValue;
	int iBestValue = INT_MAX;
	int iPass;
	int iLoop;
	int iCurrentDanger = GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot());

	pCity = plot()->getPlotCity();


	if (0 == iCurrentDanger)
	{
		if (pCity != nullptr)
		{
			if (pCity->getOwnerINLINE() == getOwnerINLINE())
			{
				if (!bPrimary || GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pCity->area()))
				{
					if (!bAirlift || (pCity->getMaxAirlift() > 0))
					{
						if (!(pCity->plot()->isVisibleEnemyUnit(this)))
						{
							getGroup()->pushMission(MISSION_SKIP);
							return true;
						}
					}
				}
			}
		}
	}

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	std::vector<const CvCity*> cities;
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_retreatToCity])
	{
		cities.reserve(GET_PLAYER(getOwnerINLINE()).getNumCities());
		for (const CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
			cities.push_back(pLoopCity);
		// Deterministic sort by distance.
		std::ranges::sort(cities, std::less<>(), [originX = getX_INLINE(), originY = getY_INLINE()](const CvCity* city) {
			return std::pair(::stepDistance(originX, originY, city->getX_INLINE(), city->getY_INLINE()), city->getID());
			});
	}
#endif

	for (iPass = 0; iPass < 4; iPass++)
	{
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
		if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::Faster_CvUnitAI_AI_retreatToCity])
		{
			const unsigned int pathingFlags = iPass > 1 ? MOVE_IGNORE_DANGER : 0;
			const int passMaxTurns = iPass == 2 ? 1 : iMaxPath;
			for (const CvCity* const pLoopCity : cities)
			{
				if (AI_plotValid(pLoopCity->plot()))
				{
					if (!bPrimary || GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pLoopCity->area()))
					{
						if (!bAirlift || (pLoopCity->getMaxAirlift() > 0))
						{
							if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
							{
								if ((iPass > 0) || (getGroup()->canFight() || GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopCity->plot()) < iCurrentDanger))
								{
									const int valueMul = AI_getUnitAIType() == UNITAI_SETTLER_SEA
										? 1 + std::max(0, GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLE) - GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLER_SEA))
										: 1;

									// iPathTurns * valueMul < iBestValue
									// iPathTurns <= floor(iBestValue / valueMul)
									const int pathMaxTurns = std::min(passMaxTurns, iBestValue / valueMul);

									if (!atPlot(pLoopCity->plot())
										&& generatePath(pLoopCity->plot(), PathingArg(pathingFlags, pathMaxTurns), true, &iPathTurns)
										&& iPathTurns <= passMaxTurns)
									{
										iValue = iPathTurns * valueMul;

										if (iValue < iBestValue)
										{
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
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
		else
#endif
		{
			for (CvCity* pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
			{
				if (AI_plotValid(pLoopCity->plot()))
				{
					if (!bPrimary || GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pLoopCity->area()))
					{
						if (!bAirlift || (pLoopCity->getMaxAirlift() > 0))
						{
							if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
							{
								if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), ((iPass > 1) ? MOVE_IGNORE_DANGER : 0), true, &iPathTurns))
								{
									if (iPathTurns <= ((iPass == 2) ? 1 : iMaxPath))
									{
										if ((iPass > 0) || (getGroup()->canFight() || GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pLoopCity->plot()) < iCurrentDanger))
										{
											iValue = iPathTurns;

											if (AI_getUnitAIType() == UNITAI_SETTLER_SEA)
											{
												iValue *= 1 + std::max(0, GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLE) - GET_PLAYER(getOwnerINLINE()).AI_totalAreaUnitAIs(pLoopCity->area(), UNITAI_SETTLER_SEA));
											}

											if (iValue < iBestValue)
											{
												iBestValue = iValue;
												pBestPlot = getPathEndTurnPlot();
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
				if (pCity->getOwnerINLINE() == getOwnerINLINE())
				{
					if (!bPrimary || GET_PLAYER(getOwnerINLINE()).AI_isPrimaryArea(pCity->area()))
					{
						if (!bAirlift || (pCity->getMaxAirlift() > 0))
						{
							if (!(pCity->plot()->isVisibleEnemyUnit(this)))
							{
								getGroup()->pushMission(MISSION_SKIP);
								return true;
							}
						}
					}
				}
			}
		}

		if (getGroup()->alwaysInvisible())
		{
			break;
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((iPass > 0) ? MOVE_IGNORE_DANGER : 0));
		return true;
	}

	if (pCity != nullptr)
	{
		if (pCity->getTeam() == getTeam())
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_pickup(UnitAITypes eUnitAI)
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestPickupPlot;
	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	FAssert(cargoSpace() > 0);
	if (0 == cargoSpace())
	{
		return false;
	}

	pCity = plot()->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwnerINLINE() == getOwnerINLINE())
		{
			if (pCity->plot()->plotCount(PUF_isUnitAIType, eUnitAI, -1, getOwnerINLINE()) > 0)
			{
				if ((AI_getUnitAIType() != UNITAI_ASSAULT_SEA) || pCity->AI_isDefended(-1))
				{
					getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, MISSIONAI_PICKUP, pCity->plot());
					return true;
				}
			}
		}
	}

	iBestValue = 0;
	pBestPlot = nullptr;
	pBestPickupPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (AI_plotValid(pLoopCity->plot()))
		{
			
			if ((AI_getUnitAIType() != UNITAI_ASSAULT_SEA) || pLoopCity->AI_isDefended(-1))
			{
				int iCount = pLoopCity->plot()->plotCount(PUF_isUnitAIType, eUnitAI, -1, getOwnerINLINE(), NO_TEAM, PUF_isFiniteRange);
				iValue = iCount * 10;
				
				if (pLoopCity->getProductionUnitAI() == eUnitAI)
				{
					CvUnitInfo& kUnitInfo = GC.getUnitInfo(pLoopCity->getProductionUnit());
					if ((kUnitInfo.getDomainType() != DOMAIN_AIR) || kUnitInfo.getAirRange() > 0)
					{
						iValue++;
						iCount++;
					}
				}

				if (iValue > 0)
				{
					iValue += pLoopCity->getPopulation();

					if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
					{
						if (GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_PICKUP, getGroup()) < ((iCount + (cargoSpace() - 1)) / cargoSpace()))
						{
							if (!atPlot(pLoopCity->plot()) && generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
							{
								iValue *= 1000;

								iValue /= (iPathTurns + 1);

								if (iValue > iBestValue)
								{
									iBestValue = iValue;
									pBestPlot = pLoopCity->plot();
									pBestPickupPlot = pLoopCity->plot();
								}
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr) && (pBestPickupPlot != nullptr))
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), 0, false, false, MISSIONAI_PICKUP, pBestPickupPlot);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airOffensiveCity()
{
	PROFILE_FUNC();

	CvCity* pNearestEnemyCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iI;

	FAssert(canAirAttack() || nukeRange() >= 0);

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->isCity(true, getTeam()))
		{
			if (pLoopPlot->getTeam() == getTeam()) // XXX team???
			{
				CvCity* pLoopCity = pLoopPlot->getPlotCity();
				bool bValid = false;
				
				int iAirBaseValue = (pLoopCity != nullptr) ? 0 : GET_PLAYER(getOwnerINLINE()).AI_getPlotAirbaseValue(pLoopPlot);
				iAirBaseValue /= 6;
				
				int iDefenders = pLoopPlot->plotCount(PUF_canDefend, -1, -1, getOwnerINLINE());
				
				if (pLoopCity != nullptr)
				{
					if (iDefenders > 2)
					{
						bValid = true;
						if (!pLoopCity->AI_isDanger())
						{
							iDefenders += 2;
						}
					}				
				}
				else if (iAirBaseValue > 0)
				{
					bValid = true;
				}
				

				
				if (bValid)
				{
					if (atPlot(pLoopPlot) || canMoveInto(pLoopPlot))
					{
						// XXX is in danger?
						iValue = iAirBaseValue;
						if (pLoopCity != nullptr)
						{
							iValue = (pLoopCity->getPopulation() + 20);
							iValue += pLoopCity->AI_cityThreat();
						}

						if (pLoopPlot->getOwnerINLINE() == getOwnerINLINE())
						{
							iValue *= 3;
							iValue /= 2;
						}
					
						iValue *= 1000;

						pNearestEnemyCity = GC.getMapINLINE().findCity(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), NO_PLAYER, NO_TEAM, false, false, getTeam());

						if (pNearestEnemyCity != nullptr)
						{
							int iDistance = plotDistance(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), pNearestEnemyCity->getX_INLINE(), pNearestEnemyCity->getY_INLINE());
							if (iDistance > airRange())
							{
								iValue /= 10 * (2 + airRange());
							}
							else
							{
								iValue /= 2 + iDistance;
							}
						}
						
						int iAttackAirCount = pLoopPlot->plotCount(PUF_canAirAttack, -1, -1, NO_PLAYER, getTeam());
						iAttackAirCount += 2 * pLoopPlot->plotCount(PUF_isUnitAIType, UNITAI_ICBM, -1, NO_PLAYER, getTeam());
						if (atPlot(pLoopPlot))
						{
							iAttackAirCount += canAirAttack() ? -1 : 0;
							iAttackAirCount += (nukeRange() >= 0) ? -2 : 0;
						}
						
						
						if (iAttackAirCount <= iDefenders)
						{
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

	if (pBestPlot != nullptr)
	{
		if (!atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airDefensiveCity()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	int iValue;
	int iBestValue;
	int iLoop;

	FAssert(getDomainType() == DOMAIN_AIR);
	FAssert(canAirDefend());

	if (canAirDefend())
	{
		pCity = plot()->getPlotCity();

		if (pCity != nullptr)
		{
			if (pCity->getOwnerINLINE() == getOwnerINLINE())
			{
				if (!(pCity->AI_isAirDefended(-1)))
				{
					getGroup()->pushMission(MISSION_AIRPATROL);
					return true;
				}
			}
		}
	}

	iBestValue = 0;
	pBestPlot = nullptr;

	for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
	{
		if (canAirDefend(pLoopCity->plot()))
		{
			if (!(pLoopCity->AI_isAirDefended()))
			{
				if (!atPlot(pLoopCity->plot()) && canMoveInto(pLoopCity->plot()))
				{
					iValue = pLoopCity->getPopulation() + pLoopCity->AI_cityThreat();

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						pBestPlot = pLoopCity->plot();
					}
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		FAssert(!atPlot(pBestPlot));
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airCarrier()
{
	PROFILE_FUNC();

	CvUnit* pLoopUnit;
	CvUnit* pBestUnit;
	int iValue;
	int iBestValue;
	int iLoop;

	if (getCargo() > 0)
	{
		return false;
	}

	if (isCargo())
	{
		if (canAirDefend())
		{
			getGroup()->pushMission(MISSION_AIRPATROL);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
	}

	iBestValue = 0;
	pBestUnit = nullptr;

	for(pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop))
	{
		if (canLoadUnit(pLoopUnit, pLoopUnit->plot()))
		{
			iValue = 10;

			if (!(pLoopUnit->plot()->isCity()))
			{
				iValue += 20;
			}

			if (pLoopUnit->plot()->isOwned())
			{
				if (isEnemy(pLoopUnit->plot()->getTeam(), pLoopUnit->plot()))
				{
					iValue += 20;
				}
			}
			else
			{
				iValue += 10;
			}

			iValue /= (pLoopUnit->getCargo() + 1);

			if (iValue > iBestValue)
			{
				iBestValue = iValue;
				pBestUnit = pLoopUnit;
			}
		}
	}

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			setTransportUnit(pBestUnit); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestUnit->getX_INLINE(), pBestUnit->getY_INLINE());
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_missileLoad(UnitAITypes eTargetUnitAI, int iMaxOwnUnitAI, bool bStealthOnly)
{
	PROFILE_FUNC();

	CvUnit* pBestUnit = nullptr;
	int iBestValue = 0;
	int iLoop;
	CvUnit* pLoopUnit;
	for(pLoopUnit = GET_PLAYER(getOwnerINLINE()).firstUnit(&iLoop); pLoopUnit != nullptr; pLoopUnit = GET_PLAYER(getOwnerINLINE()).nextUnit(&iLoop))
	{
		if (!bStealthOnly || pLoopUnit->getInvisibleType() != NO_INVISIBLE)
		{
			if (pLoopUnit->AI_getUnitAIType() == eTargetUnitAI)
			{
				if ((iMaxOwnUnitAI == -1) || (pLoopUnit->getUnitAICargo(AI_getUnitAIType()) <= iMaxOwnUnitAI))
				{
					if (canLoadUnit(pLoopUnit, pLoopUnit->plot()))
					{
						int iValue = 100;
						
						iValue += GC.getGame().getSorenRandNum(100, "AI missile load");

						iValue *= 1 + pLoopUnit->getCargo();

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestUnit = pLoopUnit;
						}
					}
				}
			}
		}
	}

	if (pBestUnit != nullptr)
	{
		if (atPlot(pBestUnit->plot()))
		{
			setTransportUnit(pBestUnit); // XXX is this dangerous (not pushing a mission...) XXX air units?
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestUnit->getX_INLINE(), pBestUnit->getY_INLINE());
			setTransportUnit(pBestUnit);
			return true;
		}
	}

	return false;	
	
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_airStrike()
{
	PROFILE_FUNC();

	CvUnit* pDefender;
	CvUnit* pInterceptor;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iDamage;
	int iPotentialAttackers;
	int iInterceptProb;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	iSearchRange = airRange();

	iBestValue = (isSuicide() && m_pUnitInfo->getProductionCost() > 0) ? (5 * m_pUnitInfo->getProductionCost()) / 6 : 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (canMoveInto(pLoopPlot, true))
				{
					iValue = 0;
					iPotentialAttackers = GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pLoopPlot);
					if (pLoopPlot->isCity())
					{
						iPotentialAttackers += GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pLoopPlot, MISSIONAI_ASSAULT, getGroup(), 1) * 2;							
					}
					if (pLoopPlot->isWater() || (iPotentialAttackers > 0) || pLoopPlot->isAdjacentTeam(getTeam()))
					{
						pDefender = pLoopPlot->getBestDefender(NO_PLAYER, getOwnerINLINE(), this, true);

						FAssert(pDefender != nullptr);
						FAssert(pDefender->canDefend());

						// XXX factor in air defenses...

						iDamage = airCombatDamage(pDefender);

						iValue = std::max(0, (std::min((pDefender->getDamage() + iDamage), airCombatLimit()) - pDefender->getDamage()));

						iValue += ((((iDamage * collateralDamage()) / 100) * std::min((pLoopPlot->getNumVisibleEnemyDefenders(this) - 1), collateralDamageMaxUnits())) / 2);

						iValue *= (3 + iPotentialAttackers);
						iValue /= 4;

						pInterceptor = bestInterceptor(pLoopPlot);

						if (pInterceptor != nullptr)
						{
							iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

							iInterceptProb *= std::max(0, (100 - evasionProbability()));
							iInterceptProb /= 100;

							iValue *= std::max(0, 100 - iInterceptProb / 2);
							iValue /= 100;
						}
						
						if (pLoopPlot->isWater())
						{
							iValue *= 3;
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = pLoopPlot;
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_airBombPlots()
{
	PROFILE_FUNC();

	CvUnit* pInterceptor;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iInterceptProb;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	iSearchRange = airRange();

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (!pLoopPlot->isCity() && pLoopPlot->isOwned() && pLoopPlot != plot())
				{
					if (canAirBombAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
					{
						iValue = 0;

						if (pLoopPlot->getBonusType(pLoopPlot->getTeam()) != NO_BONUS)
						{
							iValue += AI_pillageValue(pLoopPlot, 15);

							iValue += GC.getGameINLINE().getSorenRandNum(10, "AI Air Bomb");
						}
						else if (isSuicide())
						{
							//This should only be reached when the unit is desperate to die
							iValue += AI_pillageValue(pLoopPlot);
							// Guided missiles lean towards destroying resource-producing tiles as opposed to improvements like Towns
							if (pLoopPlot->getBonusType(pLoopPlot->getTeam()) != NO_BONUS)
							{
								//and even more so if it's a resource
								iValue += GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(pLoopPlot->getBonusType(pLoopPlot->getTeam()));
							}
						}

						if (iValue > 0)
						{

							pInterceptor = bestInterceptor(pLoopPlot);

							if (pInterceptor != nullptr)
							{
								iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

								iInterceptProb *= std::max(0, (100 - evasionProbability()));
								iInterceptProb /= 100;

								iValue *= std::max(0, 100 - iInterceptProb / 2);
								iValue /= 100;
							}

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
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
		getGroup()->pushMission(MISSION_AIRBOMB, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}	


bool CvUnitAI::AI_airBombDefenses()
{
	PROFILE_FUNC();

	CvCity* pCity;
	CvUnit* pInterceptor;
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
	int iSearchRange;
	int iPotentialAttackers;
	int iInterceptProb;
	int iValue;
	int iBestValue;
	int iDX, iDY;

	iSearchRange = airRange();

	iBestValue = 0;
	pBestPlot = nullptr;

	for (iDX = -(iSearchRange); iDX <= iSearchRange; iDX++)
	{
		for (iDY = -(iSearchRange); iDY <= iSearchRange; iDY++)
		{
			pLoopPlot	= plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				pCity = pLoopPlot->getPlotCity();
				if (pCity != nullptr)
				{
					iValue = 0;

					if (canAirBombAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
					{
						iPotentialAttackers = GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pLoopPlot);
						iPotentialAttackers += std::max(0, GET_PLAYER(getOwnerINLINE()).AI_plotTargetMissionAIs(pCity->plot(), NO_MISSIONAI, getGroup(), 2) - 4);

						if (iPotentialAttackers > 1)
						{
							iValue += std::max(0, (std::min((pCity->getDefenseDamage() + airBombCurrRate()), GC.getMAX_CITY_DEFENSE_DAMAGE()) - pCity->getDefenseDamage()));

							iValue *= 4 + iPotentialAttackers;

							if (pCity->AI_isDanger())
							{
								iValue *= 2;
							}

							if (pCity == pCity->area()->getTargetCity(getOwnerINLINE()))
							{
								iValue *= 2;
							}
						}
						
						if (iValue > 0)
						{
							pInterceptor = bestInterceptor(pLoopPlot);

							if (pInterceptor != nullptr)
							{
								iInterceptProb = isSuicide() ? 100 : pInterceptor->currInterceptionProbability();

								iInterceptProb *= std::max(0, (100 - evasionProbability()));
								iInterceptProb /= 100;

								iValue *= std::max(0, 100 - iInterceptProb / 2);
								iValue /= 100;
							}

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopPlot;
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
		getGroup()->pushMission(MISSION_AIRBOMB, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;	
	
}

bool CvUnitAI::AI_exploreAir()
{
	PROFILE_FUNC();
	
	CvPlayer& kPlayer = GET_PLAYER(getOwnerINLINE());
	int iLoop;
	CvCity* pLoopCity;
	CvPlot* pBestPlot = nullptr;
	int iBestValue = 0;
	
	for (int iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !GET_PLAYER((PlayerTypes)iI).isBarbarian())
		{
			if (GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
			{
				for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
				{
					if (!pLoopCity->isVisible(getTeam(), false))
					{
						if (canReconAt(plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
						{
							int iValue = 1 + GC.getGame().getSorenRandNum(15, "AI explore air");
							if (isEnemy(GET_PLAYER((PlayerTypes)iI).getTeam()))
							{
								iValue += 10;
								iValue += std::min(10,  pLoopCity->area()->getNumAIUnits(getOwnerINLINE(), UNITAI_ATTACK_CITY));
								iValue += 10 * kPlayer.AI_plotTargetMissionAIs(pLoopCity->plot(), MISSIONAI_ASSAULT);
							}
							
							iValue *= plotDistance(getX_INLINE(), getY_INLINE(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE());
							
							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();								
							}
						}
					}
				}
			}
		}
	}
	
	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_RECON, pBestPlot->getX(), pBestPlot->getY());
		return true;
	}
	
	return false;	
}


// Returns true if a mission was pushed...
bool CvUnitAI::AI_nuke()
{
	PROFILE_FUNC();

	CvCity* pLoopCity;
	CvCity* pBestCity;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	pBestCity = nullptr;

	iBestValue = 0;

	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if (GET_PLAYER((PlayerTypes)iI).isAlive() && !GET_PLAYER((PlayerTypes)iI).isBarbarian())
		{
			if (isEnemy(GET_PLAYER((PlayerTypes)iI).getTeam()))
			{
				if (GET_PLAYER(getOwnerINLINE()).AI_getAttitude((PlayerTypes)iI) == ATTITUDE_FURIOUS)
				{
					for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
					{
						if (canNukeAt(plot(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()))
						{
							iValue = AI_nukeValue(pLoopCity);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestCity = pLoopCity;
								FAssert(pBestCity->getTeam() != getTeam());
							}
						}
					}
				}
			}
		}
	}

	if (pBestCity != nullptr)
	{
		getGroup()->pushMission(MISSION_NUKE, pBestCity->getX_INLINE(), pBestCity->getY_INLINE());
		return true;
	}

	return false;
}

bool CvUnitAI::AI_nukeRange(int iRange)
{
	CvPlot* pBestPlot = nullptr;
	int iBestValue = 0;
	for (int iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (int iDY = -(iRange); iDY <= iRange; iDY++)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);
			
			if (pLoopPlot != nullptr)
			{
				if (canNukeAt(plot(), pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE()))
				{
					int iValue = -99;
					
					for (int iDX2 = -(nukeRange()); iDX2 <= nukeRange(); iDX2++)
					{
						for (int iDY2 = -(nukeRange()); iDY2 <= nukeRange(); iDY2++)
						{
							CvPlot* pLoopPlot2 = plotXY(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iDX2, iDY2);

							if (pLoopPlot2 != nullptr)
							{
								int iEnemyCount = 0;
								int iTeamCount = 0;
								int iNeutralCount = 0;
								int iDamagedEnemyCount = 0;
								
								CLLNode<IDInfo>* pUnitNode;
								CvUnit* pLoopUnit;
								pUnitNode = pLoopPlot2->headUnitNode();
								while (pUnitNode != nullptr)
								{
									pLoopUnit = ::getUnit(pUnitNode->m_data);
									pUnitNode = pLoopPlot2->nextUnitNode(pUnitNode);
									
									if (!pLoopUnit->isNukeImmune())
									{
										if (pLoopUnit->getTeam() == getTeam())
										{
											iTeamCount++;
										}
										else if (!pLoopUnit->isInvisible(getTeam(), false))
										{
											if (isEnemy(pLoopUnit->getTeam()))
											{
												iEnemyCount++;
												if (pLoopUnit->getDamage() * 2 > pLoopUnit->maxHitPoints())
												{
													iDamagedEnemyCount++;
												}
											}
											else
											{
												iNeutralCount++;
											}
										}
									}
								}
								
								iValue += (iEnemyCount + iDamagedEnemyCount) * (pLoopPlot2->isWater() ? 25 : 12);
								iValue -= iTeamCount * 15;
								iValue -= iNeutralCount * 20;
								
								
								int iMultiplier = 1;
								if (pLoopPlot2->getTeam() == getTeam())
								{
									iMultiplier = -2;
								}
								else if (isEnemy(pLoopPlot2->getTeam()))
								{
									iMultiplier = 1;
								}
								else if (!pLoopPlot2->isOwned())
								{
									iMultiplier = 0;
								}
								else
								{
									iMultiplier = -10;
								}
								
								if (pLoopPlot2->getImprovementType() != NO_IMPROVEMENT)
								{
									iValue += iMultiplier * 10;
								}

								if (pLoopPlot2->getBonusType() != NO_BONUS)
								{
									iValue += iMultiplier * 20;
								}
								
								if (pLoopPlot2->isCity())
								{
									iValue += std::max(0, iMultiplier * (-20 + 15 * pLoopPlot2->getPlotCity()->getPopulation()));
								}
							}
						}
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
	
	if (pBestPlot != nullptr)
	{
		getGroup()->pushMission(MISSION_NUKE, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}
	
	return false;
}

bool CvUnitAI::AI_trade(int iValueThreshold)
{
	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvPlot* pBestTradePlot;

	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;


	iBestValue = 0;
	pBestPlot = nullptr;
	pBestTradePlot = nullptr;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_trade])
	{
		struct Entry
		{
			const CvCity* city = nullptr;
			int dist = 0;
			int value = 0;

			auto cmpValue() const
			{
				return std::tuple{ dist, city->getX_INLINE(), city->getY_INLINE() };
			}

			bool operator<(Entry b) const
			{
				return cmpValue() < b.cmpValue();
			}
		};

		std::vector<Entry> entries;
		entries.reserve(250);

		for (iI = 0; iI < MAX_PLAYERS; iI++)
		{
			if (GET_PLAYER((PlayerTypes)iI).isAlive())
			{
				for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
				{
					if (AI_plotValid(pLoopCity->plot()))
					{
						if (getTeam() != pLoopCity->getTeam())
						{
							iValue = getTradeGold(pLoopCity->plot());

							if ((iValue >= iValueThreshold) && canTrade(pLoopCity->plot(), true))
							{
								if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
								{
									entries.emplace_back(pLoopCity, stepDistance(getX_INLINE(), getY_INLINE(), pLoopCity->getX_INLINE(), pLoopCity->getY_INLINE()), iValue);
								}
							}
						}
					}
				}
			}
		}

		std::ranges::sort(entries, std::less<>());

		for (auto [city, dist, value] : entries)
		{
			// iValue / (4 + iPathTurns) > iBestValue
			// iPathTurns < (iValue-4*iBestValue)/iBestValue
			if (generatePath(city->plot(), PathingArg(0, iBestValue ? (value - 4 * iBestValue) / iBestValue + 1 : INT_MAX), true, &iPathTurns))
			{
				FAssert(iPathTurns > 0);

				value /= (4 + iPathTurns);

				if (value > iBestValue)
				{
					iBestValue = value;
					pBestPlot = getPathEndTurnPlot();
					pBestTradePlot = city->plot();
				}
			}
		}
	}
	else
#endif
	{
		for (iI = 0; iI < MAX_PLAYERS; iI++)
		{
			if (GET_PLAYER((PlayerTypes)iI).isAlive())
			{
				for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
				{
					if (AI_plotValid(pLoopCity->plot()))
					{
						if (getTeam() != pLoopCity->getTeam())
						{
							iValue = getTradeGold(pLoopCity->plot());

							if ((iValue >= iValueThreshold) && canTrade(pLoopCity->plot(), true))
							{
								if (!(pLoopCity->plot()->isVisibleEnemyUnit(this)))
								{
									if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
									{
										FAssert(iPathTurns > 0);

										iValue /= (4 + iPathTurns);

										if (iValue > iBestValue)
										{
											iBestValue = iValue;
											pBestPlot = getPathEndTurnPlot();
											pBestTradePlot = pLoopCity->plot();
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

	if ((pBestPlot != nullptr) && (pBestTradePlot != nullptr))
	{
		if (atPlot(pBestTradePlot))
		{
			getGroup()->pushMission(MISSION_TRADE);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_infiltrate()
{
	CvCity* pLoopCity;
	CvPlot* pBestPlot;

	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;
	int iI;

	iBestValue = 0;
	pBestPlot = nullptr;
	
	if (canInfiltrate(plot()))
	{
		getGroup()->pushMission(MISSION_INFILTRATE);
		return true;
	}
	
	for (iI = 0; iI < MAX_PLAYERS; iI++)
	{
		if ((GET_PLAYER((PlayerTypes)iI).isAlive()) && GET_PLAYER((PlayerTypes)iI).getTeam() != getTeam())
		{
			for (pLoopCity = GET_PLAYER((PlayerTypes)iI).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER((PlayerTypes)iI).nextCity(&iLoop))
			{
				if (canInfiltrate(pLoopCity->plot()))
				{
					iValue = getEspionagePoints(pLoopCity->plot());
					
					if (iValue > 0)
					{
						if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
						{
							FAssert(iPathTurns > 0);
							
							if (getPathLastNode()->m_iData1 == 0)
							{
								iPathTurns++;
							}

							iValue /= 1 + iPathTurns;

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestPlot = pLoopCity->plot();
							}
						}
					}
				}
			}
		}
	}

	if ((pBestPlot != nullptr))
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_INFILTRATE);
			return true;
		}
		else
		{
			FAssert(!atPlot(pBestPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			getGroup()->pushMission(MISSION_INFILTRATE, -1, -1, 0, (getGroup()->getLengthMissionQueue() > 0));
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_reconSpy(int iRange)
{
	PROFILE_FUNC();
	CvPlot* pLoopPlot;
	int iX, iY;
	
	CvPlot* pBestPlot = nullptr;
	CvPlot* pBestTargetPlot = nullptr;
	int iBestValue = 0;
	
	int iSearchRange = AI_searchRange(iRange);

	for (iX = -iSearchRange; iX <= iSearchRange; iX++)
	{
		for (iY = -iSearchRange; iY <= iSearchRange; iY++)
		{
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			int iDistance = stepDistance(0, 0, iX, iY);
			if ((iDistance > 0) && (pLoopPlot != nullptr) && AI_plotValid(pLoopPlot))
			{
				int iValue = 0;
				if (pLoopPlot->getPlotCity() != nullptr)
				{
					iValue += GC.getGameINLINE().getSorenRandNum(4000, "AI Spy Scout City");
				}
				
				if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
				{
					iValue += GC.getGameINLINE().getSorenRandNum(1000, "AI Spy Recon Bonus");
				}
				
				for (int iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
				{
					CvPlot* pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), ((DirectionTypes)iI));

					if (pAdjacentPlot != nullptr)
					{
						if (!pAdjacentPlot->isRevealed(getTeam(), false))
						{
							iValue += 500;
						}
						else if (!pAdjacentPlot->isVisible(getTeam(), false))
						{
							iValue += 200;
						}
					}
				}


				if (iValue > 0)
				{
					int iPathTurns;
					if (generatePath(pLoopPlot, 0, true, &iPathTurns))
					{
						if (iPathTurns <= iRange)
						{
							// don't give each and every plot in range a value before generating the patch (performance hit)
							iValue += GC.getGameINLINE().getSorenRandNum(250, "AI Spy Scout Best Plot");

							iValue *= iDistance;

							/* Can no longer perform missions after having moved
							if (getPathLastNode()->m_iData2 == 1)
							{
								if (getPathLastNode()->m_iData1 > 0)
								{
									//Prefer to move and have movement remaining to perform a kill action.
									iValue *= 2;
								}
							} */

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								pBestTargetPlot = getPathEndTurnPlot();
								pBestPlot = pLoopPlot;
							}
						}
					}
				}
			}
		}
	}
	
	if ((pBestPlot != nullptr) && (pBestTargetPlot != nullptr))
	{
		if (atPlot(pBestTargetPlot))
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestTargetPlot->getX_INLINE(), pBestTargetPlot->getY_INLINE());			
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
	}
	
	return false;
}

int CvUnitAI::AI_getEspionageTargetValue(CvPlot* pPlot, int iMaxPath)
{
	CvTeamAI& kTeam = GET_TEAM(getTeam());
	int iValue = 0;

	if (pPlot->isOwned() && pPlot->getTeam() != getTeam() && !GET_TEAM(getTeam()).isVassal(pPlot->getTeam()))
	{
		if (AI_plotValid(pPlot))
		{
			if (pPlot->isCity())
			{
				iValue += 10;
				int iRand = GC.getGame().getSorenRandNum(8, "AI spy choose city");
				iValue += iRand * iRand;
			}
			else
			{
				BonusTypes eBonus = pPlot->getNonObsoleteBonusType(getTeam());
				if (eBonus != NO_BONUS)
				{
					iValue += GET_PLAYER(pPlot->getOwnerINLINE()).AI_baseBonusVal(eBonus) - 10;
				}
			}

			int iPathTurns;
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
			if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_getEspionageTargetValue]
				? generatePath(pPlot, PathingArg(0, iMaxPath), true, &iPathTurns)
				: generatePath(pPlot, 0, true, &iPathTurns))
#else
			if (generatePath(pPlot, 0, true, &iPathTurns))
#endif
			{
				if (iPathTurns <= iMaxPath)
				{
					if (kTeam.AI_getWarPlan(pPlot->getTeam()) == NO_WARPLAN)
					{
						iValue *= 1;
					}
					else if (kTeam.AI_isSneakAttackPreparing(pPlot->getTeam()))
					{
						iValue *= (pPlot->isCity()) ? 15 : 10;						
					}
					else
					{
						iValue *= 3;
					}
				}
			}
		}
	}

	return iValue;
}


bool CvUnitAI::AI_cityOffenseSpy(int iMaxPath)
{
	int iBestValue = 0;
	CvPlot* pBestPlot = nullptr;

	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; ++iPlayer)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.isAlive() && kLoopPlayer.getTeam() != getTeam() && !GET_TEAM(getTeam()).isVassal(kLoopPlayer.getTeam()))
		{
			int iLoop;
			for (CvCity* pLoopCity = kLoopPlayer.firstCity(&iLoop); nullptr != pLoopCity; pLoopCity = kLoopPlayer.nextCity(&iLoop))
			{
				if (pLoopCity->area() == area())
				{
					CvPlot* pLoopPlot = pLoopCity->plot();
					if (AI_plotValid(pLoopPlot))
					{
						int iValue = AI_getEspionageTargetValue(pLoopPlot, iMaxPath);
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
	
	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_SKIP);
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());			
			getGroup()->pushMission(MISSION_SKIP);
		}
		return true;
	}
	
	return false;
}

bool CvUnitAI::AI_bonusOffenseSpy(int iRange)
{
	CvPlot* pBestPlot = nullptr;
	int iBestValue = 0;

	int iSearchRange = AI_searchRange(iRange);

	for (int iX = -iSearchRange; iX <= iSearchRange; iX++)
	{
		for (int iY = -iSearchRange; iY <= iSearchRange; iY++)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);

			if (nullptr != pLoopPlot && pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
			{
				int iValue = AI_getEspionageTargetValue(pLoopPlot, iRange);
				if (iValue > iBestValue)
				{
					iBestValue = iValue;
					pBestPlot = pLoopPlot;								
				}
			}
		}
	}

	if (pBestPlot != nullptr)
	{
		if (atPlot(pBestPlot))
		{
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());			
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
	}

	return false;
}

//Returns true if the spy performs espionage.
bool CvUnitAI::AI_espionageSpy()
{
	if (!canEspionage(plot()))
	{
		return false;
	}
	
	EspionageMissionTypes eBestMission = NO_ESPIONAGEMISSION;
	CvPlot* pTargetPlot = nullptr;
	PlayerTypes eTargetPlayer = NO_PLAYER;
	int iExtraData = -1;
	
	eBestMission = GET_PLAYER(getOwnerINLINE()).AI_bestPlotEspionage(plot(), eTargetPlayer, pTargetPlot, iExtraData);
	if (NO_ESPIONAGEMISSION == eBestMission)
	{
		return false;
	}

	if (!GET_PLAYER(getOwnerINLINE()).canDoEspionageMission(eBestMission, eTargetPlayer, pTargetPlot, iExtraData, this))
	{
		return false;
	}
	
	if (!espionage(eBestMission, iExtraData))
	{
		return false;
	}
	
	return true;
}

bool CvUnitAI::AI_moveToStagingCity()
{
	CvCity* pLoopCity;
	CvPlot* pBestPlot;

	int iPathTurns;
	int iValue;
	int iBestValue;
	int iLoop;

	iBestValue = 0;
	pBestPlot = nullptr;
	
	int iWarCount = 0;
	TeamTypes eTargetTeam = NO_TEAM;
	CvTeam& kTeam = GET_TEAM(getTeam());
	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		if ((iI != getTeam()) && GET_TEAM((TeamTypes)iI).isAlive())
		{
			if (kTeam.AI_isSneakAttackPreparing((TeamTypes)iI))
			{
				eTargetTeam = (TeamTypes)iI;
				iWarCount++;
			}			
		}		
	}
	if (iWarCount > 1)
	{
		eTargetTeam = NO_TEAM;
	}
	
#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	if (gGlobals.enhancedDLLConfig[EEnhancedDLLConfigOption::PathLimit_CvUnitAI_AI_moveToStagingCity])
	{
		// TODO: Verify against firaxis implementation. Should yield exact same result.

		std::vector<CvCity*> cities;
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
			if (AI_plotValid(pLoopCity->plot()) && pLoopCity->AI_cityThreat() > 0)
				cities.push_back(pLoopCity);

		std::ranges::sort(cities, std::less<>(), [originX = getX_INLINE(), originY = getY_INLINE()](const CvCity* city) {
			return std::pair(::stepDistance(originX, originY, city->getX_INLINE(), city->getY_INLINE()), city->getID());
			});

		int bestCityI = INT_MAX;

		for (CvCity* const city : cities)
		{
			iValue = city->AI_cityThreat();

			// iBestValue < floor(floor(AI_cityThreat * 1000 / (5 + iPathTurns)) / penalty)
			// iBestValue < AI_cityThreat * 1000 / (5 + iPathTurns) / penalty
			// iBestValue * penalty * (5 + iPathTurns) < AI_cityThreat * 1000
			// (5 + iPathTurns) < AI_cityThreat * 1000 / (iBestValue * penalty)
			// iPathTurns < AI_cityThreat * 1000 / (iBestValue * penalty) - 5

			const bool hasPenalty = (city->plot() != plot()) && city->isVisible(eTargetTeam, false);
			constexpr int kPenalty = 2;
			const int maxTurns = iBestValue ? iValue * 1000 / (iBestValue * (hasPenalty ? kPenalty : 1)) - 5 : INT_MAX;

			if (generatePath(city->plot(), PathingArg(0, maxTurns), true, &iPathTurns))
			{
				iValue *= 1000;
				iValue /= (5 + iPathTurns);
				if (hasPenalty)
					iValue /= 2;

				if (iValue > iBestValue && city->getIndex() < bestCityI) // prefer cities earlier in index order
				{
					iBestValue = iValue;
					pBestPlot = getPathEndTurnPlot();
					bestCityI = city->getIndex();
				}
			}
		}
	}
	else
#endif
	{
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				iValue = pLoopCity->AI_cityThreat();
				if (iValue > 0)
				{
					if (generatePath(pLoopCity->plot(), 0, true, &iPathTurns))
					{
						iValue *= 1000;
						iValue /= (5 + iPathTurns);
						if ((pLoopCity->plot() != plot()) && pLoopCity->isVisible(eTargetTeam, false))
						{
							iValue /= 2;
						}

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							pBestPlot = getPathEndTurnPlot();
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
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
			return true;
		}
	}

	return false;
}

bool CvUnitAI::AI_seaRetreatFromCityDanger()
{
	if (plot()->isCity(true) && GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0) //prioritize getting outta there
	{
		if (AI_anyAttack(2, 40))
		{
			return true;
		}

		if (AI_anyAttack(4, 50))
		{
			return true;
		}

		if (AI_retreatToCity())
		{
			return true;
		}

		if (AI_safety())
		{
			return true;
		}
	}
	return false;
}

bool CvUnitAI::AI_airRetreatFromCityDanger()
{
	if (plot()->isCity(true))
	{
		CvCity* pCity = plot()->getPlotCity();
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(plot(), 2) > 0 || (pCity != nullptr && !pCity->AI_isDefended()))
		{
			if (AI_airOffensiveCity())
			{
				return true;
			}

			if (canAirDefend() && AI_airDefensiveCity())
			{
				return true;
			}
		}
	}
	return false;
}

bool CvUnitAI::AI_airAttackDamagedSkip()
{
	if (getDamage() == 0)
	{
		return false;
	}

	bool bSkip = (currHitPoints() * 100 / maxHitPoints() < 40);
	if (!bSkip)
	{
		int iSearchRange = airRange();
		bool bSkiesClear = true;
		for (int iDX = -iSearchRange; iDX <= iSearchRange && bSkiesClear; iDX++)
		{
			for (int iDY = -iSearchRange; iDY <= iSearchRange && bSkiesClear; iDY++)
			{
				CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);	
				if (pLoopPlot != nullptr)
				{
					if (bestInterceptor(pLoopPlot) != nullptr)
					{
						bSkiesClear = false;
						break;
					}
				}
			}
		}
		bSkip = !bSkiesClear;
	}

	if (bSkip)
	{
		getGroup()->pushMission(MISSION_SKIP);
		return true;
	}

	return false;
}


// Returns true if a mission was pushed or we should wait for another unit to bombard...
bool CvUnitAI::AI_followBombard()
{
	CLLNode<IDInfo>* pUnitNode;
	CvUnit* pLoopUnit;
	CvPlot* pAdjacentPlot1;
	CvPlot* pAdjacentPlot2;
	int iI, iJ;

	if (canBombard(plot()))
	{
		getGroup()->pushMission(MISSION_BOMBARD);
		return true;
	}

	if (getDomainType() == DOMAIN_LAND)
	{
		for (iI = 0; iI < NUM_DIRECTION_TYPES; iI++)
		{
			pAdjacentPlot1 = plotDirection(getX_INLINE(), getY_INLINE(), ((DirectionTypes)iI));

			if (pAdjacentPlot1 != nullptr)
			{
				if (pAdjacentPlot1->isCity())
				{
					if (AI_potentialEnemy(pAdjacentPlot1->getTeam(), pAdjacentPlot1))
					{
						for (iJ = 0; iJ < NUM_DIRECTION_TYPES; iJ++)
						{
							pAdjacentPlot2 = plotDirection(pAdjacentPlot1->getX_INLINE(), pAdjacentPlot1->getY_INLINE(), ((DirectionTypes)iJ));

							if (pAdjacentPlot2 != nullptr)
							{
								pUnitNode = pAdjacentPlot2->headUnitNode();

								while (pUnitNode != nullptr)
								{
									pLoopUnit = ::getUnit(pUnitNode->m_data);
									pUnitNode = pAdjacentPlot2->nextUnitNode(pUnitNode);

									if (pLoopUnit->getOwnerINLINE() == getOwnerINLINE())
									{
										if (pLoopUnit->canBombard(pAdjacentPlot2))
										{
											if (pLoopUnit->isGroupHead())
											{
												if (pLoopUnit->getGroup() != getGroup())
												{
													if (pLoopUnit->getGroup()->readyToMove())
													{
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

	return false;
}


// Returns true if the unit has found a potential enemy...
bool CvUnitAI::AI_potentialEnemy(TeamTypes eTeam, const CvPlot* pPlot) const
{
	if (getGroup()->AI_isDeclareWar(pPlot))
	{
		return isPotentialEnemy(eTeam, pPlot);
	}
	else
	{
		return isEnemy(eTeam, pPlot);
	}
}


// Returns true if this plot needs some defense...
bool CvUnitAI::AI_defendPlot(CvPlot* pPlot)
{
	CvCity* pCity;

	if (!canDefend(pPlot))
	{
		return false;
	}

	pCity = pPlot->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getOwnerINLINE() == getOwnerINLINE())
		{
			if (pCity->AI_isDanger())
			{
				return true;
			}
		}
	}
	else
	{
		if (pPlot->plotCount(PUF_canDefendGroupHead, -1, -1, getOwnerINLINE()) <= ((atPlot(pPlot)) ? 1 : 0))
		{
			if (pPlot->plotCount(PUF_cannotDefend, -1, -1, getOwnerINLINE()) > 0)
			{
				return true;
			}

//			if (pPlot->defenseModifier(getTeam(), false) >= 50 && pPlot->isRoute() && pPlot->getTeam() == getTeam())
//			{
//				return true;
//			}
		}
	}

	return false;
}


int CvUnitAI::AI_pillageValue(const CvPlot* pPlot, int iBonusValueThreshold) const
{
	const CvPlot* pAdjacentPlot;
	ImprovementTypes eImprovement;
	BonusTypes eNonObsoleteBonus;
	int iValue;
	int iTempValue;
	int iBonusValue;
	int iI;

	FAssert(canPillage(pPlot) || canAirBombAt(plot(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) || (getGroup()->getCargo() > 0));

	if (!(pPlot->isOwned()))
	{
		return 0;
	}
	
	iBonusValue = 0;
	eNonObsoleteBonus = pPlot->getNonObsoleteBonusType(pPlot->getTeam());
	if (eNonObsoleteBonus != NO_BONUS)
	{
		iBonusValue = (GET_PLAYER(pPlot->getOwnerINLINE()).AI_bonusVal(eNonObsoleteBonus));
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
				pAdjacentPlot = plotDirection(getX_INLINE(), getY_INLINE(), ((DirectionTypes)iI));

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
		eImprovement = pPlot->getRevealedImprovementType(getTeam(), false);
	}

	if (eImprovement != NO_IMPROVEMENT)
	{
		if (pPlot->getWorkingCity() != nullptr)
		{
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_FOOD, pPlot->getOwnerINLINE()) * 5);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_PRODUCTION, pPlot->getOwnerINLINE()) * 4);
			iValue += (pPlot->calculateImprovementYieldChange(eImprovement, YIELD_COMMERCE, pPlot->getOwnerINLINE()) * 3);
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

				if (pPlot->isConnectedToCapital() && (pPlot->getPlotGroupConnectedBonus(pPlot->getOwnerINLINE(), eNonObsoleteBonus) == 1))
				{
					iTempValue *= 2;
				}

				iValue += iTempValue;
			}
		}
	}

	return iValue;
}


int CvUnitAI::AI_nukeValue(CvCity* pCity)
{
	PROFILE_FUNC();
	FAssertMsg(pCity != nullptr, "City is not assigned a valid value");

	for (int iI = 0; iI < MAX_TEAMS; iI++)
	{
		CvTeam& kLoopTeam = GET_TEAM((TeamTypes)iI);
		if (kLoopTeam.isAlive() && !isEnemy((TeamTypes)iI))
		{
			if (isNukeVictim(pCity->plot(), ((TeamTypes)iI)))
			{
				// Don't start wars with neutrals
				return 0;
			}
		}
	}

	int iValue = 1;

	iValue += GC.getGameINLINE().getSorenRandNum((pCity->getPopulation() + 1), "AI Nuke City Value");
	iValue += std::max(0, pCity->getPopulation() - 10);

	iValue += ((pCity->getPopulation() * (100 + pCity->calculateCulturePercent(pCity->getOwnerINLINE()))) / 100);

	iValue += -(GET_PLAYER(getOwnerINLINE()).AI_getAttitudeVal(pCity->getOwnerINLINE()) / 3);

	for (int iDX = -(nukeRange()); iDX <= nukeRange(); iDX++)
	{
		for (int iDY = -(nukeRange()); iDY <= nukeRange(); iDY++)
		{
			CvPlot* pLoopPlot = plotXY(pCity->getX_INLINE(), pCity->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
				{
					iValue++;
				}

				if (pLoopPlot->getBonusType() != NO_BONUS)
				{
					iValue++;
				}
			}
		}
	}
	
	if (!(pCity->isEverOwned(getOwnerINLINE())))
	{
		iValue *= 3;
		iValue /= 2;
	}
	
	if (!GET_TEAM(pCity->getTeam()).isAVassal())
	{
		iValue *= 2;
	}
	
	if (pCity->plot()->isVisible(getTeam(), false))
	{
		iValue += 2 * pCity->plot()->getNumVisibleEnemyDefenders(this);
	}
	else
	{
		iValue += 6;
	}

	return iValue;
}


int CvUnitAI::AI_searchRange(int iRange) const
{
	if (iRange == 0)
	{
		return 0;
	}

	if (flatMovementCost() || (getDomainType() == DOMAIN_SEA))
	{
		return (iRange * baseMoves());
	}
	else
	{
		return ((iRange + 1) * (baseMoves() + 1));
	}
}


// XXX at some point test the game with and without this function...
bool CvUnitAI::AI_plotValid(const CvPlot* pPlot) const
{
	PROFILE_FUNC();

	if (m_pUnitInfo->isNoRevealMap() && willRevealByMove(pPlot))
	{
		return false;
	}

	switch (getDomainType())
	{
	case DOMAIN_SEA:
		if (pPlot->isWater() || canMoveAllTerrain())
		{
			return true;
		}
		else if (pPlot->isFriendlyCity(*this, true) && pPlot->isCoastalLand())
		{
			return true;
		}
		break;

	case DOMAIN_AIR:
		FAssert(false);
		break;

	case DOMAIN_LAND:
		if (pPlot->getArea() == getArea() || canMoveAllTerrain())
		{
			return true;
		}
		break;

	case DOMAIN_IMMOBILE:
		FAssert(false);
		break;

	default:
		FAssert(false);
		break;
	}

	return false;
}


int CvUnitAI::AI_finalOddsThreshold(const CvPlot* pPlot, int iOddsThreshold) const
{
	PROFILE_FUNC();

	CvCity* pCity;

	int iFinalOddsThreshold;

	iFinalOddsThreshold = iOddsThreshold;

	pCity = pPlot->getPlotCity();

	if (pCity != nullptr)
	{
		if (pCity->getDefenseDamage() < ((GC.getMAX_CITY_DEFENSE_DAMAGE() * 3) / 4))
		{
			iFinalOddsThreshold += std::max(0, (pCity->getDefenseDamage() - pCity->getLastDefenseDamage() - (GC.getDefineINT("CITY_DEFENSE_DAMAGE_HEAL_RATE") * 2)));
		}
	}

	if (pPlot->getNumVisiblePotentialEnemyDefenders(this) == 1)
	{
		if (pCity != nullptr)
		{
			iFinalOddsThreshold *= 2;
			iFinalOddsThreshold /= 3;
		}
		else
		{
			iFinalOddsThreshold *= 7;
			iFinalOddsThreshold /= 8;
		}
	}
	
	if ((getDomainType() == DOMAIN_SEA) && !getGroup()->hasCargo())
	{
		iFinalOddsThreshold *= 3;
		iFinalOddsThreshold /= 2 + getGroup()->getNumUnits();
	}
	else
	{
		iFinalOddsThreshold *= 6;
		iFinalOddsThreshold /= (3 + GET_PLAYER(getOwnerINLINE()).AI_adjacentPotentialAttackers(pPlot, true) + ((stepDistance(getX_INLINE(), getY_INLINE(), pPlot->getX_INLINE(), pPlot->getY_INLINE()) > 1) ? 1 : 0) + ((AI_isCityAIType()) ? 2 : 0));
	}

	return range(iFinalOddsThreshold, 1, 99);
}


int CvUnitAI::AI_stackOfDoomExtra() const
{
	return ((AI_getBirthmark() % (1 + GET_PLAYER(getOwnerINLINE()).getCurrentEra())) + 4);
}

bool CvUnitAI::AI_stackAttackCity(int iRange, int iPowerThreshold, bool bFollow)
{
    PROFILE_FUNC();
	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
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
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot))
				{
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true) && pLoopPlot->isVisibleEnemyUnit(this)))
					{
						if (AI_potentialEnemy(pLoopPlot->getTeam(), pLoopPlot))
						{
							if (!atPlot(pLoopPlot) && ((bFollow) ? canMoveInto(pLoopPlot, /*bAttack*/ true, /*bDeclareWar*/ true) : (generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= iRange))))
							{
								iValue = getGroup()->AI_compareStacks(pLoopPlot, /*bPotentialEnemy*/ true, /*bCheckCanAttack*/ true, /*bCheckCanMove*/ true);

								if (iValue >= iPowerThreshold)
								{
									if (iValue > iBestValue)
									{
										iBestValue = iValue;
										pBestPlot = ((bFollow) ? pLoopPlot : getPathEndTurnPlot());
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), ((bFollow) ? MOVE_DIRECT_ATTACK : 0));
		return true;
	}

	return false;
}

bool CvUnitAI::AI_moveIntoCity(int iRange)
{
    PROFILE_FUNC();

	CvPlot* pLoopPlot;
	CvPlot* pBestPlot;
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
			pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iDX, iDY);
			
			if (pLoopPlot != nullptr)
			{
				if (AI_plotValid(pLoopPlot) && (!isEnemy(pLoopPlot->getTeam(), pLoopPlot)))
				{
					if (pLoopPlot->isCity() || (pLoopPlot->isCity(true)))
					{
                        if (canMoveInto(pLoopPlot, false) && (generatePath(pLoopPlot, 0, true, &iPathTurns) && (iPathTurns <= 1)))
                        {
                            iValue = 1;
                            if (pLoopPlot->getPlotCity() != nullptr)
                            {
                                 iValue += pLoopPlot->getPlotCity()->getPopulation();                                
                            }
                            
                            if (iValue > iBestValue)
                            {
                                iBestValue = iValue;
                                pBestPlot = getPathEndTurnPlot();
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
		getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
		return true;
	}

	return false;
}

//bolsters the culture of the weakest city.
//returns true if a mission is pushed.
bool CvUnitAI::AI_artistCultureVictoryMove()
{
    bool bGreatWork = false;
    bool bJoin = true;

    if (!GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_CULTURE1))
    {
        return false;        
    }
    
    if (GET_PLAYER(getOwnerINLINE()).AI_isDoStrategy(AI_STRATEGY_CULTURE3))
    {
        //Great Work
        bGreatWork = true;
    }
    
	int iCultureCitiesNeeded = GC.getGameINLINE().culturalVictoryNumCultureCities();
	FAssertMsg(iCultureCitiesNeeded > 0, "CultureVictory Strategy should not be true");

	CvCity* pLoopCity;
	CvPlot* pBestPlot;
	CvCity* pBestCity;
	SpecialistTypes eBestSpecialist;
	int iLoop, iValue, iBestValue;

	pBestPlot = nullptr;
	eBestSpecialist = NO_SPECIALIST;

	pBestCity = nullptr;
	
	iBestValue = 0;
	iLoop = 0;
	
	int iTargetCultureRank = iCultureCitiesNeeded;
	while (iTargetCultureRank > 0 && pBestCity == nullptr)
	{
		for (pLoopCity = GET_PLAYER(getOwnerINLINE()).firstCity(&iLoop); pLoopCity != nullptr; pLoopCity = GET_PLAYER(getOwnerINLINE()).nextCity(&iLoop))
		{
			if (AI_plotValid(pLoopCity->plot()))
			{
				// instead of commerce rate rank should use the culture on tile...
				if (pLoopCity->findCommerceRateRank(COMMERCE_CULTURE) == iTargetCultureRank)
				{
					// if the city is a fledgling, probably building culture, try next higher city
					if (std::to_underlying(pLoopCity->getCultureLevel()) < 2)
					{
						break;
					}
					
					// if we cannot path there, try the next higher culture city
					if (!generatePath(pLoopCity->plot(), 0, true))
					{
						break;
					}

					pBestCity = pLoopCity;
					pBestPlot = pLoopCity->plot();
					if (bGreatWork)
					{
						if (canGreatWork(pBestPlot))
						{
							break;
						}
					}

					for (int iI = 0; iI < GC.getNumSpecialistInfos(); iI++)
					{
						if (canJoin(pBestPlot, ((SpecialistTypes)iI)))
						{
							iValue = pLoopCity->AI_specialistValue(((SpecialistTypes)iI), pLoopCity->AI_avoidGrowth(), false);

							if (iValue > iBestValue)
							{
								iBestValue = iValue;
								eBestSpecialist = ((SpecialistTypes)iI);
							}
						}
					}

					if (eBestSpecialist == NO_SPECIALIST)
					{
						bJoin = false;
						if (canGreatWork(pBestPlot))
						{
							bGreatWork = true; 
							break;                        
						}
						bGreatWork = false;
					}
					break;
				}
			}
		}
	
		iTargetCultureRank--;
	}


	FAssertMsg(bGreatWork || bJoin, "This wasn't a Great Artist");
	
	if (pBestCity == nullptr)
	{
	    //should try to airlift there...
	    return false;
	}


    if (atPlot(pBestPlot))
    {
        if (bGreatWork)
        {
            getGroup()->pushMission(MISSION_GREAT_WORK);
            return true;
        }
        if (bJoin)
        {
            getGroup()->pushMission(MISSION_JOIN, eBestSpecialist);
            return true;
        }
        FAssert(false);
        return false;		    
    }
    else
    {
        FAssert(!atPlot(pBestPlot));
        getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE());
        return true;
    }
}    

bool CvUnitAI::AI_poach()
{
	CvPlot* pLoopPlot;
	int iX, iY;
	
	int iBestPoachValue = 0;
	CvPlot* pBestPoachPlot = nullptr;
	TeamTypes eBestPoachTeam = NO_TEAM;
	
	if (!GC.getGameINLINE().isOption(GAMEOPTION_AGGRESSIVE_AI))
	{
		return false;
	}
	
	if (GET_TEAM(getTeam()).getNumMembers() > 1)
	{
		return false;
	}
	
	int iNoPoachRoll = GET_PLAYER(getOwnerINLINE()).AI_totalUnitAIs(UNITAI_WORKER);
	iNoPoachRoll += GET_PLAYER(getOwnerINLINE()).getNumCities();
	iNoPoachRoll = std::max(0, (iNoPoachRoll - 1) / 2);
	if (GC.getGameINLINE().getSorenRandNum(iNoPoachRoll, "AI Poach") > 0)
	{
		return false;
	}

	if (GET_TEAM(getTeam()).getAnyWarPlanCount(true) > 0)
	{
		return false;
	}
	
	FAssert(canAttack());
	
	
	
	int iRange = 1;
	//Look for a unit which is non-combat
	//and has a capture unit type
	for (iX = -iRange; iX <= iRange; iX++)
	{
		for (iY = -iRange; iY <= iRange; iY++)
		{
			if (iX != 0 && iY != 0)
			{
				pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
				if ((pLoopPlot != nullptr) && (pLoopPlot->getTeam() != getTeam()) && pLoopPlot->isVisible(getTeam(), false))
				{
					int iPoachCount = 0;
					int iDefenderCount = 0;
					CvUnit* pPoachUnit = nullptr;
					CLLNode<IDInfo>* pUnitNode = pLoopPlot->headUnitNode();
					while (pUnitNode != nullptr)
					{
						CvUnit* pLoopUnit = ::getUnit(pUnitNode->m_data);
						pUnitNode = pLoopPlot->nextUnitNode(pUnitNode);
						if ((pLoopUnit->getTeam() != getTeam())
							&& GET_TEAM(getTeam()).canDeclareWar(pLoopUnit->getTeam()))
						{
							if (!pLoopUnit->canDefend())
							{
								if (pLoopUnit->getCaptureUnitType(getCivilizationType()) != NO_UNIT)
								{
									iPoachCount++;
									pPoachUnit = pLoopUnit;						
								}
							}
							else
							{
								iDefenderCount++;
							}
						}
					}
					
					if (pPoachUnit != nullptr)
					{
						if (iDefenderCount == 0)
						{
							int iValue = iPoachCount * 100;
							iValue -= iNoPoachRoll * 25;
							if (iValue > iBestPoachValue)
							{
								iBestPoachValue = iValue;
								pBestPoachPlot = pLoopPlot;
								eBestPoachTeam = pPoachUnit->getTeam();
							}
						}
					}
				}
			}
		}
	}
	
	if (pBestPoachPlot != nullptr)
	{
		//No war roll.
		if (!GET_TEAM(getTeam()).AI_performNoWarRolls(eBestPoachTeam))
		{
			GET_TEAM(getTeam()).declareWar(eBestPoachTeam, true, WARPLAN_LIMITED);
			
			FAssert(!atPlot(pBestPoachPlot));
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPoachPlot->getX_INLINE(), pBestPoachPlot->getY_INLINE(), MOVE_DIRECT_ATTACK);
			return true;
		}
		
	}
	
	return false;
}

bool CvUnitAI::AI_choke(int iRange)
{
	CvPlot* pBestPlot = nullptr;
	[[maybe_unused]] int iBestValue = 0;
	for (int iX = -iRange; iX <= iRange; iX++)
	{
		for (int iY = -iRange; iY <= iRange; iY++)
		{
			CvPlot* pLoopPlot = plotXY(getX_INLINE(), getY_INLINE(), iX, iY);
			if (pLoopPlot != nullptr)
			{
				if (isEnemy(pLoopPlot->getTeam()))
				{
					CvCity* pWorkingCity = pLoopPlot->getWorkingCity();
					if ((pWorkingCity != nullptr) && (pWorkingCity->getTeam() == pLoopPlot->getTeam()))
					{
						int iValue = -15;
						if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
						{
							iValue += GET_PLAYER(pLoopPlot->getOwnerINLINE()).AI_bonusVal(pLoopPlot->getBonusType(), 0);
						}
						
						iValue += pLoopPlot->getYield(YIELD_PRODUCTION) * 10;
						iValue += pLoopPlot->getYield(YIELD_FOOD) * 10;
						iValue += pLoopPlot->getYield(YIELD_COMMERCE) * 5;
						
						if (noDefensiveBonus())
						{
							iValue *= std::max(0, ((baseCombatStr() * 120) - GC.getGame().getBestLandUnitCombat()));
						}
						else
						{
							iValue *= pLoopPlot->defenseModifier(getTeam(), false);
						}
						
						if (iValue > 0)
						{
							iValue *= 10;
							
							iValue /= std::max(1, (pLoopPlot->getNumDefenders(getOwnerINLINE()) + ((pLoopPlot == plot()) ? 0 : 1)));
							
							if (generatePath(pLoopPlot))
							{
								pBestPlot = getPathEndTurnPlot();
								iBestValue = iValue;
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
			getGroup()->pushMission(MISSION_SKIP);
			return true;
		}
		else
		{
			getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX(), pBestPlot->getY());
			return true;
		}
	}
	
		
	
	return false;
}

bool CvUnitAI::AI_solveBlockageProblem(CvPlot* pDestPlot, bool bDeclareWar)
{
	FAssert(pDestPlot != nullptr);
	

	if (pDestPlot != nullptr)
	{
		FAStarNode* pStepNode;
		
		CvPlot* pSourcePlot = plot();

		if (gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), pSourcePlot->getX_INLINE(), pSourcePlot->getY_INLINE(), pDestPlot->getX_INLINE(), pDestPlot->getY_INLINE(), false, 0, true))
		{
			pStepNode = gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder());

			while (pStepNode != nullptr)
			{
				CvPlot* pStepPlot = GC.getMapINLINE().plotSorenINLINE(pStepNode->m_iX, pStepNode->m_iY);
				if (canMoveOrAttackInto(pStepPlot) && generatePath(pStepPlot, 0, true))
				{
					if (bDeclareWar && pStepNode->m_pPrev != nullptr)
					{
						CvPlot* pPlot = GC.getMapINLINE().plotSorenINLINE(pStepNode->m_pPrev->m_iX, pStepNode->m_pPrev->m_iY);
						if (pPlot->getTeam() != NO_TEAM)
						{
							if (!canMoveInto(pPlot, true, true))
							{
								if (!isPotentialEnemy(pPlot->getTeam(), pPlot))
								{									
									CvTeamAI& kTeam = GET_TEAM(getTeam());
									if (kTeam.canDeclareWar(pPlot->getTeam()))
									{
										WarPlanTypes eWarPlan = WARPLAN_LIMITED;
										WarPlanTypes eExistingWarPlan = kTeam.AI_getWarPlan(pDestPlot->getTeam());
										if (eExistingWarPlan != NO_WARPLAN)
										{
											if ((eExistingWarPlan == WARPLAN_TOTAL) || (eExistingWarPlan == WARPLAN_PREPARING_TOTAL))
											{
												eWarPlan = WARPLAN_TOTAL;
											}
											
											if (!kTeam.isAtWar(pDestPlot->getTeam()))
											{
												kTeam.AI_setWarPlan(pDestPlot->getTeam(), NO_WARPLAN);
											}
										}
										kTeam.AI_setWarPlan(pPlot->getTeam(), eWarPlan, true);
										return (AI_targetCity());
									}
								}
							}
						}
					}
					if (pStepPlot->isVisibleEnemyUnit(this))
					{
						FAssert(canAttack());
						CvPlot* pBestPlot = pStepPlot;
						//To prevent puppeteering attempt to barge through
						//if quite close
						if (getPathLastNode()->m_iData2 > 3)
						{
							pBestPlot = getPathEndTurnPlot();
						}

						FAssert(!atPlot(pBestPlot));
						getGroup()->pushMission(MISSION_MOVE_TO, pBestPlot->getX_INLINE(), pBestPlot->getY_INLINE(), MOVE_DIRECT_ATTACK);
						return true;						
					}
				}
				pStepNode = pStepNode->m_pParent;
			}
		}
	}
	
	return false;
}

int CvUnitAI::AI_calculatePlotWorkersNeeded(const CvPlot* pPlot, BuildTypes eBuild) const
{
	int iBuildTime = pPlot->getBuildTime(eBuild) - pPlot->getBuildProgress(eBuild);
	int iWorkRate = workRate(true);
	
	if (iWorkRate <= 0)
	{
		FAssert(false);
		return 1;
	}
	int iTurns = iBuildTime / iWorkRate;
	
	if (iBuildTime > (iTurns * iWorkRate))
	{
		iTurns++;
	}
	
	int iNeeded = std::max(1, (iTurns + 2) / 3);
	
	if (pPlot->getBonusType() != NO_BONUS)
	{
		iNeeded *= 2;		
	}
	return iNeeded;
	
}

bool CvUnitAI::AI_canGroupWithAIType(UnitAITypes eUnitAI) const
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



bool CvUnitAI::AI_allowGroup(const CvUnit* pUnit, UnitAITypes eUnitAI) const
{
	CvSelectionGroup* pGroup = pUnit->getGroup();
	CvPlot* pPlot = pUnit->plot();

	if (pUnit == this)
	{
		return false;
	}

	if (!pUnit->isGroupHead())
	{
		return false;
	}

	if (pGroup == getGroup())
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

	if (pGroup->getActivityType() == ACTIVITY_HEAL)
	{
		// do not attempt to join groups which are healing this turn
		// (healing is cleared every turn for automated groups, so we know we pushed a heal this turn)
		return false;
	}

	if (!canJoinGroup(pPlot, pGroup))
	{
		return false;
	}

	if (eUnitAI == UNITAI_SETTLE)
	{
		if (GET_PLAYER(getOwnerINLINE()).AI_getPlotDanger(pPlot, 3) > 0)
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

	if ((getGroup()->getHeadUnitAI() == UNITAI_CITY_DEFENSE))
	{
		if (plot()->isCity() && (plot()->getTeam() == getTeam()) && plot()->getBestDefender(getOwnerINLINE())->getGroup() == getGroup())
		{
			return false;
		}
	}

	if (plot()->getOwnerINLINE() == getOwnerINLINE())
	{
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
		if (getInvisibleType() == NO_INVISIBLE)
		{
			return false;
		}
	}

	return true;
}


void CvUnitAI::read(FDataStreamBase* pStream)
{
	CvUnit::read(pStream);

	unsigned int uiFlag=0;
	pStream->Read(&uiFlag);	// flags for expansion

	pStream->Read(&m_iBirthmark);

	pStream->Read((int*)&m_eUnitAIType);
	pStream->Read(&m_iAutomatedAbortTurn);
}


void CvUnitAI::write(FDataStreamBase* pStream)
{
	CvUnit::write(pStream);

	unsigned int uiFlag=0;
	pStream->Write(uiFlag);		// flag for expansion

	pStream->Write(m_iBirthmark);

	pStream->Write(m_eUnitAIType);
	pStream->Write(m_iAutomatedAbortTurn);
}

// Private Functions...
