#pragma once

#include <CommonStuff\SimdCommon.h>

#include <array>
#include <bitset>
#include <algorithm>
#include <span>

template<class T, size_t k>
struct GenericVector
{
	// NOTE: SIMD does not have signed overflow UB, so for testing against SIMD, this implementation avoids signed overflow UB.
	using UnsignedT = std::make_unsigned_t<T>;

	using Array = std::array<T, k>;
	Array values{};
	using Mask = std::bitset<k>;

	GenericVector() = default;
	GenericVector(const GenericVector&) = default;
	GenericVector& operator=(const GenericVector&) = default;

	constexpr GenericVector(T value)
	{
		values.fill(value);
	}

	GenericVector(std::span<const T, k> a)
	{
		std::ranges::copy(a, values.begin());
	}

	constexpr GenericVector(const Array& a) : values(a)
	{
	}

	// Construct from shorter array.
	template<size_t n>
		requires(n <= k)
	constexpr GenericVector(const std::array<T, n>& a, T fill)
	{
		Array wider{};
		wider.fill(fill);
		std::ranges::copy(a, wider.begin());
		*this = wider;
	}

	// Generic conversion from same number of elements.
	template<class ElementFrom>
	explicit GenericVector(GenericVector<ElementFrom, k> b)
	{
		for (size_t i = 0; i < k; ++i)
			values[i] = static_cast<T>(b.values[i]);
	}

	constexpr GenericVector(GenericVector<T, k / 2> lo, GenericVector<T, k / 2> hi) requires (k >= 2 && k % 2 == 0)
	{
		std::ranges::copy(lo.values, values.begin());
		std::ranges::copy(hi.values, values.begin() + k / 2);
	}


	// Masked gather
	template<class Index>
	explicit GenericVector(std::span<const T> array, GenericVector<Index, k> indices, Mask mask)
	{
		for (size_t i = 0; i < k; ++i)
			if (mask[i])
				values[i] = array[indices.values[i]];
	}

	// Gather
	template<class Index>
	explicit GenericVector(std::span<const T> array, GenericVector<Index, k> indices)
	{
		for (size_t i = 0; i < k; ++i)
			values[i] = array[indices.values[i]];
	}

	// Gather elements from byte array with scale
	template<class Index, size_t kScale>
	explicit GenericVector(std::span<const std::byte> array, GenericVector<Index, k> indices, std::integral_constant<size_t, kScale>)
	{
		for (size_t i = 0; i < k; ++i)
			std::memcpy(&values[i], std::span(array).subspan(indices[i] * kScale, sizeof(T)).data(), sizeof(T));
	}

	// Gather elements from an array of a different type.
	template<heck::simd::ScalarType U, class Index>
	explicit GenericVector(heck::simd::OutOfBoundsGather, std::span<const U> array, GenericVector<Index, k> indices)
	{
		// Load raw bytes.
		*this = GenericVector(std::as_bytes(array), indices, heck::simd::imm<sizeof(U)>);
		// Then mask.
		if constexpr (sizeof(U) < sizeof(T))
			*this &= GenericVector(static_cast<std::make_unsigned_t<U>>(-1));
	}

	// Extend GenericVector with default elements.
	template<size_t kNumElementsFrom>
		requires (kNumElementsFrom < k)
	explicit GenericVector(GenericVector<T, kNumElementsFrom> b, T defValue)
	{
		values.fill(defValue);
		std::ranges::copy(b.values, values.begin());
	}

	// Truncate GenericVector
	template<size_t kNumElementsFrom>
		requires (kNumElementsFrom > k)
	explicit GenericVector(GenericVector<T, kNumElementsFrom> b)
	{
		std::ranges::copy_n(b.values.begin(), k, values.begin());
	}

	GenericVector broadcastElement(size_t i) const
	{
		return GenericVector(values[i]);
	}

	template<std::array<size_t, k> kIndices>
	GenericVector swizzle() const
	{
		GenericVector v{};
		for (size_t i = 0; i < k; ++i)
			v.values[i] = values[kIndices[i]];
		return v;
	}

