#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"

#include <algorithm>

using namespace hecktui;

Checkbox::Checkbox(ECheckStyle style, std::wstring label, std::function<void(bool)> onCheckChangedCallback)
	: mStyle(style), mLabel(std::move(label)), mOnCheckChangedCallback(std::move(onCheckChangedCallback))
{
	mCanFocus = true;
	mIsMouseInteractable = true;
}

RadioButton::RadioButton(ECheckStyle style, std::wstring label, std::function<void(bool)> onCheckChangedCallback)
	: Checkbox(style, std::move(label), std::move(onCheckChangedCallback))
{
	setIsRadioButton(true);
}

bool Checkbox::onEvent(const ConsoleEvent& e)
{
	switch (e.type)
	{
		//case EConsoleEventType::MouseCheckboxDown:
		//	if (static_cast<const MouseCheckboxEvent&>(e).Checkbox == EMouseCheckbox::Left)
		//		mMouseCaptured = true;
		//	return true;
		//case EConsoleEventType::MouseMove:
		//	//mMouseOver = iaabb2{ .max = getRect().size() }.contains(static_cast<const MouseMoveEvent&>(e).coord);
		//	return true;
	case EConsoleEventType::MouseButtonUp:
	{
		if (static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Left)
		{
			const UISceneState state = getUISceneState();
			if (state.capture && state.hover)
			{
				const bool newValue = mIsRadioButton ? true : !value;
				if (newValue != value)
				{
					value = newValue;
					onCheckChanged();
				}
			}
		}
		return true;
	}
	default:
		return false;
	}
}

void Checkbox::update()
{
	Element::update();
}

void Checkbox::onCheckChanged()
{
	if (mOnCheckChangedCallback)
		mOnCheckChangedCallback(value);
}

void Checkbox::setLabel(std::wstring label)
{
	std::swap(mLabel, label);
}

std::wstring_view Checkbox::getLabel() const noexcept
{
	return mLabel;
}

void Checkbox::setColouring(PixelColouring colouring)
{
	mDefaultColouring = colouring;
}

void Checkbox::setIsRadioButton(bool enable)
{
	mIsRadioButton = enable;
}

//bool Checkbox::onMouseOver()
//{
//	mMouseOver = true;
//	return true;
//}
//bool Checkbox::onMouseOut()
//{
//	mMouseOver = false;
//	return true;
//}

static constexpr std::wstring_view kCheckChars[][2]{
	{ L"", L"" },
	{ L"[ ]", L"[X]" },
	{ L" ", L"√" },
	{ L"□", L"√" },
	// { L'☐', L'☑' },
	{ L" ", L"•" },
	{ L"○", L"●" },
	// ■
};

LayoutSizeInfo Checkbox::measureThis() const noexcept
{
	ivec2 minSize((int)mLabel.size(), 1);
	if (mStyle == Border)
		minSize += ivec2(2);
	else
		minSize.x += (int)kCheckChars[(int)mStyle][0].size() + 1;
	return { minSize, minSize };
}

void Checkbox::drawThis(ivec2 offset, Framebuffer& fb)
{
	const UISceneState state = getUISceneState();

	PixelColouring colouring = mDefaultColouring;

	if (state.hover)
		colouring.text = EColour::Grey100;

	//PixelColouring textColouring = colouring;

	//if (state.capture && state.hover)
	//	std::swap(colouring.text, colouring.back);

	const iaabb2 rect = offset + getRect();

	if (mStyle == Border)
	{
		fb.drawBox(rect, value ? EBorderStyle::Double : EBorderStyle::Thin, colouring);
		fb.drawText(mLabel, rect.shrunk(1), EAlign::Center, EAlign::Middle, colouring, false);
	}
	else
	{
		const std::wstring_view checkStr = kCheckChars[(int)mStyle][value];
		fb.drawText(checkStr, rect, EAlign::Left, EAlign::Middle, colouring, false);
		fb.drawText(mLabel, rect + ivec2((int)checkStr.size() + 1, 0), EAlign::Left, EAlign::Middle, colouring, false);
	}
}
