#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"
#include "CvApp.h"
#include "AudioSystem.h"
#include "DXT.h"

#include <Cv4CommonEngineLib/CvPopup.h>

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvCity.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvMap.h>

#include <CommonStuff/range.h>

#include <iostream>

using namespace cvengine;

using heck::range;

CvTuiInterface& CvTuiInterface::getInstance()
{
	static CvTuiInterface x;
	return x;
}

CvTuiInterface::CvTuiInterface() = default;
CvTuiInterface::~CvTuiInterface() noexcept = default;

void CvTuiInterface::uninit()
{
	CvInterface::uninit();
	mMainInterface = nullptr;
	CvApp::getInstance().audioSystem->clearSoundScape();
	mIsExitingToMainMenu = false;
}
void CvTuiInterface::reset()
{
	CvInterface::reset();
	mMainInterface = std::make_unique<CvTuiMainInterface>();
	CvApp::getInstance().audioSystem->clearSoundScape();
	mIsExitingToMainMenu = false;
}

CvTuiMainInterface* CvTuiInterface::getTuiMainInterface()
{
	return mMainInterface.get();
}

WorldView* CvTuiInterface::getWorldView()
{
	if (mMainInterface)
		return &mMainInterface->getWorldView();
	else
		return nullptr;
}

bool CvTuiInterface::isAIAutorunActive() const
{
	return mMainInterface && mMainInterface->isAIAutorunActive();
}

bool CvTuiInterface::isInMainMenu() const
{
	// Probably
	return !!mMainInterface;
}
bool CvTuiInterface::isLeftMouseDown() const
{
	std::abort();
}
bool CvTuiInterface::isRightMouseDown() const
{
	std::abort();
}

void CvTuiInterface::startMainInterface()
{
	if (mMainInterface)
		mMainInterface->startMainInterface();
}

/// Camera control
void CvTuiInterface::lookAtPlot(ivec2 coord)
{
	if (mMainInterface)
		mMainInterface->lookAt(coord);
}
void CvTuiInterface::lookAtCityBuilding(int iCityId, BuildingTypes)
{
	lookAtCityOffset(iCityId);
}
void CvTuiInterface::lookAtSelectionPlot()
{
	if (mMainInterface)
		mMainInterface->lookAtSelectionPlot();
}
void CvTuiInterface::lookAtCityOffset(int iCity)
{
	if (const CvCity* const city = GET_PLAYER(gGlobals.getGame().getActivePlayer()).getCity(iCity))
		lookAtPlot({ city->getX(), city->getY() });
}
CvPlot* CvTuiInterface::getLookAtPlot() const
{
	if (mMainInterface)
		return mMainInterface->getLookAtPlot();
	else
		return nullptr;
}
CvPlot* CvTuiInterface::getMouseOverPlot() const
{
	return getLookAtPlot();
}
CvPlot* CvTuiInterface::getSelectedCityOrUnitPlot() const
{
	if (mMainInterface)
		return mMainInterface->getSelectedCityOrUnitPlot();
	else
		return nullptr;
}

const CvSelectionGroup* CvTuiInterface::getSelectionList() const
{
	if (mMainInterface)
		return &mMainInterface->getInterfaceSelectionList();
	else
		return nullptr;
}
CvSelectionGroup* CvTuiInterface::getSelectionList()
{
	if (mMainInterface)
		return &mMainInterface->getInterfaceSelectionList();
	else
		return nullptr;
}
void CvTuiInterface::makeSelectionListDirty()
{
	if (mMainInterface)
		mMainInterface->makeSelectionListDirty();
}
void CvTuiInterface::selectionListPostChange()
{
}
void CvTuiInterface::selectionListPreChange()
{
}

void CvTuiInterface::clearQueuedPopups()
{
}
bool CvTuiInterface::isDiploOrPopupWaiting() const
{
	return mMainInterface && mMainInterface->hasWaitingPopupsOrDiplo();
}
// This is what finally gets called after CvDLLButtonPopup has filled in the controls.
// Implementation should behave according to PopupStates, and set IPopupUIWindow when displayed.
void CvTuiInterface::launchPopup(std::unique_ptr<CvPopup> pPopup, bool bCreateOkButton, PopupStates bState, [[maybe_unused]] int iNumPixelScroll)
{
	if (mMainInterface)
		mMainInterface->launchPopup(std::move(pPopup), bCreateOkButton, bState);
}

