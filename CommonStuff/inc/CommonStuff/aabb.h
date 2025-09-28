#pragma once

#include "vec.h"

namespace heck
{
	template<class T>
	struct aabb2
	{
		using V = vec2<T>;
		V min{};
		V max{};

		V size() const
		{
			return max - min;
		}

		bool contains(V p) const
		{
			return min.x <= p.x && p.x < max.x && min.y <= p.y && p.y < max.y;
		}

		// Full containment.
		bool contains(aabb2 b) const
		{
			return contains(b.min) && contains(b.max);
		}

		bool empty() const
		{
			return min.x >= max.x || min.y >= max.y;
		}

		void combine(aabb2 rect)
		{
			if (empty())
				*this = rect;
			else if (!rect.empty())
			{
				min = vmin(min, rect.min);
				max = vmax(max, rect.max);
			}
		}

		aabb2 combined(aabb2 b) const
		{
			b.min.x = std::min(min.x, b.min.x);
			b.min.y = std::min(min.y, b.min.y);
			b.max.x = std::max(max.x, b.max.x);
			b.max.y = std::max(max.y, b.max.y);
			return b;
		}
		aabb2 intersection(aabb2 b) const
		{
			b.min.x = std::max(min.x, b.min.x);
			b.min.y = std::max(min.y, b.min.y);
			b.max.x = std::min(max.x, b.max.x);
			b.max.y = std::min(max.y, b.max.y);
			return b;
		}

		aabb2 shrunk(V d) const
		{
			aabb2 b = *this;
			b.min += d;
			b.max -= d;
			return b;
		}
		aabb2 expanded(V d) const
		{
			aabb2 b = *this;
			b.min -= d;
			b.max += d;
			return b;
		}

		auto area() const
		{
			const V s = size();
			return s.x * s.y;
		}

		template<class U>
		aabb2<U> cast() const
		{
			return {
				.min = vec2<U>(min),
				.max = vec2<U>(max),
			};
		}

		static aabb2 combineOneOrMore(std::ranges::input_range auto&& boxes, aabb2 def)
		{
			if (std::ranges::empty(boxes))
				return def;

			def = *std::ranges::begin(boxes);
			for (const aabb2& box : boxes)
				def = def.combined(box);
			return def;
		}

		static constexpr aabb2 sized(V position, V size)
		{
			return { position, position + size };
		}

		friend aabb2 operator+(const aabb2& a, V v)
		{
			return { a.min + v, a.max + v };
		}

		friend aabb2 operator-(const aabb2& a, V v)
		{
			return { a.min - v, a.max - v };
		}

		friend aabb2 operator+(V v, const aabb2& a)
		{
			return a + v;
		}

		// Because aabb2 uses exclusive bounds, this clamps to max - 1!
		// Another way of looking at it: `p` represents an aabb of size `1`.
		friend V vclampGridCellCoord(V p, const aabb2& rect)
		{
			assert(!rect.empty());
			return {
				std::clamp(p.x, rect.min.x, rect.max.x - 1),
				std::clamp(p.y, rect.min.y, rect.max.y - 1),
			};
		}

		friend bool operator==(aabb2, aabb2) = default;
	};

	template<class T>
	inline std::ostream& operator<<(std::ostream& os, const aabb2<T>& v)
	{
		return os << '{' << v.min << ".." << v.max << '}';
	}

	using iaabb2 = aabb2<int>;
	using i16aabb2 = aabb2<int16_t>;


}