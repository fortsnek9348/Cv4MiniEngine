#include "CyInterface.h"
#include "CyCommon.h"
#include "../CommonEngineGlobal.h"
#include "../MyCvDLLInterfaceIFace.h"
#include "../inc/Cv4CommonEngineLib/CvInterface.h"
#include "../inc/Cv4CommonEngineLib/AudioXmlDefs.h"

#include <CvGameCoreDLL/CvCity.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvPlot.h>
#include <CvGameCoreDLL/CyCity.h>
#include <CvGameCoreDLL/CyPlot.h>
#include <CvGameCoreDLL/CyUnit.h>

#include <CommonStuff/range.h>

#include <pybind11/stl.h>

#include <iostream>
#include <ranges>

using heck::range;

using cvengine::abortOnUnimplementedPythonFunction;
using cvengine::gCommonEngineConfig;

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
		gCommonEngineConfig.interface->doSoundtrack(it->second.index);
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
	//gCommonEngineConfig.interface->addCombatMessage(ePlayer, szString);
	addMessage(ePlayer, false, 10, szString.c_str(), std::nullopt,
		InterfaceMessageTypes::MESSAGE_TYPE_COMBAT_MESSAGE,
		std::nullopt, NO_COLOR, -1, -1, false, false
	);
}

void CyInterface::addImmediateMessage(const std::wstring& szString, const std::string& szSound)
{
	//gCommonEngineConfig.interface->addImmediateMessage(szString, szSound);
	addMessage(gGlobals.getGame().getActivePlayer(), false, 0, szString, szSound.c_str(),
		InterfaceMessageTypes::MESSAGE_TYPE_DISPLAY_ONLY, {},
		NO_COLOR, -1, -1, false, false
	);
}

