#include "CvGameCoreDLL.h"
#include "CyArtFileMgr.h"
#include "CvInfos.h"

#include <pybind11/pybind11.h>

//
// published python interface for CyArea
//

void CyArtFileMgrPythonInterface(pybind11::module m)
{
	//OutputDebugString("Python Extension Module - CyArtFileMgrPythonInterface\n");

	pybind11::class_<CyArtFileMgr>(m, "CyArtFileMgr")
		.def(pybind11::init())
		.def("isNone", &CyArtFileMgr::isNone, "bool () - Checks to see if pointer points to a real object")

		.def("Reset", &CyArtFileMgr::Reset, "void ()")
		.def("buildArtFileInfoMaps", &CyArtFileMgr::buildArtFileInfoMaps, "void ()")

		.def("getInterfaceArtInfo", &CyArtFileMgr::getInterfaceArtInfo,  pybind11::return_value_policy::reference, "CvArtInfoInterface ()")
		.def("getMovieArtInfo", &CyArtFileMgr::getMovieArtInfo,  pybind11::return_value_policy::reference, "CvArtInfoMovie ()")
		.def("getMiscArtInfo", &CyArtFileMgr::getMiscArtInfo, pybind11::return_value_policy::reference, "CvArtInfoMisc ()")
		.def("getUnitArtInfo", &CyArtFileMgr::getUnitArtInfo, pybind11::return_value_policy::reference, "CvArtInfoUnit ()")
		.def("getBuildingArtInfo", &CyArtFileMgr::getBuildingArtInfo, pybind11::return_value_policy::reference, "CvArtInfoBuilding ()")
		.def("getCivilizationArtInfo", &CyArtFileMgr::getCivilizationArtInfo, pybind11::return_value_policy::reference, "CvArtInfoCivilization ()")
		.def("getLeaderheadArtInfo", &CyArtFileMgr::getLeaderheadArtInfo, pybind11::return_value_policy::reference, "CvArtInfoLeaderhead ()")
		.def("getBonusArtInfo", &CyArtFileMgr::getBonusArtInfo, pybind11::return_value_policy::reference, "CvArtInfoBonus ()")
		.def("getImprovementArtInfo", &CyArtFileMgr::getImprovementArtInfo, pybind11::return_value_policy::reference, "CvArtInfoImprovement ()")
		.def("getTerrainArtInfo", &CyArtFileMgr::getTerrainArtInfo, pybind11::return_value_policy::reference, "CvArtInfoTerrain ()")
		.def("getFeatureArtInfo", &CyArtFileMgr::getFeatureArtInfo, pybind11::return_value_policy::reference, "CvArtInfoFeature ()")
	;
}

