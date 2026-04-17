#pragma once

#include "EnumFwd.h"

#include <vector>

namespace cvbot
{
	struct CivilisationInfo
	{
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
	};

	struct CultureLevelInfo
	{
		int culture{};
		int cityDefenceModifier{};
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
	};
}