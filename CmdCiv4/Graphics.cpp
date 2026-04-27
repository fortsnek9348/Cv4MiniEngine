#include "Graphics.h"

#include <CommonStuff/range.h>

#include <bit>
#include <ranges>
#include <fstream>

using heck::range;

WorldViewFramebuffer::WorldViewFramebuffer(heck::ivec2 dim) : mDim(dim), mCells(dim.x * dim.y, Cell{ .c = u' ', .textColour = White, .backColour = Black })//, mDepthBuffer(dim.x * dim.y, INT_MIN)
{
}

struct WorldViewFramebuffer::Internals
{
	WorldViewFramebuffer& fb;

	bool inBounds(heck::ivec2 coord) const
	{
		return static_cast<unsigned int>(coord.x) < static_cast<unsigned int>(fb.mDim.x)
			&& static_cast<unsigned int>(coord.y) < static_cast<unsigned int>(fb.mDim.y);
	}

	Cell& cell(heck::ivec2 coord) const
	{
		return fb.mCells[coord.x + coord.y * fb.mDim.x];
	}

	//int& depth(heck::ivec2 coord) const
	//{
	//	return fb.mDepthBuffer[coord.x + coord.y * fb.mDim.x];
	//}
	//
	//bool drawDepth(heck::ivec2 coord) const
	//{
	//	int& d = depth(coord);
	//	if (z >= d)
	//		return d = z, true;
	//	else
	//		return false;
	//}

	void drawCell(heck::ivec2 coord, Cell c) const
	{
		if (!c.ignoreTextColour)
			cell(coord).textColour = c.textColour;

		if (!c.ignoreBackColour)
			cell(coord).backColour = c.backColour;

		if (!c.ignoreChar)
		{
			cell(coord).c = c.c;
		}

		cell(coord).underline = c.underline;
	}

	std::optional<std::pair<heck::ivec2, heck::ivec2>> clipRect(heck::ivec2 coord, heck::ivec2 maxCoord) const
	{
		heck::ivec2 minCoord = coord;
		minCoord.x = std::max(minCoord.x, 0);
		minCoord.y = std::max(minCoord.y, 0);
		maxCoord.x = std::min(maxCoord.x, fb.mDim.x);
		maxCoord.y = std::min(maxCoord.y, fb.mDim.y);

		if (minCoord.x >= maxCoord.x ||
			minCoord.y >= maxCoord.y)
			return std::nullopt;

		const heck::ivec2 minDelta = minCoord - coord;
		const heck::ivec2 clippedDim = maxCoord - minCoord;

		return std::pair(minDelta, clippedDim);
	}
};

void WorldViewFramebuffer::drawCell(heck::ivec2 coord, Cell cell) noexcept
{
	const Internals internals{ *this };
	if (internals.inBounds(coord))
		internals.drawCell(coord, cell);
}

void WorldViewFramebuffer::drawBox(heck::ivec2 coord, heck::ivec2 size, Cell cell) noexcept
{
	drawVLine(coord, size.y, cell);
	drawVLine(coord + heck::ivec2(size.x - 1, 0), size.y, cell);

	//cell.c = u'▄';
	drawHLine(coord, size.x, cell);
	//cell.c = u'▀';
	drawHLine(coord + heck::ivec2(0, size.y - 1), size.x, cell);
}

void WorldViewFramebuffer::drawHLine(heck::ivec2 a, int n, Cell cell) noexcept
{
	const Internals internals{ *this };
	if (const auto optClipped = internals.clipRect(a, a + heck::ivec2(n, 1)))
	{
		const auto [minDelta, clippedDim] = *optClipped;
		for (int i = 0; i < clippedDim.x; ++i)
			internals.drawCell(a + minDelta + heck::ivec2(i, 0), cell);
	}
}
void WorldViewFramebuffer::drawVLine(heck::ivec2 a, int n, Cell cell) noexcept
{
	const Internals internals{ *this };
	if (const auto optClipped = internals.clipRect(a, a + heck::ivec2(1, n)))
	{
		const auto [minDelta, clippedDim] = *optClipped;
		for (int i = 0; i < clippedDim.y; ++i)
			internals.drawCell(a + minDelta + heck::ivec2(0, i), cell);
	}
}

