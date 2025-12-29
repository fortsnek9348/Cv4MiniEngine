#include "MyCvDLLEngineIFace.h"
#include "../WorldView.h"
#include "../CvInterface.h"
#include "../Common.h"
#include "../Logger.h"
#include "../PythonAPI/CyInterface.h" // temporary for addMessage
#include "../MyFFile.h"
#include "../CvApp.h"
#include "../IniReader.h"
#include "../CvEngine.h"

#include <CvGlobals.h>
#include <CvMap.h>
#include <CvGameAI.h>
#include <CvInfos.h>
#include <CvPlayerAI.h>
#include <CvGameTextMgr.h>
#include <CvDLLUtilityIFaceBase.h>
#include <CvInitCore.h>

#include <iostream>

void MyCvDLLEngineIFace::cameraLookAt([[maybe_unused]] NiPoint3 lookingPoint) {}
bool MyCvDLLEngineIFace::isCameraLocked() { return false; }

void MyCvDLLEngineIFace::SetObeyEntityVisibleFlags([[maybe_unused]] bool bObeyHide) { std::abort(); }
void MyCvDLLEngineIFace::AutoSave(bool bInitial)
{
	CvEngine::getInstance().AutoSave(bInitial);
}
void MyCvDLLEngineIFace::SaveReplay(PlayerTypes ePlayer)
{
	//cvengine::logInfo("TODO");

	CvWString timeStr;
	CvGameTextMgr::GetInstance().setTimeStr(timeStr, gGlobals.getGame().getGameTurn(), true);
	const std::filesystem::path dir = cvengine::getUserDataDir() / cvengine::kReplaysDirName;
	std::filesystem::path path;
	for (int i = 0; i < 1000; ++i)
	{
		path = dir / std::format(L"{}_{}_{}.CivBeyondSwordReplay",
			static_cast<const std::wstring&>(gGlobals.getInitCore().getGameName()),
			static_cast<const std::wstring&>(timeStr),
			i
		);
		FFile<StdRawBinaryStream> file(path, std::ios::out | std::ios::binary | std::ios::noreplace);
		if (file.stream.stream)
		{
			gGlobals.getGame().writeReplay(file, ePlayer);
			std::clog << "Replay saved to " << path << std::endl;
			return;
		}
	}

	std::clog << "Could not save replay to " << path << std::endl;
}
void MyCvDLLEngineIFace::SaveGame([[maybe_unused]] CvString& szFilename, [[maybe_unused]] SaveGameTypes eType) { std::abort(); }

void MyCvDLLEngineIFace::DoTurn()
{
	// Just stick this here.
	clearColoredPlots(PLOT_LANDSCAPE_LAYER_ALL);

	// Direct debug output for when logging AI autoplay to file.
	cvengine::outputDebugString(("MyCvDLLEngineIFace::DoTurn: " + std::to_string(gGlobals.getGame().getGameTurn()) + '\n').c_str());
}
void MyCvDLLEngineIFace::ClearMinimap()	 { std::abort(); }
uint8_t MyCvDLLEngineIFace::GetLandscapePlotTerrainData([[maybe_unused]] unsigned int uiX, [[maybe_unused]] unsigned int uiY, [[maybe_unused]] unsigned int uiPointX, [[maybe_unused]] unsigned int uiPointY) { abort(); }
uint8_t MyCvDLLEngineIFace::GetLandscapePlotHeightData([[maybe_unused]] unsigned int uiX, [[maybe_unused]] unsigned int uiY, [[maybe_unused]] unsigned int uiPointX, [[maybe_unused]] unsigned int uiPointY) { abort(); }
LoadType MyCvDLLEngineIFace::getLoadType() { abort(); }
void MyCvDLLEngineIFace::ClampToWorldCoords([[maybe_unused]] NiPoint3* pPt3, [[maybe_unused]] float fOffset) { abort(); }
void MyCvDLLEngineIFace::SetCameraZoom([[maybe_unused]] float zoom) { abort(); }
float MyCvDLLEngineIFace::GetUpdateRate() { abort(); }
bool MyCvDLLEngineIFace::SetUpdateRate([[maybe_unused]] float fUpdateRate) { abort(); }
void MyCvDLLEngineIFace::toggleGlobeview() { abort(); }
bool MyCvDLLEngineIFace::isGlobeviewUp() { abort(); }
void MyCvDLLEngineIFace::toggleResourceLayer() { abort(); }
void MyCvDLLEngineIFace::toggleUnitLayer() { abort(); }
void MyCvDLLEngineIFace::setResourceLayer([[maybe_unused]] bool bOn) { abort(); }

