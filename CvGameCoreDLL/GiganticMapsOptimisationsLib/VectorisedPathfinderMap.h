#pragma once

#include "TeamPathingPlotPropsCache.h"
#include "PlotDangerCache.h"
#include "GameEvents.h"
#include "UnitPathingUtil.h"

//#include <CommonStuff/PQSimdAStar.h>
#include <CommonStuff/Simd.h>
#include <CommonStuff/NoCopy.h>

#include "../CvEnums.h"

#include <array>
#include <span>
#include <map>
#include <future>

class CvPlot;
class CvUnit;
class CvMap;

namespace GiganticMapsOptimisationsLib
{
	class VectorisedPathfinderMap : public heck::NoCopyNoMove // self-referencing
	{
	public:
		static constexpr int kNumLandmarks = 16;
		using MultiLandmarkDistanceVector = heck::simd::Vector<int32_t, kNumLandmarks>;
		using MultiLandmarkDistanceField = DynamicArray2D<MultiLandmarkDistanceVector>;

		VectorisedPathfinderMap();

		void onTeamEvent(EGameTeamChangeEvent, TeamTypes, int valueB);
		void onPlotEvent(EGamePlotChangeEvent, const CvPlot& plot, int oldValue = -1, int newValue = -1);
		void onUnitEvent(EGameUnitChangeEvent, const CvUnit& unit);
		// Thread-safe for MoveFrom and MoveTo.
		void onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to);

		static uint32_t getOwnUnitMovementTeamPlotPropsInvalidationMask();
		static uint32_t getOwnUnitMovementTeamPlotPropsInvalidationMask(const CvSelectionGroup&);

		// Used to garbage collect.
		// Doesn't have to be called on actual turn counter changes.
		void onGameTurn();

		void update(TeamTypes team);
		
		// Remember to update first!
		void verify(TeamTypes team) const;

		struct LandmarkDistanceFieldWithBias
		{
			const MultiLandmarkDistanceField* distanceField{};
			int bias{}; // Subtract this from distances.
		};

		LandmarkDistanceFieldWithBias tryGetMultiLandmarkDistanceField(const CvSelectionGroup& group);
		const TeamPathingPlotPropsCache& getTeamPlotProps(TeamTypes) const;

		PromotionTypes getForestMovementPromotion() const;
		PromotionTypes getHillsMovementPromotion() const;

		
	
	private:
		struct HeuristicUnitType
		{
			int maxMP = 0;
			std::array<int, kNumRouteTypes> teamRouteCostChanges{};
			bool doubleForestMovement = false;
			bool doubleHillsMovement = false;
			bool water = false;

			std::string toString() const;

			friend auto operator<=>(HeuristicUnitType, HeuristicUnitType) = default;
		};

		struct HeuristicInstance
		{
			int firstUseGCTime = INT_MAX;
			int lastGCTimeUsed = -1;
			MultiLandmarkDistanceField distanceField;
			heck::ISimdAStarGraph::Vector evaluatedRouteCosts;
			int bias = 0;

			std::future<MultiLandmarkDistanceField> deferredDistanceField;
			int deferredBias = 0;
		};

		using HeuristicPlot = uint8_t;

		struct LandmarkHeuristicGraph;

		struct HeuristicMap
		{
			// When non-zero, we can't just change the plots inplace.
			std::atomic_size_t numConcurrentUsers{};
			DynamicArray2D<HeuristicPlot> plots;
		};

		int mCurrentGCTime = 0;

		// Precomputed, constant.
		std::vector<std::array<i16vec2, kNumLandmarks>> mLandAreaLandmarks;
		std::vector<std::array<i16vec2, kNumLandmarks>> mWaterAreaLandmarks;
		std::shared_ptr<HeuristicMap> mLandHeuristicMap;
		std::shared_ptr<HeuristicMap> mWaterHeuristicMap;
		std::map<HeuristicUnitType, HeuristicInstance> mHeuristicInstances;

		std::vector<TeamPathingPlotPropsCache> mTeamPlotProps;
		//std::vector<BorderPlotDangerSourceCache> mTeamBorderPlotDanger;
		AllBorderPlotDangerSourceCache mAllBorderPlotDangerSourceCache;
		UnitPlotDangerCache mUnitPlotDanger;

		ImprovementTypes mFortImprovementI = NO_IMPROVEMENT;
		PromotionTypes mForestMovementPromotionI = NO_PROMOTION;
		PromotionTypes mHillsMovementPromotionI = NO_PROMOTION;

		static DynamicArray2D<HeuristicPlot> buildHeuristicMap(const CvMap& map, bool water);
		static HeuristicPlot calcHeuristicPlot(const CvMap& map, ivec2 coord, bool water);
		ivec2 pickNewLandmarkLocation(const HeuristicMap& plotMap, std::span<const i16vec2> seeds) const;
		std::array<i16vec2, kNumLandmarks> computeLandmarkLocations(const HeuristicMap& plotMap, ivec2 seed) const;
		static MultiLandmarkDistanceField computeLandmarkHeuristicInstance(
			std::span<const std::array<i16vec2, kNumLandmarks>> areas,
			const DynamicArray2D<HeuristicPlot>& heuristicMap,
			HeuristicUnitType unit,
			heck::ISimdAStarGraph::Vector evaluatedRouteCosts
		);


		void updateHeuristicPlot(ivec2 coord);
	};

	
	//std::array<i16vec2, kNumLandmarks> computeLandmarkLocations(const CvArea* area);
	//PathfinderHeuristicInstance computeLandmarkHeuristicInstance(const TeamPathingPlotPropsCache* plotProps, const std::array<i16vec2, kNumLandmarks>& landmarkLocations, int maxMP, bool doubleForestMovement, bool doubleHillsMovement);


	
}