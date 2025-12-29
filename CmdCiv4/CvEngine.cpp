#include "CvEngine.h"
#include "Common.h"
#include "IniReader.h"
#include "CvInterface.h"
#include "CvApp.h"
#include "CivIni.h"

#include <CommonStuff/range.h>

#include <CvTalkingHeadMessage.h>
#include <CvString.h>
#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvDLLUtilityIFaceBase.h>
#include <CvGameTextMgr.h>

#include <iostream>

using heck::range;
using namespace cvengine;

CvEngine& CvEngine::getInstance()
{
	static CvEngine g;
	return g;
}

void CvEngine::reset()
{
	*this = CvEngine();
}

void CvEngine::AutoSave(bool bInitial)
{
	// This is called every turn.
	const int interval = gCivilizationIVIni.get(kCivilizationIVIniSection_CONFIG, { "AutoSaveInterval" }, 4);
	const int maxSaves = gCivilizationIVIni.get(kCivilizationIVIniSection_CONFIG, { "MaxAutoSaves" }, 5);
	if (bInitial || (interval > 0 && ++mAutoSaveCounter >= interval))
	{
		mAutoSaveCounter = 0;

		// CreateDestroy CvTalkingHeadMessage::CvTalkingHeadMessage(4, 10, Autosaving..., , 1, , -1, -1, -1, false, false)
		// Mutating CvTalkingHeadMessage::setShown(true)
		// Not sure where the message goes. CvPlayer::addMessage is not called.
		CvTalkingHeadMessage msg(GC.getGame().getGameTurn(), 10, gDLL->getText("TXT_KEY_AUTOSAVING").c_str(), nullptr, MESSAGE_TYPE_DISPLAY_ONLY, nullptr, NO_COLOR, -1, -1, false, false);
		msg.setShown(true);
		CvInterface::getInstance().addTurnMessageDisplayEntry(std::move(msg));

		const std::filesystem::path autosavesDirPath = getUserDataDir() / kSavesDirName / kSavesSingleDirName / kSavesAutoDirName;

		try
		{
			std::filesystem::directory_entry oldestEntry;
			size_t numExistingSaves = 0;
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(autosavesDirPath))
			{
				if (entry.path().extension() == ".CivBeyondSwordSave")
				{
					++numExistingSaves;
					if (oldestEntry.path().empty() || entry.last_write_time() < oldestEntry.last_write_time())
						oldestEntry = entry;
				}
			}

			if (std::cmp_greater_equal(numExistingSaves, maxSaves) && !oldestEntry.path().empty())
			{
				std::error_code ec{};
				if (!std::filesystem::remove(oldestEntry, ec))
					std::clog << "Failed to delete old autosave " << oldestEntry.path() << std::endl;
				else
					std::clog << "Deleted old autosave " << oldestEntry.path() << std::endl;
			}
		}
		catch (const std::filesystem::filesystem_error& ex)
		{
			std::clog << "Error while iterating over autosave directory: " << ex.what() << std::endl;
		}

		CvWString str;
		GAMETEXT.setTimeStr(str, GC.getGame().getGameTurn(), true);

		const std::filesystem::path path = autosavesDirPath / std::format(L"Autosave turn {} {}{}.CivBeyondSwordSave", GC.getGame().getGameTurn(), bInitial ? L"Initial " : L"", std::wstring(str));

		std::clog << "Autosaving to " << path << "..." << std::endl;

		FFile<StdRawBinaryStream> file{ StdRawBinaryStream(path, std::ios::out | std::ios::binary) };
		CvApp::getInstance().serialise(file);
	}
}

void CvEngine::setSignText(PlayerTypes playerI, heck::ivec2 coord, std::wstring text)
{
	mSigns[SignKey(playerI, coord)] = std::move(text);
}
std::wstring_view CvEngine::findSignTextAt(PlayerTypes playerI, heck::ivec2 coord) const
{
	const auto it = mSigns.find(SignKey(playerI, coord));
	if (it != mSigns.end())
		return it->second;
	else
		return {};
}

size_t CvEngine::Hasher::operator()(SignKey x) noexcept
{
	return std::hash<uint64_t>()((uint64_t(x.playerI) * (1543 * 1543)) | (uint64_t(x.coord.y) * 1543) | uint64_t(x.coord.x));
}

bool CvEngine::SignKey::operator<(CvEngine::SignKey b) const
{
	return std::tie(playerI, coord.y, coord.x) < std::tie(b.playerI, b.coord.y, b.coord.x);
}

void CvEngine::serialise(FFile<StdRawBinaryStream>& file) const
{
	// For deterministic save files, order the signs.
	std::vector<std::pair<SignKey, std::wstring_view>> orderedSigns(std::from_range, mSigns);
	std::ranges::sort(orderedSigns);

	file.Write(static_cast<uint32_t>(orderedSigns.size()));
	for (const auto [signKey, text] : orderedSigns)
	{
		file.Write(uint32_t(0)); // unk
		file.Write(signKey.coord.x); // x
		file.Write(signKey.coord.y); // y
		file.Write(signKey.playerI); // playerId
		file.WriteString(std::wstring(text));
	}

	constexpr uint32_t kU32Unk0 = 0;

	// Greatwall stuff probably
	file.Write(kU32Unk0);
	file.Write(kU32Unk0);
	file.Write(kU32Unk0);
	file.Write(kU32Unk0);
	file.Write(kU32Unk0);
	file.Write(uint32_t(0)); // no greatwalls
}
void CvEngine::deserialise(FFile<StdRawBinaryStream>& file)
{
	reset();

	uint32_t u32{};

	// Signs
	uint32_t numSigns{};
	file.Read(&numSigns);
	CvWString wstr;
	for ([[maybe_unused]] const auto i : range(numSigns))
	{
		file.Read(&u32); // unk
		if (u32 != 0)
			std::abort();
		SignKey signKey{};
		file.Read(&signKey.coord.x); // x
		file.Read(&signKey.coord.y); // y
		int playerI = 0;
		file.Read(&playerI); // playerId
		signKey.playerI = static_cast<PlayerTypes>(playerI);
		file.ReadString(wstr);

		setSignText(signKey.playerI, signKey.coord, wstr);
	}

	uint8_t u8{};

	// Greatwall stuff probably
	for ([[maybe_unused]] const int i : range(5))
	{
		file.Read(&u32); // unk
		if (u32 != 0)
			std::abort();
	}
	file.Read(&u32);
	for ([[maybe_unused]] const int i : range(u32))
	{
		file.Read(&u32); // unk
		if (u32 != 0)
			std::abort();
		file.Read(&u32); // numStrips?
		for ([[maybe_unused]] const int j : range(std::max(1u, u32)))
		{
			file.Read(&u32); // unk
			if (u32 != 0)
				std::abort();
			file.Read(&u32); // num verts
			file.stream.stream.seekg(u32 * sizeof(float) * 3, std::ios::cur);
			file.Read(&u8); // unk bool
		}
		file.Read(&u32); // num unk
		file.stream.stream.seekg(u32 * sizeof(uint32_t) * 4, std::ios::cur);
		file.Read(&u32); // playerId
		file.Read(&u32); // cityId
		file.Read(&u32); // unk
		if (u32 != 0)
			std::abort();
		file.Read(&u32); // unk
		if (u32 != 0)
			std::abort();
	}
}