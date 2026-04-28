#include "AppGameSetupWindow.h"
#include "AppGameSetupPlayerListPanel.h"
#include "AppGameSetupConfigPanel.h"
#include "CvApp.h"

#include <Cv4CommonEngineLib/CvVFS.h>
#include <Cv4CommonEngineLib/CivIni.h>
#include <Cv4CommonEngineLib/CvTranslator.h>

#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>

#include <CommonStuff/System.h>
#include <CommonStuff/range.h>

#include <random>
#include <ranges>

namespace tui = hecktui;
using heck::range;
using namespace cvengine;

static std::vector<AppGameSetupMapScriptInfo> enumerateMapScripts(const CvVFS& vfs)
{
	// If the map script doesn't exist, just pick anything.
	const auto files = vfs.enumeratePhysExtNonRecursive(L"", L".py");
	if (files.empty())
		throw std::runtime_error("There are no map scripts!");

	std::vector<AppGameSetupMapScriptInfo> v(std::from_range, files | std::views::transform([](const std::filesystem::path& path) {
		AppGameSetupMapScriptInfo info{
			.moduleName = path.stem().string()
		};
		//CvWString descTag;
		//if (gDLL->getPythonIFace()->callFunction(info.moduleName.c_str(), "getDescription", nullptr, &descTag))
		//	info.descName = CvTranslator::getInstance().getTextGeneric(descTag, {});
		//else
		//	info.descName = CvWString(info.moduleName);
		return info;
		}));

	std::ranges::stable_sort(v, std::less<>(), &AppGameSetupMapScriptInfo::moduleName);

	return v;
}

static app::SimplifiedInitCore loadInitCoreFromIni(const std::vector<AppGameSetupMapScriptInfo>& mapScripts)
{
	app::SimplifiedInitCore setup = app::getGameSetupFromIni();

	if (!gDLL->pythonMapExists(setup.mapScriptName.c_str()))
	{
		// If the map script doesn't exist, just pick anything.
		setup.mapScriptName = mapScripts.front().moduleName;
	}

	return setup;
}

AppGameSetupWindow::AppGameSetupWindow(CvApp& app)
	: Window{ L"", tui::WindowConfig{
		.isDefaultFocus = true,
		.isFullscreen = true,
		.isModal = true,
		.canClose = true,
		.borderStyle = tui::EBorderStyle::None,
	} }
	, mMapScriptsList(enumerateMapScripts(app.getVFS()))
{
	const app::SimplifiedInitCore initialSetup = loadInitCoreFromIni(mMapScriptsList);
	//loadInitCoreFromIni(gGlobals.getIniInitCore(), mMapScriptsList);
	//CvInitCore& initCore = gGlobals.getInitCore();
	//initCore.resetGame(&gGlobals.getIniInitCore(), true, false);
	//initCore.resetPlayers(&gGlobals.getIniInitCore(), true, false);

	const auto client = getClientArea();

	//auto tabBar = std::make_shared<tui::Element>();
	//tabBar->addChild(mTabOptions = std::make_shared<tui::Checkbox>(tui::ECheckStyle::Border, CvTranslator::getInstance().getText(L"TXT_KEY_OPTIONS_TITLE"), [this](bool) { updateTabs(0); }));
	//tabBar->addChild(mTabPlayers = std::make_shared<tui::Checkbox>(tui::ECheckStyle::Border, CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_PLAYERS"), [this](bool) { updateTabs(1); }));
	//tabBar->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{ .axis = tui::EAxis::Horizontal }));
	//
	//mTabOptions->setIsRadioButton(true);
	//mTabPlayers->setIsRadioButton(true);

	client->addChild(std::make_shared<tui::Label>(CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_CUSTOM_GAME")));
	client->addChild(std::make_shared<tui::HorizontalRule>(tui::EBorderStyle::Thick));
	mPlayerListPanel = std::make_shared<AppGameSetupPlayerListPanel>(initialSetup);
	mGameSetupBottomPanel = std::make_shared<AppGameSetupConfigPanel>(initialSetup, mMapScriptsList, *this, mPlayerListPanel);

	//client->addChild(mPlayerListPanel);
	client->addChild(mGameSetupBottomPanel);
	//client->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
	//	.axis = tui::EAxis::Vertical,
	//	.linesCrosswiseJustilign = tui::EJustilign::Stretch,
	//	}));

	client->addChild(std::make_shared<tui::Button>(CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_GO_BACK"), [&app, this] {
		app.getUI().removeWindow(this);
		}));

	client->addChild(std::make_shared<tui::Button>(CvTranslator::getInstance().getText(L"TXT_KEY_MAIN_MENU_LAUNCH"), [this] {
		launch();
		}));

	//updateTabs(0);

	//client->addChild(mPlayerListPanel = std::make_shared<PlayerListPanel>());
	//client->addChild(mGameSetupBottomPanel = std::make_shared<GameSetupBottomPanel>(mMapScriptsList));
	client->setLayout(std::make_unique<tui::TableLayout>(tui::TableLayoutConfig{
		.cols{ tui::TableLayoutConfig::kFlex },
		.rows{
			tui::TableLayoutConfig::kAutoSize,
			tui::TableLayoutConfig::kAutoSize,
			{.weight = 1 },
			/*{.weight = 2 }, */
			tui::TableLayoutConfig::kAutoSize
		},
		.cells{
			tui::TableLayoutConfig::Cell{.coord{ 0, 0 }, .align{ tui::EJustilign::Center, tui::EJustilign::Stretch } },
			tui::TableLayoutConfig::Cell{.coord{ 0, 1 } },
			tui::TableLayoutConfig::Cell{.coord{ 0, 2 } },
			//tui::TableLayoutConfig::Cell{.coord{ 0, 1 } },
			tui::TableLayoutConfig::Cell{.coord{ 0, 3 }, .align{ tui::EJustilign::Start, tui::EJustilign::Stretch } },
			tui::TableLayoutConfig::Cell{.coord{ 0, 3 }, .align{ tui::EJustilign::End, tui::EJustilign::Stretch } },
		},
		.gap{ 0, 0 },
		}));

	onPlayerCountChanged();
}

void AppGameSetupWindow::drawThis(tui::ivec2 offset, tui::Framebuffer& fb)
{
	fb.drawFilledBox(offset + getRect(), { .c = L' ', .colour{.back = tui::EColour::Black } });
	tui::Window::drawThis(offset, fb);
}

void AppGameSetupWindow::onPlayerCountChanged()
{
	mPlayerListPanel->onPlayerCountChanged(mGameSetupBottomPanel->getNumPlayers().value_or(1));
}

void AppGameSetupWindow::onUnrestrictedLeadersOptionChanged(bool value)
{
	mPlayerListPanel->onUnrestrictedLeadersChanged(value);
}

const std::vector<AppGameSetupMapScriptInfo>& AppGameSetupWindow::getMapScripts() const
{
	return mMapScriptsList;
}

void AppGameSetupWindow::launch()
{
	app::SimplifiedInitCore config;
	mGameSetupBottomPanel->setupConfig(mMapScriptsList, config);
	mPlayerListPanel->setupConfig(config);

	CvApp::getInstance().deferNewGameStartup(std::move(config));
}