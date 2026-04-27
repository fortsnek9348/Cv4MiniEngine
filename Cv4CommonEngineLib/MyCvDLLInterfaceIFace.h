#pragma once

#include <CvGameCoreDLL/CvDLLInterfaceIFaceBase.h>

class MyCvDLLInterfaceIFace : public CvDLLInterfaceIFaceBase
{
public:
	static MyCvDLLInterfaceIFace& getInstance();

	virtual void lookAtSelectionPlot(bool bRelease) override;

	virtual bool canHandleAction(int iAction, CvPlot* pPlot, bool bTestVisible) override;
	virtual bool canDoInterfaceMode(InterfaceModeTypes eInterfaceMode, CvSelectionGroup* pSelectionGroup) override;

	virtual CvPlot* getLookAtPlot() override;
	virtual CvPlot* getSelectionPlot() override;
	virtual CvUnit* getInterfacePlotUnit(const CvPlot* pPlot, int iIndex) override;
	virtual CvUnit* getSelectionUnit(int iIndex) override;
	virtual CvUnit* getHeadSelectedUnit() override;
	virtual void selectUnit(CvUnit* pUnit, bool bClear, bool bToggle, bool bSound) override;
	virtual void selectGroup(CvUnit* pUnit, bool bShift, bool bCtrl, bool bAlt) override;
	virtual void selectAll(CvPlot* pPlot) override;

	virtual bool removeFromSelectionList(CvUnit* pUnit) override;
	virtual void makeSelectionListDirty() override;
	virtual bool mirrorsSelectionGroup() override;
	virtual bool canSelectionListFound() override;

	virtual void bringToTop(CvPopup* pPopup) override;
	virtual bool isPopupUp() override;
	virtual bool isPopupQueued() override;
	virtual bool isDiploOrPopupWaiting() override;

	virtual CvUnit* getLastSelectedUnit() override;
	virtual void setLastSelectedUnit(CvUnit* pUnit) override;
	virtual void changePlotListColumn(int iChange) override;
	virtual CvPlot* getGotoPlot() override;
	virtual CvPlot* getSingleMoveGotoPlot() override;
	virtual CvPlot* getOriginalPlot() override;

	virtual void playGeneralSound(const TCHAR* pszSound, NiPoint3 vPos) override;
	virtual void playGeneralSound(int iSoundId, int iSoundType, NiPoint3 vPos) override;
	virtual void clearQueuedPopups() override;

