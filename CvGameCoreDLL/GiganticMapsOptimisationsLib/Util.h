#pragma once

//#include "LongRangeAStar.h"

#include <CommonStuff/aabb.h>
#include <CommonStuff/div.h>
#include <CommonStuff/range.h>
#include <CommonStuff/Array2D.h>
#include <CommonStuff/Bitmap2D.h>
#include <CommonStuff/PPM.h>
#include <CommonStuff/SimdAStarGraph.h>
#include <CommonStuff/MapGeometry.h>
#include <CommonStuff/Hashing.h>

#include "../CvGameAI.h"
#include "../CvGlobals.h"
#include "../CvMap.h"
#include "../CvGameCoreUtils.h"

#include <array>
#include <shared_mutex>
#include <chrono>

class CvSelectionGroup;
class CvRandom;

namespace GiganticMapsOptimisationsLib
{
	using heck::ivec2;
	using heck::iaabb2;
	using heck::i16vec2;
	using heck::i16aabb2;
	using heck::range;
	using heck::Array2D;
	using heck::DynamicArray2D;
	using heck::Bitmap2D;
	using heck::DynamicBitmap2D;
	using heck::unpackCoords;
	using heck::packCoords;
	using heck::VectorHasher;
	using heck::VectorComparator;

	using Clock = std::chrono::steady_clock;
	using Microseconds = std::chrono::duration<double, std::micro>;
	using Milliseconds = std::chrono::duration<double, std::milli>;
	using Seconds = std::chrono::duration<double>;

	inline constexpr int kMaxCacheTurns = 2;

	inline constexpr RouteTypes kRouteType_Road = RouteTypes(0);
	inline constexpr RouteTypes kRouteType_Railroad = std::bit_cast<RouteTypes>(1);

	inline constexpr int MOVE_DENOMINATOR = 60;

	using heck::kAdj;

	constexpr bool isDiagAdj(int adjI)
	{
		return bool((0b10100101 >> adjI) & 1);
	}

	inline constexpr DirectionTypes kAdjIndexToDirectionType[]{
		DIRECTION_SOUTHWEST,
		DIRECTION_SOUTH,
		DIRECTION_SOUTHEAST,
		DIRECTION_WEST,
		DIRECTION_EAST,
		DIRECTION_NORTHWEST,
		DIRECTION_NORTH,
		DIRECTION_NORTHEAST,
	};

	/*template<class Map>
	struct TurnCache
	{
		static_assert(kMaxCacheTurns >= 2);

		int firstTurn = 0;
		std::array<Map, kMaxCacheTurns> maps;

		std::shared_mutex mutex;

		auto& grab(const auto& key, auto calcValue)
		{
			const int turn = GC.getGame().getGameTurn();

			{
				const std::shared_lock sharedLock(mutex);
				if (const auto it = maps.back().find(key); it != maps.back().end())
					return it->second;
			}

			const std::lock_guard lock(mutex);

			while (turn > firstTurn + kMaxCacheTurns)
			{
				maps[0].clear();
				std::rotate(maps.begin(), maps.begin() + 1, maps.end());
				++firstTurn;
			}

			if (turn < firstTurn)
			{
				maps = {};
				firstTurn = -kMaxCacheTurns + 1;
			}

			for (const int i : range(kMaxCacheTurns - 1))
				if (const auto it = maps[i].find(key); it != maps[i].end())
					return maps.back().insert(maps[i].extract(it)).position->second; // Transfer element to current turn.

			struct Constructor
			{
				decltype(calcValue)& calcValue;
			
				operator auto() const
				{
					return calcValue();
				}
			};
			
			return maps.back().try_emplace(key, Constructor{ calcValue }).first->second;

			//if (const auto it = maps.back().find(key); it != maps.back().end())
			//	return it->second;
			//else
			//	return maps.back().emplace(key, calcValue()).first->second;

		}

		void erase_if(auto predicate)
		{
			const std::lock_guard lock(mutex);
			for (auto& map : maps)
				std::erase_if(map, predicate);
		}
	};*/

