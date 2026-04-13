#include "PlayerBotEnablement.h"

#if ENABLE_PLAYER_BOT

#include "PlayerBotGameInterface.h"
#include "PlayerBotUtil.h"

#include "CLinkListIterator.h"
#include "CvPlot.h"
#include "CvGameCoreUtils.h"
#include "CvUnit.h"
#include "CvCity.h"
#include "CvMap.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "CvInfos.h"
#include "CvInitCore.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvDLLEngineIFaceBase.h"

#include <PlayerBotGameBinding/GameStructs.h>

#include <CommonStuff/range.h>

#include <algorithm>
#include <iostream>
#include <functional>

using namespace cvbot;

static constinit BotInit gBotInitInstance;
static constinit AllKnowingGameInterface gAllKnowingGameInterfaceInstance;
static constinit Game gGameInstance;

static Plot convert(const CvPlot& plot, PlayerTypes activePlayer, TeamTypes activeTeam)
{
	const bool allKnowing = activeTeam == NO_TEAM;
	const bool isRevealed = plot.isRevealed(activeTeam, false);
	bool hasMyUnits = false;
	bool hasOtherUnits = false;

	for (auto* node = plot.headUnitNode(); node; node = plot.nextUnitNode(node))
	{
		const CvUnit& unit = *::getUnit(node->m_data);
		hasMyUnits |= unit.getOwner() == activePlayer;
		hasOtherUnits |= unit.getOwner() != activePlayer && !unit.isInvisible(activeTeam, false);
	}

	if (allKnowing || isRevealed)
	{
		return {
			.type = static_cast<EPlotType>(plot.getPlotType()),
			.owner = static_cast<EPlayer>(plot.getOwner()),
			.improvement = static_cast<EImprovement>(allKnowing ? plot.getImprovementType() : plot.getRevealedImprovementType(activeTeam, false)),
			.feature = static_cast<EFeature>(plot.getFeatureType()),
			.terrain = static_cast<ETerrain>(plot.getTerrainType()),
			.bonus = static_cast<EBonus>(plot.getBonusType()),
			.isVisible = plot.isVisible(activeTeam, false),
			.hasMyUnits = hasOtherUnits,
			.hasOtherVisibleUnits = hasOtherUnits,
			.hasRevealedCity = plot.getPlotCity() && (allKnowing ? true : plot.getPlotCity()->isRevealed(activeTeam, false)),
			.isRiverside = plot.isRiverSide(),
			.isLake = plot.isLake(),
			.yields{
				static_cast<int8_t>(plot.getYield(YIELD_FOOD)),
				static_cast<int8_t>(plot.getYield(YIELD_PRODUCTION)),
				static_cast<int8_t>(plot.getYield(YIELD_COMMERCE)),
			},
		};
	}
	else
	{
		return {
			.type = EPlotType::None,
			.owner = kNoPlayer,
			.improvement = static_cast<EImprovement>(NO_IMPROVEMENT),
			.feature = static_cast<EFeature>(NO_FEATURE),
			.terrain = static_cast<ETerrain>(NO_TERRAIN),
			.bonus = static_cast<EBonus>(plot.getBonusType()),
			.isVisible = plot.isVisible(activeTeam, false),
			.hasMyUnits = hasOtherUnits,
			.hasOtherVisibleUnits = false,
			.hasRevealedCity = false,
			.isRiverside = false,
			.isLake = false,
			.yields{},
		};
	}
}

static MapGeometry getMapGeometry()
{
	CvMap& map = gGlobals.getMap();
	return {
		.dim{ map.getGridWidth(), map.getGridHeight() },
		.wrapX = map.isWrapX(),
		.wrapY = map.isWrapY(),
	};
}

static void getPlots(ivec2 origin, Span2D<Plot> out, bool allKnowing)
{
	const int h = out.extent(0);
	const int w = out.extent(1);
	const MapGeometry geom = getMapGeometry();
	const CvMap& map = gGlobals.getMap();
	const PlayerTypes playerI = allKnowing ? NO_PLAYER : gGlobals.getGame().getActivePlayer();
	const TeamTypes teamI = allKnowing ? NO_TEAM : GET_PLAYER(playerI).getTeam();

	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			out[y, x] = convert(*map.plot(origin.x + x, origin.y + y), playerI, teamI);
}

static Unit convert(const CvUnit& unit, bool allKnowing)
{
	// We assume the unit is visible for this to be called.
	const PlayerTypes owner = unit.getOwner();
	const TeamTypes activeTeam = gGlobals.getGame().getActiveTeam();
	const bool isOurs = owner == gGlobals.getGame().getActivePlayer();

	const bool isAir = unit.getDomainType() == DOMAIN_AIR;
	const int baseStrength = isAir ? unit.airBaseCombatStr() : unit.baseCombatStr();
	const int strength = (baseStrength * unit.currHitPoints() + unit.maxHitPoints() / 2) / unit.maxHitPoints();

	std::bitset<kAPIMaxPromotions> promotions{};
	for (const auto i : heck::range<PromotionTypes>(gGlobals.getNumPromotionInfos()))
		promotions[i] = unit.isHasPromotion(i);

	const bool canMove = allKnowing || isOurs ? unit.canMove() : false;
	return {
		.id = static_cast<EUnitId>(isOurs ? unit.getID() : -1),
		.owner = static_cast<EPlayer>(unit.getVisualOwner(activeTeam)),
		.coord{
			static_cast<int16_t>(unit.getX()),
			static_cast<int16_t>(unit.getY()),
			},
		.type = static_cast<EUnitType>(unit.getUnitType()),
		.exp = static_cast<uint8_t>(std::clamp<int>(allKnowing || isOurs ? unit.getExperience() : 0, 0, UINT8_MAX)),
		.strength = static_cast<uint8_t>(std::clamp<int>(strength, 0, UINT8_MAX)),
		.maxStrength = static_cast<uint8_t>(std::clamp<int>(baseStrength, 0, UINT8_MAX)),
		.remMoves = static_cast<uint16_t>(std::clamp<int>(canMove ? unit.movesLeft() : 0, 0, UINT16_MAX)),
		.maxMoves = static_cast<uint16_t>(std::clamp<int>(unit.baseMoves() * gGlobals.getMOVE_DENOMINATOR(), 0, UINT16_MAX)),
		.promotions = promotions,
	};
}

/*static UnitGroup convert(CvSelectionGroup& group, bool allKnowing)
{
	const PlayerTypes owner = group.getOwner();
	const TeamTypes activeTeam = GET_PLAYER(gGlobals.getGame().getActivePlayer()).getTeam();
	const bool isOurs = owner == gGlobals.getGame().getActivePlayer();
	
	UnitGroup out{
		//.id = static_cast<EUnitGroupId>(isOurs ? group.getID() : 0),
		.owner = static_cast<EPlayer>(owner),
		//EActivity activity : 4 = EActivity::None; // None for other players.
		//EAutomation automation : 4 = EAutomation::None; // None for other players.
		//EMission mission = EMission::None; // None for other players.
		//bool hasTarget : 1{}; // False for other players.
		//i16vec2 missionTarget{}; // 0 for other players.
		//i16vec2 coord{};
	};

	if (isOurs)
	{
		out.id = static_cast<EUnitGroupId>(group.getID());
		out.activity = static_cast<EActivity>(group.getActivityType());
		out.automation = static_cast<EAutomation>(group.getAutomateType());
		out.mission = static_cast<EAutomation>(group.getMissionType(0));
		if (const CvPlot* const target = group.lastMissionPlot())
		{
			out.hasTarget = true;
			out.missionTarget = {
				static_cast<int16_t>(target->getX()),
				static_cast<int16_t>(target->getY())
			};
		}
	}

	out.coord.x = group.getX();
	out.coord.y = group.getY();

	out.units.reserve(group.getNumUnits());
	for (auto* node = group.headUnitNode(); node; node = group.nextUnitNode(node))
	{
		const CvUnit& unit = *::getUnit(node->m_data);
		if (allKnowing || (unit.plot()->isVisible(activeTeam, false) && !unit.isInvisible(activeTeam, false)))
			out.units.push_back(convert(unit, allKnowing));
	}

	return out;
	
}*/

static bool isUnitVisible(const CvUnit& unit)
{
	return unit.plot()->isActiveVisible(false) && !unit.isInvisible(gGlobals.getGame().getActiveTeam(), false);
}

