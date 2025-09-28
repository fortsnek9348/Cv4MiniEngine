#include "inc/HeckTextUI/HeckTextUI.h"

#include "Util.h"

#include <array>
#include <bit>
#include <cassert>
#include <cwctype>
#include <cmath>

using namespace hecktui;

static bool isWhitespace(wchar_t c)
{
	return (bool)std::iswspace((std::uint16_t)c);
}

/*static std::wstring_view consumeWord(std::wstring_view& text)
{
	const auto it = std::ranges::find_if_not(text, isWhitespace);
	const std::wstring_view word(text.begin(), it);
	text = { it, text.end() };
	return word;
}

static std::wstring_view consumeNonWord(std::wstring_view& text)
{
	const auto it = std::ranges::find_if(text, isWhitespace);
	const std::wstring_view word(text.begin(), it);
	text = { it, text.end() };
	return word;
}*/

static std::wstring_view rtrim(std::wstring_view text)
{
	return { text.begin(), std::find_if_not(text.rbegin(), text.rend(), isWhitespace).base() };
}

static std::wstring_view rtrimWordChars(std::wstring_view text)
{
	return { text.begin(), std::find_if(text.rbegin(), text.rend(), isWhitespace).base() };
}

std::wstring_view hecktui::consumeWrappedTextLine(std::wstring_view& text, int width) noexcept
{
	width = std::max(1, width);

	std::wstring_view local = text;
	std::wstring_view line = rtrim(consumeMultilineTextLine(local));

	if (line.size() <= static_cast<size_t>(width))
	{
		text = local;
		return line;
	}
	else
	{
		const auto chopIt = line.size() >= static_cast<size_t>(width) ? std::find_if_not(line.begin() + width, line.end(), isWhitespace) : line.end();
		const size_t chopWidth = chopIt - line.begin();
		std::wstring_view chopped = line.substr(0, chopWidth);
		std::wstring_view wordCharsChopped = chopped;
		if (chopWidth < line.size() && !isWhitespace(line[chopWidth]))
			wordCharsChopped = rtrimWordChars(chopped);
		if (wordCharsChopped.empty())
			wordCharsChopped = chopped;
		text.remove_prefix(wordCharsChopped.size());
		std::wstring_view whitespaceChopped = rtrim(wordCharsChopped);
		return whitespaceChopped;
	}
}
std::wstring_view hecktui::consumeMultilineTextLine(std::wstring_view& text) noexcept
{
	const auto it = std::ranges::find(text, L'\n');
	const std::wstring_view line(text.begin(), it);
	if (it == text.end())
		text = { text.end(), text.end() };
	else
		text = { it + 1, text.end() };
	return line;
}
ivec2 hecktui::calcWrappedTextMinSize(std::wstring_view text) noexcept
{
	const int numLines = int(1 + std::ranges::count(text, L'\n'));

	const int longestWord = text.empty() ? 1 : int(std::ranges::max(text | std::views::chunk_by([](wchar_t a, wchar_t b) {
		return isWhitespace(a) != isWhitespace(b);
	}) | std::views::transform([](std::ranges::subrange<std::wstring_view::const_iterator> word) {
		return isWhitespace(word.front()) ? 0 : word.size();
	})));

	return { longestWord, numLines };
}
ivec2 hecktui::calcWrappedTextSizeForWidth(std::wstring_view text, int width) noexcept
{
	int n = 0;
	int maxLength = 0;
	do
	{
		maxLength = std::max(maxLength, (int)consumeWrappedTextLine(text, width).size());
		++n;
	} while (!text.empty());
	return { maxLength, n };
}
ivec2 hecktui::calcMultilineTextSize(std::wstring_view text) noexcept
{
	int n = 0;
	int maxLength = 0;
	do
	{
		maxLength = std::max(maxLength, (int)consumeMultilineTextLine(text).size());
		++n;
	} while (!text.empty());
	return { maxLength, n };
}

Framebuffer::Framebuffer(ivec2 dim) : mDim(dim), mPixels(size_t(dim.x) * dim.y)
{
}

ivec2 Framebuffer::getDim() const
{
	return mDim;
}

void Framebuffer::setInputCaret(InputCaret caret)
{
	mInputCaret = caret;
}

