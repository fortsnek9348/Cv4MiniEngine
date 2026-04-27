#include "MyCvDLLInterfaceIFace.h"
#include "MyCvDLLUtility.h"
#include "inc/Cv4CommonEngineLib/EngineSpecificsHeader.h"
#include "inc/Cv4CommonEngineLib/CvInterface.h"
#include "inc/Cv4CommonEngineLib/CvTranslator.h"
#include "inc/Cv4CommonEngineLib/CvButtonPopup.h"
#include "inc/Cv4CommonEngineLib/AudioXmlDefs.h"

#include <CvGameAI.h>
#include <CvGlobals.h>
#include <CvInfos.h>
#include <CvGameCoreUtils.h>
#include <CvDLLButtonPopup.h>
#include <CvPlayerAI.h>

#include <CommonStuff/range.h>

#include <iostream>

using heck::range;
using cvengine::engine_specific::getCvInterface;

MyCvDLLInterfaceIFace& MyCvDLLInterfaceIFace::getInstance()
{
	static MyCvDLLInterfaceIFace s;
	return s;
}

void MyCvDLLInterfaceIFace::lookAtSelectionPlot(bool bRelease)
{
	if (bRelease) // Always seems to be false.
		std::abort();
	cvengine::engine_specific::getCvInterface().lookAtSelectionPlot();
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
	return getCvInterface().getLookAtPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSelectionPlot()
{
	return getCvInterface().getSelectedCityOrUnitPlot();
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
	return getCvInterface().getSelectionUnit(iIndex);
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
	return getCvInterface().removeFromSelectionList(unit);
}



void MyCvDLLInterfaceIFace::makeSelectionListDirty()
{
	getCvInterface().makeSelectionListDirty();
}

bool MyCvDLLInterfaceIFace::mirrorsSelectionGroup()
{
	return getCvInterface().mirrorsSelectionGroup();
}

bool MyCvDLLInterfaceIFace::canSelectionListFound()
{
	// Used in CvGame::updateColoredPlots()
	CvSelectionGroup& group = *getCvInterface().getSelectionList();
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
	return getCvInterface().isDiploOrPopupWaiting();
}

CvUnit* MyCvDLLInterfaceIFace::getLastSelectedUnit()
{
	return getCvInterface().getLastSelectedUnit();
}

void MyCvDLLInterfaceIFace::setLastSelectedUnit(CvUnit* pUnit)
{
	getCvInterface().setLastSelectedUnit(pUnit);
}

void MyCvDLLInterfaceIFace::changePlotListColumn([[maybe_unused]] int iChange)
{
	getCvInterface().changePlotListColumn(iChange);
}

CvPlot* MyCvDLLInterfaceIFace::getGotoPlot()
{
	return getCvInterface().getGotoPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getSingleMoveGotoPlot()
{
	return getCvInterface().getSingleMoveGotoPlot();
}

CvPlot* MyCvDLLInterfaceIFace::getOriginalPlot()
{
	return getCvInterface().getOriginalUnitSelectionPlot();
}

void MyCvDLLInterfaceIFace::playGeneralSound(const TCHAR* pszSound, NiPoint3 vPos)
{
	const cvengine::AudioXmlDefs& xmlDefs = cvengine::AudioXmlDefs::getInstance();
	const auto it = xmlDefs.tagIndexLookup.find(pszSound);
	if (it->second.type == xmlDefs.scriptType2D)
		getCvInterface().play2DSound(it->second.index);
	else if (it->second.type == xmlDefs.scriptType3D)
		getCvInterface().play3DSound(it->second.index, vPos);
	else if (it == xmlDefs.tagIndexLookup.end())
		std::clog << "Sound '" << pszSound << "' does not exist.\n";
	else
		throw std::runtime_error(std::string("Unexpected sound type in ") + __FUNCTION__ + '.');
}

void MyCvDLLInterfaceIFace::playGeneralSound([[maybe_unused]] int iSoundId, [[maybe_unused]] int iSoundType, [[maybe_unused]] NiPoint3 vPos)
{
	// Not used?
	std::abort();
	//getCvInterface().playGeneralSound(iSoundId, iSoundType, vPos);
}

void MyCvDLLInterfaceIFace::clearQueuedPopups()
{
	getCvInterface().clearQueuedPopups();
}

CvSelectionGroup* MyCvDLLInterfaceIFace::getSelectionList()
{
	return getCvInterface().getSelectionList();
}

void MyCvDLLInterfaceIFace::clearSelectionList()
{
	getCvInterface().clearSelectionList();
}

void MyCvDLLInterfaceIFace::insertIntoSelectionList(CvUnit* pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound, bool bMinimalChange)
{
	getCvInterface().insertIntoSelectionList(pUnit, bClear, bToggle, bGroup, bSound, bMinimalChange);
}

void MyCvDLLInterfaceIFace::selectionListPostChange()
{
	getCvInterface().selectionListPostChange();
}

void MyCvDLLInterfaceIFace::selectionListPreChange()
{
	getCvInterface().selectionListPreChange();
}

int MyCvDLLInterfaceIFace::getSymbolID(int iSymbol)
{
	return CvTranslator::getInstance().symbols[iSymbol];
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::deleteSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return getCvInterface().getSelectionList()->deleteUnitNode(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectionListNode(CLLNode<IDInfo>* pNode)
{
	return getCvInterface().getSelectionList()->nextUnitNode(pNode);
}

int MyCvDLLInterfaceIFace::getLengthSelectionList()
{
	return getCvInterface().getSelectionList()->getNumUnits();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectionListNode()
{
	return getCvInterface().getSelectionList()->headUnitNode();
}

void MyCvDLLInterfaceIFace::selectCity(CvCity* pNewValue, bool bTestProduction)
{
	// Okay, what happens here is that `bTestProduction` is deferred until exit city screen.
	//CvInterface::getInstance().enterCityScreen(pNewValue, bTestProduction);
	return getCvInterface().selectCity(pNewValue, bTestProduction);
}

void MyCvDLLInterfaceIFace::selectLookAtCity(bool bAdd)
{
	getCvInterface().selectLookAtCity(bAdd);
}

void MyCvDLLInterfaceIFace::addSelectedCity(CvCity* pNewValue, bool bToggle)
{
	getCvInterface().addSelectedCity(pNewValue, bToggle);
}

void MyCvDLLInterfaceIFace::clearSelectedCities()
{
	getCvInterface().clearSelectedCities();
}

bool MyCvDLLInterfaceIFace::isCitySelected(CvCity* city)
{
	return getCvInterface().isCitySelected(city);
}

CvCity* MyCvDLLInterfaceIFace::getHeadSelectedCity()
{
	return getCvInterface().getHeadSelectedCity();
}

bool MyCvDLLInterfaceIFace::isCitySelection()
{
	return !!getHeadSelectedCity();
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::nextSelectedCitiesNode(CLLNode<IDInfo>* pNode)
{
	return getCvInterface().nextSelectedCitiesNode(pNode);
}

CLLNode<IDInfo>* MyCvDLLInterfaceIFace::headSelectedCitiesNode()
{
	return getCvInterface().headSelectedCitiesNode();
}

void MyCvDLLInterfaceIFace::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, CvWString szString, const TCHAR* pszSound,
	InterfaceMessageTypes eType, const char* pszIcon, ColorTypes eFlashColor, int iFlashX, int iFlashY,
	bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	getCvInterface().addMessage(ePlayer, bForce, iLength, std::move(szString), pszSound,
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
	getCvInterface().addQuestMessage(ePlayer, szString, iQuestId);
}

void MyCvDLLInterfaceIFace::showMessage([[maybe_unused]] CvTalkingHeadMessage& msg)
{
	// This might only be called for multiplayer.
	std::abort();
}

void MyCvDLLInterfaceIFace::flushTalkingHeadMessages()
{
	getCvInterface().flushTalkingHeadMessages();
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
				//getCvInterface().createAndLaunchButtonPopup(std::move(pInfoOwner));
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

void MyCvDLLInterfaceIFace::getDisplayedButtonPopups(CvPopupQueue&)
{
	std::clog << __func__ << " not implemented. Used to serialise player popups.\n";
}

int MyCvDLLInterfaceIFace::getCycleSelectionCounter()
{
	return getCvInterface().getCycleSelectionCounter();
}

void MyCvDLLInterfaceIFace::setCycleSelectionCounter(int iNewValue)
{
	return getCvInterface().setCycleSelectionCounter(iNewValue);
}

void MyCvDLLInterfaceIFace::changeCycleSelectionCounter(int iChange)
{
	return getCvInterface().changeCycleSelectionCounter(iChange);
}

int MyCvDLLInterfaceIFace::getEndTurnCounter()
{
	return getCvInterface().getEndTurnCounter();
}

void MyCvDLLInterfaceIFace::setEndTurnCounter(int iNewValue)
{
	return getCvInterface().setEndTurnCounter(iNewValue);
}

void MyCvDLLInterfaceIFace::changeEndTurnCounter(int iChange)
{
	return getCvInterface().changeEndTurnCounter(iChange);
}

bool MyCvDLLInterfaceIFace::isCombatFocus()
{
	return getCvInterface().isCombatFocus();
}

void MyCvDLLInterfaceIFace::setCombatFocus(bool bNewValue)
{
	//std::clog << "Ignoring " << __FUNCTION__ << std::endl;
	return getCvInterface().setCombatFocus(bNewValue);
}

void MyCvDLLInterfaceIFace::setDiploQueue(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	// What is the difference?
	MyCvDLLUtility::getInstance().beginDiplomacy(pDiploParams, ePlayer);
}

bool MyCvDLLInterfaceIFace::isDirty(InterfaceDirtyBits eDirtyItem)
{
	return getCvInterface().getInterfaceDirtyBit(eDirtyItem);
}

void MyCvDLLInterfaceIFace::setDirty(InterfaceDirtyBits eDirtyItem, bool bNewValue)
{
	getCvInterface().setInterfaceDirtyBit(eDirtyItem, bNewValue);
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
	getCvInterface().updateCursorType();
	return false;
}

void MyCvDLLInterfaceIFace::updatePythonScreens()
{
	// Any game action will invalidate UI anyway.
	// This might possibly be useful if we had real-time game updates where not every update will invalidate UI.
	//std::clog << "Ignoring call to " << __FUNCTION__ << std::endl;
	getCvInterface().updatePythonScreens();
}

void MyCvDLLInterfaceIFace::lookAt(NiPoint3 pt3Target, CameraLookAtTypes type, NiPoint3 attackDirection)
{
	// Reuse map functions.
	//const int x = gGlobals.getMap().pointXToPlotX(pt3Target.x);
	//const int y = gGlobals.getMap().pointYToPlotY(pt3Target.y);
	getCvInterface().lookAt(pt3Target, type, attackDirection);
}

void MyCvDLLInterfaceIFace::centerCamera(CvUnit* unit)
{
	getCvInterface().lookAtPlot({ unit->getX(), unit->getY() });
}

void MyCvDLLInterfaceIFace::releaseLockedCamera()
{
	getCvInterface().releaseLockedCamera();
}

bool MyCvDLLInterfaceIFace::isFocusedWidget()
{
	return getCvInterface().isFocusedWidget();
}

bool MyCvDLLInterfaceIFace::isFocused()
{
	return getCvInterface().isFocused();
}

bool MyCvDLLInterfaceIFace::isBareMapMode()
{
	return getCvInterface().isBareMapMode();
}

void MyCvDLLInterfaceIFace::toggleBareMapMode()
{
	return getCvInterface().toggleBareMapMode();
}

bool MyCvDLLInterfaceIFace::isShowYields()
{
	return getCvInterface().isShowYields();
}

void MyCvDLLInterfaceIFace::toggleYieldVisibleMode()
{
	return getCvInterface().toggleYieldVisibleMode();
}

bool MyCvDLLInterfaceIFace::isScoresVisible()
{
	return getCvInterface().isScoresVisible();
}

void MyCvDLLInterfaceIFace::toggleScoresVisible()
{
	return getCvInterface().toggleScoresVisible();
}

bool MyCvDLLInterfaceIFace::isScoresMinimized()
{
	return getCvInterface().isScoresMinimized();
}

void MyCvDLLInterfaceIFace::toggleScoresMinimized()
{
	return getCvInterface().toggleScoresMinimized();
}

bool MyCvDLLInterfaceIFace::isNetStatsVisible()
{
	std::abort();
}

int MyCvDLLInterfaceIFace::getOriginalPlotCount()
{
	return getCvInterface().getOriginalUnitSelectionIndex();
}

bool MyCvDLLInterfaceIFace::isCityScreenUp()
{
	return getCvInterface().isCityScreenUp();
}

bool MyCvDLLInterfaceIFace::isEndTurnMessage()
{
	return getCvInterface().isEndTurnMessage();
}

void MyCvDLLInterfaceIFace::setInterfaceMode(InterfaceModeTypes eNewValue)
{
	return getCvInterface().setInterfaceMode(eNewValue);
}

InterfaceModeTypes MyCvDLLInterfaceIFace::getInterfaceMode()
{
	return getCvInterface().getInterfaceMode();
}

InterfaceVisibility MyCvDLLInterfaceIFace::getShowInterface()
{
	return getCvInterface().getShowInterface();
}

CvPlot* MyCvDLLInterfaceIFace::getMouseOverPlot()
{
	return getCvInterface().getMouseOverPlot();
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
	return getCvInterface().setMinimapColor(eMinimapMode, { iX, iY }, eColor, fAlpha);
}

unsigned char* MyCvDLLInterfaceIFace::getMinimapBaseTexture() const
{
	return getCvInterface().getMinimapBaseTextureDXT1();
}

void MyCvDLLInterfaceIFace::setEndTurnMessage(bool bNewValue)
{
	getCvInterface().setEndTurnMessage(bNewValue);
}

bool MyCvDLLInterfaceIFace::isHasMovedUnit()
{
	return getCvInterface().isHasMovedUnit();
}

// Has the player manually moved a unit?
void MyCvDLLInterfaceIFace::setHasMovedUnit(bool bNewValue)
{
	return getCvInterface().setHasMovedUnit(bNewValue);
}

bool MyCvDLLInterfaceIFace::isForcePopup()
{
	return getCvInterface().isForcePopup();
}

void MyCvDLLInterfaceIFace::setForcePopup(bool bNewValue)
{
	getCvInterface().setForcePopup(bNewValue);
}

void MyCvDLLInterfaceIFace::lookAtCityOffset(int iCity)
{
	if (const CvCity* const city = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer()).getCity(iCity))
		getCvInterface().lookAtPlot({ city->getX(), city->getY() });
}

void MyCvDLLInterfaceIFace::toggleTurnLog()
{
	getCvInterface().toggleTurnLog();
}

void MyCvDLLInterfaceIFace::showTurnLog([[maybe_unused]] ChatTargetTypes eTarget)
{
	std::abort();
}

void MyCvDLLInterfaceIFace::dirtyTurnLog(PlayerTypes ePlayer)
{
	getCvInterface().dirtyTurnLog(ePlayer);
}

int MyCvDLLInterfaceIFace::getPlotListColumn()
{
	return getCvInterface().getPlotListColumn();
}

void MyCvDLLInterfaceIFace::verifyPlotListColumn()
{
	return getCvInterface().verifyPlotListColumn();
}

int MyCvDLLInterfaceIFace::getPlotListOffset()
{
	return getCvInterface().getPlotListOffset();
}

void MyCvDLLInterfaceIFace::unlockPopupHelp()
{
	// Not used?
	std::abort();
}

void MyCvDLLInterfaceIFace::showDetails(bool bPasswordOnly)
{
	//throw std::runtime_error("Not implemented.");
	return getCvInterface().showDetails(bPasswordOnly);
}

void MyCvDLLInterfaceIFace::showAdminDetails()
{
	//throw std::runtime_error("Not implemented.");
	return getCvInterface().showAdminDetails();
}

void MyCvDLLInterfaceIFace::toggleClockAlarm(bool bValue, int iHour, int iMin)
{
	return getCvInterface().toggleClockAlarm(bValue, iHour, iMin);
}

bool MyCvDLLInterfaceIFace::isClockAlarmOn()
{
	return getCvInterface().isClockAlarmOn();
}

void MyCvDLLInterfaceIFace::setScreenDying([[maybe_unused]] int iPythonFileID, [[maybe_unused]] bool bDying)
{
	// Not used?
	std::abort();
}

bool MyCvDLLInterfaceIFace::isExitingToMainMenu()
{
	return getCvInterface().isExitingToMainMenu();
}

void MyCvDLLInterfaceIFace::exitingToMainMenu(const char* szLoadFile)
{
	// Always null?
	if (szLoadFile)
		std::abort();
	return getCvInterface().exitToMainMenu();
}

void MyCvDLLInterfaceIFace::setWorldBuilder(bool bTurnOn)
{
	return getCvInterface().setWorldBuilder(bTurnOn);
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
	return getCvInterface().launchPopup(std::unique_ptr<CvPopup>(pPopup), bCreateOkButton, bState, iNumPixelScroll);
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
	return getCvInterface().setCityTabSelectionRow(t);
}

bool MyCvDLLInterfaceIFace::noTechSplash()
{
	return getCvInterface().noTechSplash();
}

bool MyCvDLLInterfaceIFace::isInAdvancedStart() const
{
	return getCvInterface().isInAdvancedStart();
}

void MyCvDLLInterfaceIFace::setInAdvancedStart(bool bAdvancedStart)
{
	return getCvInterface().setInAdvancedStart(bAdvancedStart);
}

bool MyCvDLLInterfaceIFace::isSpaceshipScreenUp() const
{
	return getCvInterface().isSpaceshipScreenUp();
}

bool MyCvDLLInterfaceIFace::isDebugMenuCreated() const
{
	return false;
}

void MyCvDLLInterfaceIFace::setBusy(bool bBusy)
{
	return getCvInterface().setBusy(bBusy);
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
