#include "inc/CommonStuff/TradeNetworkSystem.h"
#include "inc/CommonStuff/StatisticsRegistry.h"
#include "inc/CommonStuff/Hashing.h"
#include "inc/CommonStuff/ParallelExt.h"
#include "inc/CommonStuff/UnionFind.h"
#include "inc/CommonStuff/FunctionOutputIterator.h"

#include <fstream>
#include <format>

using namespace heck;

static constexpr int kBlockDim = 16;

// Enable this when testing in case local block updates are buggy.
static constexpr bool kAlwaysDoGeneralBlockRebuilds = false;

// NOTE: Thought that CvPlot::isTradeNetworkConnected was symmetric, but it's not. Updated code below to remove that assumption. Watch out for lingering bugs.

static auto& gStat_TotalUpdatedBlocks = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem TotalUpdatedBlocks");
static auto& gStat_NumTrivialBlockUpdates = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumTrivialBlockUpdates");
static auto& gStat_NumTrivialBlockUpdatesWithoutBorderChanges = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumTrivialBlockUpdatesWithoutBorderChanges");
static auto& gStat_NumGeneralBlockUpdates = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumGeneralBlockUpdates");
static auto& gStat_NumCPGsLocallyUpdated = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumCPGsLocallyUpdated");
static auto& gStat_NumCPGsGloballyCreated = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumCPGsGloballyCreated");
static auto& gStat_NumCPGsGloballyUpdated = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumCPGsGloballyUpdated");
static auto& gStat_NumCPGsGloballyDeleted = StatisticsRegistry::getInstance().grab<CountingStatistic>("TradeNetworkSystem NumCPGsGloballyDeleted");

TradeNetworkSystem::TradeNetworkSystem(MapGeometry mapGeom, size_t numBonuses, const IDataSource& dataSource)
	: mMapGeom(mapGeom)
	, mMapBlocksGeom(cdiv<int, unsigned int>(mMapGeom.dim, kBlockDim), mMapGeom.wrapX, mMapGeom.wrapY)
	, mIsTradeNetworkCache(mMapGeom.dim)
	, mIsTradeNetworkConnectedCache(mMapGeom.dim)
	, mCore(mMapGeom.dim, numBonuses)
	, mBlocks(mMapBlocksGeom.dim)
{
	updateWholeBitmap(nullptr, dataSource);

	// Setup bonuses in the core.
	for (int y = 0; y < mMapGeom.dim.y; ++y)
		for (int x = 0, w = mMapGeom.dim.x; x < w; ++x)
			onUpdateBonusesAtPlot({ x, y }, dataSource);

	std::vector<BlockUpdateEntry> blkList;

	// Make new blocks.
	for (const int blkY : range(mBlocks.dim.y))
		for (const int blkX : range(mBlocks.dim.x))
		{
			(void)rebuildBlock({ blkX, blkY });
			blkList.emplace_back(ivec2(blkX, blkY), true);
		}

	// Build connectivity info.
	for (const int blkY : range(mBlocks.dim.y))
		for (const int blkX : range(mBlocks.dim.x))
			(void)updateInterBlockConnections({ blkX, blkY });
	
	updateCPGs(std::move(blkList));
}

void TradeNetworkSystem::onUpdateAllPlotGroups(const IDataSource& dataSource)
{
	update(std::nullopt, dataSource);
}

void TradeNetworkSystem::onUpdatePlotGroupsForPlot(ivec2 coord, const IDataSource& dataSource)
{
	update(coord, dataSource);
}

void TradeNetworkSystem::onUpdateBonusesAtPlot(ivec2 coord, const IDataSource& dataSource)
{
	// void CvPlot::updatePlotGroupBonus(bool bAdd)
	mCore.setPlotBonus(coord, dataSource.getNetworkPlotBonus(coord));
	mCore.setCityExtraBonuses(coord, dataSource.getNetworkCityExtraBonuses(coord));
}

void TradeNetworkSystem::newCity(ivec2 coord)
{
	mCore.newCity(coord);
}

std::span<const TradeNetworkCore::BonusCount> TradeNetworkSystem::getCityNetworkBonuses(ivec2 coord) const
{
	return mCore.getCityNetworkBonuses(coord);
}

void TradeNetworkSystem::destroyCity(ivec2 coord)
{
	mCore.destroyCity(coord);
}

std::generator<TradeNetworkCore::CityUpdate> TradeNetworkSystem::flushCityUpdates()
{
	return mCore.flushCityUpdates();
}

// !!a->getPlotGroup()
bool TradeNetworkSystem::isInPlotGroup(ivec2 coord) const
{
	assert(mIsTradeNetworkCache[coord] == (mCore.getCPGAt(coord) != kNoCPG));
	return mIsTradeNetworkCache[coord];
}
// a->getPlotGroup() == b->getPlotGroup()
bool TradeNetworkSystem::isSamePlotGroup(ivec2 a, ivec2 b) const
{
	const ECivPlotGroup cpgA = mCore.getCPGAt(a);
	const ECivPlotGroup cpgB = mCore.getCPGAt(b);
	return cpgA == cpgB;// && cpgA != kNoCPG;
}
// a->getPlotGroup() ? a->getPlotGroup()->getNumBonuses(bonusI) : 0
int TradeNetworkSystem::getPlotGroupNumBonuses(ivec2 a, EBonus bonusI) const
{
	if (bonusI == TradeNetworkCore::kNoBonus)
		return 0;
	else
		return mCore.getNetworkBonusCountAtPlot(a)[std::to_underlying(bonusI)];
}
// a->getPlotGroup() && a->getPlotGroup()->hasBonus(bonusI)
int TradeNetworkSystem::getPlotGroupHasBonus(ivec2 a, EBonus bonusI) const
{
	return getPlotGroupNumBonuses(a, bonusI) > 0;
}

size_t TradeNetworkSystem::getNumActiveCPGs() const
{
	return mCore.getNumActiveCPGs();
}
size_t TradeNetworkSystem::getNumAllocatedCPGs() const
{
	return mCore.getNumAllocatedCPGs();
}

bool TradeNetworkSystem::isFreeCpgIndex(TradeNetworkCore::ECivPlotGroup cpgI) const
{
	return mCore.getNumBPGsInCPG(cpgI) <= 0;
}

std::vector<i16vec2> TradeNetworkSystem::getPlotsOfCPG(TradeNetworkCore::ECivPlotGroup cpgI) const
{
	return mCore.getPlotsOf(cpgI);
}

std::span<const int> TradeNetworkSystem::getCPGBonusCounts(TradeNetworkCore::ECivPlotGroup cpgI) const
{
	return mCore.getCPGBonusCounts(cpgI);
}

TradeNetworkCore::EBlockPlotGroup TradeNetworkSystem::getBPGAt(ivec2 coord) const
{
	return mCore.getBPGAt(coord);
}
TradeNetworkCore::ECivPlotGroup TradeNetworkSystem::getCPGAt(ivec2 coord) const
{
	return mCore.getCPGAt(coord);
}

