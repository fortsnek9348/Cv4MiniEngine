#pragma once

#include <ranges>

namespace heck::views
{
	template<class T>
	inline constexpr auto cast = std::views::transform([](auto&& from) -> T { return static_cast<T>(from); });

	inline constexpr auto addressof = std::views::transform([]<class T>(T& from) -> T* { return std::addressof(from); });

	inline constexpr auto deref = std::views::transform([](auto&& from) -> auto& { return *from; });
}