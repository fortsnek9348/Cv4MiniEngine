#pragma once

#include "inc/Cv4CommonEngineLib/Common.h"

#include <CvString.h>
#include <CvEnums.h>
#include <CvStructs.h>
#include <CvDLLUtilityIFaceBase.h>


#include <vector>
#include <unordered_map>

class MyCvDLLUtility : public CvDLLUtilityIFaceBase
{
public:
	static MyCvDLLUtility& getInstance();

	

	// Quit
	//bool isDone = false;

	// AI Auto Run. Prevents Game Over if the human player disappears.
	//bool isAutorun = false;

	virtual CvDLLEntityIFaceBase* getEntityIFace() override;
	virtual CvDLLInterfaceIFaceBase* getInterfaceIFace() override;
	virtual CvDLLEngineIFaceBase* getEngineIFace() override;
	virtual CvDLLIniParserIFaceBase* getIniParserIFace() override;
	virtual CvDLLSymbolIFaceBase* getSymbolIFace() override;
	virtual CvDLLFeatureIFaceBase* getFeatureIFace() override;
	virtual CvDLLRouteIFaceBase* getRouteIFace() override;
	virtual CvDLLPlotBuilderIFaceBase* getPlotBuilderIFace() override;
	virtual CvDLLRiverIFaceBase* getRiverIFace() override;
	virtual CvDLLFAStarIFaceBase* getFAStarIFace() override;
	virtual CvDLLXmlIFaceBase* getXMLIFace() override;
	virtual CvDLLFlagEntityIFaceBase* getFlagEntityIFace() override;
	virtual CvDLLPythonIFaceBase* getPythonIFace() override;
	virtual void clearVector(std::vector<int>& vec) override;
	virtual void clearVector(std::vector<uint8_t>& vec) override;
	virtual void clearVector(std::vector<float>& vec) override;
	virtual int getAssignedNetworkID(int iPlayerID) override;
	virtual bool isConnected(int iNetID) override;
	virtual bool isGameActive() override;
	virtual int GetLocalNetworkID() override;
	virtual int GetSyncOOS(int iNetID) override;
	virtual int GetOptionsOOS(int iNetID) override;
	virtual int GetLastPing(int iNetID) override;
	virtual bool IsModem() override;
	virtual void SetModem(bool bModem) override;
	virtual void AcceptBuddy(const char* szName, int iRequestID) override;
	virtual void RejectBuddy(const char* szName, int iRequestID) override;
	virtual void messageControlLog(char* s) override;
	virtual int getChtLvl() override;
	virtual void setChtLvl(int iLevel) override;
	virtual bool GetWorldBuilderMode() override;
	virtual int getCurrentLanguage() const override;
	virtual void setCurrentLanguage(int iNewLanguage) override;
	virtual bool isModularXMLLoading() const override;
	virtual bool IsPitbossHost() const override;
	virtual CvString GetPitbossSmtpHost() const override;
	virtual CvWString GetPitbossSmtpLogin() const override;
	virtual CvWString GetPitbossSmtpPassword() const override;
	virtual CvString GetPitbossEmail() const override;
	virtual void sendMessageData(CvMessageData* pData) override;
	virtual void sendPlayerInfo(PlayerTypes eActivePlayer) override;
	virtual void sendGameInfo(const CvWString& szGameName, const CvWString& szAdminPassword) override;
	virtual void sendPlayerOption(PlayerOptionTypes eOption, bool bValue) override;
	virtual void sendChat(const CvWString& szChatString, ChatTargetTypes eTarget) override;
	virtual void sendPause(int iPauseID) override;
	virtual void sendMPRetire() override;
	virtual void sendToggleTradeMessage(PlayerTypes eWho, TradeableItems eItemType, int iData, int iOtherWho, bool bAIOffer, bool bSendToAll) override;
	virtual void sendClearTableMessage(PlayerTypes eWhoTradingWith) override;
	virtual void sendImplementDealMessage(PlayerTypes eOtherWho, CLinkList<TradeData>* pOurList, CLinkList<TradeData>* pTheirList) override;
	virtual void sendContactCiv(NetContactTypes eContactType, PlayerTypes eWho) override;
	virtual void sendOffer() override;
	virtual void sendDiploEvent(PlayerTypes eWhoTradingWith, DiploEventTypes eDiploEvent, int iData1, int iData2) override;
	virtual void sendRenegotiate(PlayerTypes eWhoTradingWith) override;
	virtual void sendRenegotiateThisItem(PlayerTypes ePlayer2, TradeableItems eItemType, int iData) override;
	virtual void sendExitTrade() override;
	virtual void sendKillDeal(int iDealID, bool bFromDiplomacy) override;
	virtual void sendDiplomacy(PlayerTypes ePlayer, CvDiploParameters* pParams) override;
	virtual void sendPopup(PlayerTypes ePlayer, CvPopupInfo* pInfo) override;
	virtual int getMillisecsPerTurn() override;
	virtual float getSecsPerTurn() override;
	virtual int getTurnsPerSecond() override;
	virtual int getTurnsPerMinute() override;
	virtual void openSlot(PlayerTypes eID) override;
	virtual void closeSlot(PlayerTypes eID) override;
	virtual CvWString getMapScriptName() override;
	virtual bool getTransferredMap() override;
	virtual bool isDescFileName(const char* szFileName) override;
	virtual bool isWBMapScript() override;
	virtual bool isWBMapNoPlayers() override;
	virtual bool pythonMapExists(const char* szMapName) override;
	virtual void stripSpecialCharacters(CvWString& szName) override;
	virtual void initGlobals() override;
	virtual void uninitGlobals() override;
	virtual void callUpdater() override;
	virtual bool Uncompress(uint8_t** bufIn, unsigned long* bufLenIn, unsigned long maxBufLenOut, int offset) override;
	virtual bool Compress(uint8_t** bufIn, unsigned long* bufLenIn, int offset) override;
	virtual void NiTextOut(const TCHAR* szText) override;
	virtual void MessageBox(const TCHAR* szText, const TCHAR* szCaption) override;
	virtual void SetDone(bool bDone) override;
	virtual bool GetDone() override;
	virtual bool GetAutorun() override;
	virtual void beginDiplomacy(CvDiploParameters* pDiploParams, PlayerTypes ePlayer) override;
	virtual void endDiplomacy() override;
	virtual bool isDiplomacy() override;
	virtual int getDiplomacyPlayer() override;
	virtual void updateDiplomacyAttitude(bool bForce) override;
	virtual bool isMPDiplomacy() override;
	virtual bool isMPDiplomacyScreenUp() override;
	virtual int getMPDiplomacyPlayer() override;
	virtual void beginMPDiplomacy(PlayerTypes eWhoTalkingTo, bool bRenegotiate, bool bSimultaneous) override;
	virtual void endMPDiplomacy() override;
	virtual bool getAudioDisabled() override;
	virtual int getAudioTagIndex(const TCHAR* szTag, int iScriptType) override;
	virtual void DoSound(int iScriptId) override;
	virtual void Do3DSound(int iScriptId, NiPoint3 vPosition) override;
	virtual FDataStreamBase* createFileStream() override;
	virtual void destroyDataStream(FDataStreamBase*& stream) override;
	virtual CvCacheObject* createGlobalTextCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createGlobalDefinesCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createTechInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createBuildingInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createUnitInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createLeaderHeadInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createCivilizationInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createPromotionInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createDiplomacyInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createEventInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createEventTriggerInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createCivicInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createHandicapInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createBonusInfoCacheObject(const TCHAR* szCacheFileName) override;
	virtual CvCacheObject* createImprovementInfoCacheObject(const TCHAR* szCacheFileName) override;

