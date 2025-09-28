#pragma once

#include "SimdAvxRegs.h"
#include "SimdConversion.h"
#include "SimdConversionsAvx.h"
#include "SimdMask.h"
#include "CompilerMacros.h"

#include <cassert>
#include <ranges>
#include <cstring>

namespace heck::simd
{
	// These won't use AVX512 instructions if AVX512 is disabled, as they are only used in AVX512 Ops.
	inline constexpr bool kEnableGather = true;
	inline constexpr bool kEnableScatter = true;

	template<size_t kBytes>
	using SimdRegisterSel =
		std::conditional_t<kBytes <= 128 / 8, Avx128IntegerReg,
		std::conditional_t<kBytes <= 256 / 8 || !kEnableAVX512, Avx256IntegerReg,
		Avx512IntegerReg>>;

	template<class IndexType, class MyElementType>
	concept CompatibleIndexType = sizeof(IndexType) == sizeof(MyElementType) && std::integral<IndexType>;

	template<class T, size_t k, class Reg_ = SimdRegisterSel<sizeof(T) * k>>
	struct Vector
	{
		static constexpr size_t kNumElements = k;
		static constexpr size_t kNumElementsPerReg = sizeof(Reg_) / sizeof(T);
		static constexpr size_t kNumRegs = k / kNumElementsPerReg;
		static_assert(kNumElementsPerReg * kNumRegs == k);

		using Reg = Reg_;
		using value_type = T;
		using Ops = RegOps<Reg, T>;
		using SingleRegMask = typename Ops::Mask;

		using Array = std::array<T, k>;
		using DefaultIndexType = SignedTypeFromSize<sizeof(T)>::type;
		using DefaultIndexVector = Vector<DefaultIndexType, k, Reg>;

		using Mask = GenericMask<SingleRegMask, kNumRegs>;
		
		std::array<Reg, kNumRegs> regs{};

		Vector() = default;
		Vector(const Vector&) = default;
		Vector& operator=(const Vector&) = default;

		constexpr Vector(T value)
		{
			regs.fill(Ops::initBroadcast(value));
		}

		Vector(std::span<const T, k> a)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				regs[i] = Ops::initLoadUnaligned(a.subspan(i * kNumElementsPerReg).template subspan<0, kNumElementsPerReg>());
		}

