#include "MyCvDLLInterfaceIFace.h"
#include "MyCvDLLEngineIFace.h"
#include "MyCvDLLUtility.h"
#include "../CvInterface.h"
#include "../Common.h"
#include "../Logger.h"
#include "../CvApp.h"
#include "../WorldView.h"
#include "../CvButtonPopup.h"
#include "../DXT.h"

#include <CLinkListIterator.h>
#include <CvGameAI.h>
#include <CvGlobals.h>
#include <CvPopupInfo.h>
#include <CvGameCoreUtils.h>
#include <CvUnit.h>
#include <CvMessageControl.h>
#include <CvEventReporter.h>
#include <CvSelectionGroup.h>
#include <CvInfos.h>
#include <CvPlayerAI.h>
#include <CvDLLButtonPopup.h>

#include <CommonStuff/range.h>

#include <iostream>

using cvengine::Minimap;
using cvengine::getMinimapBaseTextureDim;
using cvengine::encodeDXT1;
using heck::range;

void MyCvDLLInterfaceIFace::lookAtSelectionPlot(bool bRelease)
{
	if (bRelease) // Always seems to be false.
		std::abort();
	CvInterface::getInstance().lookAtSelectionPlot();
}

bool MyCvDLLInterfaceIFace::canHandleAction(int iAction, CvPlot* pPlot, bool bTestVisible)
{
	// Yes, just call back into the DLL.
	return gGlobals.getGame().canHandleAction(iAction, pPlot, bTestVisible, false);
}

bool MyCvDLLInterfaceIFace::canDoInterfaceMode(InterfaceModeTypes eInterfaceMode, CvSelectionGroup* pSelectionGroup)
{
	if (eInterfaceMode == NO_INTERFACEMODE)
		return false;
	
	const CvInterfaceModeInfo& info = gGlobals.getInterfaceModeInfo(eInterfaceMode);
	// Checks getMissionType, which appears to be NO_MISSION for interface modes that /don't/ apply to a selection group.
	return info.getMissionType() == NO_MISSION || pSelectionGroup->canDoInterfaceMode(eInterfaceMode);
}

