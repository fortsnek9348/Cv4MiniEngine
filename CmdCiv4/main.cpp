#include "CvApp.h"
#include "CvVFS.h"
#include "CvInterface.h"
#include "CvTranslator.h"
#include "DLLInterface/MyCvDLLPython.h"
#include "DLLInterface/MyCvDLLUtility.h"
#include "DLLInterface/FMPIManager.h"
#include "DLLInterface/MyCvDLLEngineIFace.h"
#include "CvUserProfile.h"
#include "TuiTextCode.h"
#include "DLLMessageQueue.h"
#include "CvEngine.h"
#include "CivIni.h"

#include <HeckTextUI/BasicControls.h>

#include <CvGlobals.h>
#include <CvXMLLoadUtility.h>
#include <CvGameTextMgr.h>
#include <CvPlayerAI.h>
#include <CvRandom.h>
#include <CvInitCore.h>
#include <CvArtFileMgr.h>
#include <CvEventReporter.h>
#include <CvGameAI.h>
#include <CvTeamAI.h>
#include <CvMap.h>
#include <CvMapGenerator.h>

#include <CommonStuff/StringConversion.h>
#include <CommonStuff/range.h>

#include <iostream>
#include <sstream>
#include <chrono>

#ifndef _WIN32
#include <linux/prctl.h>
#include <sys/prctl.h>
#endif

using heck::range;

const int kMaxCivPlayers = gGlobals.getMaxCivPlayers(); // 18
const int kMaxPlayers = kMaxCivPlayers + 1; // 19
const int kMaxTeams = kMaxPlayers;

constexpr int kASyncSeed = 9347;

class LoadingScreen
{
public:
	LoadingScreen()
	{
		mTextLabel = std::make_shared<hecktui::Label>();
		mTextLabel->setLabelAlignment(hecktui::EAlign::Center);
		mWnd->getClientArea()->addChild(mTextLabel);
		mWnd->getClientArea()->setLayout(std::make_unique<hecktui::FillLayout>());

		CvApp::getInstance().getUI().pushWindow(mWnd);

		update(L"TXT_KEY_INIT_INITIALIZING");
	}

	template<class... Args>
	void update(std::wstring_view textKey, const Args&... args)
	{
		const std::wstring text = MyCvDLLUtility::getInstance().getTextGeneric(textKey, std::array<CvDLLUtilityIFaceBase::TextArg, sizeof...(args)>{ CvDLLUtilityIFaceBase::TextArg(args)... });
		mTextLabel->setLabel(text);

		CvApp::getInstance().getUI().drawUI();
	}

	~LoadingScreen()
	{
		CvApp::getInstance().getUI().removeWindow(mWnd.get());
	}

private:
	const std::shared_ptr<hecktui::Window> mWnd = std::make_shared<hecktui::Window>(std::wstring(), hecktui::WindowConfig{
		.isDefaultFocus = true,
		.isFullscreen = true,
		.isModal = true,
		.canClose = false,
		.borderStyle = hecktui::EBorderStyle::None,
	});
	std::shared_ptr<hecktui::Label> mTextLabel;
};


// This just updates the human status bit based on init core slot info.
static void updatePlayersAreHuman()
{
	for (const auto playerI : range<PlayerTypes>(kMaxPlayers))
		CvPlayerAI::getPlayerNonInl(playerI).updateHuman();
}

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
	updatePlayersAreHuman();
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
	CvApp::getInstance().symbols.resize(MAX_NUM_SYMBOLS, L'�'); // probably
	gameTextMgr.assignFontIds(CvTranslator::kFirstSymbolCode, 25);
	CvInterface::getInstance().reset();
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
	initialiseTextCodeTags();

	loadPostMenuGlobals();
}

