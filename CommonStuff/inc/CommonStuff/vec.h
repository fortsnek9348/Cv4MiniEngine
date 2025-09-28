#pragma once

#include "div.h"
#include "Hashing.h"

#include <cassert>

#include <algorithm>
#include <ostream>

namespace heck
{
	template<class T>
	struct vec2
	{
		T x{};
		T y{};

		vec2() = default;
		constexpr vec2(T s) : x(s), y(s) {}
		constexpr vec2(T x, T y) : x(x), y(y) {}

		template<class U>
			requires (!std::same_as<T, U>)
		explicit(!requires { T{ U() }; }) constexpr vec2(vec2<U> b) : x(T(b.x)), y(T(b.y))
		{
		}

		constexpr vec2 flipped() const
		{
			return { y, x };
		}

		constexpr T operator[](size_t i) const
		{
			return this->*std::array{ &vec2::x, &vec2::y } [i] ;
		}

		constexpr T& operator[](size_t i)
		{
			return this->*std::array{ &vec2::x, &vec2::y } [i] ;
		}

		friend constexpr vec2 vmin(vec2 a, vec2 b)
		{
			return { std::min(a.x, b.x), std::min(a.y, b.y) };
		}

		friend constexpr vec2 vmax(vec2 a, vec2 b)
		{
			return { std::max(a.x, b.x), std::max(a.y, b.y) };
		}

		friend constexpr vec2 operator-(vec2 b)
		{
			return { -b.x, -b.y };
		}

		friend constexpr vec2 operator+(vec2 a, vec2 b)
		{
			return { a.x + b.x, a.y + b.y };
		}

		friend constexpr vec2 operator-(vec2 a, vec2 b)
		{
			return { T(a.x - b.x), T(a.y - b.y) };
		}

		friend constexpr vec2 operator*(vec2 a, vec2 b)
		{
			return { a.x * b.x, a.y * b.y };
		}

		friend constexpr vec2 operator/(vec2 a, vec2 b)
		{
			return { a.x / b.x, a.y / b.y };
		}

		friend constexpr vec2 operator%(vec2 a, vec2 b)
		{
			return { a.x % b.x, a.y % b.y };
		}

		friend constexpr vec2 operator>>(vec2 a, int n)
		{
			return { a.x >> n, a.y >> n };
		}

		friend constexpr vec2 operator<<(vec2 a, int n)
		{
			return { a.x << n, a.y << n };
		}

		constexpr vec2& operator+=(vec2 a)
		{
			return *this = *this + a;
		}

		constexpr vec2& operator-=(vec2 a)
		{
			return *this = *this - a;
		}

		constexpr vec2& operator*=(vec2 a)
		{
			return *this = *this * a;
		}

		friend bool operator==(vec2 a, vec2 b) = default;
	};

	template<class T>
	struct vec3
	{
		T x{};
		T y{};
		T z{};

		vec3() = default;
		constexpr vec3(T s) : x(s), y(s) {}
		constexpr vec3(T x, T y, T z) : x(x), y(y), z(z) {}
		constexpr vec3(std::array<T, 3> a) : x(a[0]), y(a[1]), z(a[2]) {}

		constexpr std::array<T, 3> toArray() const
		{
			return { x, y, z };
		}

		constexpr T operator[](size_t i) const
		{
			return this->*std::array{ &vec3::x, &vec3::y, &vec3::z } [i] ;
		}

		constexpr T& operator[](size_t i)
		{
			return this->*std::array{ &vec3::x, &vec3::y, &vec3::z } [i] ;
		}

		friend constexpr vec3 vmin(vec3 a, vec3 b)
		{
			return { std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
		}

		friend constexpr vec3 vmax(vec3 a, vec3 b)
		{
			return { std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
		}

		friend constexpr vec3 operator-(vec3 b)
		{
			return { -b.x, -b.y, -b.z };
		}

		friend constexpr vec3 operator+(vec3 a, vec3 b)
		{
			return { a.x + b.x, a.y + b.y, a.z + b.z };
		}

		friend constexpr vec3 operator-(vec3 a, vec3 b)
		{
			return { a.x - b.x, a.y - b.y, a.z - b.z };
		}

		friend constexpr vec3 operator*(vec3 a, vec3 b)
		{
			return { a.x * b.x, a.y * b.y, a.z * b.z };
		}

		friend constexpr vec3 operator/(vec3 a, vec3 b)
		{
			return { a.x / b.x, a.y / b.y, a.z / b.z };
		}

		constexpr vec3& operator+=(vec3 a)
		{
			return *this = *this + a;
		}

		constexpr vec3& operator-=(vec3 a)
		{
			return *this = *this - a;
		}

		constexpr vec3& operator*=(vec3 a)
		{
			return *this = *this * a;
		}

		friend bool operator==(vec3 a, vec3 b) = default;
	};

	template<class T>
	inline std::ostream& operator<<(std::ostream& os, const vec2<T>& v)
	{
		return os << '[' << v.x << ", " << v.y << ']';
	}

	template<class T>
	inline std::wostream& operator<<(std::wostream& os, const vec2<T>& v)
	{
		return os << L'[' << v.x << L", " << v.y << L']';
	}

	using ivec2 = vec2<int>;
	using i16vec2 = vec2<int16_t>;
	using u8vec2 = vec2<uint8_t>;

	template<class N, class D>
	constexpr vec2<N> fdiv(vec2<N> a, D b)
	{
		return { fdiv(a.x, b), fdiv(a.y, b) };
	}

	template<class N, class D>
	constexpr vec2<N> cdiv(vec2<N> a, D b)
	{
		return { cdiv(a.x, b), cdiv(a.y, b) };
	}
	
	using RGB8 = vec3<uint8_t>;

	struct VectorHasher
	{
		static uint32_t operator()(ivec2 v)
		{
			return theIronBornHash32(v.x | (v.y << 16));
		}
	};

	struct VectorComparator
	{
		template<class T>
		static bool operator()(vec2<T> a, vec2<T> b)
		{
			return std::pair(a.y, a.x) < std::pair(b.y, b.x);
		}
	};
}