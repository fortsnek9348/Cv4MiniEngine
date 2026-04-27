#include "MyCvDLLEngineIFace.h"
#include "inc/Cv4CommonEngineLib/CvEngine.h"
#include "inc/Cv4CommonEngineLib/EngineSpecificsHeader.h"
#include "inc/Cv4CommonEngineLib/Common.h"
#include "inc/Cv4CommonEngineLib/CvAppUtil.h"

#include <CvGlobals.h>
#include <CvMap.h>

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
	engine_specific::getCvEngine().autoSave(bInitial);
}
void MyCvDLLEngineIFace::SaveReplay(PlayerTypes ePlayer)
{
	engine_specific::getCvEngine().saveReplay(ePlayer);
}
void MyCvDLLEngineIFace::SaveGame(CvString& szFilename, [[maybe_unused]] SaveGameTypes eType)
{
	//std::abort();

	// This is only called by player bot I think, so assume UTF8.
	const std::filesystem::path defPath = getUserDataDir() / kSavesDirName / kSavesSingleDirName / heck::convertUtf8ToWide(szFilename);

	if (const auto optPath = engine_specific::promptSaveGamePath(defPath))
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

	engine_specific::getCvEngine().doTurn();
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
	engine_specific::getCvEngine().toggleGlobeview();
}
bool MyCvDLLEngineIFace::isGlobeviewUp()
{
	return engine_specific::getCvEngine().isGlobeviewUp();
}
void MyCvDLLEngineIFace::toggleResourceLayer()
{
	engine_specific::getCvEngine().toggleResourceLayer();
}
void MyCvDLLEngineIFace::toggleUnitLayer()
{
	engine_specific::getCvEngine().toggleUnitLayer();
}
void MyCvDLLEngineIFace::setResourceLayer(bool bOn)
{
	engine_specific::getCvEngine().setResourceLayer(bOn);
}

