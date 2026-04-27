#include "MyCvDLLEngineIFace.h"
#include "inc/Cv4CommonEngineLib/CvEngine.h"
#include "CvAppUtil.h"

#include "Common.h"
#include "CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvMap.h>

#include <CommonStuff/StringConversion.h>

//#include <iostream>

using namespace cvengine;

MyCvDLLEngineIFace& MyCvDLLEngineIFace::getInstance()
{
	static MyCvDLLEngineIFace s;
	return s;
}

void MyCvDLLEngineIFace::cameraLookAt([[maybe_unused]] NiPoint3 lookingPoint) {}
bool MyCvDLLEngineIFace::isCameraLocked() { return false; }

void MyCvDLLEngineIFace::SetObeyEntityVisibleFlags([[maybe_unused]] bool bObeyHide) { std::abort(); }
void MyCvDLLEngineIFace::AutoSave(bool bInitial)
{
	gCommonEngineConfig.engine->autoSave(bInitial);
}
void MyCvDLLEngineIFace::SaveReplay(PlayerTypes ePlayer)
{
	gCommonEngineConfig.engine->saveReplay(ePlayer);
}
void MyCvDLLEngineIFace::SaveGame(CvString& szFilename, [[maybe_unused]] SaveGameTypes eType)
{
	//std::abort();

	// This is only called by player bot I think, so assume UTF8.
	const std::filesystem::path defPath = gCommonEngineConfig.userDataDirPath / gCommonEngineConfig.savesDirName / kSavesSingleDirName / heck::convertUtf8ToWide(szFilename);

	if (const auto optPath = gCommonEngineConfig.callbackHandler->promptSaveGamePath(defPath))
	{
		FFile<StdRawBinaryStream> file{ StdRawBinaryStream(*optPath, std::ios::out | std::ios::binary) };
		app::serialise(file);
	}
}

void MyCvDLLEngineIFace::DoTurn()
{
	// Just stick this here.
	//clearColoredPlots(PLOT_LANDSCAPE_LAYER_ALL);

	// Direct debug output for when logging AI autoplay to file.
	//cvengine::outputDebugString(("MyCvDLLEngineIFace::DoTurn: " + std::to_string(gGlobals.getGame().getGameTurn()) + '\n').c_str());

	gCommonEngineConfig.engine->doTurn();
}
void MyCvDLLEngineIFace::ClearMinimap()
{
	// Not used?
	std::abort();
}
uint8_t MyCvDLLEngineIFace::GetLandscapePlotTerrainData([[maybe_unused]] unsigned int uiX, [[maybe_unused]] unsigned int uiY, [[maybe_unused]] unsigned int uiPointX, [[maybe_unused]] unsigned int uiPointY)
{
	// Not used?
	std::abort();
}
uint8_t MyCvDLLEngineIFace::GetLandscapePlotHeightData([[maybe_unused]] unsigned int uiX, [[maybe_unused]] unsigned int uiY, [[maybe_unused]] unsigned int uiPointX, [[maybe_unused]] unsigned int uiPointY)
{
	// Not used?
	abort();
}
LoadType MyCvDLLEngineIFace::getLoadType()
{
	// Not used?
	abort();
}
void MyCvDLLEngineIFace::ClampToWorldCoords([[maybe_unused]] NiPoint3* pPt3, [[maybe_unused]] float fOffset)
{
	// Not used?
	abort();
}
void MyCvDLLEngineIFace::SetCameraZoom([[maybe_unused]] float zoom)
{
	// Not used?
	abort();
}
float MyCvDLLEngineIFace::GetUpdateRate()
{
	// Not used?
	abort();
}
bool MyCvDLLEngineIFace::SetUpdateRate([[maybe_unused]] float fUpdateRate)
{
	// Not used?
	abort();
}
void MyCvDLLEngineIFace::toggleGlobeview()
{
	gCommonEngineConfig.engine->toggleGlobeview();
}
bool MyCvDLLEngineIFace::isGlobeviewUp()
{
	return gCommonEngineConfig.engine->isGlobeviewUp();
}
void MyCvDLLEngineIFace::toggleResourceLayer()
{
	gCommonEngineConfig.engine->toggleResourceLayer();
}
void MyCvDLLEngineIFace::toggleUnitLayer()
{
	gCommonEngineConfig.engine->toggleUnitLayer();
}
void MyCvDLLEngineIFace::setResourceLayer(bool bOn)
{
	gCommonEngineConfig.engine->setResourceLayer(bOn);
}

