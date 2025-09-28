#include "MyCvDLLUtility.h"
#include "MyCvDLLXml.h"
#include "MyCvDLLPython.h"
#include "MyCvDLLFAStar.h"
#include "MyCvDLLFeature.h"
#include "MyCvDLLFlagEntity.h"
#include "MyCvDLLEntityIFace.h"
#include "MyCvDLLInterfaceIFace.h"
#include "MyCvDLLEngineIFace.h"
#include <FAStar.h>
#include "FMPIManager.h"
#include "../CvVFS.h"
#include "../Common.h"
#include "../MyFFile.h"
#include "../CvInterface.h"
#include "../CvApp.h"
#include "../CvTranslator.h"
#include "../Logger.h"
#include "../DLLMessageQueue.h"
#include "../CvDiplomacyScreen.h"
#include "../CvUserProfile.h"
#include "../CivIni.h"

#include <CvArtFileMgr.h>
#include <CvGameTextMgr.h>
#include <CvGlobals.h>
#include <CvRandom.h>
#include <CvDLLPythonIFaceBase.h>
#include <CvDLLPlotBuilderIFaceBase.h>
#include <CvEventReporter.h>
#include <CvXMLLoadUtility.h>
#include <CvInitCore.h>
#include <CvPlayerAI.h>
#include <CvMessageData.h>
#include <FVariableSystem.h>
#include <FVariableSystem.inl> // Probably not supposed to include this, but it has the read write functions.
#include <CvDLLEngineIFaceBase.h>
#include <CvGameAI.h>
#include <CvReplayInfo.h>
#include <CvMap.h>
#include <FAStarNode.h>

#include <CommonStuff/range.h>

#include <filesystem>
#include <iostream>
#include <fstream>

using namespace cmdciv4;
using heck::range;

static constexpr bool kEnableXmlCaching = true;

MyCvDLLUtility& MyCvDLLUtility::getInstance()
{
	static MyCvDLLUtility x;
	return x;
}

CvDLLEntityIFaceBase* MyCvDLLUtility::getEntityIFace()
{
	static MyCvDLLEntityIFace x;
	return &x;
}

CvDLLInterfaceIFaceBase* MyCvDLLUtility::getInterfaceIFace()
{
	static MyCvDLLInterfaceIFace x;
	return &x;
}

CvDLLEngineIFaceBase* MyCvDLLUtility::getEngineIFace()
{
	static MyCvDLLEngineIFace x;
	return &x;
}

CvDLLIniParserIFaceBase* MyCvDLLUtility::getIniParserIFace()
{
	std::abort();
}

CvDLLSymbolIFaceBase* MyCvDLLUtility::getSymbolIFace()
{
	static MyCvDLLSymbol x;
	return &x;
}

CvDLLFeatureIFaceBase* MyCvDLLUtility::getFeatureIFace()
{
	static MyCvDLLFeature x;
	return &x;
}

CvDLLRouteIFaceBase* MyCvDLLUtility::getRouteIFace()
{
	static MyCvDLLRoute x;
	return &x;
}


class CvPlotBuilder : public CvEntity
{
public:
	virtual EType getType() const noexcept override
	{
		return kPlotBuilder;
	}
};

CvDLLPlotBuilderIFaceBase* MyCvDLLUtility::getPlotBuilderIFace()
{
	static struct : CvDLLPlotBuilderIFaceBase {
		virtual void init(CvPlotBuilder*, CvPlot*) override
		{
		}
		virtual CvPlotBuilder* create() override
		{
			return new CvPlotBuilder();
		}
	} x;

	return &x;
}

CvDLLRiverIFaceBase* MyCvDLLUtility::getRiverIFace()
{
	static MyCvDLLRiver x;
	return &x;
}

CvDLLFAStarIFaceBase* MyCvDLLUtility::getFAStarIFace()
{
	static constinit MyCvDLLFAStar x{};
	return &x;
}

CvDLLXmlIFaceBase* MyCvDLLUtility::getXMLIFace()
{
	static MyCvDLLXml instance;
	return &instance;
}

CvDLLFlagEntityIFaceBase* MyCvDLLUtility::getFlagEntityIFace()
{
	static MyCvDLLFlagEntity instance;
	return &instance;
}

CvDLLPythonIFaceBase* MyCvDLLUtility::getPythonIFace()
{
	static MyCvDLLPython x;
	return &x;
}

void MyCvDLLUtility::delMem(void*)
{
	std::abort();
}

void* MyCvDLLUtility::newMem(size_t size)
{
	return operator new(size);
}

void MyCvDLLUtility::delMem(void* p, [[maybe_unused]] const char* pcFile, [[maybe_unused]] int iLine)
{
	return operator delete(p);
}

void* MyCvDLLUtility::newMem(size_t size, [[maybe_unused]] const char* pcFile, [[maybe_unused]] int iLine)
{
	return operator new(size);
}

void MyCvDLLUtility::delMemArray(void* p, [[maybe_unused]] const char* pcFile, [[maybe_unused]] int iLine)
{
	return operator delete(p);
}

void* MyCvDLLUtility::newMemArray(size_t size, [[maybe_unused]] const char* pcFile, [[maybe_unused]] int iLine)
{
	return operator new(size);
}

void* MyCvDLLUtility::reallocMem([[maybe_unused]] void* a, [[maybe_unused]] unsigned int uiBytes, [[maybe_unused]] const char* pcFile, [[maybe_unused]] int iLine)
{
	std::abort();
}

unsigned int MyCvDLLUtility::memSize(void*)
{
	std::abort();
}

