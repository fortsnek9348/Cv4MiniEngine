#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueCityDistanceField.h"
#include "FoundValueUtil.h"

#include "../../CvGameCoreUtils.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;
using namespace GiganticMapsOptimisationsLib;

// NOTE: There is a magical algorithm at <https://perso.liris.cnrs.fr/david.coeurjolly/courses/voronoimap2d/> that will compute
//       voronoi regions by trivially looping over rows and columns of a grid.
// Might be worth implementing.
// Paper: A Linear Time Algorithm for Computing Exact Euclidean Distance Transforms of Binary Images in Arbitrary Dimensions
// Can be adapted to wrapped maps: https://github.com/DGtal-team/DGtal/blob/master/src/DGtal/geometry/volumes/distance/VoronoiMap.ih
// 
// The plotDistance distance metric would need to be implemented inside `hiddenBy`.
// 
// That magical function would need to calculate: `equidistant(U, V).y > equidistant(V, W).y`, a point equidistant within the column being evaluated.
// 
// There's an implementation for plain vanilla ecludian of course, but that won't work for `plotDistance`.
// 
// Note that this needs to handle wrapping too!


static bool isUnset(ivec2 v)
{
	return v.x < 0;
}

static int unwrapCoord(int dim, int min, int max, int x)
{
	if (x < min)
		return x + dim;
	else if (x < max)
		return x;
	else
		return x - dim;
}

ivec2 CityDistanceField::unwrapCoord(ivec2 wrappedCoord) const
{
	const ivec2 unwrappedCoord{
		::unwrapCoord(mOriginalMapGeom.dim.x, mUnwrappedFieldGeometry.origin.x, mUnwrappedFieldGeometry.origin.x + mUnwrappedFieldGeometry.geom.dim.x, wrappedCoord.x),
		::unwrapCoord(mOriginalMapGeom.dim.y, mUnwrappedFieldGeometry.origin.y, mUnwrappedFieldGeometry.origin.y + mUnwrappedFieldGeometry.geom.dim.y, wrappedCoord.y),
	};
	assert(iaabb2::sized(mUnwrappedFieldGeometry.origin, mUnwrappedFieldGeometry.geom.dim).contains(unwrappedCoord));
	return unwrappedCoord;
}

CityDistanceField::UnwrappedFieldGeometry::UnwrappedFieldGeometry(CivMapGeometry mapGeom, iaabb2 unwrappedRect)
	: origin(unwrappedRect.min)
	, geom(unwrappedRect.size(), false, false)
{
	assert(unwrappedRect.area() > 0);
	// Now we need to check if the search algorithm may need to cross over the ocean and wrap around the world to assign nearest cities correctly.
	// That is, if the width of the ocean is less than the width of the area.
	if (mapGeom.wrapX && unwrappedRect.size().x > mapGeom.dim.x / 2)
	{
		geom.wrapX = true;
		origin.x = 0;
		geom.dim.x = mapGeom.dim.x;
	}
	if (mapGeom.wrapY && unwrappedRect.size().y > mapGeom.dim.y / 2)
	{
		geom.wrapY = true;
		origin.y = 0;
		geom.dim.y = mapGeom.dim.y;
	}
}



CityDistanceField::CityDistanceField(const CivMapGeometry& mapGeom, iaabb2 unwrappedRect)
	: mOriginalMapGeom(mapGeom)
	, mUnwrappedFieldGeometry(mapGeom, unwrappedRect)
#if HECK_CityDistanceField_VERSION == 1
	, mQuadTree(mUnwrappedFieldGeometry.geom, 8, 2)
	, mInvalidated(true)
#endif
{
	mNearestCityArray = DynamicArray2D<i16vec2>(mUnwrappedFieldGeometry.geom.dim, -1); // -1 = unset
}

#if HECK_CityDistanceField_VERSION == 0
void CityDistanceField::setIsCity(ivec2 wrappedCoord, bool isCity)
{
	const ivec2 unwrappedCoord = unwrapCoord(wrappedCoord);
	const i16vec2 localCoord = i16vec2(unwrappedCoord - mUnwrappedFieldGeometry.origin);
	if (isCity)
	{
		if (mNearestCityArray[localCoord] != localCoord)
		{
			if (!isUnset(mNearestCityArray[localCoord]))
				removeRegion(localCoord);
			mNearestCityArray[localCoord] = localCoord;
			mOpenQueue.push_back(localCoord);
		}
	}
	else
	{
		// Handle this by blnking out the voronoi cell for this city.
		if (mNearestCityArray[localCoord] == localCoord)
			removeRegion(localCoord);
	}
}

