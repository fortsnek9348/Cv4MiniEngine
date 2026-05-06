#include "CvAppUtil.h"
#include "CommonEngineGlobal.h"
#include "CompressionBinaryStream.h"
#include "inc/Cv4CommonEngineLib/RawBinaryStream.h"
#include "inc/Cv4CommonEngineLib/CvVFS.h"
#include "inc/Cv4CommonEngineLib/MyCvDLLPython.h"
#include "inc/Cv4CommonEngineLib/CvEngine.h"

#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvTeamAI.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CyArgsList.h>

#include <CommonStuff/range.h>

#include <iostream>

using heck::range;

void cvengine::app::serialise(FFile<StdRawBinaryStream>& file)
{
	CvEventReporter::getInstance().preSave();

	static constexpr uint32_t kU32Unk0 = 0;
	//static constexpr uint32_t kU32Unk1 = 1;

	file.Write(gGlobals.getDefineINT("SAVE_VERSION"));
	file.WriteString(heck::toAsciiString(gVFS->getModRelPathString()));
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

	gCommonEngineConfig.engine->serialise(file);

	file.Write(uint8_t(0)); // unk

	file.WriteString(std::string()); // Save hash.
}

void cvengine::app::deserialise(FFile<StdRawBinaryStream>& file)
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

	// I have no idea what the criteria is, but something sets the active player even when no player is isHuman in CvGame.
	// When loading the save, player 0 is SS_COMPUTER.
	// In the logs, it looks like the Firaxis engine is overwriting the first player. Maybe the first player is always the active player in single player?
	//mainInitCore.setNetID(PlayerTypes(0), 0);
	//mainInitCore.setSlotClaim(PlayerTypes(0), SLOTCLAIM_ASSIGNED);
	//mainInitCore.setSlotStatus(PlayerTypes(0), SS_TAKEN);
	
	// Find the first human player.
	for (int i = 0; i < gGlobals.getMAX_CIV_PLAYERS(); ++i)
		if (mainInitCore.getSlotStatus(static_cast<PlayerTypes>(i)) == SlotStatus::SS_TAKEN)
		{
			mainInitCore.setActivePlayer(static_cast<PlayerTypes>(i));
			break;
		}
	// Fallback to player 0.
	if (mainInitCore.getActivePlayer() == NO_PLAYER)
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

	gCommonEngineConfig.engine->deserialise(file);

	uint8_t u8{};
	file.Read(&u8); // unk
	if (u8 != 0)
		std::abort();

	file.ReadString(str);
}