void CyInterface::addMessage(PlayerTypes ePlayer, bool bForce, int iLength, const std::wstring& szString, const std::optional<std::string>& szSound,
	InterfaceMessageTypes eTypes, const std::optional<std::string>& szIcon,
	ColorTypes eFlashColor, int iFlashX, int iFlashY, bool bShowOffScreenArrows, bool bShowOnScreenArrows)
{
	gCommonEngineConfig.interface->addMessage(ePlayer, bForce, iLength, szString, szSound.value_or("").c_str(),
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
	gCommonEngineConfig.interface->addSelectedCity(pNewValue.getCity(), false);
}

void CyInterface::cacheInterfacePlotUnits(CyPlot* pyPlot)
{
	// If plot null, then list should still be cleared.
	gCommonEngineConfig.interface->cacheInterfacePlotUnits(pyPlot ? pyPlot->getPlot() : nullptr);
}

bool CyInterface::canCreateGroup()
{
	return !mirrorsSelectionGroup();
}

bool CyInterface::canDeleteGroup()
{
	return gCommonEngineConfig.interface->getSelectionList()->getNumUnits() > 1 && mirrorsSelectionGroup();
}

bool CyInterface::canHandleAction(int iAction, bool bTestVisible)
{
	return gGlobals.getGame().canHandleAction(iAction, gCommonEngineConfig.interface->getGotoPlot(), bTestVisible, false);
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
	gCommonEngineConfig.interface->clearSelectedCities();
}

void CyInterface::clearSelectionList()
{
	gCommonEngineConfig.interface->clearSelectionList();
}

int CyInterface::countEntities(int iI)
{
	return gCommonEngineConfig.interface->countEntities(static_cast<UnitTypes>(iI));
}

int CyInterface::determineWidth(const std::wstring& szBuffer)
{
	return gCommonEngineConfig.interface->determineWidth(szBuffer);
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
	gCommonEngineConfig.interface->exitToMainMenu();
}

static bool isPlayableAction(int index)
{
	return gGlobals.getActionInfo(index).isVisible() && gGlobals.getGame().canHandleAction(index, gCommonEngineConfig.interface->getGotoPlot(), true, true);
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
	return gCommonEngineConfig.interface->getCachedInterfacePlotUnit(iIndex);
}

int CyInterface::getCityTabSelectionRow()
{
	return gCommonEngineConfig.interface->getCityTabSelectionRow();
}

CyPlot CyInterface::getCursorPlot()
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

EndTurnButtonStates CyInterface::getEndTurnState()
{
	if (gCommonEngineConfig.interface->isEndTurnMessage())
		return END_TURN_OVER_HIGHLIGHT;
	else
		return gGlobals.getGame().getEndTurnState();
}

CyPlot CyInterface::getGotoPlot()
{
	return gCommonEngineConfig.interface->getGotoPlot();
}

std::unique_ptr<CyCity> CyInterface::getHeadSelectedCity()
{
	if (auto* const x = gCommonEngineConfig.interface->getHeadSelectedCity())
		return std::make_unique<CyCity>(x);
	return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getHeadSelectedUnit()
{
	if (auto* const x = gCommonEngineConfig.interface->getSelectionUnit(0))
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
	return gCommonEngineConfig.interface->getInterfaceMode();
}

CyUnit CyInterface::getInterfacePlotUnit(CyPlot plot, int index)
{
	return MyCvDLLInterfaceIFace::getInstance().getInterfacePlotUnit(plot.getPlot(), index);
}

int CyInterface::getLengthSelectionList()
{
	return gCommonEngineConfig.interface->getSelectionList()->getNumUnits();
}

CyPlot CyInterface::getMouseOverPlot()
{
	return gCommonEngineConfig.interface->getMouseOverPlot();
}

int CyInterface::getNumCachedInterfacePlotUnits()
{
	return gCommonEngineConfig.interface->getNumCachedInterfacePlotUnits();
}

int CyInterface::getNumOrdersQueued()
{
	if (const CvCity* const city = gCommonEngineConfig.interface->getHeadSelectedCity())
		return city->getNumOrdersQueued();
	else
		return 0;
}

int CyInterface::getNumVisibleUnits()
{
	return gCommonEngineConfig.interface->getNumVisibleUnits();
}

int CyInterface::getOrderNodeData1(int iNode)
{
	if (const CvCity* const city = gCommonEngineConfig.interface->getHeadSelectedCity())
		return city->getOrderData(iNode).iData1;
	else
		return -1;
}

int CyInterface::getOrderNodeData2(int iNode)
{
	if (const CvCity* const city = gCommonEngineConfig.interface->getHeadSelectedCity())
		return city->getOrderData(iNode).iData2;
	else
		return -1;
}

bool CyInterface::getOrderNodeSave(int iNode)
{
	if (const CvCity* const city = gCommonEngineConfig.interface->getHeadSelectedCity())
		return city->getOrderData(iNode).bSave;
	else
		return false;
}

OrderTypes CyInterface::getOrderNodeType(int iNode)
{
	if (const CvCity* const city = gCommonEngineConfig.interface->getHeadSelectedCity())
		return city->getOrderData(iNode).eOrderType;
	else
		return OrderTypes::NO_ORDER;
}

int CyInterface::getPlotListColumn()
{
	return gCommonEngineConfig.interface->getPlotListColumn();
}

int CyInterface::getPlotListOffset()
{
	return gCommonEngineConfig.interface->getPlotListOffset();
}

std::unique_ptr<CyPlot> CyInterface::getSelectionPlot()
{
	// Used by CvMainInterface plot list, which also accepts the selected city plot.
	if (auto* const plot = gCommonEngineConfig.interface->getSelectedCityOrUnitPlot())
		return std::make_unique<CyPlot>(plot);
	else
		return nullptr;
}

std::unique_ptr<CyUnit> CyInterface::getSelectionUnit(int index)
{
	if (auto* const x = gCommonEngineConfig.interface->getSelectionUnit(index)) // O(N)! Quadratic time when listing a stack!
		return std::make_unique<CyUnit>(x);
	else
		return nullptr;
}

InterfaceVisibility CyInterface::getShowInterface()
{
	return gCommonEngineConfig.interface->getShowInterface();
}

void CyInterface::insertIntoSelectionList(CyUnit pUnit, bool bClear, bool bToggle, bool bGroup, bool bSound)
{
	gCommonEngineConfig.interface->insertIntoSelectionList(pUnit.getUnit(), bClear, bToggle, bGroup, bSound, false);
}

bool CyInterface::isCityScreenUp()
{
	return gCommonEngineConfig.interface->isCityScreenUp();
}

bool CyInterface::isCitySelected(CyCity pCity)
{
	return gCommonEngineConfig.interface->isCitySelected(pCity.getCity());
}

bool CyInterface::isCitySelection()
{
	return gCommonEngineConfig.interface->isCitySelection();
}

bool CyInterface::isDirty(InterfaceDirtyBits eDirty)
{
	return gCommonEngineConfig.interface->getInterfaceDirtyBit(eDirty);
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
	return gCommonEngineConfig.interface->isFocusedWidget();
}

bool CyInterface::isInAdvancedStart()
{
	return gCommonEngineConfig.interface->isInAdvancedStart();
}

bool CyInterface::isInMainMenu()
{
	return gCommonEngineConfig.interface->isInMainMenu();
}

bool CyInterface::isLeftMouseDown()
{
	return gCommonEngineConfig.interface->isLeftMouseDown();
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
	auto* const head = gCommonEngineConfig.interface->headSelectedCitiesNode();
	return head && !gCommonEngineConfig.interface->nextSelectedCitiesNode(head);
}

bool CyInterface::isRightMouseDown()
{
	return gCommonEngineConfig.interface->isRightMouseDown();
}

bool CyInterface::isScoresMinimized()
{
	return gCommonEngineConfig.interface->isScoresMinimized();
}

bool CyInterface::isScoresVisible()
{
	return gCommonEngineConfig.interface->isScoresVisible();
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
	return gCommonEngineConfig.interface->lookAtCityBuilding(iCity, static_cast<BuildingTypes>(iBuilding));
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
	return gCommonEngineConfig.interface->mirrorsSelectionGroup();
}

bool CyInterface::noTechSplash()
{
	return gCommonEngineConfig.interface->noTechSplash();
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
		gCommonEngineConfig.interface->play2DSound(it->second.index);
	else if (it == xmlDefs.tagIndexLookup.end())
		std::clog << "Sound '" << pszSound << "' does not exist.\n";
	else
		throw std::runtime_error(std::string("Unexpected sound type in ") + __FUNCTION__ + '.');
}

void CyInterface::playGeneralSoundAtPlot([[maybe_unused]] int iScriptID, [[maybe_unused]] CyPlot pPlot)
{
	//if (pPlot.getPlot())
	//	return gCommonEngineConfig.interface->playGeneralSoundAtPlot(iScriptID, *pPlot.getPlot());

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
	gCommonEngineConfig.interface->removeFromSelectionList(pUnit.getUnit());
}

void CyInterface::selectAll(CyPlot pPlot)
{
	gGlobals.getGame().selectAll(pPlot.getPlot());
}

// This is called when clicking "Examine City" on a warning popup.
void CyInterface::selectCity(CyCity pNewValue, bool bTestProduction)
{
	gCommonEngineConfig.interface->selectCity(pNewValue.getCity(), bTestProduction);
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
	gCommonEngineConfig.interface->setBusy(bBusy);
}

void CyInterface::setCityTabSelectionRow(CityTabTypes eIndex)
{
	gCommonEngineConfig.interface->setCityTabSelectionRow(eIndex);
}

void CyInterface::setDirty(InterfaceDirtyBits eDirty, bool bDirty)
{
	gCommonEngineConfig.interface->setInterfaceDirtyBit(eDirty, bDirty);
}

void CyInterface::setInterfaceMode(InterfaceModeTypes eMode)
{
	gCommonEngineConfig.interface->setInterfaceMode(eMode);
}

void CyInterface::setPausedPopups([[maybe_unused]] bool bPausedPopups)
{
	// Not used?
	abortOnUnimplementedPythonFunction();
}

void CyInterface::setShowInterface(InterfaceVisibility eInterfaceVisibility)
{
	gCommonEngineConfig.interface->setShowInterface(eInterfaceVisibility);
}

void CyInterface::setSoundSelectionReady(bool bReady)
{
	// Called by CvDawnOfMan.
	//cvengine::logWarning("({}). Unimplemented.", bReady);
	gCommonEngineConfig.interface->setSoundSelectionReady(bReady);
}

void CyInterface::setWorldBuilder(bool bTurnOn)
{
	gCommonEngineConfig.interface->setWorldBuilder(bTurnOn);
}

bool CyInterface::shiftKey()
{
	return gCommonEngineConfig.callbackHandler->isShiftDown();
}

bool CyInterface::shouldDisplayEndTurn()
{
	return gCommonEngineConfig.interface->shouldDisplayEndTurn();
}

bool CyInterface::shouldDisplayEndTurnButton()
{
	return gGlobals.getGame().shouldDisplayEndTurnButton();
}

bool CyInterface::shouldDisplayFlag()
{
	return gCommonEngineConfig.interface->shouldDisplayFlag();
}

bool CyInterface::shouldDisplayReturn()
{
	return gCommonEngineConfig.interface->shouldDisplayReturn();
}

bool CyInterface::shouldDisplayUnitModel()
{
	return gCommonEngineConfig.interface->shouldDisplayUnitModel();
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
	return gCommonEngineConfig.interface->shouldShowResearchButtons();
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
	gCommonEngineConfig.interface->toggleBareMapMode();
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
	gCommonEngineConfig.interface->toggleScoresMinimized();
}

void CyInterface::toggleScoresVisible()
{
	gCommonEngineConfig.interface->toggleScoresVisible();
}

void CyInterface::toggleYieldVisibleMode()
{
	gCommonEngineConfig.interface->toggleYieldVisibleMode();
}
