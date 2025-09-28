#pragma once

#include <ranges>
#include <type_traits>
#include <utility>

namespace heck
{
	template<class Cast = void, class T = void>
	constexpr auto range(T start, T end)
	{
		if (start > end)
			end = start;

		if constexpr (std::is_void_v<Cast> || (std::is_same_v<Cast, T> && !std::is_enum_v<T>))
			return std::views::iota(start, end);
		else if constexpr (!std::is_enum_v<T>)
			return std::views::iota(start, end) | std::views::transform([](T value) { return static_cast<Cast>(value); });
		else
			return std::views::iota(std::to_underlying(start), std::to_underlying(end)) | std::views::transform([](std::underlying_type_t<T> value) { return static_cast<Cast>(value); });
	}

	template<class Cast = void, class I = void>
	constexpr auto range(I end)
	{
		if constexpr (std::is_void_v<Cast>)
			return range<I>(I(), end);
		else
			return range<Cast>(I(), end);
	}
}