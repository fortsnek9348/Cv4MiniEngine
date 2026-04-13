#include "AppMainMenusState.h"
#include "AppGameSetupWindow.h"
#include "Common.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "VersionInfo.h"
#include "CvVFS.h"

#include <CvGlobals.h>
#include <CvDLLPythonIFaceBase.h>

#include <PlayerBotGameBinding/IPlayerBotPlugin.h>

namespace tui = hecktui;

namespace
{
	class AboutWindow : public hecktui::Window
	{
	public:
		AboutWindow()
			: Window{ MyCvDLLUtility::getInstance().getText(L"TXT_KEY_PITBOSS_ABOUT"), tui::WindowConfig{
				.isDefaultFocus = true,
				.isFullscreen = false,
				.isModal = true,
				.canClose = true,
				.allowSceneToCloseWindow = true,
				.borderStyle = tui::EBorderStyle::Rounded,
			} }
		{
			auto client = getClientArea();
			client->setLayout(std::make_unique<tui::FillLayout>());

			std::wstringstream text;
			text << MyCvDLLUtility::getInstance().getText(L"TXT_KEY_MAIN_MENU_SAVE_VERSION") << L' ' << gGlobals.getDefineINT("SAVE_VERSION") << L'\n';

			text << MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_BUILD_VERSION", {}) + L' ' + std::wstring(cvengine::kEngineVersionString) << L'\n';
			for (const std::string_view s : getCvGameCoreDLLConfigStrings())
				text << CvWString(CvString(s)) << L'\n';
			
			text << L"MAX_CIV_PLAYERS: " << gGlobals.getMAX_CIV_PLAYERS() << L'\n';

			text << L'\n';
			text << L"Engine and DLL alterations copyright © 2026 fortsnek/snowern\n";
			text << L"Licenced under GPLv3. Source code available on GitHub.\n";
			text << L"Based on original DLL source code from Civilization IV Beyond the Sword by Firaxis.\n";
			text << L'\n';
			text << L"Libraries used:\n";
			text << L"    SFML 3: https://www.sfml-dev.org/\n";
			text << L"    pybind11: https://github.com/pybind/pybind11/releases/tag/v2.9.2\n";
			text << L"    Python 2.7: https://www.python.org/ftp/python/2.7.18/\n";
			text << L"    pugixml: https://github.com/zeux/pugixml\n";
			text << L"    STB DXT compression: https://github.com/nothings/stb/tree/master\n";
			text << L"    DXT decompression: https://github.com/Benjamin-Dobell/s3tc-dxt-decompression\n";
			text << L"    Save file compression: https://zlib.net/\n";
#if !CV4MINIENGINE_USE_CONSOLE_FILE_BROWSER
			text << L"    Native file dialogs: https://github.com/btzy/nativefiledialog-extended\n";
#endif

			client->addChild(std::make_shared<tui::Label>(text.str()));
		}
	};

	class BackgroundWindow : public tui::Window
	{
	public:
		BackgroundWindow(const CvApp& app) : Window{ L"", tui::WindowConfig{
			.isDefaultFocus = false,
			.isFullscreen = true,
			.isModal = false,
			.canClose = false,
			.borderStyle = tui::EBorderStyle::None,
			} }
		{
			const auto client = getClientArea();

			// https://patorjk.com/software/taag/#p=display&f=Standard&t=Cv4MiniEngine
			client->addChild(std::make_shared<tui::Label>(LR"(
  ____       _  _   __  __ _       _ _____             _            
 / ___|_   _| || | |  \/  (_)_ __ (_) ____|_ __   __ _(_)_ __   ___ 
| |   \ \ / / || |_| |\/| | | '_ \| |  _| | '_ \ / _` | | '_ \ / _ \
| |___ \ V /|__   _| |  | | | | | | | |___| | | | (_| | | | | |  __/
 \____| \_/    |_| |_|  |_|_|_| |_|_|_____|_| |_|\__, |_|_| |_|\___|
                                                 |___/              
)"));
			std::wstring buildString;
			buildString = MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_BUILD_VERSION", {}) + L' ' + std::wstring(cvengine::kEngineVersionString);
			for (const std::string_view s : getCvGameCoreDLLConfigStrings())
			{
				buildString += L' ';
				buildString += CvWString(CvString(s));
			}
			if (const std::wstring s = app.getVFS().getModName(false); !s.empty())
			{
				buildString += L'\n';
				buildString += L"Mod: " + s;
			}
			if (const cvbot::IPlayerBotPlugin* const plugin = app.getPlayerBotPlugin())
			{
				buildString += L'\n';
				buildString += L"Bot: " + plugin->getName();
			}
			auto lblBuildString = std::make_shared<tui::Label>(buildString);
			lblBuildString->enableWrapping = true;
			lblBuildString->setLabelAlignment(tui::EAlign::Center);
			client->addChild(lblBuildString);

			auto menuBorderPanel = std::make_shared<tui::BoxPanel>(tui::EBorderStyle::None);
			auto menuPanel = std::make_shared<tui::Element>();

			menuBorderPanel->addChild(menuPanel);
			auto layout = std::make_unique<tui::FillLayout>();
			layout->marginTopLeft = layout->marginBottomRight = 1;
			menuBorderPanel->setLayout(std::move(layout));

			static constexpr const wchar_t* kPadding = L"  ";

			menuPanel->addChild(std::make_shared<tui::Button>(kPadding + MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_NEW_GAME", {}) + kPadding, [] {
				CvApp::getInstance().getUI().pushWindow(std::make_shared<AppGameSetupWindow>());
				}));
			menuPanel->addChild(std::make_shared<tui::Button>(kPadding + MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_LOAD_GAME", {}) + kPadding, [] {
				MyCvDLLUtility::getInstance().LoadGame();
				}));
			menuPanel->addChild(std::make_shared<tui::Button>(kPadding + MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_OPTIONS", {}) + kPadding, [] {
				gDLL->getPythonIFace()->callFunction("CvScreensInterface", "showOptionsScreen");
				}));
			menuPanel->addChild(std::make_shared<tui::Button>(kPadding + MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_PITBOSS_ABOUT", {}) + kPadding, [] {
				CvApp::getInstance().getUI().pushWindow(std::make_shared<AboutWindow>());
				}));
			menuPanel->addChild(std::make_shared<tui::Button>(kPadding + MyCvDLLUtility::getInstance().getTextGeneric(L"TXT_KEY_MAIN_MENU_EXIT_GAME", {}) + kPadding, [] {
				MyCvDLLUtility::getInstance().isDone = true;
				}));
			menuPanel->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
				.axis = tui::EAxis::Vertical,
				.itemsCrosswiseJustilign = tui::EJustilign::Stretch,
				.linesCrosswiseJustilign = tui::EJustilign::Stretch,
				}));

			client->addChild(menuBorderPanel);

			client->setLayout(std::make_unique<tui::FlowLayout>(tui::FlowConfig{
				.axis = tui::EAxis::Vertical,
				.itemsFlowwiseJustilign = tui::EJustilign::Center,
				.itemsCrosswiseJustilign = tui::EJustilign::Center,
				.linesCrosswiseJustilign = tui::EJustilign::Stretch,
				}));
		}
	};
}

void AppMainMenusState::onEnter(CvApp& app)
{
	app.getUI().pushWindow(std::make_shared<BackgroundWindow>(app));
}
void AppMainMenusState::onUpdate(CvApp&)
{
}
void AppMainMenusState::onLeave(CvApp&)
{
}