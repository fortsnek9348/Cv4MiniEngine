#pragma once

#include <PlayerBotGameBinding/DLLDefs.h>
#include <PlayerBotGameBinding/GameStructs.h>

#include <cassert>
#include <vector>

namespace mybot
{
	using cvbot::Span2D;
	using cvbot::ivec2;
	using cvbot::i16vec2;
	using cvbot::MapGeometry;

	struct PathingPlot
	{
		enum EFlag : uint8_t
		{
			Unrevealed          /**/ = 1 << 0,
			EnemyUnit           /**/ = 1 << 1,
			NoGoTerritory       /**/ = 1 << 2,
			Water               /**/ = 1 << 3,
			Land                /**/ = 1 << 4,
			NoGoForCoastalUnits /**/ = 1 << 5,
			Impassible          /**/ = 1 << 6,
		};

		uint8_t flags = 0;
	};

	enum class EDistanceMetric
	{
		Step, // ::stepDistance, chessboard distance
		Culture, // ::plotDistance, CvCity::cultureDistance
	};

	unsigned int computeDistance(const MapGeometry& geom, ivec2 a, ivec2 b, EDistanceMetric metric) noexcept;

	template<class T>
	struct DynamicArray2D
	{
		std::vector<T> cells;
		ivec2 dim{};

		DynamicArray2D() = default;

		explicit DynamicArray2D(ivec2 dim) : cells(dim.x * dim.y), dim(dim)
		{
		}

		explicit DynamicArray2D(ivec2 dim, T def) : cells(dim.x * dim.y, def), dim(dim)
		{
		}

		Span2D<const T> view() const noexcept
		{
			return { cells.data(), std::dextents<int, 2>{ dim.y, dim.x } };
		}

		Span2D<T> view() noexcept
		{
			return { cells.data(), std::dextents<int, 2>{ dim.y, dim.x } };
		}

		T& operator[](ivec2 p)
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return cells[p.x + p.y * dim.x];
		}

		const T& operator[](ivec2 p) const
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return cells[p.x + p.y * dim.x];
		}
	};

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