#pragma once

#include "SimdCommon.h"

#include <immintrin.h>

namespace heck::simd
{
	// If disabled, then simd::Vector won't use AVX512 regs.
	// Also affects conversion routines.
#if __AVX512F__ 
	inline constexpr bool kEnableAVX512 = true;
#else
	inline constexpr bool kEnableAVX512 = false;
#endif

	// This boxing is needed to keep alignment attribute on GCC.
	struct Avx128IntegerReg
	{
		__m128i bits{};

		friend bool operator==(Avx128IntegerReg a, Avx128IntegerReg b)
		{
			if constexpr (kEnableAVX512)
				return _mm_cmpeq_epi32_mask(a.bits, b.bits) == 0b1111;
			else
				return _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(a.bits, b.bits))) == 0b1111;
		}
	};

	struct Avx256IntegerReg
	{
		__m256i bits{};

		friend bool operator==(Avx256IntegerReg a, Avx256IntegerReg b)
		{
			if constexpr (kEnableAVX512)
				return _mm256_cmpeq_epi32_mask(a.bits, b.bits) == 0b11111111;
			else
				return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(a.bits, b.bits))) == 0b11111111;
		}
	};

	struct Avx512IntegerReg
	{
		__m512i bits{};

		friend bool operator==(Avx512IntegerReg a, Avx512IntegerReg b)
		{
			return _mm512_cmpeq_epi64_mask(a.bits, b.bits) == 0b11111111;
		}
	};

	inline constexpr RegTraits kAVX2Traits{
		.supportsGather = true,
		.isNative = true,
	};

	inline constexpr RegTraits kAVX512Traits{
		.supportsGather = true,
		.supportsScatter = true,
		.supportsMaskedOps = true,
		.supportsCompress = true,
		.isNative = true,
		.maxGatherScatterScale = 8,
	};

	template<size_t kBytes>
	using AvxRegSelByBytes = decltype([] {
		if constexpr (kBytes == 16)
			return Avx128IntegerReg();
		else if constexpr (kBytes == 32)
			return Avx256IntegerReg();
		else if constexpr (kBytes == 64)
			return Avx512IntegerReg();
		else
			static_assert(false);
		}());

	
}