	template<class Index>
	GenericVector permute(GenericVector<Index, k> indices) const
	{
		static_assert(std::has_single_bit(k));
		GenericVector v{};
		for (size_t i = 0; i < k; ++i)
			v.values[i] = values[indices.values[i] % k];
		return v;
	}

	// Double-width permute.
	template<class Index>
	GenericVector<T, k * 2> permute(GenericVector<Index, k * 2> indices) const
	{
		static_assert(std::has_single_bit(k));
		GenericVector v{};
		for (size_t i = 0; i < k * 2; ++i)
			v.values[i] = values[indices.values[i] % k];
		return v;
	}

	std::array<GenericVector<T, k / 2>, 2> split() const requires(k >= 2 && k % 2 == 0)
	{
		std::array<GenericVector<T, k / 2>, 2> r{};
		std::ranges::copy_n(values.begin(), k / 2, r[0].values.begin());
		std::ranges::copy_n(values.begin() + k / 2, k / 2, r[1].values.begin());
		return r;
	}

	template<size_t i>
	void setElement(T value)
	{
		values[i] = value;
	}

	template<size_t i>
	T getElement() const
	{
		return values[i];
	}

	void setElement(size_t i, T value)
	{
		values[i] = value;
	}

	T getElement(size_t i) const
	{
		return values[i];
	}

	std::array<T, k> toArray() const
	{
		return values;
	}

