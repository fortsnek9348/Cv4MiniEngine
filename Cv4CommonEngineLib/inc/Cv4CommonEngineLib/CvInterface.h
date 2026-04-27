#pragma once

#include <CvGameCoreDLL/CvPlayer.h>
#include <CvGameCoreDLL/CvSelectionGroupAI.h>

#include <CommonStuff/vec.h>

class CvPopup;
class CvUnit;

class CvInterface
{
public:
	using ivec2 = heck::ivec2;

	CvInterface();

	virtual void reset();
	virtual void uninit();

	// Used in CvOptionsScreenCallbackInterface.py.
	virtual bool isInMainMenu() const = 0;
	virtual bool isLeftMouseDown() const = 0;
	virtual bool isRightMouseDown() const = 0;

	virtual void startMainInterface() = 0;

	/// Camera control
	virtual void lookAtPlot(ivec2 coord) = 0;
	virtual void lookAtCityBuilding(int iCityId, BuildingTypes iBuilding) = 0;
	virtual void lookAtSelectionPlot() = 0;
	virtual void lookAtCityOffset(int iCity) = 0;
	virtual CvPlot* getLookAtPlot() const = 0;
	virtual CvPlot* getMouseOverPlot() const = 0;
	virtual CvPlot* getSelectedCityOrUnitPlot() const = 0;
	
