#pragma once

#include "Util.h"

#include <mutex>

namespace GiganticMapsOptimisationsLib
{
	// "Regional". Writes are specific plots and reads are rect regions.
	// Used for pathing conflicts.
	struct RegionalConflictMap
	{
		static constexpr int kBlockDim = 8;

		explicit RegionalConflictMap(const CvMap& map) : mapWriteMask(CivMapGeometry(map, kBlockDim).dim), blockWriteMask(CivMapGeometry(map, kBlockDim * kBlockDim).dim)
		{
		}

		///

		// Called after unit update simulation to mark a plot as potentially causing a conflict.
		// This set of plots is much smaller than the full read set, as the full read set could be the whole map.
		// And a small potential conflict set is needed to quickly mask against a group's read set.
		// Thread-safe, relaxed memory ordering.
		void markPotentialConflictPlot(ivec2 coord)
		{
			setWritten(coord);
		}

		// Thread-safe. Read-only.
		bool isPotentialConflictPlot(ivec2 coord) const
		{
			return getBit(mapWriteMask, coord);
		}

		///

		// Single-threaded.
		void prepareForConflictHashing()
		{
			mConflictHashTable.resize(std::max<size_t>(10, mNumConflictPlots));
		}

		///

		// Thread-safe. Mutex-guarded.
		void addPotentialConflictAtPlot(ivec2 plotCoord, int groupId)
		{
			const size_t index = plotHash(plotCoord) % mConflictHashTable.size();
			const std::lock_guard lock(mMutexes[index % mMutexes.size()]);
			mConflictHashTable[index].push_back(groupId);
		}

		// Thread-safe. Mutex-guarded.
		void addPotentialConflictsInRect(i16aabb2 plotRect, int groupId)
		{
			enumeratePotentialConflictsPlotsInRect(plotRect, [groupId, this](ivec2 coord) {
				addPotentialConflictAtPlot(coord, groupId);
				});
		}

		///

		// Thread-safe. Read-only.
		void enumeratePotentialConflictsAtPlot(ivec2 plotCoord, auto f) const
		{
			const size_t index = plotHash(plotCoord) % mConflictHashTable.size();
			for (const int groupId : mConflictHashTable[index])
				f(groupId);
		}

		// Thread-safe. Read-only.
		void enumeratePotentialConflictsInRect(i16aabb2 plotRect, auto f) const
		{
			enumeratePotentialConflictsPlotsInRect(plotRect, [f, this](ivec2 coord) {
				enumeratePotentialConflictsAtPlot(coord, f);
				});
		}

		void enumeratePotentialConflictsPlotsInRect(i16aabb2 plotRect, auto f)
		{
			static constexpr int kSuperBlockDim = kBlockDim * kBlockDim;
			const ivec2 mapDimSuperBlocks = blockWriteMask.dim;

			const iaabb2 superblocksRect = rectdiv(plotRect.cast<int>(), kSuperBlockDim).intersection(iaabb2::sized({}, mapDimSuperBlocks));
			const iaabb2 blocksRect = rectdiv(plotRect.cast<int>(), kBlockDim).intersection(iaabb2::sized({}, mapWriteMask.dim));

			for (const int superblkY : range(superblocksRect.min.y, superblocksRect.max.y))
			{
				for (const int superblkX : range(superblocksRect.min.x, superblocksRect.max.x))
				{
					uint64_t superBlockWriteMask = blockWriteMask[{ superblkX, superblkY }];

					// Enumerate bits.
					while (superBlockWriteMask)
					{
						const int blockI = std::countr_zero(superBlockWriteMask);
						superBlockWriteMask &= superBlockWriteMask - 1;

						const int blkX = superblkX * kBlockDim + blockI % kBlockDim;
						const int blkY = superblkY * kBlockDim + blockI / kBlockDim;

						if (blocksRect.contains(ivec2(blkX, blkY)))
						{
							uint64_t plotsWriteMask = mapWriteMask[{ blkX, blkY }];

							// Enumerate bits.
							while (plotsWriteMask)
							{
								const int plotI = std::countr_zero(plotsWriteMask);
								plotsWriteMask &= plotsWriteMask - 1;

								const int plotX = blkX * kBlockDim + plotI % kBlockDim;
								const int plotY = blkY * kBlockDim + plotI / kBlockDim;

								if (plotRect.contains((i16vec2)ivec2(plotX, plotY)))
									f(ivec2(plotX, plotY));
								//addPotentialConflictAtPlot(groupId, { plotX, plotY });
							}
						}
					}
				}
			}
		}

