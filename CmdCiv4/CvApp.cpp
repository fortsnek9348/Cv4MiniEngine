#include "CvApp.h"
#include "AppMainMenusState.h"
#include "CommandLine.h"
#include "FileDialogs.h"

#include <Cv4CommonEngineLib/Common.h>
#include <Cv4CommonEngineLib/CivIni.h>
#include <Cv4CommonEngineLib/CvVFS.h>
#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

#include <CvGlobals.h>
#include <CvInitCore.h>
#include <CvGameAI.h>
#include <CvMap.h>
#include <CvTeamAI.h>
#include <CvPlayerAI.h>
#include <CvEventReporter.h>
#include <CyArgsList.h>

#include <PlayerBotGameBinding/IPlayerBotPlugin.h>

#include <HeckTextUI/Core.h>

#include <CommonStuff/System.h>
#include <CommonStuff/range.h>
#include <CommonStuff/DynamicLib.h>

#include <iostream>
#include <chrono>

using heck::range;
using namespace cvengine;

CvApp& CvApp::getInstance()
{
	static CvApp x;
	return x;
}

CvApp::CvApp()
{
}

void CvApp::start(const AppStartupConfig& config)
{
	mCmdLineConfig = config;

	std::clog << "User config directory: " << getUserConfigDir() << std::endl;
	std::clog << "User data directory: " << getUserDataDir() << std::endl;
	std::clog << "User cache directory: " << getUserCacheDir() << std::endl;

	std::clog << "Creating directories..." << std::endl;
	std::filesystem::create_directories(getUserDataDir() / kSavesDirName / kSavesSingleDirName / kSavesAutoDirName);
	std::filesystem::create_directories(getUserDataDir() / kSavesDirName / kSavesSingleDirName);
	std::filesystem::create_directories(getUserDataDir() / kReplaysDirName);
	std::filesystem::create_directories(getUserConfigDir() / kCustomAssetsDirName);
	std::filesystem::create_directories(getUserConfigDir() / kPublicMapsDirName);

	loadCivIni();

	// Only have to do this before first drawing the UI.
	redirectLoggingOutput();

	std::filesystem::path vanillaCiv4RootDir = gCivilizationIVIni.get(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, L"");
	if (vanillaCiv4RootDir.empty())
	{
		std::cout << "First start-up. Specify the root of your Civilization 4 installation." << std::endl;

		for (;;)
		{
			if (auto optPath = promptDirectoryPath())
			{
				vanillaCiv4RootDir = std::move(optPath.value());

				const std::filesystem::path checkPath = std::filesystem::path("Beyond the Sword") / "Civ4BeyondSword.exe";
				if (!std::filesystem::exists(vanillaCiv4RootDir / checkPath))
					std::cout << "Unrecognised directory. It should contain \"" + checkPath.string() + "\"." << std::endl;
				else
					break;
			}
			else
			{
				std::cout << "Cancelled." << std::endl;
				std::exit(EXIT_FAILURE);
			}
		}
		gCivilizationIVIni.set(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, vanillaCiv4RootDir.wstring());
	}

	gCivilizationIVIni.setDefault(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_EnableRightClickToShiftClickRemap, "1");

	audioSystem = std::make_unique<AudioSystem>();

	loadEnhancedDLLConfigFromINI();

	const std::filesystem::path dataDir = heck::findEnvironmentVariable(L"CV4MINIENGINE_DATADIR").value_or(L"");
	//std::cout << "Building VFS file catalog with CV4MiniEngine data directory " << dataDir << std::endl;
	std::cout << "Initialising VFS with Cv4MiniEngine data directory " << dataDir << std::endl;
	// File cataloging is ~3x slower.
	const auto t0 = std::chrono::steady_clock::now();
	mVFS = std::make_unique<CvVFS>(dataDir, vanillaCiv4RootDir, std::filesystem::path(config.modRelPath));
	const auto t1 = std::chrono::steady_clock::now();
	std::clog << "VFS initialisation took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
	//mVFS = std::make_unique<CvVFS>(dataDir, vanillaCiv4RootDir, L"Next War\\");
	gVFS = mVFS.get();

	// Add defaults.
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_MapRandSeed, 0);
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_SyncRandSeed, 0);
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_CheatCode, "");

	saveCivilizationIniIfChanged();

	audioSystem->loadXmlDefs();

	// Load player bot DLL.
	if (!config.botPath.empty())
	{
		heck::DynamicLibrary lib(config.botPath.c_str());

		mPlayerBotPlugin = &reinterpret_cast<const cvbot::IPlayerBotPlugin&(*)()>(lib.resolve("getPlayerBotPlugin"))();

		if (mPlayerBotPlugin->getModName() != gVFS->getModName(false))
			throw std::runtime_error("Bot requires mod '" + heck::convertWideToUtf8(mPlayerBotPlugin->getModName()) + "'.");
	}
}