	struct VisitSet
	{
		static constexpr int kBlockDim = 16;

		using Block = std::bitset<kBlockDim * kBlockDim>;

		std::unordered_map<int, Block> blockMap;
		int widthBlocks = (GC.getMapINLINE().getGridWidthINLINE() + kBlockDim - 1) / kBlockDim; // round up

		Block* prevBlock = nullptr;
		int prevBlkX = -1;
		int prevBlkY = -1;

		bool test(int x, int y)
		{
			const int blkX = x / kBlockDim;
			const int blkY = y / kBlockDim;
			const int subX = x % kBlockDim;
			const int subY = y % kBlockDim;
			return grabBlock(blkX, blkY)[subX + subY * kBlockDim];
		}

		void set(int x, int y, bool value = true)
		{
			const int blkX = x / kBlockDim;
			const int blkY = y / kBlockDim;
			const int subX = x % kBlockDim;
			const int subY = y % kBlockDim;
			grabBlock(blkX, blkY)[subX + subY * kBlockDim] = value;
		}

	private:
		Block& grabBlock(int blkX, int blkY)
		{
			if (blkX == prevBlkX && blkY == prevBlkY) [[likely]]
				return *prevBlock;

			prevBlkX = blkX;
			prevBlkY = blkY;
			prevBlock = &blockMap[blkX + blkY * widthBlocks];
			return *prevBlock;
		}
	};

	template<class Value, Value kDef>
	struct TwoLevelLookup
	{
		static constexpr int kBlockDim = 16;

		struct Block : Array2D<Value, kBlockDim>
		{
			Block()
			{
				this->fill(kDef);
			}
		};

		std::unordered_map<int, Block> blockMap;
		int widthBlocks = (GC.getMapINLINE().getGridWidthINLINE() + kBlockDim - 1) / kBlockDim; // round up

		Block* prevBlock = nullptr;
		int prevBlkX = -1;
		int prevBlkY = -1;

		void clear()
		{
			blockMap.clear();
			prevBlock = nullptr;
			prevBlkX = -1;
			prevBlkY = -1;
		}

		Value& operator[](ivec2 v)
		{
			const int blkX = v.x / kBlockDim;
			const int blkY = v.y / kBlockDim;
			const int subX = v.x % kBlockDim;
			const int subY = v.y % kBlockDim;
			return grabBlock(blkX, blkY)[{ subX, subY }];
		}

		Value operator[](ivec2 v) const
		{
			const int blkX = v.x / kBlockDim;
			const int blkY = v.y / kBlockDim;
			const int subX = v.x % kBlockDim;
			const int subY = v.y % kBlockDim;
			if (const Block* const blk = getBlock(blkX, blkY))
				return (*blk)[{ subX, subY }];
			else
				return kDef;
		}

	private:
		Block& grabBlock(int blkX, int blkY)
		{
			if (blkX == prevBlkX && blkY == prevBlkY) [[likely]]
				return *prevBlock;

			prevBlkX = blkX;
			prevBlkY = blkY;
			prevBlock = &blockMap[blkX + blkY * widthBlocks];
			return *prevBlock;
		}

		const Block* getBlock(int blkX, int blkY) const
		{
			if (blkX == prevBlkX && blkY == prevBlkY) [[likely]]
				return prevBlock;

			if (const auto it = blockMap.find(blkX + blkY * widthBlocks); it != blockMap.end())
				return &it->second;
			else
				return nullptr;
		}
	};


	// Simple class used as an alternative for `CvGame::getSorenRandNum` in multithreaded code.
	// Returns random numbers deterministically based on the original RNG state and plot index (so it's more of a hasher based on the given seed).
	class DeterministicParallelPlotHashRNG
	{
	public:
		explicit DeterministicParallelPlotHashRNG(CvRandom& rng, const char* text);

		// Text can be passed in as in the original vanilla code, but will be ignored.
		uint16_t get(uint16_t size, int plotI, [[maybe_unused]] const char* text = nullptr) const;

