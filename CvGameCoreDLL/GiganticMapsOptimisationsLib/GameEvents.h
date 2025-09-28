#pragma once

namespace GiganticMapsOptimisationsLib
{
	// All events needed by subcomponents.
	enum class EGamePlotChangeEvent
	{
		Owner, // old, new player
		Terrain, // old, new
		Feature, // old, new
		RouteType, // old, new
		Improvement, // old, new
		HasBonus, // old, new
		
		
		HasUnits, // old, new

		HasCity, // old, new
		CityAfterCreated, // owner, owner
		CityBeforeDestroyed, // owner, owner
		CityAfterDestroyed, // owner, owner
		CityIsOccupation,
		CityCultureLevel,
		CityDefenceDamage,
		CityPlayerAddedDefence,
		InvisibilitySight, // old = new = team
		RevealedByTeam, // old = new = team
		IsVisibleByTeam, // old = new = team

		// All for unit plot danger.
		//PlotIsFort,
		CityOccupation,
		CityBuildingDefence,

		GetYield, // old, new
		CityRadiusCount, // old, new
		Bonus, // old, new
	};

	enum class EGameUnitChangeEvent
	{
		Created, // old = new = team
		Destroyed, // old = new = team

		

		BaseMoves,
		EnemyRoutesPromotion,
		IsMadeAttack,
		BlitzPromotion,
		Damage,
		BaseCombatStr,
		ExtraCombatPercent,
		Fortify,
		CityDefence,
		HillsDefence,
		FeatureDefence,
		TerrainDefence,
		CityAttack,
		HillsAttack,
		FeatureAttack,
		TerrainAttack,
		DomainModifier,
		RiverPromotion,
		AmphibPromotion,
		KamikazePercent,
		ImmuneToFirstStrikesCount,
		ExtraChanceFirstStrikes,
		ExtraFirstStrikes,
		CargoUnits,
		LeaderType,
		Level,
		Experience,
	};

	// All events needed by subcomponents.
	enum class EGameTeamChangeEvent
	{
		// valueA = teamA, valueB = teamB
		War, 
		Vassal,
		OpenBorders,
		PermanentAlliance,
		CanDeclareWar,
		AI_isSneakAttackReady,
		AI_isDeclareWar,
		AI_setWarPlan,
		Alive,
		ForcePeace,
		PermanentWarPeace,
		HasMet,

		PlayerCityDefenceModifier, // Team-wide in BTS. valueA, valueB = team
		AreaBorderObstacle, // valueA, valueB = team
		BridgeBuilding, // valueA, valueB = team
		TeamTech, // valueA = team, valueB = tech

		ForceRevealBonusChanged, // valueA = team, valueB = bonus
	};
}