static std::vector<Unit> getUnits(const iaabb2& rect, bool allKnowing)
{
	const MapGeometry geom = getMapGeometry();
	const CvMap& map = gGlobals.getMap();

	std::vector<Unit> out;

	const int area = rect.area();
	if (area <= MAX_PLAYERS)
	{
		// Check each plot.

		out.reserve(100);

		for (int y = rect.min.y; y < rect.max.y; ++y)
			for (int x = rect.min.x; x < rect.max.x; ++x)
			{
				const CvPlot& plot = *map.plot(x, y);
				for (auto* node = plot.headUnitNode(); node; node = plot.nextUnitNode(node))
					if (CvUnit& unit = *::getUnit(node->m_data); isUnitVisible(unit))
						out.push_back(convert(unit, allKnowing));
			}
	}
	else
	{
		// Check each player.

		size_t sum{};
		for (int i = 0; i < MAX_PLAYERS; ++i)
			sum += GET_PLAYER(static_cast<PlayerTypes>(i)).getNumUnits();
		out.reserve(sum);

		for (int i = 0; i < MAX_PLAYERS; ++i)
		{
			int itIndex{};
			const CvPlayer& player = GET_PLAYER(static_cast<PlayerTypes>(i));
			for (const CvUnit* unit = player.firstUnit(&itIndex); unit; unit = player.nextUnit(&itIndex))
				if (isUnitVisible(*unit))
					out.push_back(convert(*unit, allKnowing));
		}
	}

	return out;
}

// Utils functions from BUG.
static bool diploCanContact(const CvPlayer& player)
{
	if (player.getID() == gGlobals.getGame().getActivePlayer())
		return false;
	if (!player.isAlive() || player.isBarbarian() || player.isMinorCiv())
		return false;
	if (!GET_TEAM(gGlobals.getGame().getActiveTeam()).isHasMet(player.getTeam()))
		return false;
	if (!GET_TEAM(gGlobals.getGame().getActiveTeam()).isAtWar(player.getTeam()) && (gGlobals.getGame().isOption(GAMEOPTION_ALWAYS_WAR) || gGlobals.getGame().isOption(GAMEOPTION_NO_CHANGING_WAR_PEACE)))
		return false;
	return true;
}

static bool diploIsWillingToTalk(const CvPlayer& initiatingPlayer, const CvPlayer& targetPlayer)
{
	if (initiatingPlayer.getID() == initiatingPlayer.getID() || targetPlayer.isHuman())
		return true;
	return targetPlayer.AI_isWillingToTalk(initiatingPlayer.getID());
}

static bool diploCanTrade(const CvPlayer& player)
{
	return diploCanContact(player) && diploIsWillingToTalk(GET_PLAYER(gGlobals.getGame().getActivePlayer()), player);
}

static bool canSeeAllCities(const CvPlayer& player)
{
	if (!gGlobals.getGame().isOption(GAMEOPTION_ONE_CITY_CHALLENGE))
		return false;
	if (player.getTeam() == gGlobals.getGame().getActiveTeam())
		return true;
	const CvTeam& otherTeam = GET_TEAM(player.getTeam());
	if (otherTeam.isAVassal() && !otherTeam.isVassal(gGlobals.getGame().getActiveTeam()))
		return false;
	return diploCanTrade(player);
}

// Logic from scoreboard UI.
static bool canSeeResearch(PlayerTypes rivalPlayerI)
{
	const TeamTypes activeTeamI = gGlobals.getGame().getActiveTeam();
	const TeamTypes otherTeamI = GET_PLAYER(rivalPlayerI).getTeam();
	if (otherTeamI == activeTeamI || GET_TEAM(otherTeamI).isVassal(activeTeamI))
		return true;

	const CvPlayer& activePlayer = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	const bool bEspionageAllowed = !gGlobals.getGame().isOption(GAMEOPTION_NO_ESPIONAGE);
	if (bEspionageAllowed)
		for (const auto iMissionLoop : heck::range<EspionageMissionTypes>(gGlobals.getNumEspionageMissionInfos()))
			if (gGlobals.getEspionageMissionInfo(iMissionLoop).isSeeResearch())
				if (activePlayer.canDoEspionageMission(iMissionLoop, rivalPlayerI, nullptr, -1, nullptr))
					return true;

	return false;
}
// Logic from CvInfoScreen.
static bool canSeeDemographics(PlayerTypes rivalPlayerI)
{
	// Assume revealed.
	const TeamTypes activeTeamI = gGlobals.getGame().getActiveTeam();
	const TeamTypes otherTeamI = GET_PLAYER(rivalPlayerI).getTeam();
	if (otherTeamI == activeTeamI)
		return true;

	const CvPlayer& activePlayer = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	const bool bEspionageAllowed = !gGlobals.getGame().isOption(GAMEOPTION_NO_ESPIONAGE);
	if (bEspionageAllowed)
	{
		EspionageMissionTypes mission = NO_ESPIONAGEMISSION;
		for (const auto iMissionLoop : heck::range<EspionageMissionTypes>(gGlobals.getNumEspionageMissionInfos()))
			if (gGlobals.getEspionageMissionInfo(iMissionLoop).isSeeDemographics())
				mission = iMissionLoop;
		if (mission == NO_ESPIONAGEMISSION || activePlayer.canDoEspionageMission(mission, rivalPlayerI, nullptr, -1, nullptr))
			return true;
	}

	return false;
}

TradeList cvbot::convertTradeList(const CLinkList<TradeData>& table, const CvPlayerAI& us, const CvPlayerAI& them)
{
	TradeList list{};
	for (const TradeData& item : viewCLinkList(table))
	{
		switch (item.m_eItemType)
		{
		case TRADE_GOLD:
			list.gold = us.AI_maxGoldTrade(them.getID());
			break;
		case TRADE_GOLD_PER_TURN:
			list.gold = us.AI_maxGoldPerTurnTrade(them.getID());
			break;
		case TRADE_MAPS:
			list.map = true;
			break;
		case TRADE_VASSAL:
			list.vassalState = true;
			break;
		case TRADE_SURRENDER:
			list.surrender = true;
			break;
		case TRADE_OPEN_BORDERS:
			list.openBorders = true;
			break;
		case TRADE_DEFENSIVE_PACT:
			list.defensivePact = true;
			break;
		case TRADE_PERMANENT_ALLIANCE:
			break;
		case TRADE_PEACE_TREATY:
			list.peaceTreaty = true;
			break;
		case TRADE_TECHNOLOGIES:
			list.techs.push_back(static_cast<ETech>(item.m_iData));
			break;
		case TRADE_RESOURCES:
			list.bonuses.push_back(static_cast<EBonus>(item.m_iData));
			break;
		case TRADE_CITIES:
			break;
		case TRADE_PEACE:
			list.makePeace.push_back(static_cast<ETeam>(item.m_iData));
			break;
		case TRADE_WAR:
			list.declareWar.push_back(static_cast<ETeam>(item.m_iData));
			break;
		case TRADE_EMBARGO:
			break;
		case TRADE_CIVIC:
			break;
		case TRADE_RELIGION:
			break;
		default:
			break;
		}
	}
	return list;
}

static CLinkList<TradeData> convert(const TradeList& list)
{
	CLinkList<TradeData> table;

	if (list.gold)
		table.insertAtEnd({ .m_eItemType = TRADE_GOLD, .m_iData = list.gold });
	if (list.goldPerTurn)
		table.insertAtEnd({ .m_eItemType = TRADE_GOLD_PER_TURN, .m_iData = list.goldPerTurn });
	if (list.openBorders)
		table.insertAtEnd({ .m_eItemType = TRADE_OPEN_BORDERS });
	if (list.peaceTreaty)
		table.insertAtEnd({ .m_eItemType = TRADE_PEACE_TREATY });
	if (list.vassalState)
		table.insertAtEnd({ .m_eItemType = TRADE_VASSAL });
	if (list.surrender)
		table.insertAtEnd({ .m_eItemType = TRADE_SURRENDER });
	if (list.defensivePact)
		table.insertAtEnd({ .m_eItemType = TRADE_DEFENSIVE_PACT });
	if (list.map)
		table.insertAtEnd({ .m_eItemType = TRADE_MAPS });

	for (const auto bonus : list.bonuses)
		table.insertAtEnd({ .m_eItemType = TRADE_RESOURCES, .m_iData = bonus });
	for (const auto tech : list.techs)
		table.insertAtEnd({ .m_eItemType = TRADE_TECHNOLOGIES, .m_iData = tech });
	for (const auto team : list.declareWar)
		table.insertAtEnd({ .m_eItemType = TRADE_WAR, .m_iData = static_cast<int>(team) });
	for (const auto team : list.makePeace)
		table.insertAtEnd({ .m_eItemType = TRADE_PEACE, .m_iData = static_cast<int>(team) });

	return table;
}