	private:
		uint32_t mSeed{};
	};

	// Used to instansiate new RNG instances per-task. Seeded using a combination of a root seed and some deterministic task identifier.
	class DeterministicParallelSeedingRNG
	{
	public:
		explicit DeterministicParallelSeedingRNG(CvRandom& rng, const char* text);

		CvRandom createTaskRNGFor(uint32_t taskSeed) const;

	private:
		uint32_t mRootSeed{};
	};

	// Returns floor(x / y).
	//constexpr int fdiv(int x, unsigned int y)
	//{
	//	const int q = x / (int)y;
	//	return q - (x % (int)y < 0);
	//}
	//
	//// Returns ceil(x / y).
	//constexpr int cdiv(int x, unsigned int y)
	//{
	//	const int q = x / (int)y;
	//	return q + (x % (int)y > 0);
	//}

	constexpr iaabb2 rectdiv(iaabb2 rect, unsigned int blockDim)
	{
		rect.min.x = heck::fdiv(rect.min.x, blockDim);
		rect.min.y = heck::fdiv(rect.min.y, blockDim);
		// Remember: max is exclusive.
		// interval[-8..-1) / 8 -> interval[-1..0)
		// interval[-8..+0) / 8 -> interval[-1..0)
		// interval[-8..+1) / 8 -> interval[-1..1)
		// interval[0..0) / 8 -> interval[0..0)
		// interval[0..7) / 8 -> interval[0..1)
		// interval[0..8) / 8 -> interval[0..1)
		// interval[0..9) / 8 -> interval[0..2)
		rect.max.x = heck::cdiv(rect.max.x, blockDim);
		rect.max.y = heck::cdiv(rect.max.y, blockDim);
		return rect;
	}

	inline std::optional<int> nextBitIndex(auto& x)
	{
		if (x)
			return std::countr_zero(std::exchange(x, x & (x - 1)));
		else
			return std::nullopt;
	}

	inline int getUnwrappedPlotDistance(ivec2 origin, ivec2 coord)
	{
		const int iDX = std::abs(coord.x - origin.x);
		const int iDY = std::abs(coord.y - origin.y);
		return std::max(iDX, iDY) + std::min(iDX, iDY) / 2;
	}

	struct CivMapGeometry : heck::MapGeometry
	{
		using heck::MapGeometry::MapGeometry;

		explicit CivMapGeometry(const CvMap& map, unsigned int blockDim = 1)
			: MapGeometry({ int16_t(heck::cdiv(map.getGridWidthINLINE(), blockDim)), int16_t(heck::cdiv(map.getGridHeightINLINE(), blockDim)) },
				map.isWrapXINLINE(), map.isWrapYINLINE())
		{
		}

		using heck::MapGeometry::isValidCoord;

		heck::ISimdAStarGraph::Vector::Mask HECK_VECTORCALL isValidCoord(heck::vec2<heck::ISimdAStarGraph::Vector> coord) const
		{
			return (wrapX ? heck::ISimdAStarGraph::Vector::Mask::kAll : vcmple(0, coord.x) & vcmplt(coord.x, dim.x))
				& (wrapY ? heck::ISimdAStarGraph::Vector::Mask::kAll : vcmple(0, coord.y) & vcmplt(coord.y, dim.y));
		}

		[[nodiscard]] iaabb2 clampNonWrapped(iaabb2 rect) const
		{
			if (!wrapX)
			{
				rect.min.x = std::max(0, rect.min.x);
				rect.max.x = std::min(dim.x - 1, rect.min.x);
			}

			if (!wrapY)
			{
				rect.min.y = std::max(0, rect.min.y);
				rect.max.y = std::min(dim.y - 1, rect.min.y);
			}

			return rect;
		}