	virtual CvSelectionGroup* getSelectionList() override;
	virtual void clearSelectionList() override;
	virtual void insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange) override;
	virtual void selectionListPostChange() override;
	virtual void selectionListPreChange() override;
	virtual int getSymbolID(int iSymbol) override;
	virtual CLLNode<IDInfo>* deleteSelectionListNode(CLLNode<IDInfo>* pNode) override;
	virtual CLLNode<IDInfo>* nextSelectionListNode(CLLNode<IDInfo>* pNode) override;
	virtual int getLengthSelectionList() override;
	virtual CLLNode<IDInfo>* headSelectionListNode() override;

	virtual void selectCity(CvCity* pNewValue, bool bTestProduction) override;
	virtual void selectLookAtCity(bool bAdd) override;
	virtual void addSelectedCity(CvCity* pNewValue, bool bToggle) override;
	virtual void clearSelectedCities() override;
	virtual bool isCitySelected(CvCity* pCity) override;
	virtual CvCity* getHeadSelectedCity() override;
	virtual bool isCitySelection() override;
	virtual CLLNode<IDInfo>* nextSelectedCitiesNode(CLLNode<IDInfo>* pNode) override;
	virtual CLLNode<IDInfo>* headSelectedCitiesNode() override;

	virtual void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
		InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor,
		int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows) override;
	virtual void addCombatMessage(PlayerTypes ePlayer, CvWString szString) override;
	virtual void addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId) override;
	virtual void showMessage(CvTalkingHeadMessage& msg) override;
	virtual void flushTalkingHeadMessages() override;
	virtual void clearEventMessages() override;
	virtual void addPopup(CvPopupInfo* pInfo, PlayerTypes ePlayer, bool bImmediate, bool bFront) override;
	virtual void getDisplayedButtonPopups(CvPopupQueue& infos) override;

	virtual int getCycleSelectionCounter() override;
	virtual void setCycleSelectionCounter(int iNewValue) override;
	virtual void changeCycleSelectionCounter(int iChange) override;

	virtual int getEndTurnCounter() override;
	virtual void setEndTurnCounter(int iNewValue) override;
	virtual void changeEndTurnCounter(int iChange) override;

	virtual bool isCombatFocus() override;
	virtual void setCombatFocus(bool bNewValue) override;
	virtual void setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override;

	virtual bool isDirty(InterfaceDirtyBits eDirtyItem) override;
	virtual void setDirty(InterfaceDirtyBits eDirtyItem, bool bNewValue) override;
	virtual void makeInterfaceDirty() override;
	virtual bool updateCursorType() override;
	virtual void updatePythonScreens() override;

	virtual void lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection) override;
	virtual void centerCamera(CvUnit*) override;
	virtual void releaseLockedCamera() override;
	virtual bool isFocusedWidget() override;
	virtual bool isFocused() override;
	virtual bool isBareMapMode() override;
	virtual void toggleBareMapMode() override;
	virtual bool isShowYields() override;
	virtual void toggleYieldVisibleMode() override;
	virtual bool isScoresVisible() override;
	virtual void toggleScoresVisible() override;
	virtual bool isScoresMinimized() override;
	virtual void toggleScoresMinimized() override;
	virtual bool isNetStatsVisible() override;

	virtual int getOriginalPlotCount() override;
	virtual bool isCityScreenUp() override;
	virtual bool isEndTurnMessage() override;
	virtual void setInterfaceMode(InterfaceModeTypes eNewValue) override;
	virtual InterfaceModeTypes getInterfaceMode() override;
	virtual InterfaceVisibility getShowInterface() override;
	virtual CvPlot* getMouseOverPlot() override;
	virtual void setFlashing(PlayerTypes eWho, bool bFlashing) override;
	virtual bool isFlashing(PlayerTypes eWho) override;
	virtual void setDiplomacyLocked(bool bLocked) override;
	virtual bool isDiplomacyLocked() override;

	virtual void setMinimapColor(MinimapModeTypes eMinimapMode, int iX, int iY, ColorTypes eColor, float fAlpha) override;
	virtual unsigned char* getMinimapBaseTexture() const override;
	virtual void setEndTurnMessage(bool bNewValue) override;

	virtual bool isHasMovedUnit() override;
	virtual void setHasMovedUnit(bool bNewValue) override;

	virtual bool isForcePopup() override;
	virtual void setForcePopup(bool bNewValue) override;

	virtual void lookAtCityOffset(int iCity) override;

	virtual void toggleTurnLog() override;
	virtual void showTurnLog(ChatTargetTypes eTarget) override;
	virtual void dirtyTurnLog(PlayerTypes ePlayer) override;

	virtual int getPlotListColumn() override;
	virtual void verifyPlotListColumn() override;
	virtual int getPlotListOffset() override;

	virtual void unlockPopupHelp() override;

	virtual void showDetails(bool bPasswordOnly) override;
	virtual void showAdminDetails() override;

	virtual void toggleClockAlarm(bool bValue, int iHour, int iMin) override;
	virtual bool isClockAlarmOn() override;

	virtual void setScreenDying(int iPythonFileID, bool bDying) override;
	virtual bool isExitingToMainMenu() override;
	virtual void exitingToMainMenu(const char* szLoadFile) override;
	virtual void setWorldBuilder(bool bTurnOn) override;

	virtual int getFontLeftJustify() override;
	virtual int getFontRightJustify() override;
	virtual int getFontCenterJustify() override;
	virtual int getFontCenterVertically() override;
	virtual int getFontAdditive() override;

	virtual void popupSetHeaderString(CvPopup* pPopup, CvWString szText, unsigned int uiFlags) override;
	virtual void popupSetBodyString(CvPopup* pPopup, CvWString szText, unsigned int uiFlags, char* szName, CvWString szHelpText) override;
	virtual void popupLaunch(CvPopup* pPopup, bool bCreateOkButton, PopupStates bState, int iNumPixelScroll) override;
	virtual void popupSetPopupType(CvPopup* pPopup, PopupEventTypes ePopupType, const TCHAR* szArtFileName) override;
	virtual void popupSetStyle(CvPopup* pPopup, const char* styleId) override;

	virtual void popupAddDDS(CvPopup* pPopup, const char* szIconFilename, int iWidth, int iHeight, CvWString szHelpText) override;

	virtual void popupAddSeparator(CvPopup* pPopup, int iSpace) override;

	virtual void popupAddGenericButton(CvPopup* pPopup, CvWString szText, const char* szIcon, int iButtonId, WidgetTypes eWidgetType, int iData1, int iData2,
		bool bOption, PopupControlLayout ctrlLayout, unsigned int textJustifcation) override;

	virtual void popupCreateEditBox(CvPopup* pPopup, CvWString szDefaultString, WidgetTypes eWidgetType, CvWString szHelpText, int iGroup,
		PopupControlLayout ctrlLayout, unsigned int preferredCharWidth, unsigned int maxCharCount) override;
	virtual void popupEnableEditBox(CvPopup* pPopup, int iGroup, bool bEnable) override;

	virtual void popupCreateRadioButtons(CvPopup* pPopup, int iNumButtons, int iGroup, WidgetTypes eWidgetType, PopupControlLayout ctrlLayout) override;
	virtual void popupSetRadioButtonText(CvPopup* pPopup, int iRadioButtonID, CvWString szText, int iGroup, CvWString szHelpText) override;

	virtual void popupCreateCheckBoxes(CvPopup* pPopup, int iNumBoxes, int iGroup, WidgetTypes eWidgetType, PopupControlLayout ctrlLayout) override;
	virtual void popupSetCheckBoxText(CvPopup* pPopup, int iCheckBoxID, CvWString szText, int iGroup, CvWString szHelpText) override;
	virtual void popupSetCheckBoxState(CvPopup* pPopup, int iCheckBoxID, bool bChecked, int iGroup) override;

	virtual void popupSetAsCancelled(CvPopup* pPopup) override;
	virtual bool popupIsDying(CvPopup* pPopup) override;
	virtual void setCityTabSelectionRow(CityTabTypes eTabType) override;

	virtual bool noTechSplash() override;

	virtual bool isInAdvancedStart() const override;
	virtual void setInAdvancedStart(bool bAdvancedStart) override;

	virtual bool isSpaceshipScreenUp() const override;
	virtual bool isDebugMenuCreated() const override;

	virtual void setBusy(bool bBusy) override;

	virtual void getInterfaceScreenIdsForInput(std::vector<int>& aIds) override;
	virtual void doPing(int iX, int iY, PlayerTypes ePlayer) override;

private:
	MyCvDLLInterfaceIFace() = default;
};