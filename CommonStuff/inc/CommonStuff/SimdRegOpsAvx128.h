#pragma once

#include "CompilerMacros.h"
#include "SimdAvxRegs.h"
#include "SimdAvxBitMask.h"
#include "SimdAvxRegMask.h"
#include "SimdConversionsAvx.h"
#include "SimdAvx2Polyfills.h"

#include <cassert>

namespace heck::simd
{
	template<IntegerType T>
	struct RegOps<Avx128IntegerReg, T>
	{
		using Reg = Avx128IntegerReg;
		static constexpr int kNumElements = sizeof(Reg) / sizeof(T);
		static_assert(sizeof(Reg) == sizeof(__m128i));

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
				const Reg r{ _mm_undefined_si128() };
				return { _mm_cmpeq_epi32(r.bits, r.bits) };
			}
		}

		static constexpr Reg initBroadcast(T value)
		{
			if (std::is_constant_evaluated())
				return initBuild(filledArray<T, kNumElements>(value));
			else if constexpr (sizeof(T) == 1)
				return { _mm_set1_epi8(value) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_set1_epi16(value) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_set1_epi32(value) };
			else
				static_assert(false);
		}

		static constexpr Reg initBuild(std::array<T, kNumElements> values)
		{
#ifndef HECK_USING_GCC_SIMD
			if (std::is_constant_evaluated())
			{
				Reg reg{};
				std::ranges::copy(std::bit_cast<std::array<uint8_t, sizeof(values)>>(values), reg.bits.m128i_u8);
				return reg;
			}
			else
#endif
				return std::bit_cast<Reg>(values);
		}

		static Reg initMaskRegFromBits(unsigned int bits)
		{
			if constexpr (sizeof(T) == 1)
			{
				const uint64_t lo = _pdep_u64(bits, 0x01010101'01010101);
				const uint64_t hi = _pdep_u64(bits >> (kNumElements / 2), 0x01010101'01010101);
				return Reg(_mm_cmplt_epi8(Reg{}.bits, _mm_set_epi64x(hi, lo)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				const uint64_t lo = _pdep_u64(bits, 0x01010101'01010101);
				return Reg(_mm_cmplt_epi16(Reg{}.bits, _mm_cvtepi8_epi16(_mm_cvtsi64_si128(lo))));
			}
			else if constexpr (sizeof(T) == 4)
			{
				const uint32_t lo = _pdep_u32(bits, 0x01010101);
				return Reg(_mm_cmplt_epi32(Reg{}.bits, _mm_cvtepi8_epi32(_mm_cvtsi32_si128(lo))));
			}
			else
				static_assert(false);
		}

		static Reg initLoadUnaligned(std::span<const T, kNumElements> values)
		{
			return { _mm_loadu_si128(reinterpret_cast<const __m128i*>(values.data())) };
		}

		static Reg initGather(std::span<const T> array, Reg indices, MaskAVX2 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm_mask_i32gather_epi32(_mm_setzero_si128(), reinterpret_cast<const int*>(array.data()), indices.bits, mask.reg.bits, sizeof(T)) };
			else
				static_assert(false);
		}

		static Reg initGather(std::span<const T> array, Reg indices, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm_mmask_i32gather_epi32(_mm_setzero_si128(), mask.bits, indices.bits, array.data(), sizeof(T)) };
			else
				static_assert(false);
		}

		static Reg initGather(std::span<const T> array, Reg indices)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm_i32gather_epi32(reinterpret_cast<const int*>(array.data()), indices.bits, sizeof(T)) };
			else
				static_assert(false);
		}

		template<size_t kScale>
		static Reg initRawGather(std::span<const std::byte> array, Reg indices, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm_i32gather_epi32(reinterpret_cast<const int*>(array.data()), indices.bits, kScale) };
			else
				static_assert(false);
		}

	private:
		static __m128 castToFP(__m128i r)
		{
			return _mm_castsi128_ps(r);
		}

		static __m128i castFromFP(__m128 r)
		{
			return _mm_castps_si128(r);
		}

	public:

		static Reg broadcast(Reg r, size_t i)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm_shuffle_epi8(r.bits, initBroadcast(i).bits) };
			else if constexpr (sizeof(T) == 2)
			{
				i *= 2;
				return { _mm_shuffle_epi8(r.bits, initBroadcast(static_cast<T>(i | ((i + 1) << 8))).bits) };
			}
			else if constexpr (sizeof(T) == 4)
				return { castFromFP(_mm_permutevar_ps(castToFP(r.bits), initBroadcast(i).bits)) };
			else
				static_assert(false);
		}


		template<size_t i>
		static void set(Reg& r, T value)
		{
			if constexpr (sizeof(T) == 1)
				r.bits = _mm_insert_epi8(r.bits, value, i);
			else if constexpr (sizeof(T) == 2)
				r.bits = _mm_insert_epi16(r.bits, value, i);
			else if constexpr (sizeof(T) == 4)
				r.bits = _mm_insert_epi32(r.bits, value, i);
			else
				static_assert(false);
		}

		template<size_t i>
		static T get(Reg r)
		{
			if constexpr (sizeof(T) == 1)
				return static_cast<T>(_mm_extract_epi8(r.bits, i));
			else if constexpr (sizeof(T) == 2)
				return static_cast<T>(_mm_extract_epi16(r.bits, i));
			else if constexpr (sizeof(T) == 4)
				return static_cast<T>(_mm_extract_epi32(r.bits, i));
			else
				static_assert(false);
		}

		static void set(Reg& r, size_t i, T value)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				r.bits.m128i_u8[i] = value;
			else if constexpr (sizeof(T) == 2)
				r.bits.m128i_u16[i] = value;
			else if constexpr (sizeof(T) == 4)
				r.bits.m128i_u32[i] = value;