	private:
		// Hierarchical bitmap.
		DynamicArray2D<uint64_t> mapWriteMask; // A bit for each plot, a word for each 8x8 block. 83KB for 1040x640 map.
		DynamicArray2D<uint64_t> blockWriteMask; // A bit for each 8x8 block, a word for each 64x64 super-block. 1.3KB for 1040x640 map.
		std::atomic_size_t mNumConflictPlots{};

		std::array<std::mutex, 16> mMutexes;

		// After all the potential writes have been counted, this is the simple hash table to get from write/read to potentially conflicting units.
		// So this is basically a bloom filter, false positives are likely.
		// [plotHash % mConflictHashTable.size()].
		std::vector<std::vector<int>> mConflictHashTable;

		static bool getBit(const DynamicArray2D<uint64_t>& array, ivec2 coord)
		{
			const ivec2 blkCoord = coord / kBlockDim;
			const int bitX = coord.x % kBlockDim;
			const int bitY = coord.y % kBlockDim;
			return bool((std::atomic_ref(array[blkCoord]).load(std::memory_order_relaxed) >> (bitX + bitY * kBlockDim)) & 1);
		}

		static void setBit(DynamicArray2D<uint64_t>& array, ivec2 coord)
		{
			const ivec2 blkCoord = coord / kBlockDim;
			const int bitX = coord.x % kBlockDim;
			const int bitY = coord.y % kBlockDim;
			std::atomic_ref(array[blkCoord]).fetch_or(uint64_t(1) << (bitX + bitY * kBlockDim), std::memory_order_relaxed);
		}

		// Thread-safe.
		void setWritten(ivec2 coord)
		{
			if (!getBit(mapWriteMask, coord))
			{
				setBit(mapWriteMask, coord);
				setBit(blockWriteMask, coord / kBlockDim);
				mNumConflictPlots.fetch_add(1, std::memory_order_relaxed);
			}
		}

		static uint32_t plotHash(ivec2 coord)
		{
			// Truncate for a possibly faster modulo.
			return (uint32_t)VectorHasher()(coord);
		}
	};

	// "Linear". Addresses are an arbitrary index. Indices are wrapped to the allocated size, so this is effectively a "bloom filter".
	class LinearConflictMap
	{
	public:
		explicit LinearConflictMap(size_t numBuckets)
			: mBuckets(std::max<size_t>(1, numBuckets))
			, mWriteMask(heck::cdiv(mBuckets, size_t(64)))
			, mConflictTable(mBuckets)
		{
		}

		///

		// Thread-safe.
		void markPotentialConflictIndex(size_t index)
		{
			index %= mBuckets;
			mWriteMask[index / 64].fetch_or(uint64_t(1) << (index % 64), std::memory_order_relaxed);
		}

		// Thread-safe. Read-only.
		bool isPotentialConflictIndex(size_t index) const
		{
			index %= mBuckets;
			return (mWriteMask[index / 64].load(std::memory_order_relaxed) & (uint64_t(1) << (index % 64))) != 0;
		}

		///

		// Thread-safe. Mutex-guarded.
		void addPotentialConflict(size_t index, int taskId)
		{
			index %= mBuckets;
			const std::lock_guard lock(mMutexes[index % mMutexes.size()]);
			mConflictTable[index].push_back(taskId);
		}

		///

		// Thread-safe. Read-only.
		void enumeratePotentialConflicts(size_t index, auto f) const
		{
			index %= mBuckets;
			for (const int groupId : mConflictTable[index])
				f(groupId);
		}

	private:
		size_t mBuckets = 0;
		std::vector<std::atomic_uint64_t> mWriteMask; // [bucket / 64]

		std::array<std::mutex, 16> mMutexes;

		std::vector<std::vector<int>> mConflictTable; // [bucket]
	};
}