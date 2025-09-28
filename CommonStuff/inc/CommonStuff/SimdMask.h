#pragma once

//#include "Bitset.h"
#include "SimdConversion.h"
#include "SimdAvxBitMask.h"
#include "SimdAvxRegMask.h"

#include <array>

namespace heck::simd
{
	template<class SingleMask, size_t kNumRegs>
	struct Mask;

	

	template<class SingleMask, size_t kNumRegs>
	constexpr UnsignedTypeAtLeastBits<SingleMask::kNumElements * kNumRegs> convertMaskArrayToUInt(std::array<SingleMask, kNumRegs> bitMasks)
	{
		constexpr size_t kElementsPerReg = SingleMask::kNumElements;
		using U = UnsignedTypeAtLeastBits<SingleMask::kNumElements * kNumRegs>;
		U x = bitMasks[0].asBitsUInt();
		for (size_t i = 1; i < kNumRegs; ++i)
			x |= static_cast<U>(bitMasks[i].asBitsUInt()) << (i * kElementsPerReg);
		return x;
	}

	template<class SingleMask, size_t kNumRegs>
	constexpr std::array<SingleMask, kNumRegs> convertUIntToSingleMaskArray(uintmax_t bits)
	{
		assert((size_t)std::bit_width(bits) <= SingleMask::kNumElements * kNumRegs);
		std::array<SingleMask, kNumRegs> regs{};
		for (size_t i = 0; i < kNumRegs; ++i)
			regs[i] = SingleMask(static_cast<SingleMask::BitsUInt>(bits >> (i * SingleMask::kNumElements)));
		return regs;
	}

	template<class SingleMaskTo>
	struct MaskConversion;

	template<class RegTo, size_t kBytesPerElementTo>
	struct MaskConversion<AvxRegMask<RegTo, kBytesPerElementTo>>
	{
		using SingleRegTo = AvxRegMask<RegTo, kBytesPerElementTo>;
		
		template<size_t kNumRegsTo, size_t kBytesPerElementFrom, class RegFrom, size_t kNumRegsFrom>
		static std::array<SingleRegTo, kNumRegsTo> convert(std::array<AvxRegMask<RegFrom, kBytesPerElementFrom>, kNumRegsFrom> from)
		{
			std::array<RegFrom, kNumRegsFrom> fromRegs{};
			for (size_t i = 0; i < kNumRegsFrom; ++i)
				fromRegs[i] = from[i].reg;

			// Signed: The masks need sign extension!
			using ElementTo = SignedTypeFromSize<kBytesPerElementTo>::type;
			using ElementFrom = SignedTypeFromSize<kBytesPerElementFrom>::type;
			const std::array<RegTo, kNumRegsTo> toRegs = genericConvertRegArray<RegTo, ElementTo, kNumRegsTo, ElementFrom>(fromRegs);

			std::array<SingleRegTo, kNumRegsTo> toMasks{};
			for (size_t i = 0; i < kNumRegsTo; ++i)
				toMasks[i] = SingleRegTo(toRegs[i]);

			return toMasks;
		}
	};

	template<size_t kNumElementsPerRegTo>
	struct MaskConversion<AvxBitMask<kNumElementsPerRegTo>>
	{
		using SingleRegTo = AvxBitMask<kNumElementsPerRegTo>;

		template<size_t kNumRegsTo, size_t kNumElementsPerRegFrom, size_t kNumRegsFrom>
		static std::array<SingleRegTo, kNumRegsTo> convert(std::array<AvxBitMask<kNumElementsPerRegFrom>, kNumRegsFrom> from)
		{
			static_assert(kNumElementsPerRegTo * kNumRegsTo == kNumElementsPerRegFrom * kNumRegsFrom);
			return convertUIntToSingleMaskArray<SingleRegTo, kNumRegsTo>(convertMaskArrayToUInt(from));
		}
	};

	template<class SingleMask, size_t kNumRegs>
	struct GenericMask
	{
		static constexpr size_t kNumElements = kNumRegs * SingleMask::kNumElements;
		static constexpr size_t kElementsPerReg = SingleMask::kNumElements;

		// For AVX512, would this be better as a single integer? Not sure. Maybe the compiler will use the mask regs more efficiently if the masks are kept separate.
		std::array<SingleMask, kNumRegs> regs{};

		using Bitset = std::bitset<kNumElements>;

		static const GenericMask kAll;

