#pragma once

// playerAI.h

#ifndef CIV4_PLAYER_AI_H
#define CIV4_PLAYER_AI_H

#include "CvPlayer.h"
#include "AI_Defines.h"

class CvEventTriggerInfo;

class CvPlayerAI : public CvPlayer
{

public:

	CvPlayerAI();
	virtual ~CvPlayerAI();

  // inlined for performance reasons
#ifdef _USRDLL
  static CvPlayerAI& getPlayer(PlayerTypes ePlayer) 
  {
	  FAssertMsg(ePlayer != NO_PLAYER, "Player is not assigned a valid value");
	  FAssertMsg(std::to_underlying(ePlayer) < MAX_PLAYERS, "Player is not assigned a valid value");
	  return m_aPlayers[ePlayer]; 
  }
#endif
	DllExport static CvPlayerAI& getPlayerNonInl(PlayerTypes ePlayer);

	static void initStatics();
	static void freeStatics();
	DllExport static bool areStaticsInitialized();

	void AI_init() override;
	void AI_uninit();
	void AI_reset(bool bConstructor) override;

	int AI_getFlavorValue(FlavorTypes eFlavor) const;

	void AI_doTurnPre() override;
	void AI_doTurnPost() override;
	void AI_doTurnUnitsPre() override;
	void AI_doTurnUnitsPost() override;

	void AI_doPeace();

	void AI_updateFoundValues(bool bStartingLoc = false) const override;
	void AI_updateAreaTargets();

	int AI_movementPriority(CvSelectionGroup* pGroup) const;
	void AI_unitUpdate() override;

	void AI_makeAssignWorkDirty() override;
	void AI_assignWorkingPlots() override;
	void AI_updateAssignWork() override;

	void AI_makeProductionDirty() override;

	void AI_conquerCity(CvCity* pCity) override;

	bool AI_acceptUnit(CvUnit* pUnit) const;
	bool AI_captureUnit(UnitTypes eUnit, CvPlot* pPlot) const;

	DomainTypes AI_unitAIDomainType(UnitAITypes eUnitAI) const;

	int AI_yieldWeight(YieldTypes eYield) const;
	int AI_commerceWeight(CommerceTypes eCommerce, CvCity* pCity = nullptr) const;

	int AI_foundValue(int iX, int iY, int iMinRivalRange = -1, bool bStartingLoc = false
#if ENABLE_GAMECOREDLL_ENHANCEMENTS
		, int startingPlotRangeOverride = -1
#endif
	) const override;

	bool AI_isAreaAlone(CvArea* pArea) const;
	bool AI_isCapitalAreaAlone() const;
	bool AI_isPrimaryArea(CvArea* pArea) const;

	int AI_militaryWeight(CvArea* pArea) const;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	int AI_targetCityValue(const CvCity* pCity, bool bRandomize, bool bIgnoreAttackers = false, bool bOverestimate = false) const;
#else
	int AI_targetCityValue(CvCity* pCity, bool bRandomize, bool bIgnoreAttackers = false) const;
#endif
	CvCity* AI_findTargetCity(CvArea* pArea) const;

	bool AI_isCommercePlot(CvPlot* pPlot) const override;
	// fortsnek: const
	int AI_getPlotDanger(const CvPlot* pPlot, int iRange = -1, bool bTestMoves = true) const override;
	int AI_getUnitDanger(CvUnit* pUnit, int iRange = -1, bool bTestMoves = true, bool bAnyDanger = true) const;
	int AI_getWaterDanger(CvPlot* pPlot, int iRange, bool bTestMoves = true) const;

	bool AI_avoidScience() const;
	bool AI_isFinancialTrouble() const override;
	int AI_goldTarget() const;

	TechTypes AI_bestTech(int iMaxPathLength = 1, bool bIgnoreCost = false, bool bAsync = false, TechTypes eIgnoreTech = NO_TECH, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR) const override;
	void AI_chooseFreeTech() override;
	void AI_chooseResearch() override;

	DllExport DiploCommentTypes AI_getGreeting(PlayerTypes ePlayer) const;
	bool AI_isWillingToTalk(PlayerTypes ePlayer) const override;
	bool AI_demandRebukedSneak(PlayerTypes ePlayer) const override;
	bool AI_demandRebukedWar(PlayerTypes ePlayer) const override;
	bool AI_hasTradedWithTeam(TeamTypes eTeam) const;

