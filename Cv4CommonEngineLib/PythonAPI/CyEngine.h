#pragma once

#include "../inc/Cv4CommonEngineLib/CvEngine.h"

#include <CommonShared.h>
#include <CvEnums.h>

#include <pybind11/pybind11.h>

#include <string>

class CyPlot;

class CySign
{
public:
	static void registerWithPython(const pybind11::module& m);

	CvSign data;

	const std::wstring& getCaption();
	PlayerTypes getPlayerType();
	CyPlot getPlot();
};

class CyEngine
{
public:
	static void registerWithPython(const pybind11::module& m);

	void addColoredPlot(int plotX, int plotY, NiColorA color, int iLayer);
	void addColoredPlotAlt(int plotX, int plotY, int iPlotStyle, int iLayer, const std::string& szColor, float fAlpha);
	void addLandmark(CyPlot pPlot, const std::wstring& caption);
	void addLandmarkPopup(CyPlot pPlot);
	void addSign(CyPlot plot, PlayerTypes playerType, const std::wstring& caption);
	void clearAreaBorderPlots(int iLayer);
	void clearColoredPlots(int iLayer);
	void fillAreaBorderPlot(int plotX, int plotY, NiColorA color, int iLayer);
	void fillAreaBorderPlotAlt(int plotX, int plotY, int iLayer, const std::string& szColor, float fAlpha);
	bool getCityBillboardVisibility();
	bool getCultureVisibility();
	int getNumSigns();
	bool getSelectionCursorVisibility();
	CySign getSignByIndex(int index);
	bool getUnitFlagVisibility();
	float getUpdateRate();
	bool isDirty(EngineDirtyBits eBit);
	bool isGlobeviewUp();
	// fortsnek: Don't know what purpose this has.
	//bool isNone();
	void reloadEffectInfos();
	void removeLandmark(CyPlot pPlot);
	void removeSign(CyPlot pPlot, PlayerTypes playerType);
	void setCityBillboardVisibility(bool bState);
	void setCultureVisibility(bool bState);
	void setDirty(EngineDirtyBits eBit, bool bNewValue);
	void setFogOfWar(bool bState);
	void setSelectionCursorVisibility(bool bState);
	void setUnitFlagVisibility(bool bState);
	void setUpdateRate(float fUpdateRate);
	void toggleGlobeview();
	void triggerEffect(int iEffect, NiPoint3 plotPoint);
};

