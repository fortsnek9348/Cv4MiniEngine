#pragma once

#include "WorldView.h"
#include "Minimap.h"

#include <CvSelectionGroupAI.h>
#include <LinkedList.h>

#include <CommonStuff/NoCopy.h>

#include <bitset>
#include <map>
#include <memory>

namespace hecktui
{
	class Element;
	class Window;
}

class CvPopup;
class CvTalkingHeadMessage;

namespace cvengine
{
	class CvGInterfaceScreen;
	class CvGTurnLogWindow;

	// Getting away from singletons, this call is instantiated only when the main interface is active.
	// In constrast to CvInterface which is always in memory.
	// Still, everything else is global. Like the screens themselves.
	class CvTuiMainInterface : public heck::NoCopyNoMove
	{
	public:
		CvTuiMainInterface();
		~CvTuiMainInterface() noexcept;

		enum class EGameStateChangeReason
		{
			UnitSelection,
			CitySelection,
			Action,
			PushMessage,
			CityScreen,
			DiploEvent,
			// Used to update the action list UI.
			GotoPlotChanged,

			Deal,

			Num,
		};

		void onGameStateChanged(EGameStateChangeReason);

		void startMainInterface();
		void updateMainInterface();
		
		void activateAIAutoPlay(int numTurns);
		bool isAIAutorunActive() const;
		
		void setInterfaceMode(InterfaceModeTypes);
		void cancelInterfaceMode();
		InterfaceModeTypes getInterfaceMode() const;
		
		// Called from MyCvDLLInterfaceIFace.
		const CvSelectionGroup& getInterfaceSelectionList() const;
		CvSelectionGroup& getInterfaceSelectionList();
		void lookAtSelectionPlot();
		CvUnit* findSelectedUnitByIndex(int index) const;
		// Called after changes to the selection list from the DLL.
		void makeSelectionListDirty();
		void resetInterfaceMode();
		// Called from MyCvDLLInterfaceIFace::insertIntoSelectionList.
		// Only one sound is played until the next UI update.
		// Not sure the exe ensure only one sound is played.
		void playUnitSelectionSound(const CvUnit& unit);
		
		int getCycleSelectionCounter() const;
		void setCycleSelectionCounter(int);
		void changeCycleSelectionCounter(int delta);
		
		void clearSelectedCities();
		void addSelectedCity(CvCity* city, bool toggle);
		bool isCitySelected(const CvCity* city) const;
		const CLinkList<IDInfo>& getSelectedCityList() const;
		bool isInCityScreen() const;
		void enterCityScreen(CvCity* city, bool testProduction);
		void exitCityScreen();
		
		CvPlot* getSelectedCityOrUnitPlot() const;
		CvPlot* getLookAtPlot() const;
		CvPlot* getOriginalUnitSelectionPlot() const;
		int getOriginalUnitSelectionIndex() const;
		void lookAt(heck::ivec2);
		
		// Message functions.
		// Called by MyCvDLLInterfaceIFace and CyInterface.
		void addMessage(PlayerTypes ePlayer, CvTalkingHeadMessage msg);
		void addMessage(PlayerTypes ePlayer, bool bForce, int iLength, std::wstring szString, const TCHAR* pszSound,
			InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY,
			bool bShowOffScreenArrows, bool bShowOnScreenArrows);
		void addCombatMessage(PlayerTypes ePlayer, const std::wstring& szString);
		void addImmediateMessage(const std::wstring& szString, const std::string& szSound);
		void addQuestMessage(PlayerTypes ePlayer, const std::wstring& szString, int iQuestId);
		
		// On-screen "talking head" messages
		void addTurnMessageDisplayEntry(CvTalkingHeadMessage);
		void clearTurnMessageDisplay() noexcept;
		uint64_t getFirstTurnMessageSerial() const noexcept;
		std::span<const CvTalkingHeadMessage> getTurnMessages() const noexcept;
		
		// The "event log" window.
		void toggleTurnLog();
		void dirtyTurnLog(PlayerTypes ePlayer);
		
		void showDebugScreen();
		
		bool isScoresMinimized() const;
		void toggleScoresMinimized();
		
		void setEndTurnMessage(bool);
		bool isEndTurnMessage() const;
		
		bool getInterfaceDirtyBit(InterfaceDirtyBits) const;
		void setInterfaceDirtyBit(InterfaceDirtyBits, bool value);
		
		// Called by MyCvDLLEngineIFace.
		//void invalidateAllPlots();
		//void invalidatePlot(heck::ivec2 coord);
		
		// Called by CyPopup.
		bool launchPopup(std::unique_ptr<CvPopup> popup, bool bCreateOK, PopupStates eState);
		
		// Maybe this is used to focus minimised popups before EndTurnMessage?
		void setForcePopup(bool);
		bool isForcePopup() const;
		
		// Called by mainInterface.
		void doUnitListUIUnitSelection(CvUnit* unit, bool isToggleSelect, bool isGroupSelect);
		void onWorldViewClickPlot(CvPlot* plot, bool shift);
		void onWorldViewButtonUpOnPlot(CvPlot* plot, hecktui::ModifierKeyState modifierKeys);
		void onWorldViewMiddleButtonOnPlot(CvPlot* plot);
		