void MyCvDLLUtility::clearVector(std::vector<int>& vec)
{
	vec.clear();
}

void MyCvDLLUtility::clearVector(std::vector<uint8_t>& vec)
{
	vec.clear();
}

void MyCvDLLUtility::clearVector(std::vector<float>& vec)
{
	vec.clear();
}

int MyCvDLLUtility::getAssignedNetworkID(int iPlayerID)
{
	return gGlobals.getInitCore().getNetID((PlayerTypes)iPlayerID);
}

bool MyCvDLLUtility::isConnected([[maybe_unused]] int iNetID)
{
	std::abort();
}

bool MyCvDLLUtility::isGameActive()
{
	// Only used by CvInitCore::getHuman, and only for SS_OPEN slots.
	// Just return true.
	return true;
}

int MyCvDLLUtility::GetLocalNetworkID()
{
	std::abort();
}

int MyCvDLLUtility::GetSyncOOS([[maybe_unused]] int iNetID)
{
	std::abort();
}

int MyCvDLLUtility::GetOptionsOOS([[maybe_unused]] int iNetID)
{
	std::abort();
}

int MyCvDLLUtility::GetLastPing([[maybe_unused]] int iNetID)
{
	std::abort();
}

bool MyCvDLLUtility::IsModem()
{
	std::abort();
}

void MyCvDLLUtility::SetModem([[maybe_unused]] bool bModem)
{
	std::abort();
}

void MyCvDLLUtility::AcceptBuddy([[maybe_unused]] const char* szName, [[maybe_unused]] int iRequestID)
{
	std::abort();
}

void MyCvDLLUtility::RejectBuddy([[maybe_unused]] const char* szName, [[maybe_unused]] int iRequestID)
{
	std::abort();
}

void MyCvDLLUtility::messageControlLog(char* s)
{
	//std::clog << __FUNCTION__ << ": " << s << std::flush;
	static std::ofstream file("MPLog.txt");
	file << s;
}

int MyCvDLLUtility::getChtLvl()
{
	return gCivilizationIVIni.get(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_CheatCode, L"") == L"chipotle" ? 1 : 0;
}

void MyCvDLLUtility::setChtLvl([[maybe_unused]] int iLevel)
{
	std::abort();
}

bool MyCvDLLUtility::GetWorldBuilderMode()
{
	return false;
}

int MyCvDLLUtility::getCurrentLanguage() const
{
	return CvUserProfile::getInstance().getLanguage();
}

void MyCvDLLUtility::setCurrentLanguage(int iNewLanguage)
{
	CvUserProfile::getInstance().setLanguage(iNewLanguage);
}

bool MyCvDLLUtility::isModularXMLLoading() const
{
	return false;
}

bool MyCvDLLUtility::IsPitbossHost() const
{
	return false;
}

CvString MyCvDLLUtility::GetPitbossSmtpHost() const
{
	std::abort();
}

CvWString MyCvDLLUtility::GetPitbossSmtpLogin() const
{
	std::abort();
}

CvWString MyCvDLLUtility::GetPitbossSmtpPassword() const
{
	std::abort();
}

CvString MyCvDLLUtility::GetPitbossEmail() const
{
	std::abort();
}

void MyCvDLLUtility::sendMessageData(CvMessageData* pData)
{
	// Messages need to be queued. Unit selection logic depends on it as selection groups are joined while iterating over one.
	// NOTE: This is taking ownership of the pointer.
	DLLMessageQueue::getInstance().push(std::unique_ptr<CvMessageData>(pData));
}

void MyCvDLLUtility::sendPlayerInfo(PlayerTypes eActivePlayer)
{
	cmdciv4::logInfo("{}", int(eActivePlayer));
}

void MyCvDLLUtility::sendGameInfo([[maybe_unused]] const CvWString& szGameName, [[maybe_unused]] const CvWString& szAdminPassword)
{
	std::abort();
}

void MyCvDLLUtility::sendPlayerOption(PlayerOptionTypes opt, bool value)
{
	class CvNetPlayerOption : public CvMessageData
	{
	public:
		explicit CvNetPlayerOption(PlayerTypes ePlayer, PlayerOptionTypes opt, bool value) : CvMessageData(GAMEMESSAGE_PLAYER_OPTION), m_ePlayer(ePlayer), mOpt(opt), mValue(value)
		{
		}
		virtual void Debug([[maybe_unused]] char* szAddendum) override
		{
		}
		virtual void Execute() override
		{
			// TODO: Is this what happens?
			if (m_ePlayer != NO_PLAYER)
				GET_PLAYER(m_ePlayer).setOption(mOpt, mValue);
		}
		virtual void PutInBuffer(FDataStreamBase* pStream) override
		{
			// TODO: Not sure if this is the actual contents of the message.
			pStream->Write(static_cast<int>(m_ePlayer));
			pStream->Write(static_cast<int>(mOpt));
			pStream->Write(mValue);
		}
		virtual void SetFromBuffer(FDataStreamBase* pStream) override
		{
			int x = 0;
			pStream->Read(&x);
			m_ePlayer = static_cast<PlayerTypes>(x);
			pStream->Read(&x);
			mOpt = static_cast<PlayerOptionTypes>(x);
			pStream->Read(&mValue);
		}
	private:
		PlayerTypes m_ePlayer = NO_PLAYER;
		PlayerOptionTypes mOpt = NO_PLAYEROPTION;
		bool mValue = false;
	};

	// TODO: When does the setting get applied to user profile? Here? In CvNetPlayerOption::Execute?
	CvUserProfile::getInstance().setPlayerOption(opt, value);
	DLLMessageQueue::getInstance().push(std::make_unique<CvNetPlayerOption>(GC.getGame().getActivePlayer(), opt, value));
}

