#pragma once

#include <CommonStuff/vec.h>

#include <span>
#include <memory>

class CvSelectionGroup;

namespace GiganticMapsOptimisationsLib
{
	// This visits the whole map with uniform cost pathing and live pathValid data.
	// So turns is `steps * (movement cost / 60)`.
	class WholeMapSimpleDijkstra
	{
	public:
		WholeMapSimpleDijkstra();
		WholeMapSimpleDijkstra(WholeMapSimpleDijkstra&&) noexcept;
		WholeMapSimpleDijkstra& operator=(WholeMapSimpleDijkstra&&) noexcept;
		~WholeMapSimpleDijkstra();

		void visitAllReachable(const CvSelectionGroup& group, heck::ivec2 start, uint32_t flags);
		bool isReachableGoal(heck::ivec2 coord) const; // Checks pathDestValid too.
		int getPathStepDistance(heck::ivec2 coord) const; // Note that steps is capped to UINT16_MAX.

	private:
		struct Internals;
		std::unique_ptr<Internals> mInternals;
	};
}