static std::pair<int, int> calculateTotalTrade(const CvPlayer& activePlayer, const CvPlayer& otherPlayer)
{
	const PlayerTypes otherPlayerI = otherPlayer.getID();

	int totalYield{};
	int numRoutes{};
	int itIndex{};
	for (const CvCity* city = activePlayer.firstCity(&itIndex); city; city = activePlayer.nextCity(&itIndex))
	{
		for (const int i : heck::range(city->getTradeRoutes()))
		{
			CvCity* const tradeCity = city->getTradeCity(i);
			if (tradeCity && tradeCity->getOwner() == otherPlayerI)
			{
				totalYield += city->calculateTradeYield(YIELD_COMMERCE, city->calculateTradeProfit(tradeCity));
				++numRoutes;
			}
		}
	}
	return { totalYield, numRoutes };
}

// Based on BUG PlayerUtil.py
static std::optional<bool> isWHEOOH(PlayerTypes aiPlayerI, PlayerTypes activePlayerI)
{
	const CvPlayerAI& aiPlayer = GET_PLAYER(aiPlayerI);
	const CvTeamAI& aiTeam = GET_TEAM(aiPlayer.getTeam());
	const CvPlayerAI& activePlayer = GET_PLAYER(activePlayerI);
	const CvTeamAI& activeTeam = GET_TEAM(activePlayer.getTeam());

	if (!diploCanTrade(aiPlayer))
		return std::nullopt;
	
	for (const PlayerTypes otherPlayerI : heck::range<PlayerTypes>(MAX_CIV_PLAYERS))
	{
		const CvPlayerAI& otherPlayer = GET_PLAYER(otherPlayerI);

		if (!otherPlayer.isAlive() || otherPlayer.isMinorCiv())
			continue;

		const TeamTypes otherTeamI = otherPlayer.getTeam();

		if (otherTeamI == activePlayer.getTeam() or otherTeamI == aiPlayer.getTeam() or aiTeam.isAtWar(otherTeamI))
		{
			// won't DoW your team, their team, or a team they are fighting
			continue;
		}
		if (not (activeTeam.isHasMet(otherTeamI) and aiTeam.isHasMet(otherTeamI)))
		{
			// won't DoW someone you or they haven't met
			continue;
		}

		const TradeData tradeData{
			.m_eItemType = TRADE_WAR,
			.m_iData = otherTeamI,
		};

		if (aiPlayer.canTradeItem(activePlayerI, tradeData, false))
		{
			if (aiPlayer.getTradeDenial(activePlayerI, tradeData) == DENIAL_TOO_MANY_WARS)
				return true;
			else
				return false;
		}
	}

	return std::nullopt;
}


static Player convert(CvPlayerAI& otherPlayer, bool allKnowing)
{
	// We assume player is revealed if !allKnowing.

	const CvPlayer& activePlayer = GET_PLAYER(gGlobals.getGame().getActivePlayer());

	const PlayerTypes otherPlayerI = otherPlayer.getID();
	const TeamTypes activeTeamI = gGlobals.getGame().getActiveTeam();
	const TeamTypes otherTeamI = otherPlayer.getTeam();

	const CvTeam& activeTeam = GET_TEAM(activeTeamI);
	const CvTeamAI& otherTeam = GET_TEAM(otherTeamI);

	Player out{};
	out.team = static_cast<ETeam>(otherPlayer.getTeam());
	out.name = otherPlayer.getName();
	out.score = otherPlayer.calculateScore(); // calls python
	out.optIsWarPreping = isWHEOOH(otherPlayerI, gGlobals.getGame().getActivePlayer());
	// As in BUG, if we can see the player's city list, show the exact number, otherwise, count up revealed cities.
	if (canSeeAllCities(otherPlayer))
		out.optNumCities = out.numRevealedCities = otherPlayer.getNumCities();
	else
	{
		int itIndex{};
		for (const CvCity* city = otherPlayer.firstCity(&itIndex); city; city = otherPlayer.nextCity(&itIndex))
			out.numRevealedCities += city->isRevealed(activeTeamI, false);
	}

	if (otherPlayer.getStateReligion() != NO_RELIGION)
		out.optStateReligion = static_cast<EReligion>(otherPlayer.getStateReligion());

	
	out.civicChoices.resize(gGlobals.getNumCivicOptionInfos());
	for (int i = 0; i < out.civicChoices.size(); ++i)
		out.civicChoices[i] = static_cast<ECivic>(otherPlayer.getCivics(static_cast<CivicOptionTypes>(i)));

	if ((allKnowing || canSeeResearch(otherPlayerI)) && otherPlayer.getCurrentResearch() != NO_TECH)
	{
		out.optVisibleResearch = Player::VisibleResearch{
			 static_cast<ETech>(otherPlayer.getCurrentResearch()),
			 otherPlayer.getResearchTurnsLeft(otherPlayer.getCurrentResearch(), true),
		};
	}

	if (allKnowing || canSeeDemographics(otherPlayerI))
	{
		const int turn = gGlobals.getGame().getGameTurn() - 1;
		out.optVisibleDemographics = Player::VisibleDemographics{
			.gdp = otherPlayer.getEconomyHistory(turn),
			.power = otherPlayer.getPowerHistory(turn),
			.food = otherPlayer.getAgricultureHistory(turn),
			.prod = otherPlayer.getIndustryHistory(turn),
		};
	}

	if (activeTeamI != otherTeamI)
	{
		// Logic from CvInfoScreen.
		// Note that the tech trading game option is ignored.
		if (activeTeam.isTechTrading() || otherTeam.isTechTrading())
		{
			for (const TechTypes tech : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
			{
				const TradeData tradeItem{ .m_eItemType = TRADE_TECHNOLOGIES, .m_iData = tech };
				const bool canExportAndWontDeny = activePlayer.canTradeItem(otherPlayerI, tradeItem, true);
				const bool canImport = otherPlayer.canTradeItem(activePlayer.getID(), tradeItem, false);
				if (canExportAndWontDeny)
					out.tradeAdvisorData.techsWants.push_back(static_cast<ETech>(tech));
				else if (activeTeam.isHasTech(tech) && otherPlayer.canResearch(tech, false))
					out.tradeAdvisorData.techsWantsButWeCantTrade.push_back(static_cast<ETech>(tech));
				else if (otherPlayer.canResearch(tech, false))
					out.tradeAdvisorData.techsCanResearch.push_back(static_cast<ETech>(tech));

				if (canImport)
				{
					if (otherPlayer.getTradeDenial(activePlayer.getID(), tradeItem) == NO_DENIAL)
						out.tradeAdvisorData.techsWillTrade.push_back(static_cast<ETech>(tech));
					else
						out.tradeAdvisorData.techsWontTrade.push_back(static_cast<ETech>(tech));
				}
				else if (otherTeam.isHasTech(tech) && activePlayer.canResearch(tech, false))
					out.tradeAdvisorData.techsCantTrade.push_back(static_cast<ETech>(tech));
			}
		}
	}

	for (const auto bonus : heck::range<BonusTypes>(gGlobals.getNumBonusInfos()))
	{
		const TradeData tradeItem{ .m_eItemType = TRADE_RESOURCES, .m_iData = bonus };
		if (otherPlayer.canTradeItem(activePlayer.getID(), tradeItem))
			if (diploCanTrade(otherPlayer))
				out.tradeAdvisorData.bonusesHas.push_back(static_cast<EBonus>(bonus));
	}

	// Logic from CvInfoScreen.
	if (activeTeam.isGoldTrading() || otherTeam.isGoldTrading())
		out.tradeAdvisorData.maxTradeGold = otherPlayer.AI_maxGoldTrade(activePlayer.getID());

	{
		CvPlayerAI& us = GET_PLAYER(gGlobals.getGame().getActivePlayer());
		CvPlayerAI& them = GET_PLAYER(otherPlayerI);
		CLinkList<TradeData> dllUsTradeTable;
		CLinkList<TradeData> dllThemTradeTable;
		if (diploCanTrade(them))
		{
			them.buildTradeTable(us.getID(), dllThemTradeTable);
			us.buildTradeTable(them.getID(), dllThemTradeTable);
		}

		out.availableImports = convertTradeList(dllThemTradeTable, them, us);
		out.availableExports = convertTradeList(dllUsTradeTable, us, them);
	}
	
	const auto [tradeRoutesTotalCommerce, tradeRoutesCount] = calculateTotalTrade(activePlayer, otherPlayer);
	out.tradeRoutesTotalCommerce = tradeRoutesTotalCommerce;
	out.tradeRoutesCount = tradeRoutesCount;
	
	out.spyPointsOnThem = activeTeam.getEspionagePointsAgainstTeam(otherTeamI);
	out.spyPointsOnMe = otherTeam.getEspionagePointsAgainstTeam(activeTeamI);

	for (const auto teamI : heck::range<TeamTypes>(MAX_TEAMS))
		if (otherTeam.isVassal(teamI))
		{
			out.optMasterTeam = static_cast<ETeam>(teamI);
			break;
		}
	
	if (otherTeam.AI_getWorstEnemy() != NO_TEAM)
		out.optWorstEnemy = static_cast<ETeam>(otherTeam.AI_getWorstEnemy());


	const int iWarWeariness = otherPlayer.getModifiedWarWearinessPercentAnger(otherTeam.getWarWeariness(activeTeamI) * std::max<int>(0, 100 + activeTeam.getEnemyWarWearinessModifier()));
	out.optWarWeariness = std::max(0, iWarWeariness / 10000);
	
	for (const auto relationPlayerI : heck::range<PlayerTypes>(kMaxCivPlayers))
	{
		const CvPlayerAI& relationPlayer = GET_PLAYER(relationPlayerI);
		const TeamTypes relationTeamI = relationPlayer.getTeam();
		const CvTeamAI& relationTeam = GET_TEAM(relationTeamI);

		if (activeTeam.isHasMet(relationPlayer.getTeam()) && relationPlayer.isAlive() && relationPlayerI != otherPlayerI)
		{
			Relation relation{
				.subject = static_cast<EPlayer>(relationPlayerI),
				.target = static_cast<EPlayer>(otherPlayerI),
				.war = otherTeam.isAtWar(relationPlayer.getTeam()),
				.hasPeaceTreaty = relationTeam.isForcePeace(otherTeamI),
				.hasOpenBorders = relationTeam.isOpenBorders(otherTeamI),
				.hasDefensivePact = relationTeam.isDefensivePact(otherTeamI),
				.wontTalk = !diploIsWillingToTalk(otherPlayer, relationPlayer),
				.isVassalOf = relationTeam.isVassal(otherTeamI),
				.attitude = static_cast<EAttitude>(relationTeam.AI_getAttitude(otherTeamI)),
			};
			relation.attitudeModifiers[EAttitudeModifier::CloseBorders       /**/] = relationPlayer.AI_getCloseBordersAttitude       /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::War                /**/] = relationPlayer.AI_getWarAttitude                /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::Peace              /**/] = relationPlayer.AI_getPeaceAttitude              /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::SameReligion       /**/] = relationPlayer.AI_getSameReligionAttitude       /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::DifferentReligion  /**/] = relationPlayer.AI_getDifferentReligionAttitude  /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::BonusTrade         /**/] = relationPlayer.AI_getBonusTradeAttitude         /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::OpenBorders        /**/] = relationPlayer.AI_getOpenBordersAttitude        /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::DefensivePact      /**/] = relationPlayer.AI_getDefensivePactAttitude      /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::RivalDefensivePact /**/] = relationPlayer.AI_getRivalDefensivePactAttitude /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::RivalVassal        /**/] = relationPlayer.AI_getRivalVassalAttitude        /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::ShareWar           /**/] = relationPlayer.AI_getShareWarAttitude           /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::FavouriteCivic     /**/] = relationPlayer.AI_getFavoriteCivicAttitude      /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::Trade              /**/] = relationPlayer.AI_getTradeAttitude              /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::RivalTrade         /**/] = relationPlayer.AI_getRivalTradeAttitude         /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::Colony             /**/] = relationPlayer.AI_getColonyAttitude             /**/(otherPlayerI);
			relation.attitudeModifiers[EAttitudeModifier::Extra              /**/] = relationPlayer.AI_getAttitudeExtra(otherPlayerI);
			for (int iI = 0; iI < NUM_MEMORY_TYPES; ++iI)
				relation.attitudeModifiers[EAttitudeModifier::Memory] += relationPlayer.AI_getMemoryAttitude(otherPlayerI, static_cast<MemoryTypes>(iI));

			out.relations.push_back(relation);
		}
	}

	return out;
}

