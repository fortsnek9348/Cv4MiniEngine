#include "CyInterface.h"
#include "CvPythonExtensions.h"
#include "../Logger.h"
#include "../Common.h"
#include "../CvInterface.h"
#include "../DLLInterface/MyCvDLLUtility.h"
#include "../DLLInterface/MyCvDLLInterfaceIFace.h"
#include "../MainInterface.h"
#include "../CvApp.h"

#include <CvCity.h>
#include <CyCity.h>
#include <CyUnit.h>
#include <CyPlot.h>
#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvPlayerAI.h>


#include <pybind11/stl.h>

#include <iostream>

void CyInterface::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyInterface::x)

	pybind11::class_<CyInterface>(m, "CyInterface")
		.def(pybind11::init())
		.R(DoSoundtrack)
		.R(addCombatMessage)
		.R(addImmediateMessage)
		.R(addMessage)
		.R(addQuestMessage)
		.R(addSelectedCity)
		.R(cacheInterfacePlotUnits)
		.R(canCreateGroup)
		.R(canDeleteGroup)
		.R(canHandleAction)
		.R(canSelectHeadUnit)
		.R(checkFlashReset)
		.R(checkFlashUpdate)
		.R(clearSelectedCities)
		.R(clearSelectionList)
		.R(countEntities)
		.R(determineWidth)
		.R(doPing)
		.R(endTimer)
		.R(exitingToMainMenu)
		.R(getActionsToShow)
		.R(getCachedInterfacePlotUnit)
		.R(getCityTabSelectionRow)
		.R(getCursorPlot)
		.R(getEndTurnState)
		.R(getGotoPlot)
		.R(getHeadSelectedCity)
		.R(getHeadSelectedUnit)
		.R(getHelpString)
		.R(getHighlightPlot)
		.R(getInterfaceMode)
		.R(getInterfacePlotUnit)
		.R(getLengthSelectionList)
		.R(getMouseOverPlot)
		.R(getNumCachedInterfacePlotUnits)
		.R(getNumOrdersQueued)
		.R(getNumVisibleUnits)
		.R(getOrderNodeData1)
		.R(getOrderNodeData2)
		.R(getOrderNodeSave)
		.R(getOrderNodeType)
		.R(getPlotListColumn)
		.R(getPlotListOffset)
		.R(getSelectionPlot)
		.R(getSelectionUnit)
		.R(getShowInterface)
		.R(insertIntoSelectionList)
		.R(isCityScreenUp)
		.R(isCitySelected)
		.R(isCitySelection)
		.R(isDirty)
		.R(isFlashing)
		.R(isFlashingPlayer)
		.R(isFocusedWidget)
		.R(isInAdvancedStart)
		.R(isInMainMenu)
		.R(isLeftMouseDown)
		.R(isNetStatsVisible)
		.R(isOOSVisible)
		.R(isOneCitySelected)
		.R(isRightMouseDown)
		.R(isScoresMinimized)
		.R(isScoresVisible)
		.R(isScreenUp)
		.R(isUnitFocus)
		.R(isYieldVisibleMode)
		.R(lookAtCityBuilding)
		.R(lookAtCityOffset)
		.R(makeInterfaceDirty)
		.R(mirrorsSelectionGroup)
		.R(noTechSplash)
		.R(playAdvisorSound)
		.R(playGeneralSound)
		.R(playGeneralSoundAtPlot)
		.R(playGeneralSoundByID)
		.R(removeFromSelectionList)
		.R(selectAll)
		.R(selectCity)
		.R(selectGroup)
		.R(selectHotKeyUnit)
		.R(selectUnit)
		.R(setBusy)
		.R(setCityTabSelectionRow)
		.R(setDirty)
		.R(setInterfaceMode)
		.R(setPausedPopups)
		.R(setShowInterface)
		.R(setSoundSelectionReady)
		.R(setWorldBuilder)
		.R(shiftKey)
		.R(shouldDisplayEndTurn)
		.R(shouldDisplayEndTurnButton)
		.R(shouldDisplayFlag)
		.R(shouldDisplayReturn)
		.R(shouldDisplayUnitModel)
		.R(shouldDisplayWaitingOthers)
		.R(shouldDisplayWaitingYou)
		.R(shouldFlash)
		.R(shouldShowAction)
		.R(shouldShowChangeResearchButton)
		.R(shouldShowResearchButtons)
		.R(shouldShowSelectionButtons)
		.R(shouldShowYieldVisibleButton)
		.R(startTimer)
		.R(stop2DSound)
		.R(stopAdvisorSound)
		.R(toggleBareMapMode)
		.R(toggleMusicOn)
		.R(toggleNetStatsVisible)
		.R(toggleScoresMinimized)
		.R(toggleScoresVisible)
		.R(toggleYieldVisibleMode)
		;
}

