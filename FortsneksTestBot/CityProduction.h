#pragma once

#include "Common.h"

#include <vector>

namespace mybot
{
	struct CityBuildingProductionRecomendation;
	struct UnitProductionDemand;

	// Assign city production from building and unit demands.
	std::vector<ProductionChoice> assignCityProduction(
		IGame& game,
		const GlobalInfoData& infos,
		MapGeometry geom,
		std::span<const City> myCities,
		std::span<const CityBuildingProductionRecomendation> buildingRecommendations,
		std::span<const UnitProductionDemand> productionDemands
	);
}