CvApp::~CvApp()
{
	// Clear this pointer out.
	gVFS = nullptr;
}


void CvApp::redirectLoggingOutput()
{
	initDebugOutput();
}


const cvengine::CvVFS& CvApp::getVFS() const
{
	return *mVFS;
}

const cvengine::AppStartupConfig& CvApp::getCommandLineConfig() const
{
	return mCmdLineConfig;
}

#if ENABLE_PLAYER_BOT
const cvbot::IPlayerBotPlugin* engine_specific::getPlayerBotPlugin()
{
	return CvApp::getInstance().getPlayerBotPlugin();
}

const cvbot::IPlayerBotPlugin* CvApp::getPlayerBotPlugin() const
{
	return mPlayerBotPlugin;
}
#endif

ICvAppUI& CvApp::getUI() noexcept
{
	return *mAppUI;
}

bool CvApp::isShiftClickHackEnabled() const
{
	return mIsShiftClickHackEnabled;
}

bool engine_specific::isShiftDown()
{
	return CvApp::getInstance().getLastModifierKeysState().shift;
}
bool engine_specific::isAltDown()
{
	return CvApp::getInstance().getLastModifierKeysState().alt;
}
bool engine_specific::isCtrlDown()
{
	return CvApp::getInstance().getLastModifierKeysState().ctrl;
}

hecktui::ModifierKeyState CvApp::getLastModifierKeysState() const noexcept
{
	return mAppUI ? mAppUI->getLastModifierKeysState() : hecktui::ModifierKeyState{};
}

void CvApp::deferAppState(std::unique_ptr<ICvAppState> state)
{
	std::swap(mNextState, state);
}

void CvApp::deferMainMenu()
{
	deferAppState(std::make_unique<AppMainMenusState>());
}

void CvApp::deferLoadGame(const std::filesystem::path& path)
{
	deferAppState(std::make_unique<LoadGameCvAppState>(path));
}

void CvApp::deferNewGameStartup(cvengine::app::SimplifiedInitCore config)
{
	deferAppState(std::make_unique<NewGameStartupCvAppState>(std::move(config)));
}

void CvApp::deferInGameUI()
{
	deferAppState(std::make_unique<InGameCvAppState>());
}

void engine_specific::exitApp()
{
	CvApp::getInstance().setWantExit();
}
bool engine_specific::isAppExiting()
{
	return CvApp::getInstance().isExiting();
}

void CvApp::setWantExit()
{
	mWantExit = true;
}
bool CvApp::isExiting() const
{
	return mWantExit;
}

int CvApp::run()
{
	while (!mWantExit)
	{
		while (mNextState)
		{
			if (mCurrentState)
				mCurrentState->onLeave(*this);
			mAppUI->resetUI();
			mCurrentState = std::move(mNextState);
			mCurrentState->onEnter(*this);
		}

		mCurrentState->onUpdate(*this);

		CvApp::getInstance().getUI().updateUI();
	}

	return 0;
}