	AttitudeTypes AI_getAttitude(PlayerTypes ePlayer, bool bForced = true) const override;
	int AI_getAttitudeVal(PlayerTypes ePlayer, bool bForced = true) const;
	static AttitudeTypes AI_getAttitudeFromValue(int iAttitudeVal);

	int AI_calculateStolenCityRadiusPlots(PlayerTypes ePlayer) const;
	int AI_getCloseBordersAttitude(PlayerTypes ePlayer) const;

	int AI_getWarAttitude(PlayerTypes ePlayer) const;
	int AI_getPeaceAttitude(PlayerTypes ePlayer) const;
	int AI_getSameReligionAttitude(PlayerTypes ePlayer) const;
	int AI_getDifferentReligionAttitude(PlayerTypes ePlayer) const;
	int AI_getBonusTradeAttitude(PlayerTypes ePlayer) const;
	int AI_getOpenBordersAttitude(PlayerTypes ePlayer) const;
	int AI_getDefensivePactAttitude(PlayerTypes ePlayer) const;
	int AI_getRivalDefensivePactAttitude(PlayerTypes ePlayer) const;
	int AI_getRivalVassalAttitude(PlayerTypes ePlayer) const;
	int AI_getShareWarAttitude(PlayerTypes ePlayer) const;
	int AI_getFavoriteCivicAttitude(PlayerTypes ePlayer) const;
	int AI_getTradeAttitude(PlayerTypes ePlayer) const;
	int AI_getRivalTradeAttitude(PlayerTypes ePlayer) const;
	int AI_getMemoryAttitude(PlayerTypes ePlayer, MemoryTypes eMemory) const;
	int AI_getColonyAttitude(PlayerTypes ePlayer) const;

	PlayerVoteTypes AI_diploVote(const VoteSelectionSubData& kVoteData, VoteSourceTypes eVoteSource, bool bPropose) override;

	int AI_dealVal(PlayerTypes ePlayer, const CLinkList<TradeData>* pList, bool bIgnoreAnnual = false, int iExtra = 1) const override;
	bool AI_goldDeal(const CLinkList<TradeData>* pList) const;
	bool AI_considerOffer(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, int iChange = 1) const override;
	bool AI_counterPropose(PlayerTypes ePlayer, const CLinkList<TradeData>* pTheirList, const CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirInventory, CLinkList<TradeData>* pOurInventory, CLinkList<TradeData>* pTheirCounter, CLinkList<TradeData>* pOurCounter) const override;

	DllExport int AI_maxGoldTrade(PlayerTypes ePlayer) const override;

	DllExport int AI_maxGoldPerTurnTrade(PlayerTypes ePlayer) const override;
	int AI_goldPerTurnTradeVal(int iGoldPerTurn) const;

	int AI_bonusVal(BonusTypes eBonus, int iChange = 1) const override;
	int AI_baseBonusVal(BonusTypes eBonus) const;
	int AI_bonusTradeVal(BonusTypes eBonus, PlayerTypes ePlayer, int iChange) const override;
	DenialTypes AI_bonusTrade(BonusTypes eBonus, PlayerTypes ePlayer) const override;
	int AI_corporationBonusVal(BonusTypes eBonus) const;

	int AI_cityTradeVal(CvCity* pCity) const override;
	DenialTypes AI_cityTrade(CvCity* pCity, PlayerTypes ePlayer) const override;

	int AI_stopTradingTradeVal(TeamTypes eTradeTeam, PlayerTypes ePlayer) const;
	DenialTypes AI_stopTradingTrade(TeamTypes eTradeTeam, PlayerTypes ePlayer) const override;

	int AI_civicTradeVal(CivicTypes eCivic, PlayerTypes ePlayer) const;
	DenialTypes AI_civicTrade(CivicTypes eCivic, PlayerTypes ePlayer) const override;

	int AI_religionTradeVal(ReligionTypes eReligion, PlayerTypes ePlayer) const;
	DenialTypes AI_religionTrade(ReligionTypes eReligion, PlayerTypes ePlayer) const override;

	int AI_unitImpassableCount(UnitTypes eUnit) const;
	int AI_unitValue(UnitTypes eUnit, UnitAITypes eUnitAI, CvArea* pArea) const override;
	int AI_totalUnitAIs(UnitAITypes eUnitAI) const override;
	int AI_totalAreaUnitAIs(CvArea* pArea, UnitAITypes eUnitAI) const override;
	int AI_totalWaterAreaUnitAIs(CvArea* pArea, UnitAITypes eUnitAI) const override;
	int AI_countCargoSpace(UnitAITypes eUnitAI) const;

