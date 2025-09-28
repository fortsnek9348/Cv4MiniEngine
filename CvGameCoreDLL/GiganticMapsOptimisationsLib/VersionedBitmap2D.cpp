#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "VersionedBitmap2D.h"

using namespace GiganticMapsOptimisationsLib;



VersionedBlocks2D::VersionedBlocks2D(ivec2 dim)
	: mDim(dim)
	, mL1BlockVersions(cdiv(dim, kBlockDim))
	, mL2BlockVersions(cdiv(mL1BlockVersions.dim, kBlockDim))
{
}

std::array<uint16_t, 2> VersionedBlocks2D::getBlockVersionsAt(ivec2 coord) const
{
	return {
		mL1BlockVersions[coord / kBlockDim],
		mL2BlockVersions[coord / kBlockDim / kBlockDim],
	};
}

void VersionedBlocks2D::bumpVersionAfterMultithreadedUpdate() noexcept
{
	++mVersion;
	if ((mVersion & UINT16_MAX) == 0)
	{
		// Roll-over.
		std::ranges::fill(mL1BlockVersions.cells, uint16_t(0));
		std::ranges::fill(mL2BlockVersions.cells, uint16_t(0));
	}
}

std::generator<iaabb2> VersionedBlocks2D::getUserDirtyRects(uint64_t myOldVersion) const
{
	if ((myOldVersion >> 16) != (mVersion >> 16))
		co_yield iaabb2::sized({}, mDim);
	else if (myOldVersion < mVersion)
	{
		const uint16_t oldBlockversion = uint16_t(myOldVersion);
		for (const int l2y : range(mL2BlockVersions.dim.y))
		{
			for (const int l2x : range(mL2BlockVersions.dim.x))
			{
				const ivec2 l2Coord(l2x, l2y);
				if (mL2BlockVersions[l2Coord] > oldBlockversion)
				{
					const ivec2 l2CoordInL1 = l2Coord * kBlockDim;
					const ivec2 l2EndCoordInL1 = vmin(l2CoordInL1 + kBlockDim, mL1BlockVersions.dim);
					for (const int l1y : range(l2CoordInL1.y, l2EndCoordInL1.y))
					{
						for (const int l1x : range(l2CoordInL1.x, l2EndCoordInL1.x))
						{
							const ivec2 l1Coord(l1x, l1y);
							if (mL1BlockVersions[l1Coord] > oldBlockversion)
							{
								const ivec2 l1CoordInPlots = l1Coord * kBlockDim;
								const ivec2 l1EndCoordInPlots = vmin(l1CoordInPlots + kBlockDim, mDim);
								co_yield{ .min = l1CoordInPlots, .max = l1EndCoordInPlots };
							}
						}
					}
				}
			}
		}

		//for (const int l1y : range(mL1BlockVersions.dim.y))
		//{
		//	for (const int l1x : range(mL1BlockVersions.dim.x))
		//	{
		//		const ivec2 l1Coord(l1x, l1y);
		//		if (mL1BlockVersions[l1Coord] > oldBlockversion)
		//		{
		//			const ivec2 l1CoordInPlots = l1Coord * kBlockDim;
		//			const ivec2 l1EndCoordInPlots = vmin(l1CoordInPlots + kBlockDim, mDim);
		//			co_yield{ .min = l1CoordInPlots, .max = l1EndCoordInPlots };
		//		}
		//	}
		//}
	}
	else if (myOldVersion > mVersion)
		std::abort();
}

InvalidationState::InvalidationState(CivMapGeometry mapSizeInfo) : mapSizeInfo(mapSizeInfo), invalidationPlotListBitmap(mapSizeInfo.dim)
{
}
void InvalidationState::invalidateAll()
{
	totallyInvalidated = true;
}
void InvalidationState::invalidateStepRadius(ivec2 coord, int r)
{
	if (!totallyInvalidated)
	{
		for (const int dy : range(-r, r + 1))
		{
			for (const int dx : range(-r, r + 1))
			{
				const ivec2 unwrappedCoord = coord + ivec2(dx, dy);
				if (mapSizeInfo.isValidCoord(unwrappedCoord))
				{
					const ivec2 wrappedCoord = mapSizeInfo.wrapCoord(unwrappedCoord);
					if (!invalidationPlotListBitmap[wrappedCoord])
					{
						invalidationPlotListBitmap[wrappedCoord] = true;
						plotInvalidations.emplace_back(wrappedCoord);
					}
				}
			}
		}

		if (plotInvalidations.size() > static_cast<size_t>(invalidationPlotListBitmap.dim.x) * invalidationPlotListBitmap.dim.y / 3)
			totallyInvalidated = true;

		//localInvalidations.emplace_back((i16vec2)coord, r);
		//mTotalLocalInvalidatedPlots += (r * 2 + 1) * (r * 2 + 1);
		//if (mTotalLocalInvalidatedPlots > invalidationPlotListBitmap.dim.x * invalidationPlotListBitmap.dim.y / 3)
		//	totallyInvalidated = true;
	}
}
void InvalidationState::invalidateSingle(ivec2 coord)
{
	if (!totallyInvalidated && !invalidationPlotListBitmap[coord])
	{
		invalidationPlotListBitmap[coord] = true;
		plotInvalidations.emplace_back(coord);
		//++mTotalLocalInvalidatedPlots;

		if (plotInvalidations.size() > static_cast<size_t>(invalidationPlotListBitmap.dim.x) * invalidationPlotListBitmap.dim.y / 3)
			totallyInvalidated = true;
	}
}

