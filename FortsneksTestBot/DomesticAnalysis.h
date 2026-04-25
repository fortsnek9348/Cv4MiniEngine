#pragma once

#include "Common.h"
#include "DynamicArray2D.h"

#include <vector>

namespace mybot
{
	struct MapUnitLookup;
	struct PathingPlot;
	struct MultipleSourceDistanceFieldCell;

	struct CityAnalysis
	{
		std::wstring name; // For debugging.
		// True iff one of the rival's plots is adjacent to one of our plots that this city is closest to.
		std::array<uint8_t, kMaxCivPlayers> rivalCivBorderPlotCounts{};
		bool hasSecondRing{};
		// True iff this city is not the closest city to any border plots. These cities are minimally threatened and imply !isExposedToLandBarbs and !isExposedToWaterBarbs and sum(rivalCivBorderPlotCounts) == 0.
		bool isInterior{};
		// The number of plots from which land barbs could spawn in and attack this city.
		int landBarbsExposureLevel{};
		// The number of plots from which water barbs could spawn in and attack this city's water resources.
		int waterBarbsExposureLevel{};

		//struct PopAnalysis
		//{
		//	int pop{};
		//	// Ignoring growth, what's the most we can get from plots.
		//	std::array<int16_t, EYield::Num> maxYieldRates{};
		//	// The max yield rates that work enough food to sustain this population, or the max yield possible at max food.
		//	// maxYieldRates[Food] == maxStableYieldRates[Food]
		//	std::array<int16_t, EYield::Num> maxSustainableYieldRates{};
		//};
		//
		//struct ImprovementGoalAnalysis
		//{
		//	int plotBuildTurns{}; // How long would it take to make this happen, ignoring pop growth.
		//	std::array<PopAnalysis, EPOP_NUM> popScenario{};
		//};
		//
		//std::array<ImprovementGoalAnalysis, EIMPROVEMENTGOAL_NUM> yieldScenarios{};
	};

	struct DomesticAnalysis
	{
		DynamicArray2D<bool> barbSuppressionMap{};
		DynamicArray2D<bool> barbPotentialSpawnMap{};
		std::vector<CityAnalysis> cityAnalyses;
	};

	DomesticAnalysis doDomesticAnalysis(
		EPlayer activePlayerI,
		std::span<const City> myCities,
		MapGeometry mapGeom,
		Span2D<const Plot> map,
		Span2D<const PathingPlot> pathingMap,
		Span2D<const MultipleSourceDistanceFieldCell> cityPathLengthField,
		std::span<const Unit> allVisibleUnits,
		const MapUnitLookup& mapMyUnitsLookup,
		const GlobalInfoData& infos
	);
}