namespace
{
	struct DummyInterface
	{
		InterfaceVisibility showInterface = INTERFACE_SHOW;
	};

	constinit DummyInterface gDummyInterface{};
}

bool CyInterface::DoSoundtrack([[maybe_unused]] const std::string& szSoundtrackScript)
{
	unimplementedPythonFunction();
}

void CyInterface::addCombatMessage(PlayerTypes ePlayer, const std::wstring& szString)
{
	// Example message: CreateDestroy CvTalkingHeadMessage(1, 10, Barbarian's Wolf (1.00) vs Axolotl's Warrior (3.40), , 5, , -1, -1, -1, false, false)
	CvInterface::getInstance().addCombatMessage(ePlayer, szString);
}

void CyInterface::addImmediateMessage(const std::wstring& szString, const std::string& szSound)
{
	CvInterface::getInstance().addImmediateMessage(szString, szSound);
}

void CyInterface::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, const std::wstring& szString, const std::optional<std::string>& szSound,
	InterfaceMessageTypes eTypes, const std::optional<std::string>& szIcon,
	ColorTypes eFlashColor, int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	CvInterface::getInstance().addMessage(ePlayer, bForce, iLength, szString, szSound.value_or("").c_str(),
		eTypes, szIcon.value_or("").c_str(),
		eFlashColor, iFlashX, iFlashY, bShowOffScreenArrows, bShowOnScreenArrows
	);
}

void CyInterface::addQuestMessage([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] const std::string& szString)
{
	unimplementedPythonFunction();
}

void CyInterface::addSelectedCity(CyCity pNewValue)
{
	CvInterface::getInstance().addSelectedCity(pNewValue.getCity(), false);
}

void CyInterface::cacheInterfacePlotUnits(CyPlot* plot)
{
	CvInterface::getInstance().cacheInterfacePlotUnits(plot ? plot->getPlot() : nullptr);
}

bool CyInterface::canCreateGroup()
{
	return !mirrorsSelectionGroup();
}

bool CyInterface::canDeleteGroup()
{
	return CvInterface::getInstance().getSelectionGroup().getNumUnits() > 1 && mirrorsSelectionGroup();
}

bool CyInterface::canHandleAction(int iAction, bool bTestVisible)
{
	return gGlobals.getGame().canHandleAction(iAction, CvInterface::getInstance().getGotoPlot(), bTestVisible, false);
}

bool CyInterface::canSelectHeadUnit()
{
	unimplementedPythonFunction();
}

void CyInterface::checkFlashReset([[maybe_unused]] PlayerTypes ePlayer)
{
	unimplementedPythonFunction();
}

bool CyInterface::checkFlashUpdate()
{
	unimplementedPythonFunction();
}

void CyInterface::clearSelectedCities()
{
	CvInterface::getInstance().clearSelectedCities();
}

void CyInterface::clearSelectionList()
{
	unimplementedPythonFunction();
}

