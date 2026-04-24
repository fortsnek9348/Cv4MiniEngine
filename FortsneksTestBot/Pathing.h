#pragma once

#include "Common.h"
#include "DynamicArray2D.h"

#include <PlayerBotGameBinding/DLLDefs.h>
#include <PlayerBotGameBinding/GameStructs.h>

#include <cassert>
#include <vector>

namespace mybot
{
	struct PathingPlot
	{
		enum EFlag : uint16_t
		{
			Unrevealed              /**/ = 1 << 0,
			EnemyUnit               /**/ = 1 << 1,
			NoGoTerritory           /**/ = 1 << 2,
			Water                   /**/ = 1 << 3,
			Land                    /**/ = 1 << 4,
			NoGoForOurCoastalUnits  /**/ = 1 << 5, // Avoid open sea, land (except friendly cities). Does not avoid unrevealed or impassible.
			Impassible              /**/ = 1 << 6,
			NoGoForBarbCoastalUnits /**/ = 1 << 7, // Same as NoGoForOurCoastalUnits, but for kBarbarianPlayer.
			OwnedByOtherPlayer      /**/ = 1 << 8,
		};

		uint16_t flags = 0;
	};

	enum class EDistanceMetric
	{
		Step, // ::stepDistance, chessboard distance
		Culture, // ::plotDistance, CvCity::cultureDistance
	};

	unsigned int computeDistance(const MapGeometry& geom, ivec2 a, ivec2 b, EDistanceMetric metric) noexcept;

	

	struct PathingMap
	{
		MapGeometry geom;
		Span2D<const PathingPlot> plots;
	};

	struct alignas(uint32_t) MultipleSourceDistanceFieldCell
	{
		uint16_t distance = UINT16_MAX;
		uint16_t index{};

		bool isReachable() const
		{
			return distance < UINT16_MAX;
		}
	};

	DynamicArray2D<MultipleSourceDistanceFieldCell> computeMultipleSourcePathLengthField(PathingMap map, PathingPlot::EFlag avoidFlags, int maxDistance, std::span<const ivec2> starts);

	DynamicArray2D<MultipleSourceDistanceFieldCell> computeMultipleSourceDistanceField(PathingMap map, EDistanceMetric metric, std::span<const ivec2> starts);
	// 0 == unreachable
	// >0 == one of the reachable connected components
	DynamicArray2D<unsigned int> computeGlobalReachability(PathingMap map, PathingPlot::EFlag avoidFlags);
}