		[[nodiscard]] ivec2 wrapOrClampCoord(ivec2 coord) const
		{
			if (wrapX)
			{
				if (coord.x < 0)
					coord.x += dim.x;
				if (coord.x >= dim.x)
					coord.x -= dim.x;
			}
			else
				coord.x = std::clamp(coord.x, 0, dim.x - 1);
			if (wrapY)
			{
				if (coord.y < 0)
					coord.y += dim.y;
				if (coord.y >= dim.y)
					coord.y -= dim.y;
			}
			else
				coord.y = std::clamp(coord.y, 0, dim.y - 1);
			return coord;
		}

		// SIMD tends to prefer having coords clamped too, in case of unmasked gathers.
		[[nodiscard]] heck::vec2<heck::ISimdAStarGraph::Vector> HECK_VECTORCALL wrapOrClampCoords(heck::vec2<heck::ISimdAStarGraph::Vector> coord) const
		{
			if (wrapX)
			{
				coord.x = vmaskedadd(vcmplt(coord.x, 0), coord.x, dim.x);
				coord.x = vmaskedsub(vcmpge(coord.x, dim.x), coord.x, dim.x);
			}
			else
				coord.x = vmin(vmax(coord.x, 0), dim.x - 1);
			if (wrapY)
			{
				coord.y = vmaskedadd(vcmplt(coord.y, 0), coord.y, dim.y);
				coord.y = vmaskedsub(vcmpge(coord.y, dim.y), coord.y, dim.y);
			}
			else
				coord.y = vmin(vmax(coord.y, 0), dim.y - 1);
			return coord;
		}
	
		// Caller must wrap block coordinates!
		std::pair<ivec2, ivec2> getUnwrappedBlockEnumerationRectByStepDist(ivec2 plotCoord, int maxStepDist, unsigned int blockDim) const
		{
			const ivec2 minPlotCoord = plotCoord - ivec2(1, 1) * maxStepDist;
			const ivec2 imaxPlotCoord = plotCoord + ivec2(1, 1) * maxStepDist; // inclusive

			const int minX = heck::fdiv(minPlotCoord.x, blockDim);
			const int minY = heck::fdiv(minPlotCoord.y, blockDim);
			const int imaxX = heck::fdiv(imaxPlotCoord.x, blockDim);
			const int imaxY = heck::fdiv(imaxPlotCoord.y, blockDim);

			return adjustInclusiveRectBeforeWrapping({ minX, minY }, { imaxX, imaxY });
		}

		iaabb2 getUnwrappedBlockEnumerationRectByStepDist(iaabb2 plotRect, int maxStepDist, unsigned int blockDim) const
		{
			plotRect = plotRect.expanded({ maxStepDist, maxStepDist });

			const ivec2 minPlotCoord = plotRect.min;
			const ivec2 imaxPlotCoord = plotRect.max - ivec2(1, 1); // inclusive

			const int minX = heck::fdiv(minPlotCoord.x, blockDim);
			const int minY = heck::fdiv(minPlotCoord.y, blockDim);
			const int imaxX = heck::fdiv(imaxPlotCoord.x, blockDim);
			const int imaxY = heck::fdiv(imaxPlotCoord.y, blockDim);

			const auto [minBlocks, inclMaxBlocks] = adjustInclusiveRectBeforeWrapping({ minX, minY }, { imaxX, imaxY });
			return iaabb2(minBlocks, inclMaxBlocks + ivec2(1, 1));
		}

		int getMinimumStepDistance(const iaabb2& a, const iaabb2& b) const
		{
			return std::max(
				getAxisMinimumStepDistance(a.min.x, a.max.x, b.min.x, b.max.x, wrapX, dim.x),
				getAxisMinimumStepDistance(a.min.y, a.max.y, b.min.y, b.max.y, wrapY, dim.y)
			);
		}

		int stepDistance(ivec2 a, ivec2 b) const
		{
			return std::max(
				coordDistance(a.x, b.x, dim.x, wrapX),
				coordDistance(a.y, b.y, dim.y, wrapY)
			);
		}
	
		friend bool operator==(CivMapGeometry, CivMapGeometry) = default;

