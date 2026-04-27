#include "CvTuiEngine.h"
#include "CvTuiInterface.h"
#include "WorldView.h"

#include <Cv4CommonEngineLib/Common.h>

#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvMap.h>

using namespace cvengine;

CvTuiEngine& CvTuiEngine::getInstance()
{
	static CvTuiEngine s;
	return s;
}

void CvTuiEngine::doTurn()
{
	// Just stick this here.
	clearColoredPlots(PLOT_LANDSCAPE_LAYER_ALL);

	// Direct debug output for when logging AI autoplay to file.
	outputDebugString(("CvTuiEngine::DoTurn: " + std::to_string(gGlobals.getGame().getGameTurn()) + '\n').c_str());
}

void CvTuiEngine::toggleGlobeview()
{
	// Not implemented.
	std::abort();
}
bool CvTuiEngine::isGlobeviewUp()
{
	return false;
}
void CvTuiEngine::toggleResourceLayer()
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::toggleUnitLayer()
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setResourceLayer([[maybe_unused]] bool bOn)
{
	// Not implemented.
	std::abort();
}

void CvTuiEngine::moveBaseTurnRight([[maybe_unused]] float increment)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::moveBaseTurnLeft([[maybe_unused]] float increment)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setFlying([[maybe_unused]] bool value)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::cycleFlyingMode([[maybe_unused]] int displacement)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setMouseFlying([[maybe_unused]] bool value)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setSatelliteMode([[maybe_unused]] bool value)
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setOrthoCamera([[maybe_unused]] bool value)
{
	// Not implemented.
	std::abort();
}
bool CvTuiEngine::getFlying()
{
	return false;
}
bool CvTuiEngine::getMouseFlying()
{
	return false;
}
bool CvTuiEngine::getSatelliteMode()
{
	return false;
}
bool CvTuiEngine::getOrthoCamera()
{
	return false;
}

bool CvTuiEngine::getCityBillboardVisibility() const
{
	return true;
}
void CvTuiEngine::setCityBillboardVisibility(bool)
{
	// Not implemented.
	std::abort();
}
bool CvTuiEngine::getCultureVisibility() const
{
	return false;
}
void CvTuiEngine::setCultureVisibility(bool) const
{
	// Not implemented.
	std::abort();
}
bool CvTuiEngine::getSelectionCursorVisibility() const
{
	return true;
}
void CvTuiEngine::setSelectionCursorVisibility(bool)
{
	// Not implemented.
	std::abort();
}
bool CvTuiEngine::getUnitFlagVisibility() const
{
	return true;
}
void CvTuiEngine::setUnitFlagVisibility(bool)
{
	// Not implemented.
	std::abort();
}

// landscape
void CvTuiEngine::getLandscapeGameDimensions(float& fWidth, float& fHeight)
{
	const CvMap& map = gGlobals.getMap();
	fWidth = map.getGridWidth() * gGlobals.getPLOT_SIZE();
	fHeight = map.getGridHeight() * gGlobals.getPLOT_SIZE();
}

float CvTuiEngine::getHeightmapZ([[maybe_unused]] const NiPoint3& pt3, [[maybe_unused]] bool bClampAboveWater)
{
	return 0.0f;
}

void CvTuiEngine::lightenVisibility(ivec2 coord)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->changePlotVisibility(coord, WorldView::kLightened);
}
void CvTuiEngine::darkenVisibility(ivec2 coord)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->changePlotVisibility(coord, WorldView::kDarkened);
}
void CvTuiEngine::blackenVisibility(ivec2 coord)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->changePlotVisibility(coord, WorldView::kBlackened);
}
void CvTuiEngine::rebuildAllPlots()
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->invalidateAll();
}
void CvTuiEngine::rebuildPlot(ivec2 coord, [[maybe_unused]] bool bRebuildHeights, [[maybe_unused]] bool bRebuildTextures)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->invalidatePlot(coord);
}
void CvTuiEngine::rebuildRiverPlotTile(ivec2 coord, [[maybe_unused]] bool bRebuildHeights, [[maybe_unused]] bool bRebuildTextures)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->invalidatePlot(coord);
}
void CvTuiEngine::rebuildTileArt(ivec2 coord)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->invalidatePlot(coord);
}
void CvTuiEngine::forceTreeOffsets([[maybe_unused]] ivec2 coord)
{
	// Not implemented.
}

bool CvTuiEngine::getGridMode()
{
	return true;
}
void CvTuiEngine::setGridMode([[maybe_unused]] bool bVal)
{
	// Not implemented.
}

void CvTuiEngine::addColoredPlot(ivec2 coord, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->setColouredPlot(coord, { color, plotStyle, layer });
}
void CvTuiEngine::clearColoredPlots(PlotLandscapeLayers layer)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->clearColouredPlots(layer);
}
void CvTuiEngine::fillAreaBorderPlot([[maybe_unused]] ivec2 coord, [[maybe_unused]] const NiColorA& color, [[maybe_unused]] AreaBorderLayers layer)
{
	// Not implemented.
}
void CvTuiEngine::clearAreaBorderPlots([[maybe_unused]] AreaBorderLayers layer)
{
	// Not implemented.
}
void CvTuiEngine::updateFoundingBorder()
{
	// Not implemented.
}


void CvTuiEngine::triggerEffect([[maybe_unused]] int iEffect, [[maybe_unused]] NiPoint3 pt3Point, [[maybe_unused]] float rotation)
{
	// Not implemented.
}

CvPlot* CvTuiEngine::pickPlot([[maybe_unused]] ivec2 coord, [[maybe_unused]] NiPoint3& worldPoint)
{
	// Not implemented.
	std::abort();
}

void CvTuiEngine::setDirty([[maybe_unused]] EngineDirtyBits eBit, [[maybe_unused]] bool bNewValue)
{
}
bool CvTuiEngine::isDirty([[maybe_unused]] EngineDirtyBits eBit)
{
	return false;
}
void CvTuiEngine::pushFogOfWar([[maybe_unused]] FogOfWarModeTypes eNewMode)
{
	// Not implemented.
	std::abort();
}
FogOfWarModeTypes CvTuiEngine::popFogOfWar()
{
	// Not implemented.
	std::abort();
}
void CvTuiEngine::setFogOfWarFromStack()
{
	// Not implemented.
}
void CvTuiEngine::markBridgesDirty()
{
}
void CvTuiEngine::addLaunch([[maybe_unused]] PlayerTypes playerType)
{
	// No spaceship in TUI.
}
void CvTuiEngine::addGreatWall([[maybe_unused]] CvCity* city)
{
	// No greatwall in TUI.
}
void CvTuiEngine::removeGreatWall([[maybe_unused]] CvCity* city)
{
	// Not implemented.
}
void CvTuiEngine::markPlotTextureAsDirty([[maybe_unused]] ivec2 coord)
{
	if (auto* const worldView = CvTuiInterface::getInstance().getWorldView())
		worldView->invalidatePlot(coord);
}