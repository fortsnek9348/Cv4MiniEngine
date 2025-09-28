#pragma once

#include <ranges>
#include <algorithm>

namespace hecktui
{
	template<class T>
	constexpr std::ranges::iota_view<T, T> range(T end)
	{
		return std::views::iota(T(), end);
	}

	template<class T>
	constexpr std::ranges::iota_view<T, T> range(T begin, T end)
	{
		return std::views::iota(begin, end);
	}

	template<std::ranges::input_range R, class T = std::remove_cvref_t<std::ranges::range_value_t<R>>, class Proj = std::identity>
	constexpr auto sum(R&& r, T init = {}, Proj proj = {})
	{
		return std::ranges::fold_left(std::forward<R>(r) | std::views::transform(proj), init, std::plus<>());
	}

	struct Rational
	{
		int numer{};
		int denom{ 1 };

		int mul(int x, int& remainder) const
		{
			if (denom <= 0)
			{
				remainder = 0;
				return 0;
			}

			std::lldiv_t result = std::lldiv(std::int64_t(x) * numer + remainder, denom);
			// Round to nearest.
			if (result.rem >= (denom + 1) / 2)
			{
				++result.quot;
				result.rem -= denom;
			}
			else if (result.rem <= -((denom + 1) / 2))
			{
				--result.quot;
				result.rem += denom;
			}
			remainder = (int)result.rem;
			return (int)result.quot;
		}

		// x = lerp(numer / denom, min, max)
		// x = min + (max - min) * (numer / denom)
		int lerp(int min, int max, int& remainder) const
		{
			return min + mul(max - min, remainder);
		}
	};
}