CvUnit* CvTuiInterface::getLastSelectedUnit() const
{
	// Not implemented.
	return nullptr;
}
void CvTuiInterface::setLastSelectedUnit([[maybe_unused]] CvUnit* pUnit)
{
}
CvPlot* CvTuiInterface::getGotoPlot() const
{
	if (mMainInterface)
		return mMainInterface->getGotoPlot();
	else
		return nullptr;
}
CvPlot* CvTuiInterface::getSingleMoveGotoPlot() const
{
	// Not implemented. Used in updateFlagSymbol.
	return nullptr;
}
CvPlot* CvTuiInterface::getOriginalUnitSelectionPlot() const
{
	if (mMainInterface)
		return mMainInterface->getOriginalUnitSelectionPlot();
	else
		return nullptr;
}
int CvTuiInterface::getOriginalUnitSelectionIndex() const
{
	if (mMainInterface)
		return mMainInterface->getOriginalUnitSelectionIndex();
	else
		return 0;
}

// Possibly called by Rhye's and Fall of Civilization.
// Index should be a 2D sound as GAME_MUSIC.
void CvTuiInterface::doSoundtrack([[maybe_unused]] int index)
{
	// Not implemented.
}
void CvTuiInterface::play2DSound(int index)
{
	CvApp::getInstance().audioSystem->playScript2DSoundByIndex(index);
}
void CvTuiInterface::play3DSound(int index, const NiPoint3& vPos)
{
	CvMap& map = gGlobals.getMap();
	const int plotX = map.pointXToPlotX(vPos.x);
	const int plotY = map.pointYToPlotY(vPos.x);
	CvApp::getInstance().audioSystem->playScript3DSoundByIndex(index, { plotX, plotY });
}
// iSoundType is probably the sound category as listed in AudioDefines.xml.
//void playGeneralSound(int iSoundId, int iSoundType, NiPoint3 vPos) override;
void CvTuiInterface::playUnitSelectionSound(const CvUnit& unit)
{
	if (mMainInterface)
		mMainInterface->playUnitSelectionSound(unit);
}
void CvTuiInterface::setSoundSelectionReady([[maybe_unused]] bool bReady)
{
	// bReady is always true?

	std::clog << __FUNCTION__ << " unimplemented.\n";
}

void CvTuiInterface::selectCity(CvCity* pNewValue, bool bTestProduction)
{
	if (mMainInterface)
		mMainInterface->enterCityScreen(pNewValue, bTestProduction);
}
void CvTuiInterface::selectLookAtCity([[maybe_unused]] bool bAdd)
{
	// Not implemented.
	std::abort();
}
void CvTuiInterface::addSelectedCity(CvCity* pNewValue, bool bToggle)
{
	if (mMainInterface)
		mMainInterface->addSelectedCity(pNewValue, bToggle);
}
void CvTuiInterface::clearSelectedCities()
{
	if (mMainInterface)
		mMainInterface->clearSelectedCities();
}
bool CvTuiInterface::isCitySelected(CvCity* pCity) const
{
	return mMainInterface && mMainInterface->isCitySelected(pCity);

}
CvCity* CvTuiInterface::getHeadSelectedCity() const
{
	if (mMainInterface)
		return mMainInterface->getHeadSelectedCity();
	else
		return nullptr;
}
bool CvTuiInterface::isCitySelection() const
{
	return mMainInterface && mMainInterface->getHeadSelectedCity();
}
CLLNode<IDInfo>* CvTuiInterface::nextSelectedCitiesNode(CLLNode<IDInfo>* pNode)
{
	if (mMainInterface)
		return mMainInterface->getSelectedCityList().next(pNode);
	else
		return nullptr;
}
CLLNode<IDInfo>* CvTuiInterface::headSelectedCitiesNode()
{
	if (mMainInterface)
		return mMainInterface->getSelectedCityList().head();
	else
		return nullptr;
}
bool CvTuiInterface::isCityScreenUp() const
{
	return mMainInterface && mMainInterface->isInCityScreen();
}

void CvTuiInterface::addTurnMessageDisplayEntry(CvTalkingHeadMessage msg)
{
	if (mMainInterface)
		mMainInterface->addTurnMessageDisplayEntry(std::move(msg));
}
void CvTuiInterface::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
	InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor,
	int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	if (mMainInterface)
		mMainInterface->addMessage(ePlayer, bForce, iLength, std::move(szString), pszSound, eType, pszIcon, eFlashColor, iFlashX, iFlashY, bShowOffScreenArrows, bShowOnScreenArrows);
}
void CvTuiInterface::addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId)
{
	if (mMainInterface)
		mMainInterface->addQuestMessage(ePlayer, szString, iQuestId);
}
void CvTuiInterface::flushTalkingHeadMessages()
{
	// Not implemented. Not needed?
}