void Framebuffer::clear()
{
	mStencil = iaabb2{ .max = mDim };
	mInputCaret = {};
	std::ranges::fill(mPixels, Pixel());
}

void Framebuffer::setScissorRect(iaabb2 aabb) noexcept
{
	mStencil = iaabb2{ .max = mDim }.intersection(aabb);
}

const iaabb2& Framebuffer::getScissorRect() const noexcept
{
	return mStencil;
}

iaabb2 Framebuffer::intersectScissorRect(iaabb2 aabb) noexcept
{
	return std::exchange(mStencil, mStencil.intersection(aabb));
}

void Framebuffer::draw(ivec2 coord, const Pixel& pixel)
{
	if (!mStencil.contains(coord))
		return;

	Pixel& dst = mPixels[coord.x + coord.y * mDim.x];

	const PixelColouring dstColouring = dst.colour;

	dst = pixel;

	if (pixel.colour.text.transparent)
		dst.colour.text = dstColouring.text;
	if (pixel.colour.back.transparent)
		dst.colour.back = dstColouring.back;
}

void Framebuffer::recolour(ivec2 coord, PixelColouring colouring)
{
	if (!mStencil.contains(coord))
		return;

	Pixel& dst = mPixels[coord.x + coord.y * mDim.x];

	if (!colouring.text.transparent)
		dst.colour.text = colouring.text.code;
	if (!colouring.back.transparent)
		dst.colour.back = colouring.back.code;
}

namespace
{
	// 0 == none
	// 1 == thin
	// 2 == thick
	// 3 == double
	struct BorderCharDesc
	{
		std::uint16_t up : 2;
		std::uint16_t right : 2;
		std::uint16_t down : 2;
		std::uint16_t left : 2;
		// Curved corners, broken straights
		std::uint16_t curved_broken : 1 {};

		constexpr operator std::uint16_t() const
		{
			// MSVC doesn't support bit_cast on this.
			return uint16_t(up * (1 << 0) + right * (1 << 2) + down * (1 << 4) + left * (1 << 6) + curved_broken * (1 << 8));
		}
	};

	

	//constexpr uint8_t kUp = 1 << 0;
	//constexpr uint8_t kRight = 1 << 2;
	//constexpr uint8_t kDown = 1 << 4;
	//constexpr uint8_t kLeft = 1 << 6;
	//constexpr uint8_t kThin = 1;
	//constexpr uint8_t kThic = 2;
	//constexpr uint8_t kDubl = 3;
	//constexpr uint16_t kSpecial = 1 << 8;

	struct alignas(4) BorderCharTableEntry
	{
		wchar_t c = 0;
		BorderCharDesc desc{};
	};