bool InvalidationState::isAllDirty() const
{
	return totallyInvalidated;
}

bool InvalidationState::hasAnyDirty() const
{
	return totallyInvalidated || !plotInvalidations.empty();
}

std::span<const i16vec2> InvalidationState::getDirtyPlotList() const
{
	return plotInvalidations;
}

CivMapGeometry InvalidationState::getMapSizeInfo() const
{
	return mapSizeInfo;
}

void InvalidationState::reset()
{
	totallyInvalidated = false;
	if (plotInvalidations.size())
	{
		plotInvalidations.clear();
		invalidationPlotListBitmap.clearBits(); // Faster to clear individual bits?
		//mTotalLocalInvalidatedPlots = 0;
	}
}

///

InvalidationBitmapState::InvalidationBitmapState(CivMapGeometry mapSizeInfo, int blockDimShift)
	: mMapSizeInfo(mapSizeInfo)
	, mBlockDimShift(blockDimShift)
	, mBlockBitmap(cdiv(ivec2(mapSizeInfo.dim), 1u << blockDimShift))
{
}

void InvalidationBitmapState::reset()
{
	mBlockBitmap.clearBits();
	mHasAnyDirty.store(false, std::memory_order_release);
}

void InvalidationBitmapState::invalidateAll()
{
	mBlockBitmap.setBits();
	mHasAnyDirty.store(true, std::memory_order_release);
}

// These are thread-safe.
void InvalidationBitmapState::invalidateStepRadius(ivec2 coord, int r)
{
	const auto [min, imax] = mMapSizeInfo.getUnwrappedBlockEnumerationRectByStepDist(coord, r, 1 << mBlockDimShift);
	const CivMapGeometry blkGeom(GC.getMapINLINE(), 1 << mBlockDimShift);
	for (const int blkY : range(min.y, imax.y + 1))
		for (const int blkX : range(min.x, imax.x + 1))
			mBlockBitmap.setThreadSafe(blkGeom.wrapCoord({ blkX, blkY }), std::memory_order_relaxed);

	if (!mHasAnyDirty.load(std::memory_order_relaxed))
		mHasAnyDirty.store(true, std::memory_order_relaxed);
}
void InvalidationBitmapState::invalidateSingle(ivec2 coord)
{
	mBlockBitmap.setThreadSafe(coord >> mBlockDimShift, std::memory_order_relaxed);
	if (!mHasAnyDirty.load(std::memory_order_relaxed))
		mHasAnyDirty.store(true, std::memory_order_relaxed);
}

bool InvalidationBitmapState::hasAnyDirty() const
{
	return mHasAnyDirty.load(std::memory_order_acquire);
}

//int InvalidationBitmapState::getNumBlockRows() const
//{
//	return mBlockBitmap.dim.y;
//}
//void InvalidationBitmapState::enumerateDirtyBlocks(int blkRowY, std::function<void(i16aabb2 plotRect)> f) const
//{
//}

void InvalidationBitmapState::getAllDirtyRects(std::vector<i16aabb2>& out) const
{
	const iaabb2 mapRect{ .max = mMapSizeInfo.dim };
	const ivec2 blkDim = ivec2(1) << mBlockDimShift;
	const int blkPitch = mBlockBitmap.dim.x;
	for (const int wordI : range(static_cast<int>(mBlockBitmap.words.size())))
	{
		auto word = mBlockBitmap.words[wordI];
		while (const std::optional<int> bitI = nextBitIndex(word))
		{
			const int blkI = wordI * mBlockBitmap.kBitsPerWord + *bitI;
			const int blkY = blkI / blkPitch;
			const int blkX = blkI % blkPitch;
			out.push_back(mapRect.intersection(iaabb2::sized(ivec2(blkX, blkY) << mBlockDimShift, blkDim)).cast<int16_t>());
		}
	}
}

#endif