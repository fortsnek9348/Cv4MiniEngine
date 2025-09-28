#include "inc/HeckTextUI/BasicControls.h"

using namespace hecktui;

HorizontalRule::HorizontalRule(EBorderStyle style, PixelColouring colouring) : BoxPanel(style, colouring)
{
	enableTopBorder = true;
	enableBottomBorder = false;
	enableLeftBorder = false;
	enableRightBorder = false;
}

VerticalRule::VerticalRule(EBorderStyle style, PixelColouring colouring) : BoxPanel(style, colouring)
{
	enableTopBorder = false;
	enableBottomBorder = false;
	enableLeftBorder = true;
	enableRightBorder = false;
}

BoxPanel::BoxPanel(EBorderStyle style, PixelColouring colouring) : style(style), borderColouring(colouring)
{
}

LayoutSizeInfo BoxPanel::measureThis() const noexcept
{
	const ivec2 v{ enableLeftBorder + enableRightBorder, enableTopBorder + enableBottomBorder };
	return { v, v };
}
void BoxPanel::drawThis(ivec2 offset, Framebuffer& fb)
{
	const ivec2 size = getRect().size();
	const ivec2 position = offset + getRect().min;

	fb.drawFilledBox(offset + getRect(), { .c = L' ', .colour = interiorColouring });

	const ivec2 posTL = position;
	const ivec2 posTR = position + ivec2(size.x - 1, 0);
	const ivec2 posBL = position + ivec2(0, size.y - 1);
	const ivec2 posBR = position + size - ivec2(1, 1);
	if (enableLeftBorder)
		fb.drawVLine(posTL, size.y, style, borderColouring, enableAutoMergeCardinal);
	if (enableRightBorder)
		fb.drawVLine(posTR, size.y, style, borderColouring, enableAutoMergeCardinal);
	if (enableTopBorder)
		fb.drawHLine(posTL, size.x, style, borderColouring, enableAutoMergeCardinal);
	if (enableBottomBorder)
		fb.drawHLine(posBL, size.x, style, borderColouring, enableAutoMergeCardinal);

	if (enableLeftBorder && enableTopBorder)
		fb.draw(posTL, { .c = Framebuffer::getBorderChar(style, { -1, -1 }), .colour = borderColouring, .autoMergeBorder = false });
	if (enableLeftBorder && enableBottomBorder)
		fb.draw(posBL, { .c = Framebuffer::getBorderChar(style, { -1, +1 }), .colour = borderColouring, .autoMergeBorder = false });
	if (enableRightBorder && enableTopBorder)
		fb.draw(posTR, { .c = Framebuffer::getBorderChar(style, { +1, -1 }), .colour = borderColouring, .autoMergeBorder = false });
	if (enableRightBorder && enableBottomBorder)
		fb.draw(posBR, { .c = Framebuffer::getBorderChar(style, { +1, +1 }), .colour = borderColouring, .autoMergeBorder = false });
}