void MyCvDLLEngineIFace::MoveBaseTurnRight(float increment)
{
	engine_specific::getCvEngine().moveBaseTurnRight(increment);
}
void MyCvDLLEngineIFace::MoveBaseTurnLeft(float increment)
{
	engine_specific::getCvEngine().moveBaseTurnLeft(increment);
}
void MyCvDLLEngineIFace::SetFlying(bool value)
{
	engine_specific::getCvEngine().setFlying(value);
}
void MyCvDLLEngineIFace::CycleFlyingMode(int displacement)
{
	engine_specific::getCvEngine().cycleFlyingMode(displacement);
}
void MyCvDLLEngineIFace::SetMouseFlying(bool value)
{
	engine_specific::getCvEngine().setMouseFlying(value);
}
void MyCvDLLEngineIFace::SetSatelliteMode(bool value)
{
	engine_specific::getCvEngine().setSatelliteMode(value);
}
void MyCvDLLEngineIFace::SetOrthoCamera(bool value)
{
	engine_specific::getCvEngine().setOrthoCamera(value);
}
bool MyCvDLLEngineIFace::GetFlying()
{
	return engine_specific::getCvEngine().getFlying();
}
bool MyCvDLLEngineIFace::GetMouseFlying()
{
	return engine_specific::getCvEngine().getMouseFlying();
}
bool MyCvDLLEngineIFace::GetSatelliteMode()
{
	return engine_specific::getCvEngine().getSatelliteMode();
}
bool MyCvDLLEngineIFace::GetOrthoCamera()
{
	return engine_specific::getCvEngine().getOrthoCamera();
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
	return engine_specific::getCvEngine().getHeightmapZ(pt3, bClampAboveWater);
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
	engine_specific::getCvEngine().lightenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::DarkenVisibility(unsigned int i)
{
	//CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kDarkened);
	engine_specific::getCvEngine().darkenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::BlackenVisibility(unsigned int i)
{
	//CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kBlackened);
	engine_specific::getCvEngine().blackenVisibility(decodeFOWIndex(i));
}
void MyCvDLLEngineIFace::RebuildAllPlots()
{
	//CvInterface::getInstance().invalidateAllPlots();
	engine_specific::getCvEngine().rebuildAllPlots();
}
void MyCvDLLEngineIFace::RebuildPlot(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	engine_specific::getCvEngine().rebuildPlot({ plotX, plotY }, bRebuildHeights, bRebuildTextures);
}
void MyCvDLLEngineIFace::RebuildRiverPlotTile(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures)
{
	//// TODO: Invalidate adjacent?
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	engine_specific::getCvEngine().rebuildRiverPlotTile({ plotX, plotY }, bRebuildHeights, bRebuildTextures);
}
void MyCvDLLEngineIFace::RebuildTileArt(int plotX, int plotY)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	engine_specific::getCvEngine().rebuildTileArt({ plotX, plotY });
}
void MyCvDLLEngineIFace::ForceTreeOffsets(int plotX, int plotY)
{
	// TODO: This is used to cut off trees at rivers.
	engine_specific::getCvEngine().forceTreeOffsets({ plotX, plotY });
}

bool MyCvDLLEngineIFace::GetGridMode() { abort(); }
void MyCvDLLEngineIFace::SetGridMode([[maybe_unused]] bool bVal) { abort(); }

void MyCvDLLEngineIFace::addColoredPlot(int plotX, int plotY, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer)
{
	//CvInterface::getInstance().getWorldView().setColouredPlot({ plotX, plotY }, { color, plotStyle, layer });
	engine_specific::getCvEngine().addColoredPlot({ plotX, plotY }, color, plotStyle, layer);
}
void MyCvDLLEngineIFace::clearColoredPlots(PlotLandscapeLayers layer)
{
	//CvInterface::getInstance().getWorldView().clearColouredPlots(layer);
	engine_specific::getCvEngine().clearColoredPlots(layer);
}
void MyCvDLLEngineIFace::fillAreaBorderPlot(int plotX, int plotY, const NiColorA& color, AreaBorderLayers layer)
{
	engine_specific::getCvEngine().fillAreaBorderPlot({ plotX, plotY }, color, layer);
}
void MyCvDLLEngineIFace::clearAreaBorderPlots(AreaBorderLayers layer)
{
	engine_specific::getCvEngine().clearAreaBorderPlots(layer);
}
void MyCvDLLEngineIFace::updateFoundingBorder()
{
	engine_specific::getCvEngine().updateFoundingBorder();
}
void MyCvDLLEngineIFace::addLandmark(CvPlot* plot, const wchar_t* caption)
{
	//abort();
	engine_specific::getCvEngine().addLandmark(plot, caption);
}

void MyCvDLLEngineIFace::TriggerEffect(int iEffect, NiPoint3 pt3Point, float rotation)
{
	// If you want to display effects in the turn message log.
	//CyInterface().addMessage(gGlobals.getGame().getActivePlayer(), false, gGlobals.getDefineINT("EVENT_MESSAGE_TIME_LONG"),
	//	gGlobals.getEffectInfo(iEffect).getDescription(),
	//	std::nullopt, InterfaceMessageTypes::MESSAGE_TYPE_INFO,
	//	std::nullopt, NO_COLOR, -1, -1, false, false
	//);
	engine_specific::getCvEngine().triggerEffect(iEffect, pt3Point, rotation);
}
void MyCvDLLEngineIFace::printProfileText()
{
	//abort();
}

void MyCvDLLEngineIFace::clearSigns()
{
	engine_specific::getCvEngine().clearSigns();
}
CvPlot* MyCvDLLEngineIFace::pickPlot(int x, int y, NiPoint3& worldPoint)
{
	return engine_specific::getCvEngine().pickPlot({ x, y }, worldPoint);
}

// dirty bits
void MyCvDLLEngineIFace::SetDirty(EngineDirtyBits eBit, bool bNewValue)
{
	engine_specific::getCvEngine().setDirty(eBit, bNewValue);
}
bool MyCvDLLEngineIFace::IsDirty(EngineDirtyBits eBit)
{
	return engine_specific::getCvEngine().isDirty(eBit);
}
void MyCvDLLEngineIFace::PushFogOfWar(FogOfWarModeTypes eNewMode)
{
	engine_specific::getCvEngine().pushFogOfWar(eNewMode);
}
FogOfWarModeTypes MyCvDLLEngineIFace::PopFogOfWar() // used for debug mode
{
	return engine_specific::getCvEngine().popFogOfWar();
}
void MyCvDLLEngineIFace::setFogOfWarFromStack() // used for debug mode
{
	engine_specific::getCvEngine().setFogOfWarFromStack();
}
void MyCvDLLEngineIFace::MarkBridgesDirty()
{
	engine_specific::getCvEngine().markBridgesDirty();
}
void MyCvDLLEngineIFace::AddLaunch(PlayerTypes playerType)
{
	//std::wclog << CvPlayerAI::getPlayerNonInl(playerType).getName() << L": Imagine your spaceship launching from somewhere" << std::endl;
	engine_specific::getCvEngine().addLaunch(playerType);
}
void MyCvDLLEngineIFace::AddGreatWall(CvCity* city)
{
	engine_specific::getCvEngine().addGreatWall(city);
}
void MyCvDLLEngineIFace::RemoveGreatWall(CvCity* city)
{
	engine_specific::getCvEngine().removeGreatWall(city);
}
void MyCvDLLEngineIFace::MarkPlotTextureAsDirty(int plotX, int plotY)
{
	//CvInterface::getInstance().invalidatePlot({ plotX, plotY });
	engine_specific::getCvEngine().markPlotTextureAsDirty({ plotX, plotY });
}