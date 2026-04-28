#include "inc/Cv4CommonEngineLib/CommonEngine.h"
#include "inc/Cv4CommonEngineLib/AudioXmlDefs.h"
#include "inc/Cv4CommonEngineLib/CivIni.h"
#include "inc/Cv4CommonEngineLib/CvInterface.h"
#include "inc/Cv4CommonEngineLib/CvTranslator.h"
#include "inc/Cv4CommonEngineLib/CvUserProfile.h"
#include "inc/Cv4CommonEngineLib/CvVFS.h"
#include "inc/Cv4CommonEngineLib/MyCvDLLPython.h"
#include "MyCvDLLXml.h"
#include "MyCvDLLUtility.h"
#include "Common.h"

#include "CommonEngineGlobal.h"

#include <PlayerBotGameBinding/IPlayerBotPlugin.h>

#include <CvGameCoreDLL/CvXMLLoadUtility.h>
#include <CvGameCoreDLL/CvRandom.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvGameTextMgr.h>
#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CvArtFileMgr.h>

#include <chrono>
#include <iostream>


cvengine::CommonEngineConfig cvengine::gCommonEngineConfig;
std::unique_ptr<cvengine::CvVFS> cvengine::gVFS;

constexpr int kASyncSeed = 9347;

static void loadPostMenuGlobals()
{
	CvXMLLoadUtility xmlUtility{};
	if (!xmlUtility.SetupGlobalLandscapeInfo())
		std::abort();
	if (!xmlUtility.LoadPostMenuGlobals())
		std::abort();
}

static void initCommon()
{
	gGlobals.setDLLIFace(&MyCvDLLUtility::getInstance());
	CvXMLLoadUtility xmlUtility{};
	xmlUtility.LoadPlayerOptions();
	xmlUtility.LoadGraphicOptions();

	// This needs player options in case we need to create a new profile.
	// Note sure when this happens first, but let's just do it here deterministically.
	// Importantly, this reads the language, which is needed when loading text.
	CvUserProfile::getInstance().readFromFile();

	// fullscreen prompt here

	gGlobals.init();
	auto& gameTextMgr = CvGameTextMgr::GetInstance();
	gameTextMgr.Initialize();
	//updatePlayersAreHuman();
	gGlobals.getASyncRand().init(kASyncSeed);
	CvArtFileMgr::GetInstance().Init();
	MyCvDLLPython().initialisePython(); // CvEventReporter::init, CvAppInterface::init

	xmlUtility.SetGlobalDefines();
	xmlUtility.LoadGlobalText();
	xmlUtility.SetGlobalTypes();
	xmlUtility.SetGlobalArtDefines();
	CvArtFileMgr::GetInstance().buildArtFileInfoMaps();
	xmlUtility.LoadBasicInfos();
	xmlUtility.LoadPreMenuGlobals();
	xmlUtility.SetPostGlobalsGlobalDefines();
	CvEventReporter& eventReporter = CvEventReporter::getInstance();
	eventReporter.windowActivation(true);
	CvTranslator::getInstance().symbols.resize(MAX_NUM_SYMBOLS, L'�'); // probably
	gameTextMgr.assignFontIds(CvTranslator::getInstance().firstSymbolCode, 25);
	//cvengine::gCommonEngineConfig.interface->reset();
	CvInitCore& mainInitCore = gGlobals.getInitCore();
	CvInitCore& loadedInitCore = gGlobals.getLoadedInitCore();
	CvInitCore& iniInitCore = gGlobals.getIniInitCore();
	mainInitCore.init(GAMEMODE_NORMAL);
	loadedInitCore.init(GAMEMODE_NORMAL);
	iniInitCore.init(GAMEMODE_NORMAL);
	eventReporter.resetStatistics();
	gGlobals.SetGraphicsInitialized(true);

	// Default game settings from the INI, as if we read from the INI.
	//iniInitCore.setGameName(L"Fluffy's Game");
	//iniInitCore.setWorldSize(L"WORLDSIZE_HUGE");
	//iniInitCore.setClimate(L"CLIMATE_TROPICAL");
	//iniInitCore.setSeaLevel(L"SEALEVEL_LOW");
	//iniInitCore.setEra(L"ERA_ANCIENT");
	//iniInitCore.setGameSpeed(L"GAMESPEED_NORMAL");
	//for (const auto i : range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
	//	iniInitCore.setVictory(i, true);
	//for (const auto i : range<GameOptionTypes>(gGlobals.getNumGameOptions()))
	//	iniInitCore.setOption(i, false);
	//iniInitCore.setSyncRandSeed(kSyncSeed);
	//iniInitCore.setMapRandSeed(kMapSeed);
	//iniInitCore.setType(GAME_SP_NEW);
	//iniInitCore.setMapScriptName(L"Donut");
	//
	//loadedInitCore.resetGame(&iniInitCore);
	//loadedInitCore.resetPlayers(&iniInitCore);
	//loadedInitCore.setPitbossTurnTime(0);
	//loadedInitCore.setSyncRandSeed(kSyncSeed);
	//loadedInitCore.setMapRandSeed(kMapSeed);
	//loadedInitCore.setType(GAME_SP_NEW);
	//
	//mainInitCore.resetGame(&iniInitCore);
	//mainInitCore.setPitbossTurnTime(0);
	//mainInitCore.setSyncRandSeed(kSyncSeed);
	//mainInitCore.setMapRandSeed(kMapSeed);
	//mainInitCore.setType(GAME_SP_NEW);

	// This is probably a static variable in the EXE as it's called at different times.
	CvTranslator::getInstance().initialiseTextCodeTags();

	loadPostMenuGlobals();
}

