#pragma once

#include "SimdAvxRegs.h"
#include "SimdRegOpsAvx128.h"
#include "SimdConversionsAvx.h"
#include "SimdAvxBitMask.h"
#include "SimdAvxRegMask.h"
#include "SimdAvx2Polyfills.h"

#include <cassert>

namespace heck::simd
{
	template<IntegerType T>
	struct RegOps<Avx256IntegerReg, T>
	{
		using Reg = Avx256IntegerReg;
		static constexpr int kNumElements = sizeof(Reg) / sizeof(T);
		static_assert(sizeof(Reg) == sizeof(__m256i));

		static constexpr RegTraits kTraits{
			.supportsGather = true,
			.supportsScatter = kEnableAVX512,
			.supportsMaskedOps = kEnableAVX512,
			.supportsCompress = kEnableAVX512,
			.isNative = true,
			.maxGatherScatterScale = 8,
		};

		using MaskAVX2 = AvxRegMask<Reg, sizeof(T)>;
		using MaskAVX512 = AvxBitMask<kNumElements>;
		using Mask = std::conditional_t<kEnableAVX512, MaskAVX512, MaskAVX2>;

		static constexpr Reg initAllBits()
		{
			if (std::is_constant_evaluated())
				return initBroadcast(static_cast<T>(-1));
			else
			{
				const Reg r{ _mm256_undefined_si256() };
				return { _mm256_cmpeq_epi32(r.bits, r.bits) };
			}
		}

		static constexpr Reg initBroadcast(T value)
		{
			if (std::is_constant_evaluated())
				return initBuild(filledArray<T, kNumElements>(value));
			else if constexpr (sizeof(T) == 1)
				return { _mm256_set1_epi8(value) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_set1_epi16(value) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_set1_epi32(value) };
			else
				static_assert(false);
		}

		static constexpr Reg initBuild(std::array<T, kNumElements> values)
		{
#ifndef HECK_USING_GCC_SIMD
			if (std::is_constant_evaluated())
			{
				Reg reg{};
				std::ranges::copy(std::bit_cast<std::array<uint8_t, sizeof(values)>>(values), reg.bits.m256i_u8);
				return reg;
			}
			else
#endif
				return std::bit_cast<Reg>(values);
		}

		static Reg initMaskRegFromBits(uint32_t bits)
		{
			if constexpr (sizeof(T) == 1)
			{
				// 0x87654321 -> 0x87878787'87878787|65656565'65656565|43434343'43434343|21212121'21212121
				//static constexpr __m256i kShuffle = [] {
				//	std::array<uint8_t, kNumElements> x{};
				//	for (size_t i = 0; i < kNumElements; ++i)
				//		x[i] = i / 8;
				//	return initBuild(x);
				//	}();
				//return { .bits = (_mm256_shuffle_epi8(_mm256_set_epi64x(0, bits, 0, bits), kShuffle) & mask) != 0 });
				static_assert(false);
			}
			else if constexpr (sizeof(T) == 2)
			{
				const uint64_t lo = _pdep_u64(bits, 0x01010101'01010101);
				const uint64_t hi = _pdep_u64(bits >> (kNumElements / 2), 0x01010101'01010101);
				return Reg(_mm256_cmpgt_epi16(_mm256_cvtepi8_epi16(_mm_set_epi64x(hi, lo)), Reg{}.bits));
			}
			else if constexpr (sizeof(T) == 4)
			{
				const uint64_t lo = _pdep_u64(bits, 0x01010101'01010101);
				return Reg(_mm256_cmpgt_epi32(_mm256_cvtepi8_epi32(_mm_cvtsi64_si128(lo)), Reg{}.bits));
			}
			else
				static_assert(false);
		}

		static Reg initLoadUnaligned(std::span<const T, kNumElements> values)
		{
			return { _mm256_loadu_si256(reinterpret_cast<const __m256i*>(values.data())) };
		}

		static Reg initGather(std::span<const T> array, Reg indices, MaskAVX2 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_mask_i32gather_epi32(_mm256_setzero_si256(), reinterpret_cast<const int*>(array.data()), indices.bits, mask.reg.bits, sizeof(T)) };
			else
				static_assert(false);
		}

		static Reg initGather(std::span<const T> array, Reg indices, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_mmask_i32gather_epi32(_mm256_setzero_si256(), mask.bits, indices.bits, array.data(), sizeof(T)) };
			else
				static_assert(false);
		}

		static Reg initGather(std::span<const T> array, Reg indices)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_i32gather_epi32(reinterpret_cast<const int*>(array.data()), indices.bits, sizeof(T)) };
			else
				static_assert(false);
		}

