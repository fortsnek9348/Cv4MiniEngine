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


void CyGlobalContextPythonInterface4(pybind11::class_<CyGlobalContext>& x)
{
	//OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface1\n");

	x
		.def("getNumMissionInfos", &CyGlobalContext::getNumMissionInfos, "() - Total Mission Infos XML/Units/CIV4MissionInfos.xml")
		.def("getMissionInfo", &CyGlobalContext::getMissionInfo, pybind11::return_value_policy::reference, "(MissionID) - CvInfo for MissionID")

		.def("getNumAutomateInfos", &CyGlobalContext::getNumAutomateInfos, "() - Total Automate Infos XML/Units/CIV4AutomateInfos.xml")
		.def("getAutomateInfo", &CyGlobalContext::getAutomateInfo, pybind11::return_value_policy::reference, "(AutomateID) - CvInfo for AutomateID")

		.def("getNumCommandInfos", &CyGlobalContext::getNumCommandInfos, "() - Total Command Infos XML/Units/CIV4CommandInfos.xml")
		.def("getCommandInfo", &CyGlobalContext::getCommandInfo, pybind11::return_value_policy::reference, "(CommandID) - CvInfo for CommandID")

		.def("getNumControlInfos", &CyGlobalContext::getNumControlInfos, "() - Total Control Infos XML/Units/CIV4ControlInfos.xml")
		.def("getControlInfo", &CyGlobalContext::getControlInfo, pybind11::return_value_policy::reference, "(ControlID) - CvInfo for ControlID")

		.def("getNumPromotionInfos", &CyGlobalContext::getNumPromotionInfos, "() - Total Promotion Infos XML/Units/CIV4PromotionInfos.xml")
		.def("getPromotionInfo", &CyGlobalContext::getPromotionInfo, pybind11::return_value_policy::reference, "(PromotionID) - CvInfo for PromotionID")

		.def("getNumTechInfos", &CyGlobalContext::getNumTechInfos, "() - Total Technology Infos XML/Technologies/CIV4TechInfos.xml")
		.def("getTechInfo", &CyGlobalContext::getTechInfo, pybind11::return_value_policy::reference, "(TechID) - CvInfo for TechID")

		.def("getNumSpecialBuildingInfos", &CyGlobalContext::getNumSpecialBuildingInfos, "() - Total Special Building Infos")
		.def("getSpecialBuildingInfo", &CyGlobalContext::getSpecialBuildingInfo, pybind11::return_value_policy::reference, "(SpecialBuildingID) - CvInfo for SpecialBuildingID")

		.def("getNumReligionInfos", &CyGlobalContext::getNumReligionInfos, "() - Total Religion Infos XML/GameInfo/CIV4ReligionInfos.xml")
		.def("getReligionInfo", &CyGlobalContext::getReligionInfo, pybind11::return_value_policy::reference, "(ReligionID) - CvInfo for ReligionID")

		.def("getNumCorporationInfos", &CyGlobalContext::getNumCorporationInfos, "() - Total Religion Infos XML/GameInfo/CIV4CorporationInfos.xml")
		.def("getCorporationInfo", &CyGlobalContext::getCorporationInfo, pybind11::return_value_policy::reference, "(CorporationID) - CvInfo for CorporationID")

		.def("getNumVictoryInfos", &CyGlobalContext::getNumVictoryInfos, "() - Total Victory Infos XML/GameInfo/CIV4VictoryInfos.xml")
		.def("getVictoryInfo", &CyGlobalContext::getVictoryInfo, pybind11::return_value_policy::reference, "(VictoryID) - CvInfo for VictoryID")

		.def("getNumSpecialistInfos", &CyGlobalContext::getNumSpecialistInfos, "() - Total Specialist Infos XML/Units/CIV4SpecialistInfos.xml")
		.def("getSpecialistInfo", &CyGlobalContext::getSpecialistInfo, pybind11::return_value_policy::reference, "(SpecialistID) - CvInfo for SpecialistID")

		.def("getNumCivicOptionInfos", &CyGlobalContext::getNumCivicOptionInfos, "() - Total Civic Infos XML/Misc/CIV4CivicOptionInfos.xml")
		.def("getCivicOptionInfo", &CyGlobalContext::getCivicOptionInfo, pybind11::return_value_policy::reference, "(CivicID) - CvInfo for CivicID")

		.def("getNumCivicInfos", &CyGlobalContext::getNumCivicInfos, "() - Total Civic Infos XML/Misc/CIV4CivicInfos.xml")
		.def("getCivicInfo", &CyGlobalContext::getCivicInfo, pybind11::return_value_policy::reference, "(CivicID) - CvInfo for CivicID")

		.def("getNumDiplomacyInfos", &CyGlobalContext::getNumDiplomacyInfos, "() - Total diplomacy Infos XML/GameInfo/CIV4DiplomacyInfos.xml")
		.def("getDiplomacyInfo", &CyGlobalContext::getDiplomacyInfo, pybind11::return_value_policy::reference, "(DiplomacyID) - CvInfo for DiplomacyID")

		.def("getNumProjectInfos", &CyGlobalContext::getNumProjectInfos, "() - Total Project Infos XML/GameInfo/CIV4ProjectInfos.xml")
		.def("getProjectInfo", &CyGlobalContext::getProjectInfo, pybind11::return_value_policy::reference, "(ProjectID) - CvInfo for ProjectID")

		.def("getNumVoteInfos", &CyGlobalContext::getNumVoteInfos, "() - Total VoteInfos")
		.def("getVoteInfo", &CyGlobalContext::getVoteInfo, pybind11::return_value_policy::reference, "(VoteID) - CvInfo for VoteID")

		.def("getNumProcessInfos", &CyGlobalContext::getNumProcessInfos, "() - Total ProcessInfos")
		.def("getProcessInfo", &CyGlobalContext::getProcessInfo, pybind11::return_value_policy::reference, "(ProcessID) - CvInfo for ProcessID")

		.def("getNumEmphasizeInfos", &CyGlobalContext::getNumEmphasizeInfos, "() - Total EmphasizeInfos")
		.def("getEmphasizeInfo", &CyGlobalContext::getEmphasizeInfo, pybind11::return_value_policy::reference, "(EmphasizeID) - CvInfo for EmphasizeID")

		.def("getHurryInfo", &CyGlobalContext::getHurryInfo, pybind11::return_value_policy::reference, "(HurryID) - CvInfo for HurryID")

		.def("getUnitAIInfo", &CyGlobalContext::getUnitAIInfo, pybind11::return_value_policy::reference, "UnitAIInfo (int id)")

		.def("getColorInfo", &CyGlobalContext::getColorInfo, pybind11::return_value_policy::reference, "ColorInfo (int id)")

		.def("getInfoTypeForString", &CyGlobalContext::getInfoTypeForString, "int (string) - returns the info index with the matching type string")
		.def("getTypesEnum", &CyGlobalContext::getTypesEnum, "int (string) - returns the type enum from a type string")

		.def("getNumPlayerColorInfos", &CyGlobalContext::getNumPlayerColorInfos, "int () - Returns number of PlayerColorInfos")
		.def("getPlayerColorInfo", &CyGlobalContext::getPlayerColorInfo, pybind11::return_value_policy::reference, "PlayerColorInfo (int id)")

		.def("getNumQuestInfos", &CyGlobalContext::getNumQuestInfos, "int () - Returns number of QuestInfos")
		.def("getQuestInfo", &CyGlobalContext::getQuestInfo, pybind11::return_value_policy::reference, "QuestInfo () - Returns info object")

		.def("getNumTutorialInfos", &CyGlobalContext::getNumTutorialInfos, "int () - Returns number of TutorialInfos")
		.def("getTutorialInfo", &CyGlobalContext::getTutorialInfo, pybind11::return_value_policy::reference, "TutorialInfo () - Returns info object")

		.def("getNumEventTriggerInfos", &CyGlobalContext::getNumEventTriggerInfos, "int () - Returns number of EventTriggerInfos")
		.def("getEventTriggerInfo", &CyGlobalContext::getEventTriggerInfo, pybind11::return_value_policy::reference, "EventTriggerInfo () - Returns info object")

		.def("getNumEventInfos", &CyGlobalContext::getNumEventInfos, "int () - Returns number of EventInfos")
		.def("getEventInfo", &CyGlobalContext::getEventInfo, pybind11::return_value_policy::reference, "EventInfo () - Returns info object")

		.def("getNumEspionageMissionInfos", &CyGlobalContext::getNumEspionageMissionInfos, "int () - Returns number of EspionageMissionInfos")
		.def("getEspionageMissionInfo", &CyGlobalContext::getEspionageMissionInfo, pybind11::return_value_policy::reference, "EspionageMissionInfo () - Returns info object")

		.def("getNumHints", &CyGlobalContext::getNumHints, "int () - Returns number of Hints")
		.def("getHints", &CyGlobalContext::getHints, pybind11::return_value_policy::reference, "Hints () - Returns info object")

		.def("getNumMainMenus", &CyGlobalContext::getNumMainMenus, "int () - Returns number")
		.def("getMainMenus", &CyGlobalContext::getMainMenus, pybind11::return_value_policy::reference, "MainMenus () - Returns info object")

		.def("getNumVoteSourceInfos", &CyGlobalContext::getNumVoteSourceInfos, "int ()")
		.def("getVoteSourceInfo", &CyGlobalContext::getVoteSourceInfo, pybind11::return_value_policy::reference, "Returns info object")

		.def("getNumVoteSourceInfos", &CyGlobalContext::getNumVoteSourceInfos, "int ()")
		.def("getVoteSourceInfo", &CyGlobalContext::getVoteSourceInfo, pybind11::return_value_policy::reference, "Returns info object")

		// ArtInfos
		.def("getNumInterfaceArtInfos", &CyGlobalContext::getNumInterfaceArtInfos, "() - Total InterfaceArtnology Infos XML/InterfaceArtnologies/CIV4InterfaceArtInfos.xml")
		.def("getInterfaceArtInfo", &CyGlobalContext::getInterfaceArtInfo, pybind11::return_value_policy::reference, "(InterfaceArtID) - CvArtInfo for InterfaceArtID")

		.def("getNumMovieArtInfos", &CyGlobalContext::getNumMovieArtInfos, "() - Total MovieArt Infos XML/MovieArtInfos/CIV4ArtDefines.xml")
		.def("getMovieArtInfo", &CyGlobalContext::getMovieArtInfo, pybind11::return_value_policy::reference, "(MovieArtID) - CvArtInfo for MovieArtID")

		.def("getNumMiscArtInfos", &CyGlobalContext::getNumMiscArtInfos, "() - Total MiscArtnology Infos XML/MiscArt/CIV4MiscArtInfos.xml")
		.def("getMiscArtInfo", &CyGlobalContext::getMiscArtInfo, pybind11::return_value_policy::reference, "(MiscArtID) - CvArtInfo for MiscArtID")

		.def("getNumUnitArtInfos", &CyGlobalContext::getNumUnitArtInfos, "() - Total UnitArtnology Infos XML/UnitArt/CIV4UnitArtInfos.xml")
		.def("getUnitArtInfo", &CyGlobalContext::getUnitArtInfo, pybind11::return_value_policy::reference, "(UnitID) - CvArtInfo for UnitID")

		.def("getNumBuildingArtInfos", &CyGlobalContext::getNumBuildingArtInfos, "int () - Returns number of BuildingArtInfos")
		.def("getBuildingArtInfo", &CyGlobalContext::getBuildingArtInfo, pybind11::return_value_policy::reference, "(BuildingID) - CvArtInfo for BuildingID")

		.def("getNumCivilizationArtInfos", &CyGlobalContext::getNumCivilizationArtInfos, "int () - Returns number of CivilizationArtInfos")
		.def("getCivilizationArtInfo", &CyGlobalContext::getCivilizationArtInfo, pybind11::return_value_policy::reference, "(CivilizationID) - CvArtInfo for CivilizationID")

		.def("getNumLeaderheadArtInfos", &CyGlobalContext::getNumLeaderheadArtInfos, "int () - Returns number of LeaderHeadArtInfos")
		.def("getLeaderheadArtInfo", &CyGlobalContext::getLeaderheadArtInfo, pybind11::return_value_policy::reference, "(LeaderheadID) - CvArtInfo for LeaderheadID")

		.def("getNumBonusArtInfos", &CyGlobalContext::getNumBonusArtInfos, "int () - Returns number of BonusArtInfos")
		.def("getBonusArtInfo", &CyGlobalContext::getBonusArtInfo, pybind11::return_value_policy::reference, "BonusArtInfo () - Returns info object")

		.def("getNumImprovementArtInfos", &CyGlobalContext::getNumImprovementArtInfos, "int () - Returns number of ImprovementArtInfos")
		.def("getImprovementArtInfo", &CyGlobalContext::getImprovementArtInfo, pybind11::return_value_policy::reference, "ImprovementArtInfo () - Returns info object")

		.def("getNumTerrainArtInfos", &CyGlobalContext::getNumTerrainArtInfos, "int () - Returns number of TerrainArtInfos")
		.def("getTerrainArtInfo", &CyGlobalContext::getTerrainArtInfo, pybind11::return_value_policy::reference, "TerrainArtInfo () - Returns info object")

		.def("getNumFeatureArtInfos", &CyGlobalContext::getNumFeatureArtInfos, "int () - Returns number of FeatureArtInfos")
		.def("getFeatureArtInfo", &CyGlobalContext::getFeatureArtInfo, pybind11::return_value_policy::reference, "FeatureArtInfo () - Returns info object")

		// Types
		.def("getNumEntityEventTypes", &CyGlobalContext::getNumEntityEventTypes, "int () - Returns number of EntityEventTypes")
		.def("getEntityEventType", &CyGlobalContext::getEntityEventTypes, "string () - Returns enum string")

		.def("getNumAnimationOperatorTypes", &CyGlobalContext::getNumAnimationOperatorTypes, "int () - Returns number of AnimationOperatorTypes")
		.def("getAnimationOperatorTypes", &CyGlobalContext::getAnimationOperatorTypes, "string () - Returns enum string")

		.def("getFunctionTypes", &CyGlobalContext::getFunctionTypes, "string () - Returns enum string")

		.def("getNumArtStyleTypes", &CyGlobalContext::getNumArtStyleTypes, "int () - Returns number of ArtStyleTypes")
		.def("getArtStyleTypes", &CyGlobalContext::getArtStyleTypes, "string () - Returns enum string")

		.def("getNumFlavorTypes", &CyGlobalContext::getNumFlavorTypes, "int () - Returns number of FlavorTypes")
		.def("getFlavorTypes", &CyGlobalContext::getFlavorTypes, "string () - Returns enum string")

		.def("getNumUnitArtStyleTypeInfos", &CyGlobalContext::getNumUnitArtStyleTypeInfos, "int () - Returns number of UnitArtStyleTypes")
		.def("getUnitArtStyleTypeInfo", &CyGlobalContext::getUnitArtStyleTypeInfo, pybind11::return_value_policy::reference, "(UnitArtStyleTypeID) - CvInfo for UnitArtStyleTypeID")

		.def("getNumCitySizeTypes", &CyGlobalContext::getNumCitySizeTypes, "int () - Returns number of CitySizeTypes")
		.def("getCitySizeTypes", &CyGlobalContext::getCitySizeTypes, "string () - Returns enum string")

		.def("getContactTypes", &CyGlobalContext::getContactTypes, "string () - Returns enum string")

		.def("getDiplomacyPowerTypes", &CyGlobalContext::getDiplomacyPowerTypes, "string () - Returns enum string")
		;
}