		GenericMask() = default;

		explicit constexpr GenericMask(uintmax_t bits) : regs(convertUIntToSingleMaskArray<SingleMask, kNumRegs>(bits))
		{
		}

		explicit constexpr GenericMask(Bitset bits) requires(kNumElements <= std::numeric_limits<unsigned long long>::digits) : GenericMask(bits.to_ullong())
		{
			//[bits, this] <size_t... kI>(std::index_sequence<kI...>) {
			//	((regs[kI] = SingleMask(bits.template extract<kI * kElementsPerReg, kElementsPerReg>().toUInt())), ...);
			//}(std::make_index_sequence<kNumRegs>());
		}

		template<class SingleMaskFrom, size_t kNumRegsFrom>
		explicit GenericMask(GenericMask<SingleMaskFrom, kNumRegsFrom> from) : regs(MaskConversion<SingleMask>::template convert<kNumRegs>(from.regs))
		{
			//[bits, this] <size_t... kI>(std::index_sequence<kI...>) {
			//	((regs[kI] = SingleMask(bits.template extract<kI * kElementsPerReg, kElementsPerReg>().toUInt())), ...);
			//}(std::make_index_sequence<kNumRegs>());
		}

		//explicit constexpr Mask(std::bitset<kNumElements> bits)
		//{
		//	Bitset bits2(bits.to_ullong());
		//	static constexpr size_t kChunkSize = std::numeric_limits<unsigned long long>::digits;
		//	for (size_t i = kChunkSize; i < kNumElements; i += kChunkSize)
		//	{
		//		bits >>= kChunkSize; // Linear time
		//		bits2.bitorAt(i, heck::Bitset<kChunkSize>(bits.to_ullong()));
		//	}
		//	*this = Mask(bits2);
		//}

		GenericMask operator&(GenericMask b) const
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				b.regs[i] &= regs[i];
			return b;
		}
		GenericMask operator|(GenericMask b) const
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				b.regs[i] |= regs[i];
			return b;
		}
		GenericMask& operator&=(GenericMask b) { return *this = *this & b; }
		GenericMask& operator|=(GenericMask b) { return *this = *this | b; }
		//AvxBitMask operator<<(size_t n) const { return UIntAsMask(Bits(bits << n)); }
		//AvxBitMask operator>>(size_t n) const { return UIntAsMask(Bits(bits >> n)); }
		GenericMask operator~() const
		{
			GenericMask b = *this;
			for (size_t i = 0; i < kNumRegs; ++i)
				b.regs[i] = ~regs[i];
			return b;
		}

		bool all() const
		{
			SingleMask acc = regs[0];
			for (size_t i = 1; i < kNumRegs; ++i)
				acc &= regs[i];
			return acc.all();
		}

		bool none() const
		{
			SingleMask acc = regs[0];
			for (size_t i = 1; i < kNumRegs; ++i)
				acc |= regs[i];
			return acc.none();
		}

		bool any() const
		{
			return !none();
		}

		// auto, so return type doesn't get evaluate if >64 bits.
		auto asBitsUInt() const
		{
			return convertMaskArrayToUInt(regs);
		}

		std::bitset<kNumElements> asBitset() const
		{
			//std::bitset<kNumElements> x(regs[0].asBitsUInt());
			//for (size_t i = 1; i < kNumRegs; ++i)
			//	//x.bitorAt(i * kElementsPerReg, regs[i].asBitset());
			//	x |= std::bitset<kNumElements>(regs[i].asBitsUInt()) << (i * kElementsPerReg);
			//return x;
			
			return asBitsUInt();
		}

		void forEachBit(auto f) const
		{
			for (size_t i = 0; i < kNumRegs; ++i)
			{
				auto bits = regs[i].asBitsUInt();
				while (bits)
				{
					const size_t j = std::countr_zero(bits);
					bits &= bits - 1;
					f(i * kElementsPerReg + j);
				}
			}
		}

		size_t countr_zero() const
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				if (regs[i].any())
					return std::countr_zero(regs[i].asBitsUInt());
			return kNumElements;
		}

		friend bool operator==(GenericMask, GenericMask) = default;
	};

	template<class SingleMask, size_t kNumRegs>
	inline const constinit GenericMask<SingleMask, kNumRegs> GenericMask<SingleMask, kNumRegs>::kAll{ ~GenericMask<SingleMask, kNumRegs>::Bitset() };
}