	constexpr auto kBorderCharTable = std::to_array<BorderCharTableEntry>({
		// U+250x 	─ 	━ 	│ 	┃ 	┄ 	┅ 	┆ 	┇ 	┈ 	┉ 	┊ 	┋ 	┌ 	┍ 	┎ 	┏
		{ L'─', { 0, 1, 0, 1 } },
		{ L'━', { 0, 2, 0, 2 } },
		{ L'│', { 1, 0, 1, 0 } },
		{ L'┃', { 2, 0, 2, 0 } },
		{ L'┌', { 0, 1, 1, 0 } },
		{ L'┍', { 0, 2, 1, 0 } },
		{ L'┎', { 0, 1, 2, 0 } },
		{ L'┏', { 0, 2, 2, 0 } },
		// U+251x 	┐ 	┑ 	┒ 	┓ 	└ 	┕ 	┖ 	┗ 	┘ 	┙ 	┚ 	┛ 	├ 	┝ 	┞ 	┟
		{ L'┐', { 0, 0, 1, 1 } },
		{ L'┑', { 0, 0, 1, 2 } },
		{ L'┒', { 0, 0, 2, 1 } },
		{ L'┓', { 0, 0, 2, 2 } },
		{ L'└', { 1, 1, 0, 0 } },
		{ L'┕', { 1, 2, 0, 0 } },
		{ L'┖', { 2, 1, 0, 0 } },
		{ L'┗', { 2, 2, 0, 0 } },
		{ L'┘', { 1, 0, 0, 1 } },
		{ L'┙', { 1, 0, 0, 2 } },
		{ L'┚', { 2, 0, 0, 1 } },
		{ L'┛', { 2, 0, 0, 2 } },
		{ L'├', { 1, 1, 1, 0 } },
		{ L'┝', { 1, 2, 1, 0 } },
		{ L'┞', { 2, 1, 1, 0 } },
		{ L'┟', { 1, 1, 2, 0 } },

		{ L'┠', { 2, 1, 2, 0 } },
		{ L'┡', { 2, 2, 1, 0 } },
		{ L'┢', { 1, 2, 2, 0 } },
		{ L'┣', { 2, 2, 2, 0 } },

		{ L'┤', { 1, 0, 1, 1 } },
		{ L'┥', { 1, 0, 1, 2 } },
		{ L'┦', { 2, 0, 1, 1 } },
		{ L'┧', { 1, 0, 2, 1 } },

		{ L'┨', { 2, 0, 2, 1 } },
		{ L'┩', { 2, 0, 1, 2 } },
		{ L'┪', { 1, 0, 2, 2 } },
		{ L'┫', { 2, 0, 2, 2 } },

		{ L'┬', { 0, 1, 1, 1 } },
		{ L'┭', { 0, 1, 1, 2 } },
		{ L'┮', { 0, 2, 1, 1 } },
		{ L'┯', { 0, 2, 1, 2 } },

		{ L'┰', { 0, 1, 2, 1 } },
		{ L'┱', { 0, 1, 2, 2 } },
		{ L'┲', { 0, 2, 2, 1 } },
		{ L'┳', { 0, 2, 2, 2 } },

		{ L'┴', { 1, 1, 0, 1 } },
		{ L'┵', { 1, 1, 0, 2 } },
		{ L'┶', { 1, 2, 0, 1 } },
		{ L'┷', { 1, 2, 0, 2 } },

		{ L'┸', { 2, 1, 0, 1 } },
		{ L'┹', { 2, 1, 0, 2 } },
		{ L'┺', { 2, 2, 0, 1 } },
		{ L'┻', { 2, 2, 0, 2 } },

		{ L'┼', { 1, 1, 1, 1 } },
		{ L'┽', { 1, 1, 1, 2 } },
		{ L'┾', { 1, 2, 1, 1 } },
		{ L'┿', { 1, 2, 1, 2 } },

		{ L'╀', { 2, 1, 1, 1 } },
		{ L'╁', { 1, 1, 2, 1 } },
		{ L'╂', { 2, 1, 2, 1 } },
		{ L'╃', { 2, 1, 1, 2 } },

		{ L'╄', { 2, 2, 1, 1 } },
		{ L'╅', { 1, 1, 2, 2 } },
		{ L'╆', { 1, 2, 2, 1 } },
		{ L'╇', { 2, 2, 1, 2 } },

		{ L'╈', { 1, 2, 2, 2 } },
		{ L'╉', { 2, 1, 2, 2 } },
		{ L'╊', { 2, 2, 2, 1 } },
		{ L'╋', { 2, 2, 2, 2 } },

		{ L'╌', { 0, 1, 0, 1, 1 } },
		{ L'╍', { 0, 2, 0, 2, 1 } },
		{ L'╎', { 1, 0, 1, 0, 1 } },
		{ L'╏', { 2, 0, 2, 0, 1 } },
		
		{ L'═', { 0, 3, 0, 3 } },
		{ L'║', { 3, 0, 3, 0 } },

		{ L'╒', { 0, 3, 1, 0 } },
		{ L'╓', { 0, 1, 3, 0 } },
		{ L'╔', { 0, 3, 3, 0 } },

		{ L'╕', { 0, 0, 1, 3 } },
		{ L'╖', { 0, 0, 3, 1 } },
		{ L'╗', { 0, 0, 3, 3 } },

		{ L'╘', { 1, 3, 0, 0 } },
		{ L'╙', { 3, 1, 0, 0 } },
		{ L'╚', { 3, 3, 0, 0 } },

		{ L'╛', { 1, 0, 0, 3 } },
		{ L'╜', { 3, 0, 0, 1 } },
		{ L'╝', { 3, 0, 0, 3 } },

		{ L'╞', { 1, 3, 1, 0 } },
		{ L'╟', { 3, 1, 3, 0 } },
		{ L'╠', { 3, 3, 3, 0 } },

		{ L'╡', { 1, 0, 1, 3 } },
		{ L'╢', { 3, 0, 3, 1 } },
		{ L'╣', { 3, 0, 3, 3 } },

		{ L'╤', { 0, 3, 1, 3 } },
		{ L'╥', { 0, 1, 3, 1 } },
		{ L'╦', { 0, 3, 3, 3 } },

		{ L'╧', { 1, 3, 0, 3 } },
		{ L'╨', { 3, 1, 0, 1 } },
		{ L'╩', { 3, 3, 0, 3 } },

		{ L'╪', { 1, 3, 1, 3 } },
		{ L'╫', { 3, 1, 3, 1 } },
		{ L'╬', { 3, 3, 3, 3 } },

		{ L'╭', { 0, 1, 1, 0, 1 } },
		{ L'╮', { 0, 0, 1, 1, 1 } },
		{ L'╯', { 1, 0, 0, 1, 1 } },
		{ L'╰', { 1, 1, 0, 0, 1 } },

		{ L'╴', { 0, 0, 0, 1 } },
		{ L'╵', { 1, 0, 0, 0 } },
		{ L'╶', { 0, 1, 0, 0 } },
		{ L'╷', { 0, 0, 1, 0 } },

		{ L'╸', { 0, 0, 0, 1 } },
		{ L'╹', { 1, 0, 0, 0 } },
		{ L'╺', { 0, 1, 0, 0 } },
		{ L'╻', { 0, 0, 1, 0 } },

		{ L'╼', { 0, 2, 0, 1 } },
		{ L'╽', { 1, 0, 2, 0 } },
		{ L'╾', { 0, 1, 0, 2 } },
		{ L'╿', { 2, 0, 1, 0 } },
	});

