#pragma once

#include "CvEnums.h"
#include "CvString.h"
#include "FDataStreamBase.h"

#define FASSERT_BOUNDS(lower,upper,index,fnString)\
	if (index < lower)\
	{\
		char acOut[256];\
		sprintf(acOut, "Index in %s expected to be >= %d", fnString, lower);\
		FAssertMsg(index >= lower, acOut);\
	}\
	else if (index >= upper)\
	{\
		char acOut[256];\
		sprintf(acOut, "Index in %s expected to be < %d", fnString, upper);\
		FAssertMsg(index < upper, acOut);\
	}

class CvInitCore
{

public:
	DllExport CvInitCore();
	DllExport virtual ~CvInitCore();

	DllExport void init(GameMode eMode);

protected:

	void uninit();
	void reset(GameMode eMode);

	void setDefaults();

	bool checkBounds( int iValue, int iLower, int iUpper ) const;

public:

	// **************************
	// Applications of data
	// **************************
	DllExport bool getHuman(PlayerTypes eID) const;
	DllExport int getNumHumans() const;

	DllExport int getNumDefinedPlayers() const;

	DllExport bool getMultiplayer() const;
	DllExport bool getNewGame() const;
	DllExport bool getSavedGame() const;
	DllExport bool getGameMultiplayer() const;

	DllExport bool getPitboss() const;
	DllExport bool getHotseat() const;
	DllExport bool getPbem() const;

	DllExport bool getSlotVacant(PlayerTypes eID) const;
	DllExport PlayerTypes getAvailableSlot();
	DllExport void reassignPlayer(PlayerTypes eOldID, PlayerTypes eNewID);

	DllExport void closeInactiveSlots();
	DllExport void reopenInactiveSlots();

	DllExport void resetGame();
	DllExport void resetGame(CvInitCore * pSource, bool bClear = true, bool bSaveGameType = false);
	DllExport void resetPlayers();
	DllExport void resetPlayers(CvInitCore * pSource, bool bClear = true, bool bSaveSlotInfo = false);
	DllExport void resetPlayer(PlayerTypes eID);
	DllExport void resetPlayer(PlayerTypes eID, CvInitCore * pSource, bool bClear = true, bool bSaveSlotInfo = false);

	// **************************
	// Member access
	// **************************
	DllExport const CvWString& getGameName() const;
	DllExport void setGameName(const CvWString& szGameName);

	DllExport const CvWString& getGamePassword() const;
	DllExport void setGamePassword(const CvWString& szGamePassword);

	DllExport const CvWString& getAdminPassword() const;
	DllExport void setAdminPassword(const CvWString & szAdminPassword, bool bEncrypt = true);

	DllExport CvWString getMapScriptName() const;		
	DllExport void setMapScriptName(const CvWString & szMapScriptName);
	DllExport bool getWBMapScript() const;

	DllExport bool getWBMapNoPlayers() const;
	DllExport void setWBMapNoPlayers(bool bValue);

	DllExport WorldSizeTypes getWorldSize() const;
	DllExport void setWorldSize(WorldSizeTypes eWorldSize);
	DllExport void setWorldSize(const CvWString & szWorldSize);
	DllExport const CvWString & getWorldSizeKey(CvWString & szBuffer) const;

	// fortsnek: New world size multiplier.
	DllExport int getWorldSizeMultiplier() const;
	DllExport void setWorldSizeMultiplier(int);

	DllExport ClimateTypes getClimate() const;
	DllExport void setClimate(ClimateTypes eClimate);
	DllExport void setClimate(const CvWString & szClimate);
	DllExport const CvWString & getClimateKey(CvWString & szBuffer) const;

	DllExport SeaLevelTypes getSeaLevel() const;
	DllExport void setSeaLevel(SeaLevelTypes eSeaLevel);
	DllExport void setSeaLevel(const CvWString & szSeaLevel);
	DllExport const CvWString & getSeaLevelKey(CvWString & szBuffer) const;

