#pragma once

#include <HeckTextUI/Core.h>

// libstdc++ doesn't support this, but I suppose it isn't all that elegant anyway.
//#include <mdspan>
#include <vector>
#include <string_view>
#include <algorithm>
#include <filesystem>

template<class T>
struct SpriteSpan
{
	std::span<T> linear{};
	heck::ivec2 dim{};
	size_t pitch = dim.x;

	SpriteSpan() = default;

	SpriteSpan(std::span<T> linear, heck::ivec2 dim) : linear(linear), dim(dim)
	{
		if (dim.x != 0 && dim.y != 0)
			assert(static_cast<size_t>(dim.x - 1) + static_cast<size_t>(dim.y - 1) * pitch < linear.size());
	}

	T operator[](heck::ivec2 coord) const
	{
		assert(static_cast<unsigned int>(coord.x) < static_cast<unsigned int>(dim.x) && static_cast<unsigned int>(coord.y) < static_cast<unsigned int>(dim.y));
		return linear[coord.x + coord.y * pitch];
	}

	T& operator[](heck::ivec2 coord)
	{
		assert(static_cast<unsigned int>(coord.x) < static_cast<unsigned int>(dim.x) && static_cast<unsigned int>(coord.y) < static_cast<unsigned int>(dim.y));
		return linear[coord.x + coord.y * pitch];
	}
};

class WorldViewFramebuffer
{
public:
	using EColour = hecktui::EColour;
	using enum EColour;

	struct Cell
	{
		wchar_t c{};
		EColour textColour = White;
		EColour backColour = Black;
		bool underline : 1 = false;
		bool ignoreTextColour : 1 = false;
		bool ignoreBackColour : 1 = false;
		bool ignoreChar : 1 = false;

		friend bool operator==(Cell, Cell) = default;
	};

	enum class EHAlign
	{
		Left, Center, Right
	};

	using ConstMDSpan = SpriteSpan<const Cell>;
	using MDSpan = SpriteSpan<Cell>;

	WorldViewFramebuffer() = default;
	explicit WorldViewFramebuffer(heck::ivec2 dim);

	void drawCell(heck::ivec2 coord, Cell cell) noexcept;
	void drawBox(heck::ivec2 coord, heck::ivec2 size, Cell cell) noexcept;
	void drawHLine(heck::ivec2 a, int n, Cell cell) noexcept;
	void drawVLine(heck::ivec2 a, int n, Cell cell) noexcept;
	void drawStippleLine(heck::ivec2 a, heck::ivec2 b, Cell cell, std::string_view pattern) noexcept;
	void drawStippleEllipse(const hecktui::iaabb2& rect, Cell cell, std::string_view pattern) noexcept;
	void drawFilledEllipse(const hecktui::iaabb2& rect, Cell cell) noexcept;
	void drawSprite(heck::ivec2 coord, ConstMDSpan cells) noexcept;
	void drawSpriteWithColourRemap(heck::ivec2 coord, ConstMDSpan cells, std::span<const EColour> colours) noexcept;
	void fill(heck::ivec2 min, heck::ivec2 max, Cell cell) noexcept;
	void fillPattern(heck::ivec2 min, heck::ivec2 max, ConstMDSpan cells) noexcept;
	void drawCenteredSprite(heck::ivec2 centerCoord, ConstMDSpan cells) noexcept;
	//void drawBox(heck::ivec2 coord, heck::ivec2 dim, ELineStyle style, int z) noexcept;
	
	//void drawAxisAlignedLine(heck::ivec2 a, heck::ivec2 b, ELineStyle style, int z) noexcept;
	//void drawLine(heck::ivec2 a, heck::ivec2 b, LineStyle style) noexcept;
	void drawText(heck::ivec2 a, int alignWidth, std::wstring_view str, Cell attribs, EHAlign halign) noexcept;
	void drawText(heck::ivec2 a, int alignWidth, std::span<const Cell> str, EHAlign halign) noexcept;

	const Cell& operator[](heck::ivec2 coord) const noexcept;

	ConstMDSpan getCells() const noexcept;
	MDSpan getCells() noexcept;

	void saveAsPPM(const std::filesystem::path& path) const;

private:
	heck::ivec2 mDim{};
	std::vector<Cell> mCells;

	struct Internals;
};

template<class T, int kW, int kH>
struct GenericSprite
{
	std::array<T, kW * kH> cells{};

	GenericSprite() = default;
#ifndef __INTELLISENSE__
	constexpr explicit GenericSprite(std::span<const T, kW* kH + 1> s) : cells{} // +1 for string literal
	{
		std::ranges::copy(s.template subspan<0, kW * kH>(), cells.begin());
	}
#else
	constexpr explicit GenericSprite(std::span<const T, kW* kH + 1> s) : cells{}
	{
	}
#endif


	constexpr operator SpriteSpan<const T>() const noexcept
	{
		return SpriteSpan<const T>(cells, { kW, kH });
	}

	template<class F>
	constexpr GenericSprite<std::invoke_result_t<F, T>, kW, kH> operator|(F f) const
	{
		GenericSprite<std::invoke_result_t<F, T>, kW, kH> b{};
#ifndef __INTELLISENSE__
		std::transform(cells.begin(), cells.end(), b.cells.begin(), f);
#endif
		return b;
	}
};

template<int kW, int kH>
using SpriteTemplate = GenericSprite<char, kW, kH>;

template<int kW, int kH>
using GfxSprite = GenericSprite<WorldViewFramebuffer::Cell, kW, kH>;

struct CellMapping
{
	char c{};
	WorldViewFramebuffer::Cell cell{};

	constexpr WorldViewFramebuffer::Cell operator()(char pattern) const
	{
		return (*this)({ .c = (wchar_t)(unsigned char)pattern });
	}

	constexpr WorldViewFramebuffer::Cell operator()(WorldViewFramebuffer::Cell pattern) const
	{
		return pattern.c == c ? cell : pattern;
	}
};