void CvTuiInterface::getDisplayedButtonPopups([[maybe_unused]] CvPopupQueue& infos) const
{
	std::clog << __func__ << " not implemented. Used to serialise player popups.\n";
}

int CvTuiInterface::getCycleSelectionCounter() const
{
	if (mMainInterface)
		return mMainInterface->getCycleSelectionCounter();
	else
		return 0;
}
void CvTuiInterface::setCycleSelectionCounter(int iNewValue)
{
	if (mMainInterface)
		mMainInterface->setCycleSelectionCounter(iNewValue);
}
void CvTuiInterface::changeCycleSelectionCounter(int iChange)
{
	if (mMainInterface)
		mMainInterface->changeCycleSelectionCounter(iChange);
}

bool CvTuiInterface::isCombatFocus() const
{
	// No combat focus.
	return false;
}
void CvTuiInterface::setCombatFocus([[maybe_unused]] bool bNewValue)
{
	// Ignoring.
}
//void setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override;

bool CvTuiInterface::getInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem) const
{
	return mMainInterface && mMainInterface->getInterfaceDirtyBit(eDirtyItem);
}
void CvTuiInterface::setInterfaceDirtyBit(InterfaceDirtyBits eDirtyItem, bool bNewValue)
{
	if (mMainInterface)
		mMainInterface->setInterfaceDirtyBit(eDirtyItem, bNewValue);
}

void CvTuiInterface::updateCursorType()
{
}
void CvTuiInterface::updatePythonScreens()
{
	std::clog << "Ignoring call to " << __FUNCTION__ << std::endl;
}

void CvTuiInterface::lookAt(NiPoint3 pt3Target, [[maybe_unused]] CameraLookAtTypes type, [[maybe_unused]] NiPoint3 attackDirection)
{
	CvMap& map = gGlobals.getMap();
	const int x = map.pointXToPlotX(pt3Target.x);
	const int y = map.pointYToPlotY(pt3Target.y);
	lookAtPlot({ x, y });
}
void CvTuiInterface::centerCamera(CvUnit* unit)
{
	if (unit)
		lookAtPlot({ unit->getX(), unit->getY() });
}
void CvTuiInterface::releaseLockedCamera()
{
}
bool CvTuiInterface::isFocusedWidget() const
{
	return mMainInterface && mMainInterface->isFocusedWidget();
}
bool CvTuiInterface::isFocused() const
{
	return mMainInterface && mMainInterface->isFocused();
}
bool CvTuiInterface::isBareMapMode() const
{
	return false;
}
void CvTuiInterface::toggleBareMapMode()
{
}
bool CvTuiInterface::isShowYields() const
{
	return true;
}
void CvTuiInterface::toggleYieldVisibleMode()
{
}
bool CvTuiInterface::isScoresVisible() const
{
	return true;
}
void CvTuiInterface::toggleScoresVisible()
{
}
bool CvTuiInterface::isScoresMinimized() const
{
	return mMainInterface && mMainInterface->isScoresMinimized();
}
void CvTuiInterface::toggleScoresMinimized()
{
	if (mMainInterface)
		mMainInterface->toggleScoresMinimized();
}
//bool isNetStatsVisible() const override;



void CvTuiInterface::setInterfaceMode(InterfaceModeTypes eNewValue)
{
	if (mMainInterface)
		mMainInterface->setInterfaceMode(eNewValue);
}
InterfaceModeTypes CvTuiInterface::getInterfaceMode() const
{
	if (mMainInterface)
		return mMainInterface->getInterfaceMode();
	else
		return InterfaceModeTypes::INTERFACEMODE_SELECTION;
}
InterfaceVisibility CvTuiInterface::getShowInterface() const
{
	return InterfaceVisibility::INTERFACE_SHOW;
}
void CvTuiInterface::setShowInterface(InterfaceVisibility eInterfaceVisibility)
{
	if (eInterfaceVisibility != getShowInterface())
		std::abort();
}

//void setFlashing(PlayerTypes eWho, bool bFlashing) override;
//bool isFlashing(PlayerTypes eWho) const override;
//void setDiplomacyLocked(bool bLocked) override;
//bool isDiplomacyLocked() const override;

