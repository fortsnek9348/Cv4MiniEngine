#pragma once

// unitAI.h

#ifndef CIV4_UNIT_AI_H
#define CIV4_UNIT_AI_H

#include "CvUnit.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
namespace GiganticMapsOptimisationsLib
{
	struct DryRunUnitState;
	class UnitPostUpdateQueue;
}
#endif

class CvCity;

class CvUnitAI : public CvUnit
{

public:

	CvUnitAI();
	virtual ~CvUnitAI();



	virtual void AI_init(UnitAITypes eUnitAI) override;
	virtual void AI_uninit() override;
	virtual void AI_reset(UnitAITypes eUnitAI = NO_UNITAI) override;
	virtual bool AI_update() override;
	virtual bool AI_follow() override;
	virtual void AI_upgrade() override;
	virtual void AI_promote() override;
	virtual int AI_groupFirstVal() override;
	virtual int AI_groupSecondVal() override;
	virtual int AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const override;
	virtual bool AI_bestCityBuild(CvCity* pCity, CvPlot** ppBestPlot = nullptr, BuildTypes* peBestBuild = nullptr, CvPlot* pIgnorePlot = nullptr, CvUnit* pUnit = nullptr) override;
	virtual bool AI_isCityAIType() const override;
	virtual UnitAITypes AI_getUnitAIType() const override;																				// Exposed to Python
	virtual void AI_setUnitAIType(UnitAITypes eNewValue) override;
	virtual int AI_sacrificeValue(const CvPlot* pPlot) const override;


	int AI_getBirthmark() const;
	void AI_setBirthmark(int iNewValue);

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	void applyParallelUnitUpdateResult(const GiganticMapsOptimisationsLib::DryRunUnitState& result,
		GiganticMapsOptimisationsLib::UnitPostUpdateQueue& postUpdateQueue);
#endif

	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	int m_iBirthmark;

	UnitAITypes m_eUnitAIType;

	int m_iAutomatedAbortTurn;

	void AI_animalMove();
	void AI_settleMove();
	void AI_workerMove();
	void AI_barbAttackMove();
	void AI_attackMove();
	void AI_attackCityMove();
	void AI_attackCityLemmingMove();
	void AI_collateralMove();
	void AI_pillageMove();
	void AI_reserveMove();
	void AI_counterMove();
	void AI_paratrooperMove();
	void AI_cityDefenseMove();
	void AI_cityDefenseExtraMove();
	void AI_exploreMove();
	void AI_missionaryMove();
	void AI_prophetMove();
	void AI_artistMove();
	void AI_scientistMove();
	void AI_generalMove();
	void AI_merchantMove();
	void AI_engineerMove();
	void AI_spyMove();
	void AI_ICBMMove();
	void AI_workerSeaMove();
	void AI_barbAttackSeaMove();
	void AI_pirateSeaMove();
	void AI_attackSeaMove();
	void AI_reserveSeaMove();
	void AI_escortSeaMove();
	void AI_exploreSeaMove();
	void AI_assaultSeaMove();
	void AI_settlerSeaMove();
	void AI_missionarySeaMove();
	void AI_spySeaMove();
	void AI_carrierSeaMove();
	void AI_missileCarrierSeaMove();
	void AI_attackAirMove();
	void AI_defenseAirMove();
	void AI_carrierAirMove();
	void AI_missileAirMove();

	void AI_networkAutomated();
	void AI_cityAutomated();

	int AI_promotionValue(PromotionTypes ePromotion);

