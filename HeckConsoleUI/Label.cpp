#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"

#include <algorithm>

using namespace hecktui;

Label::Label(std::wstring label) : mLabel(std::move(label))
{
}

std::wstring_view Label::getLabel() const noexcept
{
	return mLabel;
}
void Label::setLabel(std::wstring label)
{
	mLabel = std::move(label);
}

void Label::setLabelAlignment(EAlign halign)
{
	mHAlign = halign;
}

//bool Label::hasSizeAxisDependence() const noexcept
//{
//	return enableWrapping || Element::hasSizeAxisDependence();
//}
//int Label::calcHeightFromWidth(int w) const
//{
//	return enableWrapping ? calcTextHeightForWidth(mLabel, w) : Element::calcHeightFromWidth(w);
//}

std::optional<EAxis> Label::getThisWrappingAxis() const noexcept
{
	if (enableWrapping)
		return EAxis::Horizontal;
	else
		return Element::getThisWrappingAxis();
}

bool Label::layoutPrepareIteration(bool initial) noexcept
{
	bool iterationNeeded = Element::layoutPrepareIteration(initial);
	if (enableWrapping)
	{
		if (initial)
			mLayoutWrappingWidth = INT_MAX;
		else
		{
			if (getRect().size().x < mLayoutWrappingWidth)
			{
				mLayoutWrappingWidth = getRect().size().x;
				iterationNeeded = true;
			}
		}
	}
	return iterationNeeded;
}

LayoutSizeInfo Label::measureThis() const noexcept
{
	if (enableWrapping)
	{
		const ivec2 size = calcWrappedTextSizeForWidth(mLabel, mLayoutWrappingWidth);
		return {
			.minimum = size, //calcWrappedTextMinSize(mLabel),
			.preferred = size,
		};
	}
	else
	{
		const ivec2 size = calcMultilineTextSize(mLabel);
		return {
			.minimum = size,
			.preferred = size,
		};
	}
}


void Label::drawThis(ivec2 offset, Framebuffer& fb)
{
	fb.drawText(mLabel, offset + getRect(), mHAlign, EAlign::Middle, colouring, enableWrapping);
}