	constexpr wchar_t kBorderCharBase = 0x2500;

	constexpr auto kBorderCharLookupByDesc = [] {
		// An array will take less space than a `std::[unordered_]map`.
		std::array<std::int8_t, 512> a{};
		a.fill(-1);
		for (const auto entry : kBorderCharTable)
		{
			assert(unsigned(entry.c - kBorderCharBase) <= (unsigned)std::numeric_limits<std::int8_t>::max());
			a[entry.desc] = std::int8_t(entry.c - kBorderCharBase);
		}
		return a;
	}();

	constexpr auto kBorderCharLookupByChar = [] {
		std::array<BorderCharDesc, 128> a{};
		for (const auto entry : kBorderCharTable)
			a[entry.c - kBorderCharBase] = entry.desc;
		return a;
	}();

	constexpr wchar_t getCharByBorderCharDescOr(BorderCharDesc desc, wchar_t def)
	{
		const int c = kBorderCharLookupByDesc[desc];
		return c >= 0 ? wchar_t(c + kBorderCharBase) : def;
	}

	constexpr BorderCharDesc getBorderCharDesc(const Pixel& pixel)
	{
		// Only merge if both cells have autoMergeBorder.
		if (pixel.autoMergeBorder && size_t(pixel.c - kBorderCharBase) < kBorderCharLookupByChar.size())
			return kBorderCharLookupByChar[pixel.c - kBorderCharBase];
		else
			return {};
	}

	static BorderCharDesc mergeDescHoriz(BorderCharDesc L, BorderCharDesc X, BorderCharDesc R)
	{
		if (X.left == 0)
			X.left = L.right;
		if (X.right == 0)
			X.right = R.left;
		return X;
	}

	static BorderCharDesc mergeDescVert(BorderCharDesc U, BorderCharDesc X, BorderCharDesc D)
	{
		if (X.up == 0)
			X.up = U.down;
		if (X.down == 0)
			X.down = D.up;
		return X;
	}

