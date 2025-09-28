#pragma once

#include <CommonStuff/Array2D.h>
#include <CommonStuff/vec.h>
#include <CommonStuff/MapGeometry.h>

class CvSelectionGroup;

namespace GiganticMapsOptimisationsLib
{
	// Uses live data to determine whether a goal is reachable.
	// This is intended to be used when no path is expected, so this uses a simple flood fill algorithm instead of a heuristic.
	class LivePathfinder
	{
	public:
		explicit LivePathfinder(heck::MapGeometry);

		void reset(heck::i16vec2 start);

		// Also checks pathDestValid.
		// Ignores goal-dependent costs.
		bool search(const CvSelectionGroup& group, uint32_t flags, heck::ivec2 goal, int32_t maxCost);

		std::optional<int> getGStateTurns(heck::ivec2 goal) const;
		
	private:
		heck::MapGeometry mMapGeom;

		static constexpr int32_t kUnreachable = INT32_MAX;

		struct Node
		{
			int32_t cost = kUnreachable;
			uint32_t state = 0;
		};

		struct Entry
		{
			heck::i16vec2 coord{};
			int32_t cost{};
		};

		heck::DynamicArray2D<Node> mNodes;
		std::vector<Entry> mQueue;
		heck::i16vec2 mStart{};
	};
}