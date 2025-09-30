#include "CvApp.h"
#include "MyFFile.h"
#include "CvVFS.h"
#include "CvInterface.h"
#include "DLLInterface/MyCvDLLPython.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "CvEngine.h"
#include "AppMainMenusState.h"
#include "CivIni.h"
#include "CvVFS.h"

#include <CvGlobals.h>
#include <CvInitCore.h>
#include <CvGameAI.h>
#include <CvMap.h>
#include <CvTeamAI.h>
#include <CvPlayerAI.h>
#include <CvEventReporter.h>
#include <CyArgsList.h>

#include <CommonStuff/System.h>
#include <CommonStuff/range.h>

#include <iostream>
#include <chrono>

using heck::range;

CvApp& CvApp::getInstance()
{
	static CvApp x;
	return x;
}

CvApp::CvApp()
{
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
	mVFS = std::make_unique<CvVFS>(dataDir, vanillaCiv4RootDir, L"");
	const auto t1 = std::chrono::steady_clock::now();
	std::clog << "VFS initialisation took " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
	//mVFS = std::make_unique<CvVFS>(dataDir, vanillaCiv4RootDir, L"Next War\\");
	gVFS = mVFS.get();

	saveCivilizationIniIfChanged();

	audioSystem->loadXmlDefs();
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


const CvVFS& CvApp::getVFS() const
{
	return *mVFS;
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

void CvApp::deferLoadGame(const std::filesystem::path& path)
{
	deferAppState(std::make_unique<LoadGameCvAppState>(path));
}

void CvApp::deferNewGameStartup(SimplifiedInitCore config)
{
	deferAppState(std::make_unique<NewGameStartupCvAppState>(std::move(config)));
}

void CvApp::deferInGameUI()
{
	deferAppState(std::make_unique<InGameCvAppState>());
}

int CvApp::run()
{
	while (!MyCvDLLUtility::getInstance().isDone)
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

void CvApp::serialise(FFile<StdRawBinaryStream>& file) const
{
	CvEventReporter::getInstance().preSave();

	static constexpr uint32_t kU32Unk0 = 0;
	//static constexpr uint32_t kU32Unk1 = 1;

	file.Write(gGlobals.getDefineINT("SAVE_VERSION"));
	file.WriteString(gVFS->getModRelPathString());
	file.WriteString(std::string()); // modMd5Hash
	file.Write(kU32Unk0);
	file.WriteString(std::string()); // versionStr
	file.WriteString(std::string()); // md5HashA
	file.WriteString(std::string()); // md5HashB
	file.WriteString(std::string()); // md5HashC scripts
	file.WriteString(std::string()); // md5HashD XML

	FFile<MemoryWriteRawBinaryStream> initCoreStream;
	gGlobals.getInitCore().write(&initCoreStream);

	file.Write((uint32_t)initCoreStream.stream.blob.size());
	file.Write((uint32_t)initCoreStream.stream.blob.size(), reinterpret_cast<const uint8_t*>(initCoreStream.stream.blob.data()));

	file.Write(kU32Unk0);

	//FFile<MemoryWriteRawBinaryStream> gameStateStream;
	FFile<CompressionBinaryStream> gameStateStream(&file.stream);
	gGlobals.getGame().write(&gameStateStream);
	gGlobals.getMap().write(&gameStateStream);
	for (const auto i : range<TeamTypes>(MAX_TEAMS))
		CvTeamAI::getTeamNonInl(i).write(&gameStateStream);
	for (const auto i : range<PlayerTypes>(MAX_PLAYERS))
		CvPlayerAI::getPlayerNonInl(i).write(&gameStateStream);
	CvEventReporter::getInstance().writeStatistics(&gameStateStream);
	gameStateStream.stream.flushCompression(true);

	// Code for saving the uncompressed game state blob to file.
	//std::ofstream("CvApp serialise gameStateStream.bin", std::ios::binary).write((const char*)gameStateStream.stream.blob.data(), gameStateStream.stream.blob.size());
	//FFile<CompressionBinaryStream> gameStateCompressionStream(&file.stream);
	//gameStateCompressionStream.stream.write(gameStateStream.stream.blob);
	//gameStateCompressionStream.stream.flushCompression(true);

	file.Write(kU32Unk0);

	CvString appSaveData;
	(void)MyCvDLLPython().callFunction(PYCivModule, "onSave", nullptr, &appSaveData);
	file.WriteString(appSaveData);

	file.Write(kU32Unk0);

	CvEngine::getInstance().serialise(file);
	
	file.Write(uint8_t(0)); // unk

	file.WriteString(std::string()); // Save hash.
}
void CvApp::deserialise(FFile<StdRawBinaryStream>& file)
{
	uint32_t u32{};
	file.Read(&u32);
	if (u32 != (unsigned)gGlobals.getDefineINT("SAVE_VERSION"))
		std::abort();
	CvString str;
	file.ReadString(str);
	if (str != heck::toAsciiString(gVFS->getModRelPathString()))
		throw std::runtime_error("Could not load save. Save uses mod '" + str + "' while engine is loaded with mod '" + heck::toAsciiString(gVFS->getModRelPathString()) + "'.");
	file.ReadString(str); // modMd5Hash
	file.Read(&u32); // unk
	if (u32 != 0)
		std::abort();
	file.ReadString(str); // versionStr
	file.ReadString(str); // md5HashA
	file.ReadString(str); // md5HashB
	file.ReadString(str); // md5HashC scripts
	file.ReadString(str); // md5HashD XML

	uint32_t initCoreSerialisedSize{};
	file.Read(&initCoreSerialisedSize);
	const auto initCorePos = file.stream.stream.tellg();
	CvInitCore& loadedInitCore = gGlobals.getLoadedInitCore();

	CvInitCore& mainInitCore = gGlobals.getInitCore();
	loadedInitCore.read(&file);
	const auto initCoreEnd = file.stream.stream.tellg();
	if (initCoreEnd - initCorePos != initCoreSerialisedSize)
		std::abort();

	mainInitCore.resetGame(&loadedInitCore, true, false);
	mainInitCore.resetPlayers(&loadedInitCore, true, false);

	// I gave no idea what the criteria is, but something sets the active player even when no player is isHuman in CvGame.
	// When loading the save, player 0 is SS_COMPUTER.
	// In the logs, it looks like the Firaxis engine is overwriting the first player. Maybe the first player is always the active player in single player?
	//mainInitCore.setNetID(PlayerTypes(0), 0);
	//mainInitCore.setSlotClaim(PlayerTypes(0), SLOTCLAIM_ASSIGNED);
	//mainInitCore.setSlotStatus(PlayerTypes(0), SS_TAKEN);
	mainInitCore.setActivePlayer(static_cast<PlayerTypes>(0));
	// The Firaxis engine appears to force a player to be human. Loading an AIAutoPlay save does not continue the AIAutoPlay.
	// This behaviour can be overriden in CvGame::read.

	mainInitCore.setType(GameType::GAME_SP_LOAD);

	file.Read(&u32); // unk
	if (u32 != 0)
		std::abort();

	// Code for saving the uncompressed game state blob to file.
	//FFile<DecompressionBinaryStream> gameStateDecompressionStream(&file.stream);
	//const std::vector<std::byte> gameStateBlob = gameStateDecompressionStream.stream.readUntilEnd();
	//std::ofstream("CvApp deserialised gameStateStream.bin", std::ios::binary).write((const char*)gameStateBlob.data(), gameStateBlob.size());

	FFile<DecompressionBinaryStream> gameStateStream(&file.stream);
	//FFile<MemoryReadRawBinaryStream> gameStateStream(gameStateBlob);
	gGlobals.getGame().read(&gameStateStream);

	if (mainInitCore.getActivePlayer() == NO_PLAYER)
	{
		std::cerr << "ERROR: No active player." << std::endl;
		std::abort();
	}

	gGlobals.getMap().read(&gameStateStream);
	for (const auto i : range<TeamTypes>(MAX_TEAMS))
		CvTeamAI::getTeamNonInl(i).read(&gameStateStream);
	for (const auto i : range<PlayerTypes>(MAX_PLAYERS))
		CvPlayerAI::getPlayerNonInl(i).read(&gameStateStream);
	CvEventReporter::getInstance().readStatistics(&gameStateStream);
	//gameStateStream.stream.assertStreamEnd();

	// Do this before running scripts.
	gGlobals.getGame().onCvAppFinishedLoading();

	file.Read(&u32); // unk
	if (u32 != 0)
		std::abort();

	file.ReadString(str);
	CyArgsList pyArgs;
	pyArgs.add(str.c_str());
	(void)MyCvDLLPython().callFunction(PYCivModule, "onLoad", pyArgs.makeFunctionArgs());

	file.Read(&u32); // unk
	if (u32 != 0)
		std::abort();

	CvEngine::getInstance().deserialise(file);

	uint8_t u8{};
	file.Read(&u8); // unk
	if (u8 != 0)
		std::abort();

	file.ReadString(str);
}