		constexpr Vector(const Array& a)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				regs[i] = Ops::initBuild(arraySubarray<kNumElementsPerReg>(a, i * kNumElementsPerReg));
		}

		// Construct from shorter array.
		template<size_t n>
			requires(n <= k)
		constexpr Vector(const std::array<T, n>& a, T fill)
		{
			alignas(Reg) Array wider{};
			wider.fill(fill);
			std::ranges::copy(a, wider.begin());
			*this = wider;
		}

		// Generic conversion from same number of elements.
		template<class ElementFrom, class RegFrom>
		explicit Vector(Vector<ElementFrom, k, RegFrom> b)
			requires (!std::same_as<Vector, Vector<ElementFrom, k, RegFrom>>)
			: regs(genericConvertRegArray<Reg, T, kNumRegs, ElementFrom>(b.regs))
		{

		}

		constexpr Vector(Reg reg) requires(kNumRegs == 1) : regs{ reg }
		{
		}

		constexpr Vector(std::array<Reg, kNumRegs> regs) : regs(regs)
		{
		}

		template<class OtherReg>
			requires (k >= 2 && k % 2 == 0)
		constexpr Vector(Vector<T, k / 2, OtherReg> lo, Vector<T, k / 2, OtherReg> hi)
		{
			static_assert(Vector<T, k, OtherReg>::kNumRegs % 2 == 0);
			Vector<T, k, OtherReg> v{};
			std::ranges::copy(lo.regs, v.regs.begin());
			std::ranges::copy(hi.regs, v.regs.begin() + Vector<T, k, OtherReg>::kNumRegs / 2);
			*this = Vector(v);
		}


		// Masked gather
		template<CompatibleIndexType<T> Index>
		explicit Vector(std::span<const T> array, Vector<Index, k, Reg> indices, Mask mask)
		{
			assert((vcmple(0, indices) & vcmplt(indices, static_cast<Index>(array.size())) & mask) == mask);

			if constexpr (kEnableGather)
			{
				for (size_t i = 0; i < kNumRegs; ++i)
					regs[i] = Ops::initGather(array, indices.regs[i], mask.regs[i]);
			}
			else
			{
				std::array<T, k> values{};
				const auto indicesArray = indices.toArray();
				const std::bitset<k> bits = mask.asBitset();
				for (size_t i = 0; i < k; ++i)
					if (bits[i])
						values[i] = array[indicesArray[i]];
				*this = values;
			}
		}

		// Gather
		template<CompatibleIndexType<T> Index>
		explicit Vector(std::span<const T> array, Vector<Index, k, Reg> indices)
		{
			assert(static_cast<size_t>(hmax(indices)) < array.size());

			if constexpr (kEnableGather)
			{
				for (size_t i = 0; i < kNumRegs; ++i)
					regs[i] = Ops::initGather(array, indices.regs[i]);
			}
			else
			{
				std::array<T, k> values{};
				const auto indicesArray = indices.toArray();
				for (size_t i = 0; i < k; ++i)
					values[i] = array[indicesArray[i]];
				*this = values;
			}
		}

		// Gather elements from byte array with scale
		template<CompatibleIndexType<T> Index, size_t kScale>
		explicit Vector(std::span<const std::byte> array, Vector<Index, k, Reg> indices, std::integral_constant<size_t, kScale>)
		{
			using IndexType = Index;

			assert(vcmple(0, indices).all());
			assert(static_cast<ptrdiff_t>(static_cast<ptrdiff_t>(hmax(indices)) * static_cast<ptrdiff_t>(kScale) + sizeof(T) - 1) < static_cast<ptrdiff_t>(array.size()));

			if constexpr (kEnableGather)
			{
				static constexpr bool kUsingNativeScaling = std::has_single_bit(kScale) && kScale <= Ops::kTraits.maxGatherScatterScale;
				if constexpr (!kUsingNativeScaling)
					indices = indices * static_cast<IndexType>(kScale);
				static constexpr std::integral_constant<size_t, kUsingNativeScaling ? kScale : 1> kFinalScale{};

				for (size_t i = 0; i < kNumRegs; ++i)
					regs[i] = Ops::initRawGather(array, indices.regs[i], kFinalScale);
			}
			else
			{
				std::array<T, k> values{};
				const auto indicesArray = indices.toArray();
				for (size_t i = 0; i < k; ++i)
					std::memcpy(&values[i], std::span(array).subspan(indicesArray[i] * kScale, sizeof(T)).data(), sizeof(T));
				*this = values;
			}
		}

		// Gather elements from an array of a different type.
		template<ScalarType U, CompatibleIndexType<T> Index>
		explicit Vector(OutOfBoundsGather, std::span<const U> array, Vector<Index, k, Reg> indices)
		{
			// Load raw bytes.
			*this = Vector(std::as_bytes(array), indices, imm<sizeof(U)>);
			// Then mask.
			if constexpr (sizeof(U) < sizeof(T))
				*this &= Vector(static_cast<std::make_unsigned_t<U>>(-1));
		}

		// Extend vector with default elements.
		template<size_t kNumElementsFrom>
			requires (kNumElementsFrom < k)
		explicit Vector(Vector<T, kNumElementsFrom, Reg> b, T defValue)
		{
			std::ranges::copy(b.regs, regs.begin());
			// Fill the rest.
			std::fill(regs.begin() + b.regs.size(), regs.end(), Ops::initBroadcast(defValue));
		}

		// Extend vector with default elements.
		template<class RegFrom, size_t kNumElementsFrom>
			requires (kNumElementsFrom < k)
		explicit Vector(Vector<T, kNumElementsFrom, RegFrom> b, T defValue)
		{
			const Vector<T, k, RegFrom> extendedWithFromRegs(b, defValue);
			*this = Vector(extendedWithFromRegs);
		}

		// Truncate vector
		template<class RegFrom, size_t kNumElementsFrom>
			requires (kNumElementsFrom > k)
		explicit Vector(Vector<T, kNumElementsFrom, RegFrom> b)
		{
			// Convert to same reg type.
			const Vector<T, kNumElementsFrom, Reg> sameRegType(b);
			// Assume regs are fully used.
			static_assert(kNumElementsFrom == Vector<T, kNumElementsFrom, Reg>::kNumElementsPerReg * Vector<T, kNumElementsFrom, Reg>::kNumRegs);
			static_assert(k == kNumElementsPerReg * kNumRegs);
			std::copy_n(sameRegType.regs.begin(), sameRegType.regs.begin() + kNumRegs, regs.begin());
		}

		// Join vectors.
		//template<class Reg2>
		//HECK_FORCEINLINE Vector(Vector<T, k / 2, Reg2> lo, Vector<T, k / 2, Reg2> hi) : reg(Ops::join(lo.reg, hi.reg))
		//{
		//}

		Vector HECK_VECTORCALL broadcastElement(size_t i) const
		{
			const Reg b = Ops::broadcast(regs[i / kNumElementsPerReg], i % kNumElementsPerReg);
			Vector v;
			std::ranges::fill(v.regs, b);
			return v;
		}

		template<std::array<size_t, k> kIndices>
		Vector HECK_VECTORCALL swizzle() const
		{
			// Not a pretty function. A swizzle could combine multiple regs for each output reg. So we need to loop over the input regs, swizzle, and mask.

			//static constexpr std::array<size_t, k> kAllRegIndices = [] {
			//	std::array<size_t, k> regIndices = kIndices;
			//	for (size_t i = 0; i < k; ++i)
			//		regIndices[i] /= kNumElementsPerReg;
			//	return regIndices;
			//	}();
			//
			//static constexpr std::array<size_t, k> kAllRegElementIndices = [] {
			//	std::array<size_t, k> regIndices = kIndices;
			//	for (size_t i = 0; i < k; ++i)
			//		regIndices[i] %= kNumElementsPerReg;
			//	return regIndices;
			//	}();
			//
			//struct CompiledSwizzleStep
			//{
			//	size_t inputRegIndex{};
			//	RegMask mask;
			//
			//	CompiledSwizzleStep() = default;
			//	explicit constexpr CompiledSwizzleStep(size_t inputRegIndex, std::array<size_t, kNumElementsPerReg> regIndices)
			//		: inputRegIndex(inputRegIndex)
			//	{
			//		std::bitset<kNumElementsPerReg> bitset{};
			//		for (size_t i = 0; i < kNumElementsPerReg; ++i)
			//			bitset[i] = regIndices[i] == inputRegIndex;
			//		mask = RegMask(bitset);
			//	}
			//};
			//
			//static constexpr auto kSwizzleStepsList = []<size_t kOutputRegI>(std::integral_constant<size_t, kOutputRegI>) {
			//	std::array<size_t, kNumElementsPerReg> thisRegIndices = arraySubarray<kNumElementsPerReg>(kAllRegIndices, kOutputRegI * kNumElementsPerReg);
			//	std::ranges::sort(thisRegIndices);
			//	const size_t n = std::ranges::unique(thisRegIndices).begin() - thisRegIndices.begin();
			//
			//	std::array<CompiledSwizzleStep, kNumElementsPerReg> steps{};
			//	for (size_t i = 0; i < n; ++i)
			//		steps[i] = CompiledSwizzleStep(thisRegIndices[i], thisRegIndices);
			//
			//	return std::pair(steps, n);
			//};
			//const auto calcOutputReg = [this]<size_t kOutputRegI>(std::integral_constant<size_t, kOutputRegI>) {
			//	static constexpr auto [kThisRegSteps, kNumInputRegs] = kSwizzleStepsList(std::integral_constant<size_t, kOutputRegI>());
			//	static constexpr auto kThisRegElementIndices = arraySubarray<kNumElementsPerReg>(kAllRegElementIndices, kOutputRegI * kNumElementsPerReg);
			//	Reg reg = Ops::template swizzle<kThisRegIndices>(regs[kThisRegSteps[0].inputRegIndex]);
			//	for (size_t i = 1; i < numInputRegs; ++i)
			//		reg = Ops::vselect(kThisRegSteps[i].mask, Ops::template swizzle<kThisRegIndices>(regs[kThisRegSteps[i].inputRegIndex]), reg);
			//	return reg;
			//});
			//
			//Vector v;
			//[] (size_t... i>(std::index_sequence<i...>) {
			//	((v.regs[i] = calcOutputReg(imm<i>)), ...);
			//}(std::make_index_sequence<kNumRegs>());
			//return v;

			// A good optimiser would optimise all this away.
			Vector v;
			for (size_t i = 0; i < kNumRegs; ++i)
			{
				const std::array<DefaultIndexType, kNumElementsPerReg> thisIndices = convertArray<DefaultIndexType>(arraySubarray<kNumElementsPerReg>(kIndices, i * kNumElementsPerReg));
				const auto thisIndicesReg = Vector<DefaultIndexType, kNumElementsPerReg, Reg>(thisIndices).regs[0];
				v.regs[i] = Ops::permute(regs[i], thisIndicesReg);
				for (size_t j = 0; j < kNumRegs; ++j)
				{
					if (j != i)
					{
						// Constant.
						std::bitset<kNumElementsPerReg> selBits;
						for (size_t x = 0; x < kNumElementsPerReg; ++x)
							selBits[x] = thisIndices[x] / kNumElementsPerReg == j;

						if (selBits.any())
							v.regs[i] = Ops::vselect(typename Ops::Mask(selBits), Ops::permute(regs[j], thisIndicesReg), v.regs[i]);
					}
				}
			}
			return v;
		}

		template<CompatibleIndexType<T> Index>
		Vector HECK_VECTORCALL permute(Vector<Index, k, Reg> indices) const
		{
			static_assert(std::has_single_bit(kNumElementsPerReg));
			// Mask instead of shift. Shifts not implemented for bytes.
			const Vector<Index, k, Reg> regBits = indices & (static_cast<Index>(~static_cast<Index>(kNumElementsPerReg - 1)) & static_cast<Index>(k - 1));
			Vector v;
			for (size_t i = 0; i < kNumRegs; ++i)
			{
				v.regs[i] = Ops::permute(regs[i], indices.regs[i]);
				for (size_t j = 0; j < kNumRegs; ++j)
					if (j != i)
						v.regs[i] = Ops::vselect(Ops::vcmpeq(regBits.regs[i], Ops::initBroadcast(Index(j * kNumElementsPerReg))), Ops::permute(regs[j], indices.regs[i]), v.regs[i]);
			}
			return v;
		}

		// Double-width permute.
		template<CompatibleIndexType<T> Index, class IReg>
		Vector<T, k * 2, IReg> HECK_VECTORCALL permute(Vector<Index, k * 2, IReg> indices) const
		{
			if constexpr (Vector<Index, k * 2, IReg>::kNumRegs == 1)
			{
				// Double-width is native. Extend our vector and permute that instead.
				return Vector<T, k * 2, IReg>(*this, T()).permute(indices);
			}
			else
			{
				// Double-width is being done through joined regs. Permute twice, presuming double-width is not native.
				const auto [lo, hi] = indices.split();
				const Vector<T, k, IReg> v(*this);
				return Vector<T, k * 2, IReg>(v.permute(lo), v.permute(hi));
			}
		}

		auto HECK_VECTORCALL split() const
		{
			if constexpr (kNumRegs >= 2)
			{
				if constexpr (kNumRegs % 2 == 0)
				{
					return std::array{
						Vector<T, k / 2, Reg>(arraySubarray<kNumRegs / 2>(regs, 0)),
						Vector<T, k / 2, Reg>(arraySubarray<kNumRegs / 2>(regs, kNumRegs / 2)),
					};
				}
				else // Odd number of regs.
					static_assert(false); 
			}
			else // Split single reg.
			{
				const auto [lo, hi] = splitReg(regs[0]);
				using SplitReg = std::remove_cvref_t<decltype(lo)>;
				return std::array{
					Vector<T, k / 2, SplitReg>(lo),
					Vector<T, k / 2, SplitReg>(hi),
				};
			}
		}

		template<size_t i>
		void setElement(T value)
		{
			Ops::template set<i % kNumElementsPerReg>(regs[i / kNumElementsPerReg], value);
		}

		template<size_t i>
		T getElement() const
		{
			return Ops::template get<i % kNumElementsPerReg>(regs[i / kNumElementsPerReg]);
		}

		void setElement(size_t i, T value)
		{
			Ops::set(regs[i / kNumElementsPerReg], i % kNumElementsPerReg, value);
		}

		T getElement(size_t i) const
		{
			return Ops::get(regs[i / kNumElementsPerReg], i % kNumElementsPerReg);
		}

		std::array<T, k> toArray() const
		{
			return std::bit_cast<std::array<T, k>>(regs);
		}

	private:
		static Vector HECK_VECTORCALL binop(auto f, auto... args)
		{
			Vector r;
			for (size_t i = 0; i < kNumRegs; ++i)
				r.regs[i] = f(args.regs[i]...);
			return r;
		}

		static Mask HECK_VECTORCALL binopMask(auto f, auto... args)
		{
			Mask r;
			for (size_t i = 0; i < kNumRegs; ++i)
				r.regs[i] = f(args.regs[i]...);
			return r;
		}

	public:
		friend Vector HECK_VECTORCALL operator+(Vector a, Vector b)
		{
			return binop(&Ops::add, a, b);
		}

		friend Vector HECK_VECTORCALL operator-(Vector a, Vector b)
		{
			return binop(&Ops::sub, a, b);
		}

		Vector& HECK_VECTORCALL operator+=(Vector b)
		{
			return *this = *this + b;
		}

		Vector& HECK_VECTORCALL operator-=(Vector b)
		{
			return *this = *this - b;
		}

		Vector& HECK_VECTORCALL operator*=(Vector b)
		{
			return *this = *this * b;
		}

		friend Vector HECK_VECTORCALL vmaskedadd(Mask mask, Vector a, Vector b)
		{
			if constexpr (Ops::kTraits.supportsMaskedOps)
				return binop(&Ops::maskedadd, mask, a, b);
			else
				return vselect(mask, a + b, a);
		}

		friend Vector HECK_VECTORCALL vmaskedsub(Mask mask, Vector a, Vector b)
		{
			if constexpr (Ops::kTraits.supportsMaskedOps)
				return binop(&Ops::maskedsub, mask, a, b);
			else
				return vselect(mask, a - b, a);
		}

		friend Vector HECK_VECTORCALL operator*(Vector a, Vector b)
		{
			return binop(&Ops::mul, a, b);
		}

		friend Vector HECK_VECTORCALL vmulhi(Vector a, Vector b)
		{
			return binop(&Ops::mulhi, a, b);
		}

		// Making this named as integer implementation goes through FP!
		friend Vector HECK_VECTORCALL vdiv(Vector a, Vector b)
		{
			return binop(&Ops::div, a, b);
		}

		// Division by power-of-2 only.
		// This is signed divided by unsigned.
		// Arbitrary integer division by constant could be implemented.
		template<size_t kDiv>
		friend Vector HECK_VECTORCALL operator/(Vector a, std::integral_constant<size_t, kDiv>)
		{
			static_assert(std::has_single_bit(kDiv));

			constexpr size_t kShift = std::countr_zero(kDiv);
			static_assert(kShift < std::numeric_limits<std::make_unsigned_t<T>>::digits);
			if constexpr (kShift > 0)
			{
				if constexpr (std::is_unsigned_v<T>)
					return a >> imm<kShift>;
				else if constexpr (kShift <= std::numeric_limits<T>::digits)
					return vmaskedadd(vcmplt(a, 0), a, static_cast<T>((std::make_unsigned_t<T>(1) << kShift) - 1)) >> imm<kShift>;
				else
					return {};
			}
			else
				return a;
		}

		template<size_t n>
		friend Vector HECK_VECTORCALL operator<<(Vector a, std::integral_constant<size_t, n> b)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				a.regs[i] = Ops::shl(a.regs[i], b);
			return a;
		}

		template<size_t n>
		friend Vector HECK_VECTORCALL operator>>(Vector a, std::integral_constant<size_t, n> b)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				a.regs[i] = Ops::shr(a.regs[i], b);
			return a;
		}

		friend Vector HECK_VECTORCALL operator<<(Vector a, Vector b)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				a.regs[i] = Ops::shl(a.regs[i], b.regs[i]);
			return a;
		}

		friend Vector HECK_VECTORCALL operator>>(Vector a, Vector b)
		{
			for (size_t i = 0; i < kNumRegs; ++i)
				a.regs[i] = Ops::shr(a.regs[i], b.regs[i]);
			return a;
		}

		template<size_t n>
		Vector& operator<<=(std::integral_constant<size_t, n> b)
		{
			return *this = *this << b;
		}

		template<size_t n>
		Vector& operator>>=(std::integral_constant<size_t, n> b)
		{
			return *this = *this >> b;
		}

		template<size_t n>
		friend Vector HECK_VECTORCALL vmaskedshr(Mask mask, Vector a, std::integral_constant<size_t, n> b)
		{
			if constexpr (Ops::kTraits.supportsMaskedOps)
			{
				for (size_t i = 0; i < kNumRegs; ++i)
					a.regs[i] = Ops::maskedshr(mask.regs[i], a.regs[i], b);
				return a;
			}
			else
				return vselect(mask, a >> b, a);
		}

		friend Vector HECK_VECTORCALL operator&(Vector a, Vector b)
		{
			return binop(&Ops::vbitand, a, b);
		}

		friend Vector HECK_VECTORCALL operator|(Vector a, Vector b)
		{
			return binop(&Ops::vbitor, a, b);
		}

		Vector& HECK_VECTORCALL operator&=(Vector b)
		{
			return *this = *this & b;
		}

		Vector& HECK_VECTORCALL operator|=(Vector b)
		{
			return *this = *this | b;
		}

		friend Vector HECK_VECTORCALL vmin(Vector a, Vector b)
		{
			return binop(&Ops::vmin, a, b);
		}

		friend Vector HECK_VECTORCALL vmax(Vector a, Vector b)
		{
			return binop(&Ops::vmax, a, b);
		}

		friend Vector HECK_VECTORCALL vabs(Vector a)
		{
			return binop(&Ops::vabs, a);
		}

		friend T HECK_VECTORCALL hmin(Vector a)
		{
			Reg reg = a.regs[0];
			for (size_t i = 1; i < kNumRegs; ++i)
				reg = Ops::vmin(reg, a.regs[i]);
			return Ops::hmin(reg);
		}

		friend T HECK_VECTORCALL hmax(Vector a)
		{
			Reg reg = a.regs[0];
			for (size_t i = 1; i < kNumRegs; ++i)
				reg = Ops::vmax(reg, a.regs[i]);
			return Ops::hmax(reg);
		}

		friend Mask HECK_VECTORCALL vcmpeq(Vector a, Vector b)
		{
			return binopMask(&Ops::vcmpeq, a, b);
		}

		friend Mask HECK_VECTORCALL vcmplt(Vector a, Vector b)
		{
			return binopMask(&Ops::vcmplt, a, b);
		}

		friend Mask HECK_VECTORCALL vcmple(Vector a, Vector b)
		{
			return binopMask(&Ops::vcmple, a, b);
		}

		friend Mask HECK_VECTORCALL vcmpgt(Vector a, Vector b)
		{
			return binopMask(&Ops::vcmplt, b, a);
		}

		friend Mask HECK_VECTORCALL vcmpge(Vector a, Vector b)
		{
			return binopMask(&Ops::vcmple, b, a);
		}

		friend Mask HECK_VECTORCALL vtest(Vector a, Vector b)
		{
			return binopMask(&Ops::vtest, a, b);
		}

		friend Vector HECK_VECTORCALL vselect(Mask mask, Vector ifTrue, Vector ifFalse)
		{
			return binop(&Ops::vselect, mask, ifTrue, ifFalse);
		}

		friend Vector HECK_VECTORCALL hcompress(Vector a, Mask mask)
		{
			if constexpr (Ops::kTraits.supportsCompress && kNumRegs == 1)
			{
				// Fast path for single-reg.
				a.regs[0] = Ops::hcompress(a.regs[0], mask.regs[0]);
				return a;
			}
			else
			{
				// Scalar path, probably won't ever be used.
				Array array{};
				size_t outI = 0;
				for (size_t i = 0; i < kNumRegs; ++i)
				{
					auto bits = mask.asBits();
					while (bits)
					{
						const int j = std::countr_zero(bits);
						bits &= bits - 1;
						array[outI++] = a.setElement(i * kNumElementsPerReg + j);
					}
				}
				return array;
			}
		}

		void HECK_VECTORCALL compressStore(std::span<T> out, Mask mask) const
		{
			assert(mask.asBitset().count() <= out.size());
			if constexpr (Ops::kTraits.supportsCompress && Ops::kTraits.supportsScatter && kNumRegs == 1)
			{
				// Minimum debug overhead for single reg.
				return Ops::compressStore(out, regs[0], mask.regs[0]);
			}
			else if constexpr (Ops::kTraits.supportsCompress)
			{
				for (size_t i = 0; i < kNumRegs; ++i)
				{
					Ops::compressStore(out, regs[i], mask.regs[i]);
					out = out.subspan(mask.regs[i].asBitset().count());
				}
			}
			else
			{
				// Scalar path, probably won't ever be used.
				size_t outI = 0;
				auto bits = mask.asBitsUInt();
				while (bits)
				{
					const int i = std::countr_zero(bits);
					bits &= bits - 1;
					out[outI++] = getElement(i);
				}
			}
		}
		
		// Masked scatter: if (mask[i]) array[indices[i]] = values[i];
		template<CompatibleIndexType<T> Index>
		friend void HECK_VECTORCALL scatter(std::span<T> array, Vector<Index, k, Reg> indices, Vector values, Mask mask)
		{
			if constexpr (kEnableScatter && Ops::kTraits.supportsScatter)
			{
				assert((vcmple(0, indices) & vcmplt(indices, static_cast<Index>(array.size())) & mask) == mask);
				for (size_t i = 0; i < kNumRegs; ++i)
					Ops::scatter(array, indices.regs[i], values.regs[i], mask.regs[i]);
			}
			else
			{
				const auto indicesArray = indices.toArray();
				const auto valuesArray = values.toArray();
				const auto bits = mask.asBitset();
				for (size_t i = 0; i < k; ++i)
					if (bits[i])
						array[indicesArray[i]] = valuesArray[i];
			}
		}

		// Masked scatter into raw byte array: if (mask[i]) store(array + indices[i] * kScale, values[i]);
		template<CompatibleIndexType<T> Index, size_t kScale>
		friend void scatter(std::span<std::byte> array, Vector<Index, k, Reg> indices, Vector values, Mask mask, std::integral_constant<size_t, kScale> scale)
		{
			if constexpr (kEnableScatter && Ops::kTraits.supportsScatter)
			{
				assert(((vcmplt(indices, 0) | vcmplt(static_cast<Index>(array.size()), indices * kScale + sizeof(T))) & mask).none());

				if constexpr (std::has_single_bit(kScale) && kScale <= Ops::kTraits.maxGatherScatterScale)
				{
					for (size_t i = 0; i < kNumRegs; ++i)
						Ops::scatter(array, indices.regs[i], values.regs[i], mask.regs[i], scale);
				}
				else
				{
					indices *= kScale;
					for (size_t i = 0; i < kNumRegs; ++i)
						Ops::scatter(array, indices.regs[i], values.regs[i], mask.regs[i], std::integral_constant<size_t, 1>());
				}
			}
			else
			{
				const std::array<Index, k> indicesArray = indices.toArray();
				const std::array<T, k> valuesArray = values.toArray();
				const auto bits = mask.asBitset();
				for (size_t i = 0; i < k; ++i)
					if (bits[i])
						std::memcpy(array.subspan(indicesArray[i] * kScale, sizeof(T)).data(), &valuesArray[i], sizeof(T));
			}
		}

		template<CompatibleIndexType<T> Index, size_t kScale>
		void scatterInto(std::span<std::byte> array, Vector<Index, k, Reg> indices, Mask mask, std::integral_constant<size_t, kScale> scale) const
		{
			return scatter(array, indices, *this, mask, scale);
		}
	};

	//std::array<Vector<int32_t, 8>, 8> transposed(std::span<const Vector<int32_t, 8>, 8> matrix);
	//std::array<Vector<uint32_t, 8>, 8> transposed(std::span<const Vector<uint32_t, 8>, 8> matrix);

	//std::array<Vector<int32_t, 16>, 16> transposed(std::span<const Vector<int32_t, 16>, 16> matrix);
	//std::array<Vector<uint32_t, 16>, 16> transposed(std::span<const Vector<uint32_t, 16>, 16> matrix);
}