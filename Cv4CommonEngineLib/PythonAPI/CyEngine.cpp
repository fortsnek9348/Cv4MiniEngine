#include "CyEngine.h"
#include "CyCommon.h"
#include "../inc/Cv4CommonEngineLib/CvEngine.h"
#include "../CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvPlot.h>
#include <CvGameCoreDLL/CyPlot.h>

using cvengine::abortOnUnimplementedPythonFunction;
using cvengine::logUnimplementedPythonFunction;

void CySign::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CySign::x)

	pybind11::class_<CySign>(m, "CySign")
		.R(getCaption)
		.R(getPlayerType)
		.R(getPlot)
		;
}

const std::wstring& CySign::getCaption()
{
	return data.caption;
}

PlayerTypes CySign::getPlayerType()
{
	return data.playerI;
}

CyPlot CySign::getPlot()
{
	return gGlobals.getMap().plot(data.coord.x, data.coord.y);
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

//namespace
//{
//	struct DummyEngine
//	{
//		bool cityBillboardVisibility = true;
//		bool unitFlagVisibility = true;
//		bool selectionCursorVisibility = false;
//	};
//
//	constinit DummyEngine gDummyEngine{};
//}

void CyEngine::addColoredPlot(int plotX, int plotY, NiColorA color, int iLayer)
{
	// Not sure what style to use.
	cvengine::gCommonEngineConfig.engine->addColoredPlot({ plotX, plotY }, color, PlotStyles::PLOT_STYLE_BOX_FILL, static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::addColoredPlotAlt(int plotX, int plotY, int iPlotStyle, int iLayer, const std::string& szColor, float fAlpha)
{
	// Called by BUG probably.
	const ColorTypes colourI = static_cast<ColorTypes>(gGlobals.getInfoTypeForString(szColor.c_str()));
	NiColorA colour{};
	if (colourI != NO_COLOR)
		colour = gGlobals.getColorInfo(colourI).getColor();
	colour.a = fAlpha;
	cvengine::gCommonEngineConfig.engine->addColoredPlot({ plotX, plotY }, colour, static_cast<PlotStyles>(iPlotStyle), static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::addLandmark(CyPlot pPlot, const std::wstring& caption)
{
	cvengine::gCommonEngineConfig.engine->addLandmark(pPlot.getPlot(), caption);
}

void CyEngine::addLandmarkPopup([[maybe_unused]] CyPlot pPlot)
{
	// TODO: What does this do?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::addSign(CyPlot pyPlot, PlayerTypes playerType, const std::wstring& caption)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::gCommonEngineConfig.engine->setSignText(playerType, { plot->getX(), plot->getY() }, std::move(caption));
}

void CyEngine::clearAreaBorderPlots(int iLayer)
{
	cvengine::gCommonEngineConfig.engine->clearAreaBorderPlots(static_cast<AreaBorderLayers>(iLayer));
}

void CyEngine::clearColoredPlots(int iLayer)
{
	cvengine::gCommonEngineConfig.engine->clearColoredPlots(static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::fillAreaBorderPlot(int plotX, int plotY, NiColorA color, int iLayer)
{
	cvengine::gCommonEngineConfig.engine->fillAreaBorderPlot({ plotX, plotY }, color, static_cast<AreaBorderLayers>(iLayer));
}

void CyEngine::fillAreaBorderPlotAlt(int plotX, int plotY, int iLayer, const std::string& szColor, float fAlpha)
{
	// Used by BUG to draw the city crosses.
	const ColorTypes colourI = static_cast<ColorTypes>(gGlobals.getInfoTypeForString(szColor.c_str()));
	NiColorA colour{};
	if (colourI != NO_COLOR)
		colour = gGlobals.getColorInfo(colourI).getColor();
	colour.a = fAlpha;
	cvengine::gCommonEngineConfig.engine->fillAreaBorderPlot({ plotX, plotY }, colour, static_cast<AreaBorderLayers>(iLayer));
}

bool CyEngine::getCityBillboardVisibility()
{
	return cvengine::gCommonEngineConfig.engine->getCityBillboardVisibility();
}

bool CyEngine::getCultureVisibility()
{
	return cvengine::gCommonEngineConfig.engine->getCultureVisibility();
}

int CyEngine::getNumSigns()
{
	return cvengine::gCommonEngineConfig.engine->getNumSigns();
}

bool CyEngine::getSelectionCursorVisibility()
{
	return cvengine::gCommonEngineConfig.engine->getSelectionCursorVisibility();
}

CySign CyEngine::getSignByIndex(int index)
{
	return { cvengine::gCommonEngineConfig.engine->getSignByIndex(static_cast<size_t>(index)) };
}

bool CyEngine::getUnitFlagVisibility()
{
	return cvengine::gCommonEngineConfig.engine->getUnitFlagVisibility();
}

float CyEngine::getUpdateRate()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyEngine::isDirty(EngineDirtyBits eBit)
{
	return cvengine::gCommonEngineConfig.engine->isDirty(eBit);
}

bool CyEngine::isGlobeviewUp()
{
	// TODO: Globeview?
	return cvengine::gCommonEngineConfig.engine->isGlobeviewUp();
}

void CyEngine::reloadEffectInfos()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::removeLandmark(CyPlot pyPlot)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::gCommonEngineConfig.engine->removeLandmark({ plot->getX(), plot->getY() });
}

void CyEngine::removeSign(CyPlot pyPlot, PlayerTypes playerType)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::gCommonEngineConfig.engine->removeSign(playerType, { plot->getX(), plot->getY() });
}

void CyEngine::setCityBillboardVisibility(bool bState)
{
	cvengine::gCommonEngineConfig.engine->setCityBillboardVisibility(bState);
}

void CyEngine::setCultureVisibility(bool bState)
{
	cvengine::gCommonEngineConfig.engine->setCultureVisibility(bState);
}

void CyEngine::setDirty(EngineDirtyBits eBit, bool bNewValue)
{
	cvengine::gCommonEngineConfig.engine->setDirty(eBit, bNewValue);
}

void CyEngine::setFogOfWar([[maybe_unused]] bool bState)
{
	//cvengine::gCommonEngineConfig.engine->setFogOfWar(bState);
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::setSelectionCursorVisibility(bool bState)
{
	cvengine::gCommonEngineConfig.engine->setSelectionCursorVisibility(bState);
}

void CyEngine::setUnitFlagVisibility(bool bState)
{
	cvengine::gCommonEngineConfig.engine->setUnitFlagVisibility(bState);
}

void CyEngine::setUpdateRate([[maybe_unused]] float fUpdateRate)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::toggleGlobeview()
{
	cvengine::gCommonEngineConfig.engine->toggleGlobeview();
}

void CyEngine::triggerEffect(int iEffect, NiPoint3 plotPoint)
{
	cvengine::gCommonEngineConfig.engine->triggerEffect(iEffect, plotPoint, 0.0f);
}