		void onWorldViewDoubleClickPlot(CvPlot* plot);
		void onWorldViewMouseOverPlot(CvPlot* plot, bool mouseCaptured);
		void onWorldViewMouseLeave();
		void onBeforeWorldViewRender();
		
		// The given uiElement will be checked with UIScene to determine hover state.
		void onCityBillboardMouseOver(std::weak_ptr<const hecktui::Element> uiElement, IDInfo cityIdInfo);
		CvCity* getCurrentCityBillboardHoverCity() const;
		

		
		bool isFocusedWidget() const;
		bool isFocused() const;
		bool hasWaitingPopupsOrDiplo() const;
		
		
		// Possibly called every 250ms in Civ4.
		// Modified to be event-based here.
		void updateSelectionList();
		
		CvCity* getHeadSelectedCity() const noexcept;
		
		CvPlot* getGotoPlot() const noexcept;
		
		const WorldView& getWorldView() const noexcept;
		WorldView& getWorldView() noexcept;

		CvGInterfaceScreen& grabPythonScreen(std::string name, cvengine::ECvScreen kind);

		void showScreen(cvengine::ECvScreen kind, PopupStates popupState, bool passInput);
		bool isActive(cvengine::ECvScreen kind) const;

		hecktui::iaabb2 updateAndGetMinimapSectionRect();
		
	private:
		void processEvents();
		void updateUI();
		void showPopup(std::unique_ptr<CvPopup> popup);
		void updatePopups();
		bool hasActiveScreens() const;
		void setGotoPlot(CvPlot*);
		void updateWorldViewDisplayedPath();
		void setCitySelectionInterfaceDirtyBits();

		CvSelectionGroupAI mInterfaceSelectionList;

		bool mWantUIUpdate = true;
		bool mIsAIAutoplayActive = false;

		// Not sure if this is correct.
		InterfaceModeTypes mInterfaceMode = InterfaceModeTypes::INTERFACEMODE_SELECTION;

		// This appears to be used to delay unit selection cycling.
		// As Cv4MiniEngine is 100% action-based, this functions differently.
		int mCycleSelectionCounter = 0;

		// This must be a CLinkList as CvDLLInterfaceIFace returns nodes from this.
		CLinkList<IDInfo> mSelectedCityList{};
		bool mIsInCityScreen = false;
		bool mTestProductionOnCityScreenExit = false;
		bool mIsEndTurnMessage = false;
		bool mIsForcePopup = false; // Probably in conjunction with minimise popups.
		CvPlot* mOriginalUnitPlot = nullptr;
		// -1: No selection in plot.
		int mOriginalPlotCurrentUnitIndex = -1;
		CvPlot* mGotoPlot = nullptr;

		// Left button down on unit.
		bool mIsUnitDragActive = false;
		// Set to true from mouse down on a plot with INTERFACEMODE_SELECTION until mouse up.
		bool mWasPlotClickForUnitSelection = false;

		WorldView mWorldView;

		std::vector<WorldView::PathNode> mWorldViewDisplayedPath;

		struct CityBillboardHoverState
		{
			std::weak_ptr<const hecktui::Element> uiElement;
			IDInfo cityIdInfo{};
		} mCityBillboardHoverState{};

		bool mIsCombatFocus = false;
		//bool mTestProduction = false;
		bool mIsScoreboardMinimised = false;

		// Set by actions from the UI to trigger CvGame::update.
		std::bitset<(int)EGameStateChangeReason::Num> mGameStateChangeReasons{};

		std::bitset<35> mInterfaceDirtyBits{ 0 };

		struct ScreenWindowEntry
		{
			std::optional<PopupStates> lastPopupState{};
			std::unique_ptr<CvGInterfaceScreen> screen;
			// If non-null, this screen is in the TUI.
			std::shared_ptr<hecktui::Window> currentTuiWindow;
		};

		std::map<ECvScreen, ScreenWindowEntry> mScreenWindowEntries;
		uint64_t mLastScreenUpdatePythonInstanceSerial = 0;

		// Compared to Civ4, this CvInterface will only display one popup at a time. Even for immediate popups. They all get queued here.
		std::list<std::unique_ptr<CvPopup>> mPopupQueue;
		std::unique_ptr<CvPopup> mCurrentPopup;

		// NOTE: Using pointers directly. This should only be a temporary array in the main interface's plot list loop.
		//std::vector<CvUnit*> mCachedPlotUnitList;

		uint64_t mFirstTurnMessageSerial = 0;
		std::vector<CvTalkingHeadMessage> mTurnMessages;

		std::shared_ptr<CvGTurnLogWindow> mTurnLogWindow;
		bool mTurnLogIsDirty = true;

		//std::shared_ptr<CvGDebugMenu> mDebugWindow;

		MinimapSectionTracking mMinimapSectionTracking;

		bool mHasUnitSelectionSoundPlayed = false;
	};
}