static City convert(const CvCity& city, bool allKnowing)
{
	City out{
		.coord{
			static_cast<int16_t>(city.getX_INLINE()),
			static_cast<int16_t>(city.getY_INLINE()),
			},
		.owner = static_cast<EPlayer>(city.getOwnerINLINE()),
		.isCapital = city.isCapital(),
		.isGovernmentCenter = city.isGovernmentCenter(),
		.pop = city.getPopulation(),
		.defence = city.getDefenseModifier(false),
		.defenceIgnoringBuildings = city.getDefenseModifier(true),
		.name = city.getName(),
	};

	for (const auto i : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
		if (city.isHasReligion(i))
			out.religions.push_back(static_cast<EReligion>(i));

	if (allKnowing || city.canBeSelected())
	{
		City::InspectableCityInfo& info = out.optInspectableCityInfo.emplace();

		for (const int i : heck::range(NUM_CITY_PLOTS))
			info.workedPlotsMask[i] = city.isWorkingPlot(i);

		// Left
		info.tradeRouteCommerce = city.getTradeYield(YIELD_COMMERCE);
		for (const auto i : heck::range<BuildingTypes>(gGlobals.getNumBuildingInfos()))
			if (city.getNumBuilding(i) > 0)
				info.buildings.push_back(static_cast<EBuildingType>(i));
		info.culture = city.getCulture(city.getOwnerINLINE());

		// Middle
		info.foodBinLevel = city.getFood();
		info.foodBinRate = city.foodDifference();
		info.foodBinMax = city.growthThreshold();
		info.prodBinLevel = city.getProduction();
		info.prodBinRate = city.getCurrentProductionDifference(false, true);
		info.prodBinMax = city.getProductionNeeded();
		const OrderData order = city.getOrderData(0);
		switch (order.eOrderType)
		{
		case NO_ORDER:
		default:
			break;
		case ORDER_TRAIN:
			info.productionChoice = static_cast<EUnitType>(order.iData1);
			break;
		case ORDER_CONSTRUCT:
			info.productionChoice = static_cast<EBuildingType>(order.iData1);
			break;
		case ORDER_CREATE:
			info.productionChoice = static_cast<EProject>(order.iData1);
			break;
		case ORDER_MAINTAIN:
			info.productionChoice = static_cast<EProcess>(order.iData1);
			break;
		}

		info.extraHappiness = city.getExtraHappiness();
		info.extraHappyTimer = city.getHappinessTimer();
		for (const int i : heck::range(MAX_PLAYERS))
			info.plotCultureValues[i] = city.getCulture(static_cast<PlayerTypes>(i));
		info.hurryAngerTimer = city.getHurryAngerTimer();
		info.conscriptAngerTimer = city.getConscriptAngerTimer();
		info.defyResolutionAngerTimer = city.getDefyResolutionAngerTimer();
		info.espionageUnhappinessCounter = city.getEspionageHappinessCounter();
		info.freshWaterHealth = city.getFreshWaterGoodHealth() - city.getFreshWaterBadHealth();
		info.extraHealth = city.getExtraHealth();
		info.espionageUnhealthinessCounter = city.getEspionageHealthCounter();

		// Right
		for (const int i : heck::range(gGlobals.getNumBonusInfos()))
			if (city.hasBonus(static_cast<BonusTypes>(i)))
				info.bonuses.push_back(static_cast<EBonus>(i));
		//for (const auto i : heck::range<SpecialistTypes>(ESpecialist::Num))
		//	info.specialists[(int)i] = static_cast<uint8_t>(genericCityInfo.m_paiSpecialistCount[i]); // , .max = city.getMaxSpecialistCount(i)
	}

	return out;
}

void AllKnowingGameInterface::getPlots(ivec2 origin, Span2D<Plot> out) const
{
	::getPlots(origin, out, true);
}

std::vector<Unit> AllKnowingGameInterface::getUnits(iaabb2 rect) const
{
	return ::getUnits(rect, true);
}

std::vector<std::optional<Player>> AllKnowingGameInterface::getPlayers() const
{
	const CvTeam& activeTeam = GET_TEAM(gGlobals.getGame().getActiveTeam());

	std::vector<std::optional<Player>> players(kMaxCivPlayers);
	for (const auto playerI : heck::range<PlayerTypes>(kMaxCivPlayers))
	{
		CvPlayerAI& player = GET_PLAYER(playerI);
		if (player.isAlive() && activeTeam.isHasMet(player.getTeam()))
			players[playerI] = convert(player, true);
	}

	return players;
}

///

BotInit& BotInit::getInstance()
{
	return gBotInitInstance;
}

std::ostream& BotInit::getLoggingStream() const
{
	return std::clog;
}

GameSetup BotInit::getGameSetup() const
{
	std::bitset<EGameOption::Num> options{};
	for (const auto opt : heck::range<GameOptionTypes>(NUM_GAMEOPTION_TYPES))
		options[opt] = gGlobals.getGame().isOption(opt);

	return GameSetup{
		.mapGeometry = getMapGeometry(),
		.options = options,
		.activePlayerI = static_cast<EPlayer>(gGlobals.getGame().getActivePlayer()),
		.modName = gGlobals.getDLLIFace()->getModName(false),
		.mapScriptName = gGlobals.getInitCore().getMapScriptName(),
		.speed = static_cast<EGameSpeed>(gGlobals.getGame().getGameSpeedType()),
		.worldSize = static_cast<EWorldSize>(gGlobals.getMap().getWorldSize()),
		.handicap = static_cast<EHandicap>(gGlobals.getGame().getHandicapType()),
	};
}

const IAllKnowingGameInterface& BotInit::getAllKnowingGameInterface() const
{
	return gAllKnowingGameInterfaceInstance;
}

///

Game& Game::getInstance()
{
	return gGameInstance;
}

const IAllKnowingGameInterface& Game::getAllKnowingGameInterface() const
{
	return gAllKnowingGameInterfaceInstance;
}

int Game::getTurnNumber() const
{
	return gGlobals.getGame().getGameTurn();
}

static std::vector<BuildingTypes> getWorldWonderBuildingTypes()
{
	std::vector<BuildingTypes> wonderBuildings;
	for (const auto building : heck::range<BuildingTypes>(gGlobals.getNumBuildingInfos()))
		if (isWorldWonderClass(static_cast<BuildingClassTypes>(gGlobals.getBuildingInfo(building).getBuildingClassType())))
			wonderBuildings.push_back(building);
	return wonderBuildings;
}

GlobalInfo Game::getGlobalInfo() const
{
	GlobalInfo info{};

	const CvPlayer& activePlayer = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	const TeamTypes activeTeamI = gGlobals.getGame().getActiveTeam();
	const CvTeam& activeTeam = GET_TEAM(activeTeamI);

	const std::vector<BuildingTypes> wonderBuildings = getWorldWonderBuildingTypes();

	for (const auto playerI : heck::range<PlayerTypes>(kMaxPlayers))
	{
		CvPlayerAI& player = GET_PLAYER(playerI);
		const TeamTypes teamI = player.getTeam();
		// Same logic as info screen.
		if (player.isBarbarian())
			continue;

		int itIndex{};
		for (const CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
		{
			for (const auto building : wonderBuildings)
			{
				// Differently from the info screen, we check that the city is inspectable.
				const bool isInProgress = city->getProductionBuilding() == building && city->canBeSelected();

				if (city->getNumBuilding(building) || isInProgress)
				{
					info.builtWonders.push_back(GlobalInfo::BuiltWonder{
						.buildingClass = static_cast<EBuildingClass>(gGlobals.getBuildingInfo(building).getBuildingClassType()),
						.optPlayer = activeTeam.isHasMet(teamI) ? static_cast<EPlayer>(playerI) : kNoPlayer,
						.inProgress = isInProgress,
						});
				}
			}
		}
	}

	int numAlivePlayers = 0;

	struct GlobalDemographicEntry
	{
		PlayerTypes playerI{};
		int value{};
	};

	std::array<std::vector<GlobalDemographicEntry>, GlobalInfo::Num> globalDemographicTable{};

	for (const auto playerI : heck::range<PlayerTypes>(kMaxCivPlayers))
	{
		const CvPlayer& player = GET_PLAYER(playerI);

		// Logic from info screen.
		if (player.isAlive() && !player.isBarbarian())
		{
			++numAlivePlayers;

			globalDemographicTable[GlobalInfo::GDP].emplace_back(playerI, player.calculateTotalCommerce());
			globalDemographicTable[GlobalInfo::Prod].emplace_back(playerI, player.calculateTotalYield(YIELD_PRODUCTION));
			globalDemographicTable[GlobalInfo::Food].emplace_back(playerI, player.calculateTotalYield(YIELD_FOOD));
			globalDemographicTable[GlobalInfo::Power].emplace_back(playerI, player.getPower());
			globalDemographicTable[GlobalInfo::Land].emplace_back(playerI, player.getTotalLand());
			globalDemographicTable[GlobalInfo::Pop].emplace_back(playerI, player.getRealPopulation());
			const int totalHappy = player.calculateTotalCityHappiness();
			const int totalUnhappy = player.calculateTotalCityUnhappiness();
			globalDemographicTable[GlobalInfo::ApprovalRate].emplace_back(playerI, totalHappy * 100 / std::max(1, totalHappy + totalUnhappy));
			const int totalHealth = player.calculateTotalCityHealthiness();
			const int totalUnhealth = player.calculateTotalCityUnhealthiness();
			globalDemographicTable[GlobalInfo::LifeExpectancy].emplace_back(playerI, totalHealth * 100 / std::max(1, totalHealth + totalUnhealth));
		}
	}

	for (const auto i : heck::range(GlobalInfo::Num))
	{
		int min = INT_MAX;
		int max = 0;
		int acc = 0;

		for (const auto& entry : globalDemographicTable[(int)i])
		{
			if (entry.playerI == activePlayer.getID())
				info.globalDemographics[(int)i].value = entry.value;
			max = std::max(max, entry.value);
			min = std::min(min, entry.value);
			acc += entry.value;
		}

		info.globalDemographics[(int)i].rivalBest = max;
		info.globalDemographics[(int)i].rivalWorst = min;
		info.globalDemographics[(int)i].rivalAverage = acc / static_cast<int>(globalDemographicTable[(int)i].size());
	}


	for (const auto i : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
	{
		if (gGlobals.getGame().isReligionFounded(i))
		{
			GlobalInfo::ReligionInfo religionInfo{
				.religion = static_cast<EReligion>(i),
			};

			if (const CvCity* const city = gGlobals.getGame().getHolyCity(i))
				if (city->isRevealed(activeTeamI, false))
					religionInfo.optRevealedHolyCityCoord = ivec2{ city->getX(), city->getY() };

			info.foundedReligions.push_back(religionInfo);
		}
	}
	
	info.hasCircumnavigated = gGlobals.getGame().isCircumnavigated();

	return info;
}

CivState Game::getCivState() const
{
	CvPlayer& activePlayer = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	const CvTeam& activeTeam = GET_TEAM(gGlobals.getGame().getActiveTeam());

	CivState state{
		//.researchSlider = activePlayer.getCommercePercent(COMMERCE_RESEARCH),
		//.optCultureSlider = activePlayer.isCommerceFlexible(COMMERCE_CULTURE) ? std::optional(activePlayer.getCommercePercent(COMMERCE_CULTURE)) : std::nullopt,
		//.optEspionageSlider = activePlayer.isCommerceFlexible(COMMERCE_ESPIONAGE) ? std::optional(activePlayer.getCommercePercent(COMMERCE_ESPIONAGE)) : std::nullopt,
		.optCurrentResearch = activePlayer.getCurrentResearch() != NO_TECH ? std::optional(static_cast<ETech>(activePlayer.getCurrentResearch())) : std::nullopt,
		.optStateReligion = activePlayer.getStateReligion() != NO_RELIGION ? std::optional(static_cast<EReligion>(activePlayer.getStateReligion())) : std::nullopt,
	};

	for (const auto i : heck::range<CommerceTypes>(NUM_COMMERCE_TYPES))
		if (activePlayer.isCommerceFlexible(i))
			state.optSliders[i] = activePlayer.getCommercePercent(i);

	for (const auto i : heck::range<CivicOptionTypes>(gGlobals.getNumCivicOptionInfos()))
		state.civics.push_back(static_cast<ECivic>(activePlayer.getCivics(i)));

	state.techs.resize(gGlobals.getNumTechInfos());

	//const dllgeneric::ActivePlayerInfo activePlayerInfo = dllgeneric::getActivePlayerInfo(activePlayer);

	for (const auto i : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
	{
		state.techs[i] = {
			.isResearched = activeTeam.isHasTech(i),
			.canResearch = activePlayer.canResearch(i),
			.progress = activeTeam.getResearchProgress(i),
			.cost = activeTeam.getResearchCost(i),
		};
	}

	for (auto* node = activePlayer.headResearchQueueNode(); node; node = activePlayer.nextResearchQueueNode(node))
		state.techs[node->m_data].isInQueue = true;

	for (const auto i : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
	{
		if (!GC.getGameINLINE().isReligionSlotTaken(i))
		{
			const TechTypes tech = static_cast<TechTypes>(GC.getReligionInfo(i).getTechPrereq());
			if (tech != NO_TECH && GC.getGameINLINE().countKnownTechNumTeams(tech) == 0)
				state.techs[tech].canBeFirstToFoundReligion = true;
		}
	}

	for (const TechTypes tech : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
	{
		if (GC.getGameINLINE().countKnownTechNumTeams(tech) == 0)
		{
			if (activePlayer.getTechFreeUnit(tech) != NO_UNIT)
				state.techs[tech].canBeFirstToSpawnsGreatPerson = true;
			if (gGlobals.getTechInfo(tech).getFirstFreeTechs() > 0)
				state.techs[tech].canBeFirstToGetFreeTech = true;
		}
	}

	return state;
}

std::vector<std::optional<Player>> Game::getRevealedPlayers() const
{
	const CvTeam& activeTeam = GET_TEAM(gGlobals.getGame().getActiveTeam());

	std::vector<std::optional<Player>> players(kMaxCivPlayers);
	for (const auto playerI : heck::range<PlayerTypes>(kMaxCivPlayers))
	{
		CvPlayerAI& player = GET_PLAYER(playerI);
		const TeamTypes teamI = player.getTeam();
		const CvTeamAI& team = GET_TEAM(teamI);
		if (team.isEverAlive() && activeTeam.isHasMet(teamI))
			players[playerI] = convert(player, false);
	}

	return players;
}

void Game::getPlots(ivec2 origin, Span2D<Plot> out) const
{
	::getPlots(origin, out, false);
}

std::vector<Unit> Game::getVisibleUnits(iaabb2 rect) const
{
	return ::getUnits(rect, false);
}

int Game::getPlotBuildProgress(ivec2 coord, EBuild build) const
{
	return gGlobals.getMap().plotINLINE(coord.x, coord.y)->getBuildProgress(static_cast<BuildTypes>(build));
}

std::vector<EBuild> Game::getWorkerBuildChoicesAt(EUnitId unitId, ivec2 coord) const
{
	const CvPlayer& player = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	const CvUnit& unit = *player.getUnit(static_cast<int>(unitId));
	const CvPlot& plot = *gGlobals.getMap().plotINLINE(coord.x, coord.y);
	std::vector<EBuild> builds;
	for (const auto i : heck::range<BuildTypes>(gGlobals.getNumBuildInfos()))
		if (unit.canBuild(&plot, i))
			builds.push_back(static_cast<EBuild>(i));
	return builds;
}

std::vector<EBuild> Game::getPlotBuildChoicesAt(ivec2 coord) const
{
	const PlayerTypes playerI = gGlobals.getGame().getActivePlayer();
	const CvPlot& plot = *gGlobals.getMap().plotINLINE(coord.x, coord.y);
	std::vector<EBuild> builds;
	for (const auto i : heck::range<BuildTypes>(gGlobals.getNumBuildInfos()))
		if (plot.canBuild(i, playerI, false))
			builds.push_back(static_cast<EBuild>(i));
	return builds;
}

bool Game::hasFoundActionAt(ivec2 coord) const
{
	// bTestVisible becuase we only want to know if the aciton button is visible, not if we can actually found.
	return GET_PLAYER(gGlobals.getGame().getActivePlayer()).canFound(coord.x, coord.y, true);
}

static CvSelectionGroup* regroup(CommandUnitGroup group)
{
	if (group.empty())
		return nullptr;

	CvPlayer& player = GET_PLAYER(gGlobals.getGame().getActivePlayer());

	// Check if group is mirrored.
	CvUnit& firstUnit = *player.getUnit(static_cast<int>(group[0]));
	CvSelectionGroup* missionGroup{};
	if (CvSelectionGroup* const firstGroup = firstUnit.getGroup(); firstGroup->getNumUnits() == group.size())
	{
		if (std::ranges::all_of(group, [&player, firstGroup](EUnitId id) {
			return player.getUnit(static_cast<int>(id))->getGroup() == firstGroup;
			}))
			missionGroup = firstGroup;
	}

	if (!missionGroup)
	{
		// Need to split and make a new group.
		// If there's any unit in its own group, then join that group.
		const auto it = std::ranges::find_if(group, [&player](EUnitId id) {
			return player.getUnit(static_cast<int>(id))->getGroup()->getNumUnits() == 1;
			});
		if (it != group.end())
			missionGroup = player.getUnit(static_cast<int>(*it))->getGroup();
		else
		{
			// One of the groups may be a subset of the mission group, but let's just create a new group.
			firstUnit.joinGroup(nullptr, true);
			missionGroup = firstUnit.getGroup();
		}
		for (const EUnitId id : group)
		{
			CvUnit& unit = *player.getUnit(static_cast<int>(id));
			unit.joinGroup(missionGroup, true); // Remove from UI selection too.
			if (unit.getGroup() != missionGroup)
				return nullptr; // Could not group units.
		}
	}

	return missionGroup;
}

static MissionTypes convert(EMission mission)
{
	switch (mission)
	{
		using enum EMission;
	case MoveTo: return MissionTypes::MISSION_MOVE_TO;
	case RouteTo: return MissionTypes::MISSION_ROUTE_TO;
	case Skip: return MissionTypes::MISSION_SKIP;
	case Sleep: return MissionTypes::MISSION_SLEEP;
	case Fortify: return MissionTypes::MISSION_FORTIFY;
	case Plunder: return MissionTypes::MISSION_PLUNDER;
	case AirPatrol: return MissionTypes::MISSION_AIRPATROL;
	case SeaPatrol: return MissionTypes::MISSION_SEAPATROL;
	case Heal: return MissionTypes::MISSION_HEAL;
	case Sentry: return MissionTypes::MISSION_SENTRY;
	case Airlift: return MissionTypes::MISSION_AIRLIFT;
	case Nuke: return MissionTypes::MISSION_NUKE;
	case Recon: return MissionTypes::MISSION_RECON;
	case Paradrop: return MissionTypes::MISSION_PARADROP;
	case Airbomb: return MissionTypes::MISSION_AIRBOMB;
	case RangeAttack: return MissionTypes::MISSION_RANGE_ATTACK;
	case Bombard: return MissionTypes::MISSION_BOMBARD;
	case Pillage: return MissionTypes::MISSION_PILLAGE;
	case Sabotage: return MissionTypes::MISSION_SABOTAGE;
	case Destroy: return MissionTypes::MISSION_DESTROY;
	case StealPlans: return MissionTypes::MISSION_STEAL_PLANS;
	case Found: return MissionTypes::MISSION_FOUND;
	case Spread: return MissionTypes::MISSION_SPREAD;
	case SpreadCorporation: return MissionTypes::MISSION_SPREAD_CORPORATION;
	case Join: return MissionTypes::MISSION_JOIN;
	case Construct: return MissionTypes::MISSION_CONSTRUCT;
	case Discover: return MissionTypes::MISSION_DISCOVER;
	case Hurry: return MissionTypes::MISSION_HURRY;
	case Trade: return MissionTypes::MISSION_TRADE;
	case GreatWork: return MissionTypes::MISSION_GREAT_WORK;
	case Infiltrate: return MissionTypes::MISSION_INFILTRATE;
	case GoldenAge: return MissionTypes::MISSION_GOLDEN_AGE;
	case Build: return MissionTypes::MISSION_BUILD;
	case Lead: return MissionTypes::MISSION_LEAD;
	case Espionage: return MissionTypes::MISSION_ESPIONAGE;
	default:
		throw BotFailure("Unknown mission type.");
	}
}

bool Game::canStartMission(CommandUnitGroup group, EMission mission, int data1, int data2) const
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
		return missionGroup->canStartMission(convert(mission), data1, data2);
	else
		return false;
}

bool Game::pushMission(CommandUnitGroup group, EMission mission, int data1, int data2)
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
	{
		missionGroup->clearMissionQueue();
		if (missionGroup->canStartMission(convert(mission), data1, data2))
		{
			missionGroup->pushMission(convert(mission), data1, data2, 0, false, true);
			// Handle anything that popped up...
			GET_PLAYER(gGlobals.getGameINLINE().getActivePlayer()).handleUIForPlayerBot();
			return true;
		}
	}
	return false;
}

std::vector<EPromotion> Game::getAvailablePromotions(EUnitId unitId) const
{
	const CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	std::vector<EPromotion> out;
	if (unit.isPromotionReady())
		for (const auto i : heck::range<PromotionTypes>(gGlobals.getNumPromotionInfos()))
			if (unit.canPromote(i, -1))
				out.push_back(static_cast<EPromotion>(i));
	return out;
}

bool Game::tryPromote(EUnitId unitId, EPromotion promotion)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	unit.promote(static_cast<PromotionTypes>(promotion), -1);
	return unit.isHasPromotion(static_cast<PromotionTypes>(promotion));
}


bool Game::tryUpgrade(EUnitId unitId, EUnitType type)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	unit.upgrade(static_cast<UnitTypes>(type));
	return unit.getUnitType() == static_cast<UnitTypes>(type);
}

bool Game::tryWake(CommandUnitGroup group)
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
	{
		if (missionGroup->canDoCommand(COMMAND_WAKE, -1, -1))
		{
			missionGroup->getHeadUnit()->doCommand(COMMAND_WAKE, -1, -1); // It's a group command
			return true;
		}
	}
	return false;
}

bool Game::trySkipTurn(CommandUnitGroup group)
{
	return pushMission(group, EMission::Skip, -1, -1);
}

bool Game::tryCancelOrders(CommandUnitGroup group)
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
	{
		if (missionGroup->canDoCommand(COMMAND_CANCEL_ALL, -1, -1))
		{
			missionGroup->getHeadUnit()->doCommand(COMMAND_CANCEL_ALL, -1, -1); // It's a group command
			return true;
		}
	}
	return false;
}

