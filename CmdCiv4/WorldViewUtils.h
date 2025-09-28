#pragma once

#include "Graphics.h"

namespace WorldViewUtils
{
	using EColour = WorldViewFramebuffer::EColour;

	WorldViewFramebuffer renderHill(heck::ivec2 dim, std::array<EColour, 4> colours);
	WorldViewFramebuffer renderPeak(heck::ivec2 dim, std::array<EColour, 4> baseColours, std::array<EColour, 4> snowColours);
}