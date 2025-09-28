#pragma once

#include "SimdAvxRegs.h"
#include "SimdConversionsAvx.h"

namespace heck::simd::polyfills
{
	inline __m256d convertU32ToDouble(__m128i a)
	{
		if constexpr (kEnableAVX512)
			return _mm256_cvtepu32_pd(a);
		else
		{
			// Convert by shifting value into position with the correct exponent bits, then subtract 2^32 (the implicit mantissa bit).
			// https://www6.uniovi.es/~antonio/uned/ieee754/IEEE-754.html
			// Value of double(2**32) = 0x41F0000000000000
			constexpr double kU32DoubleBias = double(uint64_t(1) << 32);
			__m256i fpBits = _mm256_set1_epi64x(std::bit_cast<uint64_t>(kU32DoubleBias));
			// Place integer into mantissa.
			fpBits = _mm256_or_si256(fpBits, _mm256_slli_epi64(_mm256_cvtepu32_epi64(a), 52 - 32));
			return _mm256_sub_pd(_mm256_castsi256_pd(fpBits), _mm256_set1_pd(kU32DoubleBias));
		}
	};

	// Truncating. Value must be [0..2^32).
	inline __m128i convertU32RangeDoubleToU32(__m256d a)
	{
		if constexpr (kEnableAVX512)
			return _mm256_cvttpd_epu32(a);
		else
		{
			// Can't just do the reverse of the above as adding 2**32 may round up: 0x7FFFFF/(1<<23)+2**32 == huge mantissa
			// So, subtract 2**31 for the high elements instead.
			const __m256d highBitValue = _mm256_set1_pd(double(uint64_t(1) << 31));
			const __m256d isHigh = _mm256_cmp_pd(highBitValue, a, _CMP_LE_OQ);
			const __m256d biasedValues = _mm256_blendv_pd(a, _mm256_sub_pd(a, highBitValue), isHigh);
			const __m128i epi32Values = _mm256_cvttpd_epi32(biasedValues);
			return _mm_blendv_epi8(epi32Values, _mm_add_epi32(epi32Values, _mm_set1_epi32(uint32_t(1) << 31)),
				ConversionPrimitive<Avx128IntegerReg, 2>::truncate<uint64_t>({ Avx256IntegerReg{ _mm256_castpd_si256(isHigh) } }).bits);
		}
	};
}