void MyCvDLLEngineIFace::MoveBaseTurnRight([[maybe_unused]] float increment) { abort(); }
void MyCvDLLEngineIFace::MoveBaseTurnLeft([[maybe_unused]] float increment) { abort(); }
void MyCvDLLEngineIFace::SetFlying([[maybe_unused]] bool value) { abort(); }
void MyCvDLLEngineIFace::CycleFlyingMode([[maybe_unused]] int displacement) { abort(); }
void MyCvDLLEngineIFace::SetMouseFlying([[maybe_unused]] bool value) { abort(); }
void MyCvDLLEngineIFace::SetSatelliteMode([[maybe_unused]] bool value) { abort(); }
void MyCvDLLEngineIFace::SetOrthoCamera([[maybe_unused]] bool value) { abort(); }
bool MyCvDLLEngineIFace::GetFlying() { abort(); }
bool MyCvDLLEngineIFace::GetMouseFlying() { abort(); }
bool MyCvDLLEngineIFace::GetSatelliteMode() { abort(); }
bool MyCvDLLEngineIFace::GetOrthoCamera() { abort(); }

// landscape
int MyCvDLLEngineIFace::InitGraphics() { abort(); }
void MyCvDLLEngineIFace::GetLandscapeDimensions([[maybe_unused]] float& fWidth, [[maybe_unused]] float& fHeight) { abort(); }
// Called when founding a city to calculate where to make the camera look at.
// Just assume 1 unit == 1 plot.
void MyCvDLLEngineIFace::GetLandscapeGameDimensions(float& fWidth, float& fHeight)
{
	const CvMap& map = gGlobals.getMap();
	fWidth = map.getGridWidth() * gGlobals.getPLOT_SIZE();
	fHeight = map.getGridHeight() * gGlobals.getPLOT_SIZE();
}
unsigned int MyCvDLLEngineIFace::GetGameCellSizeX() { abort(); }
unsigned int MyCvDLLEngineIFace::GetGameCellSizeY() { abort(); }
float MyCvDLLEngineIFace::GetPointZSpacing() { abort(); }
float MyCvDLLEngineIFace::GetPointXYSpacing() { abort(); }
float MyCvDLLEngineIFace::GetPointXSpacing() { abort(); }
float MyCvDLLEngineIFace::GetPointYSpacing() { abort(); }

// Called when founding a city to calculate where to make the camera look at.
// This is a flat world; everything is on a plane.
float MyCvDLLEngineIFace::GetHeightmapZ([[maybe_unused]] const NiPoint3& pt3, [[maybe_unused]] bool bClampAboveWater)
{
	return 0;
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
	CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kLightened);
}
void MyCvDLLEngineIFace::DarkenVisibility(unsigned int i)
{
	CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kDarkened);
}
void MyCvDLLEngineIFace::BlackenVisibility(unsigned int i)
{
	CvInterface::getInstance().getWorldView().changePlotVisibility(decodeFOWIndex(i), WorldView::kBlackened);
}
void MyCvDLLEngineIFace::RebuildAllPlots()
{
	CvInterface::getInstance().invalidateAllPlots();
}
void MyCvDLLEngineIFace::RebuildPlot(int plotX, int plotY, [[maybe_unused]] bool bRebuildHeights, [[maybe_unused]] bool bRebuildTextures)
{
	CvInterface::getInstance().invalidatePlot({ plotX, plotY });
}
void MyCvDLLEngineIFace::RebuildRiverPlotTile(int plotX, int plotY, [[maybe_unused]] bool bRebuildHeights, [[maybe_unused]] bool bRebuildTextures)
{
	// TODO: Invalidate adjacent?
	CvInterface::getInstance().invalidatePlot({ plotX, plotY });
}
void MyCvDLLEngineIFace::RebuildTileArt(int plotX, int plotY)
{
	CvInterface::getInstance().invalidatePlot({ plotX, plotY });
}
void MyCvDLLEngineIFace::ForceTreeOffsets([[maybe_unused]] int plotX, [[maybe_unused]] int plotY)
{
	// TODO: This is used to cut off tree at rivers.
}

