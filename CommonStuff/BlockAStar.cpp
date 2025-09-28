#include "inc/CommonStuff/BlockAStar.h"
#include "inc/CommonStuff/div.h"
#include "inc/CommonStuff/range.h"
#include "inc/CommonStuff/PPM.h"

#include <iostream>

using namespace heck;

static constexpr unsigned int kNumSides = 4;

[[maybe_unused]] static constexpr bool kEnableDebugChecks = false;

static constexpr BlockAStar::ESide opposite(BlockAStar::ESide a)
{
	return BlockAStar::ESide(a ^ 0b10);
}

static constexpr bool isVert(BlockAStar::ESide a)
{
	return a % 2 != 0;
}

static constexpr std::array<ivec2, kNumSides> kSideCoords{
	ivec2(0, -1),
	ivec2(-1, 0),
	ivec2(0, +1),
	ivec2(+1, 0),
};

static constexpr std::array<ivec2, kNumSides> kSideEdgeStorageCoords{
	ivec2(0, 0),
	ivec2(0, 0),
	ivec2(0, 1),
	ivec2(1, 0),
};

BlockAStar::BlockAStar(ivec2 dim)
	: mMapDim(dim)
	, mBlockHalfBorders(cdiv(dim, kMapBlockDiv))
	, mBlockStates(mBlockHalfBorders.dim)
{
	mOpenSetHeap.reserve(100);
	mResetList.reserve(mBlockStates.cells.size());
}

void BlockAStar::reset()
{
	//const ivec2 mapDimBlocks = mBlockStates.dim;
	for (const ivec2 coord : mResetList)
	{
		mBlockHalfBorders[coord] = {};
		mBlockStates[coord] = {};
		// Also need to reset the other two edges in case the neighbouring blocks aren't in the visit set.
		//if (coord.x + 1 < mapDimBlocks.x)
		//{
		//	mBlockHalfBorders[coord + ivec2(1, 0)][ESide::VMinX] = {};
		//	mBlockStates[coord + ivec2(1, 0)].fcostValues.edges[ESide::VMinX] = {};
		//}
		//if (coord.y + 1 < mapDimBlocks.y)
		//{
		//	mBlockHalfBorders[coord + ivec2(0, 1)][ESide::HMinY] = {};
		//	mBlockStates[coord + ivec2(0, 1)].fcostValues.edges[ESide::HMinY] = {};
		//}
	}

	mOpenSetHeap.clear();
	mResetList.clear();
}

void BlockAStar::addStartBlock(ivec2 startBlkCoord)
{
	//const ivec2 startBlkCoord = startCoord / kMapBlockDiv;
	mOpenSetHeap.push_back({ .bestF = 0, .blkCoord = (i16vec2)startBlkCoord }); // dummy cost will be updated
}

void BlockAStar::updateOpenSetHeuristicValues(const IGraph& g)
{
	updateHeuristic(g);
}