CvPlot* MyCvDLLInterfaceIFace::getLookAtPlot()
{
	return CvInterface::getInstance().getLookAtPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSelectionPlot()
{
	return CvInterface::getInstance().getSelectedCityOrUnitPlot();
}

CvUnit* MyCvDLLInterfaceIFace::getInterfacePlotUnit([[maybe_unused]] const CvPlot* pPlot, [[maybe_unused]] int iIndex)
{
	std::abort();
}

CvUnit* MyCvDLLInterfaceIFace::getSelectionUnit([[maybe_unused]] int iIndex)
{
	std::abort();
}

CvUnit* MyCvDLLInterfaceIFace::getHeadSelectedUnit()
{
	return CvInterface::getInstance().findSelectedUnitByIndex(0);
}

void MyCvDLLInterfaceIFace::selectUnit(CvUnit* pUnit, bool bClear, bool bToggle, bool bSound)
{
	gGlobals.getGame().selectUnit(pUnit, bClear, bToggle, bSound);
}

void MyCvDLLInterfaceIFace::selectGroup(CvUnit* pUnit, bool bShift, bool bCtrl, bool bAlt)
{
	// Probably
	gGlobals.getGame().selectGroup(pUnit, bShift, bCtrl, bAlt);
}

void MyCvDLLInterfaceIFace::selectAll([[maybe_unused]] CvPlot* pPlot)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::removeFromSelectionList(CvUnit* unit)
{
	CvInterface& interfaceController = CvInterface::getInstance();
	CvSelectionGroup& selectionGroup = interfaceController.getSelectionGroup();
	for (auto* node = selectionGroup.headUnitNode(); node; node = selectionGroup.nextUnitNode(node))
	{
		if (getUnit(node->m_data) == unit)
		{
			selectionGroup.deleteUnitNode(node);
			interfaceController.setInterfaceMode(INTERFACEMODE_SELECTION);
			return true;
		}
	}

	return false;
}



void MyCvDLLInterfaceIFace::makeSelectionListDirty()
{
	CvInterface::getInstance().makeSelectionListDirty();
}

bool MyCvDLLInterfaceIFace::mirrorsSelectionGroup()
{
	// Returns true iff the interface's selection group matches the unit's DLL-side selection group.

	CvInterface& ui = CvInterface::getInstance();
	CvUnit* unit = ui.findSelectedUnitByIndex(0);
	if (!unit)
		return false;

	CvSelectionGroup& uiGroup = ui.getSelectionGroup();
	CvSelectionGroup* const dllGroup = unit->getGroup();

	if (!dllGroup || uiGroup.getNumUnits() != dllGroup->getNumUnits())
		return false;

	std::vector<IDInfo> uiUnits;
	std::vector<IDInfo> dllUnits;
	uiUnits.reserve(uiGroup.getNumUnits());
	dllUnits.reserve(uiGroup.getNumUnits());

	for (auto* node = uiGroup.headUnitNode(); node; node = uiGroup.nextUnitNode(node))
		uiUnits.push_back(node->m_data);
	for (auto* node = dllGroup->headUnitNode(); node; node = dllGroup->nextUnitNode(node))
		dllUnits.push_back(node->m_data);

	const auto proj = [](IDInfo a) {
		return std::pair(a.eOwner, a.iID);
		};

	std::ranges::sort(uiUnits, std::less<>(), proj);
	std::ranges::sort(dllUnits, std::less<>(), proj);

	return uiUnits == dllUnits;
}

bool MyCvDLLInterfaceIFace::canSelectionListFound()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::bringToTop([[maybe_unused]] CvPopup* pPopup)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isPopupUp()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isPopupQueued()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isDiploOrPopupWaiting()
{
	return CvInterface::getInstance().hasWaitingPopupsOrDiplo();
}

CvUnit* MyCvDLLInterfaceIFace::getLastSelectedUnit()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setLastSelectedUnit([[maybe_unused]] CvUnit* pUnit)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::changePlotListColumn([[maybe_unused]] int iChange)
{
	std::abort();
}

CvPlot* MyCvDLLInterfaceIFace::getGotoPlot()
{
	return CvInterface::getInstance().getGotoPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSingleMoveGotoPlot()
{
	std::abort();
}

CvPlot* MyCvDLLInterfaceIFace::getOriginalPlot()
{
	return CvInterface::getInstance().getOriginalUnitSelectionPlot();
}

void MyCvDLLInterfaceIFace::playGeneralSound(const TCHAR* pszSound, [[maybe_unused]] NiPoint3 vPos)
{
	CvApp::getInstance().audioSystem->playSound(pszSound);
}

void MyCvDLLInterfaceIFace::playGeneralSound([[maybe_unused]] int iSoundId, [[maybe_unused]] int iSoundType, [[maybe_unused]] NiPoint3 vPos)
{
	// Not used?
	std::abort();
}

void MyCvDLLInterfaceIFace::clearQueuedPopups()
{
	//HECK_LOG_DEFAULT(Log::kWarning, "Stub implementation."sv);
}

CvSelectionGroup* MyCvDLLInterfaceIFace::getSelectionList()
{
	return &CvInterface::getInstance().getSelectionGroup();
}

void MyCvDLLInterfaceIFace::clearSelectionList()
{
	CvInterface::getInstance().getSelectionGroup().clearUnits();
}

static bool tryRemoveUnitFromSelectionGroup(CvUnit* pUnit)
{
	CvSelectionGroup& selectionGroup = CvInterface::getInstance().getSelectionGroup();

	for (auto node = selectionGroup.headUnitNode(); node; node = selectionGroup.nextUnitNode(node))
	{
		if (getUnit(node->m_data) == pUnit)
		{
			selectionGroup.deleteUnitNode(node);
			CvInterface::getInstance().makeSelectionListDirty();
			return true;
		}
	}

	return false;
}

void MyCvDLLInterfaceIFace::insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange)
{
	// Bunch of stuff goes on in here...

	if (pUnit->getGroup() && pUnit->getGroup()->isBusy())
		return;

	if (bClear)
		clearSelectionList();

	CvSelectionGroup& selectionGroup = CvInterface::getInstance().getSelectionGroup();

	bool added = false;

	if (bToggle)
	{
		if (!tryRemoveUnitFromSelectionGroup(pUnit))
			added = selectionGroup.addUnit(pUnit, bMinimalChange);
	}
	else
		added = selectionGroup.addUnit(pUnit, bMinimalChange);

	if (added)
	{
		if (bSound)
			CvInterface::getInstance().playUnitSelectionSound(pUnit);

		auto& interfaceController = CvInterface::getInstance();
		interfaceController.resetInterfaceMode();
		interfaceController.makeSelectionListDirty();

		if (bGroup)
		{
			// NOTE: If bGroup, then mirrorsSelectionGroup should be true as well, before adding, it seems.
			//       So, if bGroup, the new unit should be the only one not in the same group as the others.
			for (auto* node = selectionGroup.headUnitNode(); node; node = selectionGroup.nextUnitNode(node))
			{
				const CvUnit* const otherUnit = getUnit(node->m_data);

				if (otherUnit->getGroup() != pUnit->getGroup())
				{
					CvMessageControl::getInstance().sendJoinGroup(pUnit->getID(), otherUnit->getID());
					break;
				}
			}
		}

		CvEventReporter::getInstance().unitSelected(pUnit);
	}
}

void MyCvDLLInterfaceIFace::selectionListPostChange()
{
}

void MyCvDLLInterfaceIFace::selectionListPreChange()
{
}

int MyCvDLLInterfaceIFace::getSymbolID(int iSymbol)
{
	return CvApp::getInstance().symbols[iSymbol];
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::deleteSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return CvInterface::getInstance().getSelectionGroup().deleteUnitNode(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return CvInterface::getInstance().getSelectionGroup().nextUnitNode(pNode);
}

int MyCvDLLInterfaceIFace::getLengthSelectionList()
{
	return CvInterface::getInstance().getSelectionGroup().getNumUnits();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectionListNode()
{
	return CvInterface::getInstance().getSelectionGroup().headUnitNode();
}

void MyCvDLLInterfaceIFace::selectCity(CvCity* pNewValue, bool bTestProduction)
{
	// Okay, what happens here is that `bTestProduction` is deferred until exit city screen.
	CvInterface::getInstance().enterCityScreen(pNewValue, bTestProduction);
}

void MyCvDLLInterfaceIFace::selectLookAtCity([[maybe_unused]] bool bAdd)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::addSelectedCity(CvCity* pNewValue, bool bToggle)
{
	CvInterface::getInstance().addSelectedCity(pNewValue, bToggle);
}

void MyCvDLLInterfaceIFace::clearSelectedCities()
{
	CvInterface::getInstance().clearSelectedCities();
}

bool MyCvDLLInterfaceIFace::isCitySelected(CvCity* city)
{
	return city && std::ranges::contains(viewCLinkList(CvInterface::getInstance().getSelectedCityList()), city->getIDInfo());
}

CvCity* MyCvDLLInterfaceIFace::getHeadSelectedCity()
{
	return CvInterface::getInstance().getHeadSelectedCity();
}

bool MyCvDLLInterfaceIFace::isCitySelection()
{
	return (bool)CvInterface::getInstance().getHeadSelectedCity();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectedCitiesNode(CLLNode<IDInfo>* pNode)
{
	return CvInterface::getInstance().getSelectedCityList().next(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectedCitiesNode()
{
	return CvInterface::getInstance().getSelectedCityList().head();
}

void MyCvDLLInterfaceIFace::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
	InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY,
	bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	CvInterface::getInstance().addMessage(ePlayer, bForce, iLength, std::move(szString), pszSound,
		eType, pszIcon, eFlashColor, iFlashX, iFlashY,
		bShowOffScreenArrows, bShowOnScreenArrows
	);
}

void MyCvDLLInterfaceIFace::addCombatMessage([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] CvWString szString)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId)
{
	CvInterface::getInstance().addQuestMessage(ePlayer, szString, iQuestId);
}

void MyCvDLLInterfaceIFace::showMessage([[maybe_unused]] CvTalkingHeadMessage& msg)
{
	// This might only be called for multiplayer.
	std::abort();
}

void MyCvDLLInterfaceIFace::flushTalkingHeadMessages()
{
	// This function is called when the game-active player is no longer turn-active.
	// Maybe this function is supposed to be used to flush queued messaged onto the screen.
}

void MyCvDLLInterfaceIFace::clearEventMessages()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::addPopup(CvPopupInfo* pInfo, PlayerTypes ePlayer, bool bImmediate, bool bFront)
{
	// We own this pointer now.
	std::unique_ptr<CvPopupInfo> pInfoOwner(pInfo);

	std::wclog << L"addPopup for player " << std::to_underlying(ePlayer) << L": " << pInfo->getText() << std::endl;

	if (ePlayer == NO_PLAYER)
		ePlayer = gGlobals.getGame().getActivePlayer();

	if (ePlayer == NO_PLAYER)
		return;

	CvPlayer& player = CvPlayerAI::getPlayerNonInl(ePlayer);
	if (!player.isHuman())
		return;

	if (!bImmediate)
	{
		player.addPopup(pInfoOwner.release(), bFront);
	}
	else
	{
		// Unfortunately, ownership of this pointer passes through a raw pointer API.
		CvButtonPopup* const popup = new CvButtonPopup(std::move(pInfoOwner));
		if (!CvDLLButtonPopup::getInstance().launchButtonPopup(popup, popup->getPopupInfo()))
			delete popup;
	}
}

void MyCvDLLInterfaceIFace::getDisplayedButtonPopups(CvPopupQueue&)
{
	cvengine::logInfo("Not implemented. Used to serialise player popups.");
}

int MyCvDLLInterfaceIFace::getCycleSelectionCounter()
{
	return CvInterface::getInstance().getCycleSelectionCounter();
}

void MyCvDLLInterfaceIFace::setCycleSelectionCounter(int iNewValue)
{
	CvInterface::getInstance().setCycleSelectionCounter(iNewValue);
}

void MyCvDLLInterfaceIFace::changeCycleSelectionCounter(int iChange)
{
	CvInterface::getInstance().changeCycleSelectionCounter(iChange);
}

int MyCvDLLInterfaceIFace::getEndTurnCounter()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setEndTurnCounter([[maybe_unused]] int iNewValue)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::changeEndTurnCounter([[maybe_unused]] int iChange)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isCombatFocus()
{
	return false;
}

void MyCvDLLInterfaceIFace::setCombatFocus([[maybe_unused]] bool bNewValue)
{
	std::clog << "Ignoring " << __FUNCTION__ << std::endl;
}

void MyCvDLLInterfaceIFace::setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	// What is the difference?
	MyCvDLLUtility().beginDiplomacy(pDiploParams, ePlayer);
}

bool MyCvDLLInterfaceIFace::isDirty(InterfaceDirtyBits eDirtyItem)
{
	return CvInterface::getInstance().getInterfaceDirtyBit(eDirtyItem);
}

void MyCvDLLInterfaceIFace::setDirty(InterfaceDirtyBits eDirtyItem, bool bNewValue)
{
	CvInterface::getInstance().setInterfaceDirtyBit(eDirtyItem, bNewValue);
}

void MyCvDLLInterfaceIFace::makeInterfaceDirty()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::updateCursorType()
{
	//HECK_LOG_DEFAULT(heck::Log::kWarning, "Stub implementation. Returning false."sv);
	// NOTE: Return value isn't used anywhere.
	return false;
}

void MyCvDLLInterfaceIFace::updatePythonScreens()
{
	// Any game action will invalidate UI anyway.
	// This might possibly be useful if we had real-time game updates where not every update will invalidate UI.
	std::clog << "Ignoring call to " << __FUNCTION__ << std::endl;
}

void MyCvDLLInterfaceIFace::lookAt(NiPoint3 pt3Target, [[maybe_unused]] CameraLookAtTypes type, [[maybe_unused]] NiPoint3 attackDirection)
{
	// Reuse map functions.
	const int x = gGlobals.getMap().pointXToPlotX(pt3Target.x);
	const int y = gGlobals.getMap().pointYToPlotY(pt3Target.y);
	CvInterface::getInstance().lookAt({ x, y });
}

void MyCvDLLInterfaceIFace::centerCamera(CvUnit* unit)
{
	CvInterface::getInstance().lookAt({ unit->getX(), unit->getY() });
}

void MyCvDLLInterfaceIFace::releaseLockedCamera()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isFocusedWidget()
{
	return CvInterface::getInstance().isFocusedWidget();
}

bool MyCvDLLInterfaceIFace::isFocused()
{
	return CvInterface::getInstance().isFocused();
}

bool MyCvDLLInterfaceIFace::isBareMapMode()
{
	return false;
}

void MyCvDLLInterfaceIFace::toggleBareMapMode()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isShowYields()
{
	return false;
}

void MyCvDLLInterfaceIFace::toggleYieldVisibleMode()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isScoresVisible()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::toggleScoresVisible()
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isScoresMinimized()
{
	return CvInterface::getInstance().isScoresMinimized();
}

void MyCvDLLInterfaceIFace::toggleScoresMinimized()
{
	CvInterface::getInstance().toggleScoresMinimized();
}

bool MyCvDLLInterfaceIFace::isNetStatsVisible()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getOriginalPlotCount()
{
	return CvInterface::getInstance().getOriginalUnitSelectionIndex();
}

bool MyCvDLLInterfaceIFace::isCityScreenUp()
{
	return CvInterface::getInstance().isInCityScreen();
}

bool MyCvDLLInterfaceIFace::isEndTurnMessage()
{
	return CvInterface::getInstance().isEndTurnMessage();
}

void MyCvDLLInterfaceIFace::setInterfaceMode(InterfaceModeTypes eNewValue)
{
	CvInterface::getInstance().setInterfaceMode(eNewValue);
}

InterfaceModeTypes MyCvDLLInterfaceIFace::getInterfaceMode()
{
	return CvInterface::getInstance().getInterfaceMode();
}

InterfaceVisibility MyCvDLLInterfaceIFace::getShowInterface()
{
	return INTERFACE_SHOW;
}

CvPlot* MyCvDLLInterfaceIFace::getMouseOverPlot()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setFlashing([[maybe_unused]] PlayerTypes eWho, [[maybe_unused]] bool bFlashing)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isFlashing([[maybe_unused]] PlayerTypes eWho)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setDiplomacyLocked([[maybe_unused]] bool bLocked)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isDiplomacyLocked()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setMinimapColor([[maybe_unused]] MinimapModeTypes eMinimapMode, [[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] ColorTypes eColor, [[maybe_unused]] float fAlpha)
{
	// TODO: Minimap
	//abort();
}

unsigned char* MyCvDLLInterfaceIFace::getMinimapBaseTexture() const
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

void MyCvDLLInterfaceIFace::setEndTurnMessage(bool bNewValue)
{
	CvInterface::getInstance().setEndTurnMessage(bNewValue);
}

bool MyCvDLLInterfaceIFace::isHasMovedUnit()
{
	std::abort();
}

// Has the player manually moved a unit?
void MyCvDLLInterfaceIFace::setHasMovedUnit(bool bNewValue)
{
	mHasMovedUnit = bNewValue;
}

bool MyCvDLLInterfaceIFace::isForcePopup()
{
	return CvInterface::getInstance().isForcePopup();
}

void MyCvDLLInterfaceIFace::setForcePopup(bool bNewValue)
{
	CvInterface::getInstance().setForcePopup(bNewValue);
}

void MyCvDLLInterfaceIFace::lookAtCityOffset(int iCity)
{
	if (const CvCity* const city = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer()).getCity(iCity))
		CvInterface::getInstance().lookAt({ city->getX(), city->getY() });
}

void MyCvDLLInterfaceIFace::toggleTurnLog()
{
	CvInterface::getInstance().toggleTurnLog();
}

void MyCvDLLInterfaceIFace::showTurnLog([[maybe_unused]] ChatTargetTypes eTarget)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::dirtyTurnLog(PlayerTypes ePlayer)
{
	CvInterface::getInstance().dirtyTurnLog(ePlayer);
}

int MyCvDLLInterfaceIFace::getPlotListColumn()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::verifyPlotListColumn()
{
}

int MyCvDLLInterfaceIFace::getPlotListOffset()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::unlockPopupHelp()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::showDetails([[maybe_unused]] bool bPasswordOnly)
{
	throw std::runtime_error("Not implemented.");
}

void MyCvDLLInterfaceIFace::showAdminDetails()
{
	throw std::runtime_error("Not implemented.");
}

void MyCvDLLInterfaceIFace::toggleClockAlarm([[maybe_unused]] bool bValue, [[maybe_unused]] int iHour, [[maybe_unused]] int iMin)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isClockAlarmOn()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::setScreenDying([[maybe_unused]] int iPythonFileID, [[maybe_unused]] bool bDying)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isExitingToMainMenu()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::exitingToMainMenu(const char* szLoadFile)
{
	if (szLoadFile)
		std::abort();
	CvApp::getInstance().deferMainMenu();
}

void MyCvDLLInterfaceIFace::setWorldBuilder([[maybe_unused]] bool bTurnOn)
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontLeftJustify()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontRightJustify()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontCenterJustify()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontCenterVertically()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontAdditive()
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupSetHeaderString(CvPopup* pPopup, CvWString szText, [[maybe_unused]] unsigned int uiFlags)
{
	pPopup->headerString = std::move(szText);

	// NOTE: Flags are JustificationTypes.
}

void MyCvDLLInterfaceIFace::popupSetBodyString(CvPopup* pPopup, CvWString szText, [[maybe_unused]] unsigned int uiFlags, [[maybe_unused]] char* szName, [[maybe_unused]] CvWString szHelpText)
{
	pPopup->bodyString = std::move(szText);

	// NOTE: Are szName and szHelpText used anywhere?
}

void MyCvDLLInterfaceIFace::popupLaunch(CvPopup* pPopup, bool bCreateOkButton, PopupStates bState, [[maybe_unused]] int iNumPixelScroll)
{
	CvInterface::getInstance().launchPopup(std::unique_ptr<CvPopup>(pPopup), bCreateOkButton, bState);
}

void MyCvDLLInterfaceIFace::popupSetPopupType(CvPopup* pPopup, PopupEventTypes ePopupType, [[maybe_unused]] const TCHAR* szArtFileName)
{
	pPopup->eventType = ePopupType;
}

void MyCvDLLInterfaceIFace::popupSetStyle([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] const char* styleId)
{
	// Used only for the main menu it seems.
	//std::abort();
}

void MyCvDLLInterfaceIFace::popupAddDDS([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] const char* szIconFilename, [[maybe_unused]] int iWidth, [[maybe_unused]] int iHeight, [[maybe_unused]] CvWString szHelpText)
{
	//std::abort();
}

void MyCvDLLInterfaceIFace::popupAddSeparator(CvPopup* pPopup, [[maybe_unused]] int iSpace)
{
	pPopup->controls.push_back(CvPopup::Control{
		.type = CvPopup::EControlType::Separator,
		});
}

void MyCvDLLInterfaceIFace::popupAddGenericButton(CvPopup* pPopup, CvWString szText, [[maybe_unused]] const char* szIcon, int iButtonId, WidgetTypes eWidgetType, int iData1, int iData2, bool bOption, [[maybe_unused]] PopupControlLayout ctrlLayout, [[maybe_unused]] unsigned int textJustifcation)
{
	pPopup->controls.push_back(CvPopup::Control{
		.type = CvPopup::EControlType::Button,
		.enabled = bOption,
		.text = szText,
		.iGroup = iButtonId,
		.widgetData{.m_iData1 = iData1, .m_iData2 = iData2, .m_bOption = false, .m_eWidgetType = eWidgetType }, // Always false?
		});
}

void MyCvDLLInterfaceIFace::popupCreateEditBox([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] CvWString szDefaultString, [[maybe_unused]] WidgetTypes eWidgetType, [[maybe_unused]] CvWString szHelpText, [[maybe_unused]] int iGroup, [[maybe_unused]] PopupControlLayout ctrlLayout, [[maybe_unused]] unsigned int preferredCharWidth, [[maybe_unused]] unsigned int maxCharCount)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupEnableEditBox([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iGroup, [[maybe_unused]] bool bEnable)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupCreateRadioButtons([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iNumButtons, [[maybe_unused]] int iGroup, [[maybe_unused]] WidgetTypes eWidgetType, [[maybe_unused]] PopupControlLayout ctrlLayout)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupSetRadioButtonText([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iRadioButtonID, [[maybe_unused]] CvWString szText, [[maybe_unused]] int iGroup, [[maybe_unused]] CvWString szHelpText)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupCreateCheckBoxes([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iNumBoxes, [[maybe_unused]] int iGroup, [[maybe_unused]] WidgetTypes eWidgetType, [[maybe_unused]] PopupControlLayout ctrlLayout)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupSetCheckBoxText([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iCheckBoxID, [[maybe_unused]] CvWString szText, [[maybe_unused]] int iGroup, [[maybe_unused]] CvWString szHelpText)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupSetCheckBoxState([[maybe_unused]] CvPopup* pPopup, [[maybe_unused]] int iCheckBoxID, [[maybe_unused]] bool bChecked, [[maybe_unused]] int iGroup)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::popupSetAsCancelled(CvPopup* pPopup)
{
	pPopup->window->setWantClose();
}

bool MyCvDLLInterfaceIFace::popupIsDying(CvPopup* pPopup)
{
	// I guess...
	return pPopup->window && pPopup->window->wantClose(nullptr);
}

void MyCvDLLInterfaceIFace::setCityTabSelectionRow(CityTabTypes)
{
	std::clog << "Ignoring call to " << __FUNCTION__ << std::endl;
}

bool MyCvDLLInterfaceIFace::noTechSplash()
{
	return false;
}

bool MyCvDLLInterfaceIFace::isInAdvancedStart() const
{
	return false;
}

void MyCvDLLInterfaceIFace::setInAdvancedStart([[maybe_unused]] bool bAdvancedStart)
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isSpaceshipScreenUp() const
{
	std::abort();
}

bool MyCvDLLInterfaceIFace::isDebugMenuCreated() const
{
	return false;
}

void MyCvDLLInterfaceIFace::setBusy([[maybe_unused]] bool bBusy)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::getInterfaceScreenIdsForInput([[maybe_unused]] std::vector<int>& aIds)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::doPing([[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}
