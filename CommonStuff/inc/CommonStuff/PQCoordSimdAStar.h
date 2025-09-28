#pragma once

#include "Simd.h"
#include "aabb.h"
#include "CompilerMacros.h"
#include "DAryHeap.h"
#include "SimdAStarGraph.h"
#include "SimdMultiHeapAStarPriorityQueue.h"

#include <vector>
#include <cstdint>

namespace heck
{
	// "Coordinate" version of PQSimdAStar.
	// Coordinates are used instead of plot indices, to make wrapping implementable.
	// Coordinates are converted to indices when calling into the graph implementation.
	class PQCoordSimdAStar
	{
	public:
		using IGraph = ISimdAStarGraph;

		static constexpr unsigned int kCoordVectorLength = IGraph::kCoordVectorLength;
		static constexpr int32_t kUnreachableCost = std::numeric_limits<std::int32_t>::max();
		using Vector = IGraph::Vector;
		using StateVector = IGraph::StateVector;

		explicit PQCoordSimdAStar(ivec2 dim, bool wrapX, bool wrapY);

		// Resets entire object to same state as after construction.
		// This clears the internal visit state __without__ just completely reinitialising ~5MBs of memory (which is a lot if you have 3000 units to go through).
		// Returns the aabb of the visit set.
		i16aabb2 reset() noexcept;

		// Adds a plot to the open set.
		void addStart(ivec2 startCoord, uint32_t state);
		// Adds multiple plots to the open set, with state = 0.
		void addStart(std::span<const i16vec2> plotCoords);

		void updateOpenSetHeuristicValues(const IGraph& g);

		bool search(ivec2 goalPlotCoord, IGraph& g, int32_t maxCost = kUnreachableCost);

		bool isVisited(ivec2 coord) const
		{
			return getGCost(coord) < kUnreachableCost;
		}

		int getGCost(ivec2 coord) const
		{
			return mGCosts[coord.x + coord.y * mDim.x];
		}

		uint32_t getGState(ivec2 coord) const
		{
			return mGState[coord.x + coord.y * mDim.x];
		}

		// Note that parent pointers are not always enabled.
		int getParentRelAdj(ivec2 coord) const
		{
			if (mParents.empty())
				return -1;
			else
				return mParents[coord.x + coord.y * mDim.x];
		}

		std::optional<i16vec2> findMostDistantPlot() const;

		heck::ISimdAStarGraph::Step HECK_VECTORCALL getGCostsAndUnitStatesAroundPlot(ivec2 toCoord) const;

		void dumpStats(std::ostream& os) const;

	private:
		//DAryHeap2<int32_t, i16vec2, 4> mHeap;
		//SimdAStarPriorityQueueUsingStdHeap mHeap;
//#ifndef __clang__
		SimdAStarPriorityQueueUsingDAryHeap mHeap; // Use this one with MSVC
//#else
		//SimdAStarPriorityQueueUsingSimdHeap<kCoordVectorLength, 4, false> mHeap; // And this one with Clang. For slightly more performance.
//#endif
		//SimdMultiHeapAStarPriorityQueue mHeap;

		std::vector<int32_t> mGCosts;
		std::vector<uint32_t> mGState;
		ivec2 mDim{};
		bool mWrapX = false;
		bool mWrapY = false;

		std::vector<i16vec2> mStartPlots;

		std::vector<int32_t> mParents;

		//void HECK_VECTORCALL push(vec2<Vector> adjCoords, Vector adjFCosts, Vector::Mask adjPushMask);
		//std::pair<vec2<Vector>, Vector::Mask> HECK_VECTORCALL pop();
		void updateHeuristic(const IGraph& g);

		int toPlotIndex(ivec2 coord) const;
	};
}