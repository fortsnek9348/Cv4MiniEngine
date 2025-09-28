#pragma once

#include "Simd.h"
#include "aabb.h"
#include "CompilerMacros.h"
#include "DAryHeap.h"
#include "SimdAStarGraph.h"

#ifndef ENABLE_MODULES
#include <vector>
#else
import CommonStd;
#endif

namespace heck
{
	class PQSimdAStar
	{
	public:
		using IGraph = ISimdAStarGraph;

		static constexpr unsigned int kCoordVectorLength = IGraph::kCoordVectorLength;
		using Vector = IGraph::Vector;
		using StateVector = IGraph::StateVector;

		explicit PQSimdAStar(ivec2 dim);
		explicit PQSimdAStar(ivec2 dim, int plotI, uint32_t state);
		explicit PQSimdAStar(ivec2 dim, std::span<const int> plotIndices);

		// Resets entire object to same state as after construction.
		// This clears the internal visit state __without__ just completely reinitialising ~5MBs of memory (which is a lot if you have 3000 units to go through).
		// Returns the aabb of the visit set.
		i16aabb2 reset() noexcept;

		// Adds a plot to the open set.
		void addStart(int plotI, uint32_t state);
		// Adds multiple plots to the open set, with state = 0.
		void addStart(std::span<const int> plotIndices);

		void updateOpenSetHeuristicValues(const IGraph& g);

		bool search(int goalPlotI, const IGraph& g, int32_t maxCost = INT32_MAX);
		//void setHeuristicUpdateEnabled(bool);

		bool isVisited(int plotI) const
		{
			return mGCosts[plotI] < std::numeric_limits<std::int32_t>::max();
		}

		int getGCost(int plotI) const
		{
			return mGCosts[plotI];
		}

		uint32_t getGState(int plotI) const
		{
			return mGState[plotI];
		}

		std::optional<int> findMostDistantPlot() const;

		std::vector<i16vec2> reconstructPath(int startPlotI, int goalPlotI, const IGraph& g) const;

		void dumpStats(std::ostream& os) const;

		int getPitch() const
		{
			return mPitch;
		}

	private:
		//std::vector<int32_t> mHotQueuePlotIndices;
		//std::vector<int32_t> mHotQueueValues;

		DAryHeap2<int32_t, int32_t, 4> mHeap;
		//BHeap<int32_t, int32_t, 8> mHeap;
		//DAryHeap<int32_t, int32_t, 8> mHeap;
		//StdHeap<int32_t, int32_t> mHeap;

		

		std::vector<int32_t> mGCosts;
		std::vector<uint32_t> mGState;
		std::vector<int32_t> mParents;
		int mPitch = 0;
		int mStartPlotI = -1;

		std::vector<int> mStartPlots;

		unsigned int mNumVisited = 0;
		unsigned int mNumExpanded = 0;

		bool mHeuristicUpdateEnabled = false;

		void push(Vector adjVector, Vector adjFCosts, Vector::Mask adjPushMask);
		std::pair<Vector, Vector::Mask> pop();
		void updateHeuristic(const IGraph& g);
	};
}