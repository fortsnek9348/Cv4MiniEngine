#pragma once

#include "SimdAStarGraph.h"
#include "CompilerMacros.h"
#include "Array2D.h"

namespace heck
{
	struct Image;

	// "Block A*: Database-Driven Search with Applications in Any-Angle Path-Planning"
	// Here, we assume SIMD is used for boundary updates.
	// And a further improvement is that block boundaries are shared, so that the implementation doesn't have to care about stepping between individual plots.
	// Register layout for 4x4 LDDB blocks (3x3 map partitioning):
	//
	// 0 1 2 3 4 5 6
	// x h h x h h x 0
	// v     v     v 1
	// v     v     v 2
	// x h h x h h x 3
	// v     v     v 4
	// v     v     v 5
	// x h h x h h x 6
	//
	// `h` and `v` regs start and end at corners.
	// 'x' are the shared values at corners.
	// All registers are aligned in memory, hence, shared values.
	// Blocks on the end of the map are smaller.
	// Map wrapping is handled by extending the map by one plot in positive wrapping directions, then Block A* can wrap block-wise as block boundaries will be shared.
	// And a very nice property for the implementation is that you only have to care about cardinal adjacency!
	// Updating a cardinal neighbour will trigger the diagonal neighbour to be updated afterwards, with the correct values.
	//
	// The graph can be implemented as either an LDDB lookup or a plotwise update.
	// Because an LDDB is impractical for Civ4, we'll do plotwise updates, which are not cheap. So going through virtual functions won't be so bad.
	//
	// And because this is a blockwise algorithm, it doesn't need to be particularly efficient. Just use a normal priority queue.
	//
	// ## Wrapping
	// 
	// Map is wrapped/clamped at the block level.
	// In either case, the map is extended by one plot. The MaxX/Y border edge falls on plot x/y == width/height.
	// If clamped, the map-limit block borders are never reachable.
	// If wrapped, the map-limit block borders are alias the MinX/Y map borders.
	// Map-limit block borders are synthesised before calling into IGraph, so they are not stored explicitly.
	class BlockAStar
	{
	public:
		static constexpr unsigned int kCoordVectorLength = SimdAStarConfig::kCoordVectorLength;
		using Vector = SimdAStarConfig::Vector;
		using StateVector = SimdAStarConfig::StateVector;

		static constexpr unsigned int kMapBlockDiv = kCoordVectorLength - 1;

		static constexpr int32_t kUnreachableGlobalCost = INT32_MAX;

		enum ESide
		{
			HMinY,
			VMinX,
			HMaxY,
			VMaxX,

			// +/- 2 gets to the opposite side
		};

		struct BorderEdge
		{
			Vector gcosts = kUnreachableGlobalCost;
			StateVector state{};
		};

		using BlockBorder = std::array<BorderEdge, 4>;


		class IGraph
		{
		public:
			static constexpr unsigned int kCoordVectorLength = BlockAStar::kCoordVectorLength;
			static constexpr int kMapBlockDiv = BlockAStar::kMapBlockDiv;
			using Vector = Vector;
			using StateVector = StateVector;
			using ESide = ESide;
			using BlockBorder = BlockBorder;

			//IGraph() = default;
			//IGraph(const IGraph&) = default;
			//IGraph(IGraph&&) = default;

			//virtual BlockBorder HECK_VECTORCALL computeStartBlock(ivec2 startPlotCoord, ivec2 startBlkPlotCoord) = 0;
			virtual BlockBorder HECK_VECTORCALL updateBlock(ivec2 blkCoord, BlockBorder blockBorder, unsigned int blockBorderEdgeDirtyBits) = 0;
			// Called after update.
			virtual int32_t HECK_VECTORCALL getGoalCost(ivec2 goalPlotCoord, ivec2 goalBlkCoord) const = 0;

			virtual Vector HECK_VECTORCALL getHLineHeuristic(ivec2 pathCoord) const = 0;
			virtual Vector HECK_VECTORCALL getVLineHeuristic(ivec2 pathCoord) const = 0;

			virtual bool isDebugPathingRequest() const noexcept = 0;

			virtual ~IGraph() = default;
		};

		explicit BlockAStar(ivec2 dim);

		void reset();

		// Adds a plot to the open set.
		void addStartBlock(ivec2 startBlkCoord);

		void updateOpenSetHeuristicValues(const IGraph& g);

		bool search(ivec2 goalPlotCoord, IGraph& graph, int32_t maxFCost = INT32_MAX);

		void dumpStats(std::ostream& os) const;

		void debugDraw(Image& img) const;

	private:
		

		using BlockHalfBorder = std::array<BorderEdge, 2>; // [ESide]
		
		struct OpenSetEntry
		{
			int32_t bestF{};
			i16vec2 blkCoord{};

			// TODO: Could do an alternative ordering that tie-breaks using the number of dirty bits in a block.
			friend bool operator<(const OpenSetEntry& a, const OpenSetEntry& b)
			{
				return a.bestF > b.bestF;
			}
		};

		struct BlockHalfBorderFCostPair
		{
			std::array<int32_t, 2> edges{ kUnreachableGlobalCost, kUnreachableGlobalCost }; // [ESide]
		};

		struct BlockState
		{
			// kUnreachableGlobalCost if not in heap.
			int32_t currentHeapValue = kUnreachableGlobalCost;
			// std::bitset is bigger than it needs to be.
			unsigned int dirtyEdgesMask{};

			unsigned int expandCount = 0;

			// Set if `reset` needs to clear this block.
			bool isInResetList = false;

			BlockHalfBorderFCostPair fcostValues{};
		};

		ivec2 mMapDim{};
		//ivec2 mStartPlotCoord{};
		DynamicArray2D<BlockHalfBorder> mBlockHalfBorders;

		DynamicArray2D<BlockState> mBlockStates;

		std::vector<OpenSetEntry> mOpenSetHeap;

		std::vector<i16vec2> mResetList;

		bool isBlkCoordInRange(ivec2 coord) const;
		int32_t calcBlockHeapValueFromBoundary(ivec2 blkCoord) const;
		void updateBlockHeapValueAndPush(i16vec2 blkCoord, int32_t heapValue);
		void updateBlockHeapValueAndPush(BlockState& blockState, i16vec2 blkCoord, [[maybe_unused]] int32_t newBestFCost, int32_t heapValue);
		int32_t calcHVectorHeapValue(const IGraph& map, ivec2 blkCoord) const;
		int32_t calcVVectorHeapValue(const IGraph& map, ivec2 blkCoord) const;
		void popHeap();
		void updateHeuristic(const IGraph& map);
		void invalidateAdjCardinal(ivec2 currentBlkCoord, unsigned int edgeDirtyBits, ESide side, IGraph& graph);

		void insertIntoResetList(ivec2 blkCoord);
	};
}