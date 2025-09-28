//
// published python interface for CyGlobalContext
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

void CyGlobalContextPythonInterface3(pybind11::class_<CyGlobalContext>& x)
{
	//OutputDebugString("Python Extension Module - CyGlobalContextPythonInterface3\n");

	x
		.def("getAttitudeInfo", &CyGlobalContext::getAttitudeInfo, pybind11::return_value_policy::reference, "AttitudeInfo (int id)")
		.def("getMemoryInfo", &CyGlobalContext::getMemoryInfo, pybind11::return_value_policy::reference, "MemoryInfo (int id)")

		.def("getNumPlayerOptionInfos", &CyGlobalContext::getNumPlayerOptionInfos)
		.def("getPlayerOptionsInfo", &CyGlobalContext::getPlayerOptionsInfoByIndex, pybind11::return_value_policy::reference, "(PlayerOptionsInfoID) - PlayerOptionsInfo for PlayerOptionsInfo")
		.def("getPlayerOptionsInfoByIndex", &CyGlobalContext::getPlayerOptionsInfoByIndex, pybind11::return_value_policy::reference, "(PlayerOptionsInfoID) - PlayerOptionsInfo for PlayerOptionsInfo")

		.def("getGraphicOptionsInfo", &CyGlobalContext::getGraphicOptionsInfoByIndex, pybind11::return_value_policy::reference, "(GraphicOptionsInfoID) - GraphicOptionsInfo for GraphicOptionsInfo")
		.def("getGraphicOptionsInfoByIndex", &CyGlobalContext::getGraphicOptionsInfoByIndex, pybind11::return_value_policy::reference, "(GraphicOptionsInfoID) - GraphicOptionsInfo for GraphicOptionsInfo")

		.def("getNumHurryInfos", &CyGlobalContext::getNumHurryInfos, "() - Total Hurry Infos")

		.def("getNumConceptInfos", &CyGlobalContext::getNumConceptInfos, "int () - NumConceptInfos")
		.def("getConceptInfo", &CyGlobalContext::getConceptInfo, pybind11::return_value_policy::reference, "Concept Info () - Returns info object")

		.def("getNumNewConceptInfos", &CyGlobalContext::getNumNewConceptInfos, "int () - NumNewConceptInfos")
		.def("getNewConceptInfo", &CyGlobalContext::getNewConceptInfo, pybind11::return_value_policy::reference, "New Concept Info () - Returns info object")

		.def("getNumCityTabInfos", &CyGlobalContext::getNumCityTabInfos, "int () - Returns NumCityTabInfos")
		.def("getCityTabInfo", &CyGlobalContext::getCityTabInfo, pybind11::return_value_policy::reference, "CityTabInfo - () - Returns Info object")

		.def("getNumCalendarInfos", &CyGlobalContext::getNumCalendarInfos, "int () - Returns NumCalendarInfos")
		.def("getCalendarInfo", &CyGlobalContext::getCalendarInfo, pybind11::return_value_policy::reference, "CalendarInfo () - Returns Info object")
		 
		.def("getNumGameOptionInfos", &CyGlobalContext::getNumGameOptionInfos, "int () - Returns NumGameOptionInfos")
		.def("getGameOptionInfo", &CyGlobalContext::getGameOptionInfo, pybind11::return_value_policy::reference, "GameOptionInfo () - Returns Info object")

		.def("getNumMPOptionInfos", &CyGlobalContext::getNumMPOptionInfos, "int () - Returns NumMPOptionInfos")
		.def("getMPOptionInfo", &CyGlobalContext::getMPOptionInfo, pybind11::return_value_policy::reference, "MPOptionInfo () - Returns Info object")

		.def("getNumForceControlInfos", &CyGlobalContext::getNumForceControlInfos, "int () - Returns NumForceControlInfos")
		.def("getForceControlInfo", &CyGlobalContext::getForceControlInfo, pybind11::return_value_policy::reference, "ForceControlInfo () - Returns Info object")

		.def("getNumSeasonInfos", &CyGlobalContext::getNumSeasonInfos, "int () - Returns NumSeasonInfos")
		.def("getSeasonInfo", &CyGlobalContext::getSeasonInfo, pybind11::return_value_policy::reference, "SeasonInfo () - Returns Info object")

		.def("getNumMonthInfos", &CyGlobalContext::getNumMonthInfos, "int () - Returns NumMonthInfos")
		.def("getMonthInfo", &CyGlobalContext::getMonthInfo, pybind11::return_value_policy::reference, "MonthInfo () - Returns Info object")

		.def("getNumDenialInfos", &CyGlobalContext::getNumDenialInfos, "int () - Returns NumDenialInfos")
		.def("getDenialInfo", &CyGlobalContext::getDenialInfo, pybind11::return_value_policy::reference, "DenialInfo () - Returns Info object")
		;
}
