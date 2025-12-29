#pragma once

#include <array>
#include <span>

#include <HeckTextUI/Core.h>

namespace cvengine
{
	std::vector<uint8_t> encodeDXT1(hecktui::ivec2 dim, std::span<const std::array<uint8_t, 3>> src);
	std::vector<std::array<uint8_t, 3>> decodeDXT1(hecktui::ivec2 dim, std::span<const uint8_t> src);
}