	static wchar_t pickBorderChar(BorderCharDesc X, wchar_t def)
	{
		const int numEdges = bool(X.left) + bool(X.right) + bool(X.up) + bool(X.down);

		// Canonicalise.
		if (numEdges != 2)
			X.curved_broken = false;

		if (numEdges == 0)
			return L' ';

		if (const wchar_t c = getCharByBorderCharDescOr(X, {}))
			return c;

		{
			// Convert to thick to thin.
			BorderCharDesc y = X;
			if (y.left == 2)
				y.left = 1;
			if (y.right == 2)
				y.right = 1;
			if (y.up == 2)
				y.up = 1;
			if (y.down == 2)
				y.down = 1;
			if (const wchar_t c = getCharByBorderCharDescOr(y, {}))
				return c;
		}

		const int style = std::min<unsigned int>({ X.left - 1u, X.right - 1u, X.up - 1u, X.down - 1u });

		X.left = X.left ? style : 0;
		X.right = X.right ? style : 0;
		X.up = X.up ? style : 0;
		X.down = X.down ? style : 0;
		if (const wchar_t c = getCharByBorderCharDescOr(X, {}))
			return c;
		
		X.curved_broken = false;

		return getCharByBorderCharDescOr(X, def);
	}

	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescTL{ 0, d, d, 0, special };
	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescTR{ 0, 0, d, d, special };
	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescBR{ d, 0, 0, d, special };
	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescBL{ d, d, 0, 0, special };
	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescHoriz{ 0, d, 0, d, special };
	template<int d, bool special = false> constexpr BorderCharDesc kBorderDescVert{ d, 0, d, 0, special };

	constexpr std::array kBorderCharTL{
		L' ',
		getCharByBorderCharDescOr(kBorderDescTL<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTL<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTL<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTL<1, true>, L'#'),
		L' ',
	};

	constexpr std::array kBorderCharTR{
		L' ',
		getCharByBorderCharDescOr(kBorderDescTR<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTR<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTR<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescTR<1, true>, L'#'),
		L' ',
	};

	constexpr std::array kBorderCharBR{
		L' ',
		getCharByBorderCharDescOr(kBorderDescBR<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBR<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBR<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBR<1, true>, L'#'),
		L' ',
	};

	constexpr std::array kBorderCharBL{
		L' ',
		getCharByBorderCharDescOr(kBorderDescBL<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBL<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBL<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescBL<1, true>, L'#'),
		L' ',
	};

	constexpr std::array kBorderCharHoriz{
		L' ',
		getCharByBorderCharDescOr(kBorderDescHoriz<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescHoriz<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescHoriz<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescHoriz<1>, L'#'),
		L' ',
	};

	constexpr std::array kBorderCharVert{
		L' ',
		getCharByBorderCharDescOr(kBorderDescVert<1>, L'#'),
		getCharByBorderCharDescOr(kBorderDescVert<2>, L'#'),
		getCharByBorderCharDescOr(kBorderDescVert<3>, L'#'),
		getCharByBorderCharDescOr(kBorderDescVert<1>, L'#'),
		L' ',
	};
}

void Framebuffer::drawHLine(ivec2 coord, int length, Pixel pixel)
{
	if (length)
	{
		if (length < 0)
		{
			coord.x += length;
			length = -length;
		}
		if (coord.x < 0)
		{
			length += coord.x;
			coord.x = 0;
		}
		if (coord.x >= mStencil.max.x)
			return;
		if (length > mStencil.max.x - coord.x)
			length = mStencil.max.x - coord.x;
		if (length < 0)
			return;
		for (const int i : range(length))
			draw({ coord.x + i, coord.y }, pixel);
	}
}
void Framebuffer::drawVLine(ivec2 coord, int length, Pixel pixel)
{
	if (length)
	{
		if (length < 0)
		{
			coord.y += length;
			length = -length;
		}
		if (coord.y < 0)
		{
			length += coord.y;
			coord.y = 0;
		}
		if (coord.y >= mStencil.max.y)
			return;
		if (length > mStencil.max.y - coord.y)
			length = mStencil.max.y - coord.y;
		if (length < 0)
			return;
		for (const int i : range(length))
			draw({ coord.x, coord.y + i }, pixel);
	}
}

