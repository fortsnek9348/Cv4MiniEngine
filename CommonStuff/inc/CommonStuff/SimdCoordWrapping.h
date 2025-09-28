#pragma once

#include "SimdAStarGraph.h"

namespace heck
{
	struct SimdCoordWrappingResult
	{
		SimdAStarConfig::Vector X;
		SimdAStarConfig::Vector Y;
		SimdAStarConfig::Vector::Mask validMask;
	};

	inline SimdCoordWrappingResult wrapCoordinates(vec2<SimdAStarConfig::Vector> unwrapped, ivec2 dim, bool wrapX, bool wrapY,
		vec2<SimdAStarConfig::Vector> def)
	{
		// WARNING: clang-cl is sensitive to how this function is written.
		//          Write this a slightly different way, and the compiler might run out of memory in instruction selection.
		//          PQCoordSimdAStar.cpp should take ~1.5s to compile, not an eternity.

		SimdAStarConfig::Vector::Mask invalidMask{};

		const SimdAStarConfig::Vector::Mask minXViolated = vcmplt(unwrapped.x, 0);
		const SimdAStarConfig::Vector::Mask maxXViolated = vcmple(dim.x, unwrapped.x);
		const SimdAStarConfig::Vector::Mask minYViolated = vcmplt(unwrapped.y, 0);
		const SimdAStarConfig::Vector::Mask maxYViolated = vcmple(dim.y, unwrapped.y);

		if (wrapX)
		{
			unwrapped.x = vmaskedadd(minXViolated, unwrapped.x, dim.x);
			unwrapped.x = vmaskedsub(maxXViolated, unwrapped.x, dim.x);
		}
		else
		{
			invalidMask |= minXViolated | maxXViolated;
			// Use a known valid coordinate for unmasked gathers.
			//unwrapped.x = vselect(minXViolated | maxXViolated, def.x, unwrapped.x);
		}

		if (wrapY)
		{
			unwrapped.y = vmaskedadd(minYViolated, unwrapped.y, dim.y);
			unwrapped.y = vmaskedsub(maxYViolated, unwrapped.y, dim.y);
		}
		else
		{
			invalidMask |= minYViolated | maxYViolated;
			// Use a known valid coordinate for unmasked gathers.
			//unwrapped.y = vselect(minYViolated | maxYViolated, def.y, unwrapped.y);
		}

		unwrapped.x = vselect(invalidMask, def.x, unwrapped.x);
		unwrapped.y = vselect(invalidMask, def.y, unwrapped.y);

		return { unwrapped.x, unwrapped.y, ~invalidMask };
	}
}