	friend GenericVector operator+(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] = T(UnsignedT(a.values[i]) + UnsignedT(b.values[i]));
		return a;
	}

	friend GenericVector operator-(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] = T(UnsignedT(a.values[i]) - UnsignedT(b.values[i]));
		return a;
	}

	friend GenericVector operator*(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] = T(UnsignedT(a.values[i]) * UnsignedT(b.values[i]));
		return a;
	}

	GenericVector& operator+=(GenericVector b)
	{
		return *this = *this + b;
	}

	GenericVector& operator-=(GenericVector b)
	{
		return *this = *this - b;
	}

	GenericVector& operator*=(GenericVector b)
	{
		return *this = *this * b;
	}

	friend GenericVector vmaskedadd(Mask mask, GenericVector a, GenericVector b)
	{
		return vselect(mask, a + b, a);
	}

	friend GenericVector vmaskedsub(Mask mask, GenericVector a, GenericVector b)
	{
		return vselect(mask, a - b, a);
	}

	

	friend GenericVector vmulhi(GenericVector a, GenericVector b)
	{
		static_assert(sizeof(T) * 2 <= sizeof(uint64_t));
		for (size_t i = 0; i < k; ++i)
			a.values[i] = static_cast<T>((uint64_t(a.values[i]) * uint64_t(b.values[i])) >> std::numeric_limits<UnsignedT>::digits);
		return a;
	}

	friend GenericVector vdiv(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] /= b.values[i];
		return a;
	}

	template<size_t kDiv>
	friend GenericVector operator/(GenericVector a, std::integral_constant<size_t, kDiv>)
	{
		// This is signed divided by unsigned.
		static_assert(sizeof(T) < sizeof(uint64_t));
		for (size_t i = 0; i < k; ++i)
			a.values[i] = static_cast<T>(static_cast<int64_t>(a.values[i]) / static_cast<int64_t>(kDiv));
		return a;
	}

	template<size_t n>
	friend GenericVector operator<<(GenericVector a, std::integral_constant<size_t, n>)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] <<= n;
		return a;
	}

	template<size_t n>
	friend GenericVector operator>>(GenericVector a, std::integral_constant<size_t, n>)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] >>= n;
		return a;
	}

	friend GenericVector operator<<(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] <<= b.values[i];
		return a;
	}

	friend GenericVector operator>>(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] >>= b.values[i];
		return a;
	}

	template<size_t n>
	GenericVector& operator<<=(std::integral_constant<size_t, n> b)
	{
		return *this = *this << b;
	}

	template<size_t n>
	GenericVector& operator>>=(std::integral_constant<size_t, n> b)
	{
		return *this = *this >> b;
	}

	template<size_t n>
	friend GenericVector vmaskedshr(Mask mask, GenericVector a, std::integral_constant<size_t, n>)
	{
		for (size_t i = 0; i < k; ++i)
			if (mask[i])
				a.values[i] >>= n;
		return a;
	}

	friend GenericVector operator&(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] &= b.values[i];
		return a;
	}

	friend GenericVector operator|(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] |= b.values[i];
		return a;
	}

	GenericVector& operator&=(GenericVector b)
	{
		return *this = *this & b;
	}

	GenericVector& operator|=(GenericVector b)
	{
		return *this = *this | b;
	}

	friend GenericVector vmin(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] = std::min(a.values[i], b.values[i]);
		return a;
	}

	friend GenericVector vmax(GenericVector a, GenericVector b)
	{
		for (size_t i = 0; i < k; ++i)
			a.values[i] = std::max(a.values[i], b.values[i]);
		return a;
	}

	friend GenericVector vabs(GenericVector a)
	{
		static_assert(sizeof(T) < sizeof(uint64_t));
		// abs(INT16_MIN) == unsigned(INT16_MIN).
		for (size_t i = 0; i < k; ++i)
			a.values[i] = T(std::abs(int64_t(a.values[i])));
		return a;
	}

	friend T hmin(GenericVector a)
	{
		return std::ranges::min(a.values);
	}

	friend T hmax(GenericVector a)
	{
		return std::ranges::max(a.values);
	}

	friend Mask vcmpeq(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = a.values[i] == b.values[i];
		return m;
	}

	friend Mask vcmplt(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = a.values[i] < b.values[i];
		return m;
	}

	friend Mask vcmple(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = a.values[i] <= b.values[i];
		return m;
	}

	friend Mask vcmpgt(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = a.values[i] > b.values[i];
		return m;
	}

	friend Mask vcmpge(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = a.values[i] >= b.values[i];
		return m;
	}

	friend Mask vtest(GenericVector a, GenericVector b)
	{
		Mask m{};
		for (size_t i = 0; i < k; ++i)
			m[i] = (a.values[i] & b.values[i]) != 0;
		return m;
	}

	friend GenericVector vselect(Mask mask, GenericVector ifTrue, GenericVector ifFalse)
	{
		for (size_t i = 0; i < k; ++i)
			ifTrue.values[i] = (mask[i] ? ifTrue : ifFalse).values[i];
		return ifTrue;
	}

	friend GenericVector hcompress(GenericVector a, Mask mask)
	{
		GenericVector v{};
		for (size_t i = 0, outI = 0; i < k; ++i)
			if (mask[i])
				v.values[outI++] = a.values[i];
		return v;
	}

	void compressStore(std::span<T> out, Mask mask)
	{
		for (size_t i = 0, outI = 0; i < k; ++i)
			if (mask[i])
				out[outI++] = values[i];
	}

	// Masked scatter: if (mask[i]) array[indices[i]] = values[i];
	template<class Index>
	friend void scatter(std::span<T> array, GenericVector<Index, k> indices, GenericVector values, Mask mask)
	{
		for (size_t i = 0; i < k; ++i)
			if (mask[i])
				array[indices.values[i]] = values.values[i];
	}

	// Masked scatter into raw byte array: if (mask[i]) store(array + indices[i] * kScale, values[i]);
	template<class Index, size_t kScale>
	friend void scatter(std::span<std::byte> array, GenericVector<Index, k> indices, GenericVector values, Mask mask, std::integral_constant<size_t, kScale>)
	{
		for (size_t i = 0; i < k; ++i)
			if (mask[i])
				std::memcpy(array.subspan(indices.values[i] * kScale, sizeof(T)).data(), &values.values[i], sizeof(T));
	}

};