static void setupNewGame(const SimplifiedInitCore& config)
{
	// Written based on custom game init sequence.

	CvInitCore& mainInitCore = gGlobals.getInitCore();
	mainInitCore.setType(GAME_NONE);
	mainInitCore.setActivePlayer(NO_PLAYER);
	mainInitCore.setType(GAME_SP_NEW);

	mainInitCore.setMapRandSeed(config.mapSeed);
	mainInitCore.setSyncRandSeed(config.syncSeed);

	// Overwritten later.
	//mainInitCore.setMapScriptName(L"Pangaea");
	//for (const int i : range(mainInitCore.getNumCustomMapOptions()))
	//	mainInitCore.setCustomMapOption(i, CustomMapOptionTypes(0));

	//for (const auto i : range<PlayerTypes>(kNewGameNumPlayers))
	//{
	//	mainInitCore.setSlotStatus(i, SS_OPEN);
	//	mainInitCore.setSlotClaim(i, SLOTCLAIM_UNASSIGNED);
	//}

	updatePlayersAreHuman();

	mainInitCore.resetAdvancedStartPoints();

	//mainInitCore.setMapScriptName(L"GalacticPangaea");
	mainInitCore.setMapScriptName(config.mapScriptName);

	for (const size_t i : range(config.customMapOptions.size()))
		mainInitCore.setCustomMapOption(static_cast<int>(i), config.customMapOptions[i]);

	mainInitCore.setGameName(config.gameName);

	// These might be set directly through member access.
	mainInitCore.setWorldSize(config.worldSizeType);
	mainInitCore.setWorldSizeMultiplier(config.mapSizeOverrideMultiplier);
	mainInitCore.setClimate(config.climateType);
	mainInitCore.setSeaLevel(config.seaLevelType);
	mainInitCore.setEra(config.eraType);
	mainInitCore.setGameSpeed(config.speedType);

	//mainInitCore.setWorldSize(NO_WORLDSIZE);
	//mainInitCore.setClimate(NO_CLIMATE);
	//mainInitCore.setSeaLevel(NO_SEALEVEL);
	//mainInitCore.setEra(NO_ERA);

	for (const auto i : range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
		mainInitCore.setVictory(i, config.victoryOptions[i]);
	for (const auto i : range<GameOptionTypes>(gGlobals.getNumGameOptions()))
		mainInitCore.setOption(i, config.gameOptions[i]);
	for (const auto i : range<MultiplayerOptionTypes>(gGlobals.getNumMPOptions()))
		mainInitCore.setMPOption(i, i == MPOPTION_TAKEOVER_AI);

	//mainInitCore.setOption(GAMEOPTION_LEAD_ANY_CIV, true);
	//mainInitCore.setOption(GAMEOPTION_NO_BARBARIANS, true);

	mainInitCore.setMode(GAMEMODE_NORMAL);

	// Reset all player inits.
	for (const auto i : range<PlayerTypes>(kMaxCivPlayers))
	{
		mainInitCore.setLeaderName(i, {});
		mainInitCore.setCivDescription(i, {});
		mainInitCore.setCivShortDesc(i, {});
		mainInitCore.setCivAdjective(i, {});
		mainInitCore.setCivPassword(i, {});
		mainInitCore.setEmail(i, {});
		mainInitCore.setSmtpHost(i, {});
		mainInitCore.setFlagDecal(i, {});
		mainInitCore.setWhiteFlag(i, {});
		mainInitCore.setHandicap(i, static_cast<HandicapTypes>(gGlobals.getDefineINT("AI_HANDICAP")));
		mainInitCore.setCiv(i, i < static_cast<int>(config.players.size()) ? config.players[i].civ : NO_CIVILIZATION);
		mainInitCore.setTeam(i, i < static_cast<int>(config.players.size()) ? config.players[i].team : TeamTypes(i));
		mainInitCore.setLeader(i, i < static_cast<int>(config.players.size()) ? config.players[i].leader : NO_LEADER);
		mainInitCore.setColor(i, NO_PLAYERCOLOR);
		mainInitCore.setArtStyle(i, NO_ARTSTYLE);
		// SLOTCLAIM_UNASSIGNED / SS_OPEN.
		// Then SLOTCLAIM_UNASSIGNED / SS_COMPUTER.
		// Then SLOTCLAIM_UNASSIGNED / SS_TAKEN.
		mainInitCore.setSlotClaim(i, SLOTCLAIM_UNASSIGNED);
		mainInitCore.setSlotStatus(i, SS_TAKEN);
		mainInitCore.setPlayableCiv(i, true);
		mainInitCore.setMinorNationCiv(i, {});
		mainInitCore.setReady(i, {});
		mainInitCore.setPythonCheck(i, {});
		mainInitCore.setXMLCheck(i, {});
	}



	for (const auto i : range<PlayerTypes>(kMaxCivPlayers))
	{
		if (size_t(i) <= 0)
		{
			mainInitCore.setSlotClaim(i, SLOTCLAIM_ASSIGNED);
			mainInitCore.setSlotStatus(i, SS_TAKEN);
		}
		else if (size_t(i) < config.players.size())
		{
			mainInitCore.setSlotClaim(i, SLOTCLAIM_UNASSIGNED);
			mainInitCore.setSlotStatus(i, SS_COMPUTER);
			mainInitCore.setReady(i, true);
		}
		else
		{
			mainInitCore.setSlotClaim(i, SLOTCLAIM_UNASSIGNED);
			mainInitCore.setSlotStatus(i, SS_CLOSED);
			mainInitCore.setReady(i, false);
		}
	}

	mainInitCore.setNetID(PlayerTypes(0), 0);
	mainInitCore.setHandicap(PlayerTypes(0), config.difficulty);
	//mainInitCore.setTeam(PlayerTypes(0), TeamTypes(0));
	mainInitCore.setActivePlayer(PlayerTypes(0));
	//mainInitCore.setSlotStatus(PlayerTypes(0), SS_TAKEN);
	mainInitCore.setPythonCheck(PlayerTypes(0), "");
	mainInitCore.setXMLCheck(PlayerTypes(0), "placeholder xml hash"); // Possibly used only by the engine. And we don't check XML.
	mainInitCore.setLeaderName(PlayerTypes(0), config.playerName);
	mainInitCore.setCivPassword(PlayerTypes(0), L"");
	//mainInitCore.setTeam(PlayerTypes(0), TeamTypes(0));

	updatePlayersAreHuman();

	// Stuff set again.

	//mainInitCore.setMapScriptName(L"Pangaea");
	// Leave custom map options at default.

	//mainInitCore.setGameName(L"Fluffy's Game");

	//for (const auto i : range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
	//	mainInitCore.setVictory(i, true);
	//for (const auto i : range<GameOptionTypes>(gGlobals.getNumGameOptions()))
	//	mainInitCore.setOption(i, false);
	//for (const auto i : range<MultiplayerOptionTypes>(gGlobals.getNumMPOptions()))
	//	mainInitCore.setMPOption(i, i == MPOPTION_TAKEOVER_AI);

	// mainInitCore.setMode(GAMEMODE_NORMAL);

	// Teams/Civs/Leaders are set again.
	// Player info is set again.
}

static void updateINI()
{
	const CvInitCore& mainInitCore = gGlobals.getInitCore();

	constexpr auto& kSection = kCivilizationIVIniSection_GAME;
	CvWString temp;
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_GameName, mainInitCore.getGameName());
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_WorldSize, mainInitCore.getWorldSizeKey(temp));
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_Climate, mainInitCore.getClimateKey(temp));
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_SeaLevel, mainInitCore.getSeaLevelKey(temp));
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_Era, mainInitCore.getEraKey(temp));
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_GameSpeed, mainInitCore.getGameSpeedKey(temp));
	temp.clear();
	for (const auto i : range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
		temp.push_back(mainInitCore.getVictory(i) ? L'1' : L'0');
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_VictoryConditions, temp);
	temp.clear();
	for (const auto i : range<GameOptionTypes>(gGlobals.getNumGameOptionInfos()))
		temp.push_back(mainInitCore.getOption(i) ? L'1' : L'0');
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_GameOptions, temp);
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_Map, mainInitCore.getMapScriptName());
	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_Alias, mainInitCore.getLeaderNameKey(static_cast<PlayerTypes>(0)));

	gCivilizationIVIni.set(kSection, kCivilizationIVIniProp_QuickHandicap, gGlobals.getHandicapInfo(mainInitCore.getHandicap(static_cast<PlayerTypes>(0))).getType());

	gCivilizationIVIni.set(kCivilizationIVIniSection_CV4ENGINE, kCivilizationIVIniProp_WorldSizeMultiplier, std::to_string(mainInitCore.getWorldSizeMultiplier()));

	saveCivilizationIniIfChanged();
}

