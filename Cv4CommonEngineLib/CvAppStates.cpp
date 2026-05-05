#include "inc/Cv4CommonEngineLib/CvAppStates.h"
#include "inc/Cv4CommonEngineLib/CvUserProfile.h"
#include "inc/Cv4CommonEngineLib/MyCvDLLPython.h"
#include "inc/Cv4CommonEngineLib/CvTranslator.h"
#include "inc/Cv4CommonEngineLib/CivIni.h"
#include "inc/Cv4CommonEngineLib/DLLMessageQueue.h"
#include "inc/Cv4CommonEngineLib/CvInterface.h"
#include "inc/Cv4CommonEngineLib/CvEngine.h"
#include "inc/Cv4CommonEngineLib/CvVFS.h"
#include "CommonEngineGlobal.h"
#include "CvAppUtil.h"

#include <CvGameCoreDLL/CvEventReporter.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvTeamAI.h>
#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvMapGenerator.h>
#include <CvGameCoreDLL/CvInfos.h>

#include <CommonStuff/range.h>
#include <CommonStuff/System.h>

#include <iostream>
#include <chrono>
#include <random>

using heck::range;

using namespace cvengine;
using namespace cvengine::app;



// This just updates the human status bit based on init core slot info.
static void updatePlayersAreHuman()
{
	for (const auto playerI : range<PlayerTypes>(gGlobals.getMAX_PLAYERS()))
		CvPlayerAI::getPlayerNonInl(playerI).updateHuman();
}

static void setupStandardSinglePlayerPlayers(CvInitCore& mainInitCore, const std::wstring& playerName, HandicapTypes handicap, std::span<const SimplifiedInitCore::Player> players)
{
	// Reset all player inits.
	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
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
		mainInitCore.setCiv(i, i < static_cast<int>(players.size()) ? players[i].civ : NO_CIVILIZATION);
		mainInitCore.setTeam(i, i < static_cast<int>(players.size()) ? players[i].team : TeamTypes(i));
		mainInitCore.setLeader(i, i < static_cast<int>(players.size()) ? players[i].leader : NO_LEADER);
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

		if (size_t(i) <= 0)
		{
			mainInitCore.setSlotClaim(i, SLOTCLAIM_ASSIGNED);
			mainInitCore.setSlotStatus(i, SS_TAKEN);
		}
		else if (size_t(i) < players.size())
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
	mainInitCore.setHandicap(PlayerTypes(0), handicap);
	//mainInitCore.setTeam(PlayerTypes(0), TeamTypes(0));
	mainInitCore.setActivePlayer(PlayerTypes(0));
	//mainInitCore.setSlotStatus(PlayerTypes(0), SS_TAKEN);
	mainInitCore.setPythonCheck(PlayerTypes(0), "");
	mainInitCore.setXMLCheck(PlayerTypes(0), "placeholder xml hash"); // Possibly used only by the engine. And we don't check XML.
	mainInitCore.setLeaderName(PlayerTypes(0), playerName);
	mainInitCore.setCivPassword(PlayerTypes(0), L"");
	//mainInitCore.setTeam(PlayerTypes(0), TeamTypes(0));
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

	for (const size_t i : range(std::min<size_t>(mainInitCore.getNumCustomMapOptions(), config.customMapOptions.size())))
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

	setupStandardSinglePlayerPlayers(mainInitCore, config.playerName, config.difficulty, config.players);

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

	gCivilizationIVIni.set(kCivilizationIVIniSection_CVGAMECOREDLL_EXTENSION, kCivilizationIVIniProp_WorldSizeMultiplier, std::to_string(mainInitCore.getWorldSizeMultiplier()));

	saveCivilizationIniIfChanged();
}

template<class... Args>
static void yieldProgress(const ProgressCallback& callback, const std::wstring& textKey, const Args&... args)
{
	const std::wstring text = CvTranslator::getInstance().getText(textKey, std::array<CvDLLUtilityIFaceBase::TextArg, sizeof...(args)>{ CvDLLUtilityIFaceBase::TextArg(args)... });
	callback(text);
}

