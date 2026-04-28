#include "MyCvDLLInterfaceIFace.h"
#include "MyCvDLLUtility.h"
#include "inc/Cv4CommonEngineLib/CvInterface.h"
#include "inc/Cv4CommonEngineLib/CvTranslator.h"
#include "inc/Cv4CommonEngineLib/CvButtonPopup.h"
#include "inc/Cv4CommonEngineLib/AudioXmlDefs.h"
#include "CommonEngineGlobal.h"

#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvGameCoreUtils.h>
#include <CvGameCoreDLL/CvDLLButtonPopup.h>
#include <CvGameCoreDLL/CvPlayerAI.h>

#include <CommonStuff/range.h>

#include <iostream>

using heck::range;
using cvengine::gCommonEngineConfig;

MyCvDLLInterfaceIFace& MyCvDLLInterfaceIFace::getInstance()
{
	static MyCvDLLInterfaceIFace s;
	return s;
}

void MyCvDLLInterfaceIFace::lookAtSelectionPlot(bool bRelease)
{
	if (bRelease) // Always seems to be false.
		std::abort();
	gCommonEngineConfig.interface->lookAtSelectionPlot();
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
	return gCommonEngineConfig.interface->getLookAtPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSelectionPlot()
{
	return gCommonEngineConfig.interface->getSelectedCityOrUnitPlot();
}

CvUnit* MyCvDLLInterfaceIFace::getInterfacePlotUnit(const CvPlot* pPlot, int iIndex)
{
	// Used by main interface. Is this all it does?
	if (pPlot)
	{
		CvUnit* const unit = pPlot->getUnitByIndex(iIndex);
		return unit && CvInterface::isActiveVisibleUnit(*unit) ? unit : nullptr;
	}
	else
		return nullptr;
}

CvUnit* MyCvDLLInterfaceIFace::getSelectionUnit(int iIndex)
{ 
	return gCommonEngineConfig.interface->getSelectionUnit(iIndex);
}

CvUnit* MyCvDLLInterfaceIFace::getHeadSelectedUnit()
{
	return getSelectionUnit(0);
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

void MyCvDLLInterfaceIFace::selectAll(CvPlot* pPlot)
{
	// Probably
	gGlobals.getGame().selectAll(pPlot);
}

bool MyCvDLLInterfaceIFace::removeFromSelectionList(CvUnit* unit)
{
	return gCommonEngineConfig.interface->removeFromSelectionList(unit);
}



void MyCvDLLInterfaceIFace::makeSelectionListDirty()
{
	gCommonEngineConfig.interface->makeSelectionListDirty();
}

bool MyCvDLLInterfaceIFace::mirrorsSelectionGroup()
{
	return gCommonEngineConfig.interface->mirrorsSelectionGroup();
}

bool MyCvDLLInterfaceIFace::canSelectionListFound()
{
	// Used in CvGame::updateColoredPlots()
	CvSelectionGroup& group = *gCommonEngineConfig.interface->getSelectionList();
	// TODO: Correct coord?
	return group.canStartMission(MISSION_FOUND, -1, -1);
}

void MyCvDLLInterfaceIFace::bringToTop([[maybe_unused]] CvPopup* pPopup)
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isPopupUp()
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isPopupQueued()
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isDiploOrPopupWaiting()
{
	return gCommonEngineConfig.interface->isDiploOrPopupWaiting();
}

CvUnit* MyCvDLLInterfaceIFace::getLastSelectedUnit()
{
	return gCommonEngineConfig.interface->getLastSelectedUnit();
}

void MyCvDLLInterfaceIFace::setLastSelectedUnit(CvUnit* pUnit)
{
	gCommonEngineConfig.interface->setLastSelectedUnit(pUnit);
}

void MyCvDLLInterfaceIFace::changePlotListColumn([[maybe_unused]] int iChange)
{
	gCommonEngineConfig.interface->changePlotListColumn(iChange);
}

CvPlot* MyCvDLLInterfaceIFace::getGotoPlot()
{
	return gCommonEngineConfig.interface->getGotoPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSingleMoveGotoPlot()
{
	return gCommonEngineConfig.interface->getSingleMoveGotoPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getOriginalPlot()
{
	return gCommonEngineConfig.interface->getOriginalUnitSelectionPlot();
}

void MyCvDLLInterfaceIFace::playGeneralSound(const TCHAR* pszSound, NiPoint3 vPos)
{
	if (!pszSound || !pszSound[0])
		return;

	const cvengine::AudioXmlDefs& xmlDefs = cvengine::AudioXmlDefs::getInstance();
	const auto it = xmlDefs.tagIndexLookup.find(pszSound);
	if (it->second.type == xmlDefs.scriptType2D)
		gCommonEngineConfig.interface->play2DSound(it->second.index);
	else if (it->second.type == xmlDefs.scriptType3D)
		gCommonEngineConfig.interface->play3DSound(it->second.index, vPos);
	else if (it == xmlDefs.tagIndexLookup.end())
		std::clog << "Sound '" << pszSound << "' does not exist.\n";
	else
		throw std::runtime_error(std::string("Unexpected sound type in ") + __FUNCTION__ + '.');
}

void MyCvDLLInterfaceIFace::playGeneralSound([[maybe_unused]] int iSoundId, [[maybe_unused]] int iSoundType, [[maybe_unused]] NiPoint3 vPos)
{
	// Not used?
	std::abort();
	//gCommonEngineConfig.interface->playGeneralSound(iSoundId, iSoundType, vPos);
}

void MyCvDLLInterfaceIFace::clearQueuedPopups()
{
	gCommonEngineConfig.interface->clearQueuedPopups();
}

CvSelectionGroup* MyCvDLLInterfaceIFace::getSelectionList()
{
	return gCommonEngineConfig.interface->getSelectionList();
}

void MyCvDLLInterfaceIFace::clearSelectionList()
{
	gCommonEngineConfig.interface->clearSelectionList();
}

void MyCvDLLInterfaceIFace::insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange)
{
	gCommonEngineConfig.interface->insertIntoSelectionList(pUnit, bClear, bToggle, bGroup, bSound, bMinimalChange);
}

void MyCvDLLInterfaceIFace::selectionListPostChange()
{
	gCommonEngineConfig.interface->selectionListPostChange();
}

void MyCvDLLInterfaceIFace::selectionListPreChange()
{
	gCommonEngineConfig.interface->selectionListPreChange();
}

int MyCvDLLInterfaceIFace::getSymbolID(int iSymbol)
{
	return CvTranslator::getInstance().symbols[iSymbol];
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::deleteSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return gCommonEngineConfig.interface->getSelectionList()->deleteUnitNode(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return gCommonEngineConfig.interface->getSelectionList()->nextUnitNode(pNode);
}

int MyCvDLLInterfaceIFace::getLengthSelectionList()
{
	return gCommonEngineConfig.interface->getNumSelectedUnits();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectionListNode()
{
	return gCommonEngineConfig.interface->getSelectionList()->headUnitNode();
}

void MyCvDLLInterfaceIFace::selectCity(CvCity* pNewValue, bool bTestProduction)
{
	// Okay, what happens here is that `bTestProduction` is deferred until exit city screen.
	//CvInterface::getInstance().enterCityScreen(pNewValue, bTestProduction);
	return gCommonEngineConfig.interface->selectCity(pNewValue, bTestProduction);
}

void MyCvDLLInterfaceIFace::selectLookAtCity(bool bAdd)
{
	gCommonEngineConfig.interface->selectLookAtCity(bAdd);
}

void MyCvDLLInterfaceIFace::addSelectedCity(CvCity* pNewValue, bool bToggle)
{
	gCommonEngineConfig.interface->addSelectedCity(pNewValue, bToggle);
}

void MyCvDLLInterfaceIFace::clearSelectedCities()
{
	gCommonEngineConfig.interface->clearSelectedCities();
}

bool MyCvDLLInterfaceIFace::isCitySelected(CvCity* city)
{
	return gCommonEngineConfig.interface->isCitySelected(city);
}

CvCity* MyCvDLLInterfaceIFace::getHeadSelectedCity()
{
	return gCommonEngineConfig.interface->getHeadSelectedCity();
}

bool MyCvDLLInterfaceIFace::isCitySelection()
{
	return !!getHeadSelectedCity();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectedCitiesNode(CLLNode<IDInfo>* pNode)
{
	return gCommonEngineConfig.interface->nextSelectedCitiesNode(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectedCitiesNode()
{
	return gCommonEngineConfig.interface->headSelectedCitiesNode();
}

void MyCvDLLInterfaceIFace::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
	InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY,
	bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	gCommonEngineConfig.interface->addMessage(ePlayer, bForce, iLength, std::move(szString), pszSound,
		eType, pszIcon, eFlashColor, iFlashX, iFlashY,
		bShowOffScreenArrows, bShowOnScreenArrows
	);
}

void MyCvDLLInterfaceIFace::addCombatMessage([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] CvWString szString)
{
	// Not used?
	std::abort();
}

void MyCvDLLInterfaceIFace::addQuestMessage(PlayerTypes ePlayer, CvWString szString, int iQuestId)
{
	gCommonEngineConfig.interface->addQuestMessage(ePlayer, szString, iQuestId);
}

void MyCvDLLInterfaceIFace::showMessage([[maybe_unused]] CvTalkingHeadMessage& msg)
{
	// This might only be called for multiplayer.
	std::abort();
}

void MyCvDLLInterfaceIFace::flushTalkingHeadMessages()
{
	gCommonEngineConfig.interface->flushTalkingHeadMessages();
}

void MyCvDLLInterfaceIFace::clearEventMessages()
{
	// This might only be called for multiplayer.
	std::abort();
}

void MyCvDLLInterfaceIFace::addPopup(CvPopupInfo* pInfo, PlayerTypes ePlayer, bool bImmediate, bool bFront)
{
	// We own this pointer now.
	std::unique_ptr<CvPopupInfo> pInfoOwner(pInfo);

	std::wclog << L"addPopup for player " << std::to_underlying(ePlayer) << L": " << pInfo->getText() << std::endl;

	if (ePlayer == NO_PLAYER)
		ePlayer = gGlobals.getGame().getActivePlayer();

	if (ePlayer != NO_PLAYER)
	{
		CvPlayer& player = CvPlayerAI::getPlayerNonInl(ePlayer);
		if (player.isHuman())
		{
			if (bImmediate)
			{
				//gCommonEngineConfig.interface->createAndLaunchButtonPopup(std::move(pInfoOwner));
				// Unfortunately, ownership of this pointer passes through a raw pointer API.
				CvButtonPopup* const popup = new CvButtonPopup(std::move(pInfoOwner));
				if (!CvDLLButtonPopup::getInstance().launchButtonPopup(popup, popup->getPopupInfo()))
					delete popup;
			}
			else
				player.addPopup(pInfoOwner.release(), bFront);
		}
	}
}

void MyCvDLLInterfaceIFace::getDisplayedButtonPopups(CvPopupQueue& out)
{
	gCommonEngineConfig.interface->getDisplayedButtonPopups(out);
}

int MyCvDLLInterfaceIFace::getCycleSelectionCounter()
{
	return gCommonEngineConfig.interface->getCycleSelectionCounter();
}

void MyCvDLLInterfaceIFace::setCycleSelectionCounter(int iNewValue)
{
	return gCommonEngineConfig.interface->setCycleSelectionCounter(iNewValue);
}

void MyCvDLLInterfaceIFace::changeCycleSelectionCounter(int iChange)
{
	return gCommonEngineConfig.interface->changeCycleSelectionCounter(iChange);
}

int MyCvDLLInterfaceIFace::getEndTurnCounter()
{
	return gCommonEngineConfig.interface->getEndTurnCounter();
}

void MyCvDLLInterfaceIFace::setEndTurnCounter(int iNewValue)
{
	return gCommonEngineConfig.interface->setEndTurnCounter(iNewValue);
}

void MyCvDLLInterfaceIFace::changeEndTurnCounter(int iChange)
{
	return gCommonEngineConfig.interface->changeEndTurnCounter(iChange);
}

bool MyCvDLLInterfaceIFace::isCombatFocus()
{
	return gCommonEngineConfig.interface->isCombatFocus();
}

void MyCvDLLInterfaceIFace::setCombatFocus(bool bNewValue)
{
	//std::clog << "Ignoring " << __FUNCTION__ << std::endl;
	return gCommonEngineConfig.interface->setCombatFocus(bNewValue);
}

void MyCvDLLInterfaceIFace::setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	// What is the difference?
	MyCvDLLUtility::getInstance().beginDiplomacy(pDiploParams, ePlayer);
}

bool MyCvDLLInterfaceIFace::isDirty(InterfaceDirtyBits eDirtyItem)
{
	return gCommonEngineConfig.interface->getInterfaceDirtyBit(eDirtyItem);
}

void MyCvDLLInterfaceIFace::setDirty(InterfaceDirtyBits eDirtyItem, bool bNewValue)
{
	gCommonEngineConfig.interface->setInterfaceDirtyBit(eDirtyItem, bNewValue);
}

void MyCvDLLInterfaceIFace::makeInterfaceDirty()
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::updateCursorType()
{
	//HECK_LOG_DEFAULT(heck::Log::kWarning, "Stub implementation. Returning false."sv);
	// NOTE: Return value isn't used anywhere.
	gCommonEngineConfig.interface->updateCursorType();
	return false;
}

void MyCvDLLInterfaceIFace::updatePythonScreens()
{
	// Any game action will invalidate UI anyway.
	// This might possibly be useful if we had real-time game updates where not every update will invalidate UI.
	//std::clog << "Ignoring call to " << __FUNCTION__ << std::endl;
	gCommonEngineConfig.interface->updatePythonScreens();
}

void MyCvDLLInterfaceIFace::lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection)
{
	// Reuse map functions.
	//const int x = gGlobals.getMap().pointXToPlotX(pt3Target.x);
	//const int y = gGlobals.getMap().pointYToPlotY(pt3Target.y);
	gCommonEngineConfig.interface->lookAt(pt3Target, type, attackDirection);
}

void MyCvDLLInterfaceIFace::centerCamera(CvUnit* unit)
{
	gCommonEngineConfig.interface->lookAtPlot({ unit->getX(), unit->getY() });
}

void MyCvDLLInterfaceIFace::releaseLockedCamera()
{
	gCommonEngineConfig.interface->releaseLockedCamera();
}

bool MyCvDLLInterfaceIFace::isFocusedWidget()
{
	return gCommonEngineConfig.interface->isFocusedWidget();
}

bool MyCvDLLInterfaceIFace::isFocused()
{
	return gCommonEngineConfig.interface->isFocused();
}

bool MyCvDLLInterfaceIFace::isBareMapMode()
{
	return gCommonEngineConfig.interface->isBareMapMode();
}

void MyCvDLLInterfaceIFace::toggleBareMapMode()
{
	return gCommonEngineConfig.interface->toggleBareMapMode();
}

bool MyCvDLLInterfaceIFace::isShowYields()
{
	return gCommonEngineConfig.interface->isShowYields();
}

void MyCvDLLInterfaceIFace::toggleYieldVisibleMode()
{
	return gCommonEngineConfig.interface->toggleYieldVisibleMode();
}

bool MyCvDLLInterfaceIFace::isScoresVisible()
{
	return gCommonEngineConfig.interface->isScoresVisible();
}

void MyCvDLLInterfaceIFace::toggleScoresVisible()
{
	return gCommonEngineConfig.interface->toggleScoresVisible();
}

bool MyCvDLLInterfaceIFace::isScoresMinimized()
{
	return gCommonEngineConfig.interface->isScoresMinimized();
}

void MyCvDLLInterfaceIFace::toggleScoresMinimized()
{
	return gCommonEngineConfig.interface->toggleScoresMinimized();
}

bool MyCvDLLInterfaceIFace::isNetStatsVisible()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getOriginalPlotCount()
{
	return gCommonEngineConfig.interface->getOriginalUnitSelectionIndex();
}

bool MyCvDLLInterfaceIFace::isCityScreenUp()
{
	return gCommonEngineConfig.interface->isCityScreenUp();
}

bool MyCvDLLInterfaceIFace::isEndTurnMessage()
{
	return gCommonEngineConfig.interface->isEndTurnMessage();
}

void MyCvDLLInterfaceIFace::setInterfaceMode(InterfaceModeTypes eNewValue)
{
	return gCommonEngineConfig.interface->setInterfaceMode(eNewValue);
}

InterfaceModeTypes MyCvDLLInterfaceIFace::getInterfaceMode()
{
	return gCommonEngineConfig.interface->getInterfaceMode();
}

InterfaceVisibility MyCvDLLInterfaceIFace::getShowInterface()
{
	return gCommonEngineConfig.interface->getShowInterface();
}

CvPlot* MyCvDLLInterfaceIFace::getMouseOverPlot()
{
	return gCommonEngineConfig.interface->getMouseOverPlot();
}

void MyCvDLLInterfaceIFace::setFlashing([[maybe_unused]] PlayerTypes eWho, [[maybe_unused]] bool bFlashing)
{
	// Multiplayer?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isFlashing([[maybe_unused]] PlayerTypes eWho)
{
	// Multiplayer?
	std::abort();
}

void MyCvDLLInterfaceIFace::setDiplomacyLocked([[maybe_unused]] bool bLocked)
{
	// Multiplayer?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isDiplomacyLocked()
{
	// Multiplayer?
	std::abort();
}

void MyCvDLLInterfaceIFace::setMinimapColor(MinimapModeTypes eMinimapMode, int iX, int iY, ColorTypes eColor, float fAlpha)
{
	// TODO: Minimap
	//abort();
	return gCommonEngineConfig.interface->setMinimapColor(eMinimapMode, { iX, iY }, eColor, fAlpha);
}

unsigned char* MyCvDLLInterfaceIFace::getMinimapBaseTexture() const
{
	return gCommonEngineConfig.interface->getMinimapBaseTextureDXT1();
}

void MyCvDLLInterfaceIFace::setEndTurnMessage(bool bNewValue)
{
	gCommonEngineConfig.interface->setEndTurnMessage(bNewValue);
}

bool MyCvDLLInterfaceIFace::isHasMovedUnit()
{
	return gCommonEngineConfig.interface->isHasMovedUnit();
}

// Has the player manually moved a unit?
void MyCvDLLInterfaceIFace::setHasMovedUnit(bool bNewValue)
{
	return gCommonEngineConfig.interface->setHasMovedUnit(bNewValue);
}

bool MyCvDLLInterfaceIFace::isForcePopup()
{
	return gCommonEngineConfig.interface->isForcePopup();
}

void MyCvDLLInterfaceIFace::setForcePopup(bool bNewValue)
{
	gCommonEngineConfig.interface->setForcePopup(bNewValue);
}

void MyCvDLLInterfaceIFace::lookAtCityOffset(int iCity)
{
	if (const CvCity* const city = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer()).getCity(iCity))
		gCommonEngineConfig.interface->lookAtPlot({ city->getX(), city->getY() });
}

void MyCvDLLInterfaceIFace::toggleTurnLog()
{
	gCommonEngineConfig.interface->toggleTurnLog();
}

void MyCvDLLInterfaceIFace::showTurnLog([[maybe_unused]] ChatTargetTypes eTarget)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::dirtyTurnLog(PlayerTypes ePlayer)
{
	gCommonEngineConfig.interface->dirtyTurnLog(ePlayer);
}

int MyCvDLLInterfaceIFace::getPlotListColumn()
{
	return gCommonEngineConfig.interface->getPlotListColumn();
}

void MyCvDLLInterfaceIFace::verifyPlotListColumn()
{
	return gCommonEngineConfig.interface->verifyPlotListColumn();
}

int MyCvDLLInterfaceIFace::getPlotListOffset()
{
	return gCommonEngineConfig.interface->getPlotListOffset();
}

void MyCvDLLInterfaceIFace::unlockPopupHelp()
{
	// Not used?
	std::abort();
}

void MyCvDLLInterfaceIFace::showDetails(bool bPasswordOnly)
{
	//throw std::runtime_error("Not implemented.");
	return gCommonEngineConfig.interface->showDetails(bPasswordOnly);
}

void MyCvDLLInterfaceIFace::showAdminDetails()
{
	//throw std::runtime_error("Not implemented.");
	return gCommonEngineConfig.interface->showAdminDetails();
}

void MyCvDLLInterfaceIFace::toggleClockAlarm(bool bValue, int iHour, int iMin)
{
	return gCommonEngineConfig.interface->toggleClockAlarm(bValue, iHour, iMin);
}

bool MyCvDLLInterfaceIFace::isClockAlarmOn()
{
	return gCommonEngineConfig.interface->isClockAlarmOn();
}

void MyCvDLLInterfaceIFace::setScreenDying([[maybe_unused]] int iPythonFileID, [[maybe_unused]] bool bDying)
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isExitingToMainMenu()
{
	return gCommonEngineConfig.interface->isExitingToMainMenu();
}

void MyCvDLLInterfaceIFace::exitingToMainMenu(const char* szLoadFile)
{
	// Always null?
	if (szLoadFile)
		std::abort();
	return gCommonEngineConfig.interface->exitToMainMenu();
}

void MyCvDLLInterfaceIFace::setWorldBuilder(bool bTurnOn)
{
	return gCommonEngineConfig.interface->setWorldBuilder(bTurnOn);
}

int MyCvDLLInterfaceIFace::getFontLeftJustify()
{
	// Not used?
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontRightJustify()
{
	// Not used?
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontCenterJustify()
{
	// Not used?
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontCenterVertically()
{
	// Not used?
	std::abort();
}

int MyCvDLLInterfaceIFace::getFontAdditive()
{
	// Not used?
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

void MyCvDLLInterfaceIFace::popupLaunch(CvPopup* pPopup, bool bCreateOkButton, PopupStates bState, int iNumPixelScroll)
{
	return gCommonEngineConfig.interface->launchPopup(std::unique_ptr<CvPopup>(pPopup), bCreateOkButton, bState, iNumPixelScroll);
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
	return pPopup->window && pPopup->window->isWantClose();
}

void MyCvDLLInterfaceIFace::setCityTabSelectionRow(CityTabTypes t)
{
	return gCommonEngineConfig.interface->setCityTabSelectionRow(t);
}

bool MyCvDLLInterfaceIFace::noTechSplash()
{
	return gCommonEngineConfig.interface->noTechSplash();
}

bool MyCvDLLInterfaceIFace::isInAdvancedStart() const
{
	return gCommonEngineConfig.interface->isInAdvancedStart();
}

void MyCvDLLInterfaceIFace::setInAdvancedStart(bool bAdvancedStart)
{
	return gCommonEngineConfig.interface->setInAdvancedStart(bAdvancedStart);
}

bool MyCvDLLInterfaceIFace::isSpaceshipScreenUp() const
{
	return gCommonEngineConfig.interface->isSpaceshipScreenUp();
}

bool MyCvDLLInterfaceIFace::isDebugMenuCreated() const
{
	return false;
}

void MyCvDLLInterfaceIFace::setBusy(bool bBusy)
{
	return gCommonEngineConfig.interface->setBusy(bBusy);
}

void MyCvDLLInterfaceIFace::getInterfaceScreenIdsForInput([[maybe_unused]] std::vector<int>& aIds)
{
	// Used on mouse events? Is this needed for anything?
	std::abort();
}

void MyCvDLLInterfaceIFace::doPing([[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}