		template<size_t kScale>
		static Reg initRawGather(std::span<const std::byte> array, Reg indices, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_i32gather_epi32(reinterpret_cast<const int*>(array.data()), indices.bits, kScale) };
			else
				static_assert(false);
		}

	private:
		static __m256 castToFP(__m256i r)
		{
			return _mm256_castsi256_ps(r);
		}

		static __m256i castFromFP(__m256 r)
		{
			return _mm256_castps_si256(r);
		}

	public:

		static Reg broadcast(Reg r, size_t i)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_permutevar8x32_epi32(r.bits, initBroadcast(i).bits) };
			else if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 1)
					return { _mm256_permutexvar_epi8(initBroadcast(i).bits, r.bits) };
				else if constexpr (sizeof(T) == 2)
					return { _mm256_permutexvar_epi16(initBroadcast(i).bits, r.bits) };
				else
					static_assert(false);
			}
			else
				return RegOps<Avx128IntegerReg, T>::broadcast(splitReg(r)[i / (kNumElements / 2)], i % (kNumElements / 2));
		}


		template<size_t i>
		static void set(Reg& r, T value)
		{
			if constexpr (sizeof(T) == 1)
				r.bits = _mm256_insert_epi8(r.bits, value, i);
			else if constexpr (sizeof(T) == 2)
				r.bits = _mm256_insert_epi16(r.bits, value, i);
			else if constexpr (sizeof(T) == 4)
				r.bits = _mm256_insert_epi32(r.bits, value, i);
			else
				static_assert(false);
		}

		template<size_t i>
		static T get(Reg r)
		{
			if constexpr (sizeof(T) == 1)
				return static_cast<T>(_mm256_extract_epi8(r.bits, i));
			else if constexpr (sizeof(T) == 2)
				return static_cast<T>(_mm256_extract_epi16(r.bits, i));
			else if constexpr (sizeof(T) == 4)
				return static_cast<T>(_mm256_extract_epi32(r.bits, i));
			else
				static_assert(false);
		}

		static void set(Reg& r, size_t i, T value)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				r.bits.m256i_u8[i] = value;
			else if constexpr (sizeof(T) == 2)
				r.bits.m256i_u16[i] = value;
			else if constexpr (sizeof(T) == 4)
				r.bits.m256i_u32[i] = value;
#else
			if constexpr (sizeof(T) == 1)
				reinterpret_cast<__v32qu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 2)
				reinterpret_cast<__v16hu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 4)
				reinterpret_cast<__v8su&>(r.bits)[i] = value;
#endif
			else
				static_assert(false);
		}

		static T get(Reg r, size_t i)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				return r.bits.m256i_u8[i];
			else if constexpr (sizeof(T) == 2)
				return r.bits.m256i_u16[i];
			else if constexpr (sizeof(T) == 4)
				return r.bits.m256i_u32[i];
#else
			if constexpr (sizeof(T) == 1)
				return reinterpret_cast<const __v32qu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 2)
				return reinterpret_cast<const __v16hu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 4)
				return reinterpret_cast<const __v8su&>(r.bits)[i];
