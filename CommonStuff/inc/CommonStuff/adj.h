#pragma once

#include "vec.h"

#include <array>

namespace heck
{
	inline constexpr std::array<ivec2, 8> kAdj{ {
		{ -1, -1 },
		{ +0, -1 },
		{ +1, -1 },
		{ -1, +0 },
		// 0,  0
		{ +1, +0 },
		{ -1, +1 },
		{ +0, +1 },
		{ +1, +1 },
	} };

	inline constexpr unsigned int kNumAdj = std::size(kAdj);

	inline constexpr std::array kInvAdjIndices{ 7, 6, 5, 4, 3, 2, 1, 0 };
}