bool Game::tryAutomate(CommandUnitGroup group, EAutomation automation)
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
	{
		if (missionGroup->canDoCommand(COMMAND_AUTOMATE, static_cast<int>(automation), -1))
		{
			if (missionGroup->getAutomateType() != static_cast<int>(automation))
				missionGroup->getHeadUnit()->doCommand(COMMAND_AUTOMATE, static_cast<int>(automation), -1); // It's a group command
			return true;
		}
	}
	return false;
}

bool Game::tryStopAutomation(CommandUnitGroup group)
{
	if (CvSelectionGroup* const missionGroup = regroup(group))
	{
		if (missionGroup->canDoCommand(COMMAND_STOP_AUTOMATION, -1, -1))
		{
			missionGroup->getHeadUnit()->doCommand(COMMAND_STOP_AUTOMATION, -1, -1); // It's a group command
			return true;
		}
	}
	return false;
}

bool Game::tryDelete(EUnitId unitId)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	if (unit.canDoCommand(COMMAND_DELETE, -1, -1))
	{
		unit.doCommand(COMMAND_DELETE, -1, -1);
		return true;
	}
	return false;
}

bool Game::tryGift(EUnitId unitId)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	if (unit.canDoCommand(COMMAND_GIFT, -1, -1))
	{
		unit.doCommand(COMMAND_GIFT, -1, -1);
		return true;
	}
	return false;
}

