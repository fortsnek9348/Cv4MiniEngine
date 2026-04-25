#pragma once

#include "Pathing.h"
#include "UnitProductionDemand.h"

#include <PlayerBotGameBinding/GameStructs.h>
#include <PlayerBotGameBinding/IGame.h>

namespace mybot
{
	struct MilitaryKnowledge
	{
		std::vector<bool> barbUnitsSeenBits;
		std::vector<bool> rivalUnitsSeenBits;
		std::vector<EUnitType> barbUnitsSeen;
		std::vector<EUnitType> rivalUnitsSeen;
		bool barbsHaveEnteredTerritory = false;

		void update(
			const GlobalInfoData& infos,
			EPlayer activePlayerI,
			Span2D<const Plot> plots, 
			std::span<const Unit> allUnits,
			std::span<const Unit> enemyUnits
		);
	};

	struct MilitaryAnalysis
	{
		bool canBarbsEnterTerritory = false;
		int barbsThreatTurn = 0;
		int rivalPowerRatioPercent = 0;

		std::vector<UnitProductionDemand> unitProductionDemands;
	};

	MilitaryAnalysis doMilitaryAnalysis(
		const CivState& civState,
		std::span<const Unit> myUnits,
		std::span<const Unit> enemyUnits,
		std::optional<ivec2> futureSettlerEscortTarget,
		std::span<const City> myCities,
		std::span<const std::optional<Player>> players,
		MapGeometry mapGeom,
		[[maybe_unused]] Span2D<const Plot> plots,
		[[maybe_unused]] const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
		[[maybe_unused]] std::span<const TechState> techs,
		const GlobalInfoData& infos,
		//const GameSetup& setup,
		const GlobalInfo& globalInfo,
		IGame& game,
		const MilitaryKnowledge& militaryKnowledge
	);

	

	//struct MilitaryAdvisor
	//{
	//	std::vector<EUnitType> barbUnitsSeen;
	//	std::vector<EUnitType> rivalUnitsSeen;
	//	bool barbsHaveEnteredTerritory = false;
	//	bool canBarbsEnterTerritory = false;
	//	int barbsThreatTurn = 0;
	//	int rivalPowerRatioPercent = 0;
	//
	//	std::vector<ProductionDemand> productionDemands;
	//
	//	void update(
	//		const CivState& civState,
	//		std::span<const Unit> myUnits,
	//		std::span<const Unit> enemyUnits,
	//		std::optional<ivec2> futureSettlerEscortTarget,
	//		std::span<const City> myCities,
	//		std::span<const std::optional<Player>> players,
	//		MapGeometry mapGeom,
	//		Span2D<const Plot> plots,
	//		const mybot::DynamicArray2D<mybot::MultipleSourceDistanceFieldCell>& cityPathLengthAnalysis,
	//		std::span<const TechState> techs,
	//		const GlobalInfoData& infos,
	//		//const GameSetup& setup,
	//		const GlobalInfo& globalInfo,
	//		IGame& game
	//	);
	//};
}