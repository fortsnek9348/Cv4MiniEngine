#pragma once

#include "vec.h"

#include <array>
#include <atomic>
#include <vector>

namespace heck
{
	template<class T, size_t kN>
	struct Array2D : std::array<T, kN * kN>
	{
		using value_type = T;

		Array2D() = default;

		explicit Array2D(T value)
		{
			this->fill(value);
		}

		const T& operator[](ivec2 coord) const
		{
			return std::array<T, kN* kN>::operator[](coord.x + coord.y * kN);
		}

		T& operator[](ivec2 coord)
		{
			return std::array<T, kN* kN>::operator[](coord.x + coord.y * kN);
		}
	};

	template<class T>
	struct DynamicArray2D
	{
		using value_type = T;

		ivec2 dim{};
		std::vector<T> cells;

		DynamicArray2D() = default;
		// Additional alloc used for out-of-bounds SIMD gather.
		explicit DynamicArray2D(ivec2 dim, T def, size_t additionalAlloc = 0) : dim(dim), cells(dim.x * dim.y + additionalAlloc, def)
		{
		}

		explicit DynamicArray2D(ivec2 dim) : dim(dim), cells(dim.x * dim.y)
		{
		}

		const T& operator[](ivec2 coord) const
		{
			assert(unsigned(coord.x) < unsigned(dim.x) && unsigned(coord.y) < unsigned(dim.y));
			return cells[coord.x + coord.y * dim.x];
		}

		T& operator[](ivec2 coord)
		{
			assert(unsigned(coord.x) < unsigned(dim.x) && unsigned(coord.y) < unsigned(dim.y));
			return cells[coord.x + coord.y * dim.x];
		}

		std::atomic_ref<T> atomicRef(ivec2 coord)
		{
			assert(unsigned(coord.x) < unsigned(dim.x) && unsigned(coord.y) < unsigned(dim.y));
			return std::atomic_ref<T>(cells[coord.x + coord.y * dim.x]);
		}

		friend bool operator==(const DynamicArray2D&, const DynamicArray2D&) = default;
	};

}