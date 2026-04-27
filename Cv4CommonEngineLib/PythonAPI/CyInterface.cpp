#include "CyInterface.h"
#include "CvPythonExtensions.h"
#include "../MyCvDLLInterfaceIFace.h"
#include "../inc/Cv4CommonEngineLib/CvInterface.h"
#include "../inc/Cv4CommonEngineLib/EngineSpecificsHeader.h"
#include "../inc/Cv4CommonEngineLib/AudioXmlDefs.h"

#include <CvCity.h>
#include <CyCity.h>
#include <CyUnit.h>
#include <CyPlot.h>
#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvPlayerAI.h>
#include <CvInfos.h>
#include <CvPlot.h>

#include <CommonStuff/range.h>

#include <pybind11/stl.h>

#include <iostream>
#include <ranges>

using heck::range;

using cvengine::abortOnUnimplementedPythonFunction;
using cvengine::engine_specific::getCvInterface;

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

bool CyInterface::DoSoundtrack(const std::string& szSoundtrackScript)
{
	const cvengine::AudioXmlDefs& xmlDefs = cvengine::AudioXmlDefs::getInstance();
	const auto it = xmlDefs.tagIndexLookup.find(szSoundtrackScript);
	if (it->second.type == xmlDefs.scriptType2D)
		getCvInterface().doSoundtrack(it->second.index);
	else if (it == xmlDefs.tagIndexLookup.end())
		std::clog << "Sound '" << szSoundtrackScript << "' does not exist.\n";
	else
		throw std::runtime_error(std::string("Unexpected sound type in ") + __FUNCTION__ + '.');

	// Return value not used?
	return true;
}

void CyInterface::addCombatMessage(PlayerTypes ePlayer, const std::wstring& szString)
{
	// Example message: CreateDestroy CvTalkingHeadMessage(1, 10, Barbarian's Wolf (1.00) vs Axolotl's Warrior (3.40), , 5, , -1, -1, -1, false, false)
	//getCvInterface().addCombatMessage(ePlayer, szString);
	addMessage(ePlayer, false, 10, szString.c_str(), std::nullopt,
		InterfaceMessageTypes::MESSAGE_TYPE_COMBAT_MESSAGE,
		std::nullopt, NO_COLOR, -1, -1, false, false
	);
}

void CyInterface::addImmediateMessage(const std::wstring& szString, const std::string& szSound)
{
	//getCvInterface().addImmediateMessage(szString, szSound);
	addMessage(gGlobals.getGame().getActivePlayer(), false, 0, szString, szSound.c_str(),
		InterfaceMessageTypes::MESSAGE_TYPE_DISPLAY_ONLY, {},
		NO_COLOR, -1, -1, false, false
	);
}

void CyInterface::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, const std::wstring& szString, const std::optional<std::string>& szSound,
	InterfaceMessageTypes eTypes, const std::optional<std::string>& szIcon,
	ColorTypes eFlashColor, int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	getCvInterface().addMessage(ePlayer, bForce, iLength, szString, szSound.value_or("").c_str(),
		eTypes, szIcon.value_or("").c_str(),
		eFlashColor, iFlashX, iFlashY, bShowOffScreenArrows, bShowOnScreenArrows
	);
}

void CyInterface::addQuestMessage([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] const std::string& szString)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::addSelectedCity(CyCity pNewValue)
{
	getCvInterface().addSelectedCity(pNewValue.getCity(), false);
}

void CyInterface::cacheInterfacePlotUnits(CyPlot* pyPlot)
{
	if (pyPlot)
		if (auto* const plot = pyPlot->getPlot())
			getCvInterface().cacheInterfacePlotUnits(*plot);
}

bool CyInterface::canCreateGroup()
{
	return !mirrorsSelectionGroup();
}

bool CyInterface::canDeleteGroup()
{
	return getCvInterface().getSelectionList()->getNumUnits() > 1 && mirrorsSelectionGroup();
}

bool CyInterface::canHandleAction(int iAction, bool bTestVisible)
{
	return gGlobals.getGame().canHandleAction(iAction, getCvInterface().getGotoPlot(), bTestVisible, false);
}

bool CyInterface::canSelectHeadUnit()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::checkFlashReset([[maybe_unused]] PlayerTypes ePlayer)
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::checkFlashUpdate()
{
	abortOnUnimplementedPythonFunction();
}

void CyInterface::clearSelectedCities()
{
	getCvInterface().clearSelectedCities();
}

