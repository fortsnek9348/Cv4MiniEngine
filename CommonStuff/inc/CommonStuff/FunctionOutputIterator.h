#pragma once

#include <utility>

namespace heck
{
	template<class F>
	class FunctionRefOutputIterator
	{
	public:
		using difference_type = std::ptrdiff_t;

		FunctionRefOutputIterator(const F& f) : f(f)
		{
		}

		auto operator*()
		{
			return Proxy{ *this };
		}

		FunctionRefOutputIterator& operator++()
		{
			return *this;
		}

		FunctionRefOutputIterator operator++(int)
		{
			return *this;
		}

	private:
		std::reference_wrapper<const F> f;

		struct Proxy
		{
			FunctionRefOutputIterator& it;

			template<class T>
			void operator=(T&& x) const
			{
				it.f(std::forward<T>(x));
			}
		};
	};
}