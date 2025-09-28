#pragma once

#include <CvDLLEngineIFaceBase.h>

class WorldView;

struct MyCvDLLEngineIFace : CvDLLEngineIFaceBase
{
	virtual void cameraLookAt(NiPoint3 lookingPoint) override;
	virtual bool isCameraLocked() override;

	virtual void SetObeyEntityVisibleFlags(bool bObeyHide) override;
	virtual void AutoSave(bool bInitial = false) override;
	virtual void SaveReplay(PlayerTypes ePlayer = NO_PLAYER) override;
	virtual void SaveGame(CvString& szFilename, SaveGameTypes eType = SAVEGAME_NORMAL) override;

	virtual void DoTurn() override;

	virtual void ClearMinimap()	override;
	virtual uint8_t GetLandscapePlotTerrainData(unsigned int uiX, unsigned int uiY, unsigned int uiPointX, unsigned int uiPointY) override;
	virtual uint8_t GetLandscapePlotHeightData(unsigned int uiX, unsigned int uiY, unsigned int uiPointX, unsigned int uiPointY) override;
	virtual LoadType getLoadType() override;
	virtual void ClampToWorldCoords(NiPoint3* pPt3, float fOffset = 0.0f) override;
	virtual void SetCameraZoom(float zoom) override;
	virtual float GetUpdateRate() override;
	virtual bool SetUpdateRate(float fUpdateRate) override;
	virtual void toggleGlobeview() override;
	virtual bool isGlobeviewUp() override;
	virtual void toggleResourceLayer() override;
	virtual void toggleUnitLayer() override;
	virtual void setResourceLayer(bool bOn) override;

	virtual void MoveBaseTurnRight(float increment = 45) override;
	virtual void MoveBaseTurnLeft(float increment = 45) override;
	virtual void SetFlying(bool value) override;
	virtual void CycleFlyingMode(int displacement) override;
	virtual void SetMouseFlying(bool value) override;
	virtual void SetSatelliteMode(bool value) override;
	virtual void SetOrthoCamera(bool value) override;
	virtual bool GetFlying() override;
	virtual bool GetMouseFlying() override;
	virtual bool GetSatelliteMode() override;
	virtual bool GetOrthoCamera() override;

	// landscape
	virtual int InitGraphics() override;
	virtual void GetLandscapeDimensions(float& fWidth, float& fHeight) override;
	virtual void GetLandscapeGameDimensions(float& fWidth, float& fHeight) override;
	virtual unsigned int GetGameCellSizeX() override;
	virtual unsigned int GetGameCellSizeY() override;
	virtual float GetPointZSpacing() override;
	virtual float GetPointXYSpacing() override;
	virtual float GetPointXSpacing() override;
	virtual float GetPointYSpacing() override;
	virtual float GetHeightmapZ(const NiPoint3& pt3, bool bClampAboveWater = true) override;
	virtual void LightenVisibility(unsigned int) override;
	virtual void DarkenVisibility(unsigned int) override;
	virtual void BlackenVisibility(unsigned int) override;
	virtual void RebuildAllPlots() override;
	virtual void RebuildPlot(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures) override;
	virtual void RebuildRiverPlotTile(int plotX, int plotY, bool bRebuildHeights, bool bRebuildTextures) override;
	virtual void RebuildTileArt(int plotX, int plotY) override;
	virtual void ForceTreeOffsets(int plotX, int plotY) override;

	virtual bool GetGridMode() override;
	virtual void SetGridMode(bool bVal) override;

	virtual void addColoredPlot(int plotX, int plotY, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer) override;
	virtual void clearColoredPlots(PlotLandscapeLayers layer) override;
	virtual void fillAreaBorderPlot(int plotX, int plotY, const NiColorA& color, AreaBorderLayers layer) override;
	virtual void clearAreaBorderPlots(AreaBorderLayers layer) override;
	virtual void updateFoundingBorder() override;
	virtual void addLandmark(CvPlot* plot, const wchar_t* caption) override;

	virtual void TriggerEffect(int iEffect, NiPoint3 pt3Point, float rotation = 0.0f) override;
	virtual void printProfileText() override;

	virtual void clearSigns() override;
	virtual CvPlot* pickPlot(int x, int y, NiPoint3& worldPoint) override;

	// dirty bits
	virtual void SetDirty(EngineDirtyBits eBit, bool bNewValue) override;
	virtual bool IsDirty(EngineDirtyBits eBit) override;
	virtual void PushFogOfWar(FogOfWarModeTypes eNewMode) override;
	virtual FogOfWarModeTypes PopFogOfWar() override;
	virtual void setFogOfWarFromStack() override;
	virtual void MarkBridgesDirty() override;
	virtual void AddLaunch(PlayerTypes playerType) override;
	virtual void AddGreatWall(CvCity* city) override;
	virtual void RemoveGreatWall(CvCity* city) override;
	virtual void MarkPlotTextureAsDirty(int plotX, int plotY) override;
};