void TradeNetworkSystem::dump(const char* filename) const
{
	std::ofstream dump{ std::string(filename) };

	dump << R"(<!DOCTYPE html>
<html>
<style>
.ValidCell { fill:green;stroke:none;opacity:0.2; }
.ValidAdj { fill:blue;stroke:none; }
.N { fill:none;stroke:none; }
.City { fill:none;stroke:lightgreen;stroke-width: 0.1px; }
svg text{
  text-anchor: middle;
  dominant-baseline: central;
  font-size: 0.3px;
}
</style>
<body>
)";
	const float kCellSize = 40;

	const auto drawRect = [&](vec2<float> position, vec2<float> size, std::string_view className) {
		//dump << "<rect x='" << position.x << "' y='" << position.y << "' width='" << size.x << "' height='" << size.y << "' class='" << className << "' />";
		const auto p0 = position;
		const auto p1 = position + vec2<float>(size.x, 0);
		const auto p2 = position + size;
		const auto p3 = position + vec2<float>(0, size.y);
		dump << "<polygon points='"
			<< p0.x << ',' << p0.y << ' '
			<< p1.x << ',' << p1.y << ' '
			<< p2.x << ',' << p2.y << ' '
			<< p3.x << ',' << p3.y << "' class='" << className << "'/>";
		};

	const ivec2 dim = mMapGeom.dim;

	dump << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " << dim.x << ' ' << dim.y
		<< "' width='" << dim.x << "' height='" << dim.y
		<< "' transform='scale(" << kCellSize << ")' transform-origin='0 0'>\n";
	dump << std::format(R"svg(
	<defs>
    <pattern id="smallGrid" width="1" height="1" patternUnits="userSpaceOnUse">
     <rect width="1" height="1" style="stroke-width:0.1;stroke:black;fill:none;">
    </pattern>
    <pattern id="grid" width="{}" height="{}" patternUnits="userSpaceOnUse">
      <rect width="{}" height="{}" fill="url(#smallGrid)" />
      <rect width="{}" height="{}" style="stroke-width:0.2;stroke:black;fill:none;">
    </pattern>
  </defs>
		)svg", kBlockDim, kBlockDim, kBlockDim, kBlockDim, kBlockDim, kBlockDim);

	dump << "<rect width='100%' height='100%' fill='url(#grid)' />\n";

	for (int y = 0; y < dim.y; ++y)
	{
		for (int x = 0; x < dim.x; ++x)
		{
			const ivec2 coord(x, y);
			//if (map.plots[coord].isTradeNetwork)
			if (mIsTradeNetworkCache[coord])
				drawRect(coord, 1, "ValidCell");

			//if (map.cities.contains(coord))
			if (mCore.hasCityAt(coord))
				drawRect(coord, 1, "City");

			//const ivec2 blkCoord = coord / kBlockDim;
			//const ivec2 localCoord = coord - blkCoord * kBlockDim;

			for (size_t i = 0; i < kNumAdj; ++i)
			{
				const std::optional<ivec2> toCoord = mMapGeom.tryCanonicalise(coord + kAdj[i]);
				//if (!map.plotValid[coord] || !toCoord || !map.plotValid[*toCoord] || !((map.plotAdjValid[coord] >> i) & 1))
				//if (map.plots[coord].isTradeNetwork && (!toCoord || !map.plots[*toCoord].isTradeNetwork || !map.isTradeNetworkConnected(coord, *toCoord, i)))
				if (mIsTradeNetworkCache[coord] && (!toCoord || !mIsTradeNetworkCache[*toCoord] || !((mIsTradeNetworkConnectedCache[coord] >> i) & 1)))
				{
					const ivec2 adj = kAdj[i];
					const float centerX = (adj.x + 1) / 2.0f;
					const float centerY = (adj.y + 1) / 2.0f;
					const float sizeX = 0.4f;
					const float sizeY = 0.4f;
					const float positionX = centerX - sizeX / 2;
					const float positionY = centerY - sizeY / 2;
					const aabb2<float> adjRect = aabb2<float>::sized({ positionX, positionY }, { sizeX, sizeY }).intersection(aabb2<float>::sized({}, 1));


					drawRect(vec2<float>(coord) + adjRect.min, adjRect.size(), "ValidAdj");
				}
			}

			const TradeNetworkCore::EBlockPlotGroup bpgI = mCore.getBPGAt(coord);
			const TradeNetworkCore::ECivPlotGroup cpgI = mCore.getCPGAt(coord);
			if (bpgI != TradeNetworkCore::kNoBPG || cpgI != TradeNetworkCore::kNoCPG)
				dump << std::format("<text x='{}' y='{}'><title>{}, {}</title>c{}b{}</text>", coord.x + 0.5f, coord.y + 0.5f, x, y, std::to_underlying(cpgI), std::to_underlying(bpgI));

			//dump << "<rect x='" << x << "' y='" << y << "' width='1' height='1' alt='XXX'><title>" << x << ", " << y << "</title></rect>\n";
		}
		dump << '\n';
	}



	dump << "</svg>\n";

	dump << R"(</body>
</html>
)";
}

void TradeNetworkSystem::verify(const IDataSource& dataSource) const
{
	// Verify bitmap.
	const MapGeometry mapGeom = mMapGeom;
	for (int y = 0; y < mapGeom.dim.y; ++y)
	{
		for (int x = 0; x < mapGeom.dim.x; ++x)
		{
			const ivec2 coord(x, y);
			if (mIsTradeNetworkCache[coord] != dataSource.isTradeNetwork(coord))
				std::abort();
		}
	}

	for (int y = 0; y < mapGeom.dim.y; ++y)
	{
		for (int x = 0; x < mapGeom.dim.x; ++x)
		{
			const ivec2 coord(x, y);

			for (int i = 0; i < (int)kNumAdj; ++i)
			{
				const bool cachedBit = ((mIsTradeNetworkConnectedCache[coord] >> i) & 1) != 0;
				if (const auto optToCoord = mapGeom.tryCanonicalise(coord + kAdj[i]))
				{
					const bool liveBit = mIsTradeNetworkCache[coord] && mIsTradeNetworkCache[*optToCoord] && dataSource.isTradeNetworkConnected(coord, *optToCoord, i);
					if (cachedBit != liveBit)
						std::abort();
				}
				else if (cachedBit)
					std::abort();
			}
		}
	}

	// Verify bonus counts with the current plot linking.
	mCore.verify(
		[&](ivec2 coord) { return dataSource.getNetworkPlotBonus(coord); },
		[&](ivec2 coord) { return dataSource.getNetworkCityExtraBonuses(coord); }
		);

	// Verify plot linking by checking that plots that should be linked are linked.
	for (int y = 0; y < mapGeom.dim.y; ++y)
		for (int x = 0; x < mapGeom.dim.x; ++x)
			for (int i = 0; i < (int)kNumAdj; ++i)
				if (isTradeNetworkAndIsConnected({ x, y }, i))
					if (getCPGAt({ x, y }) != getCPGAt(*mapGeom.tryCanonicalise(ivec2(x, y) + kAdj[i])))
						std::abort();

	// Verify plot linking by using global union-find across all plots.
	DynamicArray2D<i16vec2> globalUFStorage(mapGeom.dim);
	for (int y = 0; y < mapGeom.dim.y; ++y)
		for (int x = 0; x < mapGeom.dim.x; ++x)
			globalUFStorage[ivec2(x, y)] = ivec2(x, y);

	ArrayUnionFind globalUF(globalUFStorage);
	for (int y = 0; y < mapGeom.dim.y; ++y)
		for (int x = 0; x < mapGeom.dim.x; ++x)
			for (int i = 0; i < (int)kNumAdj; ++i)
				if (isTradeNetworkAndIsConnected({ x, y }, i))
					globalUF.unionise(ivec2(x, y), *mapGeom.tryCanonicalise(ivec2(x, y) + kAdj[i]));

	std::unordered_map<i16vec2, std::vector<i16vec2>, VectorHasher> components;
	for (int y = 0; y < mapGeom.dim.y; ++y)
		for (int x = 0; x < mapGeom.dim.x; ++x)
			if (mIsTradeNetworkCache[{ x, y }])
				components[globalUF.find(ivec2(x, y))].push_back(ivec2(x, y));

	std::vector<std::vector<i16vec2>> cpgPlotLists;
	for (size_t cpgI = 0; cpgI < mCore.getNumAllocatedCPGs(); ++cpgI)
		if (mCore.getNumBPGsInCPG(static_cast<ECivPlotGroup>(cpgI)) > 0)
		{
			cpgPlotLists.push_back(mCore.getPlotsOf(static_cast<ECivPlotGroup>(cpgI)));
			std::ranges::sort(cpgPlotLists.back(), VectorComparator());
		}

	std::vector<std::vector<i16vec2>> componentPlotLists(std::from_range, components | std::views::values);
	for (auto& list : componentPlotLists)
		std::ranges::sort(list, VectorComparator());

	std::ranges::sort(cpgPlotLists, VectorComparator(), [](const std::vector<i16vec2>& list) { return list.front(); });
	std::ranges::sort(componentPlotLists, VectorComparator(), [](const std::vector<i16vec2>& list) { return list.front(); });

	if (cpgPlotLists != componentPlotLists)
		std::abort();

}

/// private

void TradeNetworkSystem::update(std::optional<ivec2> invalidatedMapCoord, const IDataSource& dataSource)
{
	const std::vector<BlockUpdateEntry> blkUpdateList = updateBlocks(invalidatedMapCoord, dataSource);
	updateBlockConnections(blkUpdateList);
	updateCPGs(blkUpdateList);
}

