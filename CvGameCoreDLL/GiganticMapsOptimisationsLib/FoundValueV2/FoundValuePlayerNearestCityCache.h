#pragma once

#include "FoundValueCityDistanceField.h"
#include "FoundValueUtil.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	struct AreaMap;

	class PlayerNearestCityCache
	{
	public:
		explicit PlayerNearestCityCache(CivMapGeometry mapGeom, const AreaMap& areaMap, PlayerTypes filterPlayerI);
		
		void onAfterCityDestroyed(const CvPlot&, PlayerTypes playerI);
		void onAfterCityCreated(const CvPlot&, PlayerTypes playerI);
		void onCapitalChanged(const CvCity* city);

		void update();
		void verify() const;

		// High bit set if capital is nearest (tie-breaks are according to CvMap::findCity).
		static constexpr uint16_t kNoCities = UINT16_MAX;
		const PaddedArray2D<uint16_t>& getDistanceValueField() const;

	private:
		PlayerTypes mFilterPlayerI = NO_PLAYER;
		bool mIsDirty = false;
		CivMapGeometry mMapGeom;
		const CvCity* mCapital = nullptr;
		CityDistanceField mGlobalCityDistanceField;
		std::unordered_map<int, CityDistanceField> mAreaCityDistanceFields;
		PaddedArray2D<uint16_t> mDistanceValueField;
	};
}