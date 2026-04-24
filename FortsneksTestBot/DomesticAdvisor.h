#pragma once

#include "ProductionDemand.h"
#include "MapUnitLookup.h"
#include "Pathing.h"

#include <map>

namespace mybot
{
	struct MultipleSourceDistanceFieldCell;

	class DomesticAdvisor
	{
	public:
		//enum EPop
		//{
		//	EPOP_CURRENT, // Current population
		//	EPOP_ATHAPPYCAP, // Greatest population where signedHappiness(pop) >= 0 and pop <= EPOP_YIELDLIMIT. At least 1.
		//	EPOP_YIELDLIMIT, // The limit determined only by available yields.
		//
		//	EPOP_NUM,
		//};
		//
		//enum EImprovementGoal
		//{
		//	EIMPROVEMENTGOAL_CURRENT,
		//	// Improve bonuses, mine hills, farm for 3+ food yields to meet pop goal, cottage everything else. Prefer to keep existing improvements.
		//	EIMPROVEMENTGOAL_GENERAL,
		//	// Improve bonuses, farm everything, mine/cottage remainder.
		//	EIMPROVEMENTGOAL_MAXFOOD,
		//	// Improve bonuses, optimise farm/mine/workshop balance for max prod.
		//	EIMPROVEMENTGOAL_MAXSTABLEPROD,
		//	// Improve bonuses, optimise farm/cottage balance for max commerce.
		//	EIMPROVEMENTGOAL_MAXSTABLECOMMERCE,
		//
		//	EIMPROVEMENTGOAL_NUM,
		//};
		//
		//struct CityWorkPlan
		//{
		//	std::array<EImprovement, kNumCityWorkPlots> improvements{};
		//};
		//
		//static CityWorkPlan buildCityWorkPlan(
		//	const City& city,
		//	Span2D<const Plot> map,
		//	int popTarget,
		//	EImprovementGoal improvementGoal,
		//	const CivState& civstate,
		//	const GlobalInfoData& infos
		//);

		struct CityAnalysis
		{
			std::wstring name; // For debugging.
			// True iff one of the rival's plots is adjacent to one of our plots that this city is closest to.
			std::bitset<kMaxCivPlayers> hasCivOnBorder{};
			bool hasSecondRing{};
			// True iff this city is not the closest city to any border plots. These cities are minimally threatened and imply !isExposedToLandBarbs and !isExposedToWaterBarbs.
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

		void analyse(
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

		const CityAnalysis& getCityAnalysis(i16vec2 coord) const;

		bool areBarbsSuppressed(ivec2 coord) const;
		bool isPotentialBarbSpawnAt(ivec2 coord) const;
		
		void assignProduction(
			IGame& game,
			const GlobalInfoData& infos,
			const MapGeometry& geom,
			const Player& activePlayer,
			std::span<const City> myCities,
			std::span<const ProductionDemand> productionDemands
		);

	private:
		

		//struct CityDecision
		//{
		//	std::wstring name; // For debugging.
		//	ProductionChoice productionChoice{};
		//	
		//};

		std::map<i16vec2, CityAnalysis> mAnalysises;
		//std::map<i16vec2, CityDecision> mDecisions;

		DynamicArray2D<bool> mBarbSuppressionMap{};
		DynamicArray2D<bool> mBarbPotentialSpawnMap{};
	};
}