#include "CvGameCoreDLL.h"
#include "CyMap.h"
#include "CyPlayer.h"
#include "CyPlot.h"
#include "CyGame.h"
#include "CyUnit.h"
#include "CyGlobalContext.h"
#include "CyCity.h"

#include <pybind11/pybind11.h>


void CyCityPythonInterface1(pybind11::class_<CyCity>& x);
void CyPlotPythonInterface1(pybind11::class_<CyPlot>& x);
void CyPlayerPythonInterface1(pybind11::class_<CyPlayer>& x);
void CyPlayerPythonInterface2(pybind11::class_<CyPlayer>& x);
void CyUnitPythonInterface1(pybind11::class_<CyUnit>& x);
void CyGlobalContextPythonInterface1(pybind11::class_<CyGlobalContext>& x);
void CyGlobalContextPythonInterface2(pybind11::class_<CyGlobalContext>& x);
void CyGlobalContextPythonInterface3(pybind11::class_<CyGlobalContext>& x);
void CyGlobalContextPythonInterface4(pybind11::class_<CyGlobalContext>& x);
void CyGamePythonInterface         /**/(pybind11::module m);
void CyRandomPythonInterface       /**/(pybind11::module m);
void CyEnumsPythonInterface        /**/(pybind11::module m);
void CyTeamPythonInterface         /**/(pybind11::module m);
void CyAreaPythonInterface         /**/(pybind11::module m);
void CyStructsPythonInterface1     /**/(pybind11::module m);
void CyMapPythonInterface          /**/(pybind11::module m);
void CyMapGeneratorPythonInterface /**/(pybind11::module m);
void CyInfoPythonInterface1        /**/(pybind11::module m);
void CyInfoPythonInterface2        /**/(pybind11::module m);
void CyInfoPythonInterface3        /**/(pybind11::module m);
void CySelectionGroupInterface     /**/(pybind11::module m);
void CyArtFileMgrPythonInterface   /**/(pybind11::module m);
void CyGameTextMgrInterface        /**/(pybind11::module m);
void CyHallOfFameInterface         /**/(pybind11::module m);
void CyGameCoreUtilsPythonInterface/**/(pybind11::module m);
void CyMessageControlInterface     /**/(pybind11::module m);

//
//
//
DllExport void DLLPublishToPython(pybind11::module m)
{
	CyEnumsPythonInterface(m);
	CyGamePythonInterface(m);
	CyRandomPythonInterface(m);
	CyTeamPythonInterface(m);
	CyAreaPythonInterface(m);
	CyStructsPythonInterface1(m);
	CyMapPythonInterface(m);
	CyMapGeneratorPythonInterface(m);
	CySelectionGroupInterface(m);
	CyArtFileMgrPythonInterface(m);
	CyGameTextMgrInterface(m);
	CyInfoPythonInterface1(m);
	CyInfoPythonInterface2(m);
	CyInfoPythonInterface3(m);
	CyHallOfFameInterface(m);
	CyGameCoreUtilsPythonInterface(m);
	CyMessageControlInterface(m);

	//
	// large interfaces which can be split across files if need be
	//
	pybind11::class_<CyCity> city(m, "CyCity");		// define city class
	CyCityPythonInterface1(city);				// publish it's methods

	pybind11::class_<CyPlayer> player(m, "CyPlayer");	// define player class
	CyPlayerPythonInterface1(player);				// publish it's methods
	CyPlayerPythonInterface2(player);				// publish it's methods

	pybind11::class_<CyUnit> unit(m, "CyUnit");		// define unit class
	CyUnitPythonInterface1(unit);				// publish it's methods

	pybind11::class_<CyPlot> plot(m, "CyPlot");		// define plot class
	CyPlotPythonInterface1(plot);				// publish it's methods

	pybind11::class_<CyGlobalContext> gc(m, "CyGlobalContext");	// define globals class 
	gc.def(pybind11::init());
	CyGlobalContextPythonInterface1(gc);					// publish it's methods 
	CyGlobalContextPythonInterface2(gc);					// publish it's methods
	CyGlobalContextPythonInterface3(gc);					// publish it's methods
	CyGlobalContextPythonInterface4(gc);					// publish it's methods 
}