static void startNewGame(LoadingScreen& loadingScreen)
{
	gGlobals.SetGraphicsInitialized(false);

	CvInitCore& mainInitCore = gGlobals.getInitCore();
	mainInitCore.closeInactiveSlots();

	// Setup random map options.
	// These don't appear in logs. The exe is probably setting the members directly, which it really shouldn't.
	// This stuff is set between `closeInactiveSlots` and `setCustomMapOptions`, using the sync/map seed.


	// Line 177002: 19f88c Random CvRandom::get(6 Choosing world size at random)
	// Line 177275 : 19f88c Random CvRandom::get(5 Choosing climate at random)
	// Line 177278 : 19f88c Random CvRandom::get(3 Choosing sea level at random)
	// Line 177281 : 19f88c Random CvRandom::get(7 Choosing era at random)

	CvRandom rng;
	rng.init(mainInitCore.getSyncRandSeed());
	if (mainInitCore.getWorldSize() == NO_WORLDSIZE)
		mainInitCore.setWorldSize(static_cast<WorldSizeTypes>(rng.get((unsigned short)gGlobals.getNumWorldInfos())));

	
	if (mainInitCore.getClimate() == NO_CLIMATE)
		mainInitCore.setClimate(static_cast<ClimateTypes>(rng.get((unsigned short)gGlobals.getNumClimateInfos())));

	if (mainInitCore.getSeaLevel() == NO_SEALEVEL)
		mainInitCore.setSeaLevel(static_cast<SeaLevelTypes>(rng.get((unsigned short)gGlobals.getNumSeaLevelInfos())));

	if (mainInitCore.getEra() == NO_ERA)
		mainInitCore.setEra(static_cast<EraTypes>(rng.get((unsigned short)gGlobals.getNumEraInfos())));

	for (const auto i : range<CustomMapOptionTypes>(mainInitCore.getNumCustomMapOptions()))
	{
		if (mainInitCore.getCustomMapOption(i) == NO_CUSTOM_MAPOPTION) // If NO_CUSTOM_MAPOPTION, then let's assume random option is allowed.
		{
			long result = 0;
			(void)MyCvDLLPython().callPythonFunction(CvString(mainInitCore.getMapScriptName()).c_str(), "getNumCustomMapOptionValues", int(i), &result);
			mainInitCore.setCustomMapOption(i, (CustomMapOptionTypes)rng.get((unsigned short)std::clamp<long>(result, 1, UINT16_MAX)));
		}
	}

	// Calc game handicap from average. Probably of human players.
	int accHandicap = 0;
	int numHumanPlayers = 0;
	for (const auto i : range<PlayerTypes>(kMaxCivPlayers))
	{
		if (mainInitCore.getHuman(i))
		{
			accHandicap += (int)mainInitCore.getHandicap(i);
			++numHumanPlayers;
		}
	}
	gGlobals.getGame().init(static_cast<HandicapTypes>(numHumanPlayers > 0 ? accHandicap / numHumanPlayers : gGlobals.getDefineINT("STANDARD_HANDICAP"))); // TODO: Rounding?

	// Now, resolve random civs.
	// NOTE: Probably not the same selection algorithm as the exe.

	const bool isMixAndMatch = mainInitCore.getOption(GAMEOPTION_LEAD_ANY_CIV);
	const LeaderHeadTypes barbarianLeader = (LeaderHeadTypes)gGlobals.getDefineINT("BARBARIAN_LEADER");

	const auto isCivTaken = [&](CivilizationTypes i) {
		for (const auto playerI : range<PlayerTypes>(kMaxCivPlayers))
			if (mainInitCore.getCiv(playerI) == i)
				return true;
		return false;
	};

	const auto isLeaderTaken = [&](LeaderHeadTypes i) {
		for (const auto playerI : range<PlayerTypes>(kMaxCivPlayers))
			if (mainInitCore.getLeader(playerI) == i)
				return true;
		return false;
	};

	std::vector<CivilizationTypes> listOfPlayableCivs(std::from_range, range<CivilizationTypes>(gGlobals.getNumCivilizationInfos()));
	std::vector<CivilizationTypes> listOfAIPlayableCivs = listOfPlayableCivs;
	std::erase_if(listOfPlayableCivs, [](CivilizationTypes civ) { return !gGlobals.getCivilizationInfo(civ).isPlayable(); });
	std::erase_if(listOfAIPlayableCivs, [](CivilizationTypes civ) { return !gGlobals.getCivilizationInfo(civ).isAIPlayable(); });

	rng.init(mainInitCore.getSyncRandSeed());

	for (const auto playerI : range<PlayerTypes>(kMaxCivPlayers))
	{
		if (mainInitCore.getCiv(playerI) == NO_CIVILIZATION && mainInitCore.getSlotStatus(playerI) != SS_CLOSED)
		{
			const auto& list = mainInitCore.getHuman(playerI) ? listOfPlayableCivs : listOfAIPlayableCivs;

			// This is certainly not how the Firaxis engine does it. They have calls to rng.get(10000, "Choosing Civ").
			//const std::ranges::subrange choices = std::ranges::stable_partition(list, isCivTaken);
			//if (!choices.empty())
			//	mainInitCore.setCiv(playerI, choices[rng.get(static_cast<unsigned short>(choices.size()), "Choosing Civ")]);
			//else
			//{
			//	// Pick any.
			//	mainInitCore.setCiv(playerI, list[rng.get((unsigned short)list.size())]);
			//}

			// As it would just so happen, "10000" also shows up in CvPlayer::init. It's "reservoir sampling".
			// In the logs, it appears every civ choice is visited.
			// There are 36 total civs, 34 playable.
			// If you start an 11-player map, "Choosing Civ" happens 319 times. That's 34+33+32+31+30+29+28+27+26+25+24.
			// So it seems it just does reservoir sampling on remaining playable civs.
			int iBestValue = 0;
			CivilizationTypes bestCivI = NO_CIVILIZATION;
			for (const CivilizationTypes choiceCivI : list)
			{
				if (!isCivTaken(choiceCivI))
				{
					const int iValue = 1 + rng.get(10000, "Choosing Civ");

					if (iValue > iBestValue)
					{
						iBestValue = iValue;
						bestCivI = choiceCivI;
					}
				}
			}

			if (bestCivI == NO_CIVILIZATION)
			{
				// No idea if this is original behaviour.
				bestCivI = list[rng.get(static_cast<unsigned short>(list.size()))];
			}

			mainInitCore.setCiv(playerI, bestCivI);

			//std::wclog << "Player " << std::to_underlying(playerI) << " civ is " << gGlobals.getCivilizationInfo(bestCivI).getDescription() << std::endl;
		}
	}

	for (const auto playerI : range<PlayerTypes>(kMaxCivPlayers))
	{
		if (mainInitCore.getLeader(playerI) == NO_LEADER && mainInitCore.getSlotStatus(playerI) != SS_CLOSED)
		{
			const auto& civ = gGlobals.getCivilizationInfo(mainInitCore.getCiv(playerI));

			// fallback == 0: Any leader not taken and can lead the civ.
			// fallback == 1: Any leader that can lead the civ.
			// fallback == 2: Any leader.
			// I guess this is what happens. If you assign the same civ to all players, they all take leaders within that civ.
			const auto isValidLeaderChoice = [&](LeaderHeadTypes leaderI, int fallback) {
				const bool isTaken = fallback <= 0 ? isLeaderTaken(leaderI) : false;
				const bool canLead = fallback <= 1 ? (isMixAndMatch ? leaderI != barbarianLeader : civ.isLeaders(leaderI)) : true;
				return canLead && !isTaken;
				};

			//const std::vector choices = range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()) | std::views::filter(isValidLeaderChoice) | std::ranges::to<std::vector>();
			//if (!choices.empty())
			//	mainInitCore.setLeader(playerI, choices[rng.get(static_cast<unsigned short>(choices.size()), "Choosing Leader")]);
			//else
			//{
			//	const auto isValidLeaderSecondChoice = [&](LeaderHeadTypes leaderI) {
			//		return isMixAndMatch ? true : civ.isLeaders(leaderI);
			//		};
			//	const std::vector choices2 = range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()) | std::views::filter(isValidLeaderSecondChoice) | std::ranges::to<std::vector>();
			//	mainInitCore.setLeader(playerI, choices2[rng.get(static_cast<unsigned short>(choices2.size()), "Choosing Leader")]);
			//}

			LeaderHeadTypes bestLeaderI = NO_LEADER;

			for (int fallback = 0; fallback < 3 && bestLeaderI == NO_LEADER; ++fallback)
			{
				int iBestValue = 0;
				for (const auto leaderI : range<LeaderHeadTypes>(gGlobals.getNumLeaderHeadInfos()))
				{
					if (isValidLeaderChoice(leaderI, fallback))
					{
						const int iValue = 1 + rng.get(10000, "Choosing Leader");

						if (iValue > iBestValue)
						{
							iBestValue = iValue;
							bestLeaderI = leaderI;
						}
					}
				}
			}

			assert(bestLeaderI != NO_LEADER);
			mainInitCore.setLeader(playerI, bestLeaderI);

			//std::wclog << "Player " << std::to_underlying(playerI) << " leader is " << gGlobals.getLeaderHeadInfo(bestLeaderI).getDescription() << std::endl;
		}
	}

	// Init teams and players.
	for (const auto i : range<TeamTypes>(kMaxTeams))
		CvTeamAI::getTeamNonInl(i).init(i);
	
	std::vector<PlayerColorTypes> unusedPlayerColours(std::from_range, range<PlayerColorTypes>(gGlobals.getNumPlayerColorInfos()));

	// Bug fix: Don't double-init barbarians!
	for (const auto i : range<PlayerTypes>(kMaxCivPlayers))
	{
		if (mainInitCore.getSlotStatus(i) != SS_CLOSED)
		{
			const PlayerColorTypes defColour = (PlayerColorTypes)gGlobals.getCivilizationInfo(mainInitCore.getCiv(i)).getDefaultPlayerColor();
			if (std::erase(unusedPlayerColours, defColour))
			{
				mainInitCore.setColor(i, defColour);
			}
			else if (unusedPlayerColours.size())
			{
				// Already used. Pick something random.
				const PlayerColorTypes chosenColour = unusedPlayerColours[rng.get((unsigned short)unusedPlayerColours.size())];
				std::erase(unusedPlayerColours, chosenColour);
				mainInitCore.setColor(i, chosenColour);
			}
			else
			{
				// No colours left (that's a lot of players). Pick any.
				const PlayerColorTypes chosenColour = (PlayerColorTypes)rng.get((unsigned short)gGlobals.getNumPlayerColorInfos());
				mainInitCore.setColor(i, chosenColour);
			}
		}
		else
			mainInitCore.setColor(i, NO_PLAYERCOLOR);

		CvPlayerAI::getPlayerNonInl(i).init(i);
	}

	// Setup barbarians.
	const PlayerTypes barbarians = PlayerTypes(kMaxCivPlayers);
	mainInitCore.setTeam(barbarians, TeamTypes(kMaxCivPlayers));
	mainInitCore.setSlotStatus(barbarians, SS_COMPUTER);
	mainInitCore.setNetID(barbarians, -1);
	mainInitCore.setHandicap(barbarians, (HandicapTypes)gGlobals.getDefineINT("BARBARIAN_HANDICAP"));
	mainInitCore.setCiv(barbarians, (CivilizationTypes)gGlobals.getDefineINT("BARBARIAN_CIVILIZATION"));
	mainInitCore.setLeader(barbarians, (LeaderHeadTypes)gGlobals.getDefineINT("BARBARIAN_LEADER"));
	mainInitCore.setColor(barbarians, (PlayerColorTypes)gGlobals.getCivilizationInfo(mainInitCore.getCiv(barbarians)).getDefaultPlayerColor());
	mainInitCore.setMinorNationCiv(barbarians, false);
	CvPlayerAI::getPlayerNonInl(barbarians).init(barbarians);

	loadingScreen.update(L"TXT_KEY_INIT_SETUP_MAP");

	const auto mapGenT0 = std::chrono::steady_clock::now();

	// Map init.
	auto& map = gGlobals.getMap();
	map.init();

	auto& mapGen = CvMapGenerator::GetInstance();
	mapGen.generateRandomMap();
	mapGen.addGameElements();

	loadingScreen.update(L"TXT_KEY_INIT_SETUP_PLAYERS");

	// Game init.
	auto& game = gGlobals.getGame();
	game.initDiplomacy();
	game.setInitialItems();
	game.initScoreCalculation();
	game.initEvents();
	game.setFinalInitialized(true);

	const auto mapGenT1 = std::chrono::steady_clock::now();

	std::clog << "Map generated and game initialised in " << std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::duration<int>>(mapGenT1 - mapGenT0) } << std::endl;
}

