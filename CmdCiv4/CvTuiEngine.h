#pragma once

#include <Cv4CommonEngineLib/CvEngine.h>

#include <CvGameCoreDLL/CvEnums.h>

#include <CommonStuff/vec.h>

#include <unordered_map>

namespace cvengine
{
	class CvTuiEngine : public CvEngine
	{
	public:
		static CvTuiEngine& getInstance();

		virtual void doTurn() override;

		virtual void toggleGlobeview() override;
		virtual bool isGlobeviewUp() override;
		virtual void toggleResourceLayer() override;
		virtual void toggleUnitLayer() override;
		virtual void setResourceLayer(bool bOn) override;

		virtual void moveBaseTurnRight(float increment) override;
		virtual void moveBaseTurnLeft(float increment) override;
		virtual void setFlying(bool value) override;
		virtual void cycleFlyingMode(int displacement) override;
		virtual void setMouseFlying(bool value) override;
		virtual void setSatelliteMode(bool value) override;
		virtual void setOrthoCamera(bool value) override;
		virtual bool getFlying() override;
		virtual bool getMouseFlying() override;
		virtual bool getSatelliteMode() override;
		virtual bool getOrthoCamera() override;

		virtual bool getCityBillboardVisibility() const override;
		virtual void setCityBillboardVisibility(bool) override;
		virtual bool getCultureVisibility() const override;
		virtual void setCultureVisibility(bool) const override;
		virtual bool getSelectionCursorVisibility() const override;
		virtual void setSelectionCursorVisibility(bool) override;
		virtual bool getUnitFlagVisibility() const override;
		virtual void setUnitFlagVisibility(bool) override;

		// landscape
		virtual void getLandscapeGameDimensions(float& fWidth, float& fHeight) override;

		virtual float getHeightmapZ(const NiPoint3& pt3, bool bClampAboveWater) override;

		virtual void lightenVisibility(ivec2 coord) override;
		virtual void darkenVisibility(ivec2 coord) override;
		virtual void blackenVisibility(ivec2 coord) override;
		virtual void rebuildAllPlots() override;
		virtual void rebuildPlot(ivec2 coord, bool bRebuildHeights, bool bRebuildTextures) override;
		virtual void rebuildRiverPlotTile(ivec2 coord, bool bRebuildHeights, bool bRebuildTextures) override;
		virtual void rebuildTileArt(ivec2 coord) override;
		virtual void forceTreeOffsets(ivec2 coord) override;

		virtual bool getGridMode() override;
		virtual void setGridMode(bool bVal) override;

		virtual void addColoredPlot(ivec2 coord, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer) override;
		virtual void clearColoredPlots(PlotLandscapeLayers layer) override;
		virtual void fillAreaBorderPlot(ivec2 coord, const NiColorA& color, AreaBorderLayers layer) override;
		virtual void clearAreaBorderPlots(AreaBorderLayers layer) override;
		virtual void updateFoundingBorder() override;


		virtual void triggerEffect(int iEffect, NiPoint3 pt3Point, float rotation) override;

		virtual CvPlot* pickPlot(ivec2 coord, NiPoint3& worldPoint) override;

		// dirty bits
		virtual void setDirty(EngineDirtyBits eBit, bool bNewValue) override;
		virtual bool isDirty(EngineDirtyBits eBit) override;
		virtual void pushFogOfWar(FogOfWarModeTypes eNewMode) override;
		virtual FogOfWarModeTypes popFogOfWar() override;
		virtual void setFogOfWarFromStack() override;
		virtual void markBridgesDirty() override;
		virtual void addLaunch(PlayerTypes playerType) override;
		virtual void addGreatWall(CvCity* city) override;
		virtual void removeGreatWall(CvCity* city) override;
		virtual void markPlotTextureAsDirty(ivec2 coord) override;
	};
}