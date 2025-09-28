#pragma once

#include "Util.h"

#include <CommonStuff/SimdAStarGraph.h>

#include "../CvEnums.h"

class CvUnit;
class CvMap;
class CvSelectionGroup;

namespace GiganticMapsOptimisationsLib
{
	inline constexpr int PATH_MOVEMENT_WEIGHT = 1000;
	inline constexpr int PATH_RIVER_WEIGHT = 100;
	inline constexpr int PATH_CITY_WEIGHT = 100;
	inline constexpr int PATH_DEFENSE_WEIGHT = 10;
	inline constexpr int PATH_TERRITORY_WEIGHT = 3;
	inline constexpr int PATH_STEP_WEIGHT = 2;
	inline constexpr int PATH_STRAIGHT_WEIGHT = 1;

	// Hard coded for fixed array sizes.
	inline constexpr unsigned int kNumRouteTypes = 2;

	inline constexpr TerrainTypes kOceanTerrainType = std::bit_cast<TerrainTypes>(6);

	// Replication of vanilla functions. Used to verify path finder.
	bool calc_pathDestValid(const CvSelectionGroup& group, unsigned int flags, ivec2 goal);
	// NOTE: Only really reliable for the single unit case. Vanilla function is bugged for groups.
	int calc_pathCost(const CvSelectionGroup& group, unsigned int flags, int fromTurns, int fromMP, ivec2 from, ivec2 to, ivec2 goal);
	bool calc_pathValid(const CvSelectionGroup& group, ivec2 start, unsigned int flags, int fromTurns, int fromMP, ivec2 from, ivec2 to);
	bool calc_pathFromValid(const CvSelectionGroup& group, ivec2 start, unsigned int flags, int fromTurns, int fromMP, ivec2 from);

	// Moves:Turns
	std::pair<int, int> calc_pathAdd(const CvSelectionGroup& group, unsigned int flags, int fromTurns, int fromMP, ivec2 from, ivec2 to);

	bool getSingleUnit_AI_isDeclareWar(const CvUnit& unit, const CvPlot* pPlot);
	bool getSingleUnit_AI_isDeclareWar(const CvUnit& unit, TeamTypes plotTeam);

	enum class [[nodiscard]] EUnitWarAIType
	{
		False,
		Limited,
		True,
	};

	EUnitWarAIType getSingleUnitWarAIType(const CvUnit& unit);

	std::array<int, kNumRouteTypes> getTeamRouteChanges(TeamTypes teamI);

	// The heuristic and the pathfinder lookup route costs using selection bits.
	// Index is formed from bit-or of route masks of adjacent plots.
	// Array is repeated to SimdAStar vector length so that masking isn't needed for permute.
	// Indexed as [fromRouteType | (toRouteType << 1)]
	std::array<int32_t, heck::ISimdAStarGraph::kCoordVectorLength> calcRouteCostCombinations(std::array<int, kNumRouteTypes> teamRouteChanges, int maxMP);

	// Used to limit A* pathfinding by limiting the max F cost.
	// Alternatively, you could limit by state value, but the requires going the whole open set.
	// This can also be checked against the heuristic.
	int calcPathCostUpperBound(const CvSelectionGroup& group, int numTurns);

	int calcPathCostLowerBoundZeroBasedTurns(const CvUnit& unit, int pathCostLowerBound);

	int calcZeroBasedMinTurnsForStepDistance(const CvUnit& unit, int dist);
}