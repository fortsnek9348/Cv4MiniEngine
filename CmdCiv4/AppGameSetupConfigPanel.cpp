#include "AppGameSetupConfigPanel.h"
#include "AppGameSetupPlayerListPanel.h"
#include "AppGameSetupMapScript.h"
#include "AppGameSetupWindow.h"
#include "Common.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "CvApp.h"

#include <CvGlobals.h>
#include <CvInitCore.h>
#include <CvDLLPythonIFaceBase.h>
#include <CvDLLUtilityIFaceBase.h>
#include <CvInfos.h>

#include <CommonStuff/range.h>

namespace tui = hecktui;
using heck::range;

static constexpr tui::EComboboxStyle kSettingsComboboxStyle = tui::EComboboxStyle::Bulky;



AppGameSetupConfigPanel::AppGameSetupConfigPanel(const std::vector<AppGameSetupMapScriptInfo>& mapScripts, AppGameSetupWindow& singlePlayerCustomWindow, std::shared_ptr<AppGameSetupPlayerListPanel> playerListPanel)
	: ScrollBarPanel(tui::EAxis::Vertical)
	, mSinglePlayerCustomWindow(singlePlayerCustomWindow)
{
	CvInitCore& initCore = gGlobals.getInitCore();

	const CvWString randomStr = MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_RANDOM");

	const auto client = getPanel();

	const tui::TableLayoutConfig kNameValueTable{
		.cols{ tui::TableLayoutConfig::kAutoSize, tui::TableLayoutConfig::kFlex },
		.rows{ tui::TableLayoutConfig::kAutoSize },
		.cells{ tui::TableLayoutConfig::Cell{.coord{ 0, 0 }, .align{ tui::EJustilign::End, tui::EJustilign::Stretch } }, tui::TableLayoutConfig::Cell{.coord{ 1, 0 } } },
		.gap{ 1, 0 },
	};

	client->setLayout(std::make_unique<tui::TableLayout>(tui::TableLayoutConfig{
	.cols{ tui::TableLayoutConfig::kAutoSize, tui::TableLayoutConfig::kAutoSize, tui::TableLayoutConfig::kFlex },
	.rows{ tui::TableLayoutConfig::kAutoSize },
	.cells{ tui::TableLayoutConfig::Cell{.coord{ 0, 0 } }, tui::TableLayoutConfig::Cell{.coord{ 1, 0 } }, tui::TableLayoutConfig::Cell{.coord{ 2, 0 } } },
	.gap{ 1, 0 },
		}));

	const auto settingsPanel = std::make_shared<tui::Element>();
	const auto optionsPanel = std::make_shared<tui::Element>();
	const auto victoriesPanel = std::make_shared<tui::Element>();

	mSettingsPanel = settingsPanel;

	client->addChild(settingsPanel);
	client->addChild(optionsPanel);
	client->addChild(playerListPanel);

	settingsPanel->setLayout(std::make_unique<tui::TableLayout>(kNameValueTable));

	optionsPanel->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
		.axis = tui::EAxis::Vertical,
		.linesCrosswiseJustilign = tui::EJustilign::Stretch,
		}));

	victoriesPanel->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
		.axis = tui::EAxis::Vertical,
		.linesCrosswiseJustilign = tui::EJustilign::Stretch,
		}));

	auto gameNameRow = settingsPanel;//std::make_shared<tui::Element>();
	gameNameRow->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_GAME_NAME"))); // French text has colon...
	gameNameRow->addChild(mTxtGameName = std::make_shared<tui::Textbox>(initCore.getGameName()));
	//gameNameRow->setLayout(std::make_unique<tui::TableLayout>(kNameValueTable));
	//settingsPanel->addChild(gameNameRow);

	//settingsPanel->addChild(std::make_shared<tui::Label>());

	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_PLAYERS")));
	mLstNumPlayers = std::make_shared<MyComboBox>(*this, kSettingsComboboxStyle);
	mLstNumPlayers->setListItems(range(MAX_CIV_PLAYERS + 1)
		| std::views::transform([](int i) { return i <= 0 ? MyCvDLLUtility::getInstance().getText(L"TXT_KEY_WB_CITY_NOCHANGE") : std::to_wstring(i); })
		| std::ranges::to<std::vector>());
	mLstNumPlayers->setSelectionIndex(0);
	settingsPanel->addChild(mLstNumPlayers);

	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_PITBOSS_DIFFICULTY")));
	mLstDifficulty = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
	mLstDifficulty->setListItems(range(gGlobals.getNumHandicapInfos())
		| std::views::transform([](int i) -> std::wstring { return gGlobals.getHandicapInfo(static_cast<HandicapTypes>(i)).getDescription(); })
		| std::ranges::to<std::vector>());
	mLstDifficulty->setSelectionIndex(initCore.getHandicap(static_cast<PlayerTypes>(0)));
	settingsPanel->addChild(mLstDifficulty);

	settingsPanel->addChild(mLblMapSeed = std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_BUG_OPTTAB_MAP") + L' ' + static_cast<std::wstring>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAP_SCRIPT_RANDOM"))));
	settingsPanel->addChild(mTxtMapSeed = std::make_shared<tui::Textbox>(L"", std::to_wstring(initCore.getMapRandSeed()), tui::Colour{}, tui::Colour{}, tui::EColour::Silver));

	settingsPanel->addChild(mLblSyncSeed = std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_SHORTCUTS_GAME") + L' ' + static_cast<std::wstring>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAP_SCRIPT_RANDOM"))));
	settingsPanel->addChild(mTxtSyncSeed = std::make_shared<tui::Textbox>(L"", std::to_wstring(initCore.getSyncRandSeed()), tui::Colour{}, tui::Colour{}, tui::EColour::Silver));


	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_MAP")));
	mLstMapScript = std::make_shared<MyComboBox>(*this, kSettingsComboboxStyle);
	mLstMapScript->setListItems(mapScripts
		| std::views::transform([](const AppGameSetupMapScriptInfo& info) { return std::wstring(CvWString(info.moduleName)); })
		| std::ranges::to<std::vector>());
	// TODO: This should be case-insensitive to match VFS filename lookup.
	mLstMapScript->setSelectionIndex(std::ranges::find(mapScripts, CvString(initCore.getMapScriptName()), &AppGameSetupMapScriptInfo::moduleName) - mapScripts.begin());
	if (mLstMapScript->getSelectionIndex() >= mapScripts.size())
		mLstMapScript->setSelectionIndex(0);
	settingsPanel->addChild(mLstMapScript);

	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_SIZE")));

	auto worldSizePanel = std::make_shared<tui::Element>();

	mLstWorldSize = std::make_shared<MyComboBox>(*this, kSettingsComboboxStyle);
	std::vector<std::wstring> items;
	items.push_back(randomStr);
	items.append_range(range<WorldSizeTypes>(gGlobals.getNumWorldInfos())
		| std::views::transform([](WorldSizeTypes i) { return std::wstring(gGlobals.getWorldInfo(i).getDescription()); }));
	mLstWorldSize->setListItems(items);
	items.clear();
	mLstWorldSize->setSelectionIndex(initCore.getWorldSize() + 1);

	items.append_range(range(1, 10 + 1)
		| std::views::transform([](int i) { return std::to_wstring(i) + L'x'; }));
	mLstWorldSizeMultiplier = std::make_shared<MyComboBox>(*this, kSettingsComboboxStyle);
	mLstWorldSizeMultiplier->setListItems(items);
	items.clear();
	mLstWorldSizeMultiplier->setSelectionIndex(initCore.getWorldSizeMultiplier() - 1);

	worldSizePanel->addChild(mLstWorldSize);
	worldSizePanel->addChild(mLstWorldSizeMultiplier);
	worldSizePanel->setLayout(std::make_unique<tui::TableLayout>(tui::TableLayoutConfig{
		.cols{ tui::TableLayoutConfig::kFlex, tui::TableLayoutConfig::kAutoSize },
		.rows{ tui::TableLayoutConfig::kAutoSize },
		.cells{ tui::TableLayoutConfig::Cell{.coord{ 0, 0 } }, tui::TableLayoutConfig::Cell{.coord{ 1, 0 } } },
		.gap{ 0, 0 },
		}));
	settingsPanel->addChild(worldSizePanel);

	//const MapScriptInfo& selMapScriptInfo = mapScripts[mLstMapScript->getSelectionIndex()];
	//if (selMapScriptInfo.isClimateMap())
	{
		settingsPanel->addChild(mLblClimate = std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_CLIMATE")));
		mLstClimate = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
		items.push_back(randomStr);
		items.append_range(range<ClimateTypes>(gGlobals.getNumClimateInfos())
			| std::views::transform([](ClimateTypes i) { return std::wstring(gGlobals.getClimateInfo(i).getDescription()); }));
		mLstClimate->setListItems(items);
		items.clear();
		mLstClimate->setSelectionIndex(initCore.getClimate() + 1);
		settingsPanel->addChild(mLstClimate);
	}
	//if (selMapScriptInfo.isSeaLevelMap())
	{
		settingsPanel->addChild(mLblSeaLevel = std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_SEALEVEL")));
		mLstSeaLevel = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
		items.push_back(randomStr);
		items.append_range(range<SeaLevelTypes>(gGlobals.getNumSeaLevelInfos())
			| std::views::transform([](SeaLevelTypes i) { return std::wstring(gGlobals.getSeaLevelInfo(i).getDescription()); }));
		mLstSeaLevel->setListItems(items);
		items.clear();
		mLstSeaLevel->setSelectionIndex(initCore.getSeaLevel() + 1);
		settingsPanel->addChild(mLstSeaLevel);
	}

	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_ERA")));
	mLstEra = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
	items.push_back(randomStr);
	items.append_range(range<EraTypes>(gGlobals.getNumEraInfos())
		| std::views::transform([](EraTypes i) { return std::wstring(gGlobals.getEraInfo(i).getDescription()); }));
	mLstEra->setListItems(items);
	items.clear();
	mLstEra->setSelectionIndex(initCore.getEra() + 1);
	settingsPanel->addChild(mLstEra);

	mGameSpeedOrder.assign_range(range<GameSpeedTypes>(gGlobals.getNumGameSpeedInfos()));
	std::ranges::stable_sort(mGameSpeedOrder, std::less<>(), [](GameSpeedTypes i) {
		int iEstimateEndTurn = 0;
		for (int iI = 0, n = gGlobals.getGameSpeedInfo(i).getNumTurnIncrements(); iI < n; iI++)
			iEstimateEndTurn += gGlobals.getGameSpeedInfo(i).getGameTurnInfo(iI).iNumGameTurnsPerIncrement;
		return iEstimateEndTurn;
		});

	settingsPanel->addChild(std::make_shared<tui::Label>(MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MENU_SPEED")));
	mLstSpeed = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
	items.append_range(mGameSpeedOrder |
		std::views::transform([](GameSpeedTypes i) { return std::wstring(gGlobals.getGameSpeedInfo(i).getDescription()); }));
	mLstSpeed->setListItems(items);
	items.clear();
	{
		size_t selIndex = std::ranges::find(mGameSpeedOrder, initCore.getGameSpeed()) - mGameSpeedOrder.begin();
		if (selIndex >= mGameSpeedOrder.size())
			selIndex = static_cast<GameSpeedTypes>(gGlobals.getDefineINT("STANDARD_GAMESPEED"));
		mLstSpeed->setSelectionIndex(selIndex);
	}
	settingsPanel->addChild(mLstSpeed);

	onMapScriptChanged();



	for (const auto i : range<GameOptionTypes>(gGlobals.getNumGameOptionInfos()))
	{
		auto chk = std::make_shared<MyCheckBox>(*this, tui::ECheckStyle::AsciiX, gGlobals.getGameOptionInfo(i).getDescription());
		chk->value = initCore.getOption(i);
		optionsPanel->addChild(chk);
		mChkGameOptions.push_back(chk);
	}

	optionsPanel->addChild(std::make_shared<tui::HorizontalRule>(tui::EBorderStyle::Thin));
	optionsPanel->addChild(victoriesPanel);

	for (const auto i : range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
	{
		auto chk = std::make_shared<tui::Checkbox>(tui::ECheckStyle::AsciiX, gGlobals.getVictoryInfo(i).getDescription());
		chk->value = initCore.getVictory(i);
		victoriesPanel->addChild(chk);
		mChkVictoryOptions.push_back(chk);
	}

	playerListPanel->onUnrestrictedLeadersChanged(initCore.getOption(GAMEOPTION_LEAD_ANY_CIV));
}

// Returns nullopt if unknown.
std::optional<int> AppGameSetupConfigPanel::getNumPlayers() const
{
	if (mLstNumPlayers->getSelectionIndex() > 0)
		return static_cast<int>(mLstNumPlayers->getSelectionIndex());
	else if (mLstWorldSize->getSelectionIndex() > 0)
		return gGlobals.getWorldInfo(static_cast<WorldSizeTypes>(mLstWorldSize->getSelectionIndex() - 1)).getDefaultPlayers();
	else
		return std::nullopt;
}

void AppGameSetupConfigPanel::setupConfig(const std::vector<AppGameSetupMapScriptInfo>& mapScripts, SimplifiedInitCore& config) const
{
	config.gameName = mTxtGameName->getText();

	config.difficulty = static_cast<HandicapTypes>(mLstDifficulty->getSelectionIndex());
	config.mapSeed = strictStringToUInt(mTxtMapSeed->getText(), mLblMapSeed->getLabel());
	config.syncSeed = strictStringToUInt(mTxtSyncSeed->getText(), mLblSyncSeed->getLabel());
	config.mapScriptName = CvWString(mapScripts[mLstMapScript->getSelectionIndex()].moduleName);
	config.worldSizeType = static_cast<WorldSizeTypes>(mLstWorldSize->getSelectionIndex() - 1);
	config.mapSizeOverrideMultiplier = static_cast<int>(mLstWorldSizeMultiplier->getSelectionIndex() + 1);
	config.climateType = static_cast<ClimateTypes>(mLstClimate->getSelectionIndex() - 1);
	config.seaLevelType = static_cast<SeaLevelTypes>(mLstSeaLevel->getSelectionIndex() - 1);
	config.eraType = static_cast<EraTypes>(mLstEra->getSelectionIndex() - 1);
	config.speedType = mGameSpeedOrder[static_cast<GameSpeedTypes>(mLstSpeed->getSelectionIndex())];
	config.customMapOptions.resize(mCustomOptions.size());
	for (const size_t i : range(mCustomOptions.size()))
		config.customMapOptions[i] = static_cast<CustomMapOptionTypes>(mCustomOptions[i].lst->getSelectionIndex() - mCustomOptions[i].isRandomisable);

	config.gameOptions.assign_range(mChkGameOptions | std::views::transform(&tui::Checkbox::value));
	config.victoryOptions.assign_range(mChkVictoryOptions | std::views::transform(&tui::Checkbox::value));
}

void AppGameSetupConfigPanel::addCustomMapOptionControls(const AppGameSetupMapScriptInfo& mapScript)
{
	const CvInitCore& initCore = gGlobals.getInitCore();

	const CvWString randomStr = MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_RANDOM");
	std::vector<std::wstring> items;
	for (const int i : range(initCore.getNumCustomMapOptions()))
	{
		auto lbl = std::make_shared<tui::Label>(mapScript.getCustomMapOptionName(i));
		mSettingsPanel->addChild(lbl);
		auto lst = std::make_shared<tui::Combobox>(kSettingsComboboxStyle);
		const bool isRandomisable = mapScript.isRandomisableCustomOption(i);
		if (isRandomisable)
			items.push_back(randomStr);
		items.append_range(range<CustomMapOptionTypes>(mapScript.getNumCustomMapOptionValues(i))
			| std::views::transform([&](CustomMapOptionTypes choice) { return mapScript.getCustomMapOptionChoiceName(i, choice); }));
		lst->setListItems(items);
		items.clear();
		lst->setSelectionIndex(initCore.getCustomMapOption(i) + isRandomisable);
		mSettingsPanel->addChild(lst);
		mCustomOptions.emplace_back(lbl, lst, isRandomisable);
	}
}

void AppGameSetupConfigPanel::onComboBoxSelectionChanged(tui::Combobox& ctrl)
{
	if (&ctrl == mLstWorldSize.get())
		mSinglePlayerCustomWindow.onPlayerCountChanged();
	if (&ctrl == mLstNumPlayers.get())
		mSinglePlayerCustomWindow.onPlayerCountChanged();
	if (&ctrl == mLstMapScript.get())
		onMapScriptChanged();
}

void AppGameSetupConfigPanel::onCheckBoxValueChanged(tui::Checkbox& chk)
{
	if (&chk == mChkGameOptions[GAMEOPTION_LEAD_ANY_CIV].get())
		mSinglePlayerCustomWindow.onUnrestrictedLeadersOptionChanged(chk.value);
}

void AppGameSetupConfigPanel::onMapScriptChanged()
{
	const AppGameSetupMapScriptInfo& selMapScriptInfo = mSinglePlayerCustomWindow.getMapScripts()[mLstMapScript->getSelectionIndex()];
	gGlobals.getInitCore().setMapScriptName(CvWString(selMapScriptInfo.moduleName));
	const bool hasClimate = selMapScriptInfo.isClimateMap();
	const bool hasSeaLevel = selMapScriptInfo.isSeaLevelMap();
	mLblClimate->setVisible(hasClimate);
	mLstClimate->setVisible(hasClimate);
	mLblSeaLevel->setVisible(hasSeaLevel);
	mLstSeaLevel->setVisible(hasSeaLevel);
	for (const auto& info : mCustomOptions)
	{
		info.lbl->orphan();
		info.lst->orphan();
	}
	mCustomOptions.clear();
	addCustomMapOptionControls(selMapScriptInfo);
}