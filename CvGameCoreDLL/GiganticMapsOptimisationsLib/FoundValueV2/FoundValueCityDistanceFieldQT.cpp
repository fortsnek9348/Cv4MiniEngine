#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueCityDistanceField.h"

#if HECK_CityDistanceField_VERSION == 1

#include "FoundValueUtil.h"
#include <CommonStuff/ParallelExt.h>

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

void CityDistanceField::setIsCity(ivec2 wrappedCoord, bool isCity)
{
	const ivec2 unwrappedCoord = unwrapCoord(wrappedCoord);
	const i16vec2 localCoord = i16vec2(unwrappedCoord - mUnwrappedFieldGeometry.origin);
	assert(localCoord.x < mUnwrappedFieldGeometry.geom.dim.x && localCoord.y < mUnwrappedFieldGeometry.geom.dim.y);
	if (isCity)
		mQuadTree.insert(localCoord);
	else
		mQuadTree.erase(localCoord);
	
	mInvalidated = true;
}

bool CityDistanceField::update()
{
	if (!mInvalidated)
		return false;

	using heck::QuadTree;


	//if (mUnwrappedFieldGeometry.geom.dim.x % QuadTree::kSimdQueryDim.x)
	//	std::abort();
	//if (mUnwrappedFieldGeometry.geom.dim.y % QuadTree::kSimdQueryDim.y)
	//	std::abort();

	heck::parallelForEachN(mUnwrappedFieldGeometry.geom.dim.y / QuadTree::kSimdQueryDim.y, [&](int blkY) {
	//heck::serialForEachN(mMapDim.y / QuadTree::kSimdQueryDim.y, [&](int blkY) {
		const int y = blkY * QuadTree::kSimdQueryDim.y;
		QuadTree::I16Vector2 hints{ -1, -1 };

		for (const int blkX : range(mUnwrappedFieldGeometry.geom.dim.x / QuadTree::kSimdQueryDim.x))
		{
			const int x = blkX * QuadTree::kSimdQueryDim.x;
			const QuadTree::I16Vector2 coords = mQuadTree.findNearestSIMD({ x, y }, hints);
			const QuadTree::I16Vector::Array resultX = coords.x.toArray();
			const QuadTree::I16Vector::Array resultY = coords.y.toArray();
			for (const int dy : range(QuadTree::kSimdQueryDim.y))
			{
				for (const int dx : range(QuadTree::kSimdQueryDim.x))
				{
					const unsigned int index = dx + dy * QuadTree::kSimdQueryDim.x;
					const i16vec2 result(resultX[index], resultY[index]);
					mNearestCityArray[{ x + dx, y + dy }] = result;
				}
			}

			hints = coords;
		}
	});

	// In case unwrapped field dims are not divisible by 8. Typical for islands.
	// TODO: Maybe round the dimensions?
	for (const int y : range(mUnwrappedFieldGeometry.geom.dim.y / QuadTree::kSimdQueryDim.y * QuadTree::kSimdQueryDim.y, mUnwrappedFieldGeometry.geom.dim.y))
	{
		i16vec2 hint = -1;
		for (const int x : range(mUnwrappedFieldGeometry.geom.dim.x))
			mNearestCityArray[{ x, y }] = hint = mQuadTree.findNearest({ x, y }, hint);
	}

	for (const int y : range(mUnwrappedFieldGeometry.geom.dim.y))
	{
		i16vec2 hint = -1;
		for (const int x : range(mUnwrappedFieldGeometry.geom.dim.x / QuadTree::kSimdQueryDim.x * QuadTree::kSimdQueryDim.x, mUnwrappedFieldGeometry.geom.dim.x))
			mNearestCityArray[{ x, y }] = hint = mQuadTree.findNearest({ x, y }, hint);
	}
	
	mInvalidated = false;

	return true;
}

#endif

#endif