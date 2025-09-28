#pragma once

#include <cassert>
#include <algorithm>
#include <limits>
#include <concepts>
#include <type_traits>
#include <stdint.h>

namespace heck
{
	template<size_t kBytes> struct UnsignedTypeFromSize;
	template<> struct UnsignedTypeFromSize<1> : std::type_identity<uint8_t> {};
	template<> struct UnsignedTypeFromSize<2> : std::type_identity<uint16_t> {};
	template<> struct UnsignedTypeFromSize<4> : std::type_identity<uint32_t> {};
	template<> struct UnsignedTypeFromSize<8> : std::type_identity<uint64_t> {};

	template<size_t kBytes> struct SignedTypeFromSize;
	template<> struct SignedTypeFromSize<1> : std::type_identity<int8_t> {};
	template<> struct SignedTypeFromSize<2> : std::type_identity<int16_t> {};
	template<> struct SignedTypeFromSize<4> : std::type_identity<int32_t> {};
	template<> struct SignedTypeFromSize<8> : std::type_identity<int64_t> {};

	template<size_t kBits>
	struct UnsignedTypeAtLeastBitsThingyToKeepIntellisenseHappy
	{
		auto operator()() const
		{
			if constexpr (kBits <= 8)
				return uint8_t();
			else if constexpr (kBits <= 16)
				return uint16_t();
			else if constexpr (kBits <= 32)
				return uint32_t();
			else if constexpr (kBits <= 64)
				return uint64_t();
			else
				static_assert(false);
		}
	};

	template<size_t kBits>
	// Doesn't work with Intellisense.
	//using UnsignedTypeAtLeastBits = UnsignedTypeFromSize<std::bit_ceil(kBits) / 8>::type;
	// Doesn't work with Intellisense.
	//using UnsignedTypeAtLeastBits = decltype([]<class = void>() {
	//	if constexpr (kBits <= 8)
	//		return uint8_t();
	//	else if constexpr (kBits <= 16)
	//		return uint16_t();
	//	else if constexpr (kBits <= 32)
	//		return uint32_t();
	//	else if constexpr (kBits <= 64)
	//		return uint64_t();
	//	else
	//		static_assert(false);
	//}());
	using UnsignedTypeAtLeastBits = decltype(UnsignedTypeAtLeastBitsThingyToKeepIntellisenseHappy<kBits>()());

	template<size_t kBits>
	using UnsignedTypeAtLeastBitsOrMax = decltype([]<class = void>() {
		if constexpr (kBits <= 8)
			return uint8_t();
		else if constexpr (kBits <= 16)
			return uint16_t();
		else if constexpr (kBits <= 32)
			return uint32_t();
		else if constexpr (kBits <= 64)
			return uint64_t();
		else
			return uintmax_t();
	}());

	template<std::integral T, size_t k>
		requires (k <= std::numeric_limits<std::make_unsigned_t<T>>::digits)
	inline constexpr T kLowNBitsSet = k < std::numeric_limits<std::make_unsigned_t<T>>::digits
		? T((std::make_unsigned_t<T>(1) << std::min<size_t>(k, std::numeric_limits<std::make_unsigned_t<T>>::digits - 1)) - 1) // For some reason, both branches get evaluated...
		: static_cast<T>(-1);

	template<std::integral T>
	constexpr T makeLowNBitsSet(size_t n)
	{
		using U = std::make_unsigned_t<T>;
		assert(n <= std::numeric_limits<U>::digits);
		//if (!std::is_constant_evaluated())
		//{
		//	if constexpr (std::numeric_limits<U>::digits <= 32)
		//		return _bzhi_u32(-1, n);
		//	else if constexpr (std::numeric_limits<U>::digits <= 64)
		//		return _bzhi_u64(-1, n);
		//}
		return T(n < std::numeric_limits<U>::digits ? (U(1) << n) - 1 : U(-1));
	}

	template<size_t kMax>
	constexpr UnsignedTypeAtLeastBits<kMax> makeLowNBitsSet(size_t n)
	{
		return makeLowNBitsSet<UnsignedTypeAtLeastBits<kMax>>(n);
	}
}