static void setPlayerOptions()
{
	// This can be NO_PLAYER if loaded an AI autoplay save. Remember: Update the player options later!
	if (gGlobals.getGame().getActivePlayer() != NO_PLAYER)
	{
		CvPlayer& player = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer());
		for (const auto i : range<PlayerOptionTypes>(NUM_PLAYEROPTION_TYPES))
			player.setOption(i, gGlobals.getDLLIFaceNonInl()->getPlayerOption(i));
	}
}

static void setupGraphical()
{
	gGlobals.SetGraphicsInitialized(true);

	for (const auto i : range<PlayerTypes>(kMaxPlayers))
		CvPlayerAI::getPlayerNonInl(i).setupGraphical();

	auto& map = gGlobals.getMap();
	map.setupGraphical();
}

static void preGameStart()
{
	// HACK: Limit all mission timers to 1, as there is no realtime update. Needed for Nukes.
	for (const auto i : range<MissionTypes>(gGlobals.getNumMissionInfos()))
	{
		struct Hack : CvMissionInfo
		{
			static int& getTime(CvMissionInfo& info)
			{
				return info.*&Hack::m_iTime;
			}
		};

		auto& info = gGlobals.getMissionInfo(i);
		Hack::getTime(info) = std::min(Hack::getTime(info), 1);
	}

	CvInterface::getInstance().reset();

	auto& game = gGlobals.getGame();
	game.initSelection();

	// Not doing this. WorldView doesn't do symbols.
	//map.updateSymbolVisibility();

	CvInitCore& mainInitCore = gGlobals.getInitCore();
	mainInitCore.reopenInactiveSlots();

	game.setInitialTime(1'000'000);

	for (const auto i : range<PlayerTypes>(kMaxPlayers))
		CvPlayerAI::getPlayerNonInl(i).setStartTime(game.getInitialTime());

	
	//game.toggleDebugMode();
	
}