	bool AI_shadow(UnitAITypes eUnitAI, int iMax = -1, int iMaxRatio = -1, bool bWithCargoOnly = true);
	bool AI_group(UnitAITypes eUnitAI, int iMaxGroup = -1, int iMaxOwnUnitAI = -1, int iMinUnitAI = -1, bool bIgnoreFaster = false, bool bIgnoreOwnUnitType = false, bool bStackOfDoom = false, int iMaxPath = INT_MAX, bool bAllowRegrouping = false);
	bool AI_load(UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI = NO_UNITAI, int iMinCargo = -1, int iMinCargoSpace = -1, int iMaxCargoSpace = -1, int iMaxCargoOurUnitAI = -1, int iFlags = 0, int iMaxPath = INT_MAX);
	bool AI_guardCityBestDefender();
	bool AI_guardCityMinDefender(bool bSearch = true);
	bool AI_guardCity(bool bLeave = false, bool bSearch = false, int iMaxPath = INT_MAX);
	bool AI_guardCityAirlift();
	bool AI_guardBonus(int iMinValue = 0);
	// fortsnek: const
	int AI_getPlotDefendersNeeded(const CvPlot* pPlot, int iExtra) const;
	bool AI_guardFort(bool bSearch = true);
	bool AI_guardCitySite();
	bool AI_guardSpy(int iRandomPercent);
	bool AI_destroySpy();
	bool AI_sabotageSpy();
	bool AI_pickupTargetSpy();
	bool AI_chokeDefend();
	bool AI_heal(int iDamagePercent = 0, int iMaxPath = INT_MAX);
	bool AI_afterAttack();
	bool AI_goldenAge();
	bool AI_spreadReligion();
	bool AI_spreadCorporation();
	bool AI_spreadReligionAirlift();
	bool AI_spreadCorporationAirlift();
	bool AI_discover(bool bThisTurnOnly = false, bool bFirstResearchOnly = false);
	bool AI_lead(std::vector<UnitAITypes>& aeAIUnitTypes);
	bool AI_join(int iMaxCount = INT_MAX);
	bool AI_construct(int iMaxCount = INT_MAX, int iMaxSingleBuildingCount = INT_MAX, int iThreshold = 15);
	bool AI_switchHurry();
	bool AI_hurry();
	bool AI_greatWork();
	bool AI_offensiveAirlift();
	bool AI_paradrop(int iRange);
	bool AI_protect(int iOddsThreshold);
	bool AI_patrol();
	bool AI_defend();
	bool AI_safety();
	bool AI_hide();
	bool AI_goody(int iRange);
	bool AI_explore();
	bool AI_exploreRange(int iRange);
	bool AI_targetCity(int iFlags = 0);
	bool AI_targetBarbCity();
	bool AI_bombardCity();
	bool AI_cityAttack(int iRange, int iOddsThreshold, bool bFollow = false);
	bool AI_anyAttack(int iRange, int iOddsThreshold, int iMinStack = 0, bool bFollow = false);
	bool AI_rangeAttack(int iRange);
	bool AI_leaveAttack(int iRange, int iThreshold, int iStrengthThreshold);
	bool AI_blockade();
	bool AI_pirateBlockade();
	bool AI_seaBombardRange(int iMaxRange);
	bool AI_pillage(int iBonusValueThreshold = 0);
	bool AI_pillageRange(int iRange, int iBonusValueThreshold = 0);
	bool AI_found();
	bool AI_foundRange(int iRange, bool bFollow = false);
	bool AI_assaultSeaTransport(bool bBarbarian = false);
	bool AI_settlerSeaTransport();
	bool AI_settlerSeaFerry();
	bool AI_specialSeaTransportMissionary();
	bool AI_specialSeaTransportSpy();
	bool AI_carrierSeaTransport();
	bool AI_connectPlot(CvPlot* pPlot, int iRange = 0);
	bool AI_improveCity(CvCity* pCity);
	bool AI_improveLocalPlot(int iRange, CvCity* pIgnoreCity);
	bool AI_nextCityToImprove(CvCity* pCity);
	bool AI_nextCityToImproveAirlift();
	bool AI_irrigateTerritory();
	bool AI_fortTerritory(bool bCanal, bool bAirbase);
	bool AI_improveBonus(int iMinValue = 0, CvPlot** ppBestPlot = nullptr, BuildTypes* peBestBuild = nullptr, int* piBestValue = nullptr);
	bool AI_improvePlot(CvPlot* pPlot, BuildTypes eBuild);
	// fortsnek: const
	BuildTypes AI_betterPlotBuild(const CvPlot* pPlot, BuildTypes eBuild) const;
	bool AI_connectBonus(bool bTestTrade = true);
	bool AI_connectCity();
	bool AI_routeCity();
	bool AI_routeTerritory(bool bImprovementOnly = false);
	bool AI_travelToUpgradeCity();
	bool AI_retreatToCity(bool bPrimary = false, bool bAirlift = false, int iMaxPath = INT_MAX);
	bool AI_pickup(UnitAITypes eUnitAI);
	bool AI_airOffensiveCity();
	bool AI_airDefensiveCity();
	bool AI_airCarrier();
	bool AI_missileLoad(UnitAITypes eTargetUnitAI, int iMaxOwnUnitAI = -1, bool bStealthOnly = false);
	bool AI_airStrike();
	bool AI_airBombPlots();
	bool AI_airBombDefenses();	
	bool AI_exploreAir();
	bool AI_nuke();
	bool AI_nukeRange(int iRange);
	bool AI_trade(int iValueThreshold);
	bool AI_infiltrate();
	bool AI_reconSpy(int iRange);
	bool AI_bonusOffenseSpy(int iMaxPath);
	bool AI_cityOffenseSpy(int iRange);
	bool AI_espionageSpy();
	bool AI_moveToStagingCity();
	bool AI_seaRetreatFromCityDanger();
	bool AI_airRetreatFromCityDanger();
	bool AI_airAttackDamagedSkip();

	bool AI_followBombard();

	// fortsnek: const
	bool AI_potentialEnemy(TeamTypes eTeam, const CvPlot* pPlot = nullptr) const;

	bool AI_defendPlot(CvPlot* pPlot);
	// fortsnek: const
	int AI_pillageValue(const CvPlot* pPlot, int iBonusValueThreshold = 0) const;
	int AI_nukeValue(CvCity* pCity);
	bool AI_canPillage(CvPlot& kPlot) const;

	// fortsnek: const
	int AI_searchRange(int iRange) const;
	// fortsnek: const, public for parallel AI update
	bool AI_plotValid(const CvPlot* pPlot) const;
	// fortsnek: const
	int AI_finalOddsThreshold(const CvPlot* pPlot, int iOddsThreshold) const;
	// fortsnek: const
	int AI_stackOfDoomExtra() const;

	bool AI_stackAttackCity(int iRange, int iPowerThreshold, bool bFollow = true);
	bool AI_moveIntoCity(int iRange);

	bool AI_groupMergeRange(UnitAITypes eUnitAI, int iRange, bool bBiggerOnly = true, bool bAllowRegrouping = false, bool bIgnoreFaster = false);
	
	bool AI_artistCultureVictoryMove();

	bool AI_poach();
	bool AI_choke(int iRange = 1);
	
	bool AI_solveBlockageProblem(CvPlot* pDestPlot, bool bDeclareWar);
	
	// fortsnek: const
	int AI_calculatePlotWorkersNeeded(const CvPlot* pPlot, BuildTypes eBuild) const;

	int AI_getEspionageTargetValue(CvPlot* pPlot, int iMaxPath);

	bool AI_canGroupWithAIType(UnitAITypes eUnitAI) const;
	bool AI_allowGroup(const CvUnit* pUnit, UnitAITypes eUnitAI) const;

	// added so under cheat mode we can call protected functions for testing
	friend class CvGameTextMgr;

};

#endif