static void startNewGame(const ProgressCallback& callback, bool isScenario)
{
	yieldProgress(callback, L"TXT_KEY_INIT_INITIALIZING");

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
			(void)MyCvDLLPython().callPythonFunction(heck::toAsciiString(mainInitCore.getMapScriptName()).c_str(), "getNumCustomMapOptionValues", int(i), &result);
			mainInitCore.setCustomMapOption(i, (CustomMapOptionTypes)rng.get((unsigned short)std::clamp<long>(result, 1, UINT16_MAX)));
		}
	}

	// Calc game handicap from average. Probably of human players.
	int accHandicap = 0;
	int numHumanPlayers = 0;
	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
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
		for (const auto playerI : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
			if (mainInitCore.getCiv(playerI) == i)
				return true;
		return false;
		};

	const auto isLeaderTaken = [&](LeaderHeadTypes i) {
		for (const auto playerI : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
			if (mainInitCore.getLeader(playerI) == i)
				return true;
		return false;
		};

	std::vector<CivilizationTypes> listOfPlayableCivs(std::from_range, range<CivilizationTypes>(gGlobals.getNumCivilizationInfos()));
	std::vector<CivilizationTypes> listOfAIPlayableCivs = listOfPlayableCivs;
	std::erase_if(listOfPlayableCivs, [](CivilizationTypes civ) { return !gGlobals.getCivilizationInfo(civ).isPlayable(); });
	std::erase_if(listOfAIPlayableCivs, [](CivilizationTypes civ) { return !gGlobals.getCivilizationInfo(civ).isAIPlayable(); });

	rng.init(mainInitCore.getSyncRandSeed());

	for (const auto playerI : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
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

	for (const auto playerI : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
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
	for (const auto i : range<TeamTypes>(gGlobals.getMAX_TEAMS()))
		CvTeamAI::getTeamNonInl(i).init(i);

	std::vector<PlayerColorTypes> unusedPlayerColours(std::from_range, range<PlayerColorTypes>(gGlobals.getNumPlayerColorInfos()));

	// Bug fix: Don't double-init barbarians!
	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
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
	const PlayerTypes barbarians = PlayerTypes(gGlobals.getMAX_CIV_PLAYERS());
	mainInitCore.setTeam(barbarians, TeamTypes(gGlobals.getMAX_CIV_TEAMS()));
	mainInitCore.setSlotStatus(barbarians, SS_COMPUTER);
	mainInitCore.setNetID(barbarians, -1);
	mainInitCore.setHandicap(barbarians, (HandicapTypes)gGlobals.getDefineINT("BARBARIAN_HANDICAP"));
	mainInitCore.setCiv(barbarians, (CivilizationTypes)gGlobals.getDefineINT("BARBARIAN_CIVILIZATION"));
	mainInitCore.setLeader(barbarians, (LeaderHeadTypes)gGlobals.getDefineINT("BARBARIAN_LEADER"));
	mainInitCore.setColor(barbarians, (PlayerColorTypes)gGlobals.getCivilizationInfo(mainInitCore.getCiv(barbarians)).getDefaultPlayerColor());
	mainInitCore.setMinorNationCiv(barbarians, false);
	CvPlayerAI::getPlayerNonInl(barbarians).init(barbarians);

	yieldProgress(callback, L"TXT_KEY_INIT_SETUP_MAP");

	auto& game = gGlobals.getGame();

	const auto mapGenT0 = std::chrono::steady_clock::now();

	if (isScenario)
	{
		MyCvDLLPython python;
		long result = -1;
		if (!python.callFunction(PYWorldBuilderModule, "applyMapDesc", nullptr, &result) || result != 0)
			throw std::runtime_error(PYWorldBuilderModule ".applyMapDesc failed.");

		game.initDiplomacy();

		if (!python.callFunction(PYWorldBuilderModule, "applyInitialItems", nullptr, &result) || result != 0)
			throw std::runtime_error(PYWorldBuilderModule ".applyInitialItems failed.");
	}
	else
	{
		// Map init.
		auto& map = gGlobals.getMap();
		map.init();

		auto& mapGen = CvMapGenerator::GetInstance();
		mapGen.generateRandomMap();
		mapGen.addGameElements();

		yieldProgress(callback, L"TXT_KEY_INIT_SETUP_PLAYERS");

		// Game init.
		game.initDiplomacy();
		game.setInitialItems();
	}

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

	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_PLAYERS()))
		CvPlayerAI::getPlayerNonInl(i).setupGraphical();

	auto& map = gGlobals.getMap();
	map.setupGraphical();
}

