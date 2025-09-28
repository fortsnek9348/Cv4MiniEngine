#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "PlotDangerCache.h"

#include "../CvGameCoreUtils.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

AllBorderPlotDangerSourceCache::AllBorderPlotDangerSourceCache()
	: mBitmap(CivMapGeometry(GC.getMapINLINE()).dim)
	, mBlockVersioning(mBitmap.dim)
	, mInvalidationState(CivMapGeometry(GC.getMapINLINE()))
{
}

void AllBorderPlotDangerSourceCache::onPlotOwnerChanged(const CvPlot& plot)
{
	mInvalidationState.invalidateStepRadius(getPlotCoord(plot), 2);
}
void AllBorderPlotDangerSourceCache::onPlotRouteTypeChanged(const CvPlot& plot)
{
	// Outer radius may have been cleared.
	mInvalidationState.invalidateStepRadius(getPlotCoord(plot), 2);
}
void AllBorderPlotDangerSourceCache::onPlotIsCityChanged(const CvPlot& plot)
{
	// Only invalidates this single plot.
	mInvalidationState.invalidateSingle(getPlotCoord(plot));
}

void AllBorderPlotDangerSourceCache::update()
{
	CvMap& map = GC.getMapINLINE();

	std::atomic_bool changed{};

	auto doPlot = [&map, this](ivec2 coord) {
		const std::bitset<MAX_TEAMS> dangerBits = calcDangerBits(map, coord);

		if (dangerBits != mBitmap[coord])
		{
			mBitmap[coord] = dangerBits;
			mBlockVersioning.updateMultithreaded(coord);
			return true;
		}
		else
			return false;
		};

	if (mInvalidationState.isAllDirty())
	{
		const ivec2 dim = mInvalidationState.getMapSizeInfo().dim;
		heck::parallelForEachN(dim.y, [&doPlot, &changed, dim](int y) {
		//heck::serialForEachN(mInvalidationState.getMapSizeInfo().dim.y, [&](int y) {
			bool rowChanged = false;
			for (const int x : range(dim.x))
				rowChanged |= doPlot({ x, y });
			if (rowChanged)
				changed.store(true, std::memory_order_relaxed);
			});
	}
	else
	{
		const auto& dirtyPlotList = mInvalidationState.getDirtyPlotList();
		std::for_each(heck::kDefaultParallelExecution, dirtyPlotList.begin(), dirtyPlotList.end(), [&](ivec2 coord) {
			if (doPlot(coord))
				changed.store(true, std::memory_order_relaxed);
			});
	}

	mInvalidationState.reset();

	if (changed.load(std::memory_order_relaxed))
		mBlockVersioning.bumpVersionAfterMultithreadedUpdate();
}

void AllBorderPlotDangerSourceCache::verify(ivec2 coord) const
{
	const std::bitset<MAX_TEAMS> dangerBits = calcDangerBits(GC.getMapINLINE(), coord);

	if (dangerBits != mBitmap[coord])
	{
		std::clog << "Verifying AllBorderPlotDangerSourceCache at " << coord << std::endl;
		std::clog << "Calculated dangerBits = " << dangerBits << std::endl;
		std::clog << "Cached     dangerBits = " << mBitmap[coord] << std::endl;
		std::abort();
	}
}

const VersionedBlocks2D& AllBorderPlotDangerSourceCache::getBlockVersioning() const
{
	return mBlockVersioning;
}

std::bitset<MAX_TEAMS> AllBorderPlotDangerSourceCache::calcDangerBits(CvMap& map, ivec2 coord) const
{
	std::bitset<MAX_TEAMS> dangerBits{};
	const CvArea* const area = map.plotINLINE(coord.x, coord.y)->area();
	if (!map.plot(coord.x, coord.y)->isCity())
	{
		for (const int dy : range(-2, 2+1))
		{
			for (const int dx : range(-2, 2+1))
			{
				const int d = stepDistance(0, 0, dx, dy);
				if (const CvPlot* const plot = map.plotINLINE(coord.x + dx, coord.y + dy))
					if (plot->getTeam() != NO_TEAM && plot->area() == area)
						if (d == 1 || (d == 2 && plot->isRoute()))
							dangerBits[plot->getTeam()] = true;
			}
		}
	}
	return dangerBits;
}

#endif