void MyCvDLLUtility::sendChat([[maybe_unused]] const CvWString& szChatString, [[maybe_unused]] ChatTargetTypes eTarget)
{
	std::abort();
}

void MyCvDLLUtility::sendPause([[maybe_unused]] int iPauseID)
{
	std::abort();
}

void MyCvDLLUtility::sendMPRetire()
{
	std::abort();
}

void MyCvDLLUtility::sendToggleTradeMessage([[maybe_unused]] PlayerTypes eWho, [[maybe_unused]] TradeableItems eItemType, [[maybe_unused]] int iData, [[maybe_unused]] int iOtherWho, [[maybe_unused]] bool bAIOffer, [[maybe_unused]] bool bSendToAll)
{
	std::abort();
}

void MyCvDLLUtility::sendClearTableMessage([[maybe_unused]] PlayerTypes eWhoTradingWith)
{
	std::abort();
}

void MyCvDLLUtility::sendImplementDealMessage(PlayerTypes eOtherWho, CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirList)
{
	struct NetImplementDeal : CvMessageData
	{
		struct Args
		{
			PlayerTypes ourPlayerI{};
			PlayerTypes theirPlayerI{};
			CLinkList<TradeData> ourList{};
			CLinkList<TradeData> theirList{};
		};

		Args args;

		explicit NetImplementDeal(Args&& args) : CvMessageData(GAMEMESSAGE_IMPLEMENT_OFFER), args(std::move(args))
		{
		}

		virtual void Debug([[maybe_unused]] char* szAddendum) override
		{
		}
		virtual void Execute() override
		{
			gGlobals.getGame().implementDeal(args.ourPlayerI, args.theirPlayerI, &args.ourList, &args.theirList);
		}
		virtual void PutInBuffer([[maybe_unused]] FDataStreamBase* pStream) override
		{
			std::abort();
		}
		virtual void SetFromBuffer([[maybe_unused]] FDataStreamBase* pStream) override
		{
			std::abort();
		}
	};

	DLLMessageQueue::getInstance().push(std::make_unique<NetImplementDeal>(NetImplementDeal::Args{
		gGlobals.getGame().getActivePlayer(),
		eOtherWho,
		*pOurList,
		*pTheirList,
		}));
	CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::PushMessage);
}

void MyCvDLLUtility::sendContactCiv([[maybe_unused]] NetContactTypes eContactType, [[maybe_unused]] PlayerTypes eWho)
{
	std::abort();
}

void MyCvDLLUtility::sendOffer()
{
	std::abort();
}

void MyCvDLLUtility::sendDiploEvent([[maybe_unused]] PlayerTypes eWhoTradingWith, [[maybe_unused]] DiploEventTypes eDiploEvent, [[maybe_unused]] int iData1, [[maybe_unused]] int iData2)
{
	std::abort();
}

void MyCvDLLUtility::sendRenegotiate([[maybe_unused]] PlayerTypes eWhoTradingWith)
{
	std::abort();
}

void MyCvDLLUtility::sendRenegotiateThisItem([[maybe_unused]] PlayerTypes ePlayer2, [[maybe_unused]] TradeableItems eItemType, [[maybe_unused]] int iData)
{
	std::abort();
}

void MyCvDLLUtility::sendExitTrade()
{
	std::abort();
}

void MyCvDLLUtility::sendKillDeal(int iDealID, bool bFromDiplomacy)
{
	struct NetKillDeal : CvMessageData
	{
		int iDealID{};
		bool bFromDiplomacy{};

		explicit NetKillDeal(int iDealID, bool bFromDiplomacy) : CvMessageData(GAMEMESSAGE_KILL_DEAL), iDealID(iDealID), bFromDiplomacy(bFromDiplomacy)
		{
		}
		virtual void Debug([[maybe_unused]] char* szAddendum) override
		{
		}
		virtual void Execute() override
		{
			gGlobals.getGame().getDeal(iDealID)->kill();

			if (bFromDiplomacy)
			{
				// I'm guessing this is what this is for.
				if (auto* const diplo = gGlobals.getDiplomacyScreen())
					diplo->regreet();
			}
		}
		virtual void PutInBuffer([[maybe_unused]] FDataStreamBase* pStream) override
		{
		}
		virtual void SetFromBuffer([[maybe_unused]] FDataStreamBase* pStream) override
		{
		}
	};

	DLLMessageQueue::getInstance().push(std::make_unique<NetKillDeal>(iDealID, bFromDiplomacy));
	CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::PushMessage);
}

void MyCvDLLUtility::sendDiplomacy([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] CvDiploParameters* pParams)
{
	std::abort();
}

void MyCvDLLUtility::sendPopup([[maybe_unused]] PlayerTypes ePlayer, [[maybe_unused]] CvPopupInfo* pInfo)
{
	std::abort();
}

int MyCvDLLUtility::getMillisecsPerTurn()
{
	return 250;
}

float MyCvDLLUtility::getSecsPerTurn()
{
	// Used when pillaging.
	return getMillisecsPerTurn() / 1000.0f;
}

int MyCvDLLUtility::getTurnsPerSecond()
{
	std::abort();
}

int MyCvDLLUtility::getTurnsPerMinute()
{
	// This is used to divide getTurnSlice(). Ie: Turn slices per minute.
	// But Cv4MiniEngine does not have turn realtime turn slices.
	// But this value does get you the same "time played" stat for existing saves.
	return 60 * 1000 / getMillisecsPerTurn();
}

