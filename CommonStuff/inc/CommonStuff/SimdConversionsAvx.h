#pragma once

#include "SimdCommon.h"
#include "SimdAvxRegs.h"

namespace heck::simd
{
	inline Avx256IntegerReg HECK_VECTORCALL joinRegs(std::array<Avx128IntegerReg, 2> parts)
	{
		return { _mm256_inserti128_si256(_mm256_castsi128_si256(parts[0].bits), parts[1].bits, 1) };
	}

	//inline Avx512IntegerReg joinRegs(std::array<Avx128IntegerReg, 4> parts)
	//{
	//	return { joinRegs(arraySubarray<2>(parts, 0)), joinRegs(arraySubarray<2>(parts, 2)) };
	//}

	inline Avx512IntegerReg HECK_VECTORCALL joinRegs(std::array<Avx256IntegerReg, 2> parts)
	{
		return { _mm512_inserti32x8(_mm512_castsi256_si512(parts[0].bits), parts[1].bits, 1) };
	}

	inline Avx128IntegerReg HECK_VECTORCALL join64BitSSERegs(Avx128IntegerReg lo, Avx128IntegerReg hi)
	{
		return { _mm_unpacklo_epi64(lo.bits, hi.bits) };
	}

	inline Avx128IntegerReg HECK_VECTORCALL join32BitSSERegs(std::array<Avx128IntegerReg, 4> parts)
	{
		return join64BitSSERegs(
			{ _mm_unpacklo_epi32(parts[0].bits, parts[1].bits) },
			{ _mm_unpacklo_epi32(parts[2].bits, parts[3].bits) }
		);
	}

	inline std::array<Avx128IntegerReg, 2> HECK_VECTORCALL splitReg(Avx256IntegerReg from)
	{
		return { Avx128IntegerReg(_mm256_castsi256_si128(from.bits)), Avx128IntegerReg(_mm256_extracti128_si256(from.bits, 1)) };
	}


	//template<std::same_as<Avx128IntegerReg> RegTo>
	//inline std::array<RegTo, 4> splitReg(Avx512IntegerReg from)
	//{
	//	return {
	//		RegTo(_mm512_castsi512_si128(from.bits)),
	//		RegTo(_mm512_extracti32x4_epi32(from.bits, 1)),
	//		RegTo(_mm512_extracti32x4_epi32(from.bits, 2)),
	//		RegTo(_mm512_extracti32x4_epi32(from.bits, 3)),
	//	};
	//}

	inline std::array<Avx256IntegerReg, 2> HECK_VECTORCALL splitReg(Avx512IntegerReg from)
	{
		return { Avx256IntegerReg(_mm512_castsi512_si256(from.bits)), Avx256IntegerReg(_mm512_extracti32x8_epi32(from.bits, 1)) };
	}

	template<class RegFrom, size_t kFrom>
	inline auto HECK_VECTORCALL multiJoinRegs(std::array<RegFrom, kFrom> parts)
	{
		if constexpr (kFrom == 1)
			return parts[0];
		else if constexpr (kFrom == 2)
			return joinRegs(parts);
		else if constexpr (std::has_single_bit(kFrom))
			return joinRegs({ multiJoinRegs(arraySubarray<kFrom / 2>(parts, 0)), multiJoinRegs(arraySubarray<kFrom / 2>(parts, kFrom / 2)) });
		else
			static_assert(false);
	}

	template<size_t kTo, class RegFrom>
	inline auto HECK_VECTORCALL multiSplitRegs(RegFrom r)
	{
		if constexpr (kTo == 1)
			return std::array{ r };
		else if constexpr (kTo == 2)
			return splitReg(r);
		else if constexpr (std::has_single_bit(kTo))
		{
			const auto [lo, hi] = splitReg(r);
			return arrayConcat(multiSplitRegs<kTo / 2>(lo), ultiSplitRegs<kTo / 2>(hi));
		}
		else
			static_assert(false);
	}

	/// [256|512x1|512x2] regs = [zs]ext2x([128|256|512] single reg)
	/// [512|512x2|512x4] regs = [zs]ext4x([128|256|512] single reg)
	/// [128|256|512] single reg = trunc2x([256|512x1|512x2] regs)
	/// [128|256|512] single reg = trunc4x([512|512x2|512x4] regs)
	/// If !kEnableAVX512, then 512 "regs" are 256x2.
	/// Note that truncation works like extension: The "single reg" determines the type of the reg array (except kEnableAVX512).
	/// But this means that there's no way to use "unusual" combinations of regs, like truncate 4x128 to 128.
	/// It is expected that the caller will use the joined reg size, upto the max reg size according to kEnableAVX512.

