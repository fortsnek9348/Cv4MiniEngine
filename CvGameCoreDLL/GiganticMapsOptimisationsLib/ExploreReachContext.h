#pragma once

#include <CommonStuff/vec.h>

#include <memory>
#include <span>

class CvUnit;

namespace GiganticMapsOptimisationsLib
{
	class ExploreReachContext
	{
	public:
		ExploreReachContext();
		ExploreReachContext(ExploreReachContext&&) noexcept;
		ExploreReachContext& operator=(ExploreReachContext&&) noexcept;
		~ExploreReachContext();

		// This is one-shot!
		void visitAllReachable(const CvUnit& unit, uint32_t flags, int maxTurns);
		std::span<const heck::i16vec2> getVisitSet() const;
		[[nodiscard]] bool isVisited(heck::ivec2) const;
		heck::i16vec2 getEndTurnStopCoord(const CvUnit& unit, uint32_t flags, heck::ivec2 coord) const;

	private:
		struct Internals;
		std::unique_ptr<Internals> mInternals;
	};
}