void MyCvDLLUtility::openSlot(PlayerTypes eID)
{
	if (eID == NO_PLAYER)
		return;

	if (!CvPlayerAI::getPlayerNonInl(eID).isBarbarian())
	{
		CvInitCore& initCore = gGlobals.getInitCore();
		if (initCore.getSlotStatus(eID) == SS_CLOSED)
			// This seems to be what happens.
			initCore.setSlotStatus(eID, SS_COMPUTER);
	}
}

void MyCvDLLUtility::closeSlot(PlayerTypes eID)
{
	// AI death test 2.CivBeyondSwordSave
	if (eID == NO_PLAYER)
		return;

	if (!CvPlayerAI::getPlayerNonInl(eID).isBarbarian())
	{
		CvInitCore& initCore = gGlobals.getInitCore();
		initCore.setSlotStatus(eID, SS_CLOSED);
	}
}

CvWString MyCvDLLUtility::getMapScriptName()
{
	return gGlobals.getInitCore().getMapScriptName();
}

bool MyCvDLLUtility::getTransferredMap()
{
	return false;
}

bool MyCvDLLUtility::isDescFileName([[maybe_unused]] const char* szFileName)
{
	// TODO: What's the meaning of this exactly? *.CivBeyondSwordWBSave?
	return false;
}

bool MyCvDLLUtility::isWBMapScript()
{
	std::abort();
}

bool MyCvDLLUtility::isWBMapNoPlayers()
{
	std::abort();
}

bool MyCvDLLUtility::pythonMapExists(const char* szMapName)
{
	return gVFS->exists(szMapName + std::string(".py"));
}

void MyCvDLLUtility::stripSpecialCharacters(CvWString& szName)
{
	// Used by player leader name.
	std::erase_if(szName, [](wchar_t c) {
		return c == L'<' || c == L'>' || c == L'&' || c == L':' || c == L'/' || c == L'\\';
	});
}


void MyCvDLLUtility::initGlobals()
{
	gGlobals.setInterface(&CvInterface::getInstance());
	static FAStar gPathFinder{};
	static FAStar gInterfacePathFinder{};
	static FAStar gStepPathFinder{};
	static FAStar gRoutePathFinder{};
	static FAStar gBorderFinder{};
	static FAStar gAreaFinder{};
	static FAStar gPlotGroupFinder{};
	gGlobals.setPathFinder(&gPathFinder);
	gGlobals.setInterfacePathFinder(&gInterfacePathFinder);
	gGlobals.setStepFinder(&gStepPathFinder);
	gGlobals.setRouteFinder(&gRoutePathFinder);
	gGlobals.setBorderFinder(&gBorderFinder);
	gGlobals.setAreaFinder(&gAreaFinder);
	gGlobals.setPlotGroupFinder(&gPlotGroupFinder);
}

void MyCvDLLUtility::uninitGlobals()
{
	std::abort();
}

void MyCvDLLUtility::callUpdater()
{
	//abort();
}

bool MyCvDLLUtility::Uncompress([[maybe_unused]] uint8_t** bufIn, [[maybe_unused]] unsigned long* bufLenIn, [[maybe_unused]] unsigned long maxBufLenOut, [[maybe_unused]] int offset)
{
	std::abort();
}

bool MyCvDLLUtility::Compress([[maybe_unused]] uint8_t** bufIn, [[maybe_unused]] unsigned long* bufLenIn, [[maybe_unused]] int offset)
{
	std::abort();
}

void MyCvDLLUtility::NiTextOut(const TCHAR* szText)
{
	std::cerr << szText << std::endl;
}

void MyCvDLLUtility::MessageBox([[maybe_unused]] const TCHAR* szText, [[maybe_unused]] const TCHAR* szCaption)
{
	std::abort();
}

void MyCvDLLUtility::SetDone(bool bDone)
{
	if (!bDone) // TODO: What does this mean?
		std::abort();
	isDone = bDone;
}

bool MyCvDLLUtility::GetDone()
{
	return isDone;
}

bool MyCvDLLUtility::GetAutorun()
{
	return isAutorun;
}

void MyCvDLLUtility::beginDiplomacy(CvDiploParameters* pDiploParams, PlayerTypes ePlayer)
{
	if (pDiploParams && ePlayer != NO_PLAYER)
	{
		CvPlayer& player = CvPlayerAI::getPlayerNonInl(ePlayer);
		if (player.isHuman() && player.isAlive())
			player.addDiplomacy(pDiploParams);
	}
}

void MyCvDLLUtility::endDiplomacy()
{
	if (auto* const screen = gGlobals.getDiplomacyScreen())
		screen->endDiplomacy();
}

bool MyCvDLLUtility::isDiplomacy()
{
	if (auto* const screen = gGlobals.getDiplomacyScreen())
		return screen->isDiplomacyActive();
	else
		return false;
}

int MyCvDLLUtility::getDiplomacyPlayer()
{
	if (auto* const screen = gGlobals.getDiplomacyScreen())
		return screen->getDiplomacyAIPlayer();
	else
		std::abort();
}

void MyCvDLLUtility::updateDiplomacyAttitude([[maybe_unused]] bool bForce)
{
	std::abort();
}

bool MyCvDLLUtility::isMPDiplomacy()
{
	std::abort();
}

bool MyCvDLLUtility::isMPDiplomacyScreenUp()
{
	return false;
}

int MyCvDLLUtility::getMPDiplomacyPlayer()
{
	std::abort();
}