bool Game::tryLoad(EUnitId unitId, EUnitId transportId)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	const IDInfo transportIdInfo = GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(transportId))->getIDInfo();
	if (unit.canDoCommand(COMMAND_LOAD_UNIT, transportIdInfo.eOwner, transportIdInfo.iID))
	{
		unit.doCommand(COMMAND_LOAD_UNIT, transportIdInfo.eOwner, transportIdInfo.iID);
		return true;
	}
	return false;
}

bool Game::tryUnload(EUnitId unitId)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	if (unit.canDoCommand(COMMAND_UNLOAD, -1, -1))
	{
		unit.doCommand(COMMAND_UNLOAD, -1, -1);
		return true;
	}
	return false;
}

bool Game::tryUnloadAll(EUnitId unitId)
{
	CvUnit& unit = *GET_PLAYER(gGlobals.getGame().getActivePlayer()).getUnit(static_cast<int>(unitId));
	if (unit.canDoCommand(COMMAND_UNLOAD_ALL, -1, -1))
	{
		unit.doCommand(COMMAND_UNLOAD_ALL, -1, -1);
		return true;
	}
	return false;
}


bool Game::canChangeCivics() const
{
	return GET_PLAYER(gGlobals.getGame().getActivePlayer()).canRevolution(nullptr);
}

