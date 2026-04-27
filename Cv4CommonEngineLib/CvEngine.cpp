#include "inc/Cv4CommonEngineLib/CvEngine.h"
#include "inc/Cv4CommonEngineLib/CivIni.h"
#include "inc/Cv4CommonEngineLib/CvInterface.h"
#include "Common.h"
#include "CommonEngineGlobal.h"
#include "CvAppUtil.h"

#include <CvGameCoreDLL/CvDLLUtilityIFaceBase.h>
#include <CvGameCoreDLL/CvTalkingHeadMessage.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvGameTextMgr.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvPlot.h>

#include <CommonStuff/range.h>

#include <iostream>
#include <format>

using heck::range;

using namespace cvengine;

void CvEngine::reset()
{
	mAutoSaveCounter = 0;
	mSigns.clear();
}

void CvEngine::clearSigns()
{
	mSigns.clear();
}

void CvEngine::autoSave(bool bInitial)
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
		cvengine::gCommonEngineConfig.interface->addTurnMessageDisplayEntry(std::move(msg));

		const std::filesystem::path autosavesDirPath = cvengine::gCommonEngineConfig.userDataDirPath / cvengine::gCommonEngineConfig.savesDirName / kSavesSingleDirName / kSavesAutoDirName;

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
		app::serialise(file);
	}
}

void CvEngine::saveReplay(PlayerTypes ePlayer)
{
	CvWString timeStr;
	CvGameTextMgr::GetInstance().setTimeStr(timeStr, gGlobals.getGame().getGameTurn(), true);
	const std::filesystem::path dir = cvengine::gCommonEngineConfig.userDataDirPath / cvengine::gCommonEngineConfig.replaysDirName;
	std::filesystem::path path;
	for (int i = 0; i < 1000; ++i)
	{
		path = dir / std::format(L"{}_{}_{}.CivBeyondSwordReplay",
			static_cast<const std::wstring&>(gGlobals.getInitCore().getGameName()),
			static_cast<const std::wstring&>(timeStr),
			i
		);
		FFile<StdRawBinaryStream> file(path, std::ios::out | std::ios::binary | std::ios::noreplace);
		if (file.stream.stream)
		{
			gGlobals.getGame().writeReplay(file, ePlayer);
			std::clog << "Replay saved to " << path << std::endl;
			return;
		}
	}

	std::clog << "Could not save replay to " << path << std::endl;
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
int CvEngine::getNumSigns() const
{
	return static_cast<int>(mSigns.size());
}
CvSign CvEngine::getSignByIndex(size_t i) const
{
	if (i >= mSigns.size())
	{
		return {
			.playerI = NO_PLAYER,
		};
	}
	const auto it = std::next(mSigns.begin(), i);
	return {
		it->first.playerI,
		it->first.coord,
		it->second
	};
}

void CvEngine::addLandmark(const CvPlot* plot, const std::wstring& caption)
{
	if (plot)
		setSignText(NO_PLAYER, { plot->getX(), plot->getY() }, caption);
}
void CvEngine::removeLandmark(ivec2 coord)
{
	mSigns.erase(SignKey{ NO_PLAYER, coord });
}
void CvEngine::removeSign(PlayerTypes playerI, ivec2 coord)
{
	mSigns.erase(SignKey{ playerI, coord });
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