static void preGameStart()
{
	// HACK: Limit all mission timers to 1, as there is no realtime update. Needed for Nukes.
	// Probably not needed anymore.
	//for (const auto i : range<MissionTypes>(gGlobals.getNumMissionInfos()))
	//{
	//	struct Hack : CvMissionInfo
	//	{
	//		static int& getTime(CvMissionInfo& info)
	//		{
	//			return info.*&Hack::m_iTime;
	//		}
	//	};
	//
	//	auto& info = gGlobals.getMissionInfo(i);
	//	Hack::getTime(info) = std::min(Hack::getTime(info), 1);
	//}

	cvengine::gCommonEngineConfig.interface->reset();

	auto& game = gGlobals.getGame();
	game.initSelection();

	// Not doing this. WorldView doesn't do symbols.
	//map.updateSymbolVisibility();

	CvInitCore& mainInitCore = gGlobals.getInitCore();
	mainInitCore.reopenInactiveSlots();

	game.setInitialTime(1'000'000);

	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_PLAYERS()))
		CvPlayerAI::getPlayerNonInl(i).setStartTime(game.getInitialTime());


	//game.toggleDebugMode();

}

static void loadGame(const std::filesystem::path& path)
{
	gGlobals.SetGraphicsInitialized(false);
	FFile<StdRawBinaryStream> file(path, std::ios::in);
	app::deserialise(file);
	updatePlayersAreHuman();
}

cvengine::app::SimplifiedInitCore cvengine::app::getGameSetupFromIni()
{
	constexpr auto& kSectionName = kCivilizationIVIniSection_GAME;

	IniData& ini = gCivilizationIVIni;

	SimplifiedInitCore setup{
		.gameName = ini.get(kSectionName, kCivilizationIVIniProp_GameName, CvTranslator::getInstance().getText(L"TXT_KEY_DEFAULT_GAMENAME")),
		.playerName = ini.get(kSectionName, kCivilizationIVIniProp_Alias, heck::getUserName()),
		.difficulty = static_cast<HandicapTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_QuickHandicap, gGlobals.getNumHandicapInfos(), gGlobals.getDefineINT("STANDARD_HANDICAP"))),
		.mapScriptName = ini.get(kSectionName, kCivilizationIVIniProp_Map, "Fractal"),
		.worldSizeType = static_cast<WorldSizeTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_WorldSize, gGlobals.getNumWorldInfos(), gGlobals.getInfoTypeForString("WORLDSIZE_STANDARD"))),
		.climateType = static_cast<ClimateTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_Climate, gGlobals.getNumClimateInfos(), gGlobals.getInfoTypeForString("CLIMATE_TEMPERATE"))),
		.seaLevelType = static_cast<SeaLevelTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_SeaLevel, gGlobals.getNumSeaLevelInfos(), gGlobals.getInfoTypeForString("SEALEVEL_MEDIUM"))),
		.eraType = static_cast<EraTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_Era, gGlobals.getNumEraInfos(), gGlobals.getInfoTypeForString("ERA_ANCIENT"))),
		.speedType = static_cast<GameSpeedTypes>(ini.getEnum(kSectionName, kCivilizationIVIniProp_GameSpeed, gGlobals.getNumGameSpeedInfos(), gGlobals.getInfoTypeForString("GAMESPEED_NORMAL"))),
		.customMapOptions{},
		.gameOptions{},
		.victoryOptions{},
		.players{},
		.mapSizeOverrideMultiplier = ini.get(kCivilizationIVIniSection_CVGAMECOREDLL_EXTENSION, kCivilizationIVIniProp_WorldSizeMultiplier, 1),
		.mapSeed{},
		.syncSeed{},
	};
	
	{
		const std::string& bits = ini.get(kSectionName, kCivilizationIVIniProp_VictoryConditions, "");
		setup.victoryOptions.resize(gGlobals.getNumVictoryInfos());
		for (const size_t i : range(gGlobals.getNumVictoryInfos()))
			setup.victoryOptions[i] = i < bits.size() ? bits[i] != '0' : true;
	}

	{
		const std::string& bits = ini.get(kSectionName, kCivilizationIVIniProp_GameOptions, "");
		setup.gameOptions.resize(gGlobals.getNumGameOptionInfos());
		for (const size_t i : range(gGlobals.getNumGameOptionInfos()))
			setup.gameOptions[i] = i < bits.size() ? bits[i] != '0' : gGlobals.getGameOptionInfo(static_cast<GameOptionTypes>(i)).getDefault();
	}

	{
		const unsigned int value = ini.getUnsigned(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_SyncRandSeed, 0);
		setup.syncSeed = value ? value : std::random_device()();
	}
	{
		const unsigned int value = ini.getUnsigned(kCivilizationIVIniSection_CONFIG, kCivilizationIVIniProp_MapRandSeed, 0);
		setup.mapSeed = value ? value : std::random_device()();
	}

	const size_t numPlayers = gGlobals.getWorldInfo(setup.worldSizeType).getDefaultPlayers();
	setup.players.resize(numPlayers);
	for (size_t i = 0; i < numPlayers; ++i)
	{
		setup.players[i] = {
			.team = static_cast<TeamTypes>(i),
			.leader = NO_LEADER,
			.civ = NO_CIVILIZATION,
		};
	}

	return setup;
}

