#include "CyEngine.h"
#include "CyCommon.h"

#include "../inc/Cv4CommonEngineLib/CvEngine.h"
#include "../inc/Cv4CommonEngineLib/EngineSpecificsHeader.h"

#include <CvGlobals.h>
#include <CvInfos.h>
#include <CvMap.h>
#include <CvPlot.h>
#include <CyPlot.h>

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
	cvengine::engine_specific::getCvEngine().addColoredPlot({ plotX, plotY }, color, PlotStyles::PLOT_STYLE_BOX_FILL, static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::addColoredPlotAlt(int plotX, int plotY, int iPlotStyle, int iLayer, const std::string& szColor, float fAlpha)
{
	// Called by BUG probably.
	const ColorTypes colourI = static_cast<ColorTypes>(gGlobals.getInfoTypeForString(szColor.c_str()));
	NiColorA colour{};
	if (colourI != NO_COLOR)
		colour = gGlobals.getColorInfo(colourI).getColor();
	colour.a = fAlpha;
	cvengine::engine_specific::getCvEngine().addColoredPlot({ plotX, plotY }, colour, static_cast<PlotStyles>(iPlotStyle), static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::addLandmark(CyPlot pPlot, const std::wstring& caption)
{
	cvengine::engine_specific::getCvEngine().addLandmark(pPlot.getPlot(), caption);
}

void CyEngine::addLandmarkPopup([[maybe_unused]] CyPlot pPlot)
{
	// TODO: What does this do?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::addSign(CyPlot pyPlot, PlayerTypes playerType, const std::wstring& caption)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::engine_specific::getCvEngine().setSignText(playerType, { plot->getX(), plot->getY() }, std::move(caption));
}

void CyEngine::clearAreaBorderPlots(int iLayer)
{
	cvengine::engine_specific::getCvEngine().clearAreaBorderPlots(static_cast<AreaBorderLayers>(iLayer));
}

void CyEngine::clearColoredPlots(int iLayer)
{
	cvengine::engine_specific::getCvEngine().clearColoredPlots(static_cast<PlotLandscapeLayers>(iLayer));
}

void CyEngine::fillAreaBorderPlot(int plotX, int plotY, NiColorA color, int iLayer)
{
	cvengine::engine_specific::getCvEngine().fillAreaBorderPlot({ plotX, plotY }, color, static_cast<AreaBorderLayers>(iLayer));
}

void CyEngine::fillAreaBorderPlotAlt(int plotX, int plotY, int iLayer, const std::string& szColor, float fAlpha)
{
	// Used by BUG to draw the city crosses.
	const ColorTypes colourI = static_cast<ColorTypes>(gGlobals.getInfoTypeForString(szColor.c_str()));
	NiColorA colour{};
	if (colourI != NO_COLOR)
		colour = gGlobals.getColorInfo(colourI).getColor();
	colour.a = fAlpha;
	cvengine::engine_specific::getCvEngine().fillAreaBorderPlot({ plotX, plotY }, colour, static_cast<AreaBorderLayers>(iLayer));
}

bool CyEngine::getCityBillboardVisibility()
{
	return cvengine::engine_specific::getCvEngine().getCityBillboardVisibility();
}

bool CyEngine::getCultureVisibility()
{
	return cvengine::engine_specific::getCvEngine().getCultureVisibility();
}

int CyEngine::getNumSigns()
{
	return cvengine::engine_specific::getCvEngine().getNumSigns();
}

bool CyEngine::getSelectionCursorVisibility()
{
	return cvengine::engine_specific::getCvEngine().getSelectionCursorVisibility();
}

CySign CyEngine::getSignByIndex(int index)
{
	return { cvengine::engine_specific::getCvEngine().getSignByIndex(static_cast<size_t>(index)) };
}

bool CyEngine::getUnitFlagVisibility()
{
	return cvengine::engine_specific::getCvEngine().getUnitFlagVisibility();
}

float CyEngine::getUpdateRate()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyEngine::isDirty(EngineDirtyBits eBit)
{
	return cvengine::engine_specific::getCvEngine().isDirty(eBit);
}

bool CyEngine::isGlobeviewUp()
{
	// TODO: Globeview?
	return cvengine::engine_specific::getCvEngine().isGlobeviewUp();
}

void CyEngine::reloadEffectInfos()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::removeLandmark(CyPlot pyPlot)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::engine_specific::getCvEngine().removeLandmark({ plot->getX(), plot->getY() });
}

void CyEngine::removeSign(CyPlot pyPlot, PlayerTypes playerType)
{
	if (const CvPlot* const plot = pyPlot.getPlot())
		cvengine::engine_specific::getCvEngine().removeSign(playerType, { plot->getX(), plot->getY() });
}

void CyEngine::setCityBillboardVisibility(bool bState)
{
	cvengine::engine_specific::getCvEngine().setCityBillboardVisibility(bState);
}

void CyEngine::setCultureVisibility(bool bState)
{
	cvengine::engine_specific::getCvEngine().setCultureVisibility(bState);
}

void CyEngine::setDirty(EngineDirtyBits eBit, bool bNewValue)
{
	cvengine::engine_specific::getCvEngine().setDirty(eBit, bNewValue);
}

void CyEngine::setFogOfWar([[maybe_unused]] bool bState)
{
	//cvengine::engine_specific::getCvEngine().setFogOfWar(bState);
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::setSelectionCursorVisibility(bool bState)
{
	cvengine::engine_specific::getCvEngine().setSelectionCursorVisibility(bState);
}

void CyEngine::setUnitFlagVisibility(bool bState)
{
	cvengine::engine_specific::getCvEngine().setUnitFlagVisibility(bState);
}

void CyEngine::setUpdateRate([[maybe_unused]] float fUpdateRate)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyEngine::toggleGlobeview()
{
	cvengine::engine_specific::getCvEngine().toggleGlobeview();
}

void CyEngine::triggerEffect(int iEffect, NiPoint3 plotPoint)
{
	cvengine::engine_specific::getCvEngine().triggerEffect(iEffect, plotPoint, 0.0f);
}

