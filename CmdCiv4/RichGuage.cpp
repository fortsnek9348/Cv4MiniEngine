#include "RichGuage.h"
#include "TuiTextCode.h"

#include <ranges>

using namespace hecktui;
using namespace cvengine;

// TODO: Guages could use half-width blocks for double the resolution, but that's totally incompatible with overlaid labels.

RichGuage::RichGuage(std::vector<hecktui::EColour> colours, CvWidgetDataStruct widgetData)
	: colours(std::move(colours))
	, actionButton(makeActionButtonWithManualLabel(L"", widgetData, {}))
{
	mCanFocus = false;
	mIsMouseInteractable = true;
	actionButton->setBorderStyle(EBorderStyle::None);
	addChild(actionButton);
	auto layout = std::make_unique<FillLayout>();
	layout->marginTopLeft = { 1, 0 };
	layout->marginBottomRight = { 1, 0 };
	setLayout(std::move(layout));
}

LayoutSizeInfo RichGuage::measureThis() const noexcept
{
	const ivec2 v{ 4, 1 };
	return { v, v };
}

void RichGuage::drawBareGuage(hecktui::Framebuffer& fb, const hecktui::iaabb2& rect,
	std::span<const hecktui::EColour> colours,
	std::span<const int> ratios,
	int max
)
{
	const int width = rect.size().x;

	int accRatio = 0;
	int lastX = 0;
	EColour prevColour{};

	for (const auto& [givenColour, ratio] : std::views::zip(colours, ratios))
	{
		//EColour colour = toTuiColour(colourCode);
		// Darken if it's the same colour as before.
		// Minor hack for most bars.
		EColour colour = givenColour;
		if (colour == prevColour)
			colour = cvengine::darken(colour);
		prevColour = colour;

		const int x = int((int64_t(accRatio + ratio) * width + max / 2) / max);
		fb.drawHLine(rect.min + ivec2(lastX, 0), x - lastX, Pixel{ .c = L' ', .colour{.back = colour } });

		accRatio += ratio;
		lastX = x;
	}
}

void RichGuage::drawThis(ivec2 offset, hecktui::Framebuffer& fb)
{
	iaabb2 rect = offset + getRect();

	fb.drawText(L"[", rect, EAlign::Left, EAlign::Center, {}, false);
	rect = rect.shrunk({ 1, 0 });
	drawBareGuage(fb, rect, colours, ratios, max);
	rect = rect.shrunk({ -1, 0 });
	fb.drawText(L"]", rect, EAlign::Right, EAlign::Center, {}, false);
}
