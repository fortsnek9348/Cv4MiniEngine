#include "inc/CommonStuff/PPM.h"
#include "inc/CommonStuff/range.h"

#include <fstream>
#include <iostream>

using namespace heck;

void Image::drawPixel(ivec2 coord, RGB8 colour, uint8_t alpha)
{
	if (unsigned(coord.x) < (unsigned)dim.x && unsigned(coord.y) < (unsigned)dim.y)
	{
		if (alpha >= UINT8_MAX)
			(*this)[coord] = colour;
		else
		{
			RGB8& dst = (*this)[coord];
			dst[0] = (dst[0] * (UINT8_MAX - alpha) + colour[0] * alpha) / UINT8_MAX;
			dst[1] = (dst[1] * (UINT8_MAX - alpha) + colour[1] * alpha) / UINT8_MAX;
			dst[2] = (dst[2] * (UINT8_MAX - alpha) + colour[2] * alpha) / UINT8_MAX;
		}
	}
}
void Image::fillRect(const iaabb2& rect, RGB8 colour, uint8_t alpha)
{
	for (const int y : range(rect.min.y, rect.max.y))
		for (const int x : range(rect.min.x, rect.max.x))
			drawPixel({ x, y }, colour, alpha);
}

void Image::drawHLine(ivec2 coord, int length, RGB8 colour, uint8_t alpha)
{
	length = std::min(length, std::max(0, dim.x - coord.x));
	for (const int x : range(coord.x, coord.x + length))
		drawPixel({ x, coord.y }, colour, alpha);
}

void Image::drawVLine(ivec2 coord, int length, RGB8 colour, uint8_t alpha)
{
	length = std::min(length, std::max(0, dim.y - coord.y));
	for (const int y : range(coord.y, coord.y + length))
		drawPixel({ coord.x, y }, colour, alpha);
}

void Image::drawRect(const iaabb2& rect, RGB8 colour, uint8_t alpha)
{
	drawHLine(rect.min, rect.size().x, colour, alpha);
	drawHLine({ rect.min.x, rect.max.y - 1 }, rect.size().x, colour, alpha);
	drawVLine(rect.min + ivec2(0, 1), rect.size().y - 2, colour, alpha);
	drawVLine({ rect.max.x - 1, rect.min.y + 1 }, rect.size().y - 2, colour, alpha);
}

void Image::drawLine(ivec2 a, ivec2 b, RGB8 colour, uint8_t alpha)
{
	// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	// TODO: Similar code from Cv4MiniEngine Framebuffer.

	const int dx = std::abs(b.x - a.x);
	const int dy = -std::abs(b.y - a.y);

	const int sx = a.x < b.x ? 1 : -1;
	const int sy = a.y < b.y ? 1 : -1;

	int error = dx + dy;

	for ([[maybe_unused]] size_t i = 0; ; ++i)
	{
		drawPixel(a, colour, alpha);

		if (a == b)
			break;

		const int e2 = error * 2;

		if (e2 >= dy)
		{
			if (a.x == b.x)
				break;
			error += dy;
			a.x += sx;
		}
		if (e2 <= dx)
		{
			if (a.y == b.y)
				break;
			error += dx;
			a.y += sy;
		}
	}
}

void Image::saveAsPPM(const std::filesystem::path& path) const
{
	// So we know if we've left in any debug PPM outputs...
	std::clog << "Saving PPM " << path << std::endl;

	std::ofstream file(path, std::ios::binary);

	file << "P6\n";
	file << dim.x << ' ' << dim.y << '\n';
	file << "255\n";
	for (const int y : range(dim.y))
	{
		for (const int x : range(dim.x))
		{
			const RGB8 rgb = (*this)[{ x, y }];
			file.write(reinterpret_cast<const char*>(&rgb), sizeof(rgb));
		}
	}
}