void MyCvDLLEngineIFace::MoveBaseTurnRight(float increment)
{
	gCommonEngineConfig.engine->moveBaseTurnRight(increment);
}
void MyCvDLLEngineIFace::MoveBaseTurnLeft(float increment)
{
	gCommonEngineConfig.engine->moveBaseTurnLeft(increment);
}
void MyCvDLLEngineIFace::SetFlying(bool value)
{
	gCommonEngineConfig.engine->setFlying(value);
}
void MyCvDLLEngineIFace::CycleFlyingMode(int displacement)
{
	gCommonEngineConfig.engine->cycleFlyingMode(displacement);
}
void MyCvDLLEngineIFace::SetMouseFlying(bool value)
{
	gCommonEngineConfig.engine->setMouseFlying(value);
}
void MyCvDLLEngineIFace::SetSatelliteMode(bool value)
{
	gCommonEngineConfig.engine->setSatelliteMode(value);
}
void MyCvDLLEngineIFace::SetOrthoCamera(bool value)
{
	gCommonEngineConfig.engine->setOrthoCamera(value);
}
bool MyCvDLLEngineIFace::GetFlying()
{
	return gCommonEngineConfig.engine->getFlying();
}
bool MyCvDLLEngineIFace::GetMouseFlying()
{
	return gCommonEngineConfig.engine->getMouseFlying();
}
bool MyCvDLLEngineIFace::GetSatelliteMode()
{
	return gCommonEngineConfig.engine->getSatelliteMode();
}
bool MyCvDLLEngineIFace::GetOrthoCamera()
{
	return gCommonEngineConfig.engine->getOrthoCamera();
}

// landscape
int MyCvDLLEngineIFace::InitGraphics()
{
	// Not used?
	abort();
}
void MyCvDLLEngineIFace::GetLandscapeDimensions([[maybe_unused]] float& fWidth, [[maybe_unused]] float& fHeight)
{
	// Not used?
	abort();
}
// Called when founding a city to calculate where to make the camera look at.
void MyCvDLLEngineIFace::GetLandscapeGameDimensions(float& fWidth, float& fHeight)
{
	const CvMap& map = gGlobals.getMap();
	fWidth = map.getGridWidth() * gGlobals.getPLOT_SIZE();
	fHeight = map.getGridHeight() * gGlobals.getPLOT_SIZE();
}
unsigned int MyCvDLLEngineIFace::GetGameCellSizeX()
{
	// Not used?
	abort();
}
unsigned int MyCvDLLEngineIFace::GetGameCellSizeY()
{
	// Not used?
	abort();
}
float MyCvDLLEngineIFace::GetPointZSpacing()
{
	// Not used?
	abort();
}
float MyCvDLLEngineIFace::GetPointXYSpacing()
{
	// Not used?
	abort();
}
float MyCvDLLEngineIFace::GetPointXSpacing()
{
	// Not used?
	abort();
}
float MyCvDLLEngineIFace::GetPointYSpacing()
{
	// Not used?
	abort();
}

// Called when founding a city to calculate where to make the camera look at.
// This is a flat world; everything is on a plane.
float MyCvDLLEngineIFace::GetHeightmapZ(const NiPoint3& pt3, bool bClampAboveWater)
{
	return gCommonEngineConfig.engine->getHeightmapZ(pt3, bClampAboveWater);
}

// Invert int CvPlot::getFOWIndex() const
static heck::ivec2 decodeFOWIndex(unsigned int i)
{
	const unsigned int fowWidth = gGlobals.getMap().getGridWidth() * LANDSCAPE_FOW_RESOLUTION;
	const int height = gGlobals.getMap().getGridHeight();
	const unsigned int fowY = i / fowWidth;
	const unsigned int fowX = i % fowWidth;
	return { int(fowX / LANDSCAPE_FOW_RESOLUTION), height - 1 - int(fowY / LANDSCAPE_FOW_RESOLUTION) };
}

