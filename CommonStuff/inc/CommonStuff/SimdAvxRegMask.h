#pragma once

#include "SimdAvxRegs.h"

#include <bitset>

namespace heck::simd
{
	template<class Reg, size_t kBytesPerElement_>
		requires requires { { Reg().bits }; }
	struct AvxRegMask
	{
		static constexpr size_t kBytesPerElement = kBytesPerElement_;
		static constexpr size_t kNumElements = sizeof(Reg) / kBytesPerElement;
		static_assert(sizeof(Reg) % kBytesPerElement == 0);
		

		Reg reg{};

		using BitsUInt = UnsignedTypeAtLeastBits<kNumElements>;
		using Bitset = std::bitset<kNumElements>;

		using ElementUInt = UnsignedTypeFromSize<kBytesPerElement>::type;
		using Ops = RegOps<Reg, ElementUInt>;

		AvxRegMask() = default;

		explicit constexpr AvxRegMask(BitsUInt bits)
		{
			if (std::is_constant_evaluated())
			{
				std::array<ElementUInt, kNumElements> x{};
				for (size_t i = 0; i < kNumElements; ++i)
					x[i] = (bits >> i) & 1 ? static_cast<ElementUInt>(-1) : 0;
				reg = Ops::initBuild(x);
			}
			else
				reg = Ops::initMaskRegFromBits(bits);
		}

		explicit constexpr AvxRegMask(Bitset bits) requires(kNumElements <= std::numeric_limits<unsigned long long>::digits) : AvxRegMask(bits.to_ullong())
		{
		}

		explicit constexpr AvxRegMask(Reg reg) : reg(reg)
		{
		}

		AvxRegMask operator&(AvxRegMask b) const
		{
			return AvxRegMask{ Ops::vbitand(b.reg, reg) };
		}
		AvxRegMask operator|(AvxRegMask b) const
		{
			return AvxRegMask{ Ops::vbitor(b.reg, reg) };
		}
		AvxRegMask& operator&=(AvxRegMask b) { return *this = *this & b; }
		AvxRegMask& operator|=(AvxRegMask b) { return *this = *this | b; }
		//AvxBitMask operator<<(size_t n) const { return UIntAsMask(Bits(bits << n)); }
		//AvxBitMask operator>>(size_t n) const { return UIntAsMask(Bits(bits >> n)); }
		AvxRegMask operator~() const
		{
			return AvxRegMask{ Ops::vbitnot(reg) };
		}

		bool all() const
		{
			return std::bitset<kNumElements>(Ops::getHighBits(reg)).all();
		}

		bool none() const
		{
			return std::bitset<kNumElements>(Ops::getHighBits(reg)).none();
		}

		bool any() const
		{
			return !none();
		}

		BitsUInt asBitsUInt() const
		{
			return Ops::getHighBits(reg);
		}

		Bitset asBitset() const
		{
			return Bitset(asBitsUInt());
		}

		friend bool operator==(AvxRegMask, AvxRegMask) = default;
	};
}