void Framebuffer::drawHLine(ivec2 coord, int length, EBorderStyle style, PixelColouring colouring, bool autoMerge)
{
	drawHLine(coord, length, {
		.c = kBorderCharHoriz[int(style)],
		.colour = colouring,
		.autoMergeBorder = autoMerge,
		});
}
void Framebuffer::drawHLineColouring(ivec2 coord, int length, PixelColouring colouring)
{
	if (length)
	{
		if (length < 0)
		{
			coord.x += length;
			length = -length;
		}
		if (coord.x < 0)
		{
			length += coord.x;
			coord.x = 0;
		}
		if (coord.x >= mStencil.max.x)
			return;
		if (length > mStencil.max.x - coord.x)
			length = mStencil.max.x - coord.x;
		if (length < 0)
			return;
		for (const int i : range(length))
			recolour({ coord.x + i, coord.y }, colouring);
	}
}
void Framebuffer::drawVLine(ivec2 coord, int length, EBorderStyle style, PixelColouring colouring, bool autoMerge)
{
	drawVLine(coord, length, {
		.c = kBorderCharVert[int(style)],
		.colour = colouring,
		.autoMergeBorder = autoMerge,
		});
}
void Framebuffer::drawBox(iaabb2 rect, EBorderStyle style, PixelColouring colouring)
{
	const ivec2 size = rect.size();

	if (rect.min.x >= rect.max.x)
		return drawVLine(rect.min, size.y, style, colouring);
	if (rect.min.y >= rect.max.y)
		return drawHLine(rect.min, size.x, style, colouring);
	//if (style == EBorderStyle::None)
	//	return;

	drawHLine(rect.min + ivec2(1, 0), size.x - 2, style, colouring);
	drawHLine(ivec2(rect.min.x + 1, rect.max.y - 1), size.x - 2, style, colouring);

	drawVLine(rect.min + ivec2(0, 1), size.y - 2, style, colouring);
	drawVLine(ivec2(rect.max.x - 1, rect.min.y + 1), size.y - 2, style, colouring);

	Pixel pixel{
		.c = getBorderChar(style, { -1, -1 }),
		.colour = colouring,
		.autoMergeBorder = true,
	};

	draw(rect.min, pixel);
	pixel.c = getBorderChar(style, { 1, -1 });
	draw(ivec2(rect.max.x - 1, rect.min.y), pixel);
	pixel.c = getBorderChar(style, { -1, 1 });
	draw(ivec2(rect.min.x, rect.max.y - 1), pixel);
	pixel.c = getBorderChar(style, { 1, 1 });
	draw(ivec2(rect.max.x - 1, rect.max.y - 1), pixel);

	if (style == EBorderStyle::Bracketed)
	{
		draw(rect.min + ivec2(0, rect.size().y / 2), { .c = L'[', .colour = colouring });
		draw(ivec2(rect.max.x - 1, rect.min.y + rect.size().y / 2), { .c = L']', .colour = colouring });
	}
}

wchar_t Framebuffer::getBorderChar(EBorderStyle style, ivec2 dir)
{
	if (dir.x == 0 && dir.y == 0)
		return L' ';
	if (dir.y == 0)
		return kBorderCharVert[int(style)];
	if (dir.x == 0)
		return kBorderCharHoriz[int(style)];

	if (dir.y < 0)
		return dir.x < 0 ? kBorderCharTL[int(style)] : kBorderCharTR[int(style)];
	else
		return dir.x < 0 ? kBorderCharBL[int(style)] : kBorderCharBR[int(style)];
}

void Framebuffer::drawFilledBox(iaabb2 rect, EBorderStyle style, PixelColouring colouring)
{
	if (rect.size().x <= 0 || rect.size().y <= 0)
		return;

	drawBox(rect, style, colouring);

	if (rect.size().y >= 3 && rect.size().x >= 3)
		for (const int y : range(rect.min.y + 1, rect.max.y - 1))
			drawHLine({ rect.min.x + 1, y }, rect.size().x - 2, EBorderStyle::None, colouring);
}

void Framebuffer::drawFilledBox(iaabb2 rect, const Pixel& pixel)
{
	if (rect.size().x <= 0 || rect.size().y <= 0)
		return;

	for (const int y : range(rect.min.y, rect.max.y))
		drawHLine({ rect.min.x, y }, rect.size().x, pixel);
}

