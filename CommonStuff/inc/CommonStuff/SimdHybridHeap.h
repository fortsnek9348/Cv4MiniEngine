#pragma once

#include "SimdHeap.h"

namespace heck
{
	// Combines an SimdHeap with a small scalar array to support masked push/pop.
	// If kStrictOrder = false, popped elements may not be in strict order, but will at least contain the minimum element in the whole heap.
	template<class Key, class Value, size_t kVectorWidth, size_t kDegree, bool kStrictOrder>
	class SimdHybridHeap
	{
	public:
		using SimdHeap = SimdHeap<Key, Value, kVectorWidth, kDegree>;
		using KeyVector = SimdHeap::KeyVector;
		using ValueVector = SimdHeap::ValueVector;
		using Mask = KeyVector::Mask;

		SimdHybridHeap()
		{
			mScalarKeys.fill(std::numeric_limits<Key>::max()); // dummy values for sorting
		}

		void clear()
		{
			mSimdHeap.clear();
			mNumScalars = 0;
			mScalarKeys.fill(std::numeric_limits<Key>::max());
		}

		bool empty() const
		{
			return mSimdHeap.empty() && mNumScalars <= 0;
		}

		int32_t peek() const
		{
			int32_t key = getScalarArrayMin();
			if (!mSimdHeap.empty())
				key = std::min(key, mSimdHeap.peekPopMin());
			return key;
		}

		void push(KeyVector keys, ValueVector values, Mask mask)
		{
			if (mask == Mask::kAll) // If all, then just push it onto the SIMD heap.
				mSimdHeap.push({ keys, values });
			else if (mask.any())
			{
				// Else, append to scalar array.
				const size_t n = std::popcount(mask.asBitsUInt());
				keys.compressStore(std::span(mScalarKeys).subspan(mNumScalars), mask);
				values.compressStore(std::span(mScalarValues).subspan(mNumScalars), mask);
				mNumScalars += n;
				if (mNumScalars >= kVectorWidth)
				{
					// We have a vector. Move it to the SIMD heap.
					keys = KeyVector(std::span(mScalarKeys).template subspan<0, kVectorWidth>());
					values = ValueVector(std::span(mScalarValues).template subspan<0, kVectorWidth>());
					mSimdHeap.push({ keys, values });
					// Copy right to left, then reset the unused keys on the right.
					std::ranges::copy(std::span(mScalarKeys).template subspan<kVectorWidth>(), std::span(mScalarKeys).begin());
					std::ranges::copy(std::span(mScalarValues).template subspan<kVectorWidth>(), std::span(mScalarValues).begin());
					std::ranges::fill(std::span(mScalarKeys).template subspan<kVectorWidth>(), std::numeric_limits<Key>::max());
					mNumScalars -= kVectorWidth;
				}
			}
		}

		struct PopResult
		{
			KeyVector keys{};
			ValueVector values{};
			Mask mask{};
		};

		PopResult pop()
		{
			if (mSimdHeap.empty()) [[unlikely]]
				return popAllScalarArray();

			if constexpr (kStrictOrder)
			{
				if (getScalarArrayMin() < mSimdHeap.peekPopMax())
				{
					// Need to merge with scalar array if we want strict order.
					const typename SimdHeap::Node scalarsNode{
						KeyVector(std::span(mScalarKeys).template subspan<0, kVectorWidth>()), // unused keys are std::numeric_limits<Key>::max().
						ValueVector(std::span(mScalarValues).template subspan<0, kVectorWidth>()),
					};

					const typename SimdHeap::Node poppedNode = mSimdHeap.pop();

					const std::array<typename SimdHeap::Node, 2> mergeResult = SimdHeap::mergeNodes(poppedNode, scalarsNode);

					// Put excess back into the scalars array.
					std::ranges::copy(mergeResult[1].keys.toArray(), mScalarKeys.begin());
					std::ranges::copy(mergeResult[1].values.toArray(), mScalarValues.begin());

					return {
						mergeResult[0].keys,
						mergeResult[0].values,
						Mask::kAll,
					};
				}
			}
			else
			{
				// Use the scalar if it has the min element.
				if (getScalarArrayMin() < mSimdHeap.peekPopMin())
					return popAllScalarArray();
			}
			
			const typename SimdHeap::Node poppedNode = mSimdHeap.pop();
			return {
				poppedNode.keys,
				poppedNode.values,
				Mask::kAll,
			};
		}

		void update(auto getVectorHeapValue)
		{
			for (auto& entry : mSimdHeap.elements())
				entry.keys = getVectorHeapValue(entry.values);
			mSimdHeap.reheapify();

			std::ranges::copy(getVectorHeapValue(ValueVector(std::span(mScalarValues).template subspan<0, kVectorWidth>())).toArray(), mScalarKeys.begin());
			std::ranges::fill(std::span(mScalarKeys).subspan(mNumScalars), std::numeric_limits<Key>::max());
		}

		void checkHeapProperty()
		{
			mSimdHeap.checkHeapProperty();
		}

	private:
		SimdHeap mSimdHeap;

		static constexpr int kScalarArraySize = kVectorWidth * 2;

		// Lowest is always at the start.
		// Always < kVectorWidth outside `push`.
		size_t mNumScalars = 0;

		std::array<Key, kScalarArraySize> mScalarKeys{};
		std::array<Value, kScalarArraySize> mScalarValues{};
		
		Key getScalarArrayMin() const
		{
			return hmin(KeyVector(std::span(mScalarKeys).template subspan<0, kVectorWidth>()));
		}

		PopResult popAllScalarArray()
		{
			const PopResult result{
				KeyVector(std::span(mScalarKeys).template subspan<0, kVectorWidth>()),
				ValueVector(std::span(mScalarValues).template subspan<0, kVectorWidth>()),
				typename KeyVector::Mask(std::bitset<kVectorWidth>((uint64_t(1) << mNumScalars) - 1)),
			};
			std::ranges::fill(std::span(mScalarKeys).template subspan<0, kVectorWidth>(), std::numeric_limits<Key>::max());
			mNumScalars = 0;
			return result;
		}
	};
}