	private:
		// When wrapping, block sizes may be uneven. Add a bit extra just in case.
		std::pair<ivec2, ivec2> adjustInclusiveRectBeforeWrapping(ivec2 min, ivec2 imax) const
		{
			if (wrapX)
			{
				if (min.x < 0)
					--min.x;
				if (imax.x >= dim.x - 1)
					++imax.x;
			}
			else
			{
				min.x = std::clamp(min.x, 0, dim.x - 1);
				imax.x = std::clamp(imax.x, 0, dim.x - 1);
			}

			if (wrapY)
			{
				if (min.y < 0)
					--min.y;
				if (imax.y >= dim.y - 1)
					++imax.y;
			}
			else
			{
				min.y = std::clamp(min.y, 0, dim.y - 1);
				imax.y = std::clamp(imax.y, 0, dim.y - 1);
			}

			return { min, imax };
		}

		static int getAxisMinimumStepDistance(int minA, int maxA, int minB, int maxB, bool wrap, int dim)
		{
			if (wrap)
			{
				if (minA > minB)
				{
					std::swap(minA, minB);
					std::swap(maxA, maxB);
				}

				if (minB < maxA)
					return 0;

				// Range reduction
				while (minB - (maxA - 1) >= dim)
				{
					minB -= dim;
					maxB -= dim;
				}

				const int unwrappedDist1 = minB - (maxA - 1);
				// By shifting B to before A.
				// If clamping happens, then that means these spans overlap after wrapping.
				const int unwrappedDist2 = std::max(0, -(unwrappedDist1 - dim + (maxB - minB)));

				return std::min(unwrappedDist1, unwrappedDist2);
			}
			else if (maxA <= minB)
				return minB - (maxA - 1);
			else if (maxB <= minA)
				return minA - (maxB - 1);
			else
				return 0;
		}
	};

	// For union a bunch of plot coordinates considering that the map wraps.
	// So, this will prefer a small rect that wraps to the other side of the map over a giant rect that includes one plot at X 0 and one plot at X dim-1.
	void unionUnwrappedRect(const CivMapGeometry& mapGeom, iaabb2& unwrappedRect, ivec2 wrappedCoord) noexcept;
	

	inline void enumerateMapBlocksByStepDist(int blockDim, const CivMapGeometry& mapSizeInfoBlocks, ivec2 plotCoord, int maxStepDist, auto blkCallback)
	{
		const auto [min, imax] = mapSizeInfoBlocks.getUnwrappedBlockEnumerationRectByStepDist(plotCoord, maxStepDist, blockDim);

		for (const int blkY : range(min.y, imax.y + 1))
			for (const int blkX : range(min.x, imax.x + 1))
				blkCallback(mapSizeInfoBlocks.wrapCoord(ivec2(blkX, blkY)));
	}

	inline i16vec2 getPlotCoord(const CvPlot& plot)
	{
		return { (int16_t)plot.getX_INLINE(), (int16_t)plot.getY_INLINE() };
	}

	inline int toPlotIndex(ivec2 coord, ivec2 dim)
	{
		return coord.x + coord.y * dim.x;
	}

	inline ivec2 toPlotCoord(int plotI, ivec2 dim)
	{
		return {
			plotI % dim.x,
			plotI / dim.x,
		};
	}

	// The min step distance that covers the given plot distance.
	inline int plotDistToStepDist(int plotDist)
	{
		// It's the same value.
		return plotDist;
	}


	inline constexpr FeatureTypes kForestFeatureTypes[]{ std::bit_cast<FeatureTypes>(1), std::bit_cast<FeatureTypes>(4) };

	constexpr bool isForest(FeatureTypes x)
	{
		const int i = std::bit_cast<int>(x); // Stop clang warnings
		return i == 1 || i == 4;
	}

	heck::Image renderMap(bool showTerrainAndFeatures = true);

	void dumpMapExperimentFile(const char* path);

	std::wstring getBasicPlotDescription(const CvPlot& plot);
	std::wstring getFullPlotDescription(const CvPlot& plot);

	std::string toString(UnitAITypes);
}