void Framebuffer::drawFilledBoxColouring(iaabb2 rect, PixelColouring colouring)
{
	if (rect.size().x <= 0 || rect.size().y <= 0)
		return;

	for (const int y : range(rect.min.y, rect.max.y))
		drawHLineColouring({ rect.min.x, y }, rect.size().x, colouring);
}

void Framebuffer::drawText(std::wstring_view str, const iaabb2& rect, EAlign halign, EAlign valign, PixelColouring colouring, bool wrap)
{
	const ivec2 textSize = wrap ? calcWrappedTextSizeForWidth(str, rect.size().x) : calcMultilineTextSize(str);

	int y = rect.min.y + alignShift(rect.size().y - textSize.y, valign);
	while (str.size())
	{
		const auto line = wrap ? consumeWrappedTextLine(str, rect.size().x) : consumeMultilineTextLine(str);

		int x = rect.min.x + alignShift(int(rect.size().x - line.size()), halign);

		for (const size_t i : range(line.size()))
		{
			draw(ivec2(x + int(i), y), Pixel{
				.c = line[i],
				.colour = colouring,
				});
		}

		++y;
	}
}

void Framebuffer::drawUnderline(ivec2 coord, int length)
{
	for (const int i : range(length))
		if (mStencil.contains(coord + ivec2{ i, 0 }))
			mPixels[coord.x + i + coord.y * mDim.x].underline = true;
}

void Framebuffer::blit(ivec2 coord, std::span<const Pixel> src)
{
	for (const auto& [x, pixel] : std::views::enumerate(src))
		draw(coord + ivec2{ int(x), 0 }, pixel);
}

void Framebuffer::blit(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign, bool wrap)
{
	if (!wrap)
	{
		ivec2 coord = rect.min;
		coord.x += alignShift(int(rect.size().x - src.size()), halign);
		coord.y += alignShift(int(rect.size().y - 1), valign);
		blit(coord, src);
	}
	else
	{
		// Alright. Wrapping rich text.
		// Simply extract the text and use the string views obtained from wrapping to access the attributes of the original text.
		const std::wstring text = std::ranges::to<std::wstring>(src | std::views::transform(&Pixel::c));
		std::wstring_view str = text;

		const ivec2 textSize = calcWrappedTextSizeForWidth(str, rect.size().x);

		int y = rect.min.y + alignShift(rect.size().y - textSize.y, valign);
		while (str.size())
		{
			const auto line = wrap ? consumeWrappedTextLine(str, rect.size().x) : consumeMultilineTextLine(str);

			const int x = rect.min.x + alignShift(int(rect.size().x - line.size()), halign);

			blit({ x, y }, src.subspan(line.data() - text.data(), line.size()));

			++y;
		}
	}
}

static Pixel recolourPixel(Pixel pixel, PixelColouring defColouring)
{
	if (pixel.colour.text.transparent)
		pixel.colour.text = defColouring.text;
	if (pixel.colour.back.transparent)
		pixel.colour.back = defColouring.back;
	return pixel;
}

void Framebuffer::blitWithDefaultColouring(ivec2 coord, std::span<const Pixel> src, PixelColouring defColouring)
{
	for (const auto& [x, pixel] : std::views::enumerate(src))
		draw(coord + ivec2{ int(x), 0 }, recolourPixel(pixel, defColouring));
}

void Framebuffer::blitWithDefaultColouring(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign, bool wrap, PixelColouring defColouring)
{
	// TODO: Deduplicate this code
	if (!wrap)
	{
		ivec2 coord = rect.min;
		coord.x += alignShift(int(rect.size().x - src.size()), halign);
		coord.y += alignShift(int(rect.size().y - 1), valign);
		blitWithDefaultColouring(coord, src, defColouring);
	}
	else
	{
		// Alright. Wrapping rich text.
		// Simply extract the text and use the string views obtained from wrapping to access the attributes of the original text.
		const std::wstring text = std::ranges::to<std::wstring>(src | std::views::transform(&Pixel::c));
		std::wstring_view str = text;

		const ivec2 textSize = calcWrappedTextSizeForWidth(str, rect.size().x);

		int y = rect.min.y + alignShift(rect.size().y - textSize.y, valign);
		while (str.size())
		{
			const auto line = wrap ? consumeWrappedTextLine(str, rect.size().x) : consumeMultilineTextLine(str);

			const int x = rect.min.x + alignShift(int(rect.size().x - line.size()), halign);

			blitWithDefaultColouring({ x, y }, src.subspan(line.data() - text.data(), line.size()), defColouring);

			++y;
		}
	}
}

