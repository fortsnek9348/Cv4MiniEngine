#pragma once

#include "Util.h"

#include <functional>
#include <generator>

namespace GiganticMapsOptimisationsLib
{
	class VersionedBlocks2D
	{
	public:
		explicit VersionedBlocks2D(ivec2 dim);

		uint64_t getVersionSerial() const
		{
			return mVersion;
		}

		std::array<uint16_t, 2> getBlockVersionsAt(ivec2 coord) const;

		//void bumpVersion()
		//{
		//	++mVersion;
		//}

		void update(ivec2 coord, bool& changed)
		{
			if (!changed)
			{
				changed = true;
				bumpVersionAfterMultithreadedUpdate();
			}
			mL2BlockVersions[coord / kBlockDim / kBlockDim] = mL1BlockVersions[coord / kBlockDim] = mVersion & UINT16_MAX;
		}

		void updateMultithreaded(ivec2 coord)
		{
			const uint16_t blockVersion = (mVersion + 1) & UINT16_MAX;
			std::atomic_ref(mL1BlockVersions[coord / kBlockDim]).store(blockVersion, std::memory_order_relaxed);
			std::atomic_ref l2ver(mL2BlockVersions[coord / kBlockDim / kBlockDim]);
			if (l2ver.load(std::memory_order_relaxed) != blockVersion)
				l2ver.store(blockVersion, std::memory_order_relaxed);
		}

		void bumpVersionAfterMultithreadedUpdate() noexcept;

		// A coroutine!
		std::generator<iaabb2> getUserDirtyRects(uint64_t myOldVersion) const;

	private:
		static constexpr unsigned int kBlockDim = 8;

		uint64_t mVersion = 0;
		ivec2 mDim{};
		DynamicArray2D<uint16_t> mL1BlockVersions; // 8x8 plots
		DynamicArray2D<uint16_t> mL2BlockVersions; // 8x8 blocks
	};

	class VersionedBitmap2D
	{
	public:
		explicit VersionedBitmap2D(ivec2 dim) : mBitmap(dim), mBlocks(dim)
		{
		}

		std::generator<iaabb2> getUserDirtyRects(uint64_t myOldVersion) const
		{
			return mBlocks.getUserDirtyRects(myOldVersion);
		}

		std::array<uint16_t, 2> getBlockVersionsAt(ivec2 coord) const
		{
			return mBlocks.getBlockVersionsAt(coord);
		}

		uint64_t getVersionSerial() const
		{
			return mBlocks.getVersionSerial();
		}

		bool operator[](ivec2 coord) const
		{
			return mBitmap[coord];
		}

		void setBit(ivec2 coord, bool value, bool& changed)
		{
			if (mBitmap[coord] != value)
			{
				mBitmap[coord] = value;
				forceInvalidate(coord, changed);
			}
		}

		void forceInvalidate(ivec2 coord, bool& changed)
		{
			mBlocks.update(coord, changed);
		}

	private:
		DynamicBitmap2D mBitmap;
		VersionedBlocks2D mBlocks;
	};

	class InvalidationState
	{
	public:
		explicit InvalidationState(CivMapGeometry mapSizeInfo);

		CivMapGeometry getMapSizeInfo() const;

		void reset();

		void invalidateAll();
		void invalidateStepRadius(ivec2 coord, int r);
		void invalidateSingle(ivec2 coord);

		bool isAllDirty() const;
		bool hasAnyDirty() const;

		std::span<const i16vec2> getDirtyPlotList() const;

	private:
		CivMapGeometry mapSizeInfo;
		bool totallyInvalidated = true;
		DynamicBitmap2D invalidationPlotListBitmap;
		std::vector<i16vec2> plotInvalidations;
		//size_t mTotalLocalInvalidatedPlots = 0;
	};

	class InvalidationBitmapState
	{
	public:
		explicit InvalidationBitmapState(CivMapGeometry mapSizeInfo, int blockDimShift);

		void reset();

		void invalidateAll();

		// These are thread-safe (relaxed mem order).
		void invalidateStepRadius(ivec2 coord, int r);
		void invalidateSingle(ivec2 coord);

		bool hasAnyDirty() const;

		//int getNumBlockRows() const;
		//void enumerateDirtyBlocks(int blkRowY, std::function<void(i16aabb2 plotRect)> f) const;

		void getAllDirtyRects(std::vector<i16aabb2>& out) const;

	private:
		CivMapGeometry mMapSizeInfo;
		std::atomic_bool mHasAnyDirty{};
		int mBlockDimShift = 0;
		DynamicBitmap2D mBlockBitmap;
	};
}