void CityDistanceField::removeRegion(ivec2 localCoord)
{
	// Do a simple DFS. Set this voronoi cell to -1.
	std::vector<ivec2> visitStack{ localCoord };

	const iaabb2 localRect{ .max = mNearestCityArray.dim };

	const bool wrapX = mUnwrappedFieldGeometry.geom.wrapX;
	const bool wrapY = mUnwrappedFieldGeometry.geom.wrapY;
	const ivec2 mapDim = mOriginalMapGeom.dim;

	while (!visitStack.empty())
	{
		const ivec2 visitCoord = visitStack.back();
		visitStack.pop_back();

		mNearestCityArray[visitCoord] = -1;

		for (const ivec2 adjD : kAdj)
		{
			ivec2 adjCoord = visitCoord + adjD;

			if (wrapX)
			{
				if (adjCoord.x < 0)
					adjCoord.x += mapDim.x;
				if (adjCoord.x >= mapDim.x)
					adjCoord.x -= mapDim.x;
			}

			if (wrapY)
			{
				if (adjCoord.y < 0)
					adjCoord.y += mapDim.y;
				if (adjCoord.y >= mapDim.y)
					adjCoord.y -= mapDim.y;
			}

			if (localRect.contains(adjCoord))
			{
				if (mNearestCityArray[adjCoord] == i16vec2(localCoord))
					visitStack.push_back(adjCoord);
				else if (!isUnset(mNearestCityArray[adjCoord]))
					mOpenQueue.push_back(localCoord);
			}
		}
	}
}

bool CityDistanceField::update()
{
	// Pre-condition: All "set" entries in mNearestCityArray are correct, or they are overestimates/unset and advancing the open set frontier will correct them.

	bool changed = false;

	size_t openQueueStart = 0;

	const iaabb2 localRect{ .max = mNearestCityArray.dim };
	const iaabb2 simdRect = localRect.shrunk(1);

	const heck::MapGeometry mapGeom = mUnwrappedFieldGeometry.geom;

	using I32Vector = heck::simd::Vector<int32_t, 8>;
	using U32Vector = heck::simd::Vector<uint32_t, 8>;
	using I16Vector = heck::simd::Vector<int16_t, 8>;

	const int pitch = mNearestCityArray.dim.x;

	static constexpr heck::vec2<I16Vector> kAdjSimd{
		I16Vector::Array{ (int16_t)kAdj[0].x, (int16_t)kAdj[1].x, (int16_t)kAdj[2].x, (int16_t)kAdj[3].x, (int16_t)kAdj[4].x, (int16_t)kAdj[5].x, (int16_t)kAdj[6].x, (int16_t)kAdj[7].x },
		I16Vector::Array{ (int16_t)kAdj[0].y, (int16_t)kAdj[1].y, (int16_t)kAdj[2].y, (int16_t)kAdj[3].y, (int16_t)kAdj[4].y, (int16_t)kAdj[5].y, (int16_t)kAdj[6].y, (int16_t)kAdj[7].y },
	};

	[[maybe_unused]] const auto nearestCityArrayBytes = std::as_writable_bytes(std::span(mNearestCityArray.cells));

	while (openQueueStart < mOpenQueue.size())
	{
		const ivec2 visitCoord = mOpenQueue[openQueueStart++];

		const i16vec2 cityLocalCoord = mNearestCityArray[visitCoord];

		// First run times:
		// Clang, with no SIMD:  930.741ms
		// Clang, with 8x SIMD:  348.469ms (2.67x faster)
		// MSVC , with no SIMD: 1910.91 ms
		// MSVC , with 8x SIMD:  714.508ms
		if (simdRect.contains(visitCoord))
		{
			// No wrapping needed.
			const heck::vec2<I16Vector> adjCoords = heck::vec2<I16Vector>(visitCoord.x, visitCoord.y) + kAdjSimd;
			const I32Vector adjIndices = static_cast<I32Vector>(adjCoords.x) + static_cast<I32Vector>(adjCoords.y) * pitch;
		
			const U32Vector oldNearestCityLocalPackedCoords = U32Vector(nearestCityArrayBytes, adjIndices, heck::simd::imm<sizeof(i16vec2)>);
		
			const heck::vec2<I16Vector> cityLocalCoordVec(cityLocalCoord.x, cityLocalCoord.y);
			const heck::vec2<I16Vector> oldNearestCityLocalCoords = heck::unpackCoords(oldNearestCityLocalPackedCoords);
		
			const I16Vector::Mask unsetMask = vcmplt(oldNearestCityLocalCoords.x, 0);
			const I16Vector::Mask improvementMask = vcmplt(mapGeom.plotDistance(adjCoords, cityLocalCoordVec), mapGeom.plotDistance(adjCoords, oldNearestCityLocalCoords));
		
			const I16Vector::Mask updateMask = unsetMask | improvementMask;
		
			if (updateMask.any())
			{
				const U32Vector cityLocalPackedCoords = heck::packCoord(cityLocalCoord);
				cityLocalPackedCoords.scatterInto(nearestCityArrayBytes, adjIndices, updateMask, heck::simd::imm<sizeof(i16vec2)>);
				const size_t n = updateMask.asBitset().count();
				const U32Vector adjCoordsPacked = heck::packCoords(adjCoords);
				const U32Vector adjCoordsPackedCompressed = hcompress(adjCoordsPacked, updateMask);
				const std::array<i16vec2, 8> adjCoordsToPush = std::bit_cast<std::array<i16vec2, 8>>(adjCoordsPackedCompressed.toArray());
				mOpenQueue.append_range(std::span(adjCoordsToPush).subspan(0, n));
				changed = true;
			}
		}
		else
		{
			for (const ivec2 adjD : kAdj)
			{
				const ivec2 adjCoord = mapGeom.wrapCoord(visitCoord + adjD);

				if (localRect.contains(adjCoord))
				{
					const ivec2 oldNearestCityLocalCoord = mNearestCityArray[adjCoord];
					if (isUnset(oldNearestCityLocalCoord)
						|| mapGeom.plotDistance(adjCoord, cityLocalCoord) < mapGeom.plotDistance(adjCoord, oldNearestCityLocalCoord))
					{
						mNearestCityArray[adjCoord] = cityLocalCoord;
						mOpenQueue.push_back(adjCoord);
						changed = true;
					}
				}
			}
		}
	}

	// Check we're not over-visiting...
	//if (openQueueStart)
	//	std::clog << openQueueStart << '/' << localRect.area() << std::endl;

	mOpenQueue.clear();
	mOpenQueue.shrink_to_fit();

	// Entire grid should be set now.
	[[maybe_unused]] const size_t numUnreached = std::ranges::count(mNearestCityArray.cells, -1);
	assert(numUnreached == mNearestCityArray.cells.size() || numUnreached == 0);

	return changed;
}
#endif