void MyCvDLLUtility::beginMPDiplomacy([[maybe_unused]] PlayerTypes eWhoTalkingTo, [[maybe_unused]] bool bRenegotiate, [[maybe_unused]] bool bSimultaneous)
{
	std::abort();
}

void MyCvDLLUtility::endMPDiplomacy()
{
	// Called when player is deactivated due to AI autoplay.
	std::clog << __FUNCTION__ << std::endl;
}

bool MyCvDLLUtility::getAudioDisabled()
{
	return false;
}

int MyCvDLLUtility::getAudioTagIndex(const TCHAR* szTag, int iScriptType)
{
	return CvApp::getInstance().audioSystem->getAudioTagIndex(szTag, (AudioTag)iScriptType);
}

void MyCvDLLUtility::DoSound([[maybe_unused]] int iScriptId)
{
	// Doesn't seem to be used.
	std::abort();
}

void MyCvDLLUtility::Do3DSound(int iScriptId, [[maybe_unused]] NiPoint3 vPosition)
{
	// Called for Nukes.
	
	return CvApp::getInstance().audioSystem->playScript3DSoundByIndex(iScriptId, {
		gGlobals.getMap().pointXToPlotX(vPosition.x),
		gGlobals.getMap().pointYToPlotY(vPosition.y),
		});
}

FDataStreamBase* MyCvDLLUtility::createFileStream()
{
	std::abort();
}

void MyCvDLLUtility::destroyDataStream([[maybe_unused]] FDataStreamBase*& stream)
{
	std::abort();
}

// Defining the class declared in the DLL.
class CvCacheObject
{
public:
	using ReadFunc = bool (*)(FDataStreamBase*);
	using WriteFunc = void (*)(FDataStreamBase*);
	
	ReadFunc readFunc{};
	WriteFunc writeFunc{};
	std::filesystem::path cacheDataPath;
	std::filesystem::file_time_type latestSourceTimestamp = std::filesystem::file_time_type::min(); // Linux has a year 2174 epoch!

	explicit CvCacheObject(ReadFunc readFunc, WriteFunc writeFunc, const char* cacheFilename)
		: readFunc(readFunc), writeFunc(writeFunc), cacheDataPath(getUserCacheDir() / cacheFilename)
	{
	}

	void specifySource(const std::filesystem::path& path)
	{
		latestSourceTimestamp = std::max(latestSourceTimestamp, std::filesystem::last_write_time(gVFS->resolve(path)));
	}

	bool load() const try
	{
		FFile<StdRawBinaryStream> file{ StdRawBinaryStream(cacheDataPath, std::ios::in | std::ios::binary) };
		try
		{
			if (file.ReadString() != gVFS->getModRelPathString())
			{
				std::wclog << L"Cache " << cacheDataPath << L" rejected: Mod mismatch." << std::endl;
				return false;
			}
		}
		catch (const std::runtime_error&)
		{
			std::wclog << L"Cache " << cacheDataPath << L" rejected: Couldn't read mod path." << std::endl;
			return false;
		}
		if (std::filesystem::last_write_time(cacheDataPath) > latestSourceTimestamp)
		{
			if (readFunc(&file))
			{
				std::wclog << L"Cache " << cacheDataPath << L" used." << std::endl;
				return true;
			}
			else
			{
				std::wclog << L"Cache " << cacheDataPath << L" rejected: Read function returned failure." << std::endl;
				return false;
			}
		}
		else
		{
			std::wclog << L"Cache " << cacheDataPath << L" rejected: Out of date." << std::endl;
			return false;
		}
	}
	catch (const std::filesystem::filesystem_error& ex)
	{
		std::wclog << L"Cache " << cacheDataPath << L" rejected: Some other error." << std::endl;
		std::clog << ex.what() << std::endl;
		return false;
	}

	void store() const
	{
		std::filesystem::create_directories(cacheDataPath.parent_path());
		FFile<StdRawBinaryStream> file{ StdRawBinaryStream(cacheDataPath, std::ios::out | std::ios::binary) };
		file.WriteString(gVFS->getModRelPathString());
		writeFunc(&file);
	}

	template<auto kReadFunc, auto kWriteFunc>
	static CvCacheObject* forCvGlobals([[maybe_unused]] const TCHAR* szCacheFileName)
	{
		if constexpr (!kEnableXmlCaching)
			return nullptr;
		return new CvCacheObject(
			[](FDataStreamBase* stream) { return (gGlobals.*kReadFunc)(stream); },
			[](FDataStreamBase* stream) { return (gGlobals.*kWriteFunc)(stream); },
			szCacheFileName
		);
	}
};

