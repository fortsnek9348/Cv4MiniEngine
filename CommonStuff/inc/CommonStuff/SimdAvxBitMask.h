#pragma once

//#include "Bitset.h"
#include "SimdAvxRegs.h"

#include <bitset>

namespace heck::simd
{
	template<size_t kNumElements_>
	struct AvxBitMask
	{
		static constexpr size_t kNumElements = kNumElements_;
		using BitsUInt = UnsignedTypeAtLeastBits<kNumElements>;
		using Bitset = std::bitset<kNumElements>;

		static constexpr BitsUInt kAllBits = kLowNBitsSet<BitsUInt, kNumElements>;

		BitsUInt bits{};

		AvxBitMask() = default;

		explicit constexpr AvxBitMask(BitsUInt bits) : bits(bits & kAllBits)
		{
		}

		explicit constexpr AvxBitMask(Bitset bits) : AvxBitMask(bits.asInteger())
		{
		}

		AvxBitMask operator&(AvxBitMask b) const
		{
			return AvxBitMask(bits & b.bits);
		}
		AvxBitMask operator|(AvxBitMask b) const
		{
			return AvxBitMask(bits | b.bits);
		}
		AvxBitMask& operator&=(AvxBitMask b) { return *this = *this & b; }
		AvxBitMask& operator|=(AvxBitMask b) { return *this = *this | b; }
		//AvxBitMask operator<<(size_t n) const { return UIntAsMask(Bits(bits << n)); }
		//AvxBitMask operator>>(size_t n) const { return UIntAsMask(Bits(bits >> n)); }
		AvxBitMask operator~() const
		{
			return AvxBitMask(~bits & kAllBits);
		}

		bool all() const
		{
			return bits == kAllBits;
		}

		bool none() const
		{
			return bits == 0;
		}

		bool any() const
		{
			return !none();
		}

		BitsUInt asBitsUInt() const
		{
			return bits;
		}

		Bitset asBitset() const
		{
			return Bitset(asBitsUInt());
		}

		friend bool operator==(AvxBitMask, AvxBitMask) = default;
	};
}