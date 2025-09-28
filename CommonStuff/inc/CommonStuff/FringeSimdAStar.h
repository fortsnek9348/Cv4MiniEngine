#pragma once

#include "Simd.h"
#include "vec.h"
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
	class FringeSimdAStar
	{
	public:
		using IGraph = ISimdAStarGraph;

		static constexpr unsigned int kCoordVectorLength = IGraph::kCoordVectorLength;
		using Vector = IGraph::Vector;
		using StateVector = IGraph::StateVector;

		explicit FringeSimdAStar(ivec2 dim, int startPlotI);
		explicit FringeSimdAStar(ivec2 dim, std::span<const int> startPlotIndices);

		bool search(int goalPlotI, const IGraph& g);

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
		struct Entry
		{
			int32_t fcost = 0;
			int32_t plotI = 0;

			bool operator<(Entry b) const
			{
				return fcost < b.fcost;
			}
		};
		std::vector<Entry> mHeap;

		std::vector<int32_t> mGCosts;
		std::vector<uint32_t> mGState;
		int mPitch = 0;
		int mStartPlotI = 0;

		unsigned int mNumVisited = 0;
		unsigned int mNumExpanded = 0;

		void updateHeuristic(const IGraph& g);
	};
}