void CvTuiInterface::setMinimapColor([[maybe_unused]] MinimapModeTypes eMinimapMode, [[maybe_unused]] ivec2 coord, [[maybe_unused]] ColorTypes eColor, [[maybe_unused]] float fAlpha)
{
	// Ignored.
}
unsigned char* CvTuiInterface::getMinimapBaseTextureDXT1() const
{
	// Called by CvReplayScreen.py.
	// Just generate the texture right here.

	const hecktui::ivec2 dim = getMinimapBaseTextureDim();
	
	std::vector<std::array<uint8_t, 3>> pixels;
	pixels.resize(dim.x * dim.y);
	
	const CvMap& map = gGlobals.getMap();
	const Minimap::RenderCoordMapping mapping(hecktui::iaabb2::sized({}, dim), hecktui::iaabb2::sized({}, { map.getGridWidth(), map.getGridHeight() }));
	
	for (const int y : range(dim.y))
		for (const int x : range(dim.x))
			pixels[x + y * dim.x] = hecktui::toRGB8(Minimap::computeMinimapBaseTexturePixelColour(map, NO_TEAM, mapping, { x, y })).toArray();
	
	// Only used temporarily, so a static is fine.
	static std::vector<unsigned char> texture;
	if (dim.x % 8 != 0) // DDS 8 bytes per 8 pixels.
		std::abort();
	texture = encodeDXT1(dim, pixels);
	return texture.data();
}

// These seem to be used when PLAYEROPTION_WAIT_END_TURN is off.
int CvTuiInterface::getEndTurnCounter() const
{
	std::abort();
}
void CvTuiInterface::setEndTurnCounter([[maybe_unused]] int iNewValue)
{
	std::abort();
}
void CvTuiInterface::changeEndTurnCounter([[maybe_unused]] int iChange)
{
	std::abort();
}

void CvTuiInterface::setEndTurnMessage(bool bNewValue)
{
	if (mMainInterface)
		return mMainInterface->setEndTurnMessage(bNewValue);
}
bool CvTuiInterface::isEndTurnMessage() const
{
	return mMainInterface && mMainInterface->isEndTurnMessage();
}

// The message?
bool CvTuiInterface::shouldDisplayEndTurn() const
{
	return false;
}
// Flag by the minimap
bool CvTuiInterface::shouldDisplayFlag() const
{
	return true;
}
// City screen return message?
bool CvTuiInterface::shouldDisplayReturn() const
{
	return false;
}
// In the bottom left?
bool CvTuiInterface::shouldDisplayUnitModel() const
{
	return false;
}
bool CvTuiInterface::shouldShowResearchButtons() const
{
	// Not used by TUI script.
	std::abort();
}

bool CvTuiInterface::isForcePopup() const
{
	return mMainInterface && mMainInterface->isForcePopup();
}
void CvTuiInterface::setForcePopup(bool bNewValue)
{
	if (mMainInterface)
		return mMainInterface->setForcePopup(bNewValue);
}

/// Turn log
void CvTuiInterface::toggleTurnLog()
{
	if (mMainInterface)
		return mMainInterface->toggleTurnLog();
}
void CvTuiInterface::dirtyTurnLog(PlayerTypes ePlayer)
{
	if (mMainInterface)
		mMainInterface->dirtyTurnLog(ePlayer);
}

//void unlockPopupHelp() override;

void CvTuiInterface::showDetails([[maybe_unused]] bool bPasswordOnly)
{
	std::abort();
}
void CvTuiInterface::showAdminDetails()
{
	std::abort();
}

void CvTuiInterface::toggleClockAlarm([[maybe_unused]] bool bValue, [[maybe_unused]] int iHour, [[maybe_unused]] int iMin)
{
}
bool CvTuiInterface::isClockAlarmOn() const
{
	return false;
}

bool CvTuiInterface::isExitingToMainMenu() const
{
	return mIsExitingToMainMenu;
}
void CvTuiInterface::exitToMainMenu()
{
	mIsExitingToMainMenu = true;
	CvApp::getInstance().deferMainMenu();
}
void CvTuiInterface::setWorldBuilder(bool bTurnOn)
{
	if (bTurnOn)
		std::abort();
}

// Called by scripts.
int CvTuiInterface::determineWidth([[maybe_unused]] const std::wstring& szBuffer) const
{
	// Called by BUG scoreboard. Contains XML.
	// Return dummy value.
	return 1;
}

void CvTuiInterface::setCityTabSelectionRow([[maybe_unused]] CityTabTypes eTabType)
{
	// No city tabs
}
CityTabTypes CvTuiInterface::getCityTabSelectionRow() const
{
	return NO_CITYTAB;
}

bool CvTuiInterface::noTechSplash() const
{
	return false;
}

bool CvTuiInterface::isInAdvancedStart() const
{
	return false;
}
void CvTuiInterface::setInAdvancedStart([[maybe_unused]] bool bAdvancedStart)
{
	std::abort();
}

bool CvTuiInterface::isSpaceshipScreenUp() const
{
	return false;
}
//bool isDebugMenuCreated() const override;

void CvTuiInterface::setBusy([[maybe_unused]] bool bBusy)
{
	// Just a busy cursor?
}