std::vector<TradeNetworkSystem::BlockUpdateEntry> TradeNetworkSystem::updateBlocks(std::optional<ivec2> invalidatedMapCoord, const IDataSource& dataSource)
{
	std::vector<BlockUpdateEntry> blkUpdateList;

	const auto doBlock = [&blkUpdateList, invalidatedMapCoord, this](ivec2 blkCoord) {
		const EBlockUpdateStatus status = updateBlock(blkCoord, invalidatedMapCoord);
		// NOTE: Even if block is unchanged, borders may be invalidated because plot connections at the border don't affect blocks.
		const bool bordersDirty = status == EBlockUpdateStatus::General || !invalidatedMapCoord || isOnBlockBorder(*invalidatedMapCoord);
		blkUpdateList.emplace_back(blkCoord, bordersDirty);
		gStat_TotalUpdatedBlocks.inc();
		};

	// Find out which blocks changed, and whether their connections may have changed.
	if (invalidatedMapCoord)
	{
		const bool plotChanged = updatePlotBits(*invalidatedMapCoord, dataSource);
		const bool adjChanged = updateAdjPlotBits(*invalidatedMapCoord, dataSource);
		if (plotChanged || adjChanged)
			doBlock(*invalidatedMapCoord / kBlockDim);
		// else, no change
	}
	else
	{
		std::mutex mutex;
		std::vector<ivec2> changedBlkCoords;

		// Whole-map update is the worst case, and updatePlotBits takes ~90% of TradeNetworkSystem time.
		const ivec2 dimBlks = mMapBlocksGeom.dim;
		heck::parallelForEachN(dimBlks.x * dimBlks.y, [&](int blkI) {
			const int blkY = blkI / dimBlks.x;
			const int blkX = blkI % dimBlks.x;

			const ivec2 blkCoord{ blkX, blkY };
			const iaabb2 blkRect = iaabb2::sized(blkCoord * kBlockDim, getBlockDim(blkCoord));

			bool changed = false;
			for (int plotY = blkRect.min.y; plotY < blkRect.max.y; ++plotY)
				for (int plotX = blkRect.min.x; plotX < blkRect.max.x; ++plotX)
					changed |= updatePlotBits({ plotX, plotY }, dataSource);

			if (changed)
				(void)std::lock_guard(mutex), changedBlkCoords.push_back(blkCoord);

			});

		// Sort for determinism and set ops.
		std::ranges::sort(changedBlkCoords, VectorComparator());

		for (const ivec2 blkCoord : changedBlkCoords)
			doBlock(blkCoord);
	}

	

	return blkUpdateList;
}

void TradeNetworkSystem::updateBlockConnections(std::span<const BlockUpdateEntry> blkUpdateList)
{
	struct AdjUpdateInfo
	{
		ivec2 blkCoord{};
		ivec2 adjBlkCoord{};
		int adjI{};

		bool operator==(const AdjUpdateInfo&) const = default;
		bool operator<(const AdjUpdateInfo& b) const
		{
			if (blkCoord != b.blkCoord)
				return VectorComparator()(blkCoord, b.blkCoord);
			if (adjBlkCoord != b.adjBlkCoord)
				return VectorComparator()(adjBlkCoord, b.adjBlkCoord);
			return adjI < b.adjI;
		}
	};
	std::vector<AdjUpdateInfo> adjUpdateList;

	for (const BlockUpdateEntry& blkEntry : blkUpdateList)
	{
		if (blkEntry.bordersDirty)
		{
			delBlockAdj(blkEntry.blkCoord);

			for (size_t adjI = 0; adjI < kNumAdj; ++adjI)
			{
				if (auto adjBlkCoord = mMapBlocksGeom.tryCanonicalise(blkEntry.blkCoord + kAdj[adjI]))
				{
					const ivec2 blkCoord = blkEntry.blkCoord;
					//if (adjI >= kNumAdj / 2)
					//{
					//	std::swap(*adjBlkCoord, blkCoord);
					//	adjI = kInvAdjIndices[adjI];
					//}

					adjUpdateList.emplace_back(blkCoord, *adjBlkCoord, static_cast<int>(adjI));
					adjUpdateList.emplace_back(*adjBlkCoord, blkCoord, kInvAdjIndices[adjI]);
				}
			}
		}
	}

	std::ranges::sort(adjUpdateList, std::less<>());
	adjUpdateList.erase(std::ranges::unique(adjUpdateList).begin(), adjUpdateList.end());

	for (const AdjUpdateInfo& adjUpdateInfo : adjUpdateList)
		(void)updateInterBlockConnections(adjUpdateInfo.blkCoord, adjUpdateInfo.adjI);
}

void TradeNetworkSystem::updateCPGs(std::span<const BlockUpdateEntry> blkUpdateList)
{
	std::vector<ivec2> localBlkList;
	for (const BlockUpdateEntry& blkEntry : blkUpdateList)
	{
		localBlkList.push_back(blkEntry.blkCoord);
		for (const ivec2 adjD : kAdj)
			if (const auto optAdjBlkCoord = mMapBlocksGeom.tryCanonicalise(blkEntry.blkCoord + adjD))
				localBlkList.push_back(*optAdjBlkCoord);
	}

	std::ranges::sort(localBlkList, VectorComparator());
	localBlkList.erase(std::ranges::unique(localBlkList).begin(), localBlkList.end());

	// Rings: All local blocks except the updated blocks.
	std::vector<ivec2> ringBlkList;
	std::ranges::set_difference(localBlkList, blkUpdateList | std::views::transform(&BlockUpdateEntry::blkCoord), std::back_inserter(ringBlkList), VectorComparator());

	// Find all potentially affected CPGs.
	std::unordered_set<ECivPlotGroup> cpgIndices;
	for (const ivec2 blkCoord : localBlkList)
		for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
			if (const ECivPlotGroup cpgI = mCore.getCPGOf(bpgI); cpgI != kNoCPG)
					cpgIndices.insert(cpgI);

	doLocalCPGUpdates(localBlkList, ringBlkList, std::ref(cpgIndices));
	doGlobalCPGUpdates(localBlkList, cpgIndices);
}

void TradeNetworkSystem::updateWholeBitmap(std::vector<ivec2>* dirtyBlkCoordsOut, const IDataSource& dataSource)
{
	// Serial. We'll probably do players in parallel.
	serialForEachN(mBlocks.dim.y, [&dataSource, dirtyBlkCoordsOut, mapGeom = mMapGeom, this](int blkY) {
		for (int blkX = 0; blkX < mBlocks.dim.x; ++blkX)
		{
			const ivec2 blkCoord{ blkX, blkY };
			const iaabb2 blkRect = iaabb2::sized(blkCoord * kBlockDim, getBlockDim(blkCoord));
			for (int y = blkRect.min.y; y < blkRect.max.y; ++y)
			{
				for (int x = blkRect.min.x; x < blkRect.max.x; ++x)
				{
					const ivec2 coord(x, y);
					uint8_t bits = 0;

					const bool plotIsTradeNetwork = dataSource.isTradeNetwork(coord);

					if (plotIsTradeNetwork)
						for (int i = 0; i < (int)kNumAdj; ++i)
							if (const auto optToCoord = mapGeom.tryCanonicalise(coord + kAdj[i]))
								bits |= (dataSource.isTradeNetwork(*optToCoord) && dataSource.isTradeNetworkConnected(coord, *optToCoord, i)) << i;

					bool blkDirty = false;
					blkDirty = blkDirty || mIsTradeNetworkCache[coord] != plotIsTradeNetwork;
					blkDirty = blkDirty || mIsTradeNetworkConnectedCache[coord] != bits; // This may not actually change the block if the changed adj info is only on block border.
					if (dirtyBlkCoordsOut && blkDirty)
						dirtyBlkCoordsOut->push_back(blkCoord);

					mIsTradeNetworkCache[coord] = plotIsTradeNetwork;
					mIsTradeNetworkConnectedCache[coord] = bits;
				}
			}
		}
		});
}

bool TradeNetworkSystem::updatePlotBits(ivec2 mapCoord, const IDataSource& dataSource)
{
	const MapGeometry mapGeom = mMapGeom;

	uint8_t bits = 0;

	const bool plotIsTradeNetwork = dataSource.isTradeNetwork(mapCoord);

	bool changed = false;

	if (plotIsTradeNetwork)
		for (int i = 0; i < (int)kNumAdj; ++i)
			if (const auto optToCoord = mapGeom.tryCanonicalise(mapCoord + kAdj[i]))
				bits |= (dataSource.isTradeNetwork(*optToCoord) && dataSource.isTradeNetworkConnected(mapCoord, *optToCoord, i)) << i;

	// All thread-safe.

	changed |= mIsTradeNetworkCache.getThreadSafe(mapCoord, std::memory_order_relaxed) != plotIsTradeNetwork;
	mIsTradeNetworkCache.setThreadSafe(mapCoord, plotIsTradeNetwork, std::memory_order_relaxed);

	std::atomic_ref isTradeNetworkConnectedCache_atomicRef = mIsTradeNetworkConnectedCache.atomicRef(mapCoord);
	if (isTradeNetworkConnectedCache_atomicRef.load(std::memory_order_relaxed) != bits)
	{
		isTradeNetworkConnectedCache_atomicRef.store(bits, std::memory_order_relaxed);
		changed = true;
	}

	return changed;
}