std::optional<i16vec2> CityDistanceField::tryGetNearestCityAt(ivec2 wrappedCoord) const
{
	const ivec2 unwrappedCoord = unwrapCoord(wrappedCoord);
	const ivec2 localCoord = unwrappedCoord - mUnwrappedFieldGeometry.origin;
	if (isUnset(mNearestCityArray[localCoord]))
		return std::nullopt;
	else
		return mOriginalMapGeom.wrapCoord(mUnwrappedFieldGeometry.origin + ivec2(mNearestCityArray[localCoord]));
}

void CityDistanceField::verify(std::span<const i16vec2> wrappedCityLocations) const
{
	const CivMapGeometry mapGeom = mOriginalMapGeom;

	for (const int localY : range(mUnwrappedFieldGeometry.geom.dim.y))
	{
		for (const int localX : range(mUnwrappedFieldGeometry.geom.dim.x))
		{
			const ivec2 localCoord{ localX, localY };
			const i16vec2 nearestCityLocalCoord = mNearestCityArray[localCoord];
			if (wrappedCityLocations.empty())
			{
				if (!isUnset(nearestCityLocalCoord))
					std::abort();
			}
			else
			{
				const int cachedNearestDist = mapGeom.plotDistance(localCoord, nearestCityLocalCoord);

				int liveNearestDist = INT_MAX;

				for (const ivec2 wrappedCityCoord : wrappedCityLocations)
				{
					const ivec2 unwrappedCoord = unwrapCoord(wrappedCityCoord);
					const ivec2 localCityCoord = unwrappedCoord - mUnwrappedFieldGeometry.origin;
					liveNearestDist = std::min(liveNearestDist, mapGeom.plotDistance(localCoord, localCityCoord));
				}

				if (liveNearestDist != cachedNearestDist)
					std::abort();
			}
		}
	}
}

#endif