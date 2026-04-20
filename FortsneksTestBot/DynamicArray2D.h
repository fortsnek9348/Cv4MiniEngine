#pragma once

#include <PlayerBotGameBinding/Common.h>

#include <vector>

namespace mybot
{
	using cvbot::ivec2;
	using cvbot::Span2D;

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

		T& operator[](ivec2 p)
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return cells[p.x + p.y * dim.x];
		}

		const T& operator[](ivec2 p) const
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return cells[p.x + p.y * dim.x];
		}
	};
}