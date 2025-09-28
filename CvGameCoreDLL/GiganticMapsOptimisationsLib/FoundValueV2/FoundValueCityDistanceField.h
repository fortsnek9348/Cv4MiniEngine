#pragma once

#include "../Util.h"

// 0 = dijkstra based
// 1 = quad-tree based
#define HECK_CityDistanceField_VERSION 1

#if HECK_CityDistanceField_VERSION == 1
#include <CommonStuff/QuadTree.h>
#endif

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	class CityDistanceField
	{
	public:
		explicit CityDistanceField(const CivMapGeometry& mapGeom, iaabb2 unwrappedRect);

		void setIsCity(ivec2 wrappedCoord, bool isCity);

		// Returns true iff a nearest city was changed.
		// This is by far the slowest part of the found value system, when an update is needed.
		[[nodiscard]] bool update();

		// The coordinate is returned so that the caller can check if it's a capital city.
		// Call only if there are any cities on this area (otherwise, an unspecified coordinate will be returned).
		std::optional<i16vec2> tryGetNearestCityAt(ivec2 wrappedCoord) const;

		// Update first!
		void verify(std::span<const i16vec2> wrappedCityLocations) const;

	private:
		CivMapGeometry mOriginalMapGeom;

		// The MapGeometry of mNearestCityArray.
		struct UnwrappedFieldGeometry
		{
			ivec2 origin;
			heck::MapGeometry geom;

			explicit UnwrappedFieldGeometry(CivMapGeometry mapGeom, iaabb2 unwrappedRect);
		};

		UnwrappedFieldGeometry mUnwrappedFieldGeometry;

		std::vector<i16vec2> mCityList;

		// -1 iff no nearest city (which means there are no cities at all)
		heck::DynamicArray2D<i16vec2> mNearestCityArray;

#if HECK_CityDistanceField_VERSION == 0
		std::vector<i16vec2> mOpenQueue;

		void removeRegion(ivec2 localCoord);
#else
		heck::QuadTree mQuadTree;
		bool mInvalidated = false;
#endif

		ivec2 unwrapCoord(ivec2 wrappedCoord) const;
	};
}