void CyInterface::clearSelectionList()
{
	getCvInterface().clearSelectionList();
}

int CyInterface::countEntities(int iI)
{
	return getCvInterface().countEntities(static_cast<UnitTypes>(iI));
}

int CyInterface::determineWidth(const std::wstring& szBuffer)
{
	return getCvInterface().determineWidth(szBuffer);
}

void CyInterface::doPing([[maybe_unused]] int iX, [[maybe_unused]] int iY, [[maybe_unused]] PlayerTypes ePlayer)
{
	abortOnUnimplementedPythonFunction();
}

void CyInterface::endTimer([[maybe_unused]] const std::string& szOutputText)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::exitingToMainMenu([[maybe_unused]] const std::string& szLoadFile)
{
	if (!szLoadFile.empty())
		std::abort();
	getCvInterface().exitToMainMenu();
}

static bool isPlayableAction(int index)
{
	return gGlobals.getActionInfo(index).isVisible() && gGlobals.getGame().canHandleAction(index, getCvInterface().getGotoPlot(), true, true);
}

static std::vector<int> buildListOfActionsToShow()
{
	gGlobals.getGame().setupActionCache();
	return range(gGlobals.getNumActionInfos()) | std::views::filter(isPlayableAction) | std::ranges::to<std::vector>();
}

std::vector<int> CyInterface::getActionsToShow()
{
	return buildListOfActionsToShow();
}

CyUnit CyInterface::getCachedInterfacePlotUnit(int iIndex)
{
	return getCvInterface().getCachedInterfacePlotUnit(iIndex);
}

int CyInterface::getCityTabSelectionRow()
{
	return getCvInterface().getCityTabSelectionRow();
}

CyPlot CyInterface::getCursorPlot()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

EndTurnButtonStates CyInterface::getEndTurnState()
{
	if (getCvInterface().isEndTurnMessage())
		return END_TURN_OVER_HIGHLIGHT;
	else
		return gGlobals.getGame().getEndTurnState();
}

CyPlot CyInterface::getGotoPlot()
{
	return getCvInterface().getGotoPlot();
}