#endif
			else
				static_assert(false);
		}

		static Reg add(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm256_add_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_add_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_add_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg sub(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm256_sub_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_sub_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_sub_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedadd(MaskAVX512 mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm256_mask_add_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_mask_add_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_mask_add_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedsub(MaskAVX512 mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm256_mask_sub_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_mask_sub_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_mask_sub_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mul(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
			{
				//if constexpr (kEnableAVX512)
				// Looks we need to get rid of kEnableAVX512 and just use a macro...
#ifdef __AVX512F__
				{
					const Avx512IntegerReg a16 = ConversionPrimitive<Reg, 2>::template extend<T>(a)[0];
					const Avx512IntegerReg b16 = ConversionPrimitive<Reg, 2>::template extend<T>(b)[0];
					const Avx512IntegerReg p16{ _mm512_mullo_epi16(a16.bits, b16.bits) };
					return ConversionPrimitive<Reg, 2>::template truncate<uint16_t>({ p16 });
				}
				//else
#else
				{
					// Mul by conversion to 16-bit.
					const auto [lo8a, hi8a] = splitReg(a);
					const auto [lo8b, hi8b] = splitReg(b);
					using HalfReg = std::remove_cvref_t<decltype(lo8a)>;
					const Reg lo16a = ConversionPrimitive<HalfReg, 2>::template extend<T>(lo8a)[0];
					const Reg hi16a = ConversionPrimitive<HalfReg, 2>::template extend<T>(hi8a)[0];
					const Reg lo16b = ConversionPrimitive<HalfReg, 2>::template extend<T>(lo8b)[0];
					const Reg hi16b = ConversionPrimitive<HalfReg, 2>::template extend<T>(hi8b)[0];
					const Reg lo = RegOps<Reg, uint16_t>::mul(lo16a, lo16b);
					const Reg hi = RegOps<Reg, uint16_t>::mul(hi16a, hi16b);
					return ConversionPrimitive<Reg, 2>::template truncate<uint16_t>({ lo, hi });
				}
#endif
			}
			else if constexpr (sizeof(T) == 2)
				return { _mm256_mullo_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_mullo_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mulhi(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>)
					return { _mm256_mulhi_epi16(a.bits, b.bits) };
				else
					return { _mm256_mulhi_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
			{
				if constexpr (kEnableAVX512)
				{
					constexpr auto widen = [](__m256i a) {
						if constexpr (std::is_signed_v<T>)
							return _mm512_cvtepi32_epi64(a);
						else
							return _mm512_cvtepu32_epi64(a);
						};

					const auto lo64sA = widen(a.bits);
					const auto lo64sB = widen(b.bits);
					const auto lo64sP = _mm512_mullo_epi64(lo64sA, lo64sB);
					const auto lo32sP = _mm512_cvtepi64_epi32(_mm512_srli_epi64(lo64sP, 32));
					return Reg(lo32sP);
				}
				else
				{
					const auto [loA, hiA] = splitReg(a);
					const auto [loB, hiB] = splitReg(b);
					using HalfOps = RegOps<Avx128IntegerReg, T>;
					return joinRegs({ HalfOps::mulhi(loA, loB), HalfOps::mulhi(hiA, hiB) });

					//constexpr auto widen = [](__m128i a) {
					//	if constexpr (std::is_signed_v<T>)
					//		return _mm256_cvtepi32_epi64(a);
					//	else
					//		return _mm256_cvtepu32_epi64(a);
					//	};
					//
					//const auto [lo32sA, hi32sA] = splitReg(a);
					//const auto [lo32sB, hi32sB] = splitReg(b);
					//const auto [lo64sA, hi64sA] = std::array{ widen(lo32sA.bits), widen(hi32sA.bits) };
					//const auto [lo64sB, hi64sB] = std::array{ widen(lo32sB.bits), widen(hi32sB.bits) };
					//const auto lo64sP = _mm256_mullo_epi64(lo64sA, lo64sB);
					//const auto hi64sP = _mm256_mullo_epi64(hi64sA, hi64sB);
					//const auto lo32sP = _mm256_cvtepi64_epi32(_mm256_srli_epi64(lo64sP, 32));
					//const auto hi32sP = _mm256_cvtepi64_epi32(_mm256_srli_epi64(hi64sP, 32));
					//return joinRegs({ Avx128IntegerReg{ lo32sP }, Avx128IntegerReg{ hi32sP } });
				}
			}
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shl(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				return { _mm256_slli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_slli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shr(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm256_srai_epi16(a.bits, n) }; else return { _mm256_srli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm256_srai_epi32(a.bits, n) }; else return { _mm256_srli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg maskedshr(MaskAVX512 mask, Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm256_mask_srai_epi16(a.bits, mask.bits, a.bits, n) }; else return { _mm256_mask_srli_epi16(a.bits, mask.bits, a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm256_mask_srai_epi32(a.bits, mask.bits, a.bits, n) }; else return { _mm256_mask_srli_epi32(a.bits, mask.bits, a.bits, n) };
			else
				static_assert(false);
		}

		static Reg shl(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_sllv_epi16(a.bits, b.bits) };
				else
				{
					const auto [loA, hiA] = splitReg(a);
					const auto [loB, hiB] = splitReg(b);
					using HalfOps = RegOps<Avx128IntegerReg, T>;
					return joinRegs({ HalfOps::shl(loA, loB), HalfOps::shl(hiA, hiB) });
				}
			}
			else if constexpr (sizeof(T) == 4)
				return { _mm256_sllv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg shr(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					if constexpr (std::is_signed_v<T>) return { _mm256_srav_epi16(a.bits, b.bits) }; else return { _mm256_srlv_epi16(a.bits, b.bits) };
				else
				{
					const auto [loA, hiA] = splitReg(a);
					const auto [loB, hiB] = splitReg(b);
					using HalfOps = RegOps<Avx128IntegerReg, T>;
					return joinRegs({ HalfOps::shr(loA, loB), HalfOps::shr(hiA, hiB) });
				}
			}
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm256_srav_epi32(a.bits, b.bits) }; else return { _mm256_srlv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg div(Reg a, Reg b)
		{
			assert(vcmpeq(b, {}).none());

			if constexpr (sizeof(T) == 4)
			{
				const auto convertToPS = [](__m256i a) -> __m256 {
					// As `a < 2^24`, this works for unsigned too.
					return _mm256_cvtepi32_ps(a);
					};
				const auto convertFromPS = [](__m256 a) -> __m256i {
					//if constexpr (std::is_signed_v<T>)
					// As `a < 2^24`, this works for unsigned too.
						return _mm256_cvttps_epi32(a);
					//else if constexpr (kEnableAVX512)
					//	return _mm256_cvttps_epu32(a);
					//else
					//{
					//	// Convert by truncation from 64-bit.
					//	const __m128 lo = _mm256_extractf128_ps(a, 0);
					//	const __m128 hi = _mm256_extractf128_ps(a, 1);
					//	const __m256i lo64 = _mm256_cvttps_epi64(lo);
					//	const __m256i hi64 = _mm256_cvttps_epi64(hi);
					//	const __m128i lo32 = _mm256_cvtepi64_epi32(lo64);
					//	const __m128i hi32 = _mm256_cvtepi64_epi32(hi64);
					//	return joinRegs({ Avx128IntegerReg{ lo32 }, Avx128IntegerReg{ hi32 } }).bits;
					//}
					};

				const auto convertTo256PD = [](__m128i a) -> __m256d {
					if constexpr (std::is_signed_v<T>)
						return _mm256_cvtepi32_pd(a);
					else
						return polyfills::convertU32ToDouble(a);
					};
				const auto convertFrom256PD = [](__m256d a) -> __m128i {
					if constexpr (std::is_signed_v<T>)
						return _mm256_cvttpd_epi32(a);
					else
						return polyfills::convertU32RangeDoubleToU32(a);
					};

				const auto convertTo512PD = [](__m256i a) -> __m512d {
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvtepi32_pd(a);
					else
						return _mm512_cvtepu32_pd(a);
				};

				const auto convertFrom512PD = [](__m512d a) -> __m256i {
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvttpd_epi32(a);
					else
						return _mm512_cvttpd_epu32(a);
					};

				using UOps = RegOps<Reg, std::make_unsigned_t<T>>;

				if constexpr (kEnableAVX512)
				{
					// Just use DP.
					return { convertFrom512PD(_mm512_div_pd(convertTo512PD(a.bits), convertTo512PD(b.bits))) };
				}
				else if (UOps::vcmplt(UOps::vmax(vabs(a), vabs(b)), initBroadcast(1 << 24)).all()) [[likely]]
				{
					// 32-bit FP is enough.
					return { convertFromPS(_mm256_div_ps(convertToPS(a.bits), convertToPS(b.bits))) };
				}
				else
				{
					// Need 64-bit FP.
					const auto [a0, a1] = splitReg(a);
					const auto [b0, b1] = splitReg(b);
					const auto fpA0 = convertTo256PD(a0.bits);
					const auto fpB0 = convertTo256PD(b0.bits);
					const auto q0 = convertFrom256PD(_mm256_div_pd(fpA0, fpB0));
					const auto fpA1 = convertTo256PD(a1.bits);
					const auto fpB1 = convertTo256PD(b1.bits);
					const auto q1 = convertFrom256PD(_mm256_div_pd(fpA1, fpB1));
					return joinRegs({ Avx128IntegerReg{ q0 }, Avx128IntegerReg{ q1 } });
				}
			}
			else
				static_assert(false);
		}

		template<std::array<size_t, kNumElements> kIndices>
		static Reg swizzle(Reg a)
		{
			static constexpr Reg kIndicesReg = initBuild(convertArray<std::make_unsigned_t<T>>(kIndices));
			return permute(a, kIndicesReg);
		}

		static Reg permute(Reg a, Reg indices)
		{
			if constexpr (sizeof(T) == 1)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_permutexvar_epi8(indices.bits, a.bits) };
				else
				{
					// Mask out the zeromask.
					indices = vbitand(indices, initBroadcast(kNumElements - 1));

					// Clang's method.
					// Form lo pair and hi pair.
					const __m256i lopair = _mm256_inserti128_si256(a.bits, _mm256_castsi256_si128(a.bits), 1);
					const __m256i hipair = _mm256_permute4x64_epi64(a.bits, 0b11'10'11'10);
					// Perform two shuffles.
					const __m256i lopairShuffle = _mm256_shuffle_epi8(lopair, indices.bits);
					const __m256i hipairShuffle = _mm256_shuffle_epi8(hipair, indices.bits);
					// Blend.
					const __m256i blendMask = vcmplt(indices, initBroadcast(sizeof(Reg) / 2)).reg.bits;
					return { _mm256_blendv_epi8(hipairShuffle, lopairShuffle, blendMask) };
				}
			}
			else if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return { _mm256_permutexvar_epi16(indices.bits, a.bits) };
				else
				{
					// Clang's method.
					// Convert to byte permute.
					indices = vbitand(indices, initBroadcast(kNumElements - 1));
					indices = mul(indices, initBroadcast(0x0202));
					indices = add(indices, initBroadcast(0x0100));
					return RegOps<Reg, uint8_t>::permute(a, indices);
				}
			}
			else if constexpr (sizeof(T) == 4)
				return { _mm256_permutevar8x32_epi32(a.bits, indices.bits) };
			else
				static_assert(false);
		}

		//template<size_t k>
		//static Reg swizzleShiftLeft(Reg a, std::integral_constant<size_t, k>)
		//{
		//	//static constexpr Reg kIndicesReg = initBuild(iotaArray<std::make_unsigned_t<T>, kNumElements>());
		//	//const Reg shiftedIota = sub(kIndicesReg, initBroadcast(T(n)));
		//	//a = swizzle(a, shiftedIota);
		//	//// Zeroed elements will have negative overflow in shiftedIota. Shift to create a mask.
		//	//a.bits = _mm256_andnot_si256(_mm256_srai_epi32(shiftedIota.bits, 31), a.bits);
		//	//return a;
		//
		//	static constexpr size_t kShiftRightAmountBytes = (kNumElements - k) * sizeof(T);
		//
		//	if constexpr (kEnableAVX512 && kShiftRightAmountBytes % 4 == 0)
		//		return { _mm256_alignr_epi32(a.bits, {}, kShiftRightAmountBytes / 4) };
		//	else
		//	{
		//		if constexpr (k > kNumElements / 2)
		//			// Big shift. First, shift by 128 bits (take low half as high half), then shift by the remainder.
		//			return joinRegs({ Avx128IntegerReg(), RegOps<Avx128IntegerReg, T>::template swizzleShiftLeft<k - kNumElements / 2>(splitReg(a)[0], {}) });
		//		else if constexpr (k == kNumElements / 2)
		//			// Exact shift low half to high half.
		//			return joinRegs({ Avx128IntegerReg(), splitReg(a)[0] });
		//		else
		//		{
		//			// Small shift.
		//			const __m256i inlaneShift = _mm256_bslli_epi128(a.bits, k * sizeof(T));
		//			const __m128i inlaneOppShift = _mm_bsrli_si128(splitReg(a)[0].bits, (kNumElements / 2 - k) * sizeof(T));
		//			return vbitor(inlaneShift, joinRegs({ Avx128IntegerReg(), inlaneOppShift() });
		//		}
		//	}
		//}


		static Reg vmin(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm256_min_epi8(a.bits, b.bits) }; else return { _mm256_min_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm256_min_epi16(a.bits, b.bits) }; else return { _mm256_min_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm256_min_epi32(a.bits, b.bits) }; else return { _mm256_min_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vmax(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm256_max_epi8(a.bits, b.bits) }; else return { _mm256_max_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm256_max_epi16(a.bits, b.bits) }; else return { _mm256_max_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm256_max_epi32(a.bits, b.bits) }; else return { _mm256_max_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vabs(Reg a)
		{
			if constexpr (std::is_unsigned_v<T>)
				return a;
			else if constexpr (sizeof(T) == 1)
				return { _mm256_abs_epi8(a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_abs_epi16(a.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm256_abs_epi32(a.bits) };
			else
				static_assert(false);
		}

		static Reg vbitand(Reg a, Reg b)
		{
			return { _mm256_and_si256(a.bits, b.bits) };
		}

		static Reg vbitor(Reg a, Reg b)
		{
			return { _mm256_or_si256(a.bits, b.bits) };
		}

		static Reg vbitxor(Reg a, Reg b)
		{
			return { _mm256_xor_si256(a.bits, b.bits) };
		}

		static Reg vbitnot(Reg a)
		{
			return vbitxor(a, initAllBits());
		}

		static T hmin(Reg a)
		{
			using HalfOps = RegOps<Avx128IntegerReg, T>;
			const auto [lo, hi] = splitReg(a);
			return HalfOps::hmin(HalfOps::vmin(lo, hi));

		}

		static T hmax(Reg a)
		{
			using HalfOps = RegOps<Avx128IntegerReg, T>;
			const auto [lo, hi] = splitReg(a);
			return HalfOps::hmax(HalfOps::vmax(lo, hi));
		}

		//template<class = void>
		static Mask vcmpeq(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm256_cmpeq_epi32_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm256_cmpeq_epi32(a.bits, b.bits)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm256_cmpeq_epi16_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm256_cmpeq_epi16(a.bits, b.bits)));
			}
			else if constexpr (sizeof(T) == 1)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm256_cmpeq_epi8_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm256_cmpeq_epi8(a.bits, b.bits)));
			}
			else
				static_assert(false);
		}

		//template<class = void>
		static Mask vtest(Reg a, Reg b)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					return MaskAVX512(_mm256_test_epi32_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 2)
					return MaskAVX512(_mm256_test_epi16_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 1)
					return MaskAVX512(_mm256_test_epi8_mask(a.bits, b.bits));
				else
					static_assert(false);
			}
			else
				return MaskAVX2(Reg(vbitnot(vcmpeq(vbitand(a, b), {}).reg)));
		}

		//template<class = void>
		static Mask vcmplt(Reg a, Reg b)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmpgt_epi32_mask(b.bits, a.bits)); else return MaskAVX512(_mm256_cmpgt_epu32_mask(b.bits, a.bits));
				else if constexpr (sizeof(T) == 2)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmpgt_epi16_mask(b.bits, a.bits)); else return MaskAVX512(_mm256_cmpgt_epu16_mask(b.bits, a.bits));
				else if constexpr (sizeof(T) == 1)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmpgt_epi8_mask(b.bits, a.bits)); else return MaskAVX512(_mm256_cmpgt_epu8_mask(b.bits, a.bits));
				else
					static_assert(false);
			}
			else
			{
				if constexpr (std::is_signed_v<T>)
				{
					if constexpr (sizeof(T) == 4)
						return MaskAVX2(Reg(_mm256_cmpgt_epi32(b.bits, a.bits)));
					else if constexpr (sizeof(T) == 2)
						return MaskAVX2(Reg(_mm256_cmpgt_epi16(b.bits, a.bits)));
					else if constexpr (sizeof(T) == 1)
						return MaskAVX2(Reg(_mm256_cmpgt_epi8(b.bits, a.bits)));
					else
						static_assert(false);
				}
				else
					return MaskAVX2(vbitnot(vcmple(b, a).reg));
			}
		}

		//template<class = void>
		static Mask vcmple(Reg a, Reg b)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmple_epi32_mask(a.bits, b.bits)); else return MaskAVX512(_mm256_cmple_epu32_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 2)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmple_epi16_mask(a.bits, b.bits)); else return MaskAVX512(_mm256_cmple_epu16_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 1)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm256_cmple_epi8_mask(a.bits, b.bits)); else return MaskAVX512(_mm256_cmple_epu8_mask(a.bits, b.bits));
				else
					static_assert(false);
			}
			else
				return MaskAVX2(vcmpeq(vmax(a, b), b));
		}

		static Reg vselect(Mask cond, Reg ifTrue, Reg ifFalse)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					return { _mm256_mask_mov_epi32(ifFalse.bits, cond.bits, ifTrue.bits) };
				else if constexpr (sizeof(T) == 2)
					return { _mm256_mask_mov_epi16(ifFalse.bits, cond.bits, ifTrue.bits) };
				else if constexpr (sizeof(T) == 1)
					return { _mm256_mask_mov_epi8(ifFalse.bits, cond.bits, ifTrue.bits) };
				else
					static_assert(false);
			}
			else
				return { _mm256_blendv_epi8(ifFalse.bits, ifTrue.bits, cond.reg.bits) };
		}

		static auto getHighBits(Reg r)
		{
			if constexpr (kEnableAVX512)
				return vtest(r, initBroadcast(kHighBit<T>)).bits;
			else if constexpr (sizeof(T) == 4)
				return static_cast<uint8_t>(_mm256_movemask_ps(castToFP(r.bits)));
			else if constexpr (sizeof(T) == 2)
				return static_cast<uint16_t>(_pext_u32(_mm256_movemask_epi8(r.bits), 0xAAAAAAAA));
			else if constexpr (sizeof(T) == 1)
				return static_cast<uint32_t>(_mm256_movemask_epi8(r.bits));
			else
				static_assert(false);
		}

		static Reg hcompress(Reg a, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm256_maskz_compress_epi32(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm256_maskz_compress_epi16(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 1)
				return { _mm256_maskz_compress_epi8(mask.bits, a.bits) };
			else
				static_assert(false);
		}

		static void compressStore(std::span<T> out, Reg a, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm256_mask_compressstoreu_epi32(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 2)
				_mm256_mask_compressstoreu_epi16(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 1)
				_mm256_mask_compressstoreu_epi8(out.data(), mask.bits, a.bits);
			else
				static_assert(false);
		}

		static void scatter(std::span<T> array, Reg indices, Reg values, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm256_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, sizeof(T));
			else
				static_assert(false);
		}

		template<size_t kScale>
		static void scatter(std::span<std::byte> array, Reg indices, Reg values, MaskAVX512 mask, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				_mm256_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, kScale);
			else
				static_assert(false);
		}
	};
}