	// fortsnek: New function.
	virtual void cacheSpecifyExtraXmlDependencies(CvCacheObject* pCache, std::span<const char* const> szSourceFileNames) override;

	virtual bool cacheRead(CvCacheObject* pCache, const TCHAR* szSourceFileName) override;
	virtual bool cacheWrite(CvCacheObject* pCache) override;
	virtual void destroyCache(CvCacheObject*& pCache) override;
	virtual bool fileManagerEnabled() override;
	virtual void logMsg(const TCHAR* pLogFileName, const TCHAR* pBuf, bool bWriteToConsole, bool bTimeStamp) override;
	virtual void logMemState(const char* msg) override;
	virtual int getSymbolID(int iID) override;
	virtual void setSymbolID(int iID, int iValue) override;
	virtual CvWString getTextGeneric(const CvWString& szIDTag, std::span<const TextArg> args) override;
	virtual CvWString getObjectText(const CvWString& szIDTag, unsigned int uiForm, bool bNoSubs) override;
	virtual void addText(const TCHAR* szIDTag, const wchar_t* szString, const wchar_t* szGender, const wchar_t* szPlural) override;
	virtual unsigned int getNumForms(CvWString szIDTag) override;
	virtual WorldSizeTypes getWorldSize() override;
	virtual unsigned int getFrameCounter() const override;
	virtual bool altKey() override;
	virtual bool shiftKey() override;
	virtual bool ctrlKey() override;
	virtual bool scrollLock() override;
	virtual bool capsLock() override;
	virtual bool numLock() override;
	virtual void ProfilerBegin() override;
	virtual void ProfilerEnd() override;
	virtual void BeginSample(ProfileSample* pSample) override;
	virtual void EndSample(ProfileSample* pSample) override;
	virtual bool isGameInitializing() override;
	virtual void enumerateFiles(std::vector<CvString>& files, const char* szPattern) override;
	virtual void enumerateModuleFiles(std::vector<CvString>& aszFiles, const CvString& refcstrRootDirectory, const CvString& refcstrModularDirectory, const CvString& refcstrExtension, bool bSearchSubdirectories) override;
	virtual void SaveGame(SaveGameTypes eSaveGame) override;
	virtual void LoadGame() override;
	virtual int loadReplays(std::vector<CvReplayInfo*>& listReplays) override;
	virtual void QuickSave() override;
	virtual void QuickLoad() override;
	virtual void sendPbemTurn(PlayerTypes ePlayer) override;
	virtual void getPassword(PlayerTypes ePlayer) override;
	virtual bool getGraphicOption(GraphicOptionTypes eGraphicOption) override;
	virtual bool getPlayerOption(PlayerOptionTypes ePlayerOption) override;
	virtual int getMainMenu() override;
	virtual bool isFMPMgrHost() override;
	virtual bool isFMPMgrPublic() override;
	virtual void handleRetirement(PlayerTypes ePlayer) override;
	virtual PlayerTypes getFirstBadConnection() override;
	virtual int getConnState(PlayerTypes ePlayer) override;
	virtual bool ChangeINIKeyValue(const char* szGroupKey, const char* szKeyValue, const char* szOut) override;
	virtual char* md5String(const char* szString) override;
	virtual const std::wstring& getModName(bool bFullPath) const override;
	virtual bool hasSkippedSaveChecksum() const override;
	virtual void reportStatistics() override;
};