	DllExport EraTypes getEra() const;
	DllExport void setEra(EraTypes eEra);
	DllExport void setEra(const CvWString & szEra);
	DllExport const CvWString & getEraKey(CvWString & szBuffer) const;

	DllExport GameSpeedTypes getGameSpeed() const;
	DllExport void setGameSpeed(GameSpeedTypes eGameSpeed);
	DllExport void setGameSpeed(const CvWString & szGameSpeed);
	DllExport const CvWString & getGameSpeedKey(CvWString & szBuffer) const;

	DllExport TurnTimerTypes getTurnTimer() const;
	DllExport void setTurnTimer(TurnTimerTypes eTurnTimer);
	DllExport void setTurnTimer(const CvWString & szTurnTimer);
	DllExport const CvWString & getTurnTimerKey(CvWString & szBuffer) const;

	DllExport CalendarTypes getCalendar() const;
	DllExport void setCalendar(CalendarTypes eCalendar);
	DllExport void setCalendar(const CvWString & szCalendar);
	DllExport const CvWString & getCalendarKey(CvWString & szBuffer) const;


	DllExport int getNumCustomMapOptions() const;
	DllExport int getNumHiddenCustomMapOptions() const;

	DllExport const CustomMapOptionTypes* getCustomMapOptions() const;
	DllExport void setCustomMapOptions(int iNumCustomMapOptions, const CustomMapOptionTypes * aeCustomMapOptions);

	DllExport CustomMapOptionTypes getCustomMapOption(int iOptionID) const;
	DllExport void setCustomMapOption(int iOptionID, CustomMapOptionTypes eCustomMapOption);


	DllExport int getNumVictories() const;

	DllExport const bool* getVictories() const;
	DllExport void setVictories(int iNumVictories, const bool * abVictories);

	DllExport bool getVictory(VictoryTypes eVictoryID) const;
	DllExport void setVictory(VictoryTypes eVictoryID, bool bVictory);


	DllExport const bool* getOptions() const;
	DllExport bool getOption(GameOptionTypes eIndex) const;
	DllExport void setOption(GameOptionTypes eIndex, bool bOption);

	DllExport const bool* getMPOptions() const;
	DllExport bool getMPOption(MultiplayerOptionTypes eIndex) const;
	DllExport void setMPOption(MultiplayerOptionTypes eIndex, bool bMPOption);

	DllExport bool getStatReporting() const;
	DllExport void setStatReporting(bool bStatReporting);

	DllExport const bool* getForceControls() const;
	DllExport bool getForceControl(ForceControlTypes eIndex) const;
	DllExport void setForceControl(ForceControlTypes eIndex, bool bForceControl);


	DllExport int getGameTurn() const;
	DllExport void setGameTurn(int iGameTurn);

	DllExport int getMaxTurns() const;
	DllExport void setMaxTurns(int iMaxTurns);

	DllExport int getPitbossTurnTime() const;
	DllExport void setPitbossTurnTime(int iPitbossTurnTime);

	DllExport int getTargetScore() const;
	DllExport void setTargetScore(int iTargetScore);


	DllExport int getMaxCityElimination() const;
	DllExport void setMaxCityElimination(int iMaxCityElimination);

	DllExport int getNumAdvancedStartPoints() const;
	DllExport void setNumAdvancedStartPoints(int iNumPoints);

	DllExport unsigned int getSyncRandSeed() const;
	DllExport void setSyncRandSeed(unsigned int uiSyncRandSeed);

	DllExport unsigned int getMapRandSeed() const;
	DllExport void setMapRandSeed(unsigned int uiMapRandSeed);

	DllExport PlayerTypes getActivePlayer() const;
	DllExport void setActivePlayer(PlayerTypes eActivePlayer);

	DllExport GameType getType() const;
	DllExport void setType(GameType eType);
	DllExport void setType(const CvWString & szType);