int CyInterface::countEntities([[maybe_unused]] int iI)
{
	unimplementedPythonFunction();
}

int CyInterface::determineWidth([[maybe_unused]] const std::string& szBuffer)
{
	return 0;
}

void CyInterface::doPing([[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] PlayerTypes ePlayer)
{
	unimplementedPythonFunction();
}

void CyInterface::endTimer([[maybe_unused]] const std::string& szOutputText)
{
	unimplementedPythonFunction();
}

void CyInterface::exitingToMainMenu([[maybe_unused]] const std::string& szLoadFile)
{
	unimplementedPythonFunction();
}

std::vector<int> CyInterface::getActionsToShow()
{
	return buildListOfActionsToShow();
}

CyUnit CyInterface::getCachedInterfacePlotUnit(int iIndex)
{
	return CyUnit(CvInterface::getInstance().getCachedInterfacePlotUnit(iIndex));
}

int CyInterface::getCityTabSelectionRow()
{
	unimplementedPythonFunction();
}

CyPlot CyInterface::getCursorPlot()
{
	unimplementedPythonFunction();
}

EndTurnButtonStates CyInterface::getEndTurnState()
{
	if (CvInterface::getInstance().isEndTurnMessage())
		return END_TURN_OVER_HIGHLIGHT;
	else
		return gGlobals.getGame().getEndTurnState();
}

CyPlot CyInterface::getGotoPlot()
{
	unimplementedPythonFunction();
}

std::unique_ptr<CyCity> CyInterface::getHeadSelectedCity()
{
	if (auto* const x = CvInterface::getInstance().getHeadSelectedCity())
		return std::make_unique<CyCity>(x);
	return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getHeadSelectedUnit()
{
	if (auto* const x = CvInterface::getInstance().findSelectedUnitByIndex(0))
		return std::make_unique<CyUnit>(x);
	return nullptr;
}

std::string CyInterface::getHelpString()
{
	unimplementedPythonFunction();
}

CyPlot CyInterface::getHighlightPlot()
{
	unimplementedPythonFunction();
}

InterfaceModeTypes CyInterface::getInterfaceMode()
{
	return CvInterface::getInstance().getInterfaceMode();
}

CyUnit CyInterface::getInterfacePlotUnit()
{
	unimplementedPythonFunction();
}

int CyInterface::getLengthSelectionList()
{
	return CvInterface::getInstance().getSelectionGroup().getNumUnits();
}

CyPlot CyInterface::getMouseOverPlot()
{
	unimplementedPythonFunction();
}

int CyInterface::getNumCachedInterfacePlotUnits()
{
	return (int)CvInterface::getInstance().getNumCachedInterfacePlotUnits();
}

int CyInterface::getNumOrdersQueued()
{
	if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
		return city->getNumOrdersQueued();
	else
		return 0;
}

int CyInterface::getNumVisibleUnits()
{
	unimplementedPythonFunction();
}

int CyInterface::getOrderNodeData1(int iNode)
{
	if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
		return city->getOrderData(iNode).iData1;
	else
		return -1;
}

int CyInterface::getOrderNodeData2(int iNode)
{
	if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
		return city->getOrderData(iNode).iData2;
	else
		return -1;
}

bool CyInterface::getOrderNodeSave(int iNode)
{
	if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
		return city->getOrderData(iNode).bSave;
	else
		return false;
}

OrderTypes CyInterface::getOrderNodeType(int iNode)
{
	if (const CvCity* const city = CvInterface::getInstance().getHeadSelectedCity())
		return city->getOrderData(iNode).eOrderType;
	else
		return OrderTypes::NO_ORDER;
}

int CyInterface::getPlotListColumn()
{
	unimplementedPythonFunction();
}

int CyInterface::getPlotListOffset()
{
	unimplementedPythonFunction();
}

std::unique_ptr<CyPlot> CyInterface::getSelectionPlot()
{
	// Used by CvMainInterface plot list, which also accepts the selected city plot.
	if (auto* const plot = CvInterface::getInstance().getSelectedCityOrUnitPlot())
		return std::make_unique<CyPlot>(plot);
	else
		return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getSelectionUnit(int index)
{
	if (auto* const x = CvInterface::getInstance().getSelectionGroup().getUnitAt(index)) // O(N)! Quadratic time when listing a stack!
		return std::make_unique<CyUnit>(x);
	else
		return nullptr;
}

InterfaceVisibility CyInterface::getShowInterface()
{
	return gDummyInterface.showInterface;
}

void CyInterface::insertIntoSelectionList([[maybe_unused]] CyUnit pUnit, [[maybe_unused]] bool bClear, [[maybe_unused]] bool bToggle, [[maybe_unused]] bool bGroup, [[maybe_unused]] bool bSound)
{
	unimplementedPythonFunction();
}

bool CyInterface::isCityScreenUp()
{
	return CvInterface::getInstance().isInCityScreen();
}

bool CyInterface::isCitySelected(CyCity pCity)
{
	return CvInterface::getInstance().isCitySelected(pCity.getCity());
}

bool CyInterface::isCitySelection()
{
	unimplementedPythonFunction();
}

bool CyInterface::isDirty(InterfaceDirtyBits eDirty)
{
	return CvInterface::getInstance().getInterfaceDirtyBit(eDirty);
}

bool CyInterface::isFlashing()
{
	unimplementedPythonFunction();
}

bool CyInterface::isFlashingPlayer([[maybe_unused]] int iPlayer)
{
	unimplementedPythonFunction();
}

bool CyInterface::isFocusedWidget()
{
	unimplementedPythonFunction();
}

bool CyInterface::isInAdvancedStart()
{
	unimplementedPythonFunction();
}

bool CyInterface::isInMainMenu()
{
	unimplementedPythonFunction();
}

bool CyInterface::isLeftMouseDown()
{
	unimplementedPythonFunction();
}

bool CyInterface::isNetStatsVisible()
{
	unimplementedPythonFunction();
}

bool CyInterface::isOOSVisible()
{
	unimplementedPythonFunction();
}

bool CyInterface::isOneCitySelected()
{
	unimplementedPythonFunction();
}

bool CyInterface::isRightMouseDown()
{
	unimplementedPythonFunction();
}

bool CyInterface::isScoresMinimized()
{
	return CvInterface::getInstance().isScoresMinimized();
}

bool CyInterface::isScoresVisible()
{
	// TODO: UI options
	return true;
}

bool CyInterface::isScreenUp([[maybe_unused]] int iEnumVal)
{
	unimplementedPythonFunction();
}

bool CyInterface::isUnitFocus()
{
	unimplementedPythonFunction();
}

bool CyInterface::isYieldVisibleMode()
{
	unimplementedPythonFunction();
}

void CyInterface::lookAtCityBuilding([[maybe_unused]] int iCity, [[maybe_unused]] int iBuilding)
{
	unimplementedPythonFunction();
}

void CyInterface::lookAtCityOffset(int iCity)
{
	MyCvDLLInterfaceIFace().lookAtCityOffset(iCity);
}

void CyInterface::makeInterfaceDirty()
{
	unimplementedPythonFunction();
}

bool CyInterface::mirrorsSelectionGroup()
{
	return MyCvDLLUtility::getInstance().getInterfaceIFace()->mirrorsSelectionGroup();
}

bool CyInterface::noTechSplash()
{
	return false;
}

void CyInterface::playAdvisorSound([[maybe_unused]] const std::string& pszSound)
{
	unimplementedPythonFunction();
}

void CyInterface::playGeneralSound(const std::string& pszSound)
{
	CvApp::getInstance().audioSystem->playSound(pszSound);
}

void CyInterface::playGeneralSoundAtPlot([[maybe_unused]] int iScriptID, [[maybe_unused]] CyPlot pPlot)
{
	unimplementedPythonFunction();
}

void CyInterface::playGeneralSoundByID([[maybe_unused]] int iScriptID)
{
	unimplementedPythonFunction();
}

void CyInterface::removeFromSelectionList([[maybe_unused]] CyUnit pUnit)
{
	unimplementedPythonFunction();
}

void CyInterface::selectAll([[maybe_unused]] CyPlot pPlot)
{
	unimplementedPythonFunction();
}

// This is called when clicking "Examine City" on a warning popup.
void CyInterface::selectCity(CyCity pNewValue, bool bTestProduction)
{
	MyCvDLLUtility::getInstance().getInterfaceIFace()->selectCity(pNewValue.getCity(), bTestProduction);
}

void CyInterface::selectGroup([[maybe_unused]] CyUnit pUnit, [[maybe_unused]] bool bShift, [[maybe_unused]] bool bCtrl, [[maybe_unused]] bool bAlt)
{
	unimplementedPythonFunction();
}

int CyInterface::selectHotKeyUnit([[maybe_unused]] int iHotKeyNumber)
{
	unimplementedPythonFunction();
}

void CyInterface::selectUnit([[maybe_unused]] CyUnit pUnit, [[maybe_unused]] bool bClear, [[maybe_unused]] bool bToggle, [[maybe_unused]] bool bSound)
{
	unimplementedPythonFunction();
}

void CyInterface::setBusy([[maybe_unused]] bool bBusy)
{
	unimplementedPythonFunction();
}

void CyInterface::setCityTabSelectionRow([[maybe_unused]] CityTabTypes eIndex)
{
	unimplementedPythonFunction();
}

void CyInterface::setDirty(InterfaceDirtyBits eDirty, bool bDirty)
{
	CvInterface::getInstance().setInterfaceDirtyBit(eDirty, bDirty);
}

void CyInterface::setInterfaceMode([[maybe_unused]] InterfaceModeTypes eMode)
{
	unimplementedPythonFunction();
}

void CyInterface::setPausedPopups([[maybe_unused]] bool bPausedPopups)
{
	unimplementedPythonFunction();
}

void CyInterface::setShowInterface(InterfaceVisibility eInterfaceVisibility)
{
	gDummyInterface.showInterface = eInterfaceVisibility;
}

void CyInterface::setSoundSelectionReady(bool bReady)
{
	// Called by CvDawnOfMan.
	cmdciv4::logWarning("({}). Unimplemented.", bReady);
}

void CyInterface::setWorldBuilder([[maybe_unused]] bool bTurnOn)
{
	unimplementedPythonFunction();
}

bool CyInterface::shiftKey()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldDisplayEndTurn()
{
	std::abort();
}

bool CyInterface::shouldDisplayEndTurnButton()
{
	return gGlobals.getGame().shouldDisplayEndTurnButton();
}

bool CyInterface::shouldDisplayFlag()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldDisplayReturn()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldDisplayUnitModel()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldDisplayWaitingOthers()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldDisplayWaitingYou()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldFlash([[maybe_unused]] int iPlayer)
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldShowAction([[maybe_unused]] int iAction)
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldShowChangeResearchButton()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldShowResearchButtons()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldShowSelectionButtons()
{
	unimplementedPythonFunction();
}

bool CyInterface::shouldShowYieldVisibleButton()
{
	unimplementedPythonFunction();
}

void CyInterface::startTimer()
{
	unimplementedPythonFunction();
}

void CyInterface::stop2DSound()
{
	unimplementedPythonFunction();
}

void CyInterface::stopAdvisorSound()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleBareMapMode()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleMusicOn()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleNetStatsVisible()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleScoresMinimized()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleScoresVisible()
{
	unimplementedPythonFunction();
}

void CyInterface::toggleYieldVisibleMode()
{
	unimplementedPythonFunction();
}
