#include "CyEngine.h"
#include "CvPythonExtensions.h"
#include "../Logger.h"

#include <CyPlot.h>

using cvengine::unimplementedPythonFunction;

void CySign::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CySign::x)

	pybind11::class_<CySign>(m, "CySign")
		.R(getCaption)
		.R(getPlayerType)
		.R(getPlot)
		;
}

const std::string& CySign::getCaption()
{
	unimplementedPythonFunction();
}

PlayerTypes CySign::getPlayerType()
{
	unimplementedPythonFunction();
}

CyPlot CySign::getPlot()
{
	unimplementedPythonFunction();
}

void CyEngine::registerWithPython(const pybind11::module& m)
{
#undef R
#define R(x) def(#x, &CyEngine::x)

	pybind11::class_<CyEngine>(m, "CyEngine")
		.def(pybind11::init())
		.R(addColoredPlot)
		.R(addColoredPlotAlt)
		.R(addLandmark)
		.R(addLandmarkPopup)
		.R(addSign)
		.R(clearAreaBorderPlots)
		.R(clearColoredPlots)
		.R(fillAreaBorderPlot)
		.R(fillAreaBorderPlotAlt)
		.R(getCityBillboardVisibility)
		.R(getCultureVisibility)
		.R(getNumSigns)
		.R(getSelectionCursorVisibility)
		.R(getSignByIndex)
		.R(getUnitFlagVisibility)
		.R(getUpdateRate)
		.R(isDirty)
		.R(isGlobeviewUp)
		.R(reloadEffectInfos)
		.R(removeLandmark)
		.R(removeSign)
		.R(setCityBillboardVisibility)
		.R(setCultureVisibility)
		.R(setDirty)
		.R(setFogOfWar)
		.R(setSelectionCursorVisibility)
		.R(setUnitFlagVisibility)
		.R(setUpdateRate)
		.R(toggleGlobeview)
		.R(triggerEffect)
		;
}

namespace
{
	struct DummyEngine
	{
		bool cityBillboardVisibility = true;
		bool unitFlagVisibility = true;
		bool selectionCursorVisibility = false;
	};

	constinit DummyEngine gDummyEngine{};
}

void CyEngine::addColoredPlot([[maybe_unused]] int plotX, [[maybe_unused]] int plotY, [[maybe_unused]] NiColorA color, [[maybe_unused]] int iLayer)
{
	unimplementedPythonFunction();
}

void CyEngine::addColoredPlotAlt([[maybe_unused]] int plotX, [[maybe_unused]] int plotY, [[maybe_unused]] int iPlotStyle, [[maybe_unused]] int iLayer, [[maybe_unused]] const std::string& szColor, [[maybe_unused]] float fAlpha)
{
	// Called by BUG probably.
	//unimplementedPythonFunction();
}

void CyEngine::addLandmark([[maybe_unused]] CyPlot pPlot, [[maybe_unused]] const std::string& caption)
{
	unimplementedPythonFunction();
}

void CyEngine::addLandmarkPopup([[maybe_unused]] CyPlot pPlot)
{
	unimplementedPythonFunction();
}

void CyEngine::addSign([[maybe_unused]] CyPlot plot, [[maybe_unused]] PlayerTypes playerType, [[maybe_unused]] const std::string& caption)
{
	// TODO: Signs.
	//unimplementedPythonFunction();
}

void CyEngine::clearAreaBorderPlots([[maybe_unused]] int iLayer)
{
	cvengine::logDebug("Unimplemented.");
}

void CyEngine::clearColoredPlots([[maybe_unused]] int iLayer)
{
	cvengine::logDebug("Unimplemented.");
}

void CyEngine::fillAreaBorderPlot([[maybe_unused]] int plotX, [[maybe_unused]] int plotY, [[maybe_unused]] NiColorA color, [[maybe_unused]] int iLayer)
{
	unimplementedPythonFunction();
}

void CyEngine::fillAreaBorderPlotAlt([[maybe_unused]] int plotX, [[maybe_unused]] int plotY, [[maybe_unused]] int iLayer, [[maybe_unused]] const std::string& szColor, [[maybe_unused]] float fAlpha)
{
	// Used by BUG to draw the city crosses.
	//unimplementedPythonFunction();
}

bool CyEngine::getCityBillboardVisibility()
{
	return gDummyEngine.cityBillboardVisibility;
}

bool CyEngine::getCultureVisibility()
{
	unimplementedPythonFunction();
}

int CyEngine::getNumSigns()
{
	return 0;
}

bool CyEngine::getSelectionCursorVisibility()
{
	return gDummyEngine.selectionCursorVisibility;
}

CySign CyEngine::getSignByIndex([[maybe_unused]] int index)
{
	unimplementedPythonFunction();
}

bool CyEngine::getUnitFlagVisibility()
{
	return gDummyEngine.unitFlagVisibility;;
}

float CyEngine::getUpdateRate()
{
	unimplementedPythonFunction();
}

bool CyEngine::isDirty([[maybe_unused]] EngineDirtyBits eBit)
{
	unimplementedPythonFunction();
}

bool CyEngine::isGlobeviewUp()
{
	// TODO: Globeview?
	return false;
}

void CyEngine::reloadEffectInfos()
{
	unimplementedPythonFunction();
}

void CyEngine::removeLandmark([[maybe_unused]] CyPlot pPlot)
{
	unimplementedPythonFunction();
}

void CyEngine::removeSign([[maybe_unused]] CyPlot pPlot, [[maybe_unused]] PlayerTypes playerType)
{
	unimplementedPythonFunction();
}

void CyEngine::setCityBillboardVisibility([[maybe_unused]] bool bState)
{
	unimplementedPythonFunction();
}

void CyEngine::setCultureVisibility([[maybe_unused]] bool bState)
{
	unimplementedPythonFunction();
}

void CyEngine::setDirty([[maybe_unused]] EngineDirtyBits eBit, [[maybe_unused]] bool bNewValue)
{
	unimplementedPythonFunction();
}

void CyEngine::setFogOfWar([[maybe_unused]] bool bState)
{
	unimplementedPythonFunction();
}

void CyEngine::setSelectionCursorVisibility([[maybe_unused]] bool bState)
{
	unimplementedPythonFunction();
}

void CyEngine::setUnitFlagVisibility([[maybe_unused]] bool bState)
{
	unimplementedPythonFunction();
}

void CyEngine::setUpdateRate([[maybe_unused]] float fUpdateRate)
{
	unimplementedPythonFunction();
}

void CyEngine::toggleGlobeview()
{
	unimplementedPythonFunction();
}

void CyEngine::triggerEffect([[maybe_unused]] int iEffect, [[maybe_unused]] NiPoint3 plotPoint)
{
	unimplementedPythonFunction();
}