void WorldViewFramebuffer::drawStippleLine(heck::ivec2 a, heck::ivec2 b, Cell cell, std::string_view pattern) noexcept
{
	// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

	const int dx = std::abs(b.x - a.x);
	const int dy = -std::abs(b.y - a.y);

	const int sx = a.x < b.x ? 1 : -1;
	const int sy = a.y < b.y ? 1 : -1;

	int error = dx + dy;

	const Internals internals(*this);

	if (internals.clipRect(vmin(a, b), vmax(a, b) + heck::ivec2(1, 1)))
	{
		for (size_t i = 0; ; ++i)
		{
			if (i >= pattern.size())
				i -= pattern.size();

			if (internals.inBounds(a) && pattern[i] != ' ')
				internals.drawCell(a, cell);

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
}

// NOTE: The pixel grid is (0.5, 0.5) at centers.
static void ellipseAlgorithm(hecktui::iaabb2 rect, auto plotHLine)
{
	const int x0 = rect.min.x;
	const int y0 = rect.min.y;
	const int x1 = rect.max.x - 1;
	const int y1 = rect.max.y - 1;

	// https://stackoverflow.com/questions/2914807/plot-ellipse-from-rectangle/54488533#54488533
	// Algorithm for any rect.
	// Modified by fortsnek to use int64_t.

	int xb, yb, xc, yc;


	// Calculate height
	yb = yc = (y0 + y1) / 2;
	int qb = (y0 < y1) ? (y1 - y0) : (y0 - y1);
	int qy = qb;
	int dy = qb / 2;
	if (qb % 2 != 0)
	{
		// Bounding box has even pixel height
		yc++;
	}

	// Calculate width
	xb = xc = (x0 + x1) / 2;
	int qa = (x0 < x1) ? (x1 - x0) : (x0 - x1);
	int qx = qa % 2;
	int dx = 0;
	int64_t qt = int64_t(qa)*qa + int64_t(qb)*qb - int64_t(2)*qa*qa*qb;
	if (qx != 0)
	{
		// Bounding box has even pixel width
		xc++;
		qt += int64_t(3)*qb*qb;
	}

	// Start at (dx, dy) = (0, b) and iterate until (a, 0) is reached
	while (qy >= 0 && qx <= qa)
	{
		// Draw the new points
		plotHLine({ xb-dx, yb-dy }, { xc+dx, yb-dy }); // TL, TR
		plotHLine({ xb-dx, yc+dy } , { xc+dx, yc+dy }); // BL, BR

		// If a (+1, 0) step stays inside the ellipse, do it
		if (qt + int64_t(2)*qb*qb*qx + int64_t(3)*qb*qb <= 0 ||
			qt + int64_t(2)*qa*qa*qy - int64_t(qa)*qa <= 0)
		{
			qt += int64_t(8)*qb*qb + int64_t(4)*qb*qb*qx;
			dx++;
			qx += 2;
			
		}
		// If a (0, -1) step stays outside the ellipse, do it
		else if (qt - int64_t(2)*qa*qa*qy + int64_t(3)*qa*qa > 0)
		{
			qt += int64_t(8)*qa*qa - int64_t(4)*qa*qa*qy;
			dy--;
			qy -= 2;
		}
		// Else step (+1, -1)
		else
		{
			qt += int64_t(8)*qb*qb + int64_t(4)*qb*qb*qx + int64_t(8)*qa*qa - int64_t(4)*qa*qa*qy;
			dx++;
			qx += 2;
			dy--;
			qy -= 2;
		}
	}
}

void WorldViewFramebuffer::drawStippleEllipse(const hecktui::iaabb2& rect, Cell cell, std::string_view pattern) noexcept
{
	size_t strippleI = 0;

	ellipseAlgorithm(rect, [this, cell, &strippleI, pattern](heck::ivec2 L, heck::ivec2 R) {
		if (strippleI >= pattern.size())
			strippleI -= pattern.size();
		if (pattern[strippleI++] != ' ')
		{
			//const heck::ivec2 c = rect.min + rect.size() / 2;
			//const int cxEven = rect.size().x % 2 == 0;
			//const int cyEven = rect.size().y % 2 == 0;
			//const heck::ivec2 rel = coord - c;
			drawCell(L, cell); // TR
			drawCell(R, cell); // TR
			//drawCell(c + heck::ivec2(rel.x, -rel.y - cyEven), cell); // BR
			//drawCell(c + heck::ivec2(-rel.x + cxEven, rel.y), cell); // TL
			//drawCell(c + heck::ivec2(-rel.x + cxEven, -rel.y - cyEven), cell); // BL
		}
	});
}

void WorldViewFramebuffer::drawFilledEllipse(const hecktui::iaabb2& rect, Cell cell) noexcept
{
	ellipseAlgorithm(rect, [this, cell](heck::ivec2 L, heck::ivec2 R) {
		drawHLine(L, R.x - L.x + 1, cell);
	});
}

void WorldViewFramebuffer::drawSprite(heck::ivec2 coord, ConstMDSpan cells) noexcept
{
	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(coord, coord + cells.dim))
	{
		const auto& [minDelta, clippedDim] = *optClip;

		for (int y = 0; y < clippedDim.y; ++y)
		{
			for (int x = 0; x < clippedDim.x; ++x)
			{
				const heck::ivec2 srcCoord = minDelta + heck::ivec2{ x, y };
				internals.drawCell(coord + srcCoord, cells[srcCoord]);
			}
		}
	}
}

void WorldViewFramebuffer::drawSpriteWithColourRemap(heck::ivec2 coord, ConstMDSpan cells, std::span<const EColour> colours) noexcept
{
	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(coord, coord + cells.dim))
	{
		const auto& [minDelta, clippedDim] = *optClip;

		for (int y = 0; y < clippedDim.y; ++y)
		{
			for (int x = 0; x < clippedDim.x; ++x)
			{
				const heck::ivec2 srcCoord = minDelta + heck::ivec2{ x, y };
				Cell cell = cells[srcCoord];
				cell.textColour = colours[int(cell.textColour)];
				cell.backColour = colours[int(cell.backColour)];
				internals.drawCell(coord + srcCoord, cell);
			}
		}
	}
}

