#pragma once

#include "SimdCommon.h"
#include "SimdAvxRegs.h"
#include "SimdConversionsAvx.h"

#include <array>
#include <bit>
#include <span>

namespace heck::simd
{
	template<class ElementTo, class ElementFrom>
	inline constexpr bool kIsBitcastConversion = sizeof(ElementTo) == sizeof(ElementFrom) && IntegerType<ElementTo> && IntegerType<ElementFrom>;
	
	//template<class ElementTo, class ElementFrom>
	//inline constexpr bool kIsIntTruncateConversion = sizeof(ElementTo) < sizeof(ElementFrom) && AInt<ElementTo> && AInt<ElementFrom>;
	
	//template<class ElementTo, class ElementFrom>
	//inline constexpr bool kIsIntExtendConversion = sizeof(ElementTo) > sizeof(ElementFrom) && AInt<ElementTo> && AInt<ElementFrom>;

	// Big and complex function at compile-time.
	// Works by distinguishing between join/split/ext/trunc cases.
	template<class RegTo, class ElementTo, size_t kNumRegTo, class ElementFrom, class RegFrom, size_t kNumRegFrom>
	inline std::array<RegTo, kNumRegTo> genericConvertRegArray(const std::array<RegFrom, kNumRegFrom>& fromRegs)
	{
		static constexpr size_t kElementsPerRegFrom = sizeof(RegFrom) / sizeof(ElementFrom);
		static constexpr size_t kElementsPerRegTo = sizeof(RegTo) / sizeof(ElementTo);
		static constexpr size_t kNumElementsFrom = kNumRegFrom * kElementsPerRegFrom;
		static constexpr size_t kNumElementsTo = kNumRegTo * kElementsPerRegTo;
		static_assert(sizeof(RegFrom) % sizeof(ElementFrom) == 0);
		static_assert(sizeof(RegTo) % sizeof(ElementTo) == 0);
		static_assert(kNumElementsFrom == kNumElementsTo);
		static_assert(std::has_single_bit(kElementsPerRegFrom));
		static_assert(std::has_single_bit(kElementsPerRegTo));
		static_assert(sizeof(RegTo) % sizeof(RegFrom) == 0 || sizeof(RegFrom) % sizeof(RegTo) == 0); // Regs must be a multiple of the other regs' size.
		
		static_assert(sizeof(ElementTo) % sizeof(ElementFrom) == 0 || sizeof(ElementFrom) % sizeof(ElementTo) == 0); // Elements must be a multiple of the other elements' size.

		if constexpr (sizeof(ElementTo) == sizeof(ElementFrom))
		{
			// This is just a reg join/split conversion.
			static_assert(kIsBitcastConversion<ElementTo, ElementFrom>); // FP not implemented.

			if constexpr (kNumRegTo == kNumRegFrom)
				return fromRegs;
			else if constexpr (kNumRegTo > kNumRegFrom)
			{
				// Split conversion. One-to-many.
				static constexpr size_t kRegsPerFromReg = sizeof(RegFrom) / sizeof(RegTo);
				static_assert(decltype(multiSplitRegs<kRegsPerFromReg>(RegFrom())){}.size() == kRegsPerFromReg);
				std::array<RegTo, kNumRegTo> output{};
				for (size_t i = 0; i < kNumRegFrom; ++i)
					std::ranges::copy(multiSplitRegs<kRegsPerFromReg>(fromRegs[i]), output.begin() + i * kRegsPerFromReg);
				return output;
			}
			else // if constexpr (kNumRegTo < kNumRegFrom)
			{
				// Join conversion. Many-to-one.
				static constexpr size_t kRegsPerToReg = sizeof(RegTo) / sizeof(RegFrom);
				std::array<RegTo, kNumRegTo> output{};
				for (size_t i = 0; i < kNumRegTo; ++i)
					output[i] = multiJoinRegs(arraySubarray<kRegsPerToReg>(fromRegs, i * kRegsPerToReg));
				return output;
			}
		}
		else if constexpr (sizeof(ElementTo) > sizeof(ElementFrom))
		{
			// S/Z-extension.
			static constexpr size_t kFactor = sizeof(ElementTo) / sizeof(ElementFrom);

			using Primitive = ConversionPrimitive<RegFrom, kFactor>;
			using NaturalOutputReg = Primitive::ExtendedArrayRegType;
			static constexpr size_t kNaturalOutputNumRegsPerSourceReg = typename Primitive::ExtendedArray().size();
			static constexpr size_t kNaturalTotalOutputNumRegs = kNaturalOutputNumRegsPerSourceReg * kNumRegFrom;

			std::array<NaturalOutputReg, kNaturalTotalOutputNumRegs> output{};
			for (size_t i = 0; i < kNumRegFrom; ++i)
				std::ranges::copy(Primitive::template extend<ElementFrom>(fromRegs[i]), output.begin() + i * kNaturalOutputNumRegsPerSourceReg);

			// Split/join as necessary.
			if constexpr (!std::same_as<NaturalOutputReg, RegTo>)
				return genericConvertRegArray<RegTo, ElementTo, kNumRegTo, ElementTo>(output);
			else
				return output;
		}
		else //if constexpr (sizeof(ElementTo) < sizeof(ElementFrom))
		{
			// Truncation
			static constexpr size_t kFactor = sizeof(ElementFrom) / sizeof(ElementTo);

			// Truncating from source reg type to dest reg type.
			// Conversion primitive takes an array of a "natural reg type" for a given dest reg type.
			using Primitive = ConversionPrimitive<RegTo, kFactor>;
			using NaturalSourceReg = Primitive::ExtendedArrayRegType;
			static constexpr size_t kNaturalSourceNumRegsPerDestReg = typename Primitive::ExtendedArray().size();
			static constexpr size_t kNaturalSourceTotalNumRegs = kNaturalSourceNumRegsPerDestReg * kNumRegTo;

			if constexpr (!std::same_as<NaturalSourceReg, RegFrom>)
			{
				// Need to convert source array.
				const std::array<NaturalSourceReg, kNaturalSourceTotalNumRegs> naturalInput = genericConvertRegArray<NaturalSourceReg, ElementFrom, kNaturalSourceTotalNumRegs, ElementFrom>(fromRegs);
				return genericConvertRegArray<RegTo, ElementTo, kNumRegTo, ElementFrom>(naturalInput);
			}
			else
			{
				std::array<RegTo, kNumRegTo> output{};
				for (size_t i = 0; i < kNumRegTo; ++i)
					output[i] = Primitive::template truncate<ElementFrom>(arraySubarray<kNaturalSourceNumRegsPerDestReg>(fromRegs, i * kNaturalSourceNumRegsPerDestReg));
				return output;
			}
		}
	}
}