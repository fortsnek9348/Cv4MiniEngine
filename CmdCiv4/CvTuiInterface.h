#pragma once

#include <Cv4CommonEngineLib/CvEngineEnums.h>
#include <Cv4CommonEngineLib/CvInterface.h>

namespace cvengine
{
	class CvTuiMainInterface;
	class WorldView;

	class CvTuiInterface : public CvInterface
	{
	public:
		static CvTuiInterface& getInstance();

		CvTuiInterface();
		~CvTuiInterface() noexcept;

		virtual void uninit() override;
		virtual void reset() override;

		// NOTE: Because CvTuiMainInterface is only created when in-game, callers must consider that these may be null if not yet in-game.
		CvTuiMainInterface* getTuiMainInterface();
		WorldView* getWorldView();
		bool isAIAutorunActive() const;

		virtual bool isInMainMenu() const override;
		virtual bool isLeftMouseDown() const override;
		virtual bool isRightMouseDown() const override;

		virtual void startMainInterface() override;

		/// Camera control
		virtual void lookAtPlot(ivec2 coord) override;
		virtual void lookAtCityBuilding(int iCityId, BuildingTypes iBuilding) override;
		virtual void lookAtSelectionPlot() override;
		virtual void lookAtCityOffset(int iCity) override;
		virtual CvPlot* getLookAtPlot() const override;
		virtual CvPlot* getMouseOverPlot() const override;
		virtual CvPlot* getSelectedCityOrUnitPlot() const override;

		virtual const CvSelectionGroup* getSelectionList() const override;
		virtual CvSelectionGroup* getSelectionList() override;
		virtual void makeSelectionListDirty() override;
		virtual void selectionListPostChange() override;
		virtual void selectionListPreChange() override;

		virtual void clearQueuedPopups() override;
		virtual bool isDiploOrPopupWaiting() const override;
		// This is what finally gets called after CvDLLButtonPopup has filled in the controls.
		// Implementation should behave according to PopupStates, and set IPopupUIWindow when displayed.
		virtual void launchPopup(std::unique_ptr<CvPopup> pPopup, bool bCreateOkButton, PopupStates bState, int iNumPixelScroll) override;

		virtual CvUnit* getLastSelectedUnit() const override;
		virtual void setLastSelectedUnit(CvUnit* pUnit) override;
		virtual CvPlot* getGotoPlot() const override;
		virtual CvPlot* getSingleMoveGotoPlot() const override;
		virtual CvPlot* getOriginalUnitSelectionPlot() const override;
		virtual int getOriginalUnitSelectionIndex() const override;

		// Possibly called by Rhye's and Fall of Civilization.
		// Index should be a 2D sound as GAME_MUSIC.
		virtual void doSoundtrack(int index) override;
		virtual void play2DSound(int index) override;
		virtual void play3DSound(int index, const NiPoint3& vPos) override;
		// iSoundType is probably the sound category as listed in AudioDefines.xml.
		//virtual void playGeneralSound(int iSoundId, int iSoundType, NiPoint3 vPos) override;
		virtual void playUnitSelectionSound(const CvUnit& unit) override;
		virtual void setSoundSelectionReady(bool bReady) override;

		// `bTestProduction` is deferred until exit city screen.
		virtual void selectCity(CvCity* pNewValue, bool bTestProduction) override;
		virtual void selectLookAtCity(bool bAdd) override;
		virtual void addSelectedCity(CvCity* pNewValue, bool bToggle) override;
		virtual void clearSelectedCities() override;
		virtual bool isCitySelected(CvCity* pCity) const override;
		virtual CvCity* getHeadSelectedCity() const override;
		virtual bool isCitySelection() const override;
		virtual CLLNode<IDInfo>* nextSelectedCitiesNode(CLLNode<IDInfo>* pNode) override;
		virtual CLLNode<IDInfo>* headSelectedCitiesNode() override;
		virtual bool isCityScreenUp() const override;