CvCacheObject* MyCvDLLUtility::createGlobalTextCacheObject([[maybe_unused]] const TCHAR* szCacheFileName)
{
	if constexpr (!kEnableXmlCaching)
		return nullptr;

	// GlobalText.dat is zlib compressed. The decompressed stream is just the language and the array of text entries.
	const auto read = [](FDataStreamBase* stream) {
		auto& utility = getInstance();
		unsigned int cachedLanguage = 0;
		int numLanguages = 0;
		stream->Read(&cachedLanguage);
		stream->Read(&numLanguages);
		//assert(CvGameText::NUM_LANGUAGES > 0); // This needs to be initialised!
		if (cachedLanguage != CvUserProfile::getInstance().getLanguage())// || numLanguages != CvGameText::NUM_LANGUAGES)
			return false;
		// Is this supposed to be read in? I don't know.
		CvGameText::NUM_LANGUAGES = numLanguages;
		unsigned int numEntries = 0;
		stream->Read(&numEntries);
		utility.textEntries.clear();
		utility.textEntries.reserve(numEntries);
		for (unsigned int i = 0; i < numEntries; ++i)
		{
			std::wstring k;
			TextEntryValue v;
			stream->ReadString(k);
			stream->ReadString(v.text);
			stream->ReadString(v.gender);
			stream->ReadString(v.plural);
			utility.textEntries.emplace(std::move(k), std::move(v));
		}
		return true;
	};
	const auto write = [](FDataStreamBase* stream) {
		auto& utility = getInstance();
		stream->Write(CvUserProfile::getInstance().getLanguage());
		assert(CvGameText::NUM_LANGUAGES > 0); // This needs to be initialised!
		stream->Write(CvGameText::NUM_LANGUAGES);
		stream->Write((uint32_t)utility.textEntries.size());
		for (const auto& [k, v] : utility.textEntries)
		{
			stream->WriteString(k);
			stream->WriteString(v.text);
			stream->WriteString(v.gender);
			stream->WriteString(v.plural);
		}
	};

	return new CvCacheObject(read, write, szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createGlobalDefinesCacheObject([[maybe_unused]] const TCHAR* szCacheFileName)
{
	if constexpr (!kEnableXmlCaching)
		return nullptr;
	return new CvCacheObject{
		[](FDataStreamBase* stream) { gGlobals.getDefinesVarSystem()->Read(stream); return true; },
		[](FDataStreamBase* stream) { gGlobals.getDefinesVarSystem()->Write(stream); },
		szCacheFileName,
	};
}

CvCacheObject* MyCvDLLUtility::createTechInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readTechInfoArray, &CvGlobals::writeTechInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createBuildingInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readBuildingInfoArray, &CvGlobals::writeBuildingInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createUnitInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readUnitInfoArray, &CvGlobals::writeUnitInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createLeaderHeadInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readLeaderHeadInfoArray, &CvGlobals::writeLeaderHeadInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createCivilizationInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readCivilizationInfoArray, &CvGlobals::writeCivilizationInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createPromotionInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readPromotionInfoArray, &CvGlobals::writePromotionInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createDiplomacyInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readDiplomacyInfoArray, &CvGlobals::writeDiplomacyInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createEventInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readEventInfoArray, &CvGlobals::writeEventInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createEventTriggerInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readEventTriggerInfoArray, &CvGlobals::writeEventTriggerInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createCivicInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readCivicInfoArray, &CvGlobals::writeCivicInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createHandicapInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readHandicapInfoArray, &CvGlobals::writeHandicapInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createBonusInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readBonusInfoArray, &CvGlobals::writeBonusInfoArray>(szCacheFileName);
}

CvCacheObject* MyCvDLLUtility::createImprovementInfoCacheObject(const TCHAR* szCacheFileName)
{
	return CvCacheObject::forCvGlobals<&CvGlobals::readImprovementInfoArray, &CvGlobals::writeImprovementInfoArray>(szCacheFileName);
}

void MyCvDLLUtility::cacheSpecifyExtraXmlDependencies(CvCacheObject* pCache, std::span<const char* const> szSourceFileNames)
{
	if (pCache)
		for (const auto& path : szSourceFileNames)
			pCache->specifySource(path);
}

bool MyCvDLLUtility::cacheRead(CvCacheObject* pCache, const TCHAR* szSourceFileName)
{
	if (!pCache)
		return false;
	if (szSourceFileName)
		pCache->specifySource(szSourceFileName);
	return pCache->load();
}

bool MyCvDLLUtility::cacheWrite(CvCacheObject* pCache)
{
	if (pCache)
		pCache->store();
	return true;
}

void MyCvDLLUtility::destroyCache(CvCacheObject*& pCache)
{
	delete pCache;
	pCache = nullptr;
}

bool MyCvDLLUtility::fileManagerEnabled()
{
	return true;
}

void MyCvDLLUtility::logMsg(const TCHAR* pLogFileName, const TCHAR* pBuf, bool bWriteToConsole, [[maybe_unused]] bool bTimeStamp)
{
	// NOTE: For consistency, I've changed callers of this function to not add an extra newline.

	static std::unordered_map<std::string, std::ofstream> files;

	std::ofstream& file = files.try_emplace(pLogFileName, pLogFileName).first->second;
	file << pBuf << '\n';
	// Re-use this argument for when I want flushing.
	if (bWriteToConsole)
		file << std::flush;

	//std::clog << pLogFileName << ": " << pBuf << std::endl;
}

void MyCvDLLUtility::logMemState(const char* msg)
{
	// Used to call outputDebugString directly as the landmark heuristic did some logging from a worker thread.
	//outputDebugString(msg);
	//outputDebugString("\n");
	std::clog << msg << '\n';
}

int MyCvDLLUtility::getSymbolID(int iID)
{
	return CvApp::getInstance().symbols[iID];
}

void MyCvDLLUtility::setSymbolID(int iID, int iValue)
{
	CvApp::getInstance().symbols[iID] = (wchar_t)iValue;
}

CvWString MyCvDLLUtility::getTextGeneric(const CvWString& szIDTag, std::span<const TextArg> args)
{
	if (szIDTag.empty())
		return szIDTag;

	const auto it = textEntries.find(szIDTag);
	if (it == textEntries.end())
		return szIDTag;

	return CvTranslator().formatText(it->second.text, args);
}

// TXT_KEY_CIV_BYZANTIUM_ADJECTIVE uses a semicolon.
static bool isTextFormDelim(wchar_t c)
{
	return c == L':' || c == L';';
}

CvWString MyCvDLLUtility::getObjectText(const CvWString& szIDTag, unsigned int uiForm, bool bNoSubs)
{
	if (szIDTag.empty())
		return szIDTag;

	const auto it = textEntries.find(szIDTag);
	if (it == textEntries.end())
		return szIDTag;

	// Seems we must split at only "top-level" colons.
	const std::wstring_view text = it->second.text;

	const auto findOffsetToDelim = [text](size_t start) {
		int bracketDepth = 0;
		for (const size_t i : range(start, text.size()))
		{
			switch (text[i])
			{
			case L'[': ++bracketDepth; break;
			case L']': --bracketDepth; break;
			default:
				if (bracketDepth == 0 && isTextFormDelim(text[i]))
					return i;
				break;
			}
		}
		return text.size();
		};

	size_t start = 0;
	size_t end = findOffsetToDelim(start);

	for ([[maybe_unused]] const unsigned int formScanIndex : range(uiForm))
	{
		// Skip consecutive deliminators.
		// There can sometimes be double colons, as in TXT_KEY_CIV_NATIVE_AMERICA_ADJECTIVE. I think they're supposed to be collapsed into one.
		while (end < text.size() && isTextFormDelim(text[end]))
			++end;
		start = end;
		end = findOffsetToDelim(start);
	}

	// If out of range, then just use all of the text. It's probably the case that the same form text gets used for all forms.
	std::wstring str{ start < text.size() ? text.substr(start, end - start) : text };

	// No substitutions?
	if (bNoSubs)
		return str;
	else
		return CvTranslator().formatText(str, {});
}



void MyCvDLLUtility::addText(const TCHAR* szIDTag, const wchar_t* szString, const wchar_t* szGender, const wchar_t* szPlural)
{
	TextEntryValue value{ szString, szGender, szPlural };

	// Take this opportunity to validate all text.

	const size_t numFormsT = std::ranges::count_if(value.text, isTextFormDelim) + 1;
	const size_t numFormsG = std::ranges::count_if(value.gender, isTextFormDelim) + 1;
	const size_t numFormsP = std::ranges::count_if(value.plural, isTextFormDelim) + 1;
	const size_t numForms = std::max({ numFormsT, numFormsG, numFormsP });
	if (!((numFormsT == numForms || numFormsT == 1)
		&& (numFormsG == numForms || numFormsG == 1)
		&& (numFormsP == numForms || numFormsP == 1))
		)
		if (std::string_view(szIDTag) != "TXT_KEY_CIV_NATIVE_AMERICA_ADJECTIVE") // The one invalid text, has a double colon.
			throw std::runtime_error("Number of forms mismatch in " + std::string(szIDTag));

	//if (std::string_view(szIDTag) == "TXT_KEY_CLIMATE_TEMPERATE")
	//	_CrtDbgBreak();

	if (value.text.find_first_of(L"<&") != value.text.npos)
	{
		// Escape all text back to XML.
		// To test: Check that INTERFACE_CITY_UNHAPPY and Knight tooltip work.
		std::wstring xml;
		for (const wchar_t c : value.text)
			if (c == '<')
				xml += L"&lt;";
			else if (c == L'&')
				xml += L"&amp;";
			else
				xml += c;
		value.text = std::move(xml);
	}

	// Converting to wstring as that's what's used all the time for some reason.
	textEntries[CvWString(szIDTag)] = std::move(value);
}

unsigned int MyCvDLLUtility::getNumForms(CvWString szIDTag)
{
	return (unsigned int)(std::ranges::count(textEntries.at(szIDTag).text, L':') + 1);
}

WorldSizeTypes MyCvDLLUtility::getWorldSize()
{
	std::abort();
}

unsigned int MyCvDLLUtility::getFrameCounter() const
{
	std::abort();
}

bool MyCvDLLUtility::altKey()
{
	return CvApp::getInstance().getLastModifierKeysState().alt;
}

bool MyCvDLLUtility::shiftKey()
{
	return CvApp::getInstance().getLastModifierKeysState().shift;
}

bool MyCvDLLUtility::ctrlKey()
{
	return CvApp::getInstance().getLastModifierKeysState().ctrl;
}

bool MyCvDLLUtility::scrollLock()
{
	return false;
}

bool MyCvDLLUtility::capsLock()
{
	return false;
}

bool MyCvDLLUtility::numLock()
{
	return false;
}

void MyCvDLLUtility::ProfilerBegin()
{
}

void MyCvDLLUtility::ProfilerEnd()
{
}

void MyCvDLLUtility::BeginSample(ProfileSample*)
{
}

void MyCvDLLUtility::EndSample(ProfileSample*)
{
}

bool MyCvDLLUtility::isGameInitializing()
{
	return false;
}

void MyCvDLLUtility::enumerateFiles(std::vector<CvString>& files, const char* szPattern)
{
	// Usages:
	// gDLL->enumerateFiles(aszFiles, "modules\\*_GlobalDefines.xml");
	// gDLL->enumerateFiles(aszModularFiles, "modules\\*_PythonCallbackDefines.xml");
	// gDLL->enumerateFiles(aszFiles, "xml\\text\\*.xml");
	// gDLL->enumerateFiles(aszModfiles, "modules\\*_CIV4GameText.xml");
	// gDLL->enumerateFiles(aszFiles, CvString::format("modules\\*_%s.xml", szFileRoot).c_str());
	// gDLL->enumerateFiles(aszFiles, CvString::format("modules\\*_%s.xml", szFileRoot).c_str());

	const std::string_view patternSV(szPattern);
	const size_t numWildcards = std::ranges::count(patternSV, '*');
	if (numWildcards != 1)
		throw std::runtime_error(__func__ + std::string(": Not a normal wildcard pattern."));
	const size_t wildcardStart = patternSV.find('*');
	const std::string_view prefix = patternSV.substr(0, wildcardStart);
	const std::string_view suffix = patternSV.substr(wildcardStart + 1);
	const std::filesystem::path prefixPath(prefix);

	files = gVFS->enumerateExtRecursive(prefixPath.parent_path(), prefixPath.filename().wstring(), std::filesystem::path(suffix).wstring())
		| std::views::transform([](const std::filesystem::path& path) { return CvString(path.string()); })
		| std::ranges::to<std::vector>();
}

void MyCvDLLUtility::enumerateModuleFiles([[maybe_unused]] std::vector<CvString>& aszFiles, [[maybe_unused]] const CvString& refcstrRootDirectory, [[maybe_unused]] const CvString& refcstrModularDirectory, [[maybe_unused]] const CvString& refcstrExtension, [[maybe_unused]] bool bSearchSubdirectories)
{
	std::abort();
}

void MyCvDLLUtility::SaveGame([[maybe_unused]] SaveGameTypes eSaveGame)
{
	const CvInitCore& initCore = gGlobals.getInitCore();
	const int turn = initCore.getGameTurn();
	CvWString dateStr;
	CvGameTextMgr::GetInstance().setTimeStr(dateStr, turn, true);

	const std::wstring defFilename = static_cast<const std::wstring&>(initCore.getGameName())
		+ L" turn " + std::to_wstring(turn) + L' ' + static_cast<const std::wstring&>(dateStr)
		+ L".CivBeyondSwordSave";

	const std::filesystem::path defPath = getUserDataDir() / kSavesDirName / kSavesSingleDirName / defFilename;

	if (const auto optPath = promptSaveFilePath(defPath))
	{
		FFile<StdRawBinaryStream> file{ StdRawBinaryStream(*optPath, std::ios::out | std::ios::binary) };
		CvApp::getInstance().serialise(file);// , "test gamecore dump.bin");
	}
}

void MyCvDLLUtility::LoadGame()
{
	if (const auto optPath = promptLoadFilePath(getUserDataDir() / kSavesDirName / kSavesSingleDirName))
		CvApp::getInstance().deferLoadGame(*optPath);
}

int MyCvDLLUtility::loadReplays(std::vector<CvReplayInfo*>& listReplays)
{
	const std::filesystem::path& dir = getUserDataDir() / kReplaysDirName;

	for (const auto& dirEntry : std::filesystem::directory_iterator(dir))
	{
		if (dirEntry.path().extension() == ".CivBeyondSwordReplay")
		{
			FFile<StdRawBinaryStream> file(dirEntry.path(), std::ios::in);
			auto info = std::make_unique<CvReplayInfo>();
			info->read(file);
			if (info->getModName() == gVFS->getModRelPathString())
				listReplays.push_back(info.release()); // Caller deallocates.
		}
	}

	// TODO: What is this value used for? (nothing, currently)
	return 0;
}

void MyCvDLLUtility::QuickSave()
{
	std::abort();
}

void MyCvDLLUtility::QuickLoad()
{
	std::abort();
}

void MyCvDLLUtility::sendPbemTurn([[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}

void MyCvDLLUtility::getPassword([[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}

bool MyCvDLLUtility::getGraphicOption(GraphicOptionTypes eGraphicOption)
{
	switch (eGraphicOption)
	{
		// Show city radius when settler selected.
	case GRAPHICOPTION_CITY_RADIUS: return false;
	case GRAPHICOPTION_CITY_DETAIL: return true;
	default:
		break;
	}
	std::abort();
}

bool MyCvDLLUtility::getPlayerOption(PlayerOptionTypes ePlayerOption)
{
	return CvUserProfile::getInstance().getPlayerOption(ePlayerOption);
}

int MyCvDLLUtility::getMainMenu()
{
	std::abort();
}

bool MyCvDLLUtility::isFMPMgrHost()
{
	std::abort();
}

bool MyCvDLLUtility::isFMPMgrPublic()
{
	std::abort();
}

void MyCvDLLUtility::handleRetirement([[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}

PlayerTypes MyCvDLLUtility::getFirstBadConnection()
{
	std::abort();
}

int MyCvDLLUtility::getConnState([[maybe_unused]] PlayerTypes ePlayer)
{
	std::abort();
}

bool MyCvDLLUtility::ChangeINIKeyValue([[maybe_unused]] const char* szGroupKey, [[maybe_unused]] const char* szKeyValue, [[maybe_unused]] const char* szOut)
{
	std::abort();
}

char* MyCvDLLUtility::md5String([[maybe_unused]] char* szString)
{
	std::abort();
}

const char* MyCvDLLUtility::getModName(bool bFullPath) const
{
	return gVFS->getModName(bFullPath);
}

bool MyCvDLLUtility::hasSkippedSaveChecksum() const
{
	// This is called by victory screen:
	// if (gc.getGame().hasSkippedSaveChecksum()) :
	//     optionsLines.append(self.BuffyWarningSkippedCheckSum)
	// Just return true as this isn't the real Civ4 Engine.
	return true;
}

void MyCvDLLUtility::reportStatistics()
{
	std::clog << __FUNCTION__ << std::endl;
}
