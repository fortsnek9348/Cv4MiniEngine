#pragma once

#include "FoundValueUtil.h"
#include "FoundValuePlayerNearestCityCache.h"
#include "FoundValueVectorisedCalculation.h"

#include "../VersionedBitmap2D.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	struct FoundValueCalculationCommonMapInterface;

	struct AreaMap
	{
		// Any area not in this is not settleable.
		std::unordered_map<int, iaabb2> unwrappedAreaRectsByAreaId;

		explicit AreaMap(CvMap& map);
	};

	class CommonCache
	{
	public:
		explicit CommonCache(CivMapGeometry, const AreaMap& areaMap);

		void onPlotTerrainOrFeatureChanged(const CvPlot&);
		void onPlotGetYieldChanged(const CvPlot&);
		void onPlotOwnerChanged(const CvPlot&, PlayerTypes from, PlayerTypes to);
		void onAfterCityDestroyed(const CvPlot&);
		void onAfterCityCreated(const CvPlot&);
		void onCityRadiusCountChanged(const CvPlot&);
		// Thread-safe.
		void onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to);
		void onAreaCityCountChanged(const CvArea& area, int from, int to);
		void onPlotBonusChanged(const CvPlot&);

		void update(PlayerTypes playerI);

		FoundValueCalculationCommonMapInterface createCommonMapInterface(ivec2 foundValueCoord) const;
		FoundValueCalculationCommonGlobalDataInterface createCommonGlobalDataInterface() const;

		void verify(const AreaMap& areaMap) const;

	private:
		FoundValueCalculationCommonGlobalDataInterface mGlobalData;

		PaddedArray2D<int16_t> mFlags;

		PaddedArray2D<int32_t> mAreaIds;
		// Depends on plot->getYield and whether the bonus is revealed.
		std::array<PaddedArray2D<int32_t>, 2> mHomePlotYieldsValuations; // without/with bonus
		// Depends on plot->getYield only.
		PaddedArray2D<int32_t> mPlotYields;
		// Depends on plot feature only.
		PaddedArray2D<int8_t> mFeatureHealthPercent;
		// Depends on CvPlot::calculateBestNatureYield, and whether the bonus is revealed.
		std::array<PaddedArray2D<int32_t>, 2> mBestNaturalYieldsWithImprovement;
		PaddedArray2D<int32_t> mBestNaturalYieldsWithBonusOnly;
		// Depends on all city locations.
		PlayerNearestCityCache mNearestCityCache;
		// Depends on plot bonuses. Events can add them sometimes.
		PaddedArray2D<int8_t> mPlotBonuses;

		// GC.getDefineINT("MIN_BARBARIAN_CITY_STARTING_DISTANCE") = 2
		int mRivalUnitRange = 0;
		InvalidationBitmapState mUnitMovementInvalidation;

		std::vector<ImprovementTypes> mBonusImprovementTypes;

		bool mAreaCityPresenceDirty = false;

		void updateBarbarianCivUnitAvoidance();
		void updateCityDistances();
		void updateAreaCityPresence();

		void updatePlotYieldStuff(const CvPlot& plot, ivec2 coord);
	};
}