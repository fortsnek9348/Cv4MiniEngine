#pragma once

#include "EnumFwd.h"

#include <vector>

namespace cvbot
{
	struct CivilisationInfo
	{
		std::vector<ETech> freeTechs;
	};

	struct LeaderInfo
	{
		std::vector<ELeaderTrait> traits;

		int creativeCulture{};
	};

	struct BuildingClassInfo
	{
		EBuildingType activeType{};
		EBuildingType defaultType{};
		bool isWorldWonder{};
	};

	struct BuildingInfo
	{
		EBuildingClass klass{};
		int productionCost{};
		// Palace: 2 culture
		std::array<int8_t, ECommerce::Num> obsoleteSafeCommerceChanges{};
	};

	struct UnitClassInfo
	{
		EUnitType activeType{};
		EUnitType defaultType{};
	};

	struct UnitInfo
	{
		EUnitClass klass{};
		int productionCost{};
		int cityDefenceModifier{}; // archer = 50
		bool canAttack{}; // strength > 0 && !isOnlyDefensive
		bool bNoDefensiveBonus{};
		bool isAnimal{};
		EReligion optMissionaryReligion{};
		EDomain domain{};
		std::vector<ETech> requiredTechs;
	};

	struct CultureLevelInfo
	{
		int culture{};
		int cityDefenceModifier{};
	};

	struct TechInfo
	{
		int researchCost{};
		int tradeRoutes{};
		int featureProductionModifier{};
		int workerSpeedModifier{};
		int health{};
		int happiness{};
		int firstFreeTechs{};

		int powerValue{}; // This could be useful.

		bool isRepeat				    : 1 {};
		bool isTrade				    : 1 {};
		bool isDisable				    : 1 {};
		bool isGoodyTech			    : 1 {};
		bool isExtraWaterSeeFrom	    : 1 {};
		bool isMapCentering			    : 1 {};
		bool isMapVisible			    : 1 {};
		bool isMapTrading			    : 1 {};
		bool isTechTrading			    : 1 {};
		bool isGoldTrading			    : 1 {};
		bool isOpenBordersTrading	    : 1 {};
		bool isDefensivePactTrading	    : 1 {};
		bool isPermanentAllianceTrading : 1 {};
		bool isVassalStateTrading	    : 1 {};
		bool isBridgeBuilding		    : 1 {};
		bool isIrrigation			    : 1 {};
		bool isIgnoreIrrigation		    : 1 {};
		bool isWaterWork			    : 1 {};
		bool isRiverTrade			    : 1 {};

		bool hasFirstToResearchBonus : 1 {};
		bool isIfFirstThenFoundReligion : 1 {};
		bool isIfFirstThenSpawnsGreatPerson : 1 {};
		bool isIfFirstThenGetFreeTech : 1 {};

		std::array<int8_t, EDomain::Num> domainExtraMoves{};

		EUnitClass firstFreeUnitClass{};
		
		std::bitset<ECommerce::Num> isCommerceFlexible{};
		std::vector<ETerrain> terrainTrades{};

		// Only list active civ's unit types.
		std::vector<EUnitType> unitsPotentiallyUnlocked;

		std::vector<EBuild> buildActionsUnlocked;

		std::vector<ETech> prereqOrTechs{};
		std::vector<ETech> prereqAndTechs{};
	};

	struct SpeedInfo
	{
		int researchPercent{};
	};

	struct HandicapInfo
	{
		// Both the human player and the AI gets these.
		// CvGame::initFreeState
		std::vector<ETech> freeTechs;
		std::vector<ETech> freeAITechs;

		// CvGame::createBarbarianUnits
		int firstMilitaryBarbCreationTurn = 0;
	};

	struct GlobalInfoData
	{
		std::vector<CivilisationInfo> civs;
		std::vector<LeaderInfo> leaders;
		std::vector<BuildingClassInfo> buildingClasses;
		std::vector<BuildingInfo> buildings;
		std::vector<UnitClassInfo> unitClasses;
		std::vector<UnitInfo> units;
		std::vector<CultureLevelInfo> cultureLevels;
		std::vector<TechInfo> techs;
		SpeedInfo speedInfo{}; // Only the current speed is relevent.
		HandicapInfo handicap{};
		
		int techCostTotalKnownTeamModifier{};
		int techCostKnownPrereqModifier{};
		int baseResearchRate{}; // 1
		int percentAngerDivisor{};
	};
}