void WorldViewFramebuffer::fill(heck::ivec2 min, heck::ivec2 max, Cell cell) noexcept
{
	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(min, max))
	{
		const auto& [minDelta, clippedDim] = *optClip;

		for (int y = 0; y < clippedDim.y; ++y)
			for (int x = 0; x < clippedDim.x; ++x)
				internals.drawCell(min + minDelta + heck::ivec2{ x, y }, cell);
	}
}

void WorldViewFramebuffer::fillPattern(heck::ivec2 min, heck::ivec2 max, ConstMDSpan cells) noexcept
{
	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(min, max))
	{
		const auto& [minDelta, clippedDim] = *optClip;
		const heck::ivec2 srcDim = cells.dim;

		for (int y = 0; y < clippedDim.y; ++y)
		{
			for (int x = 0; x < clippedDim.x; ++x)
			{
				heck::ivec2 srcCoord = minDelta + heck::ivec2{ x, y };
				srcCoord.x %= srcDim.x;
				srcCoord.y %= srcDim.y;
				internals.drawCell(min + minDelta + heck::ivec2{ x, y }, cells[srcCoord]);
			}
		}
	}
}

void WorldViewFramebuffer::drawCenteredSprite(heck::ivec2 centerCoord, ConstMDSpan cells) noexcept
{
	return drawSprite(centerCoord - cells.dim, cells);
}



//void WorldViewFramebuffer::drawBox(heck::ivec2 coord, heck::ivec2 dim, LineStyle style) noexcept
//{
//
//}

//void WorldViewFramebuffer::drawHLine(heck::ivec2 a, int bx, ELineStyle style) noexcept
//{
//}
//
//void WorldViewFramebuffer::drawVLine(heck::ivec2 a, int by, ELineStyle style) noexcept
//{
//}
//
//void WorldViewFramebuffer::drawAALine(heck::ivec2 a, heck::ivec2 b, ELineStyle style) noexcept
//{
//}

//void WorldViewFramebuffer::drawLine(heck::ivec2 a, heck::ivec2 b, LineStyle style) noexcept
//{
//}

void WorldViewFramebuffer::drawText(heck::ivec2 a, int alignWidth, std::wstring_view str, Cell attribs, EHAlign halign) noexcept
{
	switch (halign)
	{
	case EHAlign::Left: break;
	case EHAlign::Center: a.x += (alignWidth - int(str.size())) / 2; break;
	case EHAlign::Right: a.x += alignWidth - (int)str.size(); break;
	default:
		std::unreachable();
	}

	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(a, a + heck::ivec2((int)str.size(), 1)))
	{
		const auto& [minDelta, clippedDim] = *optClip;

		for (int x = 0; x < clippedDim.x; ++x)
		{
			attribs.c = str[minDelta.x + x];
			internals.drawCell(a + minDelta + heck::ivec2{ x, 0 }, attribs);
		}
	}
}

void WorldViewFramebuffer::drawText(heck::ivec2 a, int alignWidth, std::span<const Cell> str, EHAlign halign) noexcept
{
	switch (halign)
	{
	case EHAlign::Left: break;
	case EHAlign::Center: a.x += (alignWidth - int(str.size())) / 2; break;
	case EHAlign::Right: a.x += alignWidth - (int)str.size(); break;
	default:
		std::unreachable();
	}

	const Internals internals{ *this };

	if (const auto optClip = internals.clipRect(a, a + heck::ivec2((int)str.size(), 1)))
	{
		const auto& [minDelta, clippedDim] = *optClip;

		for (int x = 0; x < clippedDim.x; ++x)
			internals.drawCell(a + minDelta + heck::ivec2{ x, 0 }, str[minDelta.x + x]);
	}
}

const WorldViewFramebuffer::Cell& WorldViewFramebuffer::operator[](heck::ivec2 coord) const noexcept
{
	return mCells[coord.x + coord.y * mDim.x];
}

WorldViewFramebuffer::ConstMDSpan WorldViewFramebuffer::getCells() const noexcept
{
	return ConstMDSpan(mCells, mDim);
}

WorldViewFramebuffer::MDSpan WorldViewFramebuffer::getCells() noexcept
{
	return MDSpan(mCells, mDim);
}

void WorldViewFramebuffer::saveAsPPM(const std::filesystem::path& path) const
{
	std::ofstream file(path, std::ios::binary);

	file << "P6\n";
	file << mDim.x << ' ' << mDim.y << '\n';
	file << "255\n";
	for (const int y : range(mDim.y))
	{
		for (const int x : range(mDim.x))
		{
			const hecktui::RGB8 rgb = hecktui::toRGB8((*this)[{ x, y }].textColour);
			file.write(reinterpret_cast<const char*>(&rgb), sizeof(rgb));
		}
	}
}