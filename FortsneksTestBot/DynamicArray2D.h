#pragma once

#include <PlayerBotGameBinding/Common.h>

#include <vector>

namespace mybot
{
	// Ah yes, the great solution to this baloney.
	struct BoolReference : std::vector<bool>::reference
	{
		using std::vector<bool>::reference::operator=;

		BoolReference& operator|=(bool b)
		{
			*this = *this || b;
			return *this;
		}

		BoolReference& operator&=(bool b)
		{
			*this = *this && b;
			return *this;
		}
	};

	template<class T>
	struct DynamicArray2D
	{
		std::vector<T> cells;
		ivec2 dim{};

		DynamicArray2D() = default;

		explicit DynamicArray2D(ivec2 dim) : cells(dim.x* dim.y), dim(dim)
		{
		}

		explicit DynamicArray2D(ivec2 dim, T def) : cells(dim.x* dim.y, def), dim(dim)
		{
		}

		Span2D<const T> view() const noexcept
		{
			return { cells, dim };
		}

		Span2D<T> view() noexcept
		{
			return { cells, dim };
		}

		decltype(auto) operator[](ivec2 p)
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			if constexpr (std::is_same_v<bool, T>)
				return BoolReference(cells[p.x + p.y * dim.x]);
			else
				return cells[p.x + p.y * dim.x];
		}

		decltype(auto) operator[](ivec2 p) const
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return cells[p.x + p.y * dim.x];
		}
	};
}