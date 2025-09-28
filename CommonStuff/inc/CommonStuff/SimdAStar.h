#pragma once

#include "Simd.h"
#include "vec.h"
#include "CompilerMacros.h"
#include "SimdAStarGraph.h"

#include <vector>

namespace heck
{
	class SimdAStar
	{
	public:
		using IGraph = ISimdAStarGraph;

		static constexpr unsigned int kCoordVectorLength = IGraph::kCoordVectorLength;
		using Vector = IGraph::Vector;
		using StateVector = IGraph::StateVector;

		explicit SimdAStar(ivec2 dim, int startPlotI, uint32_t initialState);
		explicit SimdAStar(ivec2 dim, std::span<const int> startPlotIndices);

		//void reuseForNonOverlappingVisitSet(int startPlotI);

		bool search(int goalPlotI, const IGraph& g, int32_t costLimit = INT32_MAX);
		bool searchUsingPriorityQueue(int goalPlotI, const IGraph& g);

		void setHeuristicUpdateEnabled(bool);

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

		std::optional<int> findMostDistancePlot() const;

		std::vector<int> reconstructPath(int goalPlotI, const IGraph& g) const;

		void dumpStats(std::ostream& os) const;

	private:
		static constexpr int kNumHotQueues = 8;

		struct HotQueue
		{
			std::vector<int32_t> plots;
			size_t offset = 0;

			HotQueue();
			std::pair<Vector, Vector::Mask> at(size_t i) const;
			std::pair<Vector, Vector::Mask> pop();
			//void push(Vector plotIndices, Vector::Mask mask);
			explicit operator bool() const;
		};

		int32_t mFirstHotQueueGranularCost = 0;
		unsigned int mNumActiveQueues = 0;
		std::array<HotQueue, kNumHotQueues> mHotQueues;
		std::vector<int32_t> mGCosts;
		std::vector<uint32_t> mGState;
		std::vector<int32_t> mParents;
		int mPitch = 0;
		int mStartPlotI = 0;

		bool mHeuristicUpdateEnabled = true;

		unsigned int mNumOneQueuePushes = 0;
		unsigned int mNumThreeQueuePushes = 0;
		unsigned int mNumManualPushes = 0;
		std::array<unsigned int, kNumHotQueues> mFCostQuantRangeCounts{};
		std::array<unsigned int, kCoordVectorLength + 1> mIterationMaskPopCounts{};
		unsigned int mMaxDuplication = 0;

		

		void initQueues();
		void push(Vector adjVector, Vector adjFCosts, Vector::Mask adjPushMask);
		void updateHeuristic(const IGraph& g);
	};
}