static void loadGame(const std::filesystem::path& path)
{
	gGlobals.SetGraphicsInitialized(false);

	FFile<StdRawBinaryStream> file(path, std::ios::in);

	CvApp::getInstance().deserialise(file);
	updatePlayersAreHuman();
}

NewGameStartupCvAppState::NewGameStartupCvAppState(SimplifiedInitCore config) : mSimplifiedInitCore(std::move(config))
{
}
void NewGameStartupCvAppState::onEnter(CvApp& app)
{
	LoadingScreen loadingScreen;

	// Should probably do this
	CvEventReporter::getInstance().resetStatistics();

	// No idea how Civ4 handles net messages exactly.
	// We have no networking though, so we simply reset the message queue at some early point.
	DLLMessageQueue::getInstance().reset();

	CvEngine::getInstance().reset();

	setupNewGame(mSimplifiedInitCore);
	updateINI();
	startNewGame(loadingScreen);

	setPlayerOptions();

	// ""Graphics""
	loadingScreen.update(L"TXT_KEY_INIT_GRAPHICS");
	setupGraphical();
	

	app.deferInGameUI();
}
void NewGameStartupCvAppState::onUpdate(CvApp&)
{
}
void NewGameStartupCvAppState::onLeave(CvApp&)
{
}

LoadGameCvAppState::LoadGameCvAppState(const std::filesystem::path& path) : mPath(path)
{
}
void LoadGameCvAppState::onEnter(CvApp& app)
{
	LoadingScreen loadingScreen;

	// No idea how Civ4 handles net messages exactly.
	// We have no networking though, so we simply reset the message queue at some early point.
	DLLMessageQueue::getInstance().reset();

	// Reset the autosave state
	// And reset before loading the save.
	CvEngine::getInstance().reset();

	loadingScreen.update(L"TXT_KEY_MAIN_MENU_LOADING_SAVEGAME", mPath.wstring());
	loadGame(mPath);
	
	setPlayerOptions();

	loadingScreen.update(L"TXT_KEY_INIT_GRAPHICS");
	setupGraphical();
	//preGameStart();

	app.deferInGameUI();

	
}
void LoadGameCvAppState::onUpdate(CvApp&)
{
}
void LoadGameCvAppState::onLeave(CvApp&)
{
}

