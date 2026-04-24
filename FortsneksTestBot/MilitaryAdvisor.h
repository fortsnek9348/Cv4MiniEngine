#pragma once

#include "Pathing.h"
#include "ProductionDemand.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/IGame.h>

namespace mybot
{
	// TODO: Scouting needs a target

	struct MilitaryAdvisor
	{
		std::vector<EUnitType> barbUnitsSeen;
		std::vector<EUnitType> rivalUnitsSeen;
		bool barbsHaveEnteredTerritory = false;
		bool canBarbsEnterTerritory = false;
		int barbsThreatTurn = 0;

		// Use them for scouting, extra defence, barb capturing, conquering.
		//std::vector<EUnitId> spareMilitaryUnits;

		//struct CityDefenceStatus
		//{
		//	i16vec2 cityCoord{};
		//	// This measures how much defence strength is expected to be needed, including defence promotions.
		//	// If <= 1, only minimal defence is needed. Almost certainly will never be attacked.
		//	int defenceStrengthDemand = 0;
		//	// To attack roaming pillagers.
		//	int roamingBarbPillagersActiveDefenceDemand = 0;
		//};
		//
		//std::vector<CityDefenceStatus> cityDefenceStatuses;

		// Including active pillaging defence.
		//int cityDefenceDemand{};
		//int antiBarbDemand{};

		std::vector<ProductionDemand> productionDemands;

		void update(
			const CivState& civState,
			std::span<const Unit> myUnits,
			std::span<const Unit> enemyUnits,
			std::optional<ivec2> futureSettlerEscortTarget,
			std::span<const City> myCities,
			std::span<const std::optional<Player>> players,
			MapGeometry mapGeom,
			Span2D<const Plot> plots,
			const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
			std::span<const TechState> techs,
			const GlobalInfoData& infos,
			//const GameSetup& setup,
			const GlobalInfo& globalInfo,
			IGame& game
		);
	};
}