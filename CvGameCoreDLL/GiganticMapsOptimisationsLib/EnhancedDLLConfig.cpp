#include "EnhancedDLLConfig.h"

#include <array>

#define INVARIANT "This option should not change game state checksums."
#define NEEDS_NEW_PATHFINDER "This option only has an effect if VectorisedPathfinder is enabled."

// NOTE: Even if the AI proc values plots the same way as vanilla code, it may still yield a different game state if the path requests are at all different due to tie breaks and path cost quirks.
//       But if the only change is a path limit, then that change has no effect if using FAStar.
#define INVARIANT_WITH_FASTAR "This option should not change game state checksums, unless VectorisedPathfinder is enabled."

static constexpr auto kOptionInfos = std::to_array<EnhancedDLLConfig::OptionInfo>({
	// Used to be ENABLE_GAMECOREDLL_FIXES
	{ "GeneralFixes", "Enables fixes for city updates, plot grouping, pathfinder usage, and enables the FAStar heuristic update. These fixes are important for testing." },
	//{ "OrderIndependentRNG", "Changes various RNG uses to be written in a way that a parallel loop can evaluate. Useful for testing. Will change game state." },
	// ENABLE_GAMECOREDLL_ENHANCEMENTS path finder
	{ "VectorisedPathfinder", "Enables the fast vectorised pathfinder. This is a large bug-prone pile of code, but is so much faster than FAStar. Benefits from AVX-512. Only used for AI units." },
	// ENABLE_GAMECOREDLL_ENHANCEMENTS, ENABLE_GIGANTIC_MAPS_FOUND_VALUE_OPTIMISATIONS
	// This is not quite game hash preserving because CvGame::createBarbarianCities uses a parallel RNG.
	{ "VectorisedFoundValues", "Enables fast vectorised evaluation of found values. This is a large bug-prone pile of code, but is so much faster than uncached scalar found value evaluation. Benefits from AVX-512." },
	// ENABLE_PlayerTradeNetworkSystem.
	// This does not preserve game state, unless you compare with GeneralFixes enabled.
	{ "BlockwisePlotGroupSystem", "Enables the blockwise union-find-based plot group system. This is a large bug-prone pile of code, but is so much faster than the original CvPlotGroup system." },

	{ "ParallelBarbarianUnitUpdate", "Enables parallel unit updates for barbarians only. This is a large bug-prone pile of code, but makes gigantic maps much more practical when you have barbs enabled. Will parallelise \"simple\" unit AI and fallback to serial for everything else. " NEEDS_NEW_PATHFINDER },

	// CvGame
	{ "HardCapBarbSpawns", "For gigantic maps, especially on Deity, barbs can easily exceed unit ID allocation capacity. This works around that by capping barbs to 4000." },

	// CvCity
	{ "CvCity_doTurnParallel", "Enables a small parallel city update. Only does AI_updateRouteToCity for now. " INVARIANT },
	// CvCityAI
	{ "Faster_CvCityAI_AI_updateRouteToCity", INVARIANT },
	
	// CvMap
	{ "Fast_CvMap_calculatePathDistance", "A faster algorithm for computing the simple path step distance between plots." },
	// CvPlayer
	{ "Fast_CvPlayer_updateGroupCycle", "Relaxes \"group cycle\" ordering for AI, to speed up this function." },
	{ "Parallel_CvPlayer_countUnimprovedBonuses", "Basic whole-map parallel loop. " INVARIANT },
	{ "Skip_AI_CvPlayer_doWarnings", "Avoid doing things that only a human player cares about. " INVARIANT },
	// CvPlayerAI
	{ "Parallel_CvPlayerAI_AI_findTargetCity", "Parallel loop over cities. Also calls CvMap::calculatePathDistance in parallel, so needs Fast_CvMap_calculatePathDistance." },
	{ "Fast_CvPlayerAI_AI_calculateStolenCityRadiusPlots", "Use a caching system to accelerate queries. " INVARIANT },
	{ "Parallel_CvPlayerAI_AI_updateCitySites", "Basic whole-map parallel loop. " INVARIANT },
	{ "Parallel_CvPlayerAI_AI_updateFoundValues_for_bStartingLoc", "Basic whole-map parallel loop. " INVARIANT },
	// CvPlot
	{ "Fast_CvPlot_updateIrrigated", "Use a fast flood-fill algorithm. " INVARIANT },
	{ "Fast_CvPlot_isCoastalLand", "Cache this value. " INVARIANT },
	// CvPlotGroup
	{ "Fast_CvPlotGroup_recalculatePlots", "Use a faster flood-fill algorithm to count plots instead of FAStar. Only has an effect if BlockwisePlotGroupSystem is disabled. " INVARIANT },
	// CvSelectionGroup
	{ "PathLimit_CvSelectionGroup_groupAttack", "This AI procedure expects a short path, so use a path limit. " INVARIANT_WITH_FASTAR },
	// CvTeamAI
	{ "Fast_CvTeamAI_AI_calculateAdjacentLandPlots", "Use a caching system to accelerate queries. " INVARIANT },
	// CvUnitAI
	{ "PathLimit_CvUnitAI_AI_group", "Trivial path limit. " INVARIANT_WITH_FASTAR },
	{ "PathLimit_CvUnitAI_AI_guardCityMinDefender", "Trivial path limit. " INVARIANT_WITH_FASTAR },
	{ "PathLimit_CvUnitAI_AI_guardCity", "Trivial path limit. " INVARIANT_WITH_FASTAR },
	{ "Parallel_CvUnitAI_AI_guardBonus", "Whole-map parallel loop with distance sort and path limit." },
	{ "Parallel_CvUnitAI_AI_guardFort", "Whole-map parallel loop with distance sort and path limit." },
	{ "PathLimit_CvUnitAI_AI_spreadReligion", "Non-trivial path limit." },
	{ "Faster_CvUnitAI_AI_construct", "Rearranges logic to perform less pathfinding." }, // This can't be invariant because it changes pathfinding.
	{ "Faster_CvUnitAI_AI_protect", "Loop over units instead of plots." }, // This can't be invariant because it changes pathfinding.
	{ "Localised_CvUnitAI_AI_explore", "Perform a localised search instead of a whole-map loop. This is an approximation of the original exploration AI." },
	// TODO: Could this be changed to just a path limit?
	// This is not currently invariant as the "next step" plot may be different due to different pathing.
	{ "Localised_CvUnitAI_AI_exploreRange", "Perform a flood-fill to find reachable plots instead of using a pathfinder." },
	{ "Faster_CvUnitAI_AI_targetCity", "Sorts cities by distance and use a path limit." },
	{ "PathLimit_CvUnitAI_AI_targetBarbCity", "Trivial path limit. " INVARIANT_WITH_FASTAR },
	{ "Parallel_CvUnitAI_AI_pirateBlockade", "Non-trivial parallelisation  of privateer AI. May not be all that effective. Privateer pathing is almost worst case scenario." },
	{ "Parallel_CvUnitAI_AI_improveBonus", "Whole-map parallel loop to filter pathing targets." },
	{ "Faster_CvUnitAI_AI_retreatToCity", "Sorts cities by distance and use a path limit." },
	{ "PathLimit_CvUnitAI_AI_trade", "Sorts cities by distance and use a path limit." },
	{ "PathLimit_CvUnitAI_AI_getEspionageTargetValue", "Trivial path limit. " INVARIANT_WITH_FASTAR },
	{ "PathLimit_CvUnitAI_AI_moveToStagingCity", "Sorts cities by distance and use a path limit." },
	{ "Faster_CvUnitAI_AI_pillage", "Sorts cities by distance and use a path limit." },
	});

static_assert(kOptionInfos.size() == (int)EEnhancedDLLConfigOption::Num);

EnhancedDLLConfig::OptionInfo EnhancedDLLConfig::getOptionInfo(EEnhancedDLLConfigOption i)
{
	return kOptionInfos[static_cast<size_t>(i)];
}
