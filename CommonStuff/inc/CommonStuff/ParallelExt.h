#pragma once

#include <atomic>
#include <execution>
#include <ranges>

namespace heck
{
	// https://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
	// No C++26 fetch_max yet.
	template<class T>
	inline void atomicMaxRelaxed(std::atomic<T>& maximum_value, T value)
	{
		T prev_value = maximum_value.load(std::memory_order_relaxed);
		while (prev_value < value && !maximum_value.compare_exchange_weak(prev_value, value, std::memory_order_relaxed))
			;
	}

	inline constexpr auto kDefaultParallelExecution = std::execution::par;

	template<class I, class F>
	inline void parallelForEachN(I n, F&& f)
	{
		std::for_each_n(kDefaultParallelExecution, std::views::iota(I(0)).begin(), n, std::forward<F>(f));
		//std::for_each_n(std::views::iota(I(0)).begin(), n, std::forward<F>(f));
	}

	template<class R, class F>
	inline void parallelForEach(R&& r, F&& f)
	{
		std::for_each(kDefaultParallelExecution, r.begin(), r.end(), std::forward<F>(f));
	}

	template<class I, class F>
	inline void serialForEachN(I n, F&& f)
	{
		std::for_each_n(std::views::iota(I(0)).begin(), n, std::forward<F>(f));
	}
}