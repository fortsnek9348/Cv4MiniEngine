#pragma once

#include "CompilerMacros.h"
#include "IntegerTypeUtil.h"

#include <array>
#include <bit>
#include <type_traits>
#include <algorithm>
#include <span>

namespace heck::simd
{
	// Use for immediate values like shift constants.
	template<size_t k>
	inline constexpr std::integral_constant<size_t, k> imm{};

	template<class T>
	inline constexpr bool IsScalarType = false;

	template<> inline constexpr bool IsScalarType<int8_t> = true;
	template<> inline constexpr bool IsScalarType<int16_t> = true;
	template<> inline constexpr bool IsScalarType<int32_t> = true;
	template<> inline constexpr bool IsScalarType<int64_t> = true;
	template<> inline constexpr bool IsScalarType<uint8_t> = true;
	template<> inline constexpr bool IsScalarType<uint16_t> = true;
	template<> inline constexpr bool IsScalarType<uint32_t> = true;
	template<> inline constexpr bool IsScalarType<uint64_t> = true;

	
	//template<size_t kNumBits>// requires(kNumBits % 64 == 0)
	//struct UIntX
	//{
	//	//std::array<uint64_t, kNumBits / 64> parts{};
	//	std::bitset<kNumBits> bits
	//};


	template<class T>
	concept ScalarType = IsScalarType<std::remove_const_t<T>>;

	template<class T>
	concept IntegerType = ScalarType<T> && std::integral<T>;

	template<IntegerType T>
	inline constexpr T kHighBit = static_cast<T>(T(1) << (sizeof(T) * 8 - 1));



	template<class T, size_t k>
	constexpr std::array<T, k> filledArray(T value)
	{
		std::array<T, k> values{};
		values.fill(value);
		return values;
	}

	template<ScalarType U, ScalarType T, size_t k>
	constexpr std::array<U, k> convertArray(const std::array<T, k>& a)
	{
		std::array<U, k> b{};
		for (size_t i = 0; i < k; ++i)
			b[i] = U(a[i]);
		return b;
	}

	template<IntegerType T, size_t k>
	constexpr std::array<T, k> iotaArray()
	{
		std::array<T, k> b{};
		for (size_t i = 0; i < k; ++i)
			b[i] = T(i);
		return b;
	}

	template<IntegerType T, size_t k>
	constexpr std::array<T, k> iotaBitArray()
	{
		std::array<T, k> b{};
		for (size_t i = 0; i < k; ++i)
			b[i] = T(T(1) << i);
		return b;
	}

	template<class T, size_t k>
	constexpr std::array<T, k> HECK_VECTORCALL arrayShiftHigher(std::array<T, k> a, size_t n)
	{
		// Unlike std::shift_*, we preserve the "gap".
		std::ranges::copy_backward(std::span(a).subspan(0, k - n), a.end());
		return a;
	}

	template<class T, size_t k>
	constexpr std::array<T, k> HECK_VECTORCALL arrayShiftLower(std::array<T, k> a, size_t n)
	{
		// Unlike std::shift_*, we preserve the "gap".
		std::ranges::copy(std::span(a).subspan(n), a.begin());
		return a;
	}

	// HECK_VECTORCALL needed even for clang. Helps the optimiser stop spilling args to stack.
	template<size_t kSubArrayLength, class T, size_t k>
	constexpr std::array<T, kSubArrayLength> HECK_VECTORCALL arraySubarray(std::array<T, k> a, size_t baseI)
	{
		std::array<T, kSubArrayLength> out{};
		std::ranges::copy(std::span(a).subspan(baseI).template subspan<0, kSubArrayLength>(), out.begin());
		return out;
	}

	template<class T, size_t... k>
	constexpr std::array<T, (k + ...)> HECK_VECTORCALL arrayConcat(std::array<T, k>... a)
	{
		std::array<T, (k + ...)> out{};
		size_t outI = 0;
		((std::ranges::copy(a, out.begin() + outI), outI += k), ...);
		return out;
	}

	template<class T, size_t kBytes>
	concept AInt = IntegerType<T> && sizeof(T) == kBytes;

	template<class Reg, ScalarType T>
	struct RegOps;

	struct RegTraits
	{
		bool supportsGather = false;
		bool supportsScatter = false;
		bool supportsMaskedOps = false;
		bool supportsCompress = false;
		bool isNative = false;
		int maxGatherScatterScale = 0;
	};

	inline constexpr struct OutOfBoundsGather {} kOutOfBoundsGather{};
}