	int AI_neededExplorers(CvArea* pArea) const;
	int AI_neededWorkers(CvArea* pArea) const;
	int AI_neededMissionaries(CvArea* pArea, ReligionTypes eReligion) const;
	int AI_neededExecutives(CvArea* pArea, CorporationTypes eCorporation) const;
	
	int AI_missionaryValue(CvArea* pArea, ReligionTypes eReligion, PlayerTypes* peBestPlayer = nullptr) const;
	int AI_executiveValue(CvArea* pArea, CorporationTypes eCorporation, PlayerTypes* peBestPlayer = nullptr) const;
	
	int AI_corporationValue(CorporationTypes eCorporation, CvCity* pCity = nullptr) const;
	
	// fortsnek: Lots of const
	int AI_adjacentPotentialAttackers(const CvPlot* pPlot, bool bTestCanMove = false) const;
	// fortsnek: Lots of const
	int AI_totalMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	int AI_areaMissionAIs(const CvArea* pArea, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	int AI_plotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr, int iRange = 0) const override;
	int AI_plotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, int& iClosestTargetRange, const CvSelectionGroup* pSkipSelectionGroup = nullptr, int iRange = 0) const;
	int AI_plotTargetMissionAIs(const CvPlot* pPlot, const MissionAITypes* aeMissionAI, int iMissionAICount, int& iClosestTargetRange, const CvSelectionGroup* pSkipSelectionGroup = nullptr, int iRange = 0) const;
	int AI_unitTargetMissionAIs(const CvUnit* pUnit, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const override;
	int AI_unitTargetMissionAIs(const CvUnit* pUnit, const MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	int AI_enemyTargetMissionAIs(MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	int AI_enemyTargetMissionAIs(MissionAITypes* aeMissionAI, int iMissionAICount, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	int AI_wakePlotTargetMissionAIs(const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr) const;
	

	CivicTypes AI_bestCivic(CivicOptionTypes eCivicOption) const;
	int AI_civicValue(CivicTypes eCivic) const override;

	ReligionTypes AI_bestReligion() const;
	int AI_religionValue(ReligionTypes eReligion) const;

	EspionageMissionTypes AI_bestPlotEspionage(CvPlot* pSpyPlot, PlayerTypes& eTargetPlayer, CvPlot*& pPlot, int& iData) const;
	int AI_espionageVal(PlayerTypes eTargetPlayer, EspionageMissionTypes eMission, CvPlot* pPlot, int iData) const;

	int AI_getPeaceWeight() const;
	void AI_setPeaceWeight(int iNewValue);

	int AI_getEspionageWeight() const;
	void AI_setEspionageWeight(int iNewValue);

	int AI_getAttackOddsChange() const;
	void AI_setAttackOddsChange(int iNewValue);

	int AI_getCivicTimer() const;
	void AI_setCivicTimer(int iNewValue);
	void AI_changeCivicTimer(int iChange);

	int AI_getReligionTimer() const;
	void AI_setReligionTimer(int iNewValue);
	void AI_changeReligionTimer(int iChange);

	int AI_getExtraGoldTarget() const override;
	void AI_setExtraGoldTarget(int iNewValue) override;

	int AI_getNumTrainAIUnits(UnitAITypes eIndex) const;
	void AI_changeNumTrainAIUnits(UnitAITypes eIndex, int iChange);

	int AI_getNumAIUnits(UnitAITypes eIndex) const override;
	void AI_changeNumAIUnits(UnitAITypes eIndex, int iChange);

	int AI_getSameReligionCounter(PlayerTypes eIndex) const;
	void AI_changeSameReligionCounter(PlayerTypes eIndex, int iChange);

	int AI_getDifferentReligionCounter(PlayerTypes eIndex) const;
	void AI_changeDifferentReligionCounter(PlayerTypes eIndex, int iChange);

	int AI_getFavoriteCivicCounter(PlayerTypes eIndex) const;
	void AI_changeFavoriteCivicCounter(PlayerTypes eIndex, int iChange);

	int AI_getBonusTradeCounter(PlayerTypes eIndex) const;
	void AI_changeBonusTradeCounter(PlayerTypes eIndex, int iChange);

	int AI_getPeacetimeTradeValue(PlayerTypes eIndex) const;
	void AI_changePeacetimeTradeValue(PlayerTypes eIndex, int iChange) override;

	int AI_getPeacetimeGrantValue(PlayerTypes eIndex) const;
	void AI_changePeacetimeGrantValue(PlayerTypes eIndex, int iChange) override;

	int AI_getGoldTradedTo(PlayerTypes eIndex) const;
	void AI_changeGoldTradedTo(PlayerTypes eIndex, int iChange);

	int AI_getAttitudeExtra(PlayerTypes eIndex) const override;
	void AI_setAttitudeExtra(PlayerTypes eIndex, int iNewValue) override;
	void AI_changeAttitudeExtra(PlayerTypes eIndex, int iChange) override;

	bool AI_isFirstContact(PlayerTypes eIndex) const;
	void AI_setFirstContact(PlayerTypes eIndex, bool bNewValue) override;

	int AI_getContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2) const;
	void AI_changeContactTimer(PlayerTypes eIndex1, ContactTypes eIndex2, int iChange);

	int AI_getMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2) const override;
	void AI_changeMemoryCount(PlayerTypes eIndex1, MemoryTypes eIndex2, int iChange) override;

	int AI_calculateGoldenAgeValue() const;

	void AI_doCommerce() override;

	EventTypes AI_chooseEvent(int iTriggeredId) const override;
	virtual void AI_launch(VictoryTypes eVictory) override;

	int AI_getCultureVictoryStage() const;
	
	int AI_cultureVictoryTechValue(TechTypes eTech) const;
	
	bool AI_isDoStrategy(int iStrategy) const;
	void AI_forceUpdateStrategies();

	void AI_nowHasTech(TechTypes eTech);
	
	// fortsnek: const plot
    int AI_countDeadlockedBonuses(const CvPlot* pPlot) const;
    
    int AI_getOurPlotStrength(CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const;
    int AI_getEnemyPlotStrength(CvPlot* pPlot, int iRange, bool bDefensiveBonuses, bool bTestMoves) const;

	int AI_goldToUpgradeAllUnits(int iExpThreshold = 0) const;

	int AI_goldTradeValuePercent() const;
	
	int AI_averageYieldMultiplier(YieldTypes eYield) const;
	int AI_averageCommerceMultiplier(CommerceTypes eCommerce) const;
	int AI_averageGreatPeopleMultiplier() const;
	int AI_averageCommerceExchange(CommerceTypes eCommerce) const;
	
	int AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance) const;
	
	//int AI_getTotalCityThreat() const;
	//int AI_getTotalFloatingDefenseNeeded() const;
	
	
	int AI_getTotalAreaCityThreat(CvArea* pArea) const;
	int AI_countNumAreaHostileUnits(CvArea* pArea, bool bPlayer, bool bTeam, bool bNeutral, bool bHostile) const;
	int AI_getTotalFloatingDefendersNeeded(CvArea* pArea) const;
	int AI_getTotalFloatingDefenders(CvArea* pArea) const;

	RouteTypes AI_bestAdvancedStartRoute(CvPlot* pPlot, int* piYieldValue = nullptr) const;
	UnitTypes AI_bestAdvancedStartUnitAI(CvPlot* pPlot, UnitAITypes eUnitAI) const;
	CvPlot* AI_advancedStartFindCapitalPlot() const;
	
	bool AI_advancedStartPlaceExploreUnits(bool bLand);
	void AI_advancedStartRevealRadius(CvPlot* pPlot, int iRadius);
	bool AI_advancedStartPlaceCity(CvPlot* pPlot);
	bool AI_advancedStartDoRoute(CvPlot* pFromPlot, CvPlot* pToPlot);
	void AI_advancedStartRouteTerritory();
	void AI_doAdvancedStart(bool bNoExit = false) override;
	
	int AI_getMinFoundValue() const;
	
//#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
	void AI_recalculateFoundValues(int iX, int iY, int iInnerRadius, int iOuterRadius) const;
//#endif
	
	void AI_updateCitySites(int iMinFoundValueThreshold, int iMaxSites) const;
	void AI_invalidateCitySites(int iMinFoundValueThreshold) const;
	// fortsnek: const plot
	bool AI_isPlotCitySite(const CvPlot* pPlot) const;
	int AI_getNumAreaCitySites(int iAreaID, int& iBestValue) const;
	int AI_getNumAdjacentAreaCitySites(int iWaterAreaID, int iExcludeArea, int& iBestValue) const;
	
	int AI_getNumCitySites() const;
	CvPlot* AI_getCitySite(int iIndex) const;
	
	int AI_bestAreaUnitAIValue(UnitAITypes eUnitAI, CvArea* pArea, UnitTypes* peBestUnitType = nullptr) const;
	int AI_bestCityUnitAIValue(UnitAITypes eUnitAI, CvCity* pCity, UnitTypes* peBestUnitType = nullptr) const;
	
	int AI_calculateTotalBombard(DomainTypes eDomain) const;
	
	int AI_getUnitClassWeight(UnitClassTypes eUnitClass) const;
	int AI_getUnitCombatWeight(UnitCombatTypes eUnitCombat) const;
	int AI_calculateUnitAIViability(UnitAITypes eUnitAI, DomainTypes eDomain) const;
	
	void AI_updateBonusValue() override;
	void AI_updateBonusValue(BonusTypes eBonus) override;
	
	int AI_getAttitudeWeight(PlayerTypes ePlayer) const;

	ReligionTypes AI_chooseReligion() override;
	
	// fortsnek: const
	int AI_getPlotAirbaseValue(const CvPlot* pPlot) const;
	int AI_getPlotCanalValue(CvPlot* pPlot) const;

	// fortsnek: const
	bool AI_isPlotThreatened(const CvPlot* pPlot, int iRange = -1, bool bTestMoves = true) const;

	bool AI_isFirstTech(TechTypes eTech) const;

	// for serialization
  virtual void read(FDataStreamBase* pStream) override;
  virtual void write(FDataStreamBase* pStream) override;

protected:

	static CvPlayerAI* m_aPlayers;

	int m_iPeaceWeight;
	int m_iEspionageWeight;
	int m_iAttackOddsChange;
	int m_iCivicTimer;
	int m_iReligionTimer;
	int m_iExtraGoldTarget;
	
	mutable int m_iStrategyHash;
	mutable int m_iStrategyHashCacheTurn;
	
	
	mutable int m_iAveragesCacheTurn;
	
	mutable int m_iAverageGreatPeopleMultiplier;
	
	mutable int *m_aiAverageYieldMultiplier;
	mutable int *m_aiAverageCommerceMultiplier;
	mutable int *m_aiAverageCommerceExchange;
	
	mutable int m_iUpgradeUnitsCacheTurn;
	mutable int m_iUpgradeUnitsCachedExpThreshold;
	mutable int m_iUpgradeUnitsCachedGold;
	
	
	int *m_aiNumTrainAIUnits;
	int *m_aiNumAIUnits;
	int* m_aiSameReligionCounter;
	int* m_aiDifferentReligionCounter;
	int* m_aiFavoriteCivicCounter;
	int* m_aiBonusTradeCounter;
	int* m_aiPeacetimeTradeValue;
	int* m_aiPeacetimeGrantValue;
	int* m_aiGoldTradedTo;
	int* m_aiAttitudeExtra;
	int* m_aiBonusValue;
	int* m_aiUnitClassWeights;
	int* m_aiUnitCombatWeights;

	mutable int* m_aiCloseBordersAttitudeCache;


	bool* m_abFirstContact;

	int** m_aaiContactTimer;
	int** m_aaiMemoryCount;
	
	mutable std::vector<int> m_aiAICitySites;
	
	bool m_bWasFinancialTrouble;
	int m_iTurnLastProductionDirty;

	void AI_doCounter();
	void AI_doMilitary();
	void AI_doResearch();
	void AI_doCivics();
	void AI_doReligion();
	void AI_doDiplo();
	void AI_doSplit();
	void AI_doCheckFinancialTrouble();
	
	bool AI_disbandUnit(int iExpThreshold, bool bObsolete);
	
	int AI_getStrategyHash() const;
	void AI_calculateAverages() const;
	
	int AI_getHappinessWeight(int iHappy, int iExtraPop) const;
	int AI_getHealthWeight(int iHealth, int iExtraPop) const;
	
	void AI_convertUnitAITypesForCrush();
	int AI_eventValue(EventTypes eEvent, const EventTriggeredData& kTriggeredData) const;
		
	void AI_doEnemyUnitData();
	void AI_invalidateCloseBordersAttitudeCache();
	
	friend class CvGameTextMgr;
};

// helper for accessing static functions
#ifdef _USRDLL
#define GET_PLAYER CvPlayerAI::getPlayer
#else
#define GET_PLAYER CvPlayerAI::getPlayerNonInl
#endif

#endif
