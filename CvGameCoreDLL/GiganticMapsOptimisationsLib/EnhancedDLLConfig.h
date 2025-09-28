#pragma once

// This stuff must always be defined for the engine to use.
//#if ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "../CommonConfig.h"

#include <bitset>
#include <utility>

// fortsnek: Cv4MiniEngine DLL enhancements are controlled through the struct below.

// Builds:
// Strict: All enchancements disabled. Game state checksums should match Firaxis DLL.
// Enhanced: All enchancements can be configured at runtime.
// This lets BlockwisePlotGroupSystem be runtime configurable, and still keep the original CvPlotGroup CvPlot API.

enum class EEnhancedDLLConfigOption
{
	GeneralFixes, // ENABLE_GAMECOREDLL_FIXES. Includes fixes for plot grouping, pathfinder usage, and the FAStar heuristic update fix.
	//OrderIndependentRNG,
	VectorisedPathfinder, // ENABLE_GAMECOREDLL_ENHANCEMENTS path finder
	VectorisedFoundValues, // ENABLE_GAMECOREDLL_ENHANCEMENTS, ENABLE_GIGANTIC_MAPS_FOUND_VALUE_OPTIMISATIONS
	BlockwisePlotGroupSystem, // ENABLE_PlayerTradeNetworkSystem.
	ParallelBarbarianUnitUpdate,
	// CvGame
	HardCapBarbSpawns,
	// CvCity
	CvCity_doTurnParallel, // Only does AI_updateRouteToCity for now.
	// CvCityAI
	Faster_CvCityAI_AI_updateRouteToCity,
	// CvMap
	Fast_CvMap_calculatePathDistance,
	// CvPlayer
	Fast_CvPlayer_updateGroupCycle,
	Parallel_CvPlayer_countUnimprovedBonuses,
	Skip_AI_CvPlayer_doWarnings,
	// CvPlayerAI
	Parallel_CvPlayerAI_AI_findTargetCity,
	Fast_CvPlayerAI_AI_calculateStolenCityRadiusPlots,
	Parallel_CvPlayerAI_AI_updateCitySites,
	Parallel_CvPlayerAI_AI_updateFoundValues_for_bStartingLoc,
	// CvPlot
	Fast_CvPlot_updateIrrigated,
	Fast_CvPlot_isCoastalLand,
	// CvPlotGroup
	Fast_CvPlotGroup_recalculatePlots,
	// CvSelectionGroup
	PathLimit_CvSelectionGroup_groupAttack,
	// CvTeamAI
	Fast_CvTeamAI_AI_calculateAdjacentLandPlots,
	// CvUnitAI
	PathLimit_CvUnitAI_AI_group,
	PathLimit_CvUnitAI_AI_guardCityMinDefender,
	PathLimit_CvUnitAI_AI_guardCity,
	Parallel_CvUnitAI_AI_guardBonus,
	Parallel_CvUnitAI_AI_guardFort,
	PathLimit_CvUnitAI_AI_spreadReligion,
	Faster_CvUnitAI_AI_construct,
	Faster_CvUnitAI_AI_protect,
	Localised_CvUnitAI_AI_explore,
	Localised_CvUnitAI_AI_exploreRange,
	Faster_CvUnitAI_AI_targetCity,
	PathLimit_CvUnitAI_AI_targetBarbCity,
	Parallel_CvUnitAI_AI_pirateBlockade,
	Parallel_CvUnitAI_AI_improveBonus,
	Faster_CvUnitAI_AI_retreatToCity,
	PathLimit_CvUnitAI_AI_trade,
	PathLimit_CvUnitAI_AI_getEspionageTargetValue,
	PathLimit_CvUnitAI_AI_moveToStagingCity,
	Faster_CvUnitAI_AI_pillage,

	Num,
};

struct EnhancedDLLConfig
{
	std::bitset<std::to_underlying(EEnhancedDLLConfigOption::Num)> options{};

	struct OptionInfo
	{
		std::string_view iniName;
		std::string_view iniDesc;
	};

	DllExport static OptionInfo getOptionInfo(EEnhancedDLLConfigOption);

	[[nodiscard]] bool operator[](EEnhancedDLLConfigOption i) const noexcept
	{
		return options[static_cast<size_t>(i)];
	}
};

//#endif