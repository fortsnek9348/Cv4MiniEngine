#include "GameRunScope.h"

#include <CvGameCoreDLL/CvGameAI.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvInitCore.h>
#include <CvGameCoreDLL/CvMap.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvTeamAI.h>

#include <CommonStuff/range.h>

#include <algorithm>
#include <iostream>

GameRunScope::GameRunScope()
	: maxCivPlayers(gGlobals.getMAX_CIV_PLAYERS())
	, startTurn(gGlobals.getGame().getGameTurn())
	, extraPlayerInfos(maxCivPlayers)
	, t0(Clock::now())
{
}

void GameRunScope::update()
{
	CvGame& game = gGlobals.getGame();

	const int turn = game.getGameTurn();
	for (size_t i = 0; i < extraPlayerInfos.size(); ++i)
		if (GET_PLAYER(static_cast<PlayerTypes>(i)).isAlive())
			extraPlayerInfos[i].turnLastSeenAlive = turn;
}

void GameRunScope::dumpSummary() const
{
	const auto t1 = Clock::now();
	const auto duration = std::chrono::duration<double>(t1 - t0);

	CvGame& game = gGlobals.getGame();
	const int turn = game.getGameTurn();
	const int maxCivTeams = gGlobals.getMAX_CIV_TEAMS();
	const int maxTeams = gGlobals.getMAX_TEAMS();

	// Gather players
	std::vector<const CvPlayer*> players;
	for (int i = 0; i < maxCivPlayers; ++i)
		if (CvPlayer* const player = &GET_PLAYER(static_cast<PlayerTypes>(i)); player->isEverAlive())
			players.push_back(player);

	// Gather masters.
	std::vector<TeamTypes> teamMasters(std::from_range, heck::range<TeamTypes>(maxCivTeams)); // Default to master of self
	for (const auto teamI : heck::range<TeamTypes>(maxCivTeams))
	{
		if (const CvTeam& team = GET_TEAM(teamI); team.isAlive())
		{
			for (const auto masterI : heck::range<TeamTypes>(maxTeams))
			{
				if (team.isVassal(masterI))
				{
					teamMasters[teamI] = masterI;
					break;
				}
			}
		}
	}

	// Sort as a scoreboard, as in CvMainInterface.
	std::ranges::sort(players, std::less(), [&](const CvPlayer* player) -> std::array<int, 5> {
		const TeamTypes teamI = player->getTeam();
		return { game.getTeamRank(teamMasters[teamI]), teamMasters[teamI] != teamI, game.getTeamRank(teamI), game.getPlayerRank(player->getID()), player->getID() };
		});

	const CvInitCore& initCore = gGlobals.getInitCore();
	CvMap& map = gGlobals.getMap();

	std::cout << "AI/bot autoplay ended.\n";
	std::cout << "gameName = " << heck::convertWideToUtf8(game.getName()) << '\n';
	std::cout << "options = ";
	for (const auto i : heck::range<GameOptionTypes>(NUM_GAMEOPTION_TYPES))
		if (game.isOption(i))
			std::cout << gGlobals.getGameOptionInfo(i).getType() << ", ";
	std::cout << '\n';
	std::cout << "mapScript = " << heck::convertWideToUtf8(initCore.getMapScriptName()) << '\n';
	std::cout << "mapSize = " << map.getGridWidth() << ", " << map.getGridHeight() << '\n';
	std::cout << "worldSizeType = " << gGlobals.getWorldInfo(map.getWorldSize()).getType() << '\n';
	std::cout << "seaLevel = " << gGlobals.getSeaLevelInfo(map.getSeaLevel()).getType() << '\n';
	std::cout << "climate = " << gGlobals.getClimateInfo(map.getClimate()).getType() << '\n';
	std::cout << "gameSpeed = " << gGlobals.getGameSpeedInfo(game.getGameSpeedType()).getType() << '\n';
	std::cout << "handicap = " << gGlobals.getHandicapInfo(game.getHandicapType()).getType() << '\n';
	if (initCore.getType() == GAME_SP_NEW) // Original seeds are not saved.
	{
		std::cout << "mapSeed = " << initCore.getMapRandSeed() << '\n';
		std::cout << "syncSeed = " << initCore.getSyncRandSeed() << '\n';
	}
	if (game.getVictory() != NO_VICTORY)
	{
		std::cout << "victoryTeam = " << std::to_underlying(game.getWinner()) << '\n'; // Pick any player
		std::cout << "victoryType = " << gGlobals.getVictoryInfo(game.getVictory()).getType() << '\n';
	}
	std::cout << "turn = " << turn << '\n';
	std::cout << "time = " << duration.count() << '\n';
	std::cout << "timePerTurn = " << duration.count() / (turn - startTurn) << '\n';
	for (const CvPlayer* const player : players)
	{
		const TeamTypes teamI = player->getTeam();
		if (teamMasters[teamI] != teamI)
			std::cout << '\t';
		std::cout << "Player " << std::setw(2) << static_cast<int>(player->getID());
		if (teamMasters[teamI] != teamI)
			std::cout << " vassal of team " << static_cast<int>(teamMasters[teamI]);
		if (player->isAlive())
		{
			std::cout << " alive " << std::setw(4) << game.getPlayerScore(player->getID());
			std::cout << ' ' << std::setw(2) << player->getNumCities() << " cities";
		}
		else
			std::cout << " dead since turn " << (extraPlayerInfos[player->getID()].turnLastSeenAlive + 1);
		//std::cout << ' ' << gGlobals.getLeaderHeadInfo(player->getLeaderType()).getType();
		//std::cout << ' ' << gGlobals.getCivilizationInfo(player->getCivilizationType()).getType();
		std::cout << ' ' << heck::convertWideToUtf8(gGlobals.getLeaderHeadInfo(player->getLeaderType()).getDescription());
		std::cout << " / " << heck::convertWideToUtf8(player->getCivilizationDescription());
		std::cout << '\n';
	}

	// Adjust turn slice number so that each checksum always checksums the same thing.
	const int turnSliceShift = (4 - game.getTurnSlice() % 4) % 4;
	game.changeTurnSlice(turnSliceShift);

	std::cout << "turnSlice = " << game.getTurnSlice() << '\n';
	for (const int i : heck::range(4))
	{
		std::cout << "syncChecksum" << i << " = " << game.calculateSyncChecksum() << '\n';
		game.changeTurnSlice(1);
	}

	// Revert.
	game.changeTurnSlice(-(turnSliceShift + 4));
}