bool Game::canChangeReligion() const
{
	return GET_PLAYER(gGlobals.getGame().getActivePlayer()).canConvert(NO_RELIGION);
}

bool Game::canChangeCivicsTo(std::span<const ECivic> civics) const
{
	if (civics.size() != gGlobals.getNumCivicOptionInfos())
		throw BotFailure("Wrong number of civics.");

	return GET_PLAYER(gGlobals.getGame().getActivePlayer()).canRevolution(
		(civics | std::views::transform([](int i) { return static_cast<CivicTypes>(i); }) | std::ranges::to<std::vector>()).data()
	);
}

bool Game::canChangeStateReligionTo(std::optional<EReligion> religion) const
{
	return GET_PLAYER(gGlobals.getGame().getActivePlayer()).canConvert(religion ? static_cast<ReligionTypes>(*religion) : NO_RELIGION);
}

bool Game::tryChangeCivicsTo(std::span<const ECivic> civics) const
{
	if (canChangeCivicsTo(civics))
	{
		GET_PLAYER(gGlobals.getGame().getActivePlayer()).revolution(
			(civics | std::views::transform([](int i) { return static_cast<CivicTypes>(i); }) | std::ranges::to<std::vector>()).data(),
			true
		);
		return true;
	}
	else
		return false;
}

bool Game::tryChangeStateReligionTo(std::optional<EReligion> optReligion) const
{
	if (canChangeStateReligionTo(optReligion))
	{
		GET_PLAYER(gGlobals.getGame().getActivePlayer()).convert(
			optReligion ? static_cast<ReligionTypes>(*optReligion) : NO_RELIGION
			);
		return true;
	}
	else
		return false;
}

void Game::adjustSliders(std::array<int, ECommerce::Num> ratio)
{
	CvPlayer& player = GET_PLAYER(gGlobals.getGame().getActivePlayer());

	// Preprocess
	for (const int i : heck::range(ECommerce::Num))
	{
		if (!player.isCommerceFlexible(static_cast<CommerceTypes>(i)) && i != ECommerce::Gold)
			ratio[i] = 0;
		ratio[i] = std::max(0, ratio[i]);
	}
	
	// Sum.
	int sum = std::ranges::fold_left(ratio, 0, std::plus());

	if (sum <= 0)
	{
		// Shouldn't happen, but set to 100% gold.
		ratio[ECommerce::Gold] = 1;
		sum = 1;
	}

	// Scale to sum to 10, rounded to nearest.
	int rem = 0;
	for (const int i : heck::range(ECommerce::Num))
	{
		const int x = ratio[i] * 10 + rem;
		ratio[i] = (x + sum / 2) / sum;
		rem = x - ratio[i] * sum;
	}

	// Scale to percent.
	for (int& x : ratio)
		x *= 10;
	
	// Using new function to set percentages without further normalisation.
	player.setCommercePercents(ratio);
}
void Game::changeResearch(ETech tech)
{
	CvPlayer& player = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	if (static_cast<TechTypes>(tech) == NO_TECH)
		player.clearResearchQueue();
	else
		player.pushResearch(static_cast<TechTypes>(tech), true);
}

