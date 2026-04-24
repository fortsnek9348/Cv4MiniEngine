#include "PlayerBotGameInterface.h"
#include "CvInfos.h"
#include "CvGlobals.h"
#include "CvGameAI.h"
#include "CvMap.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvInitCore.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "CvGameCoreUtils.h"

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
	const CvTeam& activeTeam = GET_TEAM(activePlayer.getTeam());
	const auto& activeCiv = gGlobals.getCivilizationInfo(activePlayer.getCivilizationType());
	
	GlobalInfoData infos;

	infos.civs.resize(gGlobals.getNumCivilizationInfos());
	for (const auto i : heck::range<CivilizationTypes>(infos.civs.size()))
	{
		const CvCivilizationInfo& info = gGlobals.getCivilizationInfo(i);
		for (const auto tech : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
			if (info.isCivilizationFreeTechs(tech))
				infos.civs[i].freeTechs.push_back(static_cast<ETech>(tech));
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
			.isWorldWonder = ::isWorldWonderClass(i),
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
			.baseCombatStrength = info.getCombat(),
			.canAttack = info.getCombat() && !info.isOnlyDefensive(),
			.bNoDefensiveBonus = info.isNoDefensiveBonus(),
			.isAnimal = info.isAnimal(),
			.isMilitaryHappiness = info.isMilitaryHappiness(),
			.optMissionaryReligion = static_cast<EReligion>(NO_RELIGION),
			.domain = static_cast<EDomain>(info.getDomainType()),
			.moveSteps = static_cast<uint8_t>(info.getMoves()),
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

	infos.techs.resize(gGlobals.getNumTechInfos());
	for (const auto i : heck::range<TechTypes>(infos.techs.size()))
	{
		const auto& info = gGlobals.getTechInfo(i);
		infos.techs[i] = {
			.researchCost = activeTeam.getResearchCost(i),
			.tradeRoutes = info.getTradeRoutes(),
			.featureProductionModifier = info.getFeatureProductionModifier(),
			.workerSpeedModifier = info.getWorkerSpeedModifier(),
			.health = info.getHealth(),
			.happiness = info.getHappiness(),
			.firstFreeTechs = info.getFirstFreeTechs(),
			.powerValue = info.getPowerValue(),
			.isRepeat                   /**/ = info.isRepeat(),
			.isTrade                    /**/ = info.isTrade(),
			.isDisable                  /**/ = info.isDisable(),
			.isGoodyTech                /**/ = info.isGoodyTech(),
			.isExtraWaterSeeFrom        /**/ = info.isExtraWaterSeeFrom(),
			.isMapCentering             /**/ = info.isMapCentering(),
			.isMapVisible               /**/ = info.isMapVisible(),
			.isMapTrading               /**/ = info.isMapTrading(),
			.isTechTrading              /**/ = info.isTechTrading(),
			.isGoldTrading              /**/ = info.isGoldTrading(),
			.isOpenBordersTrading       /**/ = info.isOpenBordersTrading(),
			.isDefensivePactTrading     /**/ = info.isDefensivePactTrading(),
			.isPermanentAllianceTrading /**/ = info.isPermanentAllianceTrading(),
			.isVassalStateTrading       /**/ = info.isVassalStateTrading(),
			.isBridgeBuilding           /**/ = info.isBridgeBuilding(),
			.isIrrigation               /**/ = info.isIrrigation(),
			.isIgnoreIrrigation         /**/ = info.isIgnoreIrrigation(),
			.isWaterWork                /**/ = info.isWaterWork(),
			.isRiverTrade               /**/ = info.isRiverTrade(),
			.firstFreeUnitClass = static_cast<EUnitClass>(info.getFirstFreeUnitClass()),
		};

		for (int j = 0; j < NUM_DOMAIN_TYPES; ++j)
			infos.techs[i].domainExtraMoves[j] = static_cast<int8_t>(info.getDomainExtraMoves(j));

		for (int j = 0; j < gGlobals.getNUM_OR_TECH_PREREQS(); j++)
			if (TechTypes ePrereq = static_cast<TechTypes>(info.getPrereqOrTechs(j)); ePrereq != NO_TECH)
				infos.techs[i].prereqOrTechs.push_back(static_cast<ETech>(ePrereq));
	
		for (int j = 0; j < gGlobals.getNUM_AND_TECH_PREREQS(); j++)
			if (TechTypes ePrereq = static_cast<TechTypes>(info.getPrereqAndTechs(j)); ePrereq != NO_TECH)
				infos.techs[i].prereqAndTechs.push_back(static_cast<ETech>(ePrereq));

		for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
			infos.techs[i].isCommerceFlexible[j] = info.isCommerceFlexible(j);

		for (int j = 0; j < gGlobals.getNumTerrainInfos(); ++j)
			if (info.isTerrainTrade(j))
				infos.techs[i].terrainTrades.push_back(static_cast<ETerrain>(j));
	}

	for (const auto i : heck::range<UnitTypes>(infos.units.size()))
	{
		const auto& info = gGlobals.getUnitInfo(i);

		if (info.getPrereqAndTech() != NO_TECH)
			infos.units[i].requiredTechs.push_back(static_cast<ETech>(info.getPrereqAndTech()));

		for (int j = 0; j < gGlobals.getNUM_UNIT_AND_TECH_PREREQS(); j++)
			if (info.getPrereqAndTechs(j) != NO_TECH)
				infos.units[i].requiredTechs.push_back(static_cast<ETech>(info.getPrereqAndTechs(j)));

		for (const ETech tech : infos.units[i].requiredTechs)
			infos.techs[tech].unitsPotentiallyUnlocked.push_back(static_cast<EUnitType>(i));
	}
	for (const auto i : heck::range<BuildTypes>(gGlobals.getNumBuildInfos()))
	{
		const auto& info = gGlobals.getBuildInfo(i);
		if (info.getTechPrereq() != NO_TECH)
			infos.techs[info.getTechPrereq()].buildActionsUnlocked.push_back(static_cast<EBuild>(i));
	}

	for (const auto i : heck::range<ReligionTypes>(gGlobals.getNumReligionInfos()))
	{
		if (!GC.getGameINLINE().isReligionSlotTaken(i))
		{
			const TechTypes tech = static_cast<TechTypes>(GC.getReligionInfo(i).getTechPrereq());
			if (tech != NO_TECH)
				infos.techs[tech].isIfFirstThenFoundReligion = true;
		}
	}

	for (const auto tech : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
	{
		if (activePlayer.getTechFreeUnit(tech) != NO_UNIT)
			infos.techs[tech].isIfFirstThenSpawnsGreatPerson = true;
		if (gGlobals.getTechInfo(tech).getFirstFreeTechs() > 0)
			infos.techs[tech].isIfFirstThenGetFreeTech = true;
		
		infos.techs[tech].hasFirstToResearchBonus = infos.techs[tech].isIfFirstThenFoundReligion
			|| infos.techs[tech].isIfFirstThenSpawnsGreatPerson
			|| infos.techs[tech].isIfFirstThenGetFreeTech;
	}

	infos.projects.resize(gGlobals.getNumProjectInfos());
	for (const auto i : heck::range<ProjectTypes>(gGlobals.getNumProjectInfos()))
	{
		const auto& info = gGlobals.getProjectInfo(i);
		infos.projects[i] = {
			.productionCost = activePlayer.getProductionNeeded(i),
			.optVictoryPrereq = static_cast<EVictory>(info.getVictoryPrereq()),
		};
	}

	const CvHandicapInfo& handicapInfo = gGlobals.getHandicapInfo(game.getHandicapType());
	const CvGameSpeedInfo& speedInfo = gGlobals.getGameSpeedInfo(game.getGameSpeedType());

	infos.speedInfo = {
		.researchPercent = speedInfo.getResearchPercent(),
		.buildingProdPercent = speedInfo.getConstructPercent(),
		.unitProdPercent = speedInfo.getTrainPercent(),
		.projectProdPercent = speedInfo.getCreatePercent(),
	};

	for (const TechTypes tech : heck::range<TechTypes>(gGlobals.getNumTechInfos()))
	{
		if (handicapInfo.isFreeTechs(tech))
			infos.handicap.freeTechs.push_back(static_cast<ETech>(tech));
		if (handicapInfo.isAIFreeTechs(tech))
			infos.handicap.freeAITechs.push_back(static_cast<ETech>(tech));
	}

	infos.handicap.firstMilitaryBarbCreationTurn = handicapInfo.getBarbarianCreationTurnsElapsed() * speedInfo.getBarbPercent() / 100;

	infos.techCostTotalKnownTeamModifier = gGlobals.getDefineINT("TECH_COST_TOTAL_KNOWN_TEAM_MODIFIER");
	infos.techCostKnownPrereqModifier = gGlobals.getDefineINT("TECH_COST_KNOWN_PREREQ_MODIFIER");
	infos.baseResearchRate = gGlobals.getDefineINT("BASE_RESEARCH_RATE");
	infos.percentAngerDivisor = gGlobals.getPERCENT_ANGER_DIVISOR();
	

	return infos;
}

const IAllKnowingGameInterface& BotInit::getAllKnowingGameInterface() const
{
	return AllKnowingGameInterface::getInstance();
}