bool MyCvDLLEngineIFace::GetGridMode() { abort(); }
void MyCvDLLEngineIFace::SetGridMode([[maybe_unused]] bool bVal) { abort(); }

void MyCvDLLEngineIFace::addColoredPlot(int plotX, int plotY, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer)
{
	CvInterface::getInstance().getWorldView().setColouredPlot({ plotX, plotY }, { color, plotStyle, layer });
}
void MyCvDLLEngineIFace::clearColoredPlots(PlotLandscapeLayers layer)
{
	CvInterface::getInstance().getWorldView().clearColouredPlots(layer);
}
void MyCvDLLEngineIFace::fillAreaBorderPlot([[maybe_unused]] int plotX, [[maybe_unused]] int plotY, [[maybe_unused]] const NiColorA& color, [[maybe_unused]] AreaBorderLayers layer) { }
void MyCvDLLEngineIFace::clearAreaBorderPlots([[maybe_unused]] AreaBorderLayers layer) { }
void MyCvDLLEngineIFace::updateFoundingBorder() { }
void MyCvDLLEngineIFace::addLandmark([[maybe_unused]] CvPlot* plot, [[maybe_unused]] const wchar_t* caption) { abort(); }

void MyCvDLLEngineIFace::TriggerEffect([[maybe_unused]] int iEffect, [[maybe_unused]] NiPoint3 pt3Point, [[maybe_unused]] float rotation)
{
	// If you want to display effects in the turn message log.
	//CyInterface().addMessage(gGlobals.getGame().getActivePlayer(), false, gGlobals.getDefineINT("EVENT_MESSAGE_TIME_LONG"),
	//	gGlobals.getEffectInfo(iEffect).getDescription(),
	//	std::nullopt, InterfaceMessageTypes::MESSAGE_TYPE_INFO,
	//	std::nullopt, NO_COLOR, -1, -1, false, false
	//);
}
void MyCvDLLEngineIFace::printProfileText() { abort(); }

void MyCvDLLEngineIFace::clearSigns() { abort(); }
CvPlot* MyCvDLLEngineIFace::pickPlot([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] NiPoint3& worldPoint) { abort(); }

// dirty bits
void MyCvDLLEngineIFace::SetDirty([[maybe_unused]] EngineDirtyBits eBit, [[maybe_unused]] bool bNewValue) { /**/ }
bool MyCvDLLEngineIFace::IsDirty([[maybe_unused]] EngineDirtyBits eBit) { abort(); }
void MyCvDLLEngineIFace::PushFogOfWar([[maybe_unused]] FogOfWarModeTypes eNewMode) { /*abort();*/ } // used for debug mode
FogOfWarModeTypes MyCvDLLEngineIFace::PopFogOfWar() { abort(); }
void MyCvDLLEngineIFace::setFogOfWarFromStack() { /*abort();*/ } // used for debug mode
void MyCvDLLEngineIFace::MarkBridgesDirty() { }
void MyCvDLLEngineIFace::AddLaunch(PlayerTypes playerType)
{
	std::wclog << CvPlayerAI::getPlayerNonInl(playerType).getName() << L": Imagine your spaceship launching from somewhere" << std::endl;
}
void MyCvDLLEngineIFace::AddGreatWall([[maybe_unused]] CvCity* city) {  }
void MyCvDLLEngineIFace::RemoveGreatWall([[maybe_unused]] CvCity* city) {  }
void MyCvDLLEngineIFace::MarkPlotTextureAsDirty(int plotX, int plotY)
{
	CvInterface::getInstance().invalidatePlot({ plotX, plotY });
}