NewGameStartupState::NewGameStartupState(SimplifiedInitCore simplifiedInitCore, bool saveToIni)
	: mSimplifiedInitCore(std::move(simplifiedInitCore))
	, mSaveToIni(saveToIni)
{
}
void NewGameStartupState::onEnter(const ProgressCallback& progressCallback)
{
	// Should probably do this
	CvEventReporter::getInstance().resetStatistics();

	// No idea how Civ4 handles net messages exactly.
	// We have no networking though, so we simply reset the message queue at some early point.
	DLLMessageQueue::getInstance().reset();

	gCommonEngineConfig.engine->reset();

	setupNewGame(mSimplifiedInitCore);
	if (mSaveToIni)
		updateINI();
	startNewGame(progressCallback, false);

	setPlayerOptions();

	// ""Graphics""
	yieldProgress(progressCallback, L"TXT_KEY_INIT_GRAPHICS");
	setupGraphical();


	//app.deferInGameUI();
}
void NewGameStartupState::onLeave()
{
}

StartScenarioGameState::StartScenarioGameState(const std::filesystem::path& path, const Config& config) : mPath(path), mConfig(config)
{
}
void StartScenarioGameState::onEnter(const ProgressCallback& progressCallback)
{
	MyCvDLLPython python{};

	long retValue = -1;
	if (!python.callFunction(PYWorldBuilderModule, "readDesc", pybind11::make_tuple(mPath.wstring()).ptr(), &retValue) || retValue != 0)
		throw std::runtime_error(PYWorldBuilderModule ".readDesc failed.");

	if (!python.callFunction(PYWorldBuilderModule, "isRandomMap", nullptr, &retValue))
		throw std::runtime_error(PYWorldBuilderModule ".isRandomMap failed.");

	if (retValue != 0)
		throw std::runtime_error("The given scenario is not a scenario. It is for restarting with a mod.");

	CvWString modPath;
	if (!python.callFunction(PYWorldBuilderModule, "getModPath", nullptr, &modPath))
		throw std::runtime_error(PYWorldBuilderModule ".getModPath failed.");

	if (modPath != gVFS->getModRelPathString())
		throw std::runtime_error("WB save needs mod '" + heck::convertWideToUtf8(modPath) + "'.");

	std::vector<int> encodedGameDesc;
	if (!python.callFunction(PYWorldBuilderModule, "getGameData", nullptr, &encodedGameDesc))
		throw std::runtime_error(PYWorldBuilderModule ".getGameData failed.");

	std::vector<int> encodedPlayersData;
	if (!python.callFunction(PYWorldBuilderModule, "getPlayerData", nullptr, &encodedPlayersData))
		throw std::runtime_error(PYWorldBuilderModule ".getPlayerData failed.");

	std::vector<std::wstring> encodedPlayersDesc;
	if (!python.callFunction(PYWorldBuilderModule, "getPlayerDesc", nullptr, &encodedPlayersDesc))
		throw std::runtime_error(PYWorldBuilderModule ".getPlayerDesc failed.");
	
	// Right, what goes on here... which values are actually used and when?
	// After the call to getGameData, the BTS engine calls setOption, setMPOption, setForceControl, and setVictories.
	// CvWBDesc.py calls CvGame::setStartYear.
	// Era, Calendar, MaxTurns, TargetScore, MaxCityElimination, and GameTurn are used, but do not show up in logs. They are set bypassing the DLL.
	// But not sure what effect Era has.
	// World size, climate, and sea level are also set in init core.

	const SimplifiedInitCore iniSettings = getGameSetupFromIni();

	CvInitCore& initCore = gGlobals.getInitCore();
	int gameDescAddr = 0;
	initCore.resetGame();
	initCore.resetPlayers();
	initCore.setGameName(iniSettings.gameName);
	initCore.setMode(GameMode::GAMEMODE_NORMAL);
	initCore.setPitbossTurnTime(0);
	initCore.setSyncRandSeed(iniSettings.syncSeed);
	initCore.setMapRandSeed(iniSettings.mapSeed);
	initCore.setType(GameType::GAME_SP_SCENARIO);
	initCore.setMapScriptName(mPath.wstring());
	initCore.setWorldSize(static_cast<WorldSizeTypes>(encodedGameDesc[gameDescAddr++]));
	if (initCore.getWorldSize() == NO_WORLDSIZE)
		initCore.setWorldSize(iniSettings.worldSizeType); // Use default?
	initCore.setClimate(static_cast<ClimateTypes>(encodedGameDesc[gameDescAddr++]));
	initCore.setSeaLevel(static_cast<SeaLevelTypes>(encodedGameDesc[gameDescAddr++]));
	initCore.setEra(static_cast<EraTypes>(encodedGameDesc[gameDescAddr++]));
	initCore.setGameSpeed(static_cast<GameSpeedTypes>(encodedGameDesc[gameDescAddr++]));
	initCore.setCalendar(static_cast<CalendarTypes>(encodedGameDesc[gameDescAddr++]));
	for ([[maybe_unused]] const int i : heck::range(encodedGameDesc[gameDescAddr++]))
		initCore.setOption(static_cast<GameOptionTypes>(encodedGameDesc[gameDescAddr++]), true);
	for ([[maybe_unused]] const int i : heck::range(encodedGameDesc[gameDescAddr++]))
		initCore.setMPOption(static_cast<MultiplayerOptionTypes>(encodedGameDesc[gameDescAddr++]), true);
	for ([[maybe_unused]] const int i : heck::range(encodedGameDesc[gameDescAddr++]))
		initCore.setForceControl(static_cast<ForceControlTypes>(encodedGameDesc[gameDescAddr++]), true);
	if (const int numVictoriesSpecified = encodedGameDesc[gameDescAddr++])
	{
		for (const auto i : heck::range<VictoryTypes>(gGlobals.getNumVictoryInfos()))
			initCore.setVictory(i, false);
		for ([[maybe_unused]] const int i : heck::range(numVictoriesSpecified))
			initCore.setVictory(static_cast<VictoryTypes>(encodedGameDesc[gameDescAddr++]), true);
	}
	// else, leave all enabled?
	initCore.setGameTurn(encodedGameDesc[gameDescAddr++]);
	initCore.setMaxTurns(encodedGameDesc[gameDescAddr++]);
	initCore.setMaxCityElimination(encodedGameDesc[gameDescAddr++]);
	initCore.setNumAdvancedStartPoints(encodedGameDesc[gameDescAddr++]);
	initCore.setTargetScore(encodedGameDesc[gameDescAddr++]);

	if (gameDescAddr != encodedGameDesc.size())
		throw std::runtime_error(PYWorldBuilderModule ".getGameData returned data in an unexpected format.");

	// Now set the players.
	// Note that teams are setup by the script.
	int playerIntsAddr = 0;
	int playerStrsAddr = 0;
	PlayerTypes activePlayerI = NO_PLAYER;
	bool hasSpecifiedPlayers = false;

	// This seems to be the condition.
	const auto isSpecifiedPlayer = [&](PlayerTypes playerI) {
		return initCore.getCiv(playerI) != NO_CIVILIZATION && initCore.getLeader(playerI) != NO_LEADER;
		};

	for (const auto playerI : heck::range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
	{
		const auto civType = static_cast<CivilizationTypes>(encodedPlayersData[playerIntsAddr++]);
		initCore.setCiv(playerI, civType);
		const bool isPlayable = encodedPlayersData[playerIntsAddr++] != 0;
		const auto leaderType = static_cast<LeaderHeadTypes>(encodedPlayersData[playerIntsAddr++]);
		initCore.setLeader(playerI, leaderType);
		initCore.setHandicap(playerI, static_cast<HandicapTypes>(encodedPlayersData[playerIntsAddr++]));
		initCore.setTeam(playerI, static_cast<TeamTypes>(encodedPlayersData[playerIntsAddr++]));
		initCore.setColor(playerI, static_cast<PlayerColorTypes>(encodedPlayersData[playerIntsAddr++]));
		initCore.setArtStyle(playerI, static_cast<ArtStyleTypes>(encodedPlayersData[playerIntsAddr++]));
		initCore.setMinorNationCiv(playerI, encodedPlayersData[playerIntsAddr++] != 0);
		initCore.setWhiteFlag(playerI, encodedPlayersData[playerIntsAddr++] != 0);
		
		initCore.setCivDescription(playerI, encodedPlayersDesc[playerStrsAddr++]);
		initCore.setCivShortDesc(playerI, encodedPlayersDesc[playerStrsAddr++]);
		initCore.setLeaderName(playerI, encodedPlayersDesc[playerStrsAddr++]);
		initCore.setCivAdjective(playerI, encodedPlayersDesc[playerStrsAddr++]);
		initCore.setFlagDecal(playerI, encodedPlayersDesc[playerStrsAddr++]);

		
		const bool isSpecified = isSpecifiedPlayer(playerI);
		if (isSpecifiedPlayer(playerI) && isPlayable && mConfig.activePlayerI == playerI)
			activePlayerI = mConfig.activePlayerI;

		hasSpecifiedPlayers |= isSpecified;
	}

	if (playerIntsAddr != encodedPlayersData.size())
		throw std::runtime_error(PYWorldBuilderModule ".getPlayerData returned data in an unexpected format.");

	if (playerStrsAddr != encodedPlayersDesc.size())
		throw std::runtime_error(PYWorldBuilderModule ".getPlayerDesc returned data in an unexpected format.");

	if (hasSpecifiedPlayers)
	{
		if (activePlayerI == NO_PLAYER)
		{
			// Pick the first.
			for (const auto playerI : heck::range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
				if (isSpecifiedPlayer(playerI))
				{
					activePlayerI = playerI;
					break;
				}
		}

		assert(activePlayerI != NO_PLAYER);

		// Setup slots for specified players.
		for (const auto playerI : heck::range<PlayerTypes>(gGlobals.getMAX_CIV_PLAYERS()))
		{
			if (playerI == activePlayerI)
			{
				initCore.setHandicap(playerI, iniSettings.difficulty);
				initCore.setSlotClaim(playerI, SLOTCLAIM_ASSIGNED);
				initCore.setSlotStatus(playerI, SS_TAKEN);
			}
			else if (isSpecifiedPlayer(playerI))
			{
				initCore.setHandicap(playerI, static_cast<HandicapTypes>(gGlobals.getDefineINT("AI_HANDICAP")));
				initCore.setSlotClaim(playerI, SLOTCLAIM_UNASSIGNED);
				initCore.setSlotStatus(playerI, SS_COMPUTER);
				initCore.setReady(playerI, true);
			}
			else
			{
				initCore.setSlotClaim(playerI, SLOTCLAIM_UNASSIGNED);
				initCore.setSlotStatus(playerI, SS_CLOSED);
				initCore.setReady(playerI, false);
			}
		}
	}
	else
	{
		// Init standard single player random players.
		activePlayerI = static_cast<PlayerTypes>(0);

		std::vector<SimplifiedInitCore::Player> players(gGlobals.getWorldInfo(initCore.getWorldSize()).getDefaultPlayers());
		for (size_t i = 0; i < players.size(); ++i)
			players[i] = { .team = static_cast<TeamTypes>(i), .leader = NO_LEADER, .civ = NO_CIVILIZATION };
		setupStandardSinglePlayerPlayers(initCore, {}, iniSettings.difficulty, players);
	}

	initCore.setActivePlayer(activePlayerI);
	initCore.setLeaderName(activePlayerI, iniSettings.playerName);
	initCore.setGameSpeed(iniSettings.speedType);

	initCore.closeInactiveSlots();

	startNewGame(progressCallback, true);

	setPlayerOptions();

	// ""Graphics""
	yieldProgress(progressCallback, L"TXT_KEY_INIT_GRAPHICS");
	setupGraphical();
}
void StartScenarioGameState::onLeave()
{
}

LoadGameState::LoadGameState(const std::filesystem::path& path) : mPath(path)
{
}
void LoadGameState::onEnter(const ProgressCallback& progressCallback)
{
	// No idea how Civ4 handles net messages exactly.
	// We have no networking though, so we simply reset the message queue at some early point.
	DLLMessageQueue::getInstance().reset();

	// Reset the autosave state
	// And reset before loading the save.
	gCommonEngineConfig.engine->reset();

	yieldProgress(progressCallback, L"TXT_KEY_MAIN_MENU_LOADING_SAVEGAME", mPath.wstring());
	loadGame(mPath);

	setPlayerOptions();

	yieldProgress(progressCallback, L"TXT_KEY_INIT_GRAPHICS");
	setupGraphical();
	//preGameStart();

	//app.deferInGameUI();
}
void LoadGameState::onLeave()
{
}

void InGameState::onEnter(const ProgressCallback& progressCallback)
{
	yieldProgress(progressCallback, L"TXT_KEY_INIT_SETUP_INTERFACE");

	gCommonEngineConfig.interface->reset();

	preGameStart();

	//CvEventReporter::getInstance().gameStart();

	//if (gGlobals.getInitCore().getType() == GameType::GAME_SP_NEW)
	//{
	//	FFile<StdRawBinaryStream> file{ StdRawBinaryStream("New Game.CivBeyondSwordSave", std::ios::out | std::ios::binary) };
	//	CvApp::getInstance().serialise(file);// , "test gamecore dump.bin");
	//}

	// Call this before starting the main interface. BUG needs to register some stuff.
	(void)MyCvDLLPython().callFunction(PYCivModule, "preGameStart", nullptr);

	gCommonEngineConfig.interface->startMainInterface();

	// This happens after preGameStart and interface init.
	if (gGlobals.getInitCore().getType() == GameType::GAME_SP_NEW || gGlobals.getInitCore().getType() == GameType::GAME_SP_SCENARIO)
		CvEventReporter::getInstance().gameStart();

	if (CvCity* const capital = CvPlayerAI::getPlayerNonInl(gGlobals.getGame().getActivePlayer()).getCapitalCity())
		gCommonEngineConfig.interface->lookAtPlot({ capital->getX(), capital->getY() });

	CvMap& map = gGlobals.getMap();
	map.updateMinimapColor();
	map.updateCenterUnit();

	if (const cvbot::IPlayerBotPlugin* const botLib = gCommonEngineConfig.optPlayerBotPlugin)
	{
		std::clog << "Creating player bot..." << std::endl;
		GET_PLAYER(gGlobals.getGame().getActivePlayer()).createPlayerBot(*botLib);
	}
}
void InGameState::onLeave()
{
	gCommonEngineConfig.interface->uninit();

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
	for (const auto i : range<PlayerTypes>(gGlobals.getMAX_PLAYERS()))
		CvPlayerAI::getPlayerNonInl(i).reset();
	// Reset teams too, why not.
	for (const auto i : range<TeamTypes>(gGlobals.getMAX_TEAMS()))
		CvTeamAI::getTeamNonInl(i).reset();
	gGlobals.SetGraphicsInitialized(false);

	// Reset this too.
	gGlobals.clearGiganticMapsOptimisationsLibContext();
}

std::wstring cvengine::app::extractModRelPathFromSave(const std::filesystem::path& path)
{
	FFile<StdRawBinaryStream> file(path, std::ios::in);
	uint32_t u32{};
	file.Read(&u32);
	// Can't check this until mod is determined...
	//if (u32 != (unsigned)gGlobals.getDefineINT("SAVE_VERSION"))
	//	std::abort();
	CvString str;
	file.ReadString(str);
	return heck::convertAsciiToWide(str);
}