cvengine::CommonEngineInitResult cvengine::initialiseCommonEngine(std::wostream& log, const CommonEngineConfig& config)
{
	gCommonEngineConfig = config;

	log << L"User config directory: " << config.userConfigDirPath << std::endl;
	log << L"User data directory: " << config.userDataDirPath << std::endl;
	log << L"User cache directory: " << config.cacheDirPath << std::endl;

	log << L"Creating directories..." << std::endl;
	std::filesystem::create_directories(config.userDataDirPath / config.savesDirName / kSavesSingleDirName / kSavesAutoDirName);
	std::filesystem::create_directories(config.userDataDirPath / config.savesDirName / kSavesSingleDirName);
	std::filesystem::create_directories(config.userDataDirPath / config.replaysDirName);
	std::filesystem::create_directories(config.userConfigDirPath / kCustomAssetsDirName);
	std::filesystem::create_directories(config.userConfigDirPath / kPublicMapsDirName);

	loadCivIni();
	loadEnhancedDLLConfigFromINI();

	log << L"Initialising VFS with:\n";
	log << L'\t' << L"vanillaCiv4RootDir = " << config.civ4InstallationRoot.wstring() << L'\n';
	log << L'\t' << L"userConfigDir = " << config.userConfigDirPath.wstring() << L'\n';
	log << L'\t' << L"optModRelPath = " << (config.optModRelPath ? config.optModRelPath->wstring() : L"") << L'\n';
	log << L'\t' << L"optEngineAssetsOverrideDir = " << (config.optEngineAssetsOverrideDir ? config.optEngineAssetsOverrideDir->wstring() : L"") << L'\n';

	const auto t0 = std::chrono::steady_clock::now();
	gVFS = std::make_unique<CvVFS>(config.civ4InstallationRoot, config.userConfigDirPath, config.optModRelPath, config.optEngineAssetsOverrideDir, config.enableCustomAssets);
	const auto t1 = std::chrono::steady_clock::now();
	// Converting string. GCC doesn't support `wostream << duration`.
	//log << L"VFS initialisation took " << heck::convertUtf8ToWide((std::ostringstream() << std::chrono::duration<double, std::milli>(t1 - t0)).str()) << std::endl;
	log << L"VFS initialisation took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;

	if (config.optPlayerBotPlugin && config.optPlayerBotPlugin->getModName() != gVFS->getModName(false))
		throw std::runtime_error("Bot requires mod '" + heck::convertWideToUtf8(config.optPlayerBotPlugin->getModName()) + "'.");

	// Add defaults.
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_MapRandSeed, 0);
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_SyncRandSeed, 0);
	(void)gCivilizationIVIni.grab(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_CheatCode, "");

	saveCivilizationIniIfChanged();

	AudioXmlDefs::getInstance().loadXmlDefs();

	initCommon(); // python initialised here

	return { gVFS.get() };
}
void cvengine::shutdownCommonEngine()
{
	// Python needs to be shutdown before main ends, for some reason. Probably static destruction order issues.
	MyCvDLLPython::shutdownPython();
	gCommonEngineConfig = {};
	gVFS = nullptr;
}