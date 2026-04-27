#include "CvTuiMainInterface.h"
#include "CvApp.h"
#include "CvGInterfaceScreen.h"
#include "CvGTurnLogWindow.h"
#include "CvPopupTuiDialog.h"
#include "CvTuiDiplomacyScreen.h"
#include "CvTuiEngine.h"
#include "PythonScreen.h"

#include <Cv4CommonEngineLib/CvButtonPopup.h>
#include <Cv4CommonEngineLib/CvEngine.h>
#include <Cv4CommonEngineLib/CvPopup.h>
#include <Cv4CommonEngineLib/DLLMessageQueue.h>
#include <Cv4CommonEngineLib/MyCvDLLPython.h>

#include <CvGameCoreDLL/CLinkListIterator.h>
#include <CvGameCoreDLL/CvDiploParameters.h>
#include <CvGameCoreDLL/CvDLLInterfaceIFaceBase.h>
#include <CvGameCoreDLL/CvDLLUtilityIFaceBase.h>
#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGameCoreUtils.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvMessageData.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/FAStarNode.h>
#include <CvGameCoreDLL/CvDLLFAStarIFaceBase.h>

#include <CommonStuff/range.h>

#include <chrono>
#include <iostream>
#include <ranges>

using namespace cvengine;
using heck::range;

static constexpr size_t kMaxMessages = 100;

CvTuiMainInterface::CvTuiMainInterface()
{
	// This appears to be the state during CvGame::initSelection. This will cause the camera to be centered on the initial selected unit /after/ the camera is centered on the capital when loading a game.
	mInterfaceDirtyBits = {};
	mInterfaceDirtyBits.flip();

	mInterfaceSelectionList.reset();
	mInterfaceSelectionList.init(FFreeList::INVALID_INDEX, NO_PLAYER);
}

CvTuiMainInterface::~CvTuiMainInterface() noexcept = default;


// This can be handled on its own because it does not require holding onto args during the python reset.
static void callFunctionWithRetryPrompt(const char* moduleName, const char* fxnName)
{
	for (;;)
	{
		try
		{
			(void)MyCvDLLPython().callFunction(moduleName, fxnName, nullptr);
			break;
		}
		catch (const std::runtime_error& ex)
		{
			std::clog << ex.what() << std::endl;
			CvApp::getInstance().getUI().modalLoopPythonReloadPrompt(L"Function call failed.", L"Reload All Python");
			MyCvDLLPython().restart();
		}
	}
}

void CvTuiMainInterface::startMainInterface()
{
	mLastScreenUpdatePythonInstanceSerial = MyCvDLLPython().getCurrentPythonEnvironmentSerial();

	callFunctionWithRetryPrompt(PYScreensModule, "init");
	callFunctionWithRetryPrompt(PYScreensModule, "createDomesticAdvisor");
	callFunctionWithRetryPrompt(PYScreensModule, "createFinanceAdvisor");
	callFunctionWithRetryPrompt(PYScreensModule, "createMilitaryAdvisor");
	callFunctionWithRetryPrompt(PYScreensModule, "createCivilopedia");
	callFunctionWithRetryPrompt(PYScreensModule, "createTechSplash");
	callFunctionWithRetryPrompt(PYScreensModule, "showMainInterface");


	// There is also a call to toggleSetScreenOn for screens, but it seems to have no use.

	mWantUIUpdate = true;

	// Center the camera.
	// Doing it this way also defers the update, so you will see the capital city on the first frame after loading a game.
	// But this is what Civ4 does anyway, you can see the camera move away from the capital city.
	onGameStateChanged(EGameStateChangeReason::Action);

	// For testing.
	//{
	//	CyArgsList args;
	//	args.add(true);
	//	gDLL->getPythonIFace()->callFunction(PYScreensModule, "showHallOfFame", args.makeFunctionArgs());
	//}

	//bool wantUIUpdate = true;
}

void CvTuiMainInterface::updateMainInterface()
{
	// Update UI both before and after, so that we check the player's popup queue before checking SelectionCamera_DIRTY_BIT.
	updateUI();

	processEvents();
	updateUI();
}

void CvTuiMainInterface::activateAIAutoPlay(int numTurns)
{
	std::clog << __FUNCTION__ << std::endl;
	//MyCvDLLUtility::getInstance().isAutorun = true;
	mIsAIAutoplayActive = true;

	gGlobals.getGame().setAIAutoPlay(numTurns);
	onGameStateChanged(EGameStateChangeReason::Action);
}

bool CvTuiMainInterface::isAIAutorunActive() const
{
	return mIsAIAutoplayActive;
}

void CvTuiMainInterface::setInterfaceMode(InterfaceModeTypes mode)
{
	if (mode == mInterfaceMode)
		return;

	if (mode == InterfaceModeTypes::INTERFACEMODE_SELECTION || gGlobals.getDLLIFaceNonInl()->getInterfaceIFace()->canDoInterfaceMode(mode, &mInterfaceSelectionList))
	{
		mInterfaceMode = mode;
		mInterfaceDirtyBits[Waypoints_DIRTY_BIT] = true;
		updateWorldViewDisplayedPath();
	}
	else
		cancelInterfaceMode();
}

void CvTuiMainInterface::cancelInterfaceMode()
{
	setInterfaceMode(InterfaceModeTypes::INTERFACEMODE_SELECTION);
}

InterfaceModeTypes CvTuiMainInterface::getInterfaceMode() const
{
	return mInterfaceMode;
}

