#include "CvGameCoreDLL.h"
#include "CyGameCoreUtils.h"
#include "CyPlot.h"
#include "CyCity.h"
#include "CyUnit.h"

#include <pybind11/pybind11.h>

//
// Python interface for CvgameCoreUtils.h.
//

void CyGameCoreUtilsPythonInterface(pybind11::module m)
{
	//OutputDebugString("Python Extension Module - CyGameCoreUtilsPythonInterface\n");

	m.def("cyIntRange", cyIntRange,"int (int iNum, int iLow, int iHigh)");
	m.def("cyFloatRange", cyFloatRange,"float (float fNum, float fLow, float fHigh)");
	m.def("dxWrap", cyDxWrap,"int (int iDX)");
	m.def("dyWrap", cyDyWrap,"int (int iDY)");
	m.def("plotDistance", cyPlotDistance,"int (int iX1, int iY1, int iX2, int iY2)");
	m.def("stepDistance", cyStepDistance,"int (int iX1, int iY1, int iX2, int iY2)");
	m.def("plotDirection", cyPlotDirection, pybind11::return_value_policy::take_ownership, "CyPlot* (int iX, int iY, DirectionTypes eDirection)");
	m.def("plotCardinalDirection", cyPlotCardinalDirection, pybind11::return_value_policy::take_ownership, "CyPlot* (int iX, int iY, CardinalDirectionTypes eCardDirection)");
	m.def("splotCardinalDirection", cysPlotCardinalDirection, pybind11::return_value_policy::reference, "CyPlot* (int iX, int iY, CardinalDirectionTypes eCardDirection)");
	m.def("plotXY", cyPlotXY, pybind11::return_value_policy::take_ownership, "CyPlot* (int iX, int iY, int iDX, int iDY)");
	m.def("splotXY", cysPlotXY, pybind11::return_value_policy::reference, "CyPlot* (int iX, int iY, int iDX, int iDY)");
	m.def("directionXY", cyDirectionXYFromInt,"DirectionTypes (int iDX, int iDY)");
	m.def("directionXYFromPlot", cyDirectionXYFromPlot,"DirectionTypes (CyPlot* pFromPlot, CyPlot* pToPlot)");
	m.def("plotCity", cyPlotCity, pybind11::return_value_policy::take_ownership, "CyPlot* (int iX, int iY, int iIndex)");
	m.def("plotCityXY", cyPlotCityXYFromInt,"int (int iDX, int iDY)");
	m.def("plotCityXYFromCity", cyPlotCityXYFromCity,"int (CyCity* pCity, CyPlot* pPlot)");
	m.def("getOppositeCardinalDirection", cyGetOppositeCardinalDirection,"CardinalDirectionTypes (CardinalDirectionTypes eDir)");
	m.def("cardinalDirectionToDirection", cyCardinalDirectionToDirection, "DirectionTypes (CardinalDirectionTypes eDir) - converts a CardinalDirectionType to the corresponding DirectionType");

	m.def("isCardinalDirection", cyIsCardinalDirection,"bool (DirectionTypes eDirection)");
	m.def("estimateDirection", cyEstimateDirection, "DirectionTypes (int iDX, int iDY)");

	m.def("atWar", cyAtWar,"bool (int eTeamA, int eTeamB)");
	m.def("isPotentialEnemy", cyIsPotentialEnemy,"bool (int eOurTeam, int eTheirTeam)");

	m.def("getCity", cyGetCity, pybind11::return_value_policy::take_ownership, "CyPlot* (IDInfo city)");
	m.def("getUnit", cyGetUnit, pybind11::return_value_policy::take_ownership, "CyUnit* (IDInfo unit)");

	m.def("isPromotionValid", cyIsPromotionValid, "bool (int /*PromotionTypes*/ ePromotion, int /*UnitTypes*/ eUnit, bool bLeader)");
	m.def("getPopulationAsset", cyGetPopulationAsset, "int (int iPopulation)");
	m.def("getLandPlotsAsset", cyGetLandPlotsAsset, "int (int iLandPlots)");
	m.def("getPopulationPower", cyGetPopulationPower, "int (int iPopulation)");
	m.def("getPopulationScore", cyGetPopulationScore, "int (int iPopulation)");
	m.def("getLandPlotsScore", cyGetLandPlotsScore, "int (int iPopulation)");
	m.def("getTechScore", cyGetTechScore, "int (int /*TechTypes*/ eTech)");
	m.def("getWonderScore", cyGetWonderScore, "int (int /*BuildingClassTypes*/ eWonderClass)");
	m.def("finalImprovementUpgrade", cyFinalImprovementUpgrade, "int /*ImprovementTypes*/ (int /*ImprovementTypes*/ eImprovement, int iCount)");

	m.def("getWorldSizeMaxConscript", cyGetWorldSizeMaxConscript, "int (int /*CivicTypes*/ eCivic)");
	m.def("isReligionTech", cyIsReligionTech, "int (int /*TechTypes*/ eTech)");

	m.def("isTechRequiredForUnit", cyIsTechRequiredForUnit, "bool (int /*TechTypes*/ eTech, int /*UnitTypes*/ eUnit)");
	m.def("isTechRequiredForBuilding", cyIsTechRequiredForBuilding, "bool (int /*TechTypes*/ eTech, int /*BuildingTypes*/ eBuilding)");
	m.def("isTechRequiredForProject", cyIsTechRequiredForProject, "bool (int /*TechTypes*/ eTech, int /*ProjectTypes*/ eProject)");
	m.def("isWorldUnitClass", cyIsWorldUnitClass, "bool (int /*UnitClassTypes*/ eUnitClass)");
	m.def("isTeamUnitClass", cyIsTeamUnitClass, "bool (int /*UnitClassTypes*/ eUnitClass)");
	m.def("isNationalUnitClass", cyIsNationalUnitClass, "bool (int /*UnitClassTypes*/ eUnitClass)");
	m.def("isLimitedUnitClass", cyIsLimitedUnitClass, "bool (int /*UnitClassTypes*/ eUnitClass)");
	m.def("isWorldWonderClass", cyIsWorldWonderClass, "bool (int /*BuildingClassTypes*/ eBuildingClass)");
	m.def("isTeamWonderClass", cyIsTeamWonderClass, "bool (int /*BuildingClassTypes*/ eBuildingClass)");
	m.def("isNationalWonderClass", cyIsNationalWonderClass, "bool (int /*BuildingClassTypes*/ eBuildingClass)");
	m.def("isLimitedWonderClass", cyIsLimitedWonderClass, "bool (int /*BuildingClassTypes*/ eBuildingClass)");
	m.def("isWorldProject", cyIsWorldProject, "bool (int /*ProjectTypes*/ eProject)");
	m.def("isTeamProject", cyIsTeamProject, "bool (int /*ProjectTypes*/ eProject)");
	m.def("isLimitedProject", cyIsLimitedProject, "bool (int /*ProjectTypes*/ eProject)");
	m.def("getCombatOdds", cyGetCombatOdds, "int (CyUnit* pAttacker, CyUnit* pDefender)");
	m.def("getEspionageModifier", cyGetEspionageModifier, "int (int /*TeamTypes*/ iOurTeam, int /*TeamTypes*/ iTargetTeam)");
}