std::vector<City> Game::getRevealedCities() const
{
	std::vector<City> out;
	out.reserve(50);

	const TeamTypes activeTeamI = gGlobals.getGameINLINE().getActiveTeam();

	for (const auto playerI : heck::range<PlayerTypes>(MAX_PLAYERS))
	{
		CvPlayer& player = GET_PLAYER(playerI);
		int itIndex{};
		for (CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
			if (city->isRevealed(activeTeamI, false))
				out.push_back(convert(*city, false));
	}

	return out;
}

std::optional<City> Game::getRevealedCityAt(ivec2 coord) const
{
	const CvPlot& plot = *gGlobals.getMap().plot(coord.x, coord.y);
	if (const CvCity* const city = plot.getPlotCity())
		if (city->isRevealed(gGlobals.getGame().getActiveTeam(), false))
			return convert(*city, false);
	return std::nullopt;
}

static CvCity& accessActivePlayerCity(i16vec2 coord)
{
	CvCity& city = *gGlobals.getMap().plotINLINE(coord.x, coord.y)->getPlotCity();
	if (city.getOwnerINLINE() != gGlobals.getGameINLINE().getActivePlayer())
		throw BotFailure("Attempt to access another player's city.");
	return city;
}

bool Game::tryChangeProduction(i16vec2 coord, ProductionChoice choice)
{
	struct Visitor
	{
		CvCity& city;

		bool operator()(std::monostate) const
		{
			return true;
		}

		bool operator()(EBuildingType type) const
		{
			if (city.canConstruct(static_cast<BuildingTypes>(type)))
			{
				city.pushOrder(ORDER_CONSTRUCT, type, -1, false, false, false);
				return true;
			}
			else
				return false;
		}

		bool operator()(EUnitType type) const
		{
			if (city.canTrain(static_cast<UnitTypes>(type)))
			{
				city.pushOrder(ORDER_TRAIN, type, -1, false, false, false);
				return true;
			}
			else
				return false;
		}

		bool operator()(EProcess type) const
		{
			if (city.canMaintain(static_cast<ProcessTypes>(type)))
			{
				city.pushOrder(ORDER_MAINTAIN, type, -1, false, false, false);
				return true;
			}
			else
				return false;
		}

		bool operator()(EProject type) const
		{
			if (city.canCreate(static_cast<ProjectTypes>(type)))
			{
				city.pushOrder(ORDER_CREATE, type, -1, false, false, false);
				return true;
			}
			else
				return false;
		}
	};

	CvCity& city = accessActivePlayerCity(coord);
	Visitor visitor{ city };
	visitor.city.clearOrderQueue();
	return std::visit(visitor, choice);
}

bool Game::tryWhip(i16vec2 coord)
{
	CvCity& city = accessActivePlayerCity(coord);

	if (city.canHurry(static_cast<HurryTypes>(0)))
	{
		city.hurry(static_cast<HurryTypes>(0));
		return true;
	}
	else
		return false;
}

void Game::setSpecialistCounts(i16vec2 coord, std::span<const int> counts)
{
	CvCity& city = accessActivePlayerCity(coord);
	for (const size_t i : heck::range(counts.size()))
		city.alterSpecialistCount(static_cast<SpecialistTypes>(i), counts[i] - city.getSpecialistCount(static_cast<SpecialistTypes>(i)));
}

std::vector<CityBuildChoice> Game::getCityProductionChoices(i16vec2 coord) const
{
	CvCity& city = accessActivePlayerCity(coord);

	std::vector<CityBuildChoice> out;

	out.reserve(100);

	// We're expected to loop over classes, as in CvMainInterface.py.
	const CvCivilizationInfo& civInfo = gGlobals.getCivilizationInfo(GET_PLAYER(city.getOwnerINLINE()).getCivilizationType());


	for (const auto i : heck::range(gGlobals.getNumUnitClassInfos()))
		if (const auto type = static_cast<UnitTypes>(civInfo.getCivilizationUnits(i)); city.canTrain(type))
			out.push_back({ ProductionChoice(static_cast<EUnitType>(type)), city.getProductionNeeded(type) });

	for (const auto i : heck::range(gGlobals.getNumBuildingClassInfos()))
		if (const auto type = static_cast<BuildingTypes>(civInfo.getCivilizationBuildings(i)); city.canConstruct(type))
			out.push_back({ ProductionChoice(static_cast<EBuildingType>(type)), city.getProductionNeeded(type) });

	for (const auto i : heck::range<ProjectTypes>(gGlobals.getNumProjectInfos()))
		if (city.canCreate(i))
			out.push_back({ ProductionChoice(static_cast<EProject>(i)), city.getProductionNeeded(i) });

	for (const auto i : heck::range<ProcessTypes>(gGlobals.getNumProcessInfos()))
		if (city.canMaintain(i))
			out.push_back({ ProductionChoice(static_cast<EProcess>(i)), 0 });

	return out;
}


static bool markOffers(CLinkList<TradeData>& inv, const CLinkList<TradeData>& offer)
{
	auto invView = viewCLinkList(inv);
	for (const TradeData& item : viewCLinkList(offer))
		if (const auto it = std::ranges::find_if(invView, [item](const TradeData& invItem) { return CvDeal::isMatchingTradeItem(invItem, item); }); it != invView.end())
			it->m_bOffering = true;
		else
			return false;
	return true;
}

bool Game::tryNegotiate(EPlayer themI, TradeList& fromThem, TradeList& fromMe)
{
	CvPlayerAI& them = GET_PLAYER(static_cast<PlayerTypes>(themI));
	CvPlayerAI& us = GET_PLAYER(gGlobals.getGame().getActivePlayer());

	if (!diploCanTrade(them))
		return false;

	// Logicl from CvDiplomacyScreen.
	// them, us
	CLinkList<TradeData> invs[2];
	them.buildTradeTable(us.getID(), invs[0]);
	us.buildTradeTable(them.getID(), invs[1]);
	CLinkList<TradeData> offers[2]{
		::convert(fromThem),
		::convert(fromMe),
	};

	if (!markOffers(invs[0], offers[0]))
		return false;
	if (!markOffers(invs[1], offers[1]))
		return false;

	CLinkList<TradeData> counters[2];
	if (them.AI_counterPropose(
		us.getID(),
		&offers[1],
		&offers[0],
		&invs[1],
		&invs[0],
		&counters[1],
		&counters[0]
	))
	{
		for (int i = 0; i < 2; ++i)
			for (const TradeData item : viewCLinkList(counters[i]))
				if (CvDeal::isDual(item.m_eItemType))
					if (std::ranges::none_of(viewCLinkList(counters[1 - i]), std::bind_back(&CvDeal::isMatchingTradeItem, item)))
						counters[i - 1].insertAtEnd(item);

		// These will update the inventories, but not needed if the bot contains the right trade logic.
		//them.updateTradeList(us.getID(), invs[0], counters[0], counters[1]);
		//us.updateTradeList(them.getID(), invs[1], counters[1], counters[0]);

		fromThem = convertTradeList(counters[0], them, us);
		fromMe = convertTradeList(counters[1], us, them);

		return true;
	}
	else
		return false;
}

bool Game::tryTrade(EPlayer themI, const TradeList& fromThem, const TradeList& fromMe)
{
	CvPlayerAI& them = GET_PLAYER(static_cast<PlayerTypes>(themI));
	CvPlayerAI& us = GET_PLAYER(gGlobals.getGame().getActivePlayer());

	if (!diploCanTrade(them))
		return false;

	CLinkList<TradeData> dllOfferFromThem = ::convert(fromThem);
	CLinkList<TradeData> dllOfferFromUs = ::convert(fromMe);

	if (them.AI_considerOffer(us.getID(), &dllOfferFromUs, &dllOfferFromThem))
	{
		gGlobals.getGame().implementDeal(us.getID(), them.getID(), &dllOfferFromUs, &dllOfferFromThem, false);
		return true;
	}
	else
		return false;
}

bool Game::tryDeclareWar(ETeam themI)
{
	if (GET_TEAM(gGlobals.getGame().getActiveTeam()).canDeclareWar(static_cast<TeamTypes>(themI)))
	{
		GET_TEAM(gGlobals.getGame().getActiveTeam()).declareWar(static_cast<TeamTypes>(themI), false, NO_WARPLAN);
		return true;
	}
	else
		return false;
}

bool Game::tryCancelOpenBorders(EPlayer themI)
{
	const CvPlayerAI& them = GET_PLAYER(static_cast<PlayerTypes>(themI));
	const CvPlayerAI& us = GET_PLAYER(gGlobals.getGame().getActivePlayer());
	int itIndex{};
	for (CvDeal* deal = gGlobals.getGame().firstDeal(&itIndex); deal; deal = gGlobals.getGame().nextDeal(&itIndex))
	{
		const PlayerTypes players[2]{ deal->getFirstPlayer(), deal->getSecondPlayer() };

		if (deal->isCancelable() && (players[0] == us.getID() && players[1] == them.getID() || players[1] == us.getID() && players[0] == them.getID()))
		{
			if (std::ranges::contains(viewCLinkList(*deal->getFirstTrades()), TRADE_OPEN_BORDERS, &TradeData::m_eItemType))
			{
				deal->kill();
				return true;
			}
		}
	}
	return false;
}

void Game::saveGame(std::string_view name) const
{
	CvString nameStr(name);
	gGlobals.getDLLIFaceNonInl()->getEngineIFace()->SaveGame(nameStr);
}



#endif