void CvTuiMainInterface::makeSelectionListDirty()
{
	setGotoPlot(nullptr);

	// + Waypoints_DIRTY_BIT SelectionButtons_DIRTY_BIT Help_DIRTY_BIT
	// + Center_DIRTY_BIT Cursor_DIRTY_BIT HighlightPlot_DIRTY_BIT
	// + MiscButtons_DIRTY_BIT PlotListButtons_DIRTY_BIT UnitInfo_DIRTY_BIT InfoPane_DIRTY_BIT ColoredPlots_DIRTY_BIT
	mInterfaceDirtyBits[SelectionButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[Help_DIRTY_BIT] = true;
	// This one is needed to update the center unit.
	mInterfaceDirtyBits[Center_DIRTY_BIT] = true;
	mInterfaceDirtyBits[Cursor_DIRTY_BIT] = true;
	mInterfaceDirtyBits[MiscButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[PlotListButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[UnitInfo_DIRTY_BIT] = true;
	mInterfaceDirtyBits[InfoPane_DIRTY_BIT] = true;
	mInterfaceDirtyBits[ColoredPlots_DIRTY_BIT] = true;

	// Need this too.
	mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT] = true;

	gGlobals.getDLLIFaceNonInl()->getFAStarIFace()->ForceReset(&gGlobals.getInterfacePathFinder());

	// This is state used for unit cycling.
	mOriginalPlotCurrentUnitIndex = -1;
	if (CvUnit* const unit = findSelectedUnitByIndex(0))
	{
		mOriginalUnitPlot = unit->plot();
		int index = 0;
		for (auto it = mOriginalUnitPlot->headUnitNode(); it; it = mOriginalUnitPlot->nextUnitNode(it), ++index)
		{
			if (getUnit(it->m_data) == unit)
			{
				mOriginalPlotCurrentUnitIndex = index;
				break;
			}
		}
	}
}

int CvTuiMainInterface::getCycleSelectionCounter() const
{
	return mCycleSelectionCounter;
}
void CvTuiMainInterface::setCycleSelectionCounter(int value)
{
	mCycleSelectionCounter = value;
}
void CvTuiMainInterface::changeCycleSelectionCounter(int delta)
{
	mCycleSelectionCounter += delta;
}

void CvTuiMainInterface::clearSelectedCities()
{
	if (mSelectedCityList.getLength())
	{
		// city->setInfoDirty(true) is called somewhere in here.

		CvCity* const selectedCity = getHeadSelectedCity();

		mSelectedCityList.clear();

		if (mIsInCityScreen)
		{
			mIsInCityScreen = false;

			if (selectedCity)
			{
				resetInterfaceMode();
				selectedCity->updateSelectedCity(false);
			}

			// + Fog_DIRTY_BIT Waypoints_DIRTY_BIT PercentButtons_DIRTY_BIT MiscButtons_DIRTY_BIT PlotListButtons_DIRTY_BIT SelectionButtons_DIRTY_BIT
			//   CitizenButtons_DIRTY_BIT ResearchButtons_DIRTY_BIT Center_DIRTY_BIT GameData_DIRTY_BIT Score_DIRTY_BIT Help_DIRTY_BIT
			//   SelectionSound_DIRTY_BIT UnitInfo_DIRTY_BIT CityScreen_DIRTY_BIT InfoPane_DIRTY_BIT ColoredPlots_DIRTY_BIT
			mInterfaceDirtyBits.set(Fog_DIRTY_BIT);
			mInterfaceDirtyBits.set(Waypoints_DIRTY_BIT);
			mInterfaceDirtyBits.set(PercentButtons_DIRTY_BIT);
			mInterfaceDirtyBits.set(ResearchButtons_DIRTY_BIT);
			mInterfaceDirtyBits.set(GameData_DIRTY_BIT);
			mInterfaceDirtyBits.set(Score_DIRTY_BIT);
			mInterfaceDirtyBits.set(Help_DIRTY_BIT);
			mInterfaceDirtyBits.set(UnitInfo_DIRTY_BIT);
			mInterfaceDirtyBits.set(CityScreen_DIRTY_BIT);
			// Then setCitySelectionInterfaceDirtyBits

			lookAtSelectionPlot();

			mWorldView.updateFog();
		}

		setCitySelectionInterfaceDirtyBits();

		onGameStateChanged(EGameStateChangeReason::CityScreen);
		onGameStateChanged(EGameStateChangeReason::CitySelection);
	}
}

CvUnit* CvTuiMainInterface::findSelectedUnitByIndex(int index) const
{
	CLLNode<IDInfo>* node = mInterfaceSelectionList.headUnitNode();

	for (int i = 0; i < index && node; ++i)
		node = mInterfaceSelectionList.nextUnitNode(node);

	return node ? getUnit(node->m_data) : nullptr;
}


void CvTuiMainInterface::addSelectedCity(CvCity* city, bool toggle)
{
	if (mIsInCityScreen)
		clearSelectedCities();

	if (!city)
		return;

	const auto listView = viewCLinkList(mSelectedCityList);
	const auto it = std::ranges::find(listView, city->getIDInfo());
	if (it != listView.end())
	{
		if (toggle)
			mSelectedCityList.deleteNode(it.node);
	}
	else
	{
		// At beginning. This becomes the city that `getSelectedCityOrUnitPlot` returns.
		mSelectedCityList.insertAtBeginning(city->getIDInfo());
	}

	setCitySelectionInterfaceDirtyBits();

	onGameStateChanged(EGameStateChangeReason::CitySelection);
}

bool CvTuiMainInterface::isCitySelected(const CvCity* city) const
{
	return city && std::ranges::contains(viewCLinkList(mSelectedCityList), city->getIDInfo());
}

const CLinkList<IDInfo>& CvTuiMainInterface::getSelectedCityList() const
{
	return mSelectedCityList;
}

bool CvTuiMainInterface::isInCityScreen() const
{
	return mIsInCityScreen;
}

CvPlot* CvTuiMainInterface::getSelectedCityOrUnitPlot() const
{
	for (const IDInfo& cityID : viewCLinkList(mSelectedCityList))
		if (CvCity* const city = getCity(cityID))
			return city->plot();

	if (const CvUnit* const selectionFirstUnit = findSelectedUnitByIndex(0))
		return selectionFirstUnit->plot();
	else
		return nullptr;
}

CvPlot* CvTuiMainInterface::getLookAtPlot() const
{
	return mWorldView.getLookAtPlot();
}

CvPlot* CvTuiMainInterface::getOriginalUnitSelectionPlot() const
{
	return mOriginalUnitPlot;
}

int CvTuiMainInterface::getOriginalUnitSelectionIndex() const
{
	return mOriginalPlotCurrentUnitIndex;
}

void CvTuiMainInterface::lookAt(heck::ivec2 p)
{
	mWorldView.setCenterPlot(p);
}

void CvTuiMainInterface::addMessage(PlayerTypes ePlayer, CvTalkingHeadMessage msg)
{
	if (ePlayer == NO_PLAYER)
		return;

	CvPlayer& player = GET_PLAYER(ePlayer);

	// Should we check this?
	if (!player.isAlive())
		return;

	msg.setShown(true);

	if (ePlayer == gGlobals.getGame().getActivePlayer())
	{
		// TODO: Check this...
		if (msg.getMessageType() != MESSAGE_TYPE_COMBAT_MESSAGE && msg.getMessageType() != MESSAGE_TYPE_QUEST)
			addTurnMessageDisplayEntry(msg);
		mTurnLogIsDirty = true; // The "event log", not the "message display"!
	}

	player.addMessage(std::move(msg));
}

void CvTuiMainInterface::addMessage(PlayerTypes ePlayer, [[maybe_unused]] bool bForce, int iLength, std::wstring szString, const TCHAR* pszSound,
	InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY,
	bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	// What does this mean? An immediate message?
	//if (bForce)
	//	_CrtDbgBreak();

	addMessage(ePlayer, CvTalkingHeadMessage{
		gGlobals.getGame().getGameTurn(),
		iLength,
		szString.c_str(),
		pszSound,
		eType,
		pszIcon,
		eFlashColor,
		iFlashX,
		iFlashY,
		bShowOffScreenArrows,
		bShowOnScreenArrows
		});
}
void CvTuiMainInterface::addCombatMessage(PlayerTypes ePlayer, const std::wstring& szString)
{
	// Example message: CreateDestroy CvTalkingHeadMessage::CvTalkingHeadMessage(0, 10, While defending, your Warrior has killed a Barbarian Modern Armor!, AS2D_VICTORY_EARLY, 0, , 8, 34, 25, false, false)
	// Example message: CreateDestroy CvTalkingHeadMessage(1, 10, Barbarian's Wolf (1.00) vs Axolotl's Warrior (3.40), , 5, , -1, -1, -1, false, false)
	addMessage(ePlayer, CvTalkingHeadMessage{
		GC.getGame().getGameTurn(), 10, szString.c_str(), nullptr,
		InterfaceMessageTypes::MESSAGE_TYPE_COMBAT_MESSAGE,
		nullptr, NO_COLOR, -1, -1, false, false
		});
}
void CvTuiMainInterface::addImmediateMessage(const std::wstring& szString, const std::string& szSound)
{
	//std::wclog << L"CyInterface::addImmediateMessage: " << szString << std::endl;
	addMessage(gGlobals.getGame().getActivePlayer(), false, 0, szString, szSound.c_str(),
		InterfaceMessageTypes::MESSAGE_TYPE_DISPLAY_ONLY, {},
		NO_COLOR, -1, -1, false, false
	);
}
void CvTuiMainInterface::addQuestMessage(PlayerTypes ePlayer, const std::wstring& szString, int iQuestId)
{
	// Hills, hills everywhere - AI Autoplay turn 107.CivBeyondSwordSave

	// TODO: CvGlobals::getEVENT_MESSAGE_TIME and Getter CvGame::getGameTurn are called, but what are they used for?
	//       Set and then overwritten?

	CvTalkingHeadMessage msg{
		gGlobals.getGame().getGameTurn(),
		gGlobals.getEVENT_MESSAGE_TIME(),
		szString.c_str(),
		nullptr,
		MESSAGE_TYPE_QUEST,
		nullptr,
		NO_COLOR,
		-1,
		-1,
		false,
		false
	};

	// Yes, Civ4 then sets the quest id as the length.
	// TODO: Really?
	msg.setLength(iQuestId);


	addMessage(ePlayer, std::move(msg));
}

void CvTuiMainInterface::addTurnMessageDisplayEntry(CvTalkingHeadMessage msg)
{
	//std::wclog << L"Player " << (int)entry.ePlayer << L" event: " << entry.szString << std::endl;
	// TODO: Is this what really happens? Do these messages go anywhere else?
	// TODO: Forest growths tend to have no player as the owner is taken from the plot. So if you let NO_PLAYER through, you see everybody's forest growths.
	//if (/*ePlayer == NO_PLAYER || */entry.ePlayer == gGlobals.getGame().getActivePlayer())
	{
		//switch (entry.eType)
		//{
		//case InterfaceMessageTypes::MESSAGE_TYPE_INFO:
		//case InterfaceMessageTypes::MESSAGE_TYPE_MAJOR_EVENT:
		//case InterfaceMessageTypes::MESSAGE_TYPE_MINOR_EVENT:
		//	CvPlayerAI::getPlayerNonInl(entry.ePlayer).addMessage(
		//}
		if (mTurnMessages.size() >= kMaxMessages)
		{
			mTurnMessages.erase(mTurnMessages.begin());
			++mFirstTurnMessageSerial;
		}
		mTurnMessages.push_back(std::move(msg));
	}
}
void CvTuiMainInterface::clearTurnMessageDisplay() noexcept
{
	mFirstTurnMessageSerial += mTurnMessages.size();
	mTurnMessages.clear();
}
uint64_t CvTuiMainInterface::getFirstTurnMessageSerial() const noexcept
{
	return mFirstTurnMessageSerial;
}
std::span<const CvTalkingHeadMessage> CvTuiMainInterface::getTurnMessages() const noexcept
{
	return mTurnMessages;
}

void CvTuiMainInterface::toggleTurnLog()
{
	if (!mTurnLogWindow)
	{
		mTurnLogWindow = std::make_shared<CvGTurnLogWindow>();
		CvApp::getInstance().getUI().pushWindow(mTurnLogWindow);
	}
	else
	{
		CvApp::getInstance().getUI().removeWindow(mTurnLogWindow.get());
		mTurnLogWindow = nullptr;
	}
}
void CvTuiMainInterface::dirtyTurnLog(PlayerTypes ePlayer)
{
	if (mTurnLogWindow)
		mTurnLogWindow->dirtyTurnLog(ePlayer);
}

void CvTuiMainInterface::showDebugScreen()
{
	//if (!mDebugWindow)
	//{
	//	mDebugWindow = std::make_shared<CvGDebugMenu>();
	//	CvApp::getInstance().getUI().pushWindow(mDebugWindow);
	//}
}

bool CvTuiMainInterface::isScoresMinimized() const
{
	return mIsScoreboardMinimised;
}
void CvTuiMainInterface::toggleScoresMinimized()
{
	mIsScoreboardMinimised = !mIsScoreboardMinimised;
}

void CvTuiMainInterface::setEndTurnMessage(bool value)
{
	mIsEndTurnMessage = value;
}
bool CvTuiMainInterface::isEndTurnMessage() const
{
	return mIsEndTurnMessage;
}

bool CvTuiMainInterface::getInterfaceDirtyBit(InterfaceDirtyBits i) const
{
	return mInterfaceDirtyBits[i];
}
void CvTuiMainInterface::setInterfaceDirtyBit(InterfaceDirtyBits i, bool value)
{
	mInterfaceDirtyBits[i] = value;
}

//void CvTuiMainInterface::invalidateAllPlots()
//{
//	mWorldView.invalidateAll();
//}
//void CvTuiMainInterface::invalidatePlot(heck::ivec2 coord)
//{
//	mWorldView.invalidatePlot(coord);
//}

bool CvTuiMainInterface::launchPopup(std::unique_ptr<CvPopup> popup, bool bCreateOK, PopupStates eState)
{
	//if (!popup->bodyString.empty())
	//	popup->controls.insert(popup->controls.begin(), CvPopup::Control{ .type = CvPopup::EControlType::Label, .text = popup->bodyString });
	if (bCreateOK)
	{
		popup->controls.push_back({ .type = CvPopup::EControlType::Separator, .sepSize = 1 });
		popup->controls.push_back({
			.type = CvPopup::EControlType::Button,
			.text = gGlobals.getDLLIFaceNonInl()->getTextGeneric(L"TXT_KEY_MAIN_MENU_OK", {}),
			.iGroup = -1
			});
		popup->optEnterSubmitBtnId = -1;
	}

	popup->state = eState;

	//std::abort();
	//mPopupQueue.emplace_back(std::move(popup));
	showPopup(std::move(popup));

	// Seems like that's what this returns.
	return false;
}

void CvTuiMainInterface::setForcePopup(bool value)
{
	mIsForcePopup = value;
}
bool CvTuiMainInterface::isForcePopup() const
{
	return mIsForcePopup;
}

void CvTuiMainInterface::doUnitListUIUnitSelection(CvUnit* unit, bool isToggleSelect, bool isGroupSelect)
{
	//std::clog << "doUnitListUIUnitSelection " << (CvString)unit->getName() << std::endl;

	//if (!isGroupSelect)
	//	unit->group

	CvDLLInterfaceIFaceBase& dllInterfaceController = *gGlobals.getDLLIFaceNonInl()->getInterfaceIFace();

	if (!isToggleSelect)
	{
		if (isGroupSelect)
			gGlobals.getGame().selectGroup(unit, isToggleSelect, isGroupSelect, false);
		else
		{
			//gGlobals.getGame().selectUnit(unit, true, false);

			dllInterfaceController.clearSelectedCities();
			dllInterfaceController.clearSelectionList();
			dllInterfaceController.insertIntoSelectionList(unit, false, isToggleSelect, false, true, false);
			dllInterfaceController.makeSelectionListDirty();
		}
	}
	else
	{
		if (isGroupSelect)
			gGlobals.getGame().selectGroup(unit, isToggleSelect, isGroupSelect, false);
		//gGlobals.getGame().selectUnit(unit, false, true);
		else
		{
			//gGlobals.getGame().selectUnit(unit, false, true);
			dllInterfaceController.clearSelectedCities();
			dllInterfaceController.insertIntoSelectionList(unit, false, isToggleSelect, false, true, false);
			dllInterfaceController.makeSelectionListDirty();
		}
	}
}

void CvTuiMainInterface::onWorldViewClickPlot(CvPlot* plot, bool shift)
{
	// Ignore out of bounds.
	if (!plot)
		return;

	// So you use right-click to cancel.
	//if constexpr (hecktui::kShiftClickHackEnabled)
	//{
	//	if (shift)
	//	{
	//		// This is what's called by WorldViewUIComponent.
	//		cancelInterfaceMode();
	//		return;
	//	}
	//}

	mIsUnitDragActive = false;

	CvGame& game = gGlobals.getGame();

	if (mIsInCityScreen)
	{
		gGlobals.getGame().handleCityScreenPlotPicked(getHeadSelectedCity(), plot, false, shift, false);
		onGameStateChanged(EGameStateChangeReason::Action);
		return;
	}

	clearSelectedCities();

	mWasPlotClickForUnitSelection = false;

	switch (mInterfaceMode)
	{
	case INTERFACEMODE_SELECTION:
	{
		//CvUnit* const centerUnit = plot->getCenterUnit();
		if (mInterfaceSelectionList.plot() == plot)
		{
			mIsUnitDragActive = true;
			// Unit selection happens on mouse up (ignoring click hold).
			//if (mInterfaceSelectionList.getNumUnits() <= 0 || mOriginalUnitPlot != plot)
			//	game.selectGroup(centerUnit, false, false, false);
			//onGameStateChanged(EGameStateChangeReason::UnitSelection);
		}
		mWasPlotClickForUnitSelection = true;
		break;
	}
	case INTERFACEMODE_SIGN:
	{
		resetInterfaceMode();

		struct CvNetSign : CvMessageData
		{
			explicit CvNetSign(PlayerTypes playerI, heck::ivec2 coord, std::wstring label)
				: CvMessageData(GameMessageTypes::GAMEMESSAGE_SIGN)
				, mPlayerI(playerI)
				, mCoord(coord)
				, mLabel(std::move(label))
			{
			}

			virtual void Debug(char*) override {}
			virtual void Execute() override
			{
				CvTuiEngine::getInstance().setSignText(mPlayerI, mCoord, mLabel);
			}
			virtual void PutInBuffer(FDataStreamBase* pStream) override
			{
				pStream->Write(static_cast<int32_t>(mPlayerI));
				pStream->Write(mCoord.x);
				pStream->Write(mCoord.y);
				pStream->WriteString(mLabel);
			}
			virtual void SetFromBuffer(FDataStreamBase* pStream) override
			{
				std::underlying_type_t<PlayerTypes> playerI{};
				pStream->Read(&playerI);
				mPlayerI = static_cast<PlayerTypes>(playerI);
				pStream->Read(&mCoord.x);
				pStream->Read(&mCoord.y);
				pStream->ReadString(mLabel);
			}

		private:
			PlayerTypes mPlayerI{};
			heck::ivec2 mCoord{};
			std::wstring mLabel{};
		};

		const PlayerTypes playerI = GC.getGame().getActivePlayer();
		const heck::ivec2 coord{ plot->getX(), plot->getY() };

		if (CvTuiEngine::getInstance().findSignTextAt(playerI, coord).size())
		{
			// Delete.
			DLLMessageQueue::getInstance().push(std::make_unique<CvNetSign>(playerI, coord, std::wstring()));
		}
		else
		{
			auto popup = std::make_unique<InternalPopup>();
			popup->bodyString = gGlobals.getDLLIFaceNonInl()->getTextGeneric(L"TXT_KEY_SIGN_ENTER_CAPTION", {});
			popup->controls = { {
				.type = CvPopup::EControlType::EditBox,
				.iGroup = 0,
			} ,{
				.type = CvPopup::EControlType::Button,
				.text = gGlobals.getDLLIFaceNonInl()->getText(L"TXT_KEY_MAIN_MENU_OK"),
				.iGroup = -1,
			} };
			popup->optEnterSubmitBtnId = -1;
			popup->enableEscCancel = true;

			if (const std::optional<PopupReturn> result = InternalPopup::launchModal(std::move(popup)))
			{
				const std::wstring_view text = result->getEditBoxString();
				DLLMessageQueue::getInstance().push(std::make_unique<CvNetSign>(GC.getGame().getActivePlayer(), heck::ivec2{ plot->getX(), plot->getY() }, std::wstring(text)));
			}
		}

		onGameStateChanged(EGameStateChangeReason::PushMessage);

		break;
	}
	case INTERFACEMODE_GO_TO:
	case INTERFACEMODE_GO_TO_TYPE:
	case INTERFACEMODE_GO_TO_ALL:
	case INTERFACEMODE_ROUTE_TO:
	case INTERFACEMODE_AIRLIFT:
	case INTERFACEMODE_NUKE:
	case INTERFACEMODE_RECON:
	case INTERFACEMODE_PARADROP:
	case INTERFACEMODE_AIRBOMB:
	case INTERFACEMODE_RANGE_ATTACK:
	case INTERFACEMODE_AIRSTRIKE:
	case INTERFACEMODE_REBASE:
	{
		const auto& info = gGlobals.getInterfaceModeInfo(mInterfaceMode);
		if (const int mission = info.getMissionType(); mission != NO_MISSION)
		{
			if (mInterfaceSelectionList.canDoInterfaceModeAt(mInterfaceMode, plot))
			{
				resetInterfaceMode();
				game.selectionListGameNetMessage(GAMEMESSAGE_PUSH_MISSION, mission, plot->getX(), plot->getY(), 0, false, shift);
				onGameStateChanged(EGameStateChangeReason::PushMessage);
			}
			else
				resetInterfaceMode();
		}
		break;

	}
	default:
		break;
	}
}

void CvTuiMainInterface::onWorldViewButtonUpOnPlot(CvPlot* plot, hecktui::ModifierKeyState modifierKeys)
{
	if (!isInCityScreen()) // Stops you from exiting city screen if you happen to mouse up on a unit
	{
		if (!mIsUnitDragActive)
		{
			if (mWasPlotClickForUnitSelection)
			{
				CvUnit* const centerUnit = plot->getCenterUnit();

				if (centerUnit && centerUnit->getOwner() == gGlobals.getGame().getActivePlayer())
				{
					//mIsUnitDragActive = true;
					if (mInterfaceSelectionList.getNumUnits() <= 0 || mInterfaceSelectionList.plot() != plot)
						// || mOriginalUnitPlot != plot)
					{
						mOriginalUnitPlot = nullptr;
						//std::clog << "XXX: Selecting group" << std::endl;
						gGlobals.getGame().selectGroup(centerUnit, false, false, false);
					}
					onGameStateChanged(EGameStateChangeReason::UnitSelection);
				}
			}
		}
		else
		{
			mIsUnitDragActive = false;
			// Only do something if going to a different plot.
			//if (plot != mInterfaceSelectionList.plot())
			// Actually, even if to the same plot. This is how you cancel orders by just clicking the plot.
			{
				if (mInterfaceMode == INTERFACEMODE_SELECTION)
					gGlobals.getGame().selectionListMove(plot, modifierKeys.alt, modifierKeys.shift, modifierKeys.ctrl);
				mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT] = true;
				onGameStateChanged(EGameStateChangeReason::Action);
			}
		}
	}

	// Just in case.
	mIsUnitDragActive = false;
	mWasPlotClickForUnitSelection = false;
}

void CvTuiMainInterface::onWorldViewMiddleButtonOnPlot([[maybe_unused]] CvPlot* plot)
{
	lookAtSelectionPlot();
}

void CvTuiMainInterface::exitCityScreen()
{
	if (mIsInCityScreen)
	{
		if (mTestProductionOnCityScreenExit)
		{
			if (CvCity* const city = getHeadSelectedCity())
				// This is seemingly called before the city selection changes.
				city->updateSelectedCity(mTestProductionOnCityScreenExit);
			mTestProductionOnCityScreenExit = false;
		}


		clearSelectedCities();
		assert(!isInCityScreen());
		//updateInterfaceEnterExitCityScreen();
		//isInCityScreen = false;
		//onGameStateChanged(EGameStateChangeReason::CityScreen);
	}
}

void CvTuiMainInterface::enterCityScreen(CvCity* city, bool testProduction)
{
	if (mIsInCityScreen)
		exitCityScreen();

	// Okay, what happens here is that `bTestProduction` is deferred until exit city screen.

	mTestProductionOnCityScreenExit = testProduction;

	mSelectedCityList.clear();
	mSelectedCityList.insertAtEnd(city->getIDInfo());
	mIsInCityScreen = true;
	mIsUnitDragActive = false; // Let's not move units during the camera movement
	lookAtSelectionPlot();
	mWorldView.updateFog();
	setCitySelectionInterfaceDirtyBits();
	onGameStateChanged(EGameStateChangeReason::CityScreen);
	onGameStateChanged(EGameStateChangeReason::CitySelection);

	city->updateSelectedCity(false);
}

void CvTuiMainInterface::onWorldViewDoubleClickPlot(CvPlot* plot)
{
	if (mIsInCityScreen)
		return;

	if (CvCity* const city = plot->getPlotCity(); city && city->canBeSelected())
	{
		enterCityScreen(city, false);
	}
	else
	{
		CvUnit* const unit = plot->getCenterUnit();
		if (unit && unit->getOwner() == gGlobals.getGame().getActivePlayer())
		{
			gGlobals.getGame().selectGroup(unit, false, false, true);
			onGameStateChanged(EGameStateChangeReason::UnitSelection);
		}
	}
}

void CvTuiMainInterface::onWorldViewMouseOverPlot(CvPlot* plot, bool mouseCaptured)
{
	if (mIsInCityScreen)
	{
		mIsUnitDragActive = false;
		setGotoPlot(nullptr);
	}
	else
	{
		mIsUnitDragActive &= mouseCaptured;

		for (const ScreenWindowEntry& entry : mScreenWindowEntries | std::views::values)
			if (entry.screen)
				entry.screen->updatePlotHelp(plot);

		// The goto plot is used by action UI to list potential actions at the destination.
		setGotoPlot(mIsUnitDragActive || mInterfaceMode != INTERFACEMODE_SELECTION ? plot : nullptr);
	}
}

void CvTuiMainInterface::onWorldViewMouseLeave()
{
	for (const auto& entry : mScreenWindowEntries | std::views::values)
		if (entry.screen)
			entry.screen->updatePlotHelp(nullptr);
}

void CvTuiMainInterface::onBeforeWorldViewRender()
{
	mWorldView.updateBeforeDraw();
	updateWorldViewDisplayedPath();
}

void CvTuiMainInterface::onCityBillboardMouseOver(std::weak_ptr<const hecktui::Element> uiElement, IDInfo cityIdInfo)
{
	mCityBillboardHoverState = { std::move(uiElement), cityIdInfo };
	onWorldViewMouseLeave();
}

CvCity* CvTuiMainInterface::getCurrentCityBillboardHoverCity() const
{
	if (auto ctrl = mCityBillboardHoverState.uiElement.lock())
		if (CvApp::getInstance().getUI().getCurrentHoverControl() == ctrl)
			return getCity(mCityBillboardHoverState.cityIdInfo);
	return nullptr;
}

void CvTuiMainInterface::onGameStateChanged(EGameStateChangeReason reason)
{
	mGameStateChangeReasons[(int)reason] = true;
}

bool CvTuiMainInterface::isFocusedWidget() const
{
	// This is used when minimising scores.
	return gGlobals.getDiplomacyScreen() || gGlobals.getMPDiplomacyScreen();
}

bool CvTuiMainInterface::isFocused() const
{
	// DLL code: "Do NOT execute if a screen is up..."
	// This is also part of the condition of allowing End Turn. During combat, the end turn button does nothing.
	// MyCvDLLInterfaceIFace::setCombatFocus also affects this.

	if (isFocusedWidget() || mIsCombatFocus)
		return true;

	// Adding popup condition for Axolotl8 turn 113 BC-0050.CivBeyondSwordSave. Second city popup lookAt gets overriden by updateSelectionList.
	if (mCurrentPopup)
		return true;

	if (const CvUnit* const unit = findSelectedUnitByIndex(0))
		if (CvSelectionGroup* const group = unit->getGroup())
			if (group->isBusy())
				return true;

	// This is zero inside CvUnit::kill.
	// There is also a call to CvGame::getGameState in here.
	if (gGlobals.getGame().countNumHumanGameTurnActive() <= 0)
		return true;

	return false;
}

bool CvTuiMainInterface::hasWaitingPopupsOrDiplo() const
{
	const auto playerI = gGlobals.getGame().getActivePlayer();
	if (playerI == NO_PLAYER)
		return false;
	const auto& player = CvPlayerAI::getPlayerNonInl(playerI);
	return !player.getPopups().empty() || !player.getDiplomacy().empty();
}

void CvTuiMainInterface::updateSelectionList()
{
	CvGame& game = gGlobals.getGame();
	// There's a very specific behaviour going on here.
	// When founding a city, the unit is destroyed and selection should cycle to the next group.
	// But that won't happen if the city name popup is focused.
	// In Civ4, the popup is probably deferred, so selection cycling happens.
	// But here, the popup is immediate, so, with handling in popup submit, selection cycling happens after the popup is submitted instead of immediately after founding a city.
	//
	// New bug: If isFocused == true and mCycleSelectionCounter > 0, then mCycleSelectionCounter > 0 after closing diplo or whatever. And then when you select a unit, selection cycling will snap to a different unit.
	if (game.getActivePlayer() != NO_PLAYER && !isFocused() && mSelectedCityList.getLength() == 0 && !hasWaitingPopupsOrDiplo())
	{
		if (mCycleSelectionCounter <= 0)
			game.updateTestEndTurn();
		else
		{
			//mCycleSelectionCounter = std::max(0, mCycleSelectionCounter - 1);
			mCycleSelectionCounter = 0; // Instant!
			if (mCycleSelectionCounter <= 0)
			{
				// Reset this if the selection group moved.
				// This fixes the problem when you move a stack and then try to select units on the original plot, and the plot doesn't get selected.
				if (mOriginalUnitPlot != mInterfaceSelectionList.plot())
					mOriginalUnitPlot = nullptr;
				game.updateSelectionList();

				if (!findSelectedUnitByIndex(0) && CvPlayerAI::getPlayerNonInl(game.getActivePlayer()).hasBusyUnit())
					mCycleSelectionCounter = 1;
			}
		}
	}
}

CvCity* CvTuiMainInterface::getHeadSelectedCity() const noexcept
{
	for (const IDInfo& cityID : viewCLinkList(mSelectedCityList))
		if (CvCity* const city = getCity(cityID))
			return city;
	return nullptr;
}

CvPlot* CvTuiMainInterface::getGotoPlot() const noexcept
{
	return mGotoPlot;
}

const WorldView& CvTuiMainInterface::getWorldView() const noexcept
{
	return mWorldView;
}
WorldView& CvTuiMainInterface::getWorldView() noexcept
{
	return mWorldView;
}

CvGInterfaceScreen& CvTuiMainInterface::grabPythonScreen(std::string name, cvengine::ECvScreen kind)
{
	auto& screen = mScreenWindowEntries.emplace(kind, ScreenWindowEntry()).first->second.screen;
	if (!screen)
		screen = std::make_unique<PythonScreen>(std::move(name), kind);
	return *screen;
}

void CvTuiMainInterface::showScreen(cvengine::ECvScreen kind, PopupStates popupState, bool passInput)
{
	// TODO: What does a queued screen mean? (top civs)
	//assert(popupState == PopupStates::POPUPSTATE_IMMEDIATE);

	ScreenWindowEntry& entry = mScreenWindowEntries[kind];
	entry.lastPopupState = popupState;
	entry.screen->setWantClose(false); // Override hideScreen

	if (!entry.currentTuiWindow)
	{
		entry.currentTuiWindow = entry.screen->createTuiWindow(passInput);

		// Screens are inserted front to back, in front of the main interface.
		//const auto& mainInterfaceEntry = mScreenWindowEntries[cvengine::ECvScreen::MAIN_INTERFACE];
		//CvApp::getInstance().getUI().insertWindowAfter(mainInterfaceEntry.currentTuiWindow.get(), entry.currentTuiWindow);
		CvApp::getInstance().getUI().pushWindow(entry.currentTuiWindow);
	}
}

bool CvTuiMainInterface::isActive(cvengine::ECvScreen kind) const
{
	const auto it = mScreenWindowEntries.find(kind);
	return it != mScreenWindowEntries.end() && it->second.currentTuiWindow;
}

//// Called from python
//void CvTuiMainInterface::cacheInterfacePlotUnits(CvPlot* plot)
//{
//	mCachedPlotUnitList.clear();
//	gGlobals.getGame().getPlotUnits(plot, mCachedPlotUnitList);
//}
//size_t CvTuiMainInterface::getNumCachedInterfacePlotUnits() const noexcept
//{
//	return mCachedPlotUnitList.size();
//}
//CvUnit* CvTuiMainInterface::getCachedInterfacePlotUnit(size_t i) const noexcept
//{
//	return mCachedPlotUnitList[i];
//}

hecktui::iaabb2 CvTuiMainInterface::updateAndGetMinimapSectionRect()
{
	if (mInterfaceDirtyBits[MinimapSection_DIRTY_BIT])
	{
		mInterfaceDirtyBits[MinimapSection_DIRTY_BIT] = false;
		mMinimapSectionTracking.update();
	}
	return mMinimapSectionTracking.getCurrentRect();
}

const CvSelectionGroup& CvTuiMainInterface::getInterfaceSelectionList() const
{
	return mInterfaceSelectionList;
}
CvSelectionGroup& CvTuiMainInterface::getInterfaceSelectionList()
{
	return mInterfaceSelectionList;
}

void CvTuiMainInterface::lookAtSelectionPlot()
{
	if (const CvPlot* const plot = getSelectedCityOrUnitPlot())
		mWorldView.setCenterPlot({ plot->getX(), plot->getY() });
}



void CvTuiMainInterface::resetInterfaceMode()
{
	if (mInterfaceMode != INTERFACEMODE_SELECTION)
	{
		// Seems like if you select the same action again, it will cancel itself.
		//if (mInterfaceMode == mode)
		//	mode = INTERFACEMODE_SELECTION;

		mInterfaceMode = INTERFACEMODE_SELECTION;
		mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT] = true;
		mInterfaceDirtyBits[SelectionButtons_DIRTY_BIT] = true;
		mInterfaceDirtyBits[Help_DIRTY_BIT] = true;
		setGotoPlot(nullptr);
		mInterfaceDirtyBits[Center_DIRTY_BIT] = true;
		mInterfaceDirtyBits[Cursor_DIRTY_BIT] = true;
		mInterfaceDirtyBits[HighlightPlot_DIRTY_BIT] = true;
	}
}

