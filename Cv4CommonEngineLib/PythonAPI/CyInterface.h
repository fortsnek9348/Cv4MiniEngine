#pragma once

#include <CvEnums.h>

#include <pybind11/pybind11.h>

#include <string>
#include <vector>

class CyCity;
class CyPlot;
class CyUnit;

class CyInterface
{
public:
	static void registerWithPython(const pybind11::module& m);

	bool DoSoundtrack(const std::string& szSoundtrackScript);
	void addCombatMessage(PlayerTypes ePlayer, const std::wstring& szString);
	void addImmediateMessage(const std::wstring& szString, const std::string& szSound);
	// BUG needs optional args.
	void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, const std::wstring& szString, const std::optional<std::string>& szSound, InterfaceMessageTypes eTypes, const std::optional<std::string>& szIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows);
	void addQuestMessage(PlayerTypes ePlayer, const std::string& szString);
	void addSelectedCity(CyCity pNewValue);
	void cacheInterfacePlotUnits(CyPlot*);
	bool canCreateGroup();
	bool canDeleteGroup();
	bool canHandleAction(int iAction, bool bTestVisible);
	bool canSelectHeadUnit();
	void checkFlashReset(PlayerTypes ePlayer);
	bool checkFlashUpdate();
	void clearSelectedCities();
	void clearSelectionList();
	int countEntities(int iI);
	int determineWidth(const std::wstring& szBuffer);
	void doPing(int iX, int iY, PlayerTypes ePlayer);
	void endTimer(const std::string& szOutputText);
	void exitingToMainMenu(const std::string& szLoadFile);
	std::vector<int> getActionsToShow();
	CyUnit getCachedInterfacePlotUnit(int iIndex);
	int getCityTabSelectionRow();
	CyPlot getCursorPlot();
	EndTurnButtonStates getEndTurnState();
	CyPlot getGotoPlot();
	std::unique_ptr<CyCity> getHeadSelectedCity();
	std::unique_ptr<CyUnit> getHeadSelectedUnit();
	std::string getHelpString();
	CyPlot getHighlightPlot();
	InterfaceModeTypes getInterfaceMode();
	CyUnit getInterfacePlotUnit(CyPlot plot, int index);
	int getLengthSelectionList();
	CyPlot getMouseOverPlot();
	//POINT getMousePos();
	int getNumCachedInterfacePlotUnits();
	int getNumOrdersQueued();
	int getNumVisibleUnits();
	int getOrderNodeData1(int iNode);
	int getOrderNodeData2(int iNode);
	bool getOrderNodeSave(int iNode);
	OrderTypes getOrderNodeType(int iNode);
	int getPlotListColumn();
	int getPlotListOffset();
	std::unique_ptr<CyPlot> getSelectionPlot();
	std::unique_ptr<CyUnit> getSelectionUnit(int index); // Param not documented. Used by BUG stack sel info.
	InterfaceVisibility getShowInterface();
	void insertIntoSelectionList(CyUnit pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound);
	bool isCityScreenUp();
	bool isCitySelected(CyCity pCity);
	bool isCitySelection();
	bool isDirty(InterfaceDirtyBits eDirty);
	bool isFlashing();
	bool isFlashingPlayer(int iPlayer);
	bool isFocusedWidget();
	bool isInAdvancedStart();
	bool isInMainMenu();
	bool isLeftMouseDown();
	bool isNetStatsVisible();
	bool isOOSVisible();
	bool isOneCitySelected();
	bool isRightMouseDown();
	bool isScoresMinimized();
	bool isScoresVisible();
	bool isScreenUp(int iEnumVal);
	bool isUnitFocus();
	bool isYieldVisibleMode();
	void lookAtCityBuilding(int iCity, int iBuilding);
	void lookAtCityOffset(int iCity);
	void makeInterfaceDirty();
	bool mirrorsSelectionGroup();
	bool noTechSplash();
	void playAdvisorSound(const std::string& pszSound);
	void playGeneralSound(const std::string& pszSound);
	void playGeneralSoundAtPlot(int iScriptID, CyPlot pPlot);
	void playGeneralSoundByID(int iScriptID);
	void removeFromSelectionList(CyUnit pUnit);
	void selectAll(CyPlot pPlot);
	void selectCity(CyCity pNewValue, bool bTestProduction);
	void selectGroup(CyUnit pUnit, bool bShift, bool bCtrl, bool bAlt);
	int selectHotKeyUnit(int iHotKeyNumber);
	void selectUnit(CyUnit pUnit, bool bClear, bool bToggle, bool bSound);
	void setBusy(bool bBusy);
	void setCityTabSelectionRow(CityTabTypes eIndex);
	void setDirty(InterfaceDirtyBits eDirty, bool bDirty);
	void setInterfaceMode(InterfaceModeTypes eMode);
	void setPausedPopups(bool bPausedPopups);
	void setShowInterface(InterfaceVisibility eInterfaceVisibility);
	void setSoundSelectionReady(bool bReady);
	void setWorldBuilder(bool bTurnOn);
	bool shiftKey();
	bool shouldDisplayEndTurn();
	bool shouldDisplayEndTurnButton();
	bool shouldDisplayFlag();
	bool shouldDisplayReturn();
	bool shouldDisplayUnitModel();
	bool shouldDisplayWaitingOthers();
	bool shouldDisplayWaitingYou();
	bool shouldFlash(int iPlayer);
	bool shouldShowAction(int iAction);
	bool shouldShowChangeResearchButton();
	bool shouldShowResearchButtons();
	bool shouldShowSelectionButtons();
	bool shouldShowYieldVisibleButton();
	void startTimer();
	void stop2DSound();
	void stopAdvisorSound();
	void toggleBareMapMode();
	void toggleMusicOn();
	void toggleNetStatsVisible();
	void toggleScoresMinimized();
	void toggleScoresVisible();
	void toggleYieldVisibleMode();
};