bool BlockAStar::search(ivec2 goalPlotCoord, IGraph& map, int32_t maxFCost)
{
	static constexpr BorderEdge kUnreachableBorderEdge{
		.gcosts = kUnreachableGlobalCost,
	};

	//const ivec2 startBlockCoord = mStartPlotCoord / kMapBlockDiv;
	const ivec2 goalBlockCoord = goalPlotCoord / kMapBlockDiv;

	int32_t bestKnownCost = map.getGoalCost(goalPlotCoord, goalBlockCoord);

	while (!mOpenSetHeap.empty())
	{
		const auto [currentBlkF, currentBlkCoord] = mOpenSetHeap.front();

		//std::clog << "Block " << currentBlkCoord << ' ' << currentBlkF << ' ' << bestKnownCost << std::endl;
		//if ((ivec2)currentBlkCoord == ivec2(324, 338) / kMapBlockDiv)
		//	_CrtDbgBreak();

		// Extra check for the start block which starts at cost kUnreachableGlobalCost.
		if (bestKnownCost < kUnreachableGlobalCost && (currentBlkF >= bestKnownCost || currentBlkF > maxFCost))
			break;

		popHeap();

		BlockState& blkState = mBlockStates[currentBlkCoord];

		// Ignore out of date heap entries.
		if (currentBlkF != blkState.currentHeapValue)
			continue;

		// Allow this block to be inserted again.
		blkState.currentHeapValue = kUnreachableGlobalCost;

		insertIntoResetList(currentBlkCoord);

		++blkState.expandCount;

		const BlockBorder initialBlockBorder{
			mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[0]][0],
			mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[1]][1],
			isBlkCoordInRange(ivec2(currentBlkCoord) + kSideEdgeStorageCoords[2]) ? mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[2]][0] : kUnreachableBorderEdge,
			isBlkCoordInRange(ivec2(currentBlkCoord) + kSideEdgeStorageCoords[3]) ? mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[3]][1] : kUnreachableBorderEdge,
		};

		const BlockBorder updatedBlockBorder = map.updateBlock(currentBlkCoord, initialBlockBorder, blkState.dirtyEdgesMask);

		// Check goal after updating start costs.
		if (ivec2(currentBlkCoord) == goalBlockCoord)
			bestKnownCost = std::min(bestKnownCost, map.getGoalCost(goalPlotCoord, goalBlockCoord/*, updatedBlockBorder*/));

		const std::array<Vector::Mask, kNumSides> edgeDirtyPlotBits{
			vcmplt(updatedBlockBorder[0].gcosts, initialBlockBorder[0].gcosts),
			vcmplt(updatedBlockBorder[1].gcosts, initialBlockBorder[1].gcosts),
			vcmplt(updatedBlockBorder[2].gcosts, initialBlockBorder[2].gcosts),
			vcmplt(updatedBlockBorder[3].gcosts, initialBlockBorder[3].gcosts),
		};

		// Apply updated cost vectors.
		mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[0]][0] = updatedBlockBorder[0];
		mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[1]][1] = updatedBlockBorder[1];
		if (isBlkCoordInRange(ivec2(currentBlkCoord) + kSideEdgeStorageCoords[2]))
			mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[2]][0] = updatedBlockBorder[2];
		if (isBlkCoordInRange(ivec2(currentBlkCoord) + kSideEdgeStorageCoords[3]))
			mBlockHalfBorders[ivec2(currentBlkCoord) + kSideEdgeStorageCoords[3]][1] = updatedBlockBorder[3];

		// Propagate changes to adjacent block dirty bits, then push them onto the heap.
		invalidateAdjCardinal(currentBlkCoord, edgeDirtyPlotBits[ESide::HMinY].asBits(), ESide::HMinY, map); //newHeapValues[ESide::HMinY]);
		invalidateAdjCardinal(currentBlkCoord, edgeDirtyPlotBits[ESide::VMinX].asBits(), ESide::VMinX, map); //newHeapValues[ESide::VMinX]);
		invalidateAdjCardinal(currentBlkCoord, edgeDirtyPlotBits[ESide::HMaxY].asBits(), ESide::HMaxY, map); //newHeapValues[ESide::HMaxY]);
		invalidateAdjCardinal(currentBlkCoord, edgeDirtyPlotBits[ESide::VMaxX].asBits(), ESide::VMaxX, map); //newHeapValues[ESide::VMaxX]);
	}

	return bestKnownCost < kUnreachableGlobalCost;
}

void BlockAStar::dumpStats(std::ostream& os) const
{
	const size_t numVisitedBlocks = std::ranges::count_if(mBlockStates.cells, [](BlockState state) { return state.expandCount > 0; });
	const size_t numBlockExpands = std::ranges::fold_left(mBlockStates.cells | std::views::transform(&BlockState::expandCount), size_t(0), std::plus<>());
	os << "Num visited blocks = " << numVisitedBlocks << std::endl;
	os << "Num block expands = " << numBlockExpands << std::endl;
	os << "Num extra expands = " << numBlockExpands - numVisitedBlocks << std::endl;
	os << "Average extra expands per block = " << (numBlockExpands - numVisitedBlocks) / (double)numVisitedBlocks << std::endl;
}

