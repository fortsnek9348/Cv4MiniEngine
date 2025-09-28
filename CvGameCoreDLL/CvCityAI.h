#pragma once

// cityAI.h

#ifndef CIV4_CITY_AI_H
#define CIV4_CITY_AI_H

#include "CvCity.h"
#include "CvDefines.h"

typedef std::vector<std::pair<UnitAITypes, int> > UnitTypeWeightArray;

class CvCityAI : public CvCity
{

public:

	CvCityAI();
	virtual ~CvCityAI();

	void AI_init() override;
	void AI_uninit();
	void AI_reset() override;

	void AI_doTurn() override;

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	void AI_doTurnParallel() override;
#endif

	void AI_assignWorkingPlots() override;
	void AI_updateAssignWork() override;

	bool AI_avoidGrowth() override;
	bool AI_ignoreGrowth();
	int AI_specialistValue(SpecialistTypes eSpecialist, bool bAvoidGrowth, bool bRemove) override;

	void AI_chooseProduction() override;

	UnitTypes AI_bestUnit(bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR, UnitAITypes* peBestUnitAI = nullptr) override;
	UnitTypes AI_bestUnitAI(UnitAITypes eUnitAI, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR) override;

	BuildingTypes AI_bestBuilding(int iFocusFlags = 0, int iMaxTurns = 0, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR) override;
	BuildingTypes AI_bestBuildingThreshold(int iFocusFlags = 0, int iMaxTurns = 0, int iMinThreshold = 0, bool bAsync = false, AdvisorTypes eIgnoreAdvisor = NO_ADVISOR);
	
	int AI_buildingValue(BuildingTypes eBuilding, int iFocusFlags = 0) override;
	int AI_buildingValueThreshold(BuildingTypes eBuilding, int iFocusFlags = 0, int iThreshold = 0);

	ProjectTypes AI_bestProject();
	int AI_projectValue(ProjectTypes eProject) override;

	ProcessTypes AI_bestProcess();
	ProcessTypes AI_bestProcess(CommerceTypes eCommerceType);
	int AI_processValue(ProcessTypes eProcess);
	int AI_processValue(ProcessTypes eProcess, CommerceTypes eCommerceType);

	int AI_neededSeaWorkers() override;

	// fortsnek: const
	bool AI_isDefended(int iExtra = 0) override;
	bool AI_isAirDefended(int iExtra = 0) override;
	// fortsnek: const
	bool AI_isDanger() const override;
	int AI_neededDefenders() override;
	int AI_neededAirDefenders() override;
	// fortsnek: const
	int AI_minDefenders() const override;
//#ifndef ENABLE_GAMECOREDLL_ENHANCEMENTS
	int AI_neededFloatingDefenders();
//#else
//	int AI_neededFloatingDefenders() const;
//#endif
	void AI_updateNeededFloatingDefenders();

	int AI_getEmphasizeAvoidGrowthCount();
	bool AI_isEmphasizeAvoidGrowth() override;

	int AI_getEmphasizeGreatPeopleCount();
	bool AI_isEmphasizeGreatPeople();

	bool AI_isAssignWorkDirty() override;
	void AI_setAssignWorkDirty(bool bNewValue) override;

	bool AI_isChooseProductionDirty() override;
	void AI_setChooseProductionDirty(bool bNewValue) override;

	CvCity* AI_getRouteToCity() const override;
	void AI_updateRouteToCity();

	int AI_getEmphasizeYieldCount(YieldTypes eIndex);
	bool AI_isEmphasizeYield(YieldTypes eIndex);

	int AI_getEmphasizeCommerceCount(CommerceTypes eIndex);
	bool AI_isEmphasizeCommerce(CommerceTypes eIndex);

	bool AI_isEmphasize(EmphasizeTypes eIndex) override;
	void AI_setEmphasize(EmphasizeTypes eIndex, bool bNewValue) override;
	void AI_forceEmphasizeCulture(bool bNewValue);

	// fortsnek: const
	int AI_getBestBuildValue(int iIndex) const override;
	int AI_totalBestBuildValue(CvArea* pArea) override;

	int AI_clearFeatureValue(int iIndex) override;
	// fortsnek: const
	BuildTypes AI_getBestBuild(int iIndex) const override;
	int AI_countBestBuilds(CvArea* pArea) override;
	void AI_updateBestBuild() override;

	virtual int AI_cityValue() const override;
    
    int AI_calculateWaterWorldPercent() override;
    
    int AI_getCityImportance(bool bEconomy, bool bMilitary);
    
    int AI_yieldMultiplier(YieldTypes eYield) override;
    void AI_updateSpecialYieldMultiplier();
    int AI_specialYieldMultiplier(YieldTypes eYield);
    
    int AI_countNumBonuses(BonusTypes eBonus, bool bIncludeOurs, bool bIncludeNeutral, int iOtherCultureThreshold, bool bLand = true, bool bWater = true) override;