void CvTuiMainInterface::playUnitSelectionSound(const CvUnit& unit)
{
	if (!std::exchange(mHasUnitSelectionSoundPlayed, true))
		CvApp::getInstance().audioSystem->playScript3DSoundByIndex(unit.getSelectionSoundScript(), { unit.plot()->getX(), unit.plot()->getY() });
}



#pragma region(privates)

void CvTuiMainInterface::processEvents()
{
	bool somethingChanged = false;

	//if (mGameStateChangeReasons[(int)EGameStateChangeReason::Action])
	//{
	//	CvMessageControl::getInstance().sendAutoMoves();
	//	onGameStateChanged(EGameStateChangeReason::PushMessage);
	//}

	if (DLLMessageQueue::getInstance().execute())
	{
		// There were messages, so game state may have changed. This is specifically checked for revolt from python script.
		onGameStateChanged(EGameStateChangeReason::PushMessage);
	}

	if (mGameStateChangeReasons.any())
	{
		CvGame& game = gGlobals.getGame();
		int oldCycleSelectionCounter = mCycleSelectionCounter;
		int numUpdates = 0;
		const bool isAIAutoPlay = game.getAIAutoPlay() > 0;
		const bool isStopOnVictory = isAIAutoPlay && game.getGameState() == GAMESTATE_ON;
		const auto t0 = std::chrono::steady_clock::now();
		// Used to be 10. `Nuke Test.CivBeyondSwordSave` needs 14 on end turn.
		//for (int i = 0; i < 100 || game.getAIAutoPlay() > 0; ++i)
		for (;;)
		{
			if (isAIAutoPlay)
			{
				// If you want to debug after X turns of AI autoplay...
				//if (game.getGameTurn() == 50)
				//{
				//	MyCvDLLUtility::getInstance().SaveGame(SAVEGAME_NORMAL);
				//	std::clog << "Stopping AI autoplay on game turn " << game.getGameTurn() << std::endl;
				//	std::exit(EXIT_SUCCESS);
				//}

				//if (game.getGameTurn() >= 12)
				//{
				//	game.setAIAutoPlay(0);
				//	break;
				//}

				if (isStopOnVictory && game.getGameState() != GAMESTATE_ON)
				{
					std::clog << "Stopping AI autoplay on game.getGameState() != GAMESTATE_ON.\n";
					std::clog << "Turn = " << game.getGameTurn() << '\n';
					std::clog << "Turnslice = " << game.getTurnSlice() << '\n';
					const int turnSliceShift = (4 - game.getTurnSlice() % 4) % 4;
					game.changeTurnSlice(turnSliceShift);
					for (const int k : range(4))
					{
						std::clog << "SYNC CHECKSUM " << k << " = " << game.calculateSyncChecksum() << '\n';
						game.changeTurnSlice(1);
					}
					game.changeTurnSlice(-turnSliceShift-4);
					std::clog << std::flush;
					std::exit(EXIT_SUCCESS);
				}
			}

			CvPlayer& activePlayer = CvPlayerAI::getPlayerNonInl(game.getActivePlayer());

			//if (activePlayer.isTurnActive() && activePlayer.isHuman() && !activePlayer.hasReadyUnit(true) && !activePlayer.isAutoMoves())
			//	activePlayer.AI_unitUpdate();

			// Process messages now. Player bot wants this after alt-clicking end turn button, so that the turn is ended before the bot is invoked.
			(void)DLLMessageQueue::getInstance().execute();

			game.update();

			updateSelectionList();
			game.updateTestEndTurn();
			const bool newMessages = DLLMessageQueue::getInstance().execute();

			// Because this TUI loop is purely action-based, some logic is needed to process real-time events.
			// This seems to work for now. Typically updates 2 to 4 times.
			++numUpdates;
			//if (!newMessages && oldCycleSelectionCounter == mCycleSelectionCounter
			//	&& activePlayer.isTurnActive()
			//	//&& CvPlayerAI::getPlayerNonInl(game.getActivePlayer()).isAlive()
			//	&& activePlayer.isHuman() // isHuman can be false in AI autoplay if the AI resurrects player 0 as a colony.
			//	&& !activePlayer.isAutoMoves()) // This check stops the next update from interferring with unit selection.
			//{
			//	break;
			//}

			const auto hasReadyNonAutomatedUnit = [&] {
				CvSelectionGroup* pLoopSelectionGroup;
				int iLoop;

				for (pLoopSelectionGroup = activePlayer.firstSelectionGroup(&iLoop); pLoopSelectionGroup; pLoopSelectionGroup = activePlayer.nextSelectionGroup(&iLoop))
				{
					if (pLoopSelectionGroup->readyToMove(false) && !pLoopSelectionGroup->isAutomated())
					{
						return true;
					}
				}

				return false;
				};

			// Try to return control to player only when there's something to do. Hopefully we don't get into an infinite loop.
			// Intended to immediately update automated units at the end of a turn, as in "Automation turns test - Histories6 turn 112 BC-2380.CivBeyondSwordSave".
			if (!newMessages && oldCycleSelectionCounter == mCycleSelectionCounter)
				if (activePlayer.isTurnActive() || game.getGameState() == GAMESTATE_OVER)
					if (hasWaitingPopupsOrDiplo() || isFocusedWidget() || mCurrentPopup || isEndTurnMessage() || hasReadyNonAutomatedUnit())
						break;

			oldCycleSelectionCounter = mCycleSelectionCounter;
		}
		const auto t1 = std::chrono::steady_clock::now();
		std::clog << "Performed " << numUpdates << " game updates in " << std::chrono::duration<double>(t1 - t0) << "." << std::endl;

		updateSelectionList();

		mWorldView.invalidateAll();
		somethingChanged = true;
	}

	mWantUIUpdate |= somethingChanged;
	mWantUIUpdate |= mGameStateChangeReasons[(int)EGameStateChangeReason::GotoPlotChanged];
	mGameStateChangeReasons = {};

	mHasUnitSelectionSoundPlayed = false;

	if (mInterfaceDirtyBits[UnitInfo_DIRTY_BIT])
	{
		mInterfaceDirtyBits[UnitInfo_DIRTY_BIT] = false;
		// This is the trigger for updateCenterUnit.
		gGlobals.getMap().updateCenterUnit();
	}

	// Needed for changing working plots in city screen.
	if (mInterfaceDirtyBits[ColoredPlots_DIRTY_BIT])
	{
		mInterfaceDirtyBits[ColoredPlots_DIRTY_BIT] = false;
		mWorldView.updateColouredPlots();
	}

	// After selection cycling.
	// There's some extra condition here that delays the initial `lookAtSelectionPlot` when starting a new game.
	// Not sure what it is exactly.
	// But `lookAtSelectionPlot` is delayed until you close the Dawn Of Man screen.
	// Note that for this behaviour to take effect, you need to check the player's popup queue before getting here.
	if (mInterfaceDirtyBits[SelectionCamera_DIRTY_BIT] && !hasActiveScreens())
	{
		mInterfaceDirtyBits[SelectionCamera_DIRTY_BIT] = false;
		lookAtSelectionPlot();
	}

	CvApp::getInstance().audioSystem->updateListener();
}

