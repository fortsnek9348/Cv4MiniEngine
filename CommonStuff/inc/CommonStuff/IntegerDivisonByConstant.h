#pragma once

#include <bit>

namespace heck
{
	struct IntegerDivisionByConstantParams
	{
		uint32_t add = 0;
		uint32_t M = 0;
		uint32_t shift = 0;

		IntegerDivisionByConstantParams() = default;

		// https://rubenvannieuwpoort.nl/posts/division-by-constant-unsigned-integers
		// https://stackoverflow.com/questions/45353629/repeated-integer-division-by-a-runtime-constant-value/45353957#45353957
		constexpr explicit IntegerDivisionByConstantParams(uint32_t divisor)
		{
			const unsigned int L = std::bit_width(divisor) - 1;

			shift = L;

			if (std::has_single_bit(divisor))
			{
				M = UINT32_MAX;
				add = 1;
			}
			else
			{
				const uint32_t m_down = (uint64_t(1) << (32 + L)) / divisor;
				const uint32_t m_up = m_down + 1;
				const uint32_t temp = m_up * divisor;
				const bool use_round_up_method = temp <= (uint32_t(1) << L);

				if (use_round_up_method)
				{
					M = m_up;
					add = 0;
				}
				else
				{
					M = m_down;
					add = 1;
				}
			}
		}

		friend constexpr uint32_t operator/(uint32_t N, IntegerDivisionByConstantParams D)
		{
			return uint32_t((uint64_t(N + D.add) * D.M) >> 32) >> D.shift;
		}
	};

	static_assert(4327868 / IntegerDivisionByConstantParams(3) == 4327868 / 3);
	static_assert((1 << 3) / IntegerDivisionByConstantParams(7) == (1 << 3) / 7);
	static_assert(234934649 / IntegerDivisionByConstantParams(32) == 234934649 / 32);
	static_assert(0 / IntegerDivisionByConstantParams(4) == 0 / 4);
	static_assert(4 / IntegerDivisionByConstantParams(1) == 4 / 1);
}