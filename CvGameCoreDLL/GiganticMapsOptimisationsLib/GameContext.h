#pragma once

//#include "FoundValue/FoundValueSearchResult.h"
#include "GameEvents.h"

#include <CommonStuff/Array2D.h>
#include <CommonStuff/vec.h>

#include "../CvGameCoreDLL.h"
#include "../CvEnums.h"

#include <memory>
#include <optional>

class CvArea;
class CvPlot;
class CvSelectionGroup;
class CvUnit;
class CvCity;
class CvPlayerAI;
class CvPlayer;

namespace GiganticMapsOptimisationsLib
{
	class VectorisedPathfinderMap;

	class GameContext
	{
	public:
		// Map is assumed loaded. Reads the initial state of the map.
		GameContext();
		GameContext(const GameContext&) = delete;
		GameContext& operator=(const GameContext&) = delete;
		~GameContext() noexcept;

		// Called when the turn counter changes to perform garbage collection.
		void onGameTurn();

		// NOTE: Remember that the parallel unit update also changes game state!

		void onPlotChange(EGamePlotChangeEvent e, const CvPlot& plot, int oldValue, int newValue);
		void onPlotCultureChange(const CvPlot& plot, PlayerTypes culturePlayerI, int oldValue, int newValue);
		void onUnitChange(EGameUnitChangeEvent e, const CvUnit& unit);
		// This is thread-safe for unit MoveFrom and MoveTo, at least.
		// If unit had/has no position, then set X to any negative value.
		void onUnitMoved(const CvUnit& unit, heck::ivec2 from, heck::ivec2 to);
		void onTeamChange(EGameTeamChangeEvent e, TeamTypes teamA, int valueB);
		void onPlayerAliveChanged(PlayerTypes, bool alive);
		void onCapitalChanged(PlayerTypes, heck::ivec2 from, heck::ivec2 to);
		void onGlobalAreaCityCountChanged(const CvArea& area, int from, int to);
		void onPlayerAreaCityCountChanged(const CvArea& area, PlayerTypes playerI, int from, int to);
		void onIsAICitySiteChanged(const CvPlot& plot, PlayerTypes playerI);
		//void onEventChangedPlotBonus(const CvPlot& plot);
		void onChangePlayerCityRadiusCount(const CvPlot& plot, PlayerTypes playerI, int from, int to);

		// Call whenever the GC.getPathFinder() is potentially reset by vanilla code (ForceReset or !reuse).
		// Used to reset the internal verification FAStar.
		//void onUnitPathfinderReset();

		// Returns nullopt if unreachable or if maxTurns exceeded.
		std::optional<int> guessPathTurnsOneBased(const CvSelectionGroup& group, const CvPlot& dest, unsigned int pathingFlags, int maxTurns);

		// gDLL->getFAStarIFace()->GeneratePath drop-in. Will update FAStar state too.
		bool generatePath(const CvSelectionGroup& group, const CvPlot& start, const CvPlot& goal, unsigned int flags, int maxTurns, bool reuse);

		void resetPathfinder();

		//CvPlot* tryFindNextPathStep(const CvSelectionGroup& group, const CvPlot& dest);

		// For int CvMap::calculatePathDistance(CvPlot *pSource, CvPlot *pDest)
		// The simplest path caching. Depends only on CvAreas and plot domains.
		std::optional<int> findPathStepDistance(const CvPlot& start, const CvPlot& goal);

		struct ExploreSearchResult
		{
			CvPlot* endMissionPlot = nullptr;
			CvPlot* target = nullptr;
		};

		// Replacement of the whole-map search in `CvUnitAI::AI_explore`.
		ExploreSearchResult findExplorationTarget(const CvUnit& unit) const;

		// Find all plots reachable with `MOVE_NO_ENEMY_TERRITORY`. For `CvUnitAI::AI_exploreRange`.
		// Notable improvement for sea explorers, as this won't waste time trying to path to ocean plots for coast-hugging units.
		//std::vector<i16vec2> findExploreRangeTargets(const CvUnit& unit, int maxStepDist, int maxTurns);

		// Acceleration of CvTeamAI::AI_calculateAdjacentLandPlots.
		int getAdjacentLandPlotsCount(TeamTypes plotTeamI, TeamTypes adjTeamI) const;
		// Acceleration of CvPlayerAI::AI_calculateStolenCityRadiusPlots.
		int getStolenCityRadiusPlots(PlayerTypes cityPlayerI, PlayerTypes neighbourPlayerI) const;

		/// Found value system v1
		//void prepareForFoundValueQuerying(PlayerTypes playerI);
		//void enumerateHighestCityPlanningValues(
		//	int minFoundValue,
		//	PlayerTypes playerI,
		//	bool revealedOnly,
		//	const std::function<int(const CvArea&)>& getAreaValueScale,
		//	const std::function<bool(FoundValueSearchResult result)>& callback
		//);
		//int grabSpecificPlotFoundValue(PlayerTypes playerI, ivec2 plotCoord);
		//bool isAreaBestFoundValueAtLeast(const CvArea& area, PlayerTypes playerI, int iMinValue) const;

		/// Found value system v2
		void prepareForFoundValueQuerying(PlayerTypes playerI);
		// Call prepareForFoundValueQuerying first!
		// Used to replace AI_updateFoundValues. If non-revealed plots will be 0.
		std::vector<int> computeRevealedFoundValuesRow(PlayerTypes playerI, int y) const;
		std::vector<int> computeFoundValuesRow(PlayerTypes playerI, int y, int iMinRivalRange) const;

		// If verification is enabled, this will verify that the values matches AI_foundValue.
		void notifyFoundValueUsed(PlayerTypes player, heck::ivec2 coord, int iMinRivalRange, int value) const;


		void doAIParallelUnitUpdate(CvPlayerAI& player);

	private:
		struct Internals;
		std::unique_ptr<Internals> mInternals;

		// Used by doAIParallelUnitUpdate.
		VectorisedPathfinderMap& getVectorisedPathfinderMap();
	};

	// Thread-safe.
	int countTradeReachablePlots(PlayerTypes owner, int x, int y, int limit);

	// For int CvPlayerAI::AI_plotTargetMissionAIs(const CvPlot* pPlot, const MissionAITypes* aeMissionAI, int iMissionAICount, int& iClosestTargetRange, const CvSelectionGroup* pSkipSelectionGroup, int iRange) const
	// `AI_plotTargetMissionAIs` is super slow because it always enumerates every group for every plot.
	// So do it once.
	// Results are clamped to UINT8_MAX.
	heck::DynamicArray2D<uint8_t> buildAIPlotTargetMap(const CvPlayer& player, std::span<const MissionAITypes> missionAIs, const CvSelectionGroup* pSkipSelectionGroup, int iRange);
}