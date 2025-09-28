#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "LivePathfinder.h"
#include "UnitPathingUtil.h"
#include "VectorisedPathStep.h"

#include "../CvUnit.h"

#include <algorithm>

using namespace GiganticMapsOptimisationsLib;

LivePathfinder::LivePathfinder(heck::MapGeometry mapGeom) : mMapGeom(mapGeom), mNodes(mapGeom.dim)
{
}

void LivePathfinder::reset(heck::i16vec2 start)
{
	mQueue.clear();
	mQueue.push_back({ mStart });
	mNodes[mStart] = {};
	mStart = start;

	while (!mQueue.empty())
	{
		const ivec2 coord = mQueue.back().coord;
		mQueue.pop_back();

		for (const ivec2 adj : kAdj)
		{
			if (const auto optAdjCoord = mMapGeom.tryCanonicalise(coord + adj))
			{
				if (mNodes[*optAdjCoord].cost < kUnreachable)
				{
					mNodes[*optAdjCoord] = {};
					mQueue.push_back({ i16vec2(*optAdjCoord) });
				}
			}
		}
	}
}

bool LivePathfinder::search(const CvSelectionGroup& group, uint32_t flags, heck::ivec2 goal, int32_t maxCost)
{
	if (!calc_pathDestValid(group, flags, goal))
		return false;

	if (mNodes[goal].cost <= std::min(maxCost, kUnreachable - 1))
		return true;

	const heck::MapGeometry mapGeom = mMapGeom;

	constexpr ivec2 kNoGoal{ -1 };

	while (!mQueue.empty() && mQueue.front().cost <= maxCost)
	{
		const Entry from = mQueue.front();

		if (from.coord == i16vec2(goal))
			return true;

		std::ranges::pop_heap(mQueue, std::greater<>(), &Entry::cost);
		mQueue.pop_back();

		const int fromCost = mNodes[from.coord].cost;
		const uint32_t fromState = mNodes[from.coord].state;
		const auto [fromMP, fromTurns] = VectorisedPathStepFunction::splitMPTurns(fromState);

		for (const ivec2 adj : kAdj)
		{
			if (const auto optToCoord = mapGeom.tryCanonicalise(ivec2(from.coord) + adj))
			{
				if (calc_pathValid(group, mStart, flags, fromTurns, fromMP, from.coord, *optToCoord))
				{
					// Ignoring goal-dependent costs.
					const int stepCost = calc_pathCost(group, flags, fromTurns, fromMP, from.coord, *optToCoord, kNoGoal);
					const int toCost = fromCost + stepCost;
					if (toCost < mNodes[*optToCoord].cost)
					{
						const auto [toMP, toTurns] = calc_pathAdd(group, flags, fromTurns, fromMP, from.coord, *optToCoord);
						mNodes[*optToCoord] = { .cost = toCost, .state = VectorisedPathStepFunction::joinMPTurns(toMP, toTurns) };
						mQueue.push_back({ *optToCoord, toCost });
						std::ranges::push_heap(mQueue, std::greater<>(), &Entry::cost);
					}
				}
			}
		}
	}

	return false;
}

std::optional<int> LivePathfinder::getGStateTurns(heck::ivec2 goal) const
{
	if (mNodes[goal].cost < kUnreachable)
		return VectorisedPathStepFunction::splitMPTurns(mNodes[goal].state).second;
	else
		return std::nullopt;
}

#endif