bool TradeNetworkSystem::updateAdjPlotBits(ivec2 mapCoord, const IDataSource& dataSource)
{
	const MapGeometry mapGeom = mMapGeom;
	const bool plotIsTradeNetwork = dataSource.isTradeNetwork(mapCoord);

	bool changed = false;

	// Update adj bits from adj plots, for non-symmetric connections.
	for (int i = 0; i < (int)kNumAdj; ++i)
	{
		if (const auto optToCoord = mapGeom.tryCanonicalise(mapCoord + kAdj[i]))
		{
			const int invAdjI = heck::kInvAdjIndices[i];
			const bool value = plotIsTradeNetwork && mIsTradeNetworkCache[*optToCoord] && dataSource.isTradeNetworkConnected(*optToCoord, mapCoord, invAdjI);
			changed |= value != bool((mIsTradeNetworkConnectedCache[*optToCoord] >> invAdjI) & 1);
			mIsTradeNetworkConnectedCache[*optToCoord] &= ~(1 << invAdjI);
			mIsTradeNetworkConnectedCache[*optToCoord] |= value << invAdjI;
		}
	}

	// If connections were symmetric...
	//if (mIsTradeNetworkConnectedCache[mapCoord] != bits)
	//{
	//	mIsTradeNetworkConnectedCache[mapCoord] = bits;
	//	changed = true;
	//
	//	// Adj changed. Need to update adj from adj plots.
	//	for (int i = 0; i < kNumAdj; ++i)
	//	{
	//		if (const auto optToCoord = mapGeom.tryCanonicalise(mapCoord + kAdj[i]))
	//		{
	//			uint8_t& bitsFromAdj = mIsTradeNetworkConnectedCache[*optToCoord];
	//			bitsFromAdj &= ~(1 << kInvAdjIndices[i]);
	//			bitsFromAdj |= bool(bits & (1 << i)) << kInvAdjIndices[i];
	//		}
	//	}
	//}

	return changed;
}

bool TradeNetworkSystem::isTradeNetworkAndIsConnected(ivec2 mapCoord, int adjI) const
{
	return ((mIsTradeNetworkConnectedCache[mapCoord] >> adjI) & 1) != 0;
}

ivec2 TradeNetworkSystem::getBlockDim(ivec2 blkCoord) const
{
	return vmin((blkCoord + 1) * kBlockDim, mMapGeom.dim) - blkCoord * kBlockDim;
}

bool TradeNetworkSystem::isOnBlockBorder(ivec2 mapCoord) const
{
	const ivec2 blkCoord = mapCoord / kBlockDim;
	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 blkDim = getBlockDim(blkCoord);
	return !iaabb2::sized(blkOrigin, blkDim).shrunk(1).contains(mapCoord);
}

TradeNetworkSystem::EBlockUpdateStatus TradeNetworkSystem::updateBlock(ivec2 blkCoord, std::optional<ivec2> optMapCoord)
{
	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 blkDim = getBlockDim(blkCoord);
	const iaabb2 localRect{ .max = blkDim };

	// Try trivial update.
	if constexpr (!kAlwaysDoGeneralBlockRebuilds)
		if (optMapCoord)
			if (const ivec2 localCoord = *optMapCoord - blkOrigin; localRect.contains(localCoord))
				if (const EBlockUpdateStatus status = tryBlockTrivialUpdate(blkCoord, localCoord); status != EBlockUpdateStatus::Failed)
				{
					gStat_NumTrivialBlockUpdates.inc();
					if (!isOnBlockBorder(*optMapCoord))
						gStat_NumTrivialBlockUpdatesWithoutBorderChanges.inc();
					return status;
				}

	// Do general update.
	return rebuildBlock(blkCoord);
}

TradeNetworkSystem::EBlockUpdateStatus TradeNetworkSystem::rebuildBlock(ivec2 blkCoord)
{
	Block& block = mBlocks[blkCoord];

	delBlockAdj(blkCoord);

	// Delete all BPGs from block.
	const std::vector<EBlockPlotGroup> bpgs = std::move(block.bpgs); // `deleteBPG` will remove from block.bpgs.
	for (const EBlockPlotGroup bpgI : bpgs)
		deleteBPG(blkCoord, bpgI);

	block = buildBlock(blkCoord);

	gStat_NumGeneralBlockUpdates.inc();

	return EBlockUpdateStatus::General;
}

TradeNetworkSystem::Block TradeNetworkSystem::buildBlock(ivec2 blkCoord)
{
	// General block rebuild.

	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 blkDim = getBlockDim(blkCoord);
	const iaabb2 localRect{ .max = blkDim };

	// Perform union-find across all plots in this block.
	Array2D<u8vec2, kBlockDim> ufGrid{};
	for (uint8_t y = 0; y < kBlockDim; ++y)
		for (uint8_t x = 0; x < kBlockDim; ++x)
			ufGrid[{ x, y }] = { x, y };

	ArrayUnionFind uf(ufGrid);

	for (int y = 0; y < blkDim.y; ++y)
	{
		for (int x = 0; x < blkDim.x; ++x)
		{
			const ivec2 fromLocalCoord(x, y);
			const ivec2 fromMapCoord = blkOrigin + fromLocalCoord;
			if (mIsTradeNetworkCache[fromMapCoord])
			{
				for (int i = 0; i < (int)kNumAdj; ++i)
				{
					if (isTradeNetworkAndIsConnected(fromMapCoord, i))
					{
						const ivec2 toLocalCoord = fromLocalCoord + kAdj[i];
						if (localRect.contains(toLocalCoord))
							uf.unionise(fromLocalCoord, toLocalCoord);
					}
				}
			}
		}
	}

	Block block;

	// 1KB
	Array2D<EBlockPlotGroup, kBlockDim> bpgGrid(kNoBPG);

	// Build BPGs.
	for (int y = 0; y < blkDim.y; ++y)
	{
		for (int x = 0; x < blkDim.x; ++x)
		{
			const ivec2 localCoord(x, y);
			const ivec2 mapCoord = blkOrigin + localCoord;

			// Check that BPGs were removed from core.
			assert(mCore.getBPGAt(mapCoord) == kNoBPG);

			if (mIsTradeNetworkCache[mapCoord])
			{
				EBlockPlotGroup& bpgI = bpgGrid[uf.find(localCoord)];
				
				if (bpgI == kNoBPG)
				{
					// Create a new BPG.
					bpgI = mCore.createBPG(blkOrigin);
					block.bpgs.push_back(bpgI);
				}

				mCore.setPlotBPG(mapCoord, bpgI);
			}
		}
	}

	return block;
}

TradeNetworkSystem::EBlockUpdateStatus TradeNetworkSystem::tryBlockTrivialUpdate(ivec2 blkCoord, ivec2 localCoord)
{
	// A trivial block update can be split into: Trivial remove and trivial add to BPG.

	// Basically, if we're part of a BPG, then if we're no longer connected to it (and the BPG remains connected), then remove from the BPG.
	// Then, determine if we should connect to any adj BPG (whether or not we're already in a BPG).
	// "Adding to a BPG" also includes making a new BPG for ourselves.

	// There are other cases.
	// Case: Nothing to do. If part of a BPG, removal won't remove, and add won't find another BPG to add to.
	// Case: Isolated. If part of a BPG, removal may remove the single-plot BPG. And if not part of a BPG, add may create a new single-plot BPG.

	//const EBlockUpdateStatus removalStatus = tryBlockTrivialUpdateCaseRemoveFromBPG(blkCoord, localCoord);
	//if (removalStatus == EBlockUpdateStatus::Failed)
	//	return EBlockUpdateStatus::Failed;
	//
	//const EBlockUpdateStatus addStatus = tryBlockTrivialUpdateCaseAddToBPG(blkCoord, localCoord);
	//if (addStatus == EBlockUpdateStatus::Failed)
	//	return EBlockUpdateStatus::Failed;
	//
	//return removalStatus == EBlockUpdateStatus::NoChange && addStatus == EBlockUpdateStatus::NoChange
	//	? EBlockUpdateStatus::NoChange
	//	: EBlockUpdateStatus::Trivial;

	// Do local UF and match up the components with existing BPGs.
	// Trivial update is possible iff all BPGs in ring match.

	const ivec2 blkDim = getBlockDim(blkCoord);
	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 mapCoord = blkOrigin + localCoord;
	const iaabb2 blkLocalRect{ .max = blkDim };

	Array2D<ivec2, 3> localGrid{};
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			localGrid[{ x, y }] = { x, y };

	ArrayUnionFind localUF(localGrid);

	const ivec2 adjLocalOrigin = localCoord - 1;
	const iaabb2 adjLocalRect = iaabb2::sized(0, 3).intersection(blkLocalRect - adjLocalOrigin);

	for (int y = adjLocalRect.min.y; y < adjLocalRect.max.y; ++y)
	{
		for (int x = adjLocalRect.min.x; x < adjLocalRect.max.x; ++x)
		{
			const ivec2 fromAdjLocalCoord = ivec2(x, y);
			const ivec2 fromLocalCoord = adjLocalOrigin + fromAdjLocalCoord;
			const ivec2 fromMapCoord = blkOrigin + fromLocalCoord;
			for (int i = 0; i < (int)kNumAdj; ++i)
				if (isTradeNetworkAndIsConnected(fromMapCoord, i))
					if (const ivec2 toAdjLocalCoord = fromAdjLocalCoord + kAdj[i]; adjLocalRect.contains(toAdjLocalCoord))
						localUF.unionise(fromAdjLocalCoord, toAdjLocalCoord);
		}
	}

	Array2D<EBlockPlotGroup, 3> localGridOfBPGs{};
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			localGridOfBPGs[{ x, y }] = kNoBPG;

	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			// Skip the updated plot, we only want the ring around it.
			if (x == 1 && y == 1)
				continue;

			const ivec2 adjLocalCoord = ivec2(x, y);

			if (!adjLocalRect.contains(adjLocalCoord))
				continue;

			const ivec2 thisLocalCoord = adjLocalOrigin + adjLocalCoord;
			const ivec2 thisMapCoord = blkOrigin + thisLocalCoord;
			const EBlockPlotGroup bpgI = getBPGAt(thisMapCoord);
			const ivec2 repAdjLocalCoord = localUF.find(adjLocalCoord);
			EBlockPlotGroup& repBpgI = localGridOfBPGs[repAdjLocalCoord];
			if (bpgI != repBpgI)
			{
				if (repBpgI == kNoBPG)
					repBpgI = bpgI;
				else
					return EBlockUpdateStatus::Failed; // Can't trivially update. A component in adj-local UF spans different BPGs. So there are changes beyond adj-local.
			}
		}
	}

	// Check for duplicates.
	std::array<EBlockPlotGroup, 3 * 3> bpgList{};
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			bpgList[x + y * 3] = localGridOfBPGs[{ x, y }];
	std::ranges::sort(bpgList);
	const size_t numRingComponents = bpgList.size() - std::ranges::count(bpgList, kNoBPG);
	const bool hasUnassignedComponent = bpgList.back() == kNoBPG; // BPG indices are unsigned
	const size_t numUniqueRingBpgs = std::ranges::unique(bpgList).begin() - bpgList.begin() - hasUnassignedComponent;
	if (numRingComponents != numUniqueRingBpgs)
		return EBlockUpdateStatus::Failed; // Can't trivially update. Two components share a BPG.

	// Okay, each component has one BPG, and each component has a different BPG. So the components interesting the ring are unchanged.
	// Now, which BPG does the updated plot belong to?

	const EBlockPlotGroup updatedPlotRingBPG = localGridOfBPGs[localUF.find({ 1, 1 })];
	const EBlockPlotGroup updatedPlotCurrentBPG = getBPGAt(mapCoord);
	if (updatedPlotRingBPG == kNoBPG)
	{
		// Updated plot no longer belongs to any of the BPGs in the ring.
		// Either the plot gets its own BPG or it no longer belongs to any BPG.
		if (mIsTradeNetworkCache[mapCoord])
		{
			if (updatedPlotCurrentBPG == kNoBPG || mCore.getNumPlotsInBPG(updatedPlotCurrentBPG) > 1)
			{
				// Make a new BPG.
				const EBlockPlotGroup bpgI = mCore.createBPG(blkOrigin);
				mCore.setPlotBPG(mapCoord, bpgI);
				mBlocks[blkCoord].bpgs.push_back(bpgI);
				return EBlockUpdateStatus::Trivial;
			}
			else // we're already in our own BPG.
				return EBlockUpdateStatus::NoChange;
		}
		else
		{
			// Should no longer be part of a BPG.
			if (updatedPlotCurrentBPG != kNoBPG)
			{
				mCore.setPlotBPG(mapCoord, kNoBPG);
				if (mCore.getNumPlotsInBPG(updatedPlotCurrentBPG) <= 0)
					deleteBPG(blkCoord, updatedPlotCurrentBPG);
				return EBlockUpdateStatus::Trivial;
			}
			else
				return EBlockUpdateStatus::NoChange;
		}
	}
	else
	{
		// Updated plot belongs to one of the BPGs in the ring.
		if (updatedPlotCurrentBPG != updatedPlotRingBPG)
		{
			// Remove ourselves from current BPG.
			if (updatedPlotCurrentBPG != kNoBPG && mCore.getNumPlotsInBPG(updatedPlotCurrentBPG) <= 1)
				deleteBPG(blkCoord, updatedPlotCurrentBPG);
			// Then set.
			mCore.setPlotBPG(mapCoord, updatedPlotRingBPG);
			return EBlockUpdateStatus::Trivial;
		}
		else
			return EBlockUpdateStatus::NoChange;
	}
}

#if 0
TradeNetworkSystem::EBlockUpdateStatus TradeNetworkSystem::tryBlockTrivialUpdateCaseRemoveFromBPG(ivec2 blkCoord, ivec2 localCoord)
{
	//Block& block = mBlocks[blkCoord];
	const ivec2 blkDim = getBlockDim(blkCoord);
	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 mapCoord = blkOrigin + localCoord;
	const iaabb2 blkLocalRect{ .max = blkDim };

	// If we're not connected to a BPG, then there's trivially nothing to do.
	const EBlockPlotGroup bpgI = mCore.getBPGAt(mapCoord);
	if (bpgI == kNoBPG)
		return EBlockUpdateStatus::NoChange; // Nothing to do.

	// If this is a single-plot BPG, then we don't need to do any connectivity checks.
	if (mCore.getNumPlotsInBPG(bpgI) == 1)
	{
		if (mIsTradeNetworkCache[mapCoord])
		{
			// Single-plot BPG continues to exist. Nothing to do.
			return EBlockUpdateStatus::NoChange;
		}
		else
		{
			// Single-plot BPG gets deleted.
			deleteBPG(blkCoord, bpgI);
			return EBlockUpdateStatus::Trivial;
		}
	}

	// We're connected to a BPG with multiple plots.
	// Check if we're still connected to all adj plots in the BPG. This is a trivial early-out.
	
	if (mIsTradeNetworkCache[mapCoord])
	{
		bool remainConnected = true;
		for (const auto [adjI, adjD] : std::views::enumerate(kAdj))
		{
			const ivec2 adjLocalCoord = localCoord + adjD;
			if (iaabb2{ .max = blkDim }.contains(adjLocalCoord))
			{
				const EBlockPlotGroup adjBPGi = mCore.getBPGAt(mapCoord + adjD);
				if (bpgI == adjBPGi && !isTradeNetworkAndIsConnected(mapCoord, static_cast<int>(adjI)))
				{
					remainConnected = false;
					break;
				}
			}
		}
		// If we're connected to all adj plots in the BPG, then there's trivially nothing to do.
		if (remainConnected)
			return EBlockUpdateStatus::NoChange;
	}
	// else, we're not sure if the BPG stays connected.

	// Perform adj-local UF to determine whether the BPG stays connected by looking at only this plot and adj plots.
	{
		Array2D<ivec2, 3> localGrid{};
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x)
				localGrid[{ x, y }] = { x, y };

		ArrayUnionFind localUF(localGrid);

		const iaabb2 relRect = iaabb2{ .min = -1, .max = 2 }.intersection(blkLocalRect - localCoord);

		for (int y = relRect.min.y; y < relRect.max.y; ++y)
		{
			for (int x = relRect.min.x; x < relRect.max.x; ++x)
			{
				const ivec2 relCoordFrom(x, y);
				const ivec2 localCoordFrom = localCoord + relCoordFrom;
				const ivec2 mapCoordFrom = mapCoord + relCoordFrom;
				if (mCore.getBPGAt(mapCoordFrom) == bpgI)
				{
					for (int adjI = 0; adjI < kNumAdj; ++adjI)
					{
						const ivec2 relCoordTo = ivec2(x, y) + kAdj[adjI];
						if (relRect.contains(relCoordTo)
							&& mCore.getBPGAt(mapCoord + relCoordTo) == bpgI
							&& isTradeNetworkAndIsConnected(mapCoordFrom, adjI))
						{
							localUF.unionise(relCoordFrom + 1, relCoordTo + 1);
						}
					}
				}
			}
		}

		std::optional<ivec2> adjRep;

		// Check that all adj plots belong to the same local component.
		bool adjConnected = true;
		for (int y = -1; y <= +1 && adjConnected; ++y)
		{
			for (int x = -1; x <= +1; ++x)
			{
				const ivec2 relCoordFrom(x, y);
				if (relCoordFrom != ivec2() && relRect.contains(relCoordFrom) && mCore.getBPGAt(mapCoord + relCoordFrom) == bpgI)
				{
					const ivec2 rep = localUF.find(relCoordFrom + 1);
					if (!adjRep)
						adjRep = rep;
					else if (rep != *adjRep)
					{
						adjConnected = false;
						break;
					}
				}
			}
		}

		if (adjConnected)
		{
			// At least one adj plot belongs to the BPG, so we should have an adj rep.
			assert(adjRep);

			// All adj plots in the BPG remain locally connected, considering only this plot and the adj plots.
			// Connectivity beyond the adj plots remains unchanged, but this plot may be disconnected from the BPG now.
			// Just check the UF data structure.
			const ivec2 thisRep = localUF.find({ 1, 1 });
			if (thisRep != *adjRep)
			{
				// This plot got disconnected.
				mCore.setPlotBPG(mapCoord, kNoBPG);
				return EBlockUpdateStatus::Trivial;
			}
			else
			{
				// This plot remains part of the BPG.
				return EBlockUpdateStatus::NoChange;
			}
		}
		// Else, adj plots belong to different local components. We'll need to do UF over the whole block to check connectivity.
	}

	// If we need to do a whole-block scan to determine connectivity, then just fallback to general block update.
	return EBlockUpdateStatus::Failed;
}
TradeNetworkSystem::EBlockUpdateStatus TradeNetworkSystem::tryBlockTrivialUpdateCaseAddToBPG(ivec2 blkCoord, ivec2 localCoord)
{
	Block& block = mBlocks[blkCoord];
	const ivec2 blkDim = getBlockDim(blkCoord);
	const ivec2 blkOrigin = blkCoord * kBlockDim;
	const ivec2 mapCoord = blkOrigin + localCoord;
	const iaabb2 blkLocalRect{ .max = blkDim };

	std::optional<EBlockPlotGroup> optBPGToConnectTo;

	// Find out which adj BPG we want to connect to.
	if (mIsTradeNetworkCache[mapCoord])
	{
		for (int adjI = 0; adjI < kNumAdj; ++adjI)
		{
			const ivec2 adjLocalCoord = localCoord + kAdj[adjI];
			if (blkLocalRect.contains(adjLocalCoord))
			{
				const ivec2 adjMapCoord = blkOrigin + adjLocalCoord;
				const EBlockPlotGroup adjBpgI = mCore.getBPGAt(adjMapCoord);
				if (isTradeNetworkAndIsConnected(mapCoord, adjI))
				{
					if (adjBpgI == kNoBPG)
					{
						// Can't trivially update block because a neighbour needs to be added to a BPG.
						// If this happens, this means isTradeNetwork changed for the adjacent plot (so not just a single plot update).
						return EBlockUpdateStatus::Failed;
					}
					else if (!optBPGToConnectTo)
						optBPGToConnectTo = adjBpgI; // Found the first BPG we want to be connected to.
					else if (*optBPGToConnectTo != adjBpgI)
						return EBlockUpdateStatus::Failed; // Can't trivially update block because we need to join BPGs.
				}
			}
		}
	}

	// We may or may not be in a BPG currently.
	// Trivial removal may have left us in a BPG if we should still be in it.
	const EBlockPlotGroup bpgI = mCore.getBPGAt(mapCoord);

	if (optBPGToConnectTo)
	{ // We have an adj BPG to connect to.
		if (bpgI != *optBPGToConnectTo)
		{
			// Add to the adj BPG.
			mCore.setPlotBPG(mapCoord, *optBPGToConnectTo);
			// Original BPG may have become empty.
			if (mCore.getNumPlotsInBPG(bpgI) <= 0)
				deleteBPG(blkCoord, bpgI);
			return EBlockUpdateStatus::Trivial;
		}
		else // Nothing to do, already connected.
			return EBlockUpdateStatus::NoChange;
	}
	else
	{ // We have no adj BPG to connect to.
		if (bpgI == kNoBPG)
		{ // And we're not in a BPG.
			// If isTradeNetwork, we need to be in a BPG.
			if (mIsTradeNetworkCache[mapCoord])
			{
				const EBlockPlotGroup newBpgI = mCore.createBPG(blkOrigin);
				mCore.setPlotBPG(mapCoord, newBpgI);
				block.bpgs.push_back(newBpgI);
				return EBlockUpdateStatus::Trivial;
			}
			else // No BPG, no BPG to connect to, and not supposed to be in a BPG, so nothing to do.
				return EBlockUpdateStatus::NoChange;
		}
		else // We're in a BPG, nothing to do.
		{
			if (mCore.getNumPlotsInBPG(bpgI) != 1)
				dump("TradeNetworkSystem tryBlockTrivialUpdateCaseAddToBPG.html");
			assert(mCore.getNumPlotsInBPG(bpgI) == 1); // Because there's no adj connects, this should be the only plot in the BPG.
			return EBlockUpdateStatus::NoChange;
		}
	}
}
#endif

void TradeNetworkSystem::deleteBPG(ivec2 blkCoord, EBlockPlotGroup bpgI)
{
	Block& blk = mBlocks[blkCoord];

	delBPGAdj(bpgI);

	//for (const int adjI : range(kNumAdj))
	//{
	//	if (const auto optAdjCoord = mMapBlocksGeom.tryCanonicalise(blkCoord + kAdj[adjI]))
	//	{
	//		const int invAdjI = kInvAdjIndices[adjI];
	//		std::erase_if(blk.adjLinks[adjI], [bpgI](BPGLink link) { return link.from == bpgI; });
	//		std::erase_if(mBlocks[*optAdjCoord].adjLinks[invAdjI], [bpgI](BPGLink link) { return link.to == bpgI; });
	//	}
	//}

	

	const ECivPlotGroup cpgI = mCore.getCPGOf(bpgI);
	mCore.destroyBPG(bpgI);
	std::erase(blk.bpgs, bpgI);

	if (mCore.getNumBPGsInCPG(cpgI) <= 0)
		mCore.destroyCPG(cpgI);
}
/*
void TradeNetworkSystem::addOneWayAdj(EBlockPlotGroup a, EBlockPlotGroup b)
{
	if (a != b)
	{
		mAdjacencyLookup.emplace(a, b);
		//mAdjacencyLookup.emplace(b, a);
	}
}
*/
/*
void TradeNetworkSystem::delAdj(EBlockPlotGroup a, EBlockPlotGroup b)
{
	delOneWayAdj(a, b);
	delOneWayAdj(b, a);
}
*/
/*
void TradeNetworkSystem::delOneWayAdj(EBlockPlotGroup a, EBlockPlotGroup b)
{
	if (a != b)
	{
		const auto [begin, end] = mAdjacencyLookup.equal_range(a);
		for (auto it = begin; it != end; )
		{
			if (it->second == b)
				it = mAdjacencyLookup.erase(it);
			else
				++it;
		}
	}
}*/
void TradeNetworkSystem::delBPGAdj(EBlockPlotGroup a)
{
	/*const auto [begin, end] = mAdjacencyLookup.equal_range(a);
	if (begin != end)
	{
		// We need inclusive end to avoid iterator invalidation.
		const auto iend = std::prev(end);
		for (auto it = begin; it != iend; ++it)
			delOneWayAdj(it->second, a); // This won't erase elements in [begin, iend], as `b != a`.
		delOneWayAdj(iend->second, a);
		mAdjacencyLookup.erase(begin, std::next(iend));
	}*/

	const auto del = [](std::unordered_multimap<EBlockPlotGroup, EBlockPlotGroup>& map, EBlockPlotGroup a, EBlockPlotGroup b) {
		const auto [first, last] = map.equal_range(a);
		map.erase(std::ranges::find(std::ranges::subrange(first, last) | std::views::values, b).base());
		};

	{
		const auto [first, last] = mAdjacencyLookup.equal_range(a);
		for (const auto b : std::ranges::subrange(first, last) | std::views::values)
			del(mRevAdjacencyLookup, b, a);
		mAdjacencyLookup.erase(first, last);
	}
	
	{
		const auto [first, last] = mRevAdjacencyLookup.equal_range(a);
		for (const auto b : std::ranges::subrange(first, last) | std::views::values)
			del(mAdjacencyLookup, b, a);
		mRevAdjacencyLookup.erase(first, last);
	}
}

void TradeNetworkSystem::delBlockAdj(ivec2 blkCoord)
{
	for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
		delBPGAdj(bpgI);

	//for (auto& links : mBlocks[blkCoord].adjLinks)
	//	links.clear();
	//for (const int adjI : range(kNumAdj))
	//	if (const auto optAdjCoord = mMapBlocksGeom.tryCanonicalise(blkCoord + kAdj[adjI]))
	//		mBlocks[*optAdjCoord].adjLinks[kInvAdjIndices[adjI]].clear();
}

bool TradeNetworkSystem::updateInterBlockConnections(ivec2 blkCoord)
{
	bool changed = false;
	for (int i = 0; i < (int)kNumAdj; ++i)
		changed |= updateInterBlockConnections(blkCoord, i);
	return changed;
}
bool TradeNetworkSystem::updateInterBlockConnections(ivec2 blkCoord, int adjI)
{
	const auto realBlkCoord = mMapBlocksGeom.tryCanonicalise(blkCoord);
	if (!realBlkCoord)
		return false;

	blkCoord = *realBlkCoord;

	const auto optAdjBlkCoord = mMapBlocksGeom.tryCanonicalise(blkCoord + kAdj[adjI]);
	if (!optAdjBlkCoord)
		return false;

	const ivec2 adjBlkCoord = *optAdjBlkCoord;

	//if (adjI >= kNumAdj / 2)
	//{
	//	// Swap. We only store connections for adj < kNumAdj / 2, so that there's no duplicate connection info.
	//	std::swap(adjBlkCoord, blkCoord);
	//	adjI = kInvAdjIndices[adjI];
	//}

	const ivec2 fromBlkDim = getBlockDim(blkCoord);
	const ivec2 toBlkDim = getBlockDim(adjBlkCoord);

	const ivec2 fromBlkOrigin = blkCoord * kBlockDim;
	const ivec2 toBlkOrigin = adjBlkCoord * kBlockDim;

	ivec2 borderLocalCoordsFirst{};
	ivec2 borderLocalCoordsFirstInToBlk{};
	ivec2 borderLocalCoordsStep{};
	int borderLocalCoordsCount{};
	std::array<int, 3> borderAdjCheckList{};

	// Make border plot enumeration generic.
	switch (adjI)
	{
	case 0: // diag
		borderLocalCoordsFirst = {};
		borderLocalCoordsFirstInToBlk = toBlkDim - 1;
		borderLocalCoordsStep = {};
		borderLocalCoordsCount = 1;
		borderAdjCheckList = { adjI, adjI, adjI };
		break;
	case 1: // up
		borderLocalCoordsFirst = {};
		borderLocalCoordsFirstInToBlk = { 0, toBlkDim.y - 1 };
		borderLocalCoordsStep = { 1, 0 };
		borderLocalCoordsCount = fromBlkDim.x;
		borderAdjCheckList = { 0, 1, 2 };
		break;
	case 2: // diag
		borderLocalCoordsFirst = { fromBlkDim.x - 1, 0 };
		borderLocalCoordsFirstInToBlk = { 0, toBlkDim.y - 1 };
		borderLocalCoordsStep = {};
		borderLocalCoordsCount = 1;
		borderAdjCheckList = { adjI, adjI, adjI };
		break;
	case 3: // left
		borderLocalCoordsFirst = {};
		borderLocalCoordsFirstInToBlk = { toBlkDim.x - 1, 0 };
		borderLocalCoordsStep = { 0, 1 };
		borderLocalCoordsCount = fromBlkDim.y;
		borderAdjCheckList = { 0, 3, 5 };
		break;

	case 4: // right
		borderLocalCoordsFirst = { fromBlkDim.x - 1, 0 };
		borderLocalCoordsFirstInToBlk = { 0, 0 };
		borderLocalCoordsStep = { 0, 1 };
		borderLocalCoordsCount = fromBlkDim.y;
		borderAdjCheckList = { 2, 4, 7 };
		break;
	case 5: // bottom left
		borderLocalCoordsFirst = { 0, fromBlkDim.y - 1 };
		borderLocalCoordsFirstInToBlk = { toBlkDim.x - 1, 0 };
		borderLocalCoordsStep = {};
		borderLocalCoordsCount = 1;
		borderAdjCheckList = { adjI, adjI, adjI };
		break;
	case 6: // down
		borderLocalCoordsFirst = { 0, fromBlkDim.x - 1 };
		borderLocalCoordsFirstInToBlk = { 0, 0 };
		borderLocalCoordsStep = { 1, 0 };
		borderLocalCoordsCount = fromBlkDim.x;
		borderAdjCheckList = { 5, 6, 7 };
		break;
	case 7: // bottom right
		borderLocalCoordsFirst = fromBlkDim - 1;
		borderLocalCoordsFirstInToBlk = { 0, 0 };
		borderLocalCoordsStep = {};
		borderLocalCoordsCount = 1;
		borderAdjCheckList = { adjI, adjI, adjI };
		break;

	default:
		std::unreachable();
	}

	std::vector<std::pair<EBlockPlotGroup, EBlockPlotGroup>> links;

	for (int i = 0; i < borderLocalCoordsCount; ++i)
	{
		const ivec2 fromLocalCoord = borderLocalCoordsFirst + borderLocalCoordsStep * i;
		const ivec2 fromMapCoord = fromBlkOrigin + fromLocalCoord;
		const EBlockPlotGroup fromBPGi = mCore.getBPGAt(fromMapCoord);

		if (fromBPGi != kNoBPG)
		{
			// Need to check all plots around the from plot, but only the plots that are in toBlk.
			for (int d = -1; d <= 1; ++d)
			{
				// Don't go out of bounds. And for diagonals, only check one plot.
				if (0 <= d + i && d + i < borderLocalCoordsCount)
				{
					const ivec2 toLocalCoord = borderLocalCoordsFirstInToBlk + borderLocalCoordsStep * (i + d);
					if (const auto optToMapCoord = mMapGeom.tryCanonicalise(toBlkOrigin + toLocalCoord))
					{
						if (const EBlockPlotGroup toBPGi = mCore.getBPGAt(*optToMapCoord); toBPGi != kNoBPG)
						{
							// Okay, we've got both a from BPG and a to BPG. Check adj flag.
							const int checkAdjI = borderAdjCheckList[d + 1];
							if (((mIsTradeNetworkConnectedCache[fromMapCoord] >> checkAdjI) & 1) != 0)
								links.emplace_back(fromBPGi, toBPGi);
						}
					}
				}
			}
		}
	}

	// Deduplicate.
	std::ranges::sort(links, std::less<>());
	links.erase(std::ranges::unique(links, std::equal_to<>()).begin(), links.end());

	for (const auto [a, b] : links)
	{
		mAdjacencyLookup.emplace(a, b);
		mRevAdjacencyLookup.emplace(b, a);
		//const auto it = mAdjacencyLookup.emplace(a, b);
		//mAdjacencyBPGRefsLookup.emplace(a, it);
		//mAdjacencyBPGRefsLookup.emplace(b, it);
		//mBlocks[blkCoord].adjLinks[adjI].emplace_back(a, b);
	}

	return true;
}


auto TradeNetworkSystem::getAdj(EBlockPlotGroup a) const
{
	const auto [begin, end] = mAdjacencyLookup.equal_range(a);
	return std::ranges::subrange(begin, end) | std::views::values;
}


void TradeNetworkSystem::doLocalCPGUpdates(std::span<const ivec2> localBlkList, std::span<const ivec2> ringBlkList, std::unordered_set<ECivPlotGroup>& dirtyCPGs)
{
	/// What we do here is try to update CPGs locally.
	/// If the set of a CPG's BPGs in the ring blocks stays the same, then we only need to update the CPG at the updated blocks.
	/// To check "stays the same", perform a local union-find in localBlkList, then go through the components to see which CPG matches up with them.
	// localBlkList is all blocks within 1-step radius of updated blocks.
	// ringBlkList is localBlkList excluding updated blocks.
	
	// Local union-find over all BPGs in local blocks (with some BPGs outside because connections list them).
	AssocUnionFind<std::unordered_map<EBlockPlotGroup, EBlockPlotGroup>> localUF;
	for (const ivec2 blkCoord : localBlkList)
	{
		//for (const auto& adj : mBlocks[blkCoord].adjLinks)
		//	for (const auto& link : adj)
		//		localUF.unionise(link.from, link.to);
		for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
			for (const EBlockPlotGroup adjBpgI : getAdj(bpgI))
				localUF.unionise(bpgI, adjBpgI);
	}

	// Build local components from local union-find.
	std::unordered_map<EBlockPlotGroup, std::vector<EBlockPlotGroup>> localComponents;
	for (const ivec2 blkCoord : localBlkList)
		for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
			localComponents[localUF.find(bpgI)].push_back(bpgI);

	// Build set of all BPGs in ring blocks.
	std::unordered_set<EBlockPlotGroup> ringBpgs;
	for (const ivec2 blkCoord : ringBlkList)
		ringBpgs.insert_range(mBlocks[blkCoord].bpgs);

	// Build lookup from ring CPG to list of ring BPGs in CPG.
	std::unordered_map<ECivPlotGroup, std::vector<EBlockPlotGroup>> cpgRingBpgs;
	for (const ivec2 blkCoord : ringBlkList)
		for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
			if (const ECivPlotGroup cpgI = mCore.getCPGOf(bpgI); cpgI != kNoCPG)
				cpgRingBpgs[cpgI].push_back(bpgI);

	// Sort ring BPG lists so that they can be used for keys.
	for (auto&& [cpgI, bpgs] : cpgRingBpgs)
		std::ranges::sort(bpgs);

	// Flip the map. Get CPG by sorted ring BPG list.
	std::map<std::vector<EBlockPlotGroup>, ECivPlotGroup> ringBpgsCpgLookup(std::from_range, cpgRingBpgs | std::views::transform([](decltype(cpgRingBpgs)::value_type kv) {
		return std::pair(kv.second, kv.first);
		}));

	std::vector<EBlockPlotGroup> localComponentRingBPGs;

	// For each local component...
	for (const std::vector<EBlockPlotGroup>& localComponent : localComponents | std::views::values)
	{
		// Filter BPGs by ring blocks.
		localComponentRingBPGs.assign_range(std::views::all(localComponent)
			| std::views::filter([&](EBlockPlotGroup bpgI) { return ringBpgs.contains(bpgI); }));
		// Sort for map lookup.
		std::ranges::sort(localComponentRingBPGs);
		// If a CPG exists for this set of ring BPGs, then...
		if (const auto cpgIt = ringBpgsCpgLookup.find(localComponentRingBPGs); cpgIt != ringBpgsCpgLookup.end() && dirtyCPGs.contains(cpgIt->second))
		{
			// Assign all BPGs in this local component to the CPG.
			const ECivPlotGroup cpgI = cpgIt->second;
			for (const EBlockPlotGroup bpgI : localComponent)
			{
				const ECivPlotGroup oldCpgI = mCore.getCPGOf(bpgI);
				mCore.setBPGCPG(bpgI, cpgI);
				if (mCore.getNumBPGsInCPG(oldCpgI) <= 0)
				{
					// CPG became empty.
					// Delete CPG.
					mCore.destroyCPG(oldCpgI);
					dirtyCPGs.erase(oldCpgI);
				}
			}

			gStat_NumCPGsLocallyUpdated.inc();
			dirtyCPGs.erase(cpgI);

			// There may also (local) BPGs in the CPG that are not in this component. They will be assigned to the correct CPG later.
		}
		else
		{
			// Unassign the BPGs. Assign them later in global CPG update.
			for (const EBlockPlotGroup bpgI : localComponent)
			{
				const ECivPlotGroup oldCpgI = mCore.getCPGOf(bpgI);
				mCore.setBPGCPG(bpgI, kNoCPG);
				if (mCore.getNumBPGsInCPG(oldCpgI) <= 0)
				{
					// CPG became empty.
					// Delete CPG.
					mCore.destroyCPG(oldCpgI);
					dirtyCPGs.erase(oldCpgI);
				}
			}
		}
	}
}

void TradeNetworkSystem::doGlobalCPGUpdates(std::span<const ivec2> localBlkList, std::unordered_set<ECivPlotGroup>& dirtyCPGs)
{
	// Whether or not local CPG updates have been done, this will globally update the CPGs in `dirtyCPGs`
	// and assign CPGs to unassigned BPGs in localBlkList.

	// localBlkList is the set of all blocks within 1-step radius of updated blocks.
	
	// bpgIndices := all unassigned BPGs in localBlkList + all BPGs in dirtyCPGs.
	std::vector<EBlockPlotGroup> bpgIndices;

	for (const ivec2 blkCoord : localBlkList)
		for (const EBlockPlotGroup bpgI : mBlocks[blkCoord].bpgs)
			if (mCore.getCPGOf(bpgI) == kNoCPG)
				bpgIndices.push_back(bpgI);

	for (const ECivPlotGroup cpgI : dirtyCPGs)
		bpgIndices.append_range(mCore.getBPGsOf(cpgI));

//#ifndef NDEBUG
	const std::unordered_set<EBlockPlotGroup> bpgIndicesSet(std::from_range, bpgIndices);
//#endif
	
	// Perform union-find across all BPGs.
	AssocUnionFind<std::unordered_map<EBlockPlotGroup, EBlockPlotGroup>> globalUF;
	for (const EBlockPlotGroup bpgI : bpgIndices)
		for (const EBlockPlotGroup adjBpgI : getAdj(bpgI))
	//for (const ivec2 blkCoord : localBlkList)
	//	for (const auto& adj : mBlocks[blkCoord].adjLinks)
	//		for (const auto [bpgI, adjBpgI] : adj)
			{
				// Because `bpgIndices` contains all potentially affected BPGs, adj lookup should not leave the set of `bpgIndices`.
				//assert(bpgIndicesSet.contains(adjBpgI));
				if (bpgIndicesSet.contains(bpgI) && bpgIndicesSet.contains(adjBpgI))
					globalUF.unionise(bpgI, adjBpgI);
			}

	// Find components. These are the BPGs of the final CPGs.
	std::unordered_map<EBlockPlotGroup, std::vector<EBlockPlotGroup>> globalComponents;
	for (const EBlockPlotGroup bpgI : bpgIndices)
		globalComponents[globalUF.find(bpgI)].push_back(bpgI);

	// Sort BPG lists for set ops.
	for (std::vector<EBlockPlotGroup>& bpgs : globalComponents | std::views::values)
		std::ranges::sort(bpgs);

	// For each component...
	for (const std::vector<EBlockPlotGroup>& globalComponent : globalComponents | std::views::values)
	{
		// Find the most common CPG in this component.
		std::unordered_map<ECivPlotGroup, size_t> cpgCounts;
		for (const EBlockPlotGroup bpgI : globalComponent)
			if (const ECivPlotGroup cpgI = mCore.getCPGOf(bpgI); cpgI != kNoCPG)
				++cpgCounts[cpgI];

		const auto bestIt = std::ranges::max_element(cpgCounts, std::less<>(), [](decltype(cpgCounts)::value_type kv) { return kv.second; });

		if (bestIt == cpgCounts.end())
		{
			// No CPG found. Make a new CPG.
			const ECivPlotGroup cpgI = mCore.createCPG();
			for (const EBlockPlotGroup bpgI : globalComponent)
				mCore.setBPGCPG(bpgI, cpgI);

			gStat_NumCPGsGloballyCreated.inc();
		}
		else
		{
			// Minimally modify this CPG.
			// Note that `cpgI` is not any previously updated CPG, because previously updated CPGs are updated to have only the BPGs within a component that is not this component.
			const ECivPlotGroup cpgI = bestIt->first;

			assert(dirtyCPGs.contains(cpgI));

			// Sort current BPG list for set ops.
			std::vector<EBlockPlotGroup> originalBpgIndices(std::from_range, mCore.getBPGsOf(cpgI));
			std::ranges::sort(originalBpgIndices);

			// removed BPGs = originalBpgIndices - globalComponent
			std::ranges::set_difference(originalBpgIndices, globalComponent, FunctionRefOutputIterator([this](EBlockPlotGroup bpgI) {
				mCore.setBPGCPG(bpgI, kNoCPG); // This will get assigned to another CPG later.
				}));
			// added BPGs = globalComponent - originalBpgIndices
			std::ranges::set_difference(globalComponent, originalBpgIndices, FunctionRefOutputIterator([cpgI, this](EBlockPlotGroup bpgI) {
				mCore.setBPGCPG(bpgI, cpgI);
				}));

			dirtyCPGs.erase(cpgI);

			gStat_NumCPGsGloballyUpdated.inc();
		}
	}

	// Now, the remaining CPGs in dirtyCPGs are empty, because all the BPGs in them got reassigned to different CPGs.
	// Delete them.
	for (const ECivPlotGroup cpgI : dirtyCPGs)
	{
		assert(mCore.getNumBPGsInCPG(cpgI) <= 0);
		mCore.destroyCPG(cpgI);
		gStat_NumCPGsGloballyDeleted.inc();
	}
}