#else
			if constexpr (sizeof(T) == 1)
				reinterpret_cast<__v16qu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 2)
				reinterpret_cast<__v8hu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 4)
				reinterpret_cast<__v4su&>(r.bits)[i] = value;
#endif
			else
				static_assert(false);
		}

		static T get(Reg r, size_t i)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				return r.bits.m128i_u8[i];
			else if constexpr (sizeof(T) == 2)
				return r.bits.m128i_u16[i];
			else if constexpr (sizeof(T) == 4)
				return r.bits.m128i_u32[i];
#else
			if constexpr (sizeof(T) == 1)
				return reinterpret_cast<const __v16qu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 2)
				return reinterpret_cast<const __v8hu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 4)
				return reinterpret_cast<const __v4su&>(r.bits)[i];
#endif
			else
				static_assert(false);
		}

		static Reg add(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm_add_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_add_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_add_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg sub(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm_sub_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_sub_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_sub_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedadd(MaskAVX512 mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm_mask_add_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_mask_add_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_mask_add_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedsub(MaskAVX512 mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm_mask_sub_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_mask_sub_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_mask_sub_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mul(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
			{
				// Mul by conversion to 16-bit.
				const Avx256IntegerReg a16 = ConversionPrimitive<Reg, 2>::template extend<T>(a)[0];
				const Avx256IntegerReg b16 = ConversionPrimitive<Reg, 2>::template extend<T>(b)[0];
				const Avx256IntegerReg p16{ _mm256_mullo_epi16(a16.bits, b16.bits) };
				return ConversionPrimitive<Reg, 2>::template truncate<uint16_t>({ p16 });
			}
			else if constexpr (sizeof(T) == 2)
				return { _mm_mullo_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_mullo_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mulhi(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>)
					return { _mm_mulhi_epi16(a.bits, b.bits) };
				else
					return { _mm_mulhi_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
			{
				constexpr auto widen = [](__m128i a) {
					if constexpr (std::is_signed_v<T>)
						return _mm256_cvtepi32_epi64(a);
					else
						return _mm256_cvtepu32_epi64(a);
					};

				// Choice of widening only matters for _mm256_mullo_epi64.
				const __m256i a64 = widen(a.bits);
				const __m256i b64 = widen(b.bits);

				if constexpr (kEnableAVX512)
				{
					const __m256i lo64sP = _mm256_mullo_epi64(a64, b64);
					const __m128i lo32sP = _mm256_cvtepi64_epi32(_mm256_srli_epi64(lo64sP, 32));
					return { lo32sP };
				}
				else
				{
					const __m256i p64 = std::is_signed_v<T> ? _mm256_mul_epi32(a64, b64) : _mm256_mul_epu32(a64, b64);
					const auto [lo, hi] = splitReg(Avx256IntegerReg(p64));
					return { castFromFP(_mm_shuffle_ps(castToFP(lo.bits), castToFP(hi.bits), _MM_SHUFFLE(3, 1, 3, 1))) };
				}
			}
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shl(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				return { _mm_slli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_slli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shr(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm_srai_epi16(a.bits, n) }; else return { _mm_srli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm_srai_epi32(a.bits, n) }; else return { _mm_srli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg maskedshr(MaskAVX512 mask, Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm_mask_srai_epi16(a.bits, mask.bits, a.bits, n) }; else return { _mm_mask_srli_epi16(a.bits, mask.bits, a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm_mask_srai_epi32(a.bits, mask.bits, a.bits, n) }; else return { _mm_mask_srli_epi32(a.bits, mask.bits, a.bits, n) };
			else
				static_assert(false);
		}

		static Reg shl(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return { _mm_sllv_epi16(a.bits, b.bits) };
				else
				{
					const __m256i a32 = ConversionPrimitive<Avx128IntegerReg, 2>::template extend<T>(a)[0].bits;
					const __m256i b32 = ConversionPrimitive<Avx128IntegerReg, 2>::template extend<T>(b)[0].bits;
					const __m256i result = _mm256_sllv_epi32(a32, b32);
					return ConversionPrimitive<Avx128IntegerReg, 2>::template truncate<int32_t>({ Avx256IntegerReg(result) });
				}
			}
			else if constexpr (sizeof(T) == 4)
				return { _mm_sllv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg shr(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					if constexpr (std::is_signed_v<T>) return { _mm_srav_epi16(a.bits, b.bits) }; else return { _mm_srlv_epi16(a.bits, b.bits) };
				else
				{
					const __m256i a32 = ConversionPrimitive<Avx128IntegerReg, 2>::template extend<T>(a)[0].bits;
					const __m256i b32 = ConversionPrimitive<Avx128IntegerReg, 2>::template extend<T>(b)[0].bits;
					const __m256i result = _mm256_srav_epi32(a32, b32);
					return ConversionPrimitive<Avx128IntegerReg, 2>::template truncate<int32_t>({ Avx256IntegerReg(result) });
				}
			}
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm_srav_epi32(a.bits, b.bits) }; else return { _mm_srlv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg div(Reg a, Reg b)
		{
			assert(vcmpeq(b, {}).none());

			if constexpr (sizeof(T) == 4)
			{
				const auto convertToPD = [](__m128i a) -> __m256d {
					if constexpr (std::is_signed_v<T>)
						return _mm256_cvtepi32_pd(a);
					else
						return polyfills::convertU32ToDouble(a);
				};
				const auto convertFromPD = [](__m256d a) -> __m128i {
					if constexpr (std::is_signed_v<T>)
						return _mm256_cvttpd_epi32(a);
					else
						return polyfills::convertU32RangeDoubleToU32(a);
				};

				return { convertFromPD(_mm256_div_pd(convertToPD(a.bits), convertToPD(b.bits))) };
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
				if constexpr (kEnableAVX512)
					return { _mm_permutexvar_epi8(indices.bits, a.bits) };
				else
					return { _mm_shuffle_epi8(a.bits, vbitand(indices, initBroadcast(kNumElements - 1)).bits) };
			else if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return { _mm_permutexvar_epi16(indices.bits, a.bits) };
				else
				{
					// Convert to byte permute.
					indices = vbitand(indices, initBroadcast(kNumElements - 1));
					indices = mul(indices, initBroadcast(0x0202));
					indices = add(indices, initBroadcast(0x0100));
					return { _mm_shuffle_epi8(a.bits, indices.bits) };
				}
			}
			else if constexpr (sizeof(T) == 4)
				return { castFromFP(_mm_permutevar_ps(castToFP(a.bits), indices.bits)) };
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
		//	//a.bits = _mm_andnot_si128(_mm_srai_epi32(shiftedIota.bits, 31), a.bits);
		//	//return a;
		//
		//	//return { _mm_alignr_epi8(a.bits, {}, (kNumElements - k) * sizeof(T)) };
		//
		//	return { _mm_bslli_si128(a.bits, k * sizeof(T)) };
		//}


		static Reg vmin(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm_min_epi8(a.bits, b.bits) }; else return { _mm_min_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm_min_epi16(a.bits, b.bits) }; else return { _mm_min_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm_min_epi32(a.bits, b.bits) }; else return { _mm_min_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vmax(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm_max_epi8(a.bits, b.bits) }; else return { _mm_max_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm_max_epi16(a.bits, b.bits) }; else return { _mm_max_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm_max_epi32(a.bits, b.bits) }; else return { _mm_max_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vabs(Reg a)
		{
			if constexpr (std::is_unsigned_v<T>)
				return a;
			else if constexpr (sizeof(T) == 1)
				return { _mm_abs_epi8(a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_abs_epi16(a.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm_abs_epi32(a.bits) };
			else
				static_assert(false);
		}

		static Reg vbitand(Reg a, Reg b)
		{
			return { _mm_and_si128(a.bits, b.bits) };
		}

		static Reg vbitor(Reg a, Reg b)
		{
			return { _mm_or_si128(a.bits, b.bits) };
		}

		static Reg vbitxor(Reg a, Reg b)
		{
			return { _mm_xor_si128(a.bits, b.bits) };
		}

		static Reg vbitnot(Reg a)
		{
			return vbitxor(a, initAllBits());
		}

		static T hmin(Reg a)
		{
			if constexpr (sizeof(T) == 1)
			{
				// These reduce min intrinsics basically do the below.
				/*if constexpr (kEnableAVX512)
					if constexpr (std::is_signed_v<T>) return _mm_reduce_min_epi8(a.bits); else return _mm_reduce_min_epu8(a.bits);
				else*/

				// Convert to offset format for signed.
				//if constexpr (std::is_signed_v<T>)
				//	a = vbitxor(a, initBroadcast(INT8_MIN));
				//const __m128i hi = _mm_srli_epi16(a.bits, 8);
				//const __m128i minsOf8 = _mm_min_epu8(a.bits, hi); // The high bytes in `lo` words will be ignored.
				//const __m128i minpos = _mm_minpos_epu16(mag);
				//uint8_t value = static_cast<uint8_t>(_mm_extract_epi8(minpos, 0));
				//if constexpr (std::is_signed_v<T>)
				//	value ^= INT8_MIN;
				//return value;

				static_assert(false);
			}
			else if constexpr (sizeof(T) == 2)
				static_assert(false);
			else if constexpr (sizeof(T) == 4)
			{
				if constexpr (std::is_signed_v<T>)
				{
					a.bits = _mm_min_epi32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(3, 2, 3, 2)));
					a.bits = _mm_min_epi32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(1, 1, 1, 1)));
				}
				else
				{
					a.bits = _mm_min_epu32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(2, 3, 2, 3)));
					a.bits = _mm_min_epu32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(3, 3, 1, 1)));
				}
				return static_cast<T>(_mm_cvtsi128_si32(a.bits));
			}
			else
				static_assert(false);
			
		}

		static T hmax(Reg a)
		{
			if constexpr (sizeof(T) == 4)
			{
				if constexpr (std::is_signed_v<T>)
				{
					a.bits = _mm_max_epi32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(3, 2, 3, 2)));
					a.bits = _mm_max_epi32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(1, 1, 1, 1)));
				}
				else
				{
					a.bits = _mm_max_epu32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(2, 3, 2, 3)));
					a.bits = _mm_max_epu32(a.bits, _mm_shuffle_epi32(a.bits, _MM_SHUFFLE(3, 3, 1, 1)));
				}
				return static_cast<T>(_mm_cvtsi128_si32(a.bits));
			}
			else
				static_assert(false);
		}

		//template<class = void>
		static Mask vcmpeq(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm_cmpeq_epi32_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm_cmpeq_epi32(a.bits, b.bits)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm_cmpeq_epi16_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm_cmpeq_epi16(a.bits, b.bits)));
			}
			else if constexpr (sizeof(T) == 1)
			{
				if constexpr (kEnableAVX512)
					return MaskAVX512(_mm_cmpeq_epi8_mask(a.bits, b.bits));
				else
					return MaskAVX2(Reg(_mm_cmpeq_epi8(a.bits, b.bits)));
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
					return MaskAVX512(_mm_test_epi32_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 2)
					return MaskAVX512(_mm_test_epi16_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 1)
					return MaskAVX512(_mm_test_epi8_mask(a.bits, b.bits));
				else
					static_assert(false);
			}
			else
				return MaskAVX2(vbitnot(vcmpeq(vbitand(a, b), {}).reg));
		}

		//template<class = void>
		static Mask vcmplt(Reg a, Reg b)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmpgt_epi32_mask(b.bits, a.bits)); else return MaskAVX512(_mm_cmpgt_epu32_mask(b.bits, a.bits));
				else if constexpr (sizeof(T) == 2)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmpgt_epi16_mask(b.bits, a.bits)); else return MaskAVX512(_mm_cmpgt_epu16_mask(b.bits, a.bits));
				else if constexpr (sizeof(T) == 1)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmpgt_epi8_mask(b.bits, a.bits)); else return MaskAVX512(_mm_cmpgt_epu8_mask(b.bits, a.bits));
				else
					static_assert(false);
			}
			else
			{
				if constexpr (std::is_signed_v<T>)
				{
					if constexpr (sizeof(T) == 4)
						return MaskAVX2(Reg(_mm_cmpgt_epi32(b.bits, a.bits)));
					else if constexpr (sizeof(T) == 2)
						return MaskAVX2(Reg(_mm_cmpgt_epi16(b.bits, a.bits)));
					else if constexpr (sizeof(T) == 1)
						return MaskAVX2(Reg(_mm_cmpgt_epi8(b.bits, a.bits)));
					else
						static_assert(false);
				}
				else
					return MaskAVX2(vbitnot(vcmpeq(vmax(a, b), a).reg)); // !(a >= b)
			}
		}

		//template<class = void>
		static Mask vcmple(Reg a, Reg b)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmple_epi32_mask(a.bits, b.bits)); else return MaskAVX512(_mm_cmple_epu32_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 2)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmple_epi16_mask(a.bits, b.bits)); else return MaskAVX512(_mm_cmple_epu16_mask(a.bits, b.bits));
				else if constexpr (sizeof(T) == 1)
					if constexpr (std::is_signed_v<T>) return MaskAVX512(_mm_cmple_epi8_mask(a.bits, b.bits)); else return MaskAVX512(_mm_cmple_epu8_mask(a.bits, b.bits));
				else
					static_assert(false);
			}
			else
				return vcmpeq(vmax(a, b), b);
		}

		static Reg vselect(Mask cond, Reg ifTrue, Reg ifFalse)
		{
			if constexpr (kEnableAVX512)
			{
				if constexpr (sizeof(T) == 4)
					return { _mm_mask_mov_epi32(ifFalse.bits, cond.bits, ifTrue.bits) };
				else if constexpr (sizeof(T) == 2)
					return { _mm_mask_mov_epi16(ifFalse.bits, cond.bits, ifTrue.bits) };
				else if constexpr (sizeof(T) == 1)
					return { _mm_mask_mov_epi8(ifFalse.bits, cond.bits, ifTrue.bits) };
				else
					static_assert(false);
			}
			else
				return { _mm_blendv_epi8(ifFalse.bits, ifTrue.bits, cond.reg.bits) };
		}

		static auto getHighBits(Reg r)
		{
			if constexpr (kEnableAVX512)
				return vtest(r, initBroadcast(kHighBit<T>)).bits;
			else if constexpr (sizeof(T) == 4)
				return static_cast<uint8_t>(_mm_movemask_ps(castToFP(r.bits)));
			else if constexpr (sizeof(T) == 2)
				return static_cast<uint8_t>(_pext_u32(_mm_movemask_epi8(r.bits), 0xAAAA));
			else if constexpr (sizeof(T) == 1)
				return static_cast<uint16_t>(_mm_movemask_epi8(r.bits));
			else
				static_assert(false);
		}

		static Reg hcompress(Reg a, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm_maskz_compress_epi32(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm_maskz_compress_epi16(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 1)
				return { _mm_maskz_compress_epi8(mask.bits, a.bits) };
			else
				static_assert(false);
		}

		static void compressStore(std::span<T> out, Reg a, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm_mask_compressstoreu_epi32(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 2)
				_mm_mask_compressstoreu_epi16(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 1)
				_mm_mask_compressstoreu_epi8(out.data(), mask.bits, a.bits);
			else
				static_assert(false);
		}

		static void scatter(std::span<T> array, Reg indices, Reg values, MaskAVX512 mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, sizeof(T));
			else
				static_assert(false);
		}

		template<size_t kScale>
		static void scatter(std::span<std::byte> array, Reg indices, Reg values, MaskAVX512 mask, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				_mm_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, kScale);
			else
				static_assert(false);
		}
	};
}