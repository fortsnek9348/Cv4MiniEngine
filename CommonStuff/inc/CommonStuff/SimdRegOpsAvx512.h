#pragma once

#include "SimdAvxRegs.h"
#include "SimdRegOpsAvx256.h"
#include "SimdConversionsAvx.h"
#include "SimdAvxBitMask.h"

#include <cassert>

namespace heck::simd
{
	template<IntegerType T>
	struct RegOps<Avx512IntegerReg, T>
	{
		using Reg = Avx512IntegerReg;
		static constexpr size_t kNumElements = sizeof(Reg) / sizeof(T);
		static constexpr size_t kHalfNumElements = kNumElements / 2;
		static_assert(sizeof(Reg) == sizeof(__m512i));

		static constexpr RegTraits kTraits{
			.supportsGather = true,
			.supportsScatter = true,
			.supportsMaskedOps = true,
			.supportsCompress = true,
			.isNative = true,
			.maxGatherScatterScale = 8,
		};

		using Mask = AvxBitMask<kNumElements>;


		static constexpr Reg initAllBits()
		{
			if (std::is_constant_evaluated())
				return initBroadcast(static_cast<T>(-1));
			else
			{
				const Reg r{ _mm512_undefined_epi32() };
				return { _mm512_ternarylogic_epi32(r.bits, r.bits, r.bits, -1) };
			}
		}

		static constexpr Reg initBroadcast(T value)
		{
			if (std::is_constant_evaluated())
				return initBuild(filledArray<T, kNumElements>(value));
			else if constexpr (sizeof(T) == 1)
				return { _mm512_set1_epi8(value) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_set1_epi16(value) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_set1_epi32(value) };
			else
				static_assert(false);
		}

		static constexpr Reg initBuild(std::array<T, kNumElements> values)
		{
#ifndef HECK_USING_GCC_SIMD
			if (std::is_constant_evaluated())
			{
				Reg reg{};
				std::ranges::copy(std::bit_cast<std::array<uint8_t, sizeof(values)>>(values), reg.bits.m512i_u8);
				return reg;
			}
			else
#endif
				return std::bit_cast<Reg>(values);
		}

		static Reg initLoadUnaligned(std::span<const T, kNumElements> values)
		{
			return { _mm512_loadu_si512(reinterpret_cast<const __m512i*>(values.data())) };
		}

		static Reg initGather(std::span<const T> array, Reg indices, Mask mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_mask_i32gather_epi32(_mm512_setzero_si512(), mask.bits, indices.bits, array.data(), sizeof(T)) };
			else
				static_assert(false);
		}

		static Reg initGather(std::span<const T> array, Reg indices)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_i32gather_epi32(indices.bits, reinterpret_cast<const int*>(array.data()), sizeof(T)) };
			else
				static_assert(false);
		}

		template<size_t kScale>
		static Reg initRawGather(std::span<const std::byte> array, Reg indices, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_i32gather_epi32(indices.bits, reinterpret_cast<const int*>(array.data()), kScale) };
			else
				static_assert(false);
		}

		static Reg broadcast(Reg r, size_t i)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_permutexvar_epi32(initBroadcast(i).bits, r.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_permutexvar_epi16(initBroadcast(i).bits, r.bits) };
			else if constexpr (sizeof(T) == 1)
				return { _mm512_permutexvar_epi8(initBroadcast(i).bits, r.bits) };
			else
				static_assert(false);
		}


		template<size_t i>
		static void set(Reg& r, T value)
		{
			r = vselect(Mask(static_cast<typename Mask::BitsUInt>(typename Mask::BitsUInt(1) << i)), initBroadcast(value), r);
		}

		template<size_t i>
		static T get(Reg r)
		{
			using HalfOps = RegOps<Avx256IntegerReg, T>;
			return HalfOps::template get<i % kHalfNumElements>(splitReg(r)[i / kHalfNumElements]);
		}

		static void set(Reg& r, size_t i, T value)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				r.bits.m512i_u8[i] = value;
			else if constexpr (sizeof(T) == 2)
				r.bits.m512i_u16[i] = value;
			else if constexpr (sizeof(T) == 4)
				r.bits.m512i_u32[i] = value;
