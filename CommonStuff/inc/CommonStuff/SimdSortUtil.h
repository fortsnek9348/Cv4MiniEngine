#pragma once

#include "Simd.h"

namespace heck
{
	// NOTE: MSVC produces absolutely god-awful code if you use this to get both the index and key in a pair. More than 3x as much code than clang.
	//       Instead, just get the index!
	template<size_t k>
		requires(k > 0)
	inline auto staticReduce(size_t baseI, const auto& f, const auto& proj)
	{
		if constexpr (k <= 1)
			return proj(baseI);
		//else if constexpr (k == 4)
		//{
		//	return f(
		//		f(proj(baseI), proj(baseI + 1)),
		//		f(proj(baseI + 2), proj(baseI + 3))
		//	);
		//}
		else if constexpr (k % 2 == 0)
			return f(staticReduce<k / 2>(baseI, f, proj), staticReduce<k / 2>(baseI + k / 2, f, proj));
		else
			return f(staticReduce<k - 1>(baseI, f, proj), proj(baseI + k - 1));
	}

	template<class T, size_t k>
	struct BitonicMergeLayerResult
	{
		using IndexVector = simd::Vector<T, k>::DefaultIndexVector;
		simd::Vector<T, k> keys{};
		IndexVector indices{};
	};

	template<size_t kElements, size_t kShift>
	constexpr std::array<size_t, kElements> makeLayerSwizzle()
	{
		std::array<size_t, kElements> indices{};

		//if constexpr (kShift == kElements / 2)
		//{
		//	for (size_t i = 0; i < kElements; ++i)
		//		indices[i] = kElements - 1 - i;
		//}
		//else
		{
			for (size_t i = 0; i < kElements; ++i)
			{
				if (i / kShift % 2 == 0)
					indices[i] = i + kShift;
				else
					indices[i] = i - kShift;
			}
		}
		return indices;
	}

	template<size_t kElements>
	constexpr std::array<size_t, kElements> makeSecondHalfReverseSwizzle()
	{
		std::array<size_t, kElements> indices{};

		for (size_t i = 0; i < kElements; ++i)
		{
			if (i < kElements / 2)
				indices[i] = i;
			else
				indices[i] = kElements / 2 + (kElements - 1 - i);
		}
		
		return indices;
	}

	template<size_t kElements, size_t kShift>
	constexpr std::bitset<kElements> makeLayerHalfMask()
	{
		std::bitset<kElements> mask{};
		for (size_t i = 0; i < kElements; ++i)
			if (i / kShift % 2 == 0)
				mask[i] = true;
		return mask;
	}

	// <https://en.wikipedia.org/wiki/Bitonic_sorter>
	// Given elements in bitonic order, sort them.
	// Bitonic order here is where the first half is sorted ascending and the second half is sorted descending.
	// NOTE: Keys are given in input permuted order, not original order!
	template<size_t kLayerI, class T, size_t k>
	inline BitonicMergeLayerResult<T, k> HECK_VECTORCALL bitonicMergeLayer(BitonicMergeLayerResult<T, k> input)
	{
		static constexpr size_t kShiftAmount = k >> (kLayerI + 1);
		static constexpr std::array<size_t, k> kCmpSwizzle = makeLayerSwizzle<k, kShiftAmount>();
		static constexpr typename simd::Vector<T, k>::Mask kLayerHalfMask{ makeLayerHalfMask<k, kShiftAmount>() };

		using IndexVector = BitonicMergeLayerResult<T, k>::IndexVector;

		if constexpr (kShiftAmount == k / 2)
		{
			// First layer. Reverse the second half.
			constexpr std::array<size_t, k> kInitialReverseSwizzle = makeSecondHalfReverseSwizzle<k>();
			input.keys = input.keys.template swizzle<kInitialReverseSwizzle>();
			input.indices = input.indices.template swizzle<kInitialReverseSwizzle>();
		}

		const simd::Vector<T, k> keysCmpSwizzled = input.keys.template swizzle<kCmpSwizzle>();
		const IndexVector indicesCmpSwizzled = input.indices.template swizzle<kCmpSwizzle>();
		const typename simd::Vector<T, k>::Mask swapMaskHalf = vcmplt(keysCmpSwizzled, input.keys) & kLayerHalfMask;
		const typename simd::Vector<T, k>::Mask swapMask = swapMaskHalf | (swapMaskHalf << kShiftAmount);
		const auto keysOut = vselect(swapMask, keysCmpSwizzled, input.keys);
		const auto indicesOut = vselect(swapMask, indicesCmpSwizzled, input.indices);
		return { keysOut, indicesOut };
	}