void InGameCvAppState::onEnter(CvApp&)
{
	LoadingScreen loadingScreen;
	loadingScreen.update(L"TXT_KEY_INIT_SETUP_INTERFACE");

	CvInterface::getInstance().reset();

	preGameStart();

	//CvEventReporter::getInstance().gameStart();

	//if (gGlobals.getInitCore().getType() == GameType::GAME_SP_NEW)
	//{
	//	FFile<StdRawBinaryStream> file{ StdRawBinaryStream("New Game.CivBeyondSwordSave", std::ios::out | std::ios::binary) };
	//	CvApp::getInstance().serialise(file);// , "test gamecore dump.bin");
	//}

	// Call this before starting the main interface. BUG needs to register some stuff.
	(void)MyCvDLLPython().callFunction(PYCivModule, "preGameStart", nullptr);

	CvInterface::getInstance().startMainInterface();

	// This happens after preGameStart and interface init.
	if (gGlobals.getInitCore().getType() == GameType::GAME_SP_NEW)
		CvEventReporter::getInstance().gameStart();

	if (CvCity* const capital = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer()).getCapitalCity())
		CvInterface::getInstance().lookAt({ capital->getX(), capital->getY() });
		//CvInterface::getInstance().getWorldView().setCenterPlot({ capital->getX(), capital->getY() });

	CvMap& map = gGlobals.getMap();
	map.updateMinimapColor();
	map.updateCenterUnit();
}
void InGameCvAppState::onUpdate(CvApp&)
{
	CvInterface::getInstance().updateMainInterface();
}
void InGameCvAppState::onLeave(CvApp&)
{
	CvInterface::getInstance().uninit();

	// An unusual crash occurs if you don't do this.
	// If you start a game, settle your first city (and don't move your warrior/scout),
	// then quit to menu, then start another game, you get a crash without this reset code.
	gGlobals.getInitCore().init(GameMode::GAMEMODE_NORMAL);
	gGlobals.getLoadedInitCore().init(GameMode::GAMEMODE_NORMAL);
	gGlobals.getIniInitCore().init(GameMode::GAMEMODE_NORMAL);
	CvEventReporter::getInstance().resetStatistics();
	gGlobals.getGame().reset(NO_HANDICAP);
	CvMapInitData mapInitData{};
	gGlobals.getMap().reset(&mapInitData);
	for (const auto i : range<PlayerTypes>(kMaxPlayers))
		CvPlayerAI::getPlayerNonInl(i).reset();
	// Reset teams too, why not.
	for (const auto i : range<TeamTypes>(kMaxTeams))
		CvTeamAI::getTeamNonInl(i).reset();
	gGlobals.SetGraphicsInitialized(false);

	// Reset this too.
	gGlobals.clearGiganticMapsOptimisationsLibContext();
}