	template<class SingleRegType, size_t kFactor>
	struct ConversionPrimitive;

	template<class SingleRegType_, size_t kFactor, class ExtendedArrayRegType_>
	struct ConversionPrimitiveMixin
	{
		using SingleRegType = SingleRegType_;
		using ExtendedArrayRegType = ExtendedArrayRegType_;

		static constexpr size_t kExtendedArrayNumRegs = sizeof(SingleRegType) * kFactor / sizeof(ExtendedArrayRegType);
		static_assert(sizeof(SingleRegType_) * kFactor == sizeof(ExtendedArrayRegType_) * kExtendedArrayNumRegs);
		
		//using ExtendedArrayRegType = AvxRegSelByBytes<std::min<size_t>(sizeof(SingleRegType) * kFactor, (kEnableAVX512 || std::is_same_v<SingleRegType, Avx512IntegerReg> ? 512 : 256) / 8)>;
		
		using ExtendedArray = std::array<ExtendedArrayRegType, kExtendedArrayNumRegs>;
		//static constexpr bool kUsing512 = std::is_same_v<ExtendedArrayRegType, Avx512IntegerReg>;
	};

	template<>
	struct ConversionPrimitive<Avx128IntegerReg, 2> : ConversionPrimitiveMixin<Avx128IntegerReg, 2, Avx256IntegerReg>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			if constexpr (sizeof(FromT) == 1)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm256_cvtepi8_epi16(r.bits) } } };
				else
					return { { { _mm256_cvtepu8_epi16(r.bits) } } };
			else if constexpr (sizeof(FromT) == 2)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm256_cvtepi16_epi32(r.bits) } } };
				else
					return { { { _mm256_cvtepu16_epi32(r.bits) } } };
			else if constexpr (sizeof(FromT) == 4)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm256_cvtepi32_epi64(r.bits) } } };
				else
					return { { { _mm256_cvtepu32_epi64(r.bits) } } };
			else
				static_assert(false);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			if constexpr (sizeof(FromT) == 8)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_cvtepi64_epi32(r[0].bits) };
				else
				{
					const auto [lo, hi] = splitReg(r[0]);
					return { _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(lo.bits), _mm_castsi128_ps(hi.bits), _MM_SHUFFLE(2, 0, 2, 0))) };
				}
			}
			else if constexpr (sizeof(FromT) == 4)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_cvtepi32_epi16(r[0].bits) };
				else
				{
					const auto [lo, hi] = splitReg(Avx256IntegerReg{ _mm256_and_si256(r[0].bits, _mm256_set1_epi32(0xFFFF)) });
					return { _mm_packus_epi32(lo.bits, hi.bits) };
				}
			}
			else if constexpr (sizeof(FromT) == 2)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_cvtepi16_epi8(r[0].bits) };
				else
				{
					const auto [lo, hi] = splitReg(Avx256IntegerReg{ _mm256_and_si256(r[0].bits, _mm256_set1_epi16(0xFF)) });
					return { _mm_packus_epi16(lo.bits, hi.bits) };
				}
			}
			else
				static_assert(false);
		}
	};

	struct ConversionPrimitive_AVX128x4_AVX2 : ConversionPrimitiveMixin<Avx128IntegerReg, 4, Avx256IntegerReg>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			const __m128i hi = _mm_shuffle_epi32(r.bits, _MM_SHUFFLE(3, 2, 3, 2));
			if constexpr (sizeof(FromT) == 1)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm256_cvtepi8_epi32(r.bits) }, { _mm256_cvtepi8_epi32(hi) } } };
				else
					return { { { _mm256_cvtepu8_epi32(r.bits) }, { _mm256_cvtepu8_epi32(hi) } } };
			else if constexpr (sizeof(FromT) == 2)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm256_cvtepi16_epi64(r.bits) }, { _mm256_cvtepi16_epi64(hi) } } };
				else
					return { { { _mm256_cvtepu16_epi64(r.bits) }, { _mm256_cvtepu16_epi64(hi) } } };
			else
				static_assert(false);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			/*if constexpr (sizeof(FromT) == 8)
				return join64BitSSERegs({ _mm256_cvtepi64_epi16(r[0].bits) }, { _mm256_cvtepi64_epi16(r[1].bits) });
			else*/if constexpr (sizeof(FromT) == 4)
			{
				// AVX512!
				//return join64BitSSERegs({ _mm256_cvtepi32_epi8(r[0].bits) }, { _mm256_cvtepi32_epi8(r[1].bits) });
				// Instead, just shuffle.
				// https://stackoverflow.com/questions/70818047/how-do-i-convert-from-larger-integers-to-smaller-integers-using-avx2-and-sse
				const auto convert256 = [](__m256i x) {
					// Note that AVX2 is in-lane only.
					// There are 8 ints. This shuffle will put the first 4 ints into the first 4 bytes of the first lane, and the high 4 ints into the second 4 bytes of the second lane.
					const __m256i shuffle = _mm256_setr_epi8(
						0, 4, 8, 12, -1, -1, -1, -1,
						-1, -1, -1, -1, -1, -1, -1, -1,
						-1, -1, -1, -1, 0, 4, 8, 12,
						-1, -1, -1, -1, -1, -1, -1, -1);

					x = _mm256_shuffle_epi8(x, shuffle);
					const auto [lo, hi] = splitReg(Avx256IntegerReg{ x });
					return _mm_or_si128(lo.bits, hi.bits);
					};

				const auto lo = convert256(r[0].bits);
				const auto hi = convert256(r[1].bits);
				return { join64BitSSERegs({ lo }, { hi }) };
			}
			else
				static_assert(false);
		}
	};

	struct ConversionPrimitive_AVX128x4_AVX512 : ConversionPrimitiveMixin<Avx128IntegerReg, 4, Avx512IntegerReg>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			if constexpr (sizeof(FromT) == 1)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm512_cvtepi8_epi32(r.bits) } } };
				else
					return { { { _mm512_cvtepu8_epi32(r.bits) } } };
			else if constexpr (sizeof(FromT) == 2)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm512_cvtepi16_epi64(r.bits) } } };
				else
					return { { { _mm512_cvtepu16_epi64(r.bits) } } };
			else
				static_assert(false);
		}

		template<IntegerType FromT>
		static SingleRegType  truncate(ExtendedArray r)
		{
			if constexpr (sizeof(FromT) == 8)
				return { _mm512_cvtepi64_epi16(r[0].bits) };
			else if constexpr (sizeof(FromT) == 4)
				return { _mm512_cvtepi32_epi8(r[0].bits) };
			else
				static_assert(false);
		}
	};

	template<>
	struct ConversionPrimitive<Avx128IntegerReg, 4> : std::conditional_t<kEnableAVX512, ConversionPrimitive_AVX128x4_AVX512, ConversionPrimitive_AVX128x4_AVX2>
	{
	};

	struct ConversionPrimitive_AVX256x2_AVX2 : ConversionPrimitiveMixin<Avx256IntegerReg, 2, Avx256IntegerReg>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			const auto [lo, hi] = splitReg(r);
			return arrayConcat(
				ConversionPrimitive<Avx128IntegerReg, 2>::template extend<FromT>(lo),
				ConversionPrimitive<Avx128IntegerReg, 2>::template extend<FromT>(hi)
			);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			using HalfPrimitive = ConversionPrimitive<Avx128IntegerReg, 2>;
			return joinRegs(std::array{
				HalfPrimitive::template truncate<FromT>({ r[0] }),
				HalfPrimitive::template truncate<FromT>({ r[1] })
				});
		}
	};

	struct ConversionPrimitive_AVX256x2_AVX512 : ConversionPrimitiveMixin<Avx256IntegerReg, 2, Avx512IntegerReg>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			if constexpr (sizeof(FromT) == 1)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm512_cvtepi8_epi16(r.bits) } } };
				else
					return { { { _mm512_cvtepu8_epi16(r.bits) } } };
			else if constexpr (sizeof(FromT) == 2)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm512_cvtepi16_epi32(r.bits) } } };
				else
					return { { { _mm512_cvtepu16_epi32(r.bits) } } };
			else if constexpr (sizeof(FromT) == 4)
				if constexpr (std::is_signed_v<FromT>)
					return { { { _mm512_cvtepi32_epi64(r.bits) } } };
				else
					return { { { _mm512_cvtepu32_epi64(r.bits) } } };
			else
				static_assert(false);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			if constexpr (sizeof(FromT) == 8)
				return { _mm512_cvtepi64_epi32(r[0].bits) };
			else if constexpr (sizeof(FromT) == 4)
				return { _mm512_cvtepi32_epi16(r[0].bits) };
			else if constexpr (sizeof(FromT) == 2)
				return { _mm512_cvtepi16_epi8(r[0].bits) };
			else
				static_assert(false);
		}
	};

	template<>
	struct ConversionPrimitive<Avx256IntegerReg, 2> : std::conditional_t<kEnableAVX512, ConversionPrimitive_AVX256x2_AVX512, ConversionPrimitive_AVX256x2_AVX2>
	{
	};

	struct ConversionPrimitive_AVX256x4_AVX2 : ConversionPrimitiveMixin<Avx256IntegerReg, 4, Avx256IntegerReg>
	{
		using HalfPrimitive = ConversionPrimitive_AVX128x4_AVX2;

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			return joinRegs(std::array{
				HalfPrimitive::truncate<FromT>({ r[0], r[1] }),
				HalfPrimitive::truncate<FromT>({ r[2], r[3] }),
				});
		}
	};

	struct ConversionPrimitive_AVX256x4_AVX512 : ConversionPrimitiveMixin<Avx256IntegerReg, 4, Avx512IntegerReg>
	{
		using HalfPrimitive = ConversionPrimitive_AVX128x4_AVX512;

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			return joinRegs(std::array{
				HalfPrimitive::truncate<FromT>({ r[0] }),
				HalfPrimitive::truncate<FromT>({ r[1] })
				});
		}
	};

	template<>
	struct ConversionPrimitive<Avx256IntegerReg, 4> : std::conditional_t<kEnableAVX512, ConversionPrimitive_AVX256x4_AVX512, ConversionPrimitive_AVX256x4_AVX2>
	{
		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			using HalfPrimitive2 = ConversionPrimitive<Avx128IntegerReg, 4>;
			const auto [lo, hi] = splitReg(r);
			return arrayConcat(
				HalfPrimitive2::template extend<FromT>(lo),
				HalfPrimitive2::template extend<FromT>(hi)
			);
		}
	};

	template<>
	struct ConversionPrimitive<Avx512IntegerReg, 2> : ConversionPrimitiveMixin<Avx512IntegerReg, 2, Avx512IntegerReg>
	{
		using HalfPrimitive = ConversionPrimitive_AVX256x2_AVX512;

		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			const auto [lo, hi] = splitReg(r);
			return arrayConcat(
				HalfPrimitive::template extend<FromT>(lo),
				HalfPrimitive::template extend<FromT>(hi)
			);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			return joinRegs(std::array{
				HalfPrimitive::template truncate<FromT>({ r[0] }),
				HalfPrimitive::template truncate<FromT>({ r[1] })
				});
		}
	};

	template<>
	struct ConversionPrimitive<Avx512IntegerReg, 4> : ConversionPrimitiveMixin<Avx512IntegerReg, 4, Avx512IntegerReg>
	{
		using QuarterPrimitive = ConversionPrimitive_AVX128x4_AVX512;

		template<IntegerType FromT>
		static ExtendedArray HECK_VECTORCALL extend(SingleRegType r)
		{
			const auto [lo, hi] = splitReg(r);
			const auto [q0, q1] = splitReg(lo);
			const auto [q2, q3] = splitReg(hi);
			return arrayConcat(
				QuarterPrimitive::template extend<FromT>(q0),
				QuarterPrimitive::template extend<FromT>(q1),
				QuarterPrimitive::template extend<FromT>(q2),
				QuarterPrimitive::template extend<FromT>(q3)
			);
		}

		template<IntegerType FromT>
		static SingleRegType HECK_VECTORCALL truncate(ExtendedArray r)
		{
			return multiJoinRegs(std::array{
				QuarterPrimitive::template truncate<FromT>({ r[0] }),
				QuarterPrimitive::template truncate<FromT>({ r[1] }),
				QuarterPrimitive::template truncate<FromT>({ r[2] }),
				QuarterPrimitive::template truncate<FromT>({ r[3] })
				});
		}
	};
}