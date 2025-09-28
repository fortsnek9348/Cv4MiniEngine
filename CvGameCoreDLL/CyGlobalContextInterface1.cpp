//
// published python interface for CyGlobalContext
// Author - Mustafa Thamer
//

#include "CvGameCoreDLL.h"
#include "CyMap.h"
#include "CyPlayer.h"
#include "CyGame.h"
#include "CyGlobalContext.h"
#include "CvRandom.h"
//#include "CvStructs.h"
#include "CvInfos.h"
#include "CyTeam.h"

#include <pybind11/pybind11.h>


void CyGlobalContextPythonInterface1(pybind11::class_<CyGlobalContext>& x)
{
	//OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface1\n");

	x
		.def("isDebugBuild", &CyGlobalContext::isDebugBuild, "() - returns true if running a debug build")
		.def("getGame", &CyGlobalContext::getCyGame, pybind11::return_value_policy::reference, "() - CyGame()")
		.def("getMap", &CyGlobalContext::getCyMap, pybind11::return_value_policy::reference, "() - CyMap()")
		.def("getPlayer", &CyGlobalContext::getCyPlayer, pybind11::return_value_policy::reference, "(iPlayer) - iPlayer instance")
		.def("getActivePlayer", &CyGlobalContext::getCyActivePlayer, pybind11::return_value_policy::reference, "() - active player instance")
		.def("getASyncRand", &CyGlobalContext::getCyASyncRand, pybind11::return_value_policy::reference, "Non-Synch'd random #")
		.def("getTeam", &CyGlobalContext::getCyTeam, pybind11::return_value_policy::reference, "(iTeam) - iTeam instance")

		// infos
		.def("getNumEffectInfos", &CyGlobalContext::getNumEffectInfos, "int () - Number of effect infos")
		.def("getEffectInfo", &CyGlobalContext::getEffectInfo, pybind11::return_value_policy::reference, "(int (EffectTypes) eEffectID) - CvInfo for EffectID")

		.def("getNumTerrainInfos", &CyGlobalContext::getNumTerrainInfos, "() - Total Terrain Infos XML/Terrain/CIV4TerrainInfos.xml")
		.def("getTerrainInfo", &CyGlobalContext::getTerrainInfo, pybind11::return_value_policy::reference, "(int (TerrainTypes) eTerrainID) - CvInfo for TerrainID")

		.def("getBonusClassInfo", &CyGlobalContext::getBonusClassInfo, pybind11::return_value_policy::reference, "(int (BonusClassTypes) eBonusClassID) - CvInfo for BonusID")

		.def("getNumBonusInfos", &CyGlobalContext::getNumBonusInfos, "() - Total Bonus Infos XML/Terrain/CIV4BonusInfos.xml")
		.def("getBonusInfo", &CyGlobalContext::getBonusInfo, pybind11::return_value_policy::reference, "(BonusID) - CvInfo for BonusID")

		.def("getNumFeatureInfos", &CyGlobalContext::getNumFeatureInfos, "() - Total Feature Infos XML/Terrain/CIV4FeatureInfos.xml")
		.def("getFeatureInfo", &CyGlobalContext::getFeatureInfo, pybind11::return_value_policy::reference, "(FeatureID) - CvInfo for FeatureID")

		.def("getNumUpkeepInfos", &CyGlobalContext::getNumUpkeepInfos, "int () - Number of upkeep infos")
		.def("getUpkeepInfo", &CyGlobalContext::getUpkeepInfo, pybind11::return_value_policy::reference, "(UpkeepInfoID) - CvInfo for upkeep info")

		.def("getNumCultureLevelInfos", &CyGlobalContext::getNumCultureLevelInfos, "int () - Number of culture level infos")
		.def("getCultureLevelInfo", &CyGlobalContext::getCultureLevelInfo, pybind11::return_value_policy::reference, "(CultureLevelID) - CvInfo for CultureLevelID")

		.def("getNumEraInfos", &CyGlobalContext::getNumEraInfos, "int () - Number of era infos")
		.def("getEraInfo", &CyGlobalContext::getEraInfo, pybind11::return_value_policy::reference)

		.def("getNumWorldInfos", &CyGlobalContext::getNumWorldInfos, "int () - Number of world infos")
		.def("getWorldInfo", &CyGlobalContext::getWorldInfo, pybind11::return_value_policy::reference, "CvWorldInfo - (WorldTypeID)")

		.def("getNumClimateInfos", &CyGlobalContext::getNumClimateInfos, "int () - Number of climate infos")
		.def("getClimateInfo", &CyGlobalContext::getClimateInfo, pybind11::return_value_policy::reference, "CvClimateInfo - (ClimateTypeID)")

		.def("getNumSeaLevelInfos", &CyGlobalContext::getNumSeaLevelInfos, "int () - Number of seal level infos")
		.def("getSeaLevelInfo", &CyGlobalContext::getSeaLevelInfo, pybind11::return_value_policy::reference, "CvSeaLevelInfo - (SeaLevelTypeID)")

		.def("getNumPlayableCivilizationInfos", &CyGlobalContext::getNumPlayableCivilizationInfos, "() - Total # of Playable Civs")
		.def("getNumCivilizationInfos", &CyGlobalContext::getNumCivilizatonInfos, "() - Total Civilization Infos XML/Civilizations/CIV4CivilizationInfos.xml")
		.def("getCivilizationInfo", &CyGlobalContext::getCivilizationInfo, pybind11::return_value_policy::reference, "(CivilizationID) - CvInfo for CivilizationID")

		.def("getNumLeaderHeadInfos", &CyGlobalContext::getNumLeaderHeadInfos, "() - Total LeaderHead Infos XML/Civilizations/CIV4LeaderHeadInfos.xml")
		.def("getLeaderHeadInfo", &CyGlobalContext::getLeaderHeadInfo, pybind11::return_value_policy::reference, "(LeaderHeadID) - CvInfo for LeaderHeadID")

		.def("getNumTraitInfos", &CyGlobalContext::getNumTraitInfos, "() - Total Civilization Infos XML/Civilizations/CIV4TraitInfos.xml")
		.def("getTraitInfo", &CyGlobalContext::getTraitInfo, pybind11::return_value_policy::reference, "(TraitID) - CvInfo for TraitID")

		.def("getNumUnitInfos", &CyGlobalContext::getNumUnitInfos, "() - Total Unit Infos XML/Units/CIV4UnitInfos.xml")
		.def("getUnitInfo", &CyGlobalContext::getUnitInfo, pybind11::return_value_policy::reference, "(UnitID) - CvInfo for UnitID")

		.def("getNumSpecialUnitInfos", &CyGlobalContext::getNumSpecialUnitInfos, "() - Total SpecialUnit Infos XML/Units/CIV4SpecialUnitInfos.xml")
		.def("getSpecialUnitInfo", &CyGlobalContext::getSpecialUnitInfo, pybind11::return_value_policy::reference, "(UnitID) - CvInfo for UnitID")

		.def("getYieldInfo", &CyGlobalContext::getYieldInfo, pybind11::return_value_policy::reference, "(YieldID) - CvInfo for YieldID")

		.def("getCommerceInfo", &CyGlobalContext::getCommerceInfo, pybind11::return_value_policy::reference, "(CommerceID) - CvInfo for CommerceID")

		.def("getNumRouteInfos", &CyGlobalContext::getNumRouteInfos, "() - Total Route Infos XML/Misc/CIV4RouteInfos.xml")
		.def("getRouteInfo", &CyGlobalContext::getRouteInfo, pybind11::return_value_policy::reference, "(RouteID) - CvInfo for RouteID")

		.def("getNumImprovementInfos", &CyGlobalContext::getNumImprovementInfos, "() - Total Improvement Infos XML/Terrain/CIV4ImprovementInfos.xml")
		.def("getImprovementInfo", &CyGlobalContext::getImprovementInfo, pybind11::return_value_policy::reference, "(ImprovementID) - CvInfo for ImprovementID")

		.def("getNumGoodyInfos", &CyGlobalContext::getNumGoodyInfos, "() - Total Goody Infos XML/GameInfo/CIV4GoodyInfos.xml")
		.def("getGoodyInfo", &CyGlobalContext::getGoodyInfo, pybind11::return_value_policy::reference, "(GoodyID) - CvInfo for GoodyID")

		.def("getNumBuildInfos", &CyGlobalContext::getNumBuildInfos, "() - Total Build Infos XML/Units/CIV4BuildInfos.xml")
		.def("getBuildInfo", &CyGlobalContext::getBuildInfo, pybind11::return_value_policy::reference, "(BuildID) - CvInfo for BuildID")

		.def("getNumHandicapInfos", &CyGlobalContext::getNumHandicapInfos, "() - Total Handicap Infos XML/GameInfo/CIV4HandicapInfos.xml")
		.def("getHandicapInfo", &CyGlobalContext::getHandicapInfo, pybind11::return_value_policy::reference, "(HandicapID) - CvInfo for HandicapID")

		.def("getNumGameSpeedInfos", &CyGlobalContext::getNumGameSpeedInfos, "() - Total Game speed Infos XML/GameInfo/CIV4GameSpeedInfo.xml")
		.def("getGameSpeedInfo", &CyGlobalContext::getGameSpeedInfo, pybind11::return_value_policy::reference, "(GameSpeed Info) - CvInfo for GameSpeedID")

		.def("getNumTurnTimerInfos", &CyGlobalContext::getNumTurnTimerInfos, "() - Total Turn timer Infos XML/GameInfo/CIV4TurnTimerInfo.xml")
		.def("getTurnTimerInfo", &CyGlobalContext::getTurnTimerInfo, pybind11::return_value_policy::reference, "(TurnTimer Info) - CvInfo for TurnTimerID")

		.def("getNumBuildingClassInfos", &CyGlobalContext::getNumBuildingClassInfos, "() - Total Building Class Infos XML/Buildings/CIV4BuildingClassInfos.xml")
		.def("getBuildingClassInfo", &CyGlobalContext::getBuildingClassInfo, pybind11::return_value_policy::reference, "(BuildingClassID) - CvInfo for BuildingClassID")

		.def("getNumBuildingInfos", &CyGlobalContext::getNumBuildingInfos, "() - Total Building Infos XML/Buildings/CIV4BuildingInfos.xml")
		.def("getBuildingInfo", &CyGlobalContext::getBuildingInfo, pybind11::return_value_policy::reference, "(BuildingID) - CvInfo for BuildingID")

		.def("getNumUnitClassInfos", &CyGlobalContext::getNumUnitClassInfos, "() - Total Unit Class Infos XML/Units/CIV4UnitClassInfos.xml")
		.def("getUnitClassInfo", &CyGlobalContext::getUnitClassInfo, pybind11::return_value_policy::reference, "(UnitClassID) - CvInfo for UnitClassID")

		.def("getNumUnitCombatInfos", &CyGlobalContext::getNumUnitCombatInfos, "() - Total Unit Combat Infos XML/Units/CIV4UnitCombatInfos.xml")
		.def("getUnitCombatInfo", &CyGlobalContext::getUnitCombatInfo, pybind11::return_value_policy::reference, "(UnitCombatID) - CvInfo for UnitCombatID")

		.def("getDomainInfo", &CyGlobalContext::getDomainInfo, pybind11::return_value_policy::reference, "(DomainID) - CvInfo for DomainID")

		.def("getNumActionInfos", &CyGlobalContext::getNumActionInfos, "() - Total Action Infos XML/Units/CIV4ActionInfos.xml")
		.def("getActionInfo", &CyGlobalContext::getActionInfo, pybind11::return_value_policy::reference, "(ActionID) - CvInfo for ActionID")
	;
}
