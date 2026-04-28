#include "CvApp.h"
#include "AppMainMenusState.h"
#include "CommandLine.h"
#include "Cv4MiniEngineIni.h"
#include "FileDialogs.h"
#include "CvTuiInterface.h"
#include "CvTuiEngine.h"
#include "OutputRedirection.h"
#include "DLLInterface/MyCvDLLEntityIFace.h"
#include "DLLInterface/MyCvDLLFeature.h"
#include "DLLInterface/MyCvDLLFlagEntity.h"
#include "DLLInterface/MyCvDLLPlotBuilder.h"

#include <Cv4CommonEngineLib/CivIni.h>
#include <Cv4CommonEngineLib/CvVFS.h>
#include <Cv4CommonEngineLib/CommonEngine.h>

#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvTeamAI.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CyArgsList.h>
#include <CvGameCoreDLL/GeneratePlayerBotHeader.h>

#include <HeckTextUI/Core.h>

#include <CommonStuff/System.h>
#include <CommonStuff/range.h>
#include <CommonStuff/DynamicLib.h>

#include <pybind11/pybind11.h>

#include <iostream>
#include <chrono>

using heck::range;
using namespace cvengine;

CvApp& CvApp::getInstance()
{
	static CvApp x;
	return x;
}

std::filesystem::path CvApp::getUserDataDir()
{
	return heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Data);
}

void CvApp::start(const AppStartupConfig& config)
{
	mCmdLineConfig = config;

	const std::filesystem::path userDataDirPath = getUserDataDir();
	const std::filesystem::path userConfigDirPath = heck::getUserGamesSpecialDirectory("Beyond the Sword", heck::EUserGamesSpecialDirectory::Config);
	const std::filesystem::path cacheDirPath = heck::getUserGamesSpecialDirectory("Cv4MiniEngine", heck::EUserGamesSpecialDirectory::Cache);
	const std::filesystem::path replaysDirName = L"Replays (Cv4MiniEngine)";
	const std::filesystem::path savesDirName = L"Saves (Cv4MiniEngine)";
	const std::filesystem::path iniFilename = L"Cv4MiniEngine.ini";
	const std::filesystem::path profileFilename = L"Cv4MiniEngine Profile.txt";

	const std::filesystem::path iniPath = userConfigDirPath / iniFilename;
	IniData::createIfNew(iniPath);
	IniData ini = loadINI(iniPath);

	// Only have to do this before first drawing the UI.
	initDebugOutput(ini);

	std::filesystem::path vanillaCiv4RootDir = ini.get(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, L"");
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
		ini.set(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_VanillaCiv4RootDir, vanillaCiv4RootDir.wstring());
	}

	ini.setDefault(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_EnableRightClickToShiftClickRemap, "1");

	if (ini.isChanged())
		saveINI(iniPath, ini);

	const std::filesystem::path dataDir = heck::findEnvironmentVariable(L"CV4MINIENGINE_DATADIR").value_or(L"");

	// Load player bot DLL.
	if (!config.botPath.empty())
	{
		if (!hasCvGameCoreDLLPlayerBotSupport())
			throw std::runtime_error("Bot specified, but current CvGameCoreDLL not compiled with bot support.");

		heck::DynamicLibrary lib(config.botPath.c_str());

		mPlayerBotPlugin = reinterpret_cast<const cvbot::IPlayerBotPlugin*(*)()>(lib.resolve("getPlayerBotPlugin"))();
	}

	
	const CommonEngineInitResult initResult = initialiseCommonEngine(std::wcout, CommonEngineConfig{
		.civ4InstallationRoot = vanillaCiv4RootDir,
		.optModRelPath = !config.modRelPath.empty() ? std::optional(config.modRelPath) : std::nullopt,
		.optEngineAssetsOverrideDir = dataDir,
		.userDataDirPath = userDataDirPath,
		.userConfigDirPath = userDataDirPath,
		.cacheDirPath = cacheDirPath,
		.replaysDirName = replaysDirName,
		.savesDirName = savesDirName,
		.iniFilename = iniFilename,
		.profileFilename = profileFilename,
		.interface = &CvTuiInterface::getInstance(),
		.engine = &CvTuiEngine::getInstance(),
		.entityIFace = &MyCvDLLEntityIFace::gInstance,
		.symbolIFace = &MyCvDLLSymbol::gInstance,
		.featureIFace = &MyCvDLLFeature::gInstance,
		.routeIFace = &MyCvDLLRoute::gInstance,
		.plotBuilderIFace = &MyCvDLLPlotBuilder::gInstance,
		.riverIFace = &MyCvDLLRiver::gInstance,
		.flagEntityIFace = &MyCvDLLFlagEntity::gInstance,
		.optPlayerBotPlugin = mPlayerBotPlugin,
		.callbackHandler = this,
		});

	mVFS = initResult.vfs;

	// -generate_player_bot_game_binding "..\PlayerBotGameBinding"
	if (!config.generatePlayerBotGameDefsDir.empty())
	{
		cvbot::generatePlayerBotGameBindingHeaders(std::filesystem::path(config.generatePlayerBotGameDefsDir));
	}

	audioSystem = std::make_unique<AudioSystem>(*mVFS);

	// Patch scripts.
	(void)pybind11::module::import("Cv4MiniEngineEntryPoint").attr("init")();
}

const cvengine::CvVFS& CvApp::getVFS() const
{
	return *mVFS;
}

const cvengine::AppStartupConfig& CvApp::getCommandLineConfig() const
{
	return mCmdLineConfig;
}

const cvbot::IPlayerBotPlugin* CvApp::getPlayerBotPlugin() const
{
	return mPlayerBotPlugin;
}

ICvAppUI& CvApp::getUI() noexcept
{
	return *mAppUI;
}

bool CvApp::isShiftClickHackEnabled() const
{
	return mIsShiftClickHackEnabled;
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

void CvApp::deferNewGameStartup(cvengine::app::SimplifiedInitCore config)
{
	deferAppState(std::make_unique<NewGameStartupCvAppState>(std::move(config)));
}

void CvApp::deferInGameUI()
{
	deferAppState(std::make_unique<InGameCvAppState>());
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

bool CvApp::isShiftDown() const
{
	return getLastModifierKeysState().shift;
}
bool CvApp::isAltDown() const
{
	return getLastModifierKeysState().alt;
}
bool CvApp::isCtrlDown() const
{
	return getLastModifierKeysState().ctrl;
}

std::optional<std::filesystem::path> CvApp::promptSaveGamePath(const std::filesystem::path& defPath)
{
	return cvengine::promptSaveFilePath(defPath);
}
std::optional<std::filesystem::path> CvApp::promptLoadGamePath(const std::filesystem::path& defDir)
{
	return cvengine::promptLoadFilePath(defDir);
}

void CvApp::deferLoadGame(const std::filesystem::path& path)
{
	deferAppState(std::make_unique<LoadGameCvAppState>(path));
}

bool CvApp::isAutorun() const
{
	return CvTuiInterface::getInstance().isAIAutorunActive();
}

void CvApp::exitApp()
{
	mWantExit = true;
}
bool CvApp::isAppExiting() const
{
	return mWantExit;
}