void CvTuiMainInterface::updateUI()
{
	updatePopups();

	if (mTurnLogWindow)
	{
		if (mTurnLogIsDirty)
		{
			mTurnLogIsDirty = false;
			mTurnLogWindow->dirtyTurnLog(NO_PLAYER);
		}
		if (mTurnLogWindow->wantClose(nullptr))
			toggleTurnLog();
	}

	//if (mDebugWindow)
	//{
	//	if (mDebugWindow->wantClose(nullptr))
	//	{
	//		CvApp::getInstance().getUI().removeWindow(mDebugWindow.get());
	//		mDebugWindow = nullptr;
	//	}
	//}

	const auto checkNeedUIRebuild = [this] {
		return std::exchange(mLastScreenUpdatePythonInstanceSerial, MyCvDLLPython().getCurrentPythonEnvironmentSerial())
			!= mLastScreenUpdatePythonInstanceSerial;
		};

	mWantUIUpdate |= mLastScreenUpdatePythonInstanceSerial != MyCvDLLPython().getCurrentPythonEnvironmentSerial();

	if (mWantUIUpdate)
	{
		bool needUIRebuild = checkNeedUIRebuild();

		for ([[maybe_unused]] int i = 0; ; ++i)
		{
			try
			{
				if (needUIRebuild)
					for (auto& [kind, entry] : mScreenWindowEntries)
						if (entry.currentTuiWindow)
							entry.screen->rebuildPythonScreen();

				for (;;)
				{
					const auto oldDirtyBits = mInterfaceDirtyBits;
					for (const auto& [kind, entry] : mScreenWindowEntries)
						if (entry.currentTuiWindow)
							entry.screen->updateFromGameState(*entry.currentTuiWindow);
					// If bits were set, do the update again. Hack for CvMainInterface->CvDomesticAdvisor update.
					if ((mInterfaceDirtyBits & ~oldDirtyBits).none())
						break;
				}

				break;
			}
			catch (const std::runtime_error&)
			{
				CvApp::getInstance().getUI().modalLoopPythonReloadPrompt(L"UI rebuild or update call failed.", L"Reload All Python");
				MyCvDLLPython().restart();
			}

			needUIRebuild = true;
		}
	}

	mWantUIUpdate = false;

	if (mInterfaceDirtyBits[MinimapSection_DIRTY_BIT])
	{
		mInterfaceDirtyBits[MinimapSection_DIRTY_BIT] = false;
		mMinimapSectionTracking.update();
	}
}

