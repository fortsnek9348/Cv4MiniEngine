#pragma once

#include <unordered_map>

namespace heck
{
	template<class Container>
	class AssocUnionFind
	{
	public:
		using T = Container::mapped_type;

		// <https://en.wikipedia.org/wiki/Disjoint-set_data_structure>

		[[nodiscard]] T find(T x) const
		{
			auto it = mMap.find(x);
			if (it == mMap.end())
				return x;
			for (;;)
			{
				if (it->second == x)
					break;
				x = it->second;
				it = mMap.find(x);
			}
			return x;
		}

		void unionise(T a, T b)
		{
			a = findWithPathHalving(a);
			b = findWithPathHalving(b);

			if (a != b)
				mMap[b] = a;
		}

	private:
		Container mMap;

		[[nodiscard]] T findWithPathHalving(T x)
		{
			auto parentIt = mMap.emplace(x, x).first;
			for (;;)
			{
				if (parentIt->second == x)
					break;
				const auto gparentIt = mMap.find(parentIt->second);
				parentIt->second = gparentIt->second;
				x = parentIt->second;
				parentIt = mMap.find(x);
			}
			return x;
		}
	};

	template<class Array>
	class ArrayUnionFind
	{
	public:
		using T = Array::value_type;

		explicit ArrayUnionFind(Array& map) : mMap(map)
		{
		}

		Array& getArray() const
		{
			return mMap;
		}

		// <https://en.wikipedia.org/wiki/Disjoint-set_data_structure>

		[[nodiscard]] T find(T x) const
		{
			for (;;)
			{
				const T parent = mMap[x];
				if (parent == x)
					break;
				x = parent;
			}
			return x;
		}

		void unionise(T a, T b)
		{
			a = findWithPathHalving(a);
			b = findWithPathHalving(b);

			if (a != b)
				mMap[b] = a;
		}

	private:
		Array& mMap;

		[[nodiscard]] T findWithPathHalving(T x)
		{
			for (;;)
			{
				T& parent = mMap[x];
				if (parent == x)
					break;
				parent = mMap[parent];
				x = parent;
			}
			return x;
		}
	};
}