#else
			if constexpr (sizeof(T) == 1)
				reinterpret_cast<__v64qu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 2)
				reinterpret_cast<__v32hu&>(r.bits)[i] = value;
			else if constexpr (sizeof(T) == 4)
				reinterpret_cast<__v16su&>(r.bits)[i] = value;
#endif
			else
				static_assert(false);
		}

		static T get(Reg r, size_t i)
		{
#ifndef HECK_USING_GCC_SIMD
			if constexpr (sizeof(T) == 1)
				return r.bits.m512i_u8[i];
			else if constexpr (sizeof(T) == 2)
				return r.bits.m512i_u16[i];
			else if constexpr (sizeof(T) == 4)
				return r.bits.m512i_u32[i];
#else
			if constexpr (sizeof(T) == 1)
				return reinterpret_cast<const __v64qu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 2)
				return reinterpret_cast<const __v32hu&>(r.bits)[i];
			else if constexpr (sizeof(T) == 4)
				return reinterpret_cast<const __v16su&>(r.bits)[i];
#endif
			else
				static_assert(false);
		}

		static Reg add(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm512_add_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_add_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_add_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg sub(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm512_sub_epi8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_sub_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_sub_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedadd(Mask mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm512_mask_add_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_mask_add_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_mask_add_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg maskedsub(Mask mask, Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				return { _mm512_mask_sub_epi8(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_mask_sub_epi16(a.bits, mask.bits, a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_mask_sub_epi32(a.bits, mask.bits, a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mul(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
			{
				// Mul by conversion to 16-bit.
				const auto [lo8a, hi8a] = splitReg(a);
				const auto [lo8b, hi8b] = splitReg(b);
				const Reg lo16a = ConversionPrimitive_AVX256x2_AVX512::template extend<T>(lo8a)[0];
				const Reg hi16a = ConversionPrimitive_AVX256x2_AVX512::template extend<T>(hi8a)[0];
				const Reg lo16b = ConversionPrimitive_AVX256x2_AVX512::template extend<T>(lo8b)[0];
				const Reg hi16b = ConversionPrimitive_AVX256x2_AVX512::template extend<T>(hi8b)[0];
				const Reg lo = RegOps<Reg, uint16_t>::mul(lo16a, lo16b);
				const Reg hi = RegOps<Reg, uint16_t>::mul(hi16a, hi16b);
				return ConversionPrimitive<Reg, 2>::template truncate<uint16_t>({ lo, hi });
			}
			else if constexpr (sizeof(T) == 2)
				return { _mm512_mullo_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_mullo_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg mulhi(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>)
					return { _mm512_mulhi_epi16(a.bits, b.bits) };
				else
					return { _mm512_mulhi_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
			{
				constexpr auto widen = [](__m256i a) {
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvtepi32_epi64(a);
					else
						return _mm512_cvtepu32_epi64(a);
					};
				
				const auto [lo32sA, hi32sA] = splitReg(a);
				const auto [lo32sB, hi32sB] = splitReg(b);
				const auto [lo64sA, hi64sA] = std::array{ widen(lo32sA.bits), widen(hi32sA.bits) };
				const auto [lo64sB, hi64sB] = std::array{ widen(lo32sB.bits), widen(hi32sB.bits) };
				const auto lo64sP = _mm512_mullo_epi64(lo64sA, lo64sB);
				const auto hi64sP = _mm512_mullo_epi64(hi64sA, hi64sB);
				const auto lo32sP = _mm512_cvtepi64_epi32(_mm512_srli_epi64(lo64sP, 32));
				const auto hi32sP = _mm512_cvtepi64_epi32(_mm512_srli_epi64(hi64sP, 32));
				return joinRegs({ Avx256IntegerReg{ lo32sP }, Avx256IntegerReg{ hi32sP } });
			}
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shl(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				return { _mm512_slli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_slli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg shr(Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm512_srai_epi16(a.bits, n) }; else return { _mm512_srli_epi16(a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm512_srai_epi32(a.bits, n) }; else return { _mm512_srli_epi32(a.bits, n) };
			else
				static_assert(false);
		}

		template<size_t n>
		static Reg maskedshr(Mask mask, Reg a, std::integral_constant<size_t, n>)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm512_mask_srai_epi16(a.bits, mask.bits, a.bits, n) }; else return { _mm512_mask_srli_epi16(a.bits, mask.bits, a.bits, n) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm512_mask_srai_epi32(a.bits, mask.bits, a.bits, n) }; else return { _mm512_mask_srli_epi32(a.bits, mask.bits, a.bits, n) };
			else
				static_assert(false);
		}

		static Reg shl(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
				return { _mm512_sllv_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_sllv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg shr(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm512_srav_epi16(a.bits, b.bits) }; else return { _mm512_srlv_epi16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm512_srav_epi32(a.bits, b.bits) }; else return { _mm512_srlv_epi32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg div(Reg a, Reg b)
		{
			assert(vcmpeq(b, {}).none());

			if constexpr (sizeof(T) == 4)
			{
				const auto convertToPS = [](__m512i a) {
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvtepi32_ps(a);
					else
						return _mm512_cvtepu32_ps(a);
					};
				const auto convertFromPS = [](__m512 a) {
					//return { _mm512_cvt_roundps_epi32(fpQ, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC) };
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvttps_epi32(a);
					else
						return _mm512_cvttps_epu32(a);
					};

				const auto convertToPD = [](__m256i a) {
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvtepi32_pd(a);
					else
						return _mm512_cvtepu32_pd(a);
					};
				const auto convertFromPD = [](__m512d a) {
					//const auto q1 = _mm512_cvt_roundpd_epi32(_mm512_div_pd(fpA1, fpB1), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
					if constexpr (std::is_signed_v<T>)
						return _mm512_cvttpd_epi32(a);
					else
						return _mm512_cvttpd_epu32(a);
					};

				// We don't have an integer divide (the ISA architects obviously never heard of Civ4 found values), but we have an FP divide...
				// TODO: Is checking faster for the common case?
				//if ((std::is_signed_v<T>
				//	? vcmplt(a, initBroadcast(T(1) << 24)) & vcmplt(initBroadcast(-(T(1) << 24)), a) // Clang optimises this to an add and one compare?
				//	: vcmplt(a, initBroadcast(T(1) << 24))
				//	).all()) [[likely]]
				if (RegOps<Reg, std::make_unsigned_t<T>>::vcmplt(vabs(a), initBroadcast(1 << 24)).all())
				{
					// 32-bit FP is enough.
					const auto fpA = convertToPS(a.bits);
					const auto fpB = convertToPS(b.bits);
					const auto fpQ = _mm512_div_ps(fpA, fpB); // Should round to nearest, and all possible integer results should be exactly representable.
					//return { _mm512_cvt_roundps_epi32(fpQ, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC) };
					return { convertFromPS(fpQ) };
				}
				else
				{
					// Need 64-bit FP. Super expensive! Huge latency and we need to double up.
					const auto [a0, a1] = splitReg(a);
					const auto [b0, b1] = splitReg(b);
					const auto fpA0 = convertToPD(a0.bits);
					const auto fpB0 = convertToPD(b0.bits);
					//const auto q0 = _mm512_cvt_roundpd_epi32(_mm512_div_pd(fpA0, fpB0), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
					const auto q0 = convertFromPD(_mm512_div_pd(fpA0, fpB0));
					const auto fpA1 = convertToPD(a1.bits);
					const auto fpB1 = convertToPD(b1.bits);
					//const auto q1 = _mm512_cvt_roundpd_epi32(_mm512_div_pd(fpA1, fpB1), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
					const auto q1 = convertFromPD(_mm512_div_pd(fpA1, fpB1));
					return joinRegs({ Avx256IntegerReg{ q0 }, Avx256IntegerReg{ q1 } });
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
				return { _mm512_permutexvar_epi8(indices.bits, a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_permutexvar_epi16(indices.bits, a.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_permutexvar_epi32(indices.bits, a.bits) };
			else
				static_assert(false);
		}

		//template<size_t k>
		//static Reg swizzleShiftLeft(Reg a, std::integral_constant<size_t, k>)
		//{
		//	static constexpr size_t kShiftRightAmountBytes = (kNumElements - k) * sizeof(T);
		//
		//	if constexpr (kShiftRightAmountBytes % 4 == 0)
		//		return { _mm512_alignr_epi32(a.bits, {}, kShiftRightAmountBytes / 4) };
		//	else
		//		// If shift by less than 128 bits, then split and rejoin.
		//		if constexpr (k < kNumElements / 2)
		//			return joinRegs({ Avx256IntegerReg(), RegOps<Avx256IntegerReg, T>::template swizzleShiftLeft<k - kNumElements / 2>(splitReg(a)[0], {}) });
		//		else if constexpr (k == kNumElements / 2)
		//			return joinRegs({ Avx256IntegerReg(), splitReg(a)[1] });
		//		else
		//		{
		//			// As k > kNumElements / 2, then kShiftRightAmount < kNumElements / 2 * sizeof(T). A 128-bit alignr is enough.
		//			return { joinRegs({ {}, Avx256IntegerReg{ _mm_alignr_epi8(splitReg(a)[1].bits, {}, kShiftRightAmountBytes) } } };
		//		}
		//}


		static Reg vmin(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm512_min_epi8(a.bits, b.bits) }; else return { _mm512_min_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm512_min_epi16(a.bits, b.bits) }; else return { _mm512_min_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm512_min_epi32(a.bits, b.bits) }; else return { _mm512_min_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vmax(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return { _mm512_max_epi8(a.bits, b.bits) }; else return { _mm512_max_epu8(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return { _mm512_max_epi16(a.bits, b.bits) }; else return { _mm512_max_epu16(a.bits, b.bits) };
			else if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return { _mm512_max_epi32(a.bits, b.bits) }; else return { _mm512_max_epu32(a.bits, b.bits) };
			else
				static_assert(false);
		}

		static Reg vabs(Reg a)
		{
			if constexpr (std::is_unsigned_v<T>)
				return a;
			else if constexpr (sizeof(T) == 1)
				return { _mm512_abs_epi8(a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_abs_epi16(a.bits) };
			else if constexpr (sizeof(T) == 4)
				return { _mm512_abs_epi32(a.bits) };
			else
				static_assert(false);
		}

		static Reg vbitand(Reg a, Reg b)
		{
			return { _mm512_and_si512(a.bits, b.bits) };
		}

		static Reg vbitor(Reg a, Reg b)
		{
			return { _mm512_or_si512(a.bits, b.bits) };
		}

		static Reg vbitxor(Reg a, Reg b)
		{
			return { _mm512_xor_si512(a.bits, b.bits) };
		}

		static Reg vbitnot(Reg a)
		{
			return vbitxor(a, initAllBits());
		}

		static T hmin(Reg a)
		{
			using HalfOps = RegOps<Avx256IntegerReg, T>;
			const auto [lo, hi] = splitReg(a);
			return HalfOps::hmin(HalfOps::vmin(lo, hi));

		}

		static T hmax(Reg a)
		{
			using HalfOps = RegOps<Avx256IntegerReg, T>;
			const auto [lo, hi] = splitReg(a);
			return HalfOps::hmax(HalfOps::vmax(lo, hi));
		}

		//template<class = void>
		static Mask vcmpeq(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
				return Mask(_mm512_cmpeq_epi32_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 2)
				return Mask(_mm512_cmpeq_epi16_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 1)
				return Mask(_mm512_cmpeq_epi8_mask(a.bits, b.bits));
			else
				static_assert(false);
		}

		//template<class = void>
		static Mask vtest(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
				return Mask(_mm512_test_epi32_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 2)
				return Mask(_mm512_test_epi16_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 1)
				return Mask(_mm512_test_epi8_mask(a.bits, b.bits));
			else
				static_assert(false);
		}

		//template<class = void>
		static Mask vcmplt(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmpgt_epi32_mask(b.bits, a.bits)); else return Mask(_mm512_cmpgt_epu32_mask(b.bits, a.bits));
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmpgt_epi16_mask(b.bits, a.bits)); else return Mask(_mm512_cmpgt_epu16_mask(b.bits, a.bits));
			else if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmpgt_epi8_mask(b.bits, a.bits)); else return Mask(_mm512_cmpgt_epu8_mask(b.bits, a.bits));
			else
				static_assert(false);
		}

		//template<class = void>
		static Mask vcmple(Reg a, Reg b)
		{
			if constexpr (sizeof(T) == 4)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmple_epi32_mask(a.bits, b.bits)); else return Mask(_mm512_cmple_epu32_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 2)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmple_epi16_mask(a.bits, b.bits)); else return Mask(_mm512_cmple_epu16_mask(a.bits, b.bits));
			else if constexpr (sizeof(T) == 1)
				if constexpr (std::is_signed_v<T>) return Mask(_mm512_cmple_epi8_mask(a.bits, b.bits)); else return Mask(_mm512_cmple_epu8_mask(a.bits, b.bits));
			else
				static_assert(false);
		}

		static Reg vselect(Mask cond, Reg ifTrue, Reg ifFalse)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_mask_mov_epi32(ifFalse.bits, cond.bits, ifTrue.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_mask_mov_epi16(ifFalse.bits, cond.bits, ifTrue.bits) };
			else if constexpr (sizeof(T) == 1)
				return { _mm512_mask_mov_epi8(ifFalse.bits, cond.bits, ifTrue.bits) };
			else
				static_assert(false);
		}

		static auto getHighBits(Reg r)
		{
			return vtest(r, initBroadcast(kHighBit<T>)).bits;
		}

		static Reg hcompress(Reg a, Mask mask)
		{
			if constexpr (sizeof(T) == 4)
				return { _mm512_maskz_compress_epi32(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 2)
				return { _mm512_maskz_compress_epi16(mask.bits, a.bits) };
			else if constexpr (sizeof(T) == 1)
				return { _mm512_maskz_compress_epi8(mask.bits, a.bits) };
			else
				static_assert(false);
		}

		static void compressStore(std::span<T> out, Reg a, Mask mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm512_mask_compressstoreu_epi32(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 2)
				_mm512_mask_compressstoreu_epi16(out.data(), mask.bits, a.bits);
			else if constexpr (sizeof(T) == 1)
				_mm512_mask_compressstoreu_epi8(out.data(), mask.bits, a.bits);
			else
				static_assert(false);
		}

		static void scatter(std::span<T> array, Reg indices, Reg values, Mask mask)
		{
			if constexpr (sizeof(T) == 4)
				_mm512_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, sizeof(T));
			else
				static_assert(false);
		}

		template<size_t kScale>
		static void scatter(std::span<std::byte> array, Reg indices, Reg values, Mask mask, std::integral_constant<size_t, kScale>)
		{
			if constexpr (sizeof(T) == 4)
				_mm512_mask_i32scatter_epi32(array.data(), mask.bits, indices.bits, values.bits, kScale);
			else
				static_assert(false);
		}
	};
}