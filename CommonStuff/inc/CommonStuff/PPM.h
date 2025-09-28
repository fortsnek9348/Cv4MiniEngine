#pragma once

#include "aabb.h"

#include <filesystem>
#include <vector>

namespace heck
{
	struct Image
	{
		ivec2 dim{};
		std::vector<RGB8> pixels{};

		explicit Image(ivec2 dim) : dim(dim), pixels(dim.x * dim.y)
		{
		}

		RGB8 operator[](ivec2 coord) const
		{
			return pixels[coord.x + coord.y * dim.x];
		}

		RGB8& operator[](ivec2 coord)
		{
			return pixels[coord.x + coord.y * dim.x];
		}

		void drawPixel(ivec2 coord, RGB8 colour, uint8_t alpha);
		void fillRect(const iaabb2& rect, RGB8 colour, uint8_t alpha);
		void drawHLine(ivec2 coord, int length, RGB8 colour, uint8_t alpha);
		void drawVLine(ivec2 coord, int length, RGB8 colour, uint8_t alpha);
		void drawRect(const iaabb2& rect, RGB8 colour, uint8_t alpha);
		void drawLine(ivec2 a, ivec2 b, RGB8 colour, uint8_t alpha);
		
		void saveAsPPM(const std::filesystem::path& path) const;
	};
}