#pragma once

#include "Util.h"

#include <CommonStuff/BlockAStar.h>

class CvSelectionGroup;

namespace GiganticMapsOptimisationsLib
{
	class VectorisedPathfinderMap;

	class IVectorisedPathfinder
	{
	public:
		virtual void setConfig(const CvSelectionGroup* group, uint32_t flags) = 0;

		// Wombo combo function to both reset and get the bounding rect of the visit set.
		// This is because the rect can be found during reset.
		// The bounding rect is of all visited plots where pathValid is false.
		// Intended to be used to find which plots, that when you toggle from invalid to valid, the path may change.
		// The "valid rect" is just the bounding rect of the path.
		// For clamped axes: 0 <= min <= max <= dim
		// For wrapped axes: 0 <= min <= max <= dim * 2
		// Returned rect may include more plots than required.
		// Pass `0` as the mask to avoid computing rect.
		virtual i16aabb2 reset(uint32_t visitRectPlotPropsMask) = 0;

		virtual void start(ivec2 startCoord, uint32_t startState) = 0;

		// Call one of these first!
		virtual void setHeuristicGoal(ivec2 goal) = 0;
		virtual void setHeuristicGoalTo3x3Around(ivec2 goal) = 0;

		virtual void updateOpenSetHeuristicValues() = 0;

		// Expand open set until goal has a G cost assigned or until F cost limit is reached.
		virtual bool search(ivec2 goal, int maxFCost, bool isDebugPathingRequest) = 0;

		virtual std::vector<i16vec2> reconstructPath(ivec2 start, ivec2 goal) const = 0;

		virtual int32_t getGCostAt(ivec2 coord) const noexcept = 0;
		virtual uint32_t getUnitStateAt(ivec2 coord) const noexcept = 0;

		virtual uint32_t getTeamCostPlotPropsReadMask() const noexcept = 0;
		virtual uint32_t getTeamValidityPlotPropsReadMask() const noexcept = 0;

		// Thread-safe.
		virtual void liteVerifyAt([[maybe_unused]] ivec2 coord, [[maybe_unused]] uint32_t state) const {}


		virtual void debugDump() const = 0;

		virtual ~IVectorisedPathfinder() = default;
	};

	std::unique_ptr<IVectorisedPathfinder> createPlotwiseAStarVectorisedPathfinder(VectorisedPathfinderMap& map);
	std::unique_ptr<IVectorisedPathfinder> createBlockAStarVectorisedPathfinder(VectorisedPathfinderMap& map);
	std::unique_ptr<IVectorisedPathfinder> createCoordPlotwiseAStarVectorisedPathfinder(VectorisedPathfinderMap& map);
}