	virtual const CvSelectionGroup* getSelectionList() const = 0;
	virtual CvSelectionGroup* getSelectionList() = 0;
	int getNumSelectedUnits() const;
	CvUnit* getSelectionUnit(int iIndex) const;
	bool isUnitSelected(const CvUnit& unit) const;
	bool removeFromSelectionList(CvUnit* pUnit);
	virtual void makeSelectionListDirty() = 0;
	bool mirrorsSelectionGroup() const;
	void clearSelectionList();
	void insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange);
	virtual void selectionListPostChange() = 0;
	virtual void selectionListPreChange() = 0;

	virtual void clearQueuedPopups() = 0;
	virtual bool isDiploOrPopupWaiting() const = 0;
	// This is what finally gets called after CvDLLButtonPopup has filled in the controls.
	// Implementation should behave according to PopupStates, and set IPopupUIWindow when displayed.
	virtual void launchPopup(std::unique_ptr<CvPopup> pPopup, bool bCreateOkButton, PopupStates bState, int iNumPixelScroll) = 0;

	// CONTROL_LASTUNIT
	virtual CvUnit* getLastSelectedUnit() const = 0;
	virtual void setLastSelectedUnit(CvUnit* pUnit) = 0;
	virtual CvPlot* getGotoPlot() const = 0;
	virtual CvPlot* getSingleMoveGotoPlot() const = 0;
	virtual CvPlot* getOriginalUnitSelectionPlot() const = 0;
	virtual int getOriginalUnitSelectionIndex() const = 0;

	// Possibly called by Rhye's and Fall of Civilization.
	// Index should be a 2D sound as GAME_MUSIC.
	virtual void doSoundtrack(int index) = 0;
	virtual void play2DSound(int index) = 0;
	virtual void play3DSound(int index, const NiPoint3& vPos) = 0;
	// iSoundType is probably the sound category as listed in AudioDefines.xml.
	//virtual void playGeneralSound(int iSoundId, int iSoundType, NiPoint3 vPos) = 0;
	virtual void playUnitSelectionSound(const CvUnit& unit) = 0;
	// Called after CvDawnOfMan screen.
	virtual void setSoundSelectionReady(bool bReady) = 0;

	// `bTestProduction` is deferred until exit city screen.
	virtual void selectCity(CvCity* pNewValue, bool bTestProduction) = 0;
	virtual void selectLookAtCity(bool bAdd) = 0;
	virtual void addSelectedCity(CvCity* pNewValue, bool bToggle) = 0;
	virtual void clearSelectedCities() = 0;
	virtual bool isCitySelected(CvCity* pCity) const = 0;
	virtual CvCity* getHeadSelectedCity() const = 0;
	virtual bool isCitySelection() const = 0;
	virtual CLLNode<IDInfo>* nextSelectedCitiesNode(CLLNode<IDInfo>* pNode) = 0;
	virtual CLLNode<IDInfo>* headSelectedCitiesNode() = 0;
	virtual bool isCityScreenUp() const = 0;

	virtual void addTurnMessageDisplayEntry(CvTalkingHeadMessage msg) = 0;
	virtual void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
		InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor,
		int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows) = 0;
	//virtual void addCombatMessage(PlayerTypes ePlayer, CvWString szString) = 0;
	virtual void addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId) = 0;
	//virtual void showMessage(CvTalkingHeadMessage& msg) = 0;
	// This function is called when the game-active player is no longer turn-active.
	// Maybe this function is supposed to be used to flush queued messaged onto the screen.
	virtual void flushTalkingHeadMessages() = 0;
	//virtual void clearEventMessages() = 0;
	virtual void getDisplayedButtonPopups(CvPopupQueue& infos) const = 0;

	virtual int getCycleSelectionCounter() const = 0;
	virtual void setCycleSelectionCounter(int iNewValue) = 0;
	virtual void changeCycleSelectionCounter(int iChange) = 0;

	virtual bool isCombatFocus() const = 0;
	virtual void setCombatFocus(bool bNewValue) = 0;
	//virtual void setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) = 0;

	virtual bool getInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem) const = 0;
	virtual void setInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem, bool bNewValue) = 0;
	virtual void updateCursorType() = 0;
	virtual void updatePythonScreens() = 0;

	virtual void lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection) = 0;
	virtual void centerCamera(CvUnit*) = 0;
	virtual void releaseLockedCamera() = 0;
	virtual bool isFocusedWidget() const = 0;
	virtual bool isFocused() const = 0;
	virtual bool isBareMapMode() const = 0;
	virtual void toggleBareMapMode() = 0;
	virtual bool isShowYields() const = 0;
	virtual void toggleYieldVisibleMode() = 0;
	virtual bool isScoresVisible() const = 0;
	virtual void toggleScoresVisible() = 0;
	virtual bool isScoresMinimized() const = 0;
	virtual void toggleScoresMinimized() = 0;
	//virtual bool isNetStatsVisible() const = 0;

	
	
	virtual void setInterfaceMode(InterfaceModeTypes eNewValue) = 0;
	void resetInterfaceMode();
	virtual InterfaceModeTypes getInterfaceMode() const = 0;
	virtual InterfaceVisibility getShowInterface() const = 0;
	virtual void setShowInterface(InterfaceVisibility eInterfaceVisibility) = 0;
	
	//virtual void setFlashing(PlayerTypes eWho, bool bFlashing) = 0;
	//virtual bool isFlashing(PlayerTypes eWho) const = 0;
	//virtual void setDiplomacyLocked(bool bLocked) = 0;
	//virtual bool isDiplomacyLocked() const = 0;

	virtual void setMinimapColor(MinimapModeTypes eMinimapMode, ivec2 coord, ColorTypes eColor, float fAlpha) = 0;
	virtual unsigned char* getMinimapBaseTextureDXT1() const = 0;

	// These seem to be used when PLAYEROPTION_WAIT_END_TURN is off.
	virtual int getEndTurnCounter() const = 0;
	virtual void setEndTurnCounter(int iNewValue) = 0;
	virtual void changeEndTurnCounter(int iChange) = 0;
	
	virtual void setEndTurnMessage(bool bNewValue) = 0;
	virtual bool isEndTurnMessage() const = 0;
	
	// The message?
	virtual bool shouldDisplayEndTurn() const = 0;
	// Flag by the minimap
	virtual bool shouldDisplayFlag() const = 0;
	// City screen return message?
	virtual bool shouldDisplayReturn() const = 0;
	// In the bottom left?
	virtual bool shouldDisplayUnitModel() const = 0;
	virtual bool shouldShowResearchButtons() const = 0;

	// Only called if not wait at end of turn?
	bool isHasMovedUnit() const;
	void setHasMovedUnit(bool bNewValue);

	virtual bool isForcePopup() const = 0;
	virtual void setForcePopup(bool bNewValue) = 0;

	/// Turn log
	virtual void toggleTurnLog() = 0;
	virtual void dirtyTurnLog(PlayerTypes ePlayer) = 0;

	//virtual void unlockPopupHelp() = 0;

	virtual void showDetails(bool bPasswordOnly) = 0;
	virtual void showAdminDetails() = 0;

	virtual void toggleClockAlarm(bool bValue, int iHour, int iMin) = 0;
	virtual bool isClockAlarmOn() const = 0;

	virtual bool isExitingToMainMenu() const = 0;
	virtual void exitToMainMenu() = 0;
	virtual void setWorldBuilder(bool bTurnOn) = 0;
 
	// Called by scripts.
	virtual int determineWidth(const std::wstring& szBuffer) const = 0;
	
	virtual void setCityTabSelectionRow(CityTabTypes eTabType) = 0;
	virtual CityTabTypes getCityTabSelectionRow() const = 0;

	virtual bool noTechSplash() const = 0;

	virtual bool isInAdvancedStart() const = 0;
	virtual void setInAdvancedStart(bool bAdvancedStart) = 0;

	virtual bool isSpaceshipScreenUp() const = 0;
	//virtual bool isDebugMenuCreated() const = 0;

	virtual void setBusy(bool bBusy) = 0;

	//virtual void getInterfaceScreenIdsForInput(std::vector<int>& aIds) const = 0;
	//virtual void doPing(int iX, int iY, PlayerTypes ePlayer) = 0;


	/// Plot list UI
	// Untested. But maybe it's a buggy mess in BTS anyway...
	
	// case WIDGET_PLOT_LIST_SHIFT:
	// 	gDLL->getInterfaceIFace()->changePlotListColumn(widgetDataStruct.m_iData1 * ((gDLL->ctrlKey()) ? (GC.getDefineINT("MAX_PLOT_LIST_SIZE") - 1) : 1));
	// 	break;
	void changePlotListColumn(int iChange);
	int getPlotListColumn() const;
	void verifyPlotListColumn();
	// What is offset?
	// int iUnitIndex = widgetDataStruct.m_iData1 + gDLL->getInterfaceIFace()->getPlotListColumn() - gDLL->getInterfaceIFace()->getPlotListOffset();
	int getPlotListOffset() const;
	void cacheInterfacePlotUnits(CvPlot& plot);
	// Used by main interface
	int countEntities(UnitTypes unitType);
	// How does the indexing lagic work in CvMainInterface?
	// Well, it's weird. Say you have more tanks than can fit on a row. Those tanks will continue on the next row, AND ALSO, continue on the same row as you scroll. It's weird.
	// And furthermore, if you have a huge mixed unit stack, and you select all of one unit type (ctrl+click), it says 62 units. You count them up in the plot list and you get 56 units.
	// Only a few more in the list when you scroll, right? No, it's looks like you have hoards more.
	// So, the plot list is totally freaky and doesn't make sense.
	CvUnit* getCachedInterfacePlotUnit(int iIndex) const;
	int getNumCachedInterfacePlotUnits() const;
	// Is this getNumCachedInterfacePlotUnits?
	// elif ((iCount == (gc.getMAX_PLOT_LIST_ROWS() * self.numPlotListButtons() - 1)) and ((iVisibleUnits - iCount - CyInterface().getPlotListColumn() + iSkipped) > 1)) :
	// 	bRightArrow = True
	// if (iVisibleUnits > self.numPlotListButtons() * iMaxRows):
	int getNumVisibleUnits() const;

	static bool isActiveVisibleUnit(const CvUnit& unit);

	virtual ~CvInterface() = default;

private:
	//CvSelectionGroupAI mInterfaceSelectionGroup;
	bool mIsHasMovedUnit{};

	struct CachedPlotList
	{
		int column{};
		std::vector<IDInfo> unitIds;
	};

	CachedPlotList mCachedPlotList;

	bool tryRemoveUnitFromSelectionGroup(CvUnit* pUnit);
};