void CvTuiMainInterface::showPopup(std::unique_ptr<CvPopup> popup)
{
	if (mCurrentPopup || (popup->state != POPUPSTATE_IMMEDIATE && hasActiveScreens()))
	{
		mPopupQueue.push_back(std::move(popup));
	}
	else
	{
		auto dialog = std::make_shared<CvPopupTuiDialog>(popup.get());
		CvApp::getInstance().getUI().pushWindow(dialog);
		popup->window = std::move(dialog);
		mCurrentPopup = std::move(popup);
		mCurrentPopup->onPopupDisplayed();
	}
}
void CvTuiMainInterface::updatePopups()
{
	const bool wasFocused = isFocused();

	if (mCurrentPopup && mCurrentPopup->window->isWantClose())
	{
		CvApp::getInstance().getUI().removeWindow(static_cast<CvPopupTuiDialog*>(mCurrentPopup->window.get()));
		mCurrentPopup = nullptr;
	}

	// Close screens.
	for (auto&& [kind, entry] : mScreenWindowEntries)
	{
		if ((entry.currentTuiWindow && entry.currentTuiWindow->wantClose(nullptr)) || (entry.screen && entry.screen->wantClose()))
		{
			CvApp::getInstance().getUI().removeWindow(entry.currentTuiWindow.get());
			entry.currentTuiWindow = nullptr;
			// Keep the research screen around.
			if (!entry.screen->isPersistent())
				entry.screen = nullptr;
		}
	}

	{
		ScreenWindowEntry& diploEntry = mScreenWindowEntries[cvengine::ECvScreen::DIPLOMACY_SCREEN];
		if (!diploEntry.currentTuiWindow)
		{
			// Try diplo.
			CvPlayer& player = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer());
			while (CvDiploParameters* const diploParams = player.popFrontDiplomacy())
			{
				// Taking ownership of diplo params.
				if (auto diploScreenUI = std::make_unique<CvTuiDiplomacyScreen>(std::unique_ptr<CvDiploParameters>(diploParams)))
				{
					if (!diploScreenUI->wantClose())
					{
						diploEntry.screen = std::move(diploScreenUI);
						showScreen(cvengine::ECvScreen::DIPLOMACY_SCREEN, POPUPSTATE_IMMEDIATE, false);
					}
					break;
				}
			}
		}
	}

	// Wait for diplo. At least, need to make sure an event popup doesn't show up until after diplo in "AutoSave_AD-1120 test immediate AI demand".
	if (!mCurrentPopup && !gGlobals.getDiplomacyScreen())
	{
		// Try normal popups.
		if (mPopupQueue.empty() && !hasActiveScreens())
		{
			CvPlayer& player = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer());
			if (CvPopupInfo* const info = player.popFrontPopup())
			{
				// For Axolotl8 turn 113 BC-0050.CivBeyondSwordSave. Ensure second city production popup gets updated choices.
				if (DLLMessageQueue::getInstance().execute())
					onGameStateChanged(EGameStateChangeReason::PushMessage);
				CvButtonPopup::launch(std::make_unique<CvButtonPopup>(std::unique_ptr<CvPopupInfo>(info)));
			}
		}

		if (!mPopupQueue.empty() && !hasActiveScreens())
		{
			auto popup = std::move(mPopupQueue.front());
			mPopupQueue.pop_front();
			showPopup(std::move(popup));
		}
	}

	// This changes the condition in updateSelectionList, so it needs to be called.
	if (wasFocused && !isFocused())
		onGameStateChanged(EGameStateChangeReason::Action);
}
bool CvTuiMainInterface::hasActiveScreens() const
{
	return isInCityScreen() || std::ranges::any_of(mScreenWindowEntries, [](const decltype(mScreenWindowEntries)::value_type& kv) {
		return kv.first != cvengine::ECvScreen::MAIN_INTERFACE && kv.second.currentTuiWindow;
		});
}
void CvTuiMainInterface::setGotoPlot(CvPlot* plot)
{
	if (mGotoPlot != plot)
	{
		mGotoPlot = plot;
		onGameStateChanged(EGameStateChangeReason::GotoPlotChanged);
		mInterfaceDirtyBits[InterfaceDirtyBits::Help_DIRTY_BIT] = true;
		mInterfaceDirtyBits[InterfaceDirtyBits::HighlightPlot_DIRTY_BIT] = true;
		mInterfaceDirtyBits[InterfaceDirtyBits::Flag_DIRTY_BIT] = true; // CvPlot::updateFlagSymbol gets called, but we don't have a flag, so just set the bit.
		mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT] = true;
		mInterfaceDirtyBits[InterfaceDirtyBits::SelectionButtons_DIRTY_BIT] = true;
		CvEventReporter::getInstance().gotoPlotSet(plot, gGlobals.getGame().getActivePlayer());
	}
}
void CvTuiMainInterface::updateWorldViewDisplayedPath()
{
	if (!mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT])
		return;

	mInterfaceDirtyBits[InterfaceDirtyBits::Waypoints_DIRTY_BIT] = false;

	mWorldViewDisplayedPath.clear();
	mWorldView.clearAreaBorderPlots(AREA_BORDER_LAYER_HIGHLIGHT_PLOT);

	switch (mIsUnitDragActive ? INTERFACEMODE_GO_TO : mInterfaceMode)
	{
	case INTERFACEMODE_SELECTION:
	case INTERFACEMODE_GO_TO:
	case INTERFACEMODE_GO_TO_TYPE:
	case INTERFACEMODE_GO_TO_ALL:
	case INTERFACEMODE_ROUTE_TO:
	{
		if (mIsInCityScreen)
			break;

		//if (!mGotoPlot)
		//	break; // TODO: Here, Civ4 will display queued mission pathing.

		const CvUnit* const unit = findSelectedUnitByIndex(0);
		if (!unit)
			break;

		if (unit->getTeam() != gGlobals.getGame().getActiveTeam())
			break;

		if (unit->getDomainType() == DOMAIN_AIR)
			break; // TODO: Here, Civ4 seems to display a single waypoint. Calls `CvSelectionGroup::canMoveInto`.

		mWorldViewDisplayedPath.reserve(100);

		const CvPlot* start = unit->plot();

		const auto appendPathTo = [&](const CvPlot* dest, bool withTurnNumbering) {
			auto& fastarIFace = *gGlobals.getDLLIFaceNonInl()->getFAStarIFace();
			FAStar& pathFinder = gGlobals.getInterfacePathFinder();
			fastarIFace.SetData(&pathFinder, &mInterfaceSelectionList);

			// Have to use MOVE_DECLARE_WAR for pathDestValid.
			if (fastarIFace.GeneratePath(&pathFinder, start->getX(), start->getY(), dest->getX(), dest->getY(), false, MOVE_DECLARE_WAR, true))
			{
				// Pathing succeeded.
				const CvMap& map = gGlobals.getMap();
				const PlayerTypes player = gGlobals.getGame().getActivePlayer();
				const FAStarNode* node = fastarIFace.GetLastNode(&pathFinder);
				do
				{
					const heck::ivec2 coord{ node->m_iX, node->m_iY };
					const CvPlot* const plot = map.plot(coord.x, coord.y);
					mWorldViewDisplayedPath.push_back({
						.coord = coord,
						.turn = withTurnNumbering ? node->m_iData2 : -1,
						.red = !plot || plot->isVisibleEnemyUnit(player), // Plot *should* be valid...
						});
					node = node->m_pParent;
				} while (node);

				std::ranges::reverse(mWorldViewDisplayedPath);
			}
			else
			{
				// Pathing failed.
				mWorldViewDisplayedPath.push_back({
					.coord = { dest->getX(), dest->getY() },
					.red = true,
					});
			}

			start = dest;

			};

		// Don't display just a single waypoint at the unit.
		if (mGotoPlot)
		{
			if (unit->plot() != mGotoPlot)
			{
				appendPathTo(mGotoPlot, true);
			}
		}
		else
		{
			// Display mission path.
			if (const CvSelectionGroup* const unitsGroup = unit->getGroup())
			{
				for (auto* node = unitsGroup->headMissionQueueNode(); node; node = unitsGroup->nextMissionQueueNode(node))
				{
					switch (node->m_data.eMissionType)
					{
					case MISSION_MOVE_TO:
					case MISSION_ROUTE_TO:
						appendPathTo(GC.getMap().plot(node->m_data.iData1, node->m_data.iData2), false);
						break;

					case MISSION_MOVE_TO_UNIT:
						if (const CvUnit* const pTargetUnit = GET_PLAYER((PlayerTypes)node->m_data.iData1).getUnit(node->m_data.iData2))
							appendPathTo(pTargetUnit->plot(), false);
						break;

					default:
						break;
					}
				}
			}
		}

		break;
	}
	case INTERFACEMODE_SIGN:
		if (!mGotoPlot)
			break;
		mWorldView.addAreaBorderPlot({ mGotoPlot->getX(), mGotoPlot->getY() }, AREA_BORDER_LAYER_HIGHLIGHT_PLOT,
			gGlobals.getColorInfo((ColorTypes)gGlobals.getInfoTypeForString("COLOR_WHITE")).getColor()
		);
		break;
	default:
		if (!mGotoPlot)
			break;
		mWorldView.addAreaBorderPlot({ mGotoPlot->getX(), mGotoPlot->getY() }, AREA_BORDER_LAYER_HIGHLIGHT_PLOT,
			gGlobals.getColorInfo((ColorTypes)gGlobals.getInfoTypeForString(mInterfaceSelectionList.canDoInterfaceModeAt(mInterfaceMode, mGotoPlot) ? "COLOR_ALT_HIGHLIGHT_TEXT" : "COLOR_GREY")).getColor()
		);
		break;
	}

	mWorldView.setDisplayedPath(mWorldViewDisplayedPath);
}
void CvTuiMainInterface::setCitySelectionInterfaceDirtyBits()
{
	// + MiscButtons_DIRTY_BIT PlotListButtons_DIRTY_BIT SelectionButtons_DIRTY_BIT CitizenButtons_DIRTY_BIT Center_DIRTY_BIT SelectionSound_DIRTY_BIT
	//   InfoPane_DIRTY_BIT ColoredPlots_DIRTY_BIT
	mInterfaceDirtyBits[MiscButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[PlotListButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[SelectionButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[CitizenButtons_DIRTY_BIT] = true;
	mInterfaceDirtyBits[Center_DIRTY_BIT] = true;
	mInterfaceDirtyBits[SelectionSound_DIRTY_BIT] = true;
	mInterfaceDirtyBits[InfoPane_DIRTY_BIT] = true;
	mInterfaceDirtyBits[ColoredPlots_DIRTY_BIT] = true;
}

#pragma endregion