int main()
{
#ifndef _WIN32
	// Allow GDB to attach - https://man7.org/linux/man-pages/man2/pr_set_ptracer.2const.html
	prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

	// Call this first!
	std::ios::sync_with_stdio(false);

	if (heck::toWide(heck::toUtf8(L"\x1b[↑")) != L"\x1b[↑")
	{
		for (const uint8_t c : heck::toUtf8(L"\x1b[↑"))
			printf("%#02x\n", c);
		printf("-\n");
		for (const wint_t c : heck::toWide(heck::toUtf8(L"\x1b[↑")))
			printf("%#x\n", c);
		throw std::runtime_error("String conversion isn't working.");
	}

	// Start CvApp now.
	(void)CvApp::getInstance();

	initCommon(); // python initialised here

	//gGlobals.getLogging() = true;
	//gGlobals.getRandLogging() = true;
	//gGlobals.getSynchLogging() = true;
	
	const struct PythonDestroyer
	{
		~PythonDestroyer()
		{
			// Python needs to be shutdown before main ends, for some reason. Probably static destruction order issues.
			MyCvDLLPython::shutdownPython();
		}
	} dtor{};

	CvApp::getInstance().startUI();

	CvApp::getInstance().deferMainMenu();

	[[maybe_unused]] const auto standardSavesPath = getUserDataDir() / kSavesDirName / kSavesSingleDirName;
#ifdef _WIN32
	[[maybe_unused]] const auto originalSavesPath = getUserDataDir() / "Saves" / kSavesSingleDirName;
#else
	[[maybe_unused]] const auto originalSavesPath = standardSavesPath;
#endif

	//CvApp::getInstance().deferLoadGame(R"(Giant Pangea begin T209.CivBeyondSwordSave)");

	//CvApp::getInstance().deferLoadGame(getUserDataDir() / R"(Cv4MiniEngine Testing\Quirky plot help - Axolotl8 turn 0 BC-4000.CivBeyondSwordSave)");

	return CvApp::getInstance().run();
}
