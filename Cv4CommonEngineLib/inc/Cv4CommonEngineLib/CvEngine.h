#pragma once

#include "MyFFile.h"

#include <CvEnums.h>
#include <CommonShared.h>

#include <CommonStuff/vec.h>

#include <unordered_map>

class CvPlot;
class CvCity;

struct CvSign
{
	PlayerTypes playerI{};
	heck::ivec2 coord{};
	std::wstring caption;
};

class CvEngine
{
public:
	using ivec2 = heck::ivec2;

	virtual void reset();

	void clearSigns();
	void setSignText(PlayerTypes playerI, heck::ivec2 coord, std::wstring text);
	std::wstring_view findSignTextAt(PlayerTypes playerI, heck::ivec2 coord) const;
	int getNumSigns() const;
	CvSign getSignByIndex(size_t i) const;

	// A landmark is just a sign for all players?
	void addLandmark(const CvPlot* plot, const std::wstring& caption);
	void removeLandmark(ivec2);
	void removeSign(PlayerTypes playerI, ivec2);

	void serialise(FFile<StdRawBinaryStream>& file) const;
	void deserialise(FFile<StdRawBinaryStream>& file);

	void autoSave(bool bInitial);
	void saveReplay(PlayerTypes ePlayer);

	virtual void doTurn() = 0;

	virtual void toggleGlobeview() = 0;
	virtual bool isGlobeviewUp() = 0;
	virtual void toggleResourceLayer() = 0;
	virtual void toggleUnitLayer() = 0;
	virtual void setResourceLayer(bool bOn) = 0;

	virtual void moveBaseTurnRight(float increment) = 0;
	virtual void moveBaseTurnLeft(float increment) = 0;
	virtual void setFlying(bool value) = 0;
	virtual void cycleFlyingMode(int displacement) = 0;
	virtual void setMouseFlying(bool value) = 0;
	virtual void setSatelliteMode(bool value) = 0;
	virtual void setOrthoCamera(bool value) = 0;
	virtual bool getFlying() = 0;
	virtual bool getMouseFlying() = 0;
	virtual bool getSatelliteMode() = 0;
	virtual bool getOrthoCamera() = 0;

	virtual bool getCityBillboardVisibility() const = 0;
	virtual void setCityBillboardVisibility(bool) = 0;
	virtual bool getCultureVisibility() const = 0;
	virtual void setCultureVisibility(bool) const = 0;
	virtual bool getSelectionCursorVisibility() const = 0;
	virtual void setSelectionCursorVisibility(bool) = 0;
	virtual bool getUnitFlagVisibility() const = 0;
	virtual void setUnitFlagVisibility(bool) = 0;

	// landscape
	virtual void getLandscapeGameDimensions(float& fWidth, float& fHeight) = 0;
	
	virtual float getHeightmapZ(const NiPoint3& pt3, bool bClampAboveWater) = 0;

	virtual void lightenVisibility(ivec2 coord) = 0;
	virtual void darkenVisibility(ivec2 coord) = 0;
	virtual void blackenVisibility(ivec2 coord) = 0;
	virtual void rebuildAllPlots() = 0;
	virtual void rebuildPlot(ivec2 coord, bool bRebuildHeights, bool bRebuildTextures) = 0;
	virtual void rebuildRiverPlotTile(ivec2 coord, bool bRebuildHeights, bool bRebuildTextures) = 0;
	virtual void rebuildTileArt(ivec2 coord) = 0;
	// This is used to cut off tree at rivers.
	virtual void forceTreeOffsets(ivec2 coord) = 0;

	virtual bool getGridMode() = 0;
	virtual void setGridMode(bool bVal) = 0;

	virtual void addColoredPlot(ivec2 coord, const NiColorA& color, PlotStyles plotStyle, PlotLandscapeLayers layer) = 0;
	virtual void clearColoredPlots(PlotLandscapeLayers layer) = 0;
	virtual void fillAreaBorderPlot(ivec2 coord, const NiColorA& color, AreaBorderLayers layer) = 0;
	virtual void clearAreaBorderPlots(AreaBorderLayers layer) = 0;
	virtual void updateFoundingBorder() = 0;
	

	virtual void triggerEffect(int iEffect, NiPoint3 pt3Point, float rotation) = 0;

	virtual CvPlot* pickPlot(ivec2 coord, NiPoint3& worldPoint) = 0;

	// dirty bits
	virtual void setDirty(EngineDirtyBits eBit, bool bNewValue) = 0;
	virtual bool isDirty(EngineDirtyBits eBit) = 0;
	// used for debug mode
	virtual void pushFogOfWar(FogOfWarModeTypes eNewMode) = 0;
	virtual FogOfWarModeTypes popFogOfWar() = 0;
	// used for debug mode
	virtual void setFogOfWarFromStack() = 0;
	virtual void markBridgesDirty() = 0;
	virtual void addLaunch(PlayerTypes playerType) = 0;
	virtual void addGreatWall(CvCity* city) = 0;
	virtual void removeGreatWall(CvCity* city) = 0;
	virtual void markPlotTextureAsDirty(ivec2 coord) = 0;

	virtual ~CvEngine() = default;

private:
	int mAutoSaveCounter{};

	struct SignKey
	{
		PlayerTypes playerI{};
		heck::ivec2 coord{};

		friend bool operator==(SignKey, SignKey) = default;
		bool operator<(SignKey) const;
	};

	struct Hasher
	{
		static size_t operator()(SignKey) noexcept;
	};

	std::unordered_map<SignKey, std::wstring, Hasher> mSigns;
};