void MyCvDLLEngineIFace::LightenVisibility(unsigned int i)
{
	//CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kLightened);
	gCommonEngineConfig.engine->lightenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::DarkenVisibility(unsigned int i)
{
	//CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kDarkened);
	gCommonEngineConfig.engine->darkenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::BlackenVisibility(unsigned int i)
{
	//CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kBlackened);
	gCommonEngineConfig.engine->blackenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::RebuildAllPlots()
{
	//CvInterface::getInstance().invalidateAllPlots();
	gCommonEngineConfig.engine->rebuildAllPlots();
}
void MyCvDLLEngineIFace::RebuildPlot(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	gCommonEngineConfig.engine->rebuildPlot({ plotX, plotY }, bRebuildHeights, bRebuildTextures);
}
void MyCvDLLEngineIFace::RebuildRiverPlotTile(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures)
{
	//// TODO: Invalidate adjacent?
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	gCommonEngineConfig.engine->rebuildRiverPlotTile({ plotX, plotY }, bRebuildHeights, bRebuildTextures);
}
void MyCvDLLEngineIFace::RebuildTileArt(int plotX, int plotY)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	gCommonEngineConfig.engine->rebuildTileArt({ plotX, plotY });
}
void MyCvDLLEngineIFace::ForceTreeOffsets(int plotX, int plotY)
{
	// TODO: This is used to cut off trees at rivers.
	gCommonEngineConfig.engine->forceTreeOffsets({ plotX, plotY });
}

bool MyCvDLLEngineIFace::GetGridMode() { abort(); }
void MyCvDLLEngineIFace::SetGridMode([[maybe_unused]] bool bVal) { abort(); }

void MyCvDLLEngineIFace::addColoredPlot(int plotX, int plotY, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer)
{
	//CvInterface::getInstance().getWorldView().setColouredPlot({ plotX, plotY }, { color, plotStyle, layer });
	gCommonEngineConfig.engine->addColoredPlot({ plotX, plotY }, color, plotStyle, layer);
}
void MyCvDLLEngineIFace::clearColoredPlots(PlotLandscapeLayers layer)
{
	//CvInterface::getInstance().getWorldView().clearColouredPlots(layer);
	gCommonEngineConfig.engine->clearColoredPlots(layer);
}
void MyCvDLLEngineIFace::fillAreaBorderPlot(int plotX, int plotY, const NiColorA& color, AreaBorderLayers layer)
{
	gCommonEngineConfig.engine->fillAreaBorderPlot({ plotX, plotY }, color, layer);
}
void MyCvDLLEngineIFace::clearAreaBorderPlots(AreaBorderLayers layer)
{
	gCommonEngineConfig.engine->clearAreaBorderPlots(layer);
}
void MyCvDLLEngineIFace::updateFoundingBorder()
{
	gCommonEngineConfig.engine->updateFoundingBorder();
}
void MyCvDLLEngineIFace::addLandmark(CvPlot* plot, const wchar_t* caption)
{
	//abort();
	gCommonEngineConfig.engine->addLandmark(plot, caption);
}

void MyCvDLLEngineIFace::TriggerEffect(int iEffect, NiPoint3 pt3Point, float rotation)
{
	// If you want to display effects in the turn message log.
	//CyInterface().addMessage(gGlobals.getGame().getActivePlayer(), false, gGlobals.getDefineINT("EVENT_MESSAGE_TIME_LONG"),
	//	gGlobals.getEffectInfo(iEffect).getDescription(),
	//	std::nullopt, InterfaceMessageTypes::MESSAGE_TYPE_INFO,
	//	std::nullopt, NO_COLOR, -1, -1, false, false
	//);
	gCommonEngineConfig.engine->triggerEffect(iEffect, pt3Point, rotation);
}
void MyCvDLLEngineIFace::printProfileText()
{
	//abort();
}

void MyCvDLLEngineIFace::clearSigns()
{
	gCommonEngineConfig.engine->clearSigns();
}
CvPlot* MyCvDLLEngineIFace::pickPlot(int x, int y, NiPoint3& worldPoint)
{
	return gCommonEngineConfig.engine->pickPlot({ x, y }, worldPoint);
}

// dirty bits
void MyCvDLLEngineIFace::SetDirty(EngineDirtyBits eBit, bool bNewValue)
{
	gCommonEngineConfig.engine->setDirty(eBit, bNewValue);
}
bool MyCvDLLEngineIFace::IsDirty(EngineDirtyBits eBit)
{
	return gCommonEngineConfig.engine->isDirty(eBit);
}
void MyCvDLLEngineIFace::PushFogOfWar(FogOfWarModeTypes eNewMode)
{
	gCommonEngineConfig.engine->pushFogOfWar(eNewMode);
}
FogOfWarModeTypes MyCvDLLEngineIFace::PopFogOfWar() // used for debug mode
{
	return gCommonEngineConfig.engine->popFogOfWar();
}
void MyCvDLLEngineIFace::setFogOfWarFromStack() // used for debug mode
{
	gCommonEngineConfig.engine->setFogOfWarFromStack();
}
void MyCvDLLEngineIFace::MarkBridgesDirty()
{
	gCommonEngineConfig.engine->markBridgesDirty();
}
void MyCvDLLEngineIFace::AddLaunch(PlayerTypes playerType)
{
	//std::wclog << CvPlayerAI::getPlayerNonInl(playerType).getName() << L": Imagine your spaceship launching from somewhere" << std::endl;
	gCommonEngineConfig.engine->addLaunch(playerType);
}
void MyCvDLLEngineIFace::AddGreatWall(CvCity* city)
{
	gCommonEngineConfig.engine->addGreatWall(city);
}
void MyCvDLLEngineIFace::RemoveGreatWall(CvCity* city)
{
	gCommonEngineConfig.engine->removeGreatWall(city);
}
void MyCvDLLEngineIFace::MarkPlotTextureAsDirty(int plotX, int plotY)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	gCommonEngineConfig.engine->markPlotTextureAsDirty({ plotX, plotY });
}