	// NOTE: Keys are given in input permuted order, not original order!
	template<class T, size_t k>
	inline BitonicMergeLayerResult<T, k> HECK_VECTORCALL mergeBitonic(BitonicMergeLayerResult<T, k> input)
	{
		constexpr size_t kNumLayers = std::countr_zero(k);

		[&] <size_t... kLayerI>(std::index_sequence<kLayerI...>) {
			((input = bitonicMergeLayer<kLayerI>(input)), ...);
		}(std::make_index_sequence<kNumLayers>());

		return input;
	}

	template<class T, size_t k>
	struct SimdSortLayerConfig
	{
		using KeyVector = simd::Vector<T, k>;
		using IndexVector = KeyVector::DefaultIndexVector;

		std::array<size_t, k> cmpSwizzle{};
		KeyVector::Mask minMask;
		KeyVector::Mask maxMask;
	
		// { minI, maxI }, ... }
		explicit constexpr SimdSortLayerConfig(std::initializer_list<std::pair<int, int>> pairs)
		{
			cmpSwizzle = simd::iotaArray<size_t, k>();
			std::bitset<k> minBits{};
			std::bitset<k> maxBits{};

			for (const auto [a, b] : pairs)
			{
				std::swap(cmpSwizzle[a], cmpSwizzle[b]);
				minBits[a] = true;
				maxBits[b] = true;
			}

			minMask = typename KeyVector::Mask(minBits);
			maxMask = typename KeyVector::Mask(maxBits);
		}
	};

	template<class T, size_t k, SimdSortLayerConfig<T, k> kSimdSortLayerConfig>
	inline BitonicMergeLayerResult<T, k> HECK_VECTORCALL doSimdSortLayer(BitonicMergeLayerResult<T, k> input)
	{
		// 9 instructions per layer
		const auto keysCmp = input.keys.template swizzle<kSimdSortLayerConfig.cmpSwizzle>();
		const auto swapMinMask = vcmplt(keysCmp, input.keys) & kSimdSortLayerConfig.minMask;
		const auto swapMaxMask = vcmplt(input.keys, keysCmp) & kSimdSortLayerConfig.maxMask;
		const auto swapMask = swapMinMask | swapMaxMask;
		const auto indicesCmp = input.indices.template swizzle<kSimdSortLayerConfig.cmpSwizzle>();
		return {
			.keys = vselect(swapMask, keysCmp, input.keys),
			.indices = vselect(swapMask, indicesCmp, input.indices),
		};
	}

	template<class T>
	inline BitonicMergeLayerResult<T, 16> HECK_VECTORCALL computeSimdSortedPermutation(simd::Vector<T, 16> keys)
	{
		static constexpr unsigned int k = 16;

		// <https://bertdobbelaere.github.io/sorting_networks.html#N16L61D9>
		static constexpr SimdSortLayerConfig<T, k> kLayers[]{
			SimdSortLayerConfig<T, k>({ {0,5},{1,4},{2,12},{3,13},{6,7},{8,9},{10,15},{11,14} }),
			SimdSortLayerConfig<T, k>({ {0,2},{1,10},{3,6},{4,7},{5,14},{8,11},{9,12},{13,15} }),
			SimdSortLayerConfig<T, k>({ {0,8},{1,3},{2,11},{4,13},{5,9},{6,10},{7,15},{12,14} }),
			SimdSortLayerConfig<T, k>({ {0,1},{2,4},{3,8},{5,6},{7,12},{9,10},{11,13},{14,15} }),
			SimdSortLayerConfig<T, k>({ {1,3},{2,5},{4,8},{6,9},{7,11},{10,13},{12,14} }),
			SimdSortLayerConfig<T, k>({ {1,2},{3,5},{4,11},{6,8},{7,9},{10,12},{13,14} }),
			SimdSortLayerConfig<T, k>({ {2,3},{4,5},{6,7},{8,9},{10,11},{12,13} }),
			SimdSortLayerConfig<T, k>({ {4,6},{5,7},{8,10},{9,11} }),
			SimdSortLayerConfig<T, k>({ {3,4},{5,6},{7,8},{9,10},{11,12} }),
		};
		
		using IndexVector = simd::Vector<T, k>::DefaultIndexVector;

		static constexpr IndexVector kIota{ simd::iotaArray<typename IndexVector::value_type, k>() };

		return [] <size_t... kLayerI>(std::index_sequence<kLayerI...>, simd::Vector<T, k> keys) {
			BitonicMergeLayerResult<T, k> input{
				.keys = keys,
				.indices = kIota,
			};
			((input = doSimdSortLayer<T, k, kLayers[kLayerI]>(input)), ...);
			return input;
		}(std::make_index_sequence<std::size(kLayers)>(), keys);
	}
}