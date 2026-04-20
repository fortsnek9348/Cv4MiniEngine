#pragma once

#include <concepts>

#include <stdint.h>
#include <stddef.h>

namespace heck
{
	// Returns floor(x / y).
	// Deduce divisor type to be absolutely sure it's unsigned.
	constexpr int fdiv(int x, std::same_as<uint32_t> auto y)
	{
		const int q = int(x / (int64_t)y); // Cast in case y > INT_MAX.
		return q - (x % (int64_t)y < 0);
	}

	// Returns ceil(x / y).
	constexpr int cdiv(int x, std::same_as<uint32_t> auto y)
	{
		const int q = int(x / (int64_t)y);
		return q + (x % (int64_t)y > 0);
	}

	// Returns ceil(x / y).
	constexpr size_t cdiv(std::same_as<size_t> auto x, std::unsigned_integral auto y)
	{
		return x / y + (x % y != 0);
	}

	// Returns round(x / y), rounding ties away from zero.
	constexpr int rdiv(int x, std::same_as<uint32_t> auto y)
	{
		const int q = int(x / (int64_t)y);
		if (x >= 0)
			return q + (x % (int64_t)y * 2 >= y);
		else
			return q - (x % (int64_t)y * 2 <= -(int64_t)y);
	}
}