void Framebuffer::blitAutoContrastText(iaabb2 rect, std::span<const Pixel> src, EAlign halign, EAlign valign)
{
	ivec2 coord = rect.min;
	coord.x += alignShift(int(rect.size().x - src.size()), halign);
	coord.y += alignShift(int(rect.size().y - 1), valign);
	//blit(coord, src);

	EColour prevBackColour = EColour::Black;
	bool prevIsLight = false;

	for (const auto& [x, pixel] : std::views::enumerate(src))
	{
		const ivec2 thisCoord = coord + ivec2{ int(x), 0 };
		draw(thisCoord, pixel);
		if (pixel.colour.text.transparent && mStencil.contains(thisCoord))
		{
			Pixel dst = getPixel(thisCoord);
			bool isLight;
			//if (dst.colour.back.code < EColour(8))
			//	isLight = false;
			//else if (dst.colour.back.code < EColour(16))
			//	isLight = true;
			//else if (dst.colour.back.code < EColour::Grey3)
			if (prevBackColour != dst.colour.back.code)
			{
				const auto [r, g, b] = toRGB8(dst.colour.back.code);

				//const int rc = 10;
				//// Need a high value for the research bar.
				//const int gc = 50;
				//// Want white text on culture bar and black text on GPP rate.
				//const int bc = 5;
				//
				//const int threshold = 128; // 6 / 2
				//
				//isLight = r * rc + g * gc + b * bc >= threshold * (rc + gc + bc);

				// Okay, let's just do the proper linear check.
				const float linearR = std::pow(r / 255.0f, 2.2f);
				const float linearG = std::pow(g / 255.0f, 2.2f);
				const float linearB = std::pow(b / 255.0f, 2.2f);
				const float linearL = 0.2126f * linearR + 0.7152f * linearG + 0.0722f * linearB;
				isLight = linearL > 0.5f;
				prevBackColour = dst.colour.back.code;
				prevIsLight = isLight;
			}
			else
				isLight = prevIsLight;
			//else
			//	isLight = dst.colour.back.code >= EColour::Grey50;
			dst.colour.text = isLight ? EColour::Black : EColour::White;
			draw(thisCoord, dst);
		}
	}
}

void Framebuffer::mergeBorders()
{
	const int w = mDim.x;
	const int h = mDim.y;
	for (const int y : range(h))
	{
		for (const int x : range(w))
		{
			Pixel& pixel = mPixels[x + y * mDim.x];
			if (pixel.autoMergeBorder && kBorderCharBase <= pixel.c && pixel.c < static_cast<wchar_t>(kBorderCharBase + kBorderCharLookupByChar.size()))
			{
				const BorderCharDesc X = getBorderCharDesc(pixel);
				const BorderCharDesc L = x > 0 ? getBorderCharDesc(mPixels[x - 1 + y * mDim.x]) : BorderCharDesc();
				const BorderCharDesc R = x + 1 < w ? getBorderCharDesc(mPixels[x + 1 + y * mDim.x]) : BorderCharDesc();
				const BorderCharDesc U = y > 0 ? getBorderCharDesc(mPixels[x + (y - 1) * mDim.x]) : BorderCharDesc();
				const BorderCharDesc D = y + 1 < h ? getBorderCharDesc(mPixels[x + (y + 1) * mDim.x]) : BorderCharDesc();
				const BorderCharDesc LR = mergeDescHoriz(L, X, R);
				const BorderCharDesc UD = mergeDescVert(U, LR, D);
				pixel.c = pickBorderChar(UD, pixel.c);
			}
		}
	}
}

InputCaret Framebuffer::getInputCaret() const
{
	return mInputCaret;
}
const Pixel& Framebuffer::getPixel(ivec2 coord) const
{
	return mPixels[coord.x + coord.y * mDim.x];
}
Pixel& Framebuffer::getPixel(ivec2 coord)
{
	return mPixels[coord.x + coord.y * mDim.x];
}