	int AI_playerCloseness(PlayerTypes eIndex, int iMaxDistance) override;
	int AI_cityThreat(bool bDangerPercent = false) override;
	
	int AI_getWorkersHave() override;
	int AI_getWorkersNeeded() override;
	void AI_changeWorkersHave(int iChange) override;
	BuildingTypes AI_bestAdvancedStartBuilding(int iPass) override;
	
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	int m_iEmphasizeAvoidGrowthCount;
	int m_iEmphasizeGreatPeopleCount;

	bool m_bAssignWorkDirty;
	bool m_bChooseProductionDirty;

	IDInfo m_routeToCity;

	int* m_aiEmphasizeYieldCount;
	int* m_aiEmphasizeCommerceCount;
	bool m_bForceEmphasizeCulture;

	int m_aiBestBuildValue[NUM_CITY_PLOTS];

	BuildTypes m_aeBestBuild[NUM_CITY_PLOTS];

	bool* m_pbEmphasize;
	
	int* m_aiSpecialYieldMultiplier;
	
	int m_iCachePlayerClosenessTurn;
	int m_iCachePlayerClosenessDistance;
	int* m_aiPlayerCloseness;
	
	int m_iNeededFloatingDefenders;
	int m_iNeededFloatingDefendersCacheTurn;
	
	int m_iWorkersNeeded;
	int m_iWorkersHave;
	

	void AI_doDraft(bool bForce = false);
	void AI_doHurry(bool bForce = false);
	void AI_doEmphasize();
	int AI_getHappyFromHurry(HurryTypes eHurry);
	int AI_getHappyFromHurry(HurryTypes eHurry, UnitTypes eUnit, bool bIgnoreNew);
	int AI_getHappyFromHurry(HurryTypes eHurry, BuildingTypes eBuilding, bool bIgnoreNew);
	int AI_getHappyFromHurry(int iHurryPopulation);
	bool AI_doPanic();
	int AI_calculateCulturePressure(bool bGreatWork = false) override;

	
	bool AI_chooseUnit(UnitAITypes eUnitAI = NO_UNITAI);
	bool AI_chooseUnit(UnitTypes eUnit, UnitAITypes eUnitAI);
	
	bool AI_chooseDefender();
	bool AI_chooseLeastRepresentedUnit(UnitTypeWeightArray &allowedTypes);
	bool AI_chooseBuilding(int iFocusFlags = 0, int iMaxTurns = INT_MAX, int iMinThreshold = 0);
	bool AI_chooseProject();
	bool AI_chooseProcess(CommerceTypes eCommerceType = NO_COMMERCE);

	bool AI_bestSpreadUnit(bool bMissionary, bool bExecutive, int iBaseChance, UnitTypes* eBestSpreadUnit, int* iBestSpreadUnitValue);
	bool AI_addBestCitizen(bool bWorkers, bool bSpecialists, int* piBestPlot = nullptr, SpecialistTypes* peBestSpecialist = nullptr) override;
	bool AI_removeWorstCitizen(SpecialistTypes eIgnoreSpecialist = NO_SPECIALIST) override;
	void AI_juggleCitizens();

	bool AI_potentialPlot(short* piYields);
	bool AI_foodAvailable(int iExtra = 0);
	int AI_yieldValue(short* piYields, short* piCommerceYields, bool bAvoidGrowth, bool bRemove, bool bIgnoreFood = false, bool bIgnoreGrowth = false, bool bIgnoreStarvation = false, bool bWorkerOptimization = false);
	int AI_plotValue(CvPlot* pPlot, bool bAvoidGrowth, bool bRemove, bool bIgnoreFood = false, bool bIgnoreGrowth = false, bool bIgnoreStarvation = false);

	int AI_experienceWeight();
	int AI_buildUnitProb();

	void AI_bestPlotBuild(CvPlot* pPlot, int* piBestValue, BuildTypes* peBestBuild, int iFoodPriority, int iProductionPriority, int iCommercePriority, bool bChop, int iHappyAdjust, int iHealthAdjust, int iFoodChange);
	
	void AI_buildGovernorChooseProduction();
	
	int AI_getYieldMagicValue(const int* piYieldsTimes100, bool bHealthy);
	int AI_getPlotMagicValue(CvPlot* pPlot, bool bHealthy, bool bWorkerOptimization = false);
	int AI_countGoodTiles(bool bHealthy, bool bUnworkedOnly, int iThreshold = 50, bool bWorkerOptimization = false);
	int AI_countGoodSpecialists(bool bHealthy);
	int AI_calculateTargetCulturePerTurn();
	
	void AI_stealPlots();
	
	int AI_buildingSpecialYieldChangeValue(BuildingTypes kBuilding, YieldTypes eYield);
	
	void AI_cachePlayerCloseness(int iMaxDistance);
	void AI_updateWorkersNeededHere();

	// added so under cheat mode we can call protected functions for testing
	friend class CvGameTextMgr;
};

#endif