std::unique_ptr<CyCity> CyInterface::getHeadSelectedCity()
{
	if (auto* const x = getCvInterface().getHeadSelectedCity())
		return std::make_unique<CyCity>(x);
	return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getHeadSelectedUnit()
{
	if (auto* const x = getCvInterface().getSelectionUnit(0))
		return std::make_unique<CyUnit>(x);
	return nullptr;
}

std::string CyInterface::getHelpString()
{
	abortOnUnimplementedPythonFunction();
}

CyPlot CyInterface::getHighlightPlot()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

InterfaceModeTypes CyInterface::getInterfaceMode()
{
	return getCvInterface().getInterfaceMode();
}

CyUnit CyInterface::getInterfacePlotUnit(CyPlot plot, int index)
{
	return MyCvDLLInterfaceIFace::getInstance().getInterfacePlotUnit(plot.getPlot(), index);
}

int CyInterface::getLengthSelectionList()
{
	return getCvInterface().getSelectionList()->getNumUnits();
}

CyPlot CyInterface::getMouseOverPlot()
{
	return getCvInterface().getMouseOverPlot();
}

int CyInterface::getNumCachedInterfacePlotUnits()
{
	return getCvInterface().getNumCachedInterfacePlotUnits();
}

int CyInterface::getNumOrdersQueued()
{
	if (const CvCity* const city = getCvInterface().getHeadSelectedCity())
		return city->getNumOrdersQueued();
	else
		return 0;
}

int CyInterface::getNumVisibleUnits()
{
	return getCvInterface().getNumVisibleUnits();
}

int CyInterface::getOrderNodeData1(int iNode)
{
	if (const CvCity* const city = getCvInterface().getHeadSelectedCity())
		return city->getOrderData(iNode).iData1;
	else
		return -1;
}

int CyInterface::getOrderNodeData2(int iNode)
{
	if (const CvCity* const city = getCvInterface().getHeadSelectedCity())
		return city->getOrderData(iNode).iData2;
	else
		return -1;
}

bool CyInterface::getOrderNodeSave(int iNode)
{
	if (const CvCity* const city = getCvInterface().getHeadSelectedCity())
		return city->getOrderData(iNode).bSave;
	else
		return false;
}

OrderTypes CyInterface::getOrderNodeType(int iNode)
{
	if (const CvCity* const city = getCvInterface().getHeadSelectedCity())
		return city->getOrderData(iNode).eOrderType;
	else
		return OrderTypes::NO_ORDER;
}

int CyInterface::getPlotListColumn()
{
	return getCvInterface().getPlotListColumn();
}

int CyInterface::getPlotListOffset()
{
	return getCvInterface().getPlotListOffset();
}

std::unique_ptr<CyPlot> CyInterface::getSelectionPlot()
{
	// Used by CvMainInterface plot list, which also accepts the selected city plot.
	if (auto* const plot = getCvInterface().getSelectedCityOrUnitPlot())
		return std::make_unique<CyPlot>(plot);
	else
		return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getSelectionUnit(int index)
{
	if (auto* const x = getCvInterface().getSelectionUnit(index)) // O(N)! Quadratic time when listing a stack!
		return std::make_unique<CyUnit>(x);
	else
		return nullptr;
}

InterfaceVisibility CyInterface::getShowInterface()
{
	return getCvInterface().getShowInterface();
}

void CyInterface::insertIntoSelectionList(CyUnit pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound)
{
	getCvInterface().insertIntoSelectionList(pUnit.getUnit(), bClear, bToggle, bGroup, bSound, false);
}

bool CyInterface::isCityScreenUp()
{
	return getCvInterface().isCityScreenUp();
}

bool CyInterface::isCitySelected(CyCity pCity)
{
	return getCvInterface().isCitySelected(pCity.getCity());
}

bool CyInterface::isCitySelection()
{
	return getCvInterface().isCitySelection();
}

bool CyInterface::isDirty(InterfaceDirtyBits eDirty)
{
	return getCvInterface().getInterfaceDirtyBit(eDirty);
}

bool CyInterface::isFlashing()
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isFlashingPlayer([[maybe_unused]] int iPlayer)
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isFocusedWidget()
{
	return getCvInterface().isFocusedWidget();
}

bool CyInterface::isInAdvancedStart()
{
	return getCvInterface().isInAdvancedStart();
}

bool CyInterface::isInMainMenu()
{
	return getCvInterface().isInMainMenu();
}

bool CyInterface::isLeftMouseDown()
{
	return getCvInterface().isLeftMouseDown();
}

bool CyInterface::isNetStatsVisible()
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isOOSVisible()
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isOneCitySelected()
{
	auto* const head = getCvInterface().headSelectedCitiesNode();
	return head && !getCvInterface().nextSelectedCitiesNode(head);
}

bool CyInterface::isRightMouseDown()
{
	return getCvInterface().isRightMouseDown();
}

bool CyInterface::isScoresMinimized()
{
	return getCvInterface().isScoresMinimized();
}

bool CyInterface::isScoresVisible()
{
	return getCvInterface().isScoresVisible();
}

bool CyInterface::isScreenUp([[maybe_unused]] int iEnumVal)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isUnitFocus()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::isYieldVisibleMode()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::lookAtCityBuilding(int iCity, int iBuilding)
{
	return getCvInterface().lookAtCityBuilding(iCity, static_cast<BuildingTypes>(iBuilding));
}

void CyInterface::lookAtCityOffset(int iCity)
{
	MyCvDLLInterfaceIFace::getInstance().lookAtCityOffset(iCity);
}

void CyInterface::makeInterfaceDirty()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::mirrorsSelectionGroup()
{
	return getCvInterface().mirrorsSelectionGroup();
}

bool CyInterface::noTechSplash()
{
	return getCvInterface().noTechSplash();
}

void CyInterface::playAdvisorSound([[maybe_unused]] const std::string& pszSound)
{
	abortOnUnimplementedPythonFunction();
}

void CyInterface::playGeneralSound(const std::string& pszSound)
{
	//CvApp::getInstance().audioSystem->playSound(pszSound);
	const cvengine::AudioXmlDefs& xmlDefs = cvengine::AudioXmlDefs::getInstance();
	const auto it = xmlDefs.tagIndexLookup.find(pszSound);
	if (it->second.type == xmlDefs.scriptType2D)
		getCvInterface().play2DSound(it->second.index);
	else if (it == xmlDefs.tagIndexLookup.end())
		std::clog << "Sound '" << pszSound << "' does not exist.\n";
	else
		throw std::runtime_error(std::string("Unexpected sound type in ") + __FUNCTION__ + '.');
}

void CyInterface::playGeneralSoundAtPlot([[maybe_unused]] int iScriptID, [[maybe_unused]] CyPlot pPlot)
{
	//if (pPlot.getPlot())
	//	return getCvInterface().playGeneralSoundAtPlot(iScriptID, *pPlot.getPlot());

	// Possibly used by mods?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::playGeneralSoundByID([[maybe_unused]] int iScriptID)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::removeFromSelectionList(CyUnit pUnit)
{
	getCvInterface().removeFromSelectionList(pUnit.getUnit());
}

void CyInterface::selectAll(CyPlot pPlot)
{
	gGlobals.getGame().selectAll(pPlot.getPlot());
}

// This is called when clicking "Examine City" on a warning popup.
void CyInterface::selectCity(CyCity pNewValue, bool bTestProduction)
{
	getCvInterface().selectCity(pNewValue.getCity(), bTestProduction);
}

void CyInterface::selectGroup(CyUnit pUnit, bool bShift, bool bCtrl, bool bAlt)
{
	gGlobals.getGame().selectGroup(pUnit.getUnit(), bShift, bCtrl, bAlt);
}

int CyInterface::selectHotKeyUnit([[maybe_unused]] int iHotKeyNumber)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::selectUnit(CyUnit pUnit, bool bClear, bool bToggle, bool bSound)
{
	gGlobals.getGame().selectUnit(pUnit.getUnit(), bClear, bToggle, bSound);
}

void CyInterface::setBusy(bool bBusy)
{
	getCvInterface().setBusy(bBusy);
}

void CyInterface::setCityTabSelectionRow(CityTabTypes eIndex)
{
	getCvInterface().setCityTabSelectionRow(eIndex);
}

void CyInterface::setDirty(InterfaceDirtyBits eDirty, bool bDirty)
{
	getCvInterface().setInterfaceDirtyBit(eDirty, bDirty);
}

void CyInterface::setInterfaceMode(InterfaceModeTypes eMode)
{
	getCvInterface().setInterfaceMode(eMode);
}

void CyInterface::setPausedPopups([[maybe_unused]] bool bPausedPopups)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::setShowInterface(InterfaceVisibility eInterfaceVisibility)
{
	getCvInterface().setShowInterface(eInterfaceVisibility);
}

void CyInterface::setSoundSelectionReady(bool bReady)
{
	// Called by CvDawnOfMan.
	//cvengine::logWarning("({}). Unimplemented.", bReady);
	getCvInterface().setSoundSelectionReady(bReady);
}

void CyInterface::setWorldBuilder(bool bTurnOn)
{
	getCvInterface().setWorldBuilder(bTurnOn);
}

bool CyInterface::shiftKey()
{
	return cvengine::engine_specific::isShiftDown();
}

bool CyInterface::shouldDisplayEndTurn()
{
	return getCvInterface().shouldDisplayEndTurn();
}

bool CyInterface::shouldDisplayEndTurnButton()
{
	return gGlobals.getGame().shouldDisplayEndTurnButton();
}

bool CyInterface::shouldDisplayFlag()
{
	return getCvInterface().shouldDisplayFlag();
}

bool CyInterface::shouldDisplayReturn()
{
	return getCvInterface().shouldDisplayReturn();
}

bool CyInterface::shouldDisplayUnitModel()
{
	return getCvInterface().shouldDisplayUnitModel();
}

bool CyInterface::shouldDisplayWaitingOthers()
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldDisplayWaitingYou()
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldFlash([[maybe_unused]] int iPlayer)
{
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldShowAction([[maybe_unused]] int iAction)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldShowChangeResearchButton()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldShowResearchButtons()
{
	return getCvInterface().shouldShowResearchButtons();
}

bool CyInterface::shouldShowSelectionButtons()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

bool CyInterface::shouldShowYieldVisibleButton()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::startTimer()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::stop2DSound()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::stopAdvisorSound()
{
	// Need to check out this advisor screen...
	abortOnUnimplementedPythonFunction();
}

void CyInterface::toggleBareMapMode()
{
	getCvInterface().toggleBareMapMode();
}

void CyInterface::toggleMusicOn()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::toggleNetStatsVisible()
{
	abortOnUnimplementedPythonFunction();
}

void CyInterface::toggleScoresMinimized()
{
	getCvInterface().toggleScoresMinimized();
}

void CyInterface::toggleScoresVisible()
{
	getCvInterface().toggleScoresVisible();
}

void CyInterface::toggleYieldVisibleMode()
{
	getCvInterface().toggleYieldVisibleMode();
}
