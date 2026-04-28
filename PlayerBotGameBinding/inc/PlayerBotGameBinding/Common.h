#pragma once

#include <span>
#include <stdexcept>
#include <cassert>
#include <cstdint>

namespace cvbot
{
	struct i16vec2;

	struct [[nodiscard]] ivec2
	{
		int x{};
		int y{};

		friend int dot(ivec2 a, ivec2 b)
		{
			return a.x * b.x + a.y * b.y;
		}


		int lengthSq() const
		{
			return dot(*this, *this);
		}


		friend ivec2 operator+(ivec2 a, ivec2 b)
		{
			return { a.x + b.x, a.y + b.y };
		}

		friend ivec2 operator-(ivec2 a, ivec2 b)
		{
			return { a.x - b.x, a.y - b.y };
		}

		// For coord sorting.
		friend bool operator<(ivec2 a, ivec2 b)
		{
			return a.y < b.y || (a.y == b.y && a.x < b.x);
		}

		constexpr explicit operator i16vec2() const;

		friend bool operator==(ivec2, ivec2) = default;
	};

	// This is for storage in packed structs and arrays. Use ivec2 for math.
	struct [[nodiscard]] i16vec2
	{
		int16_t x{};
		int16_t y{};

		friend bool operator==(i16vec2 a, i16vec2 b) = default;

		// For coord sorting.
		friend bool operator<(i16vec2 a, i16vec2 b)
		{
			return a.y < b.y || (a.y == b.y && a.x < b.x);
		}

		friend std::strong_ordering operator<=>(i16vec2, i16vec2) = default;

		operator ivec2() const
		{
			return { x, y };
		}
	};

	constexpr ivec2::operator i16vec2() const
	{
		return { static_cast<int16_t>(x), static_cast<int16_t>(y) };
	}

	struct [[nodiscard]] iaabb2
	{
		ivec2 min{};
		ivec2 max{}; // exclusive

		static iaabb2 sized(ivec2 min, ivec2 dim)
		{
			return { min, min + dim };
		}

		int area() const
		{
			return (max.x - min.x) * (max.y - min.y);
		}
	};

	template<class T>
	struct Span2D
	{
		std::span<T> linear;
		ivec2 dim{};
		int pitch{};

		Span2D() = default;
		Span2D(std::span<T> linear, ivec2 dim) : linear(linear), dim(dim), pitch(dim.x)
		{
			assert(dim.x == 0 || dim.y == 0 || (dim.x > 0 && dim.y > 0 && pitch >= dim.x && static_cast<size_t>(dim.x - 1) + static_cast<size_t>(dim.y - 1) * static_cast<size_t>(pitch) < linear.size()));
		}

		template<class U>
			requires (!std::same_as<T, U>&& std::same_as<T, const U>)
		Span2D(Span2D<U> b) : linear(b.linear), dim(b.dim), pitch(b.pitch)
		{
		}

		T& operator[](ivec2 p) const
		{
			assert(unsigned(p.x) < unsigned(dim.x) && unsigned(p.y) < unsigned(dim.y));
			return linear[p.x + p.y * pitch];
		}
	};

	class BotFailure : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	// Avoiding dependency on CommonStuff.
	// Returns ceil(x / y).
	int cdiv(int x, std::same_as<std::uint32_t> auto y);
}