		virtual void addTurnMessageDisplayEntry(CvTalkingHeadMessage msg) override;
		virtual void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
			InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor,
			int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows) override;
		//virtual void addCombatMessage(PlayerTypes ePlayer, CvWString szString) override;
		virtual void addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId) override;
		//virtual void showMessage(CvTalkingHeadMessage& msg) override;
		// This function is called when the game-active player is no longer turn-active.
		// Maybe this function is supposed to be used to flush queued messaged onto the screen.
		virtual void flushTalkingHeadMessages() override;
		//virtual void clearEventMessages() override;
		virtual void getDisplayedButtonPopups(CvPopupQueue& infos) const override;

		virtual int getCycleSelectionCounter() const override;
		virtual void setCycleSelectionCounter(int iNewValue) override;
		virtual void changeCycleSelectionCounter(int iChange) override;

		virtual bool isCombatFocus() const override;
		virtual void setCombatFocus(bool bNewValue) override;
		//virtual void setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override;

		virtual bool getInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem) const override;
		virtual void setInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem, bool bNewValue) override;
		virtual void updateCursorType() override;
		virtual void updatePythonScreens() override;

		virtual void lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection) override;
		virtual void centerCamera(CvUnit*) override;
		virtual void releaseLockedCamera() override;
		virtual bool isFocusedWidget() const override;
		virtual bool isFocused() const override;
		virtual bool isBareMapMode() const override;
		virtual void toggleBareMapMode() override;
		virtual bool isShowYields() const override;
		virtual void toggleYieldVisibleMode() override;
		virtual bool isScoresVisible() const override;
		virtual void toggleScoresVisible() override;
		virtual bool isScoresMinimized() const override;
		virtual void toggleScoresMinimized() override;
		//virtual bool isNetStatsVisible() const override;



		virtual void setInterfaceMode(InterfaceModeTypes eNewValue) override;
		virtual InterfaceModeTypes getInterfaceMode() const override;
		virtual InterfaceVisibility getShowInterface() const override;
		virtual void setShowInterface(InterfaceVisibility eInterfaceVisibility) override;

		//virtual void setFlashing(PlayerTypes eWho, bool bFlashing) override;
		//virtual bool isFlashing(PlayerTypes eWho) const override;
		//virtual void setDiplomacyLocked(bool bLocked) override;
		//virtual bool isDiplomacyLocked() const override;

		virtual void setMinimapColor(MinimapModeTypes eMinimapMode, ivec2 coord, ColorTypes eColor, float fAlpha) override;
		virtual unsigned char* getMinimapBaseTextureDXT1() const override;

		// These seem to be used when PLAYEROPTION_WAIT_END_TURN is off.
		virtual int getEndTurnCounter() const override;
		virtual void setEndTurnCounter(int iNewValue) override;
		virtual void changeEndTurnCounter(int iChange) override;

		virtual void setEndTurnMessage(bool bNewValue) override;
		virtual bool isEndTurnMessage() const override;

		// The message?
		virtual bool shouldDisplayEndTurn() const override;
		// Flag by the minimap
		virtual bool shouldDisplayFlag() const override;
		// City screen return message?
		virtual bool shouldDisplayReturn() const override;
		// In the bottom left?
		virtual bool shouldDisplayUnitModel() const override;
		virtual bool shouldShowResearchButtons() const override;

		virtual bool isForcePopup() const override;
		virtual void setForcePopup(bool bNewValue) override;

		/// Turn log
		virtual void toggleTurnLog() override;
		virtual void dirtyTurnLog(PlayerTypes ePlayer) override;

		//virtual void unlockPopupHelp() override;

		virtual void showDetails(bool bPasswordOnly) override;
		virtual void showAdminDetails() override;

		virtual void toggleClockAlarm(bool bValue, int iHour, int iMin) override;
		virtual bool isClockAlarmOn() const override;

		virtual bool isExitingToMainMenu() const override;
		virtual void exitToMainMenu() override;
		virtual void setWorldBuilder(bool bTurnOn) override;

		// Called by scripts.
		virtual int determineWidth(const std::wstring& szBuffer) const override;

		virtual void setCityTabSelectionRow(CityTabTypes eTabType) override;
		virtual CityTabTypes getCityTabSelectionRow() const override;

		virtual bool noTechSplash() const override;

		virtual bool isInAdvancedStart() const override;
		virtual void setInAdvancedStart(bool bAdvancedStart) override;

		virtual bool isSpaceshipScreenUp() const override;
		//virtual bool isDebugMenuCreated() const override;

		virtual void setBusy(bool bBusy) override;

		

	private:
		std::unique_ptr<CvTuiMainInterface> mMainInterface;

		bool mIsExitingToMainMenu = false;

		//void processEvents();
		//void updateUI();
		//
		//void showPopup(std::unique_ptr<CvPopup> popup);
		//void updatePopups();
		//bool hasActiveScreens() const;
		//
		//void setGotoPlot(CvPlot*);
		//
		//void updateWorldViewDisplayedPath();
		//
		//void setCitySelectionInterfaceDirtyBits();
	};
}