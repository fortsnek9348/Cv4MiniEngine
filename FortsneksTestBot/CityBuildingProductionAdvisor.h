#pragma once

#include "Common.h"

#include <PlayerBotGameBinding/GameStructs.h>

namespace mybot
{
	struct CityAnalysis;
	struct MultipleSourceDistanceFieldCell;

	// How production decision will work is that buildings and units are separated.
	// Buildings are mostly built independently of eachother whereas units need higher level decision.
	// Buildings and units will then be bought together using an assignment solver.

	struct CityBuildingProductionRecomendation
	{
		ProductionChoice choice{};
		// This is the value that will be used in the assignment solver when comparing with unit demand values.
		int finalAssignmentValue{};
	};

	// This will clear city production.
	std::vector<CityBuildingProductionRecomendation> computeCityBuildingProductionRecomendations(
		IGame& game,
		const GlobalInfoData& infos,
		const CivState& civState,
		const Player& activePlayer,
		std::span<const City> myCities,
		std::span<const CityAnalysis> cityAnalyses
	);

	
}