	DllExport GameMode getMode() const;
	DllExport void setMode(GameMode eMode);


	DllExport const CvWString & getLeaderName(PlayerTypes eID, unsigned int uiForm = 0) const;
	DllExport void setLeaderName(PlayerTypes eID, const CvWString & szLeaderName);
	DllExport const CvWString & getLeaderNameKey(PlayerTypes eID) const;

	DllExport const CvWString & getCivDescription(PlayerTypes eID, unsigned int uiForm = 0) const;
	DllExport void setCivDescription(PlayerTypes eID, const CvWString & szCivDescription);
	DllExport const CvWString & getCivDescriptionKey(PlayerTypes eID) const;

	DllExport const CvWString & getCivShortDesc(PlayerTypes eID, unsigned int uiForm = 0) const;
	DllExport void setCivShortDesc(PlayerTypes eID, const CvWString & szCivShortDesc);
	DllExport const CvWString & getCivShortDescKey(PlayerTypes eID) const;

	DllExport const CvWString & getCivAdjective(PlayerTypes eID, unsigned int uiForm = 0) const;
	DllExport void setCivAdjective(PlayerTypes eID, const CvWString & szCivAdjective);
	DllExport const CvWString & getCivAdjectiveKey(PlayerTypes eID) const;

	DllExport const CvWString & getCivPassword(PlayerTypes eID) const;
	DllExport void setCivPassword(PlayerTypes eID, const CvWString & szCivPassword, bool bEncrypt = true);

	DllExport const CvString & getEmail(PlayerTypes eID) const;
	DllExport void setEmail(PlayerTypes eID, const CvString & szEmail);

	DllExport const CvString & getSmtpHost(PlayerTypes eID) const;
	DllExport void setSmtpHost(PlayerTypes eID, const CvString & szHost);

	DllExport bool getWhiteFlag(PlayerTypes eID) const;
	DllExport void setWhiteFlag(PlayerTypes eID, bool bWhiteFlag);

	DllExport const CvWString & getFlagDecal(PlayerTypes eID) const;
	DllExport void setFlagDecal(PlayerTypes eID, const CvWString & szFlagDecal);


	DllExport CivilizationTypes getCiv(PlayerTypes eID) const;
	DllExport void setCiv(PlayerTypes eID, CivilizationTypes eCiv);

	DllExport LeaderHeadTypes getLeader(PlayerTypes eID) const;
	DllExport void setLeader(PlayerTypes eID, LeaderHeadTypes eLeader);

	DllExport TeamTypes getTeam(PlayerTypes eID) const;
	DllExport void setTeam(PlayerTypes eID, TeamTypes eTeam);

	DllExport HandicapTypes getHandicap(PlayerTypes eID) const;
	DllExport void setHandicap(PlayerTypes eID, HandicapTypes eHandicap);

	DllExport PlayerColorTypes getColor(PlayerTypes eID) const;
	DllExport void setColor(PlayerTypes eID, PlayerColorTypes eColor);

	DllExport ArtStyleTypes getArtStyle(PlayerTypes eID) const;
	DllExport void setArtStyle(PlayerTypes eID, ArtStyleTypes eArtStyle);


	DllExport SlotStatus getSlotStatus(PlayerTypes eID) const;
	DllExport void setSlotStatus(PlayerTypes eID, SlotStatus eSlotStatus);

	DllExport SlotClaim getSlotClaim(PlayerTypes eID) const;
	DllExport void setSlotClaim(PlayerTypes eID, SlotClaim eSlotClaim);


	DllExport bool getPlayableCiv(PlayerTypes eID) const;
	DllExport void setPlayableCiv(PlayerTypes eID, bool bPlayableCiv);

	DllExport bool getMinorNationCiv(PlayerTypes eID) const;
	DllExport void setMinorNationCiv(PlayerTypes eID, bool bMinorNationCiv);


	DllExport int getNetID(PlayerTypes eID) const;
	DllExport void setNetID(PlayerTypes eID, int iNetID);

	DllExport bool getReady(PlayerTypes eID) const;
	DllExport void setReady(PlayerTypes eID, bool bReady);

	DllExport const CvString & getPythonCheck(PlayerTypes eID) const;
	DllExport void setPythonCheck(PlayerTypes eID, const CvString & iPythonCheck);

	DllExport const CvString & getXMLCheck(PlayerTypes eID) const;
	DllExport void setXMLCheck(PlayerTypes eID, const CvString & iXMLCheck);
									
	DllExport void resetAdvancedStartPoints();

	DllExport virtual void read(FDataStreamBase* pStream);
	DllExport virtual void write(FDataStreamBase* pStream);

protected:

	void clearCustomMapOptions();
	void refreshCustomMapOptions();

	void clearVictories();
	void refreshVictories();

	// ***
	// CORE GAME INIT DATA
	// ***

	// Game type
	GameType m_eType;

	// Descriptive strings about game and map
	CvWString m_szGameName;
	CvWString m_szGamePassword;
	CvWString m_szAdminPassword;
	CvWString m_szMapScriptName;
	
	bool m_bWBMapNoPlayers;

	// Standard game parameters
	WorldSizeTypes m_eWorldSize;
	int m_iWorldSizeMultiplier;
	ClimateTypes m_eClimate;
	SeaLevelTypes m_eSeaLevel;
	EraTypes m_eEra;
	GameSpeedTypes m_eGameSpeed;
	TurnTimerTypes m_eTurnTimer;
	CalendarTypes m_eCalendar;

	// Map-specific custom parameters
	int m_iNumCustomMapOptions;
	int m_iNumHiddenCustomMapOptions;
	CustomMapOptionTypes * m_aeCustomMapOptions;

	// Standard game options
	bool* m_abOptions;
	bool* m_abMPOptions;
	bool m_bStatReporting;

	bool* m_abForceControls;

	// Dynamic victory condition setting
	int m_iNumVictories;
	bool * m_abVictories;

	// Game turn mgmt
	int m_iGameTurn;
	int m_iMaxTurns;
	int m_iPitbossTurnTime;
	int m_iTargetScore;

	// Number of city eliminations permitted
	int m_iMaxCityElimination;

	int m_iNumAdvancedStartPoints;

	// Unsaved data
	unsigned int m_uiSyncRandSeed;
	unsigned int m_uiMapRandSeed;
	PlayerTypes m_eActivePlayer;
	GameMode m_eMode;

	// Temp var so we don't return locally scoped var
	mutable CvWString m_szTemp;
	mutable CvString m_szTempA;


	// ***
	// CORE PLAYER INIT DATA
	// ***

	// Civ details
	CvWString* m_aszLeaderName;
	CvWString* m_aszCivDescription;
	CvWString* m_aszCivShortDesc;
	CvWString* m_aszCivAdjective;
	CvWString* m_aszCivPassword;
	CvString* m_aszEmail;
	CvString* m_aszSmtpHost;

	bool* m_abWhiteFlag;
	CvWString* m_aszFlagDecal;

	CivilizationTypes* m_aeCiv;
	LeaderHeadTypes* m_aeLeader;
	TeamTypes* m_aeTeam;
	HandicapTypes* m_aeHandicap;
	PlayerColorTypes* m_aeColor;
	ArtStyleTypes* m_aeArtStyle;

	// Slot data
	SlotStatus* m_aeSlotStatus;
	SlotClaim* m_aeSlotClaim;

	// Civ flags
	bool* m_abPlayableCiv;
	bool* m_abMinorNationCiv;

	// Unsaved player data
	int* m_aiNetID;
	bool* m_abReady;

	CvString* m_aszPythonCheck;
	CvString* m_aszXMLCheck;
	mutable CvString m_szTempCheck;
};
