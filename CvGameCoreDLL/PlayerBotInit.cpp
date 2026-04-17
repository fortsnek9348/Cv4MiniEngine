#include "PlayerBotGameInterface.h"
#include "CvInfos.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvMap.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvInitCore.h"
#include "CvPlayerAI.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/Infos.h>

#include <CommonStuff/range.h>

#include <iostream>

using namespace cvbot;

static constinit BotInit gBotInitInstance;

static MapGeometry getMapGeometry()
{
	CvMap& map = gGlobals.getMap();
	return {
		.dim{ map.getGridWidth(), map.getGridHeight() },
		.isWrapX = map.isWrapX(),
		.isWrapY = map.isWrapY(),
	};
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
	const CvGame& game = gGlobals.getGameINLINE();

	std::bitset<EGameOption::Num> options{};
	for (const auto opt : heck::range<GameOptionTypes>(NUM_GAMEOPTION_TYPES))
		options[opt] = game.isOption(opt);

	return GameSetup{
		.mapGeometry = getMapGeometry(),
		.options = options,
		.activePlayerI = static_cast<EPlayer>(game.getActivePlayer()),
		.modName = gGlobals.getDLLIFace()->getModName(false),
		.mapScriptName = gGlobals.getInitCore().getMapScriptName(),
		.speed = static_cast<EGameSpeed>(game.getGameSpeedType()),
		.worldSize = static_cast<EWorldSize>(gGlobals.getMap().getWorldSize()),
		.handicap = static_cast<EHandicap>(game.getHandicapType()),

		.firstTurnOfBarbMilitarySpawns = (GC.getHandicapInfo(game.getHandicapType()).getBarbarianCreationTurnsElapsed() * GC.getGameSpeedInfo(game.getGameSpeedType()).getBarbPercent()) / 100,
	};
}

GlobalInfoData BotInit::buildGlobalInfoData() const
{
	const CvGame& game = gGlobals.getGameINLINE();
	const CvPlayer& activePlayer = GET_PLAYER(game.getActivePlayer());
	const auto& activeCiv = gGlobals.getCivilizationInfo(activePlayer.getCivilizationType());
	
	GlobalInfoData infos;

	infos.civs.resize(gGlobals.getNumCivilizationInfos());
	for ([[maybe_unused]] const auto i : heck::range<CivilizationTypes>(infos.civs.size()))
	{
		;
	}

	infos.leaders.resize(gGlobals.getNumLeaderHeadInfos());
	for (const auto i : heck::range<LeaderHeadTypes>(infos.leaders.size()))
	{
		for (const auto j : heck::range<TraitTypes>(gGlobals.getNumTraitInfos()))
		{
			if (gGlobals.getLeaderHeadInfo(i).hasTrait(j))
			{
				const CvTraitInfo& traitInfo = gGlobals.getTraitInfo(j);
				infos.leaders[i].traits.push_back(static_cast<ELeaderTrait>(j));
				infos.leaders[i].creativeCulture += traitInfo.getCommerceChange(COMMERCE_CULTURE);
			}
		}
		
	}

	infos.buildingClasses.resize(gGlobals.getNumBuildingClassInfos());
	for (const auto i : heck::range<BuildingClassTypes>(infos.buildingClasses.size()))
	{
		infos.buildingClasses[i] = {
			.activeType = static_cast<EBuildingType>(activeCiv.getCivilizationBuildings(i)),
			.defaultType = static_cast<EBuildingType>(gGlobals.getBuildingClassInfo(i).getDefaultBuildingIndex()),
		};
	}
	
	infos.buildings.resize(gGlobals.getNumBuildingInfos());
	for (const auto i : heck::range<BuildingTypes>(infos.buildings.size()))
	{
		const auto& info = gGlobals.getBuildingInfo(i);
		infos.buildings[i] = {
			.klass = static_cast<EBuildingClass>(info.getBuildingClassType()),
			.productionCost = activePlayer.getProductionNeeded(i),
		};
		for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
			infos.buildings[i].obsoleteSafeCommerceChanges[j] = static_cast<int8_t>(info.getObsoleteSafeCommerceChange(j));
	}
	
	infos.unitClasses.resize(gGlobals.getNumUnitClassInfos());
	for (const auto i : heck::range<UnitClassTypes>(infos.unitClasses.size()))
	{
		infos.unitClasses[i] = {
			.activeType = static_cast<EUnitType>(activeCiv.getCivilizationUnits(i)),
			.defaultType = static_cast<EUnitType>(gGlobals.getUnitClassInfo(i).getDefaultUnitIndex()),
		};
	}
	
	infos.units.resize(gGlobals.getNumUnitInfos());
	for (const auto i : heck::range<UnitTypes>(infos.units.size()))
	{
		const auto& info = gGlobals.getUnitInfo(i);
		infos.units[i] = {
			.klass = static_cast<EUnitClass>(info.getUnitClassType()),
			.productionCost = activePlayer.getProductionNeeded(i),
			.cityDefenceModifier = info.getCityDefenseModifier(),
			.canAttack = info.getCombat() && !info.isOnlyDefensive(),
			.bNoDefensiveBonus = info.isNoDefensiveBonus(),
			.isAnimal = info.isAnimal(),
			.optMissionaryReligion = static_cast<EReligion>(NO_RELIGION),
			.domain = static_cast<EDomain>(info.getDomainType()),
		};
		if (info.getDefaultUnitAIType() == UNITAI_MISSIONARY)
		{
			for (const auto j : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
				if (info.getReligionSpreads(j) > 0)
					infos.units[i].optMissionaryReligion = static_cast<EReligion>(j);
		}
	}

	infos.cultureLevels.resize(gGlobals.getNumCultureLevelInfos());
	for (const auto i : heck::range<CultureLevelTypes>(infos.cultureLevels.size()))
	{
		const auto& info = gGlobals.getCultureLevelInfo(i);
		infos.cultureLevels[i] = {
			.culture = game.getCultureThreshold(static_cast<CultureLevelTypes>(i)),
			.cityDefenceModifier = info.getCityDefenseModifier(),
		};
	}

	return infos;
}

const IAllKnowingGameInterface& BotInit::getAllKnowingGameInterface() const
{
	return AllKnowingGameInterface::getInstance();
}

