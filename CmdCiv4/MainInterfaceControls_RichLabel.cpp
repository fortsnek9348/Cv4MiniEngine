#include "MainInterfaceControls.h"
#include "TuiTextCode.h"
#include "UITheme.h"

using namespace hecktui;

RichLabel::RichLabel(std::wstring_view label, bool wantMouseInteraction) : Label(L"")
{
	colouring = { theme::kRichLabelDefaultColour, hecktui::EColour::Black };
	setLabel(label);
	mIsMouseInteractable = wantMouseInteraction; // tooltip
}

std::optional<EAxis> RichLabel::getThisWrappingAxis() const noexcept
{
	if (enableWrapping)
		return EAxis::Horizontal;
	else
		return std::nullopt;
}

void RichLabel::setLabel(std::wstring_view label)
{
	pixels = renderCiv4TextCode(label, { .colour{ hecktui::kTransparent, hecktui::kTransparent } });
	maxLineWidth = 0;
	numLines = 0;

	std::span src = pixels;

	while (src.size())
	{
		++numLines;
		const size_t n = std::ranges::find(src, L'\n', &Pixel::c) - src.begin();
		const std::span line = src.subspan(0, n);
		maxLineWidth = std::max(maxLineWidth, (int)line.size());
		src = src.subspan(n + (n < src.size()));
	}

	maxLineWidth += enableExtraSpace;
}

std::shared_ptr<Element> RichLabel::createTooltip() const
{
	return createWidgetTooltip(widgetData);
}

hecktui::LayoutSizeInfo RichLabel::measureThis() const noexcept
{
	if (enableWrapping)
	{
		const auto rawText = stripPixels(pixels);
		hecktui::LayoutSizeInfo info{
			.minimum = calcWrappedTextMinSize(rawText),
			.preferred = calcWrappedTextSizeForWidth(rawText, mLayoutWrappingWidth),
			//.preferred = calcWrappedTextSizeForWidth(rawText, INT_MAX),
		};
		info.minimum.x = std::min(info.minimum.x, measureWidthLimit);
		info.preferred.x = std::min(info.preferred.x, measureWidthLimit);
		return info;
	}
	else
	{
		const ivec2 v = ivec2(std::min(maxLineWidth, measureWidthLimit), numLines);
		return { v, v };
	}
}
void RichLabel::drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb)
{
	std::span src = pixels;

	const iaabb2 rect = offset + getRect();

	iaabb2 remainingRect = rect;

	const hecktui::PixelColouring defColouring = isHierarchicalEnabled() ? colouring : hecktui::PixelColouring{ .text = hecktui::EColour::Grey };

	if (enableWrapping)
		fb.blitWithDefaultColouring(rect, src, mHAlign, EAlign::Top, true, defColouring);
	else
	{
		//mDefPixel.c = L' ';

		while (src.size())
		{
			const size_t n = std::ranges::find(src, L'\n', &Pixel::c) - src.begin();
			const std::span line = src.subspan(0, n);
			if (enableAutoContrast)
				fb.blitAutoContrastText(remainingRect, line, mHAlign, EAlign::Top);
			else
				fb.blitWithDefaultColouring(remainingRect, line, mHAlign, EAlign::Top, false, defColouring);
			if (enableExtraSpace)
				fb.draw({ int(remainingRect.min.x + line.size()), remainingRect.min.y }, { .c = L' ', .colour = defColouring });
			remainingRect.min.y += 1;
			src = src.subspan(n + (n < src.size()));
		}
	}
}