void BlockAStar::debugDraw(Image& img) const
{
	for (const int blkY : range(mBlockHalfBorders.dim.y))
	{
		for (const int blkX : range(mBlockHalfBorders.dim.x))
		{
			const ivec2 blkCoord{ blkX, blkY };
			const ivec2 plotCoord = blkCoord * kMapBlockDiv;
			if (vcmplt(mBlockHalfBorders[blkCoord][ESide::HMinY].gcosts, kUnreachableGlobalCost).asBits())
				img.drawHLine(plotCoord, kCoordVectorLength, { 255, 0, 255 }, 128);
			if (vcmplt(mBlockHalfBorders[blkCoord][ESide::VMinX].gcosts, kUnreachableGlobalCost).asBits())
				img.drawVLine(plotCoord, kCoordVectorLength, { 255, 0, 255 }, 128);
		}
	}
}


[[maybe_unused]] static int32_t satsub(int32_t from, int32_t to)
{
	return std::max(from, to) - to;
}
bool BlockAStar::isBlkCoordInRange(ivec2 coord) const
{
	return 0 <= coord.x && coord.x < mBlockHalfBorders.dim.x
		&& 0 <= coord.y && coord.y < mBlockHalfBorders.dim.y;
}
int32_t BlockAStar::calcBlockHeapValueFromBoundary(ivec2 blkCoord) const
{
	return std::min({
		mBlockStates[blkCoord + kSideEdgeStorageCoords[0]].fcostValues.edges[0],
		mBlockStates[blkCoord + kSideEdgeStorageCoords[1]].fcostValues.edges[1],
		isBlkCoordInRange(blkCoord + kSideEdgeStorageCoords[2]) ? mBlockStates[blkCoord + kSideEdgeStorageCoords[2]].fcostValues.edges[0] : kUnreachableGlobalCost,
		isBlkCoordInRange(blkCoord + kSideEdgeStorageCoords[3]) ? mBlockStates[blkCoord + kSideEdgeStorageCoords[3]].fcostValues.edges[1] : kUnreachableGlobalCost,
	});
}
void BlockAStar::updateBlockHeapValueAndPush(i16vec2 blkCoord, int32_t heapValue)
{
	const int32_t cost = calcBlockHeapValueFromBoundary(blkCoord);

	updateBlockHeapValueAndPush(mBlockStates[blkCoord], blkCoord, cost, heapValue);
}
void BlockAStar::updateBlockHeapValueAndPush(BlockState& blockState, i16vec2 blkCoord, [[maybe_unused]] int32_t newBestFCost, int32_t heapValue)
{
	assert(heapValue > 0);
	if (heapValue < blockState.currentHeapValue)
	{
		blockState.currentHeapValue = heapValue;
		mOpenSetHeap.emplace_back(heapValue, blkCoord);
		std::ranges::push_heap(mOpenSetHeap, std::less<>()); // std::less, as std::ranges::less is more strict about ordering properties...
	}
}
int32_t BlockAStar::calcHVectorHeapValue(const IGraph& map, ivec2 blkCoord) const
{
	const Vector gcosts = mBlockHalfBorders[blkCoord][ESide::HMinY].gcosts;
	const Vector::Mask mask = vcmplt(gcosts, kUnreachableGlobalCost);
	const Vector h = map.getHLineHeuristic(blkCoord * ivec2(kMapBlockDiv));
	return hmin(vselect(mask, h + gcosts, kUnreachableGlobalCost));
}
int32_t BlockAStar::calcVVectorHeapValue(const IGraph& map, ivec2 blkCoord) const
{
	const Vector gcosts = mBlockHalfBorders[blkCoord][ESide::VMinX].gcosts;
	const Vector::Mask mask = vcmplt(gcosts, kUnreachableGlobalCost);
	const Vector h = map.getVLineHeuristic(blkCoord * ivec2(kMapBlockDiv));
	return hmin(vselect(mask, h + gcosts, kUnreachableGlobalCost));
}
void BlockAStar::popHeap()
{
	std::ranges::pop_heap(mOpenSetHeap, std::less<>());
	mOpenSetHeap.pop_back();
}
void BlockAStar::updateHeuristic(const IGraph& map)
{
	const auto updateBlockHeuristic = [&map, this](ivec2 blkCoord) {
		mBlockStates[blkCoord].fcostValues = {
			calcHVectorHeapValue(map, blkCoord),
			calcVVectorHeapValue(map, blkCoord),
		};
		mBlockStates[blkCoord].currentHeapValue = calcBlockHeapValueFromBoundary(blkCoord);
		};

	//for (const int blkY : range(mBlockHalfBorders.dim.y))
	//{
	//	for (const int blkX : range(mBlockHalfBorders.dim.x))
	//	{
	//		const ivec2 blkCoord{ blkX, blkY };
	//		mBlockStates[blkCoord].fcostValues = {
	//			calcHVectorHeapValue(map, blkCoord),
	//			calcVVectorHeapValue(map, blkCoord),
	//		};
	//	}
	//}
	//
	//for (const int blkY : range(mBlockHalfBorders.dim.y))
	//	for (const int blkX : range(mBlockHalfBorders.dim.x))
	//		mBlockStates[{ blkX, blkY }].currentHeapValue = calcBlockHeapValueFromBoundary({ blkX, blkY });

	// First, clear out the heap values.
	for (OpenSetEntry& entry : mOpenSetHeap)
		mBlockStates[entry.blkCoord].currentHeapValue = -1;

	// Then set the heap values. But for duplicates, mark for removal.
	// NOTE: Need to distinguish between goal-unreachable and duplicate. Goal-unreachable blocks should still be kept (as they would be goal-reachable with a different goal).
	for (OpenSetEntry& entry : mOpenSetHeap)
	{
		if (mBlockStates[entry.blkCoord].currentHeapValue < 0)
		{
			updateBlockHeuristic(entry.blkCoord);
			entry.bestF = mBlockStates[entry.blkCoord].currentHeapValue;
		}
		else
			entry.bestF = -1;
	}

	// Remove duplicates.
	std::erase_if(mOpenSetHeap, [](OpenSetEntry entry) { return entry.bestF < 0; });

	std::ranges::make_heap(mOpenSetHeap, std::less<>());
}
void BlockAStar::invalidateAdjCardinal(ivec2 currentBlkCoord, unsigned int edgeDirtyBits, ESide side, IGraph& graph)
{
	if (!edgeDirtyBits)
		return;

	const ivec2 adjBlkCoord = currentBlkCoord + kSideCoords[side];

	//if (adjBlkCoord == ivec2(2, 1) && graph.isDebugPathingRequest())
	//	__debugbreak();

	if (!isBlkCoordInRange(adjBlkCoord))
		return;

	const int newEdgeFCost = isVert(side) ? calcVVectorHeapValue(graph, adjBlkCoord) : calcHVectorHeapValue(graph, adjBlkCoord);

	const ivec2 edgeStorageBlkCoord = currentBlkCoord + kSideEdgeStorageCoords[side];

	//const int32_t oldEdgeFCost = mBlockStates[edgeStorageBlkCoord].fcostValues.edges[side % 2];
	//const int32_t oldAdjFCost = calcBlockHeapValueFromBoundary(adjBlkCoord);
	//const int32_t fCostReduction = satsub(oldEdgeFCost, newEdgeFCost);

	mBlockStates[edgeStorageBlkCoord].fcostValues.edges[side % 2] = newEdgeFCost;

	BlockState& adjBlockState = mBlockStates[adjBlkCoord];
	insertIntoResetList(adjBlkCoord);
	adjBlockState.dirtyEdgesMask |= 1 << opposite(side);
	updateBlockHeapValueAndPush(adjBlockState, adjBlkCoord, newEdgeFCost, newEdgeFCost);
		//oldAdjFCost < kUnreachableGlobalCost ? satsub(oldAdjFCost, fCostReduction) : newEdgeFCost);
}

void BlockAStar::insertIntoResetList(ivec2 blkCoord)
{
	BlockState& blk = mBlockStates[blkCoord];
	if (!blk.isInResetList)
	{
		blk.isInResetList = true;
		mResetList.push_back(blkCoord);
	}
}