#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"

#include <algorithm>

using namespace hecktui;

EmptyButton::EmptyButton(std::function<void()> onClickCallback) : mOnClickCallback(std::move(onClickCallback))
{
	mCanFocus = true;
	mIsMouseInteractable = true;

	//addChild(std::make_shared<Label>(std::move(label)));
	//getLabelElement().colouring = { .text = kTransparent, .back = kTransparent };
	//getLabelElement().setLabelAlignment(EAlign::Center);
	//auto layout = std::make_shared<FillLayout>();
	//layout->marginBottomRight = layout->marginTopLeft = { 1, 1 };
	//setLayout(layout);

	setLayout(std::make_unique<FillLayout>());
	setBorderStyle(mBorderStyle);
}

Button::Button(std::wstring label, std::function<void()> onClickCallback) : EmptyButton(std::move(onClickCallback))
{
	setLayout(std::make_unique<FillLayout>());
	setLabel(std::move(label));
	setBorderStyle(mBorderStyle);
}

void Button::setLabel(std::wstring label)
{
	if (label.size())
	{
		if (!mLabel || !mLabel->getParent())
		{
			if (!mLabel)
				mLabel = std::make_shared<Label>(std::wstring());
			addChild(mLabel);
		}
		mLabel->setLabel(std::move(label));
		mLabel->colouring = { .text = kTransparent, .back = kTransparent };
		mLabel->setLabelAlignment(EAlign::Center);
		//if (!mLabelLayout)
		//{
		//	mLabelLayout = std::make_unique<FillLayout>();
		//	setBorderStyle(mBorderStyle);
		//}
		//if (mLabelLayout.get() != getLayout())
		//	setLayout(mLabelLayout);
	}
	else
		removeChild(mLabel.get());
}

void Button::setLabelHAlign(EAlign align)
{
	if (mLabel)
		mLabel->setLabelAlignment(align);
}

void EmptyButton::setBorderStyle(EBorderStyle style) noexcept
{
	mBorderStyle = style;
}

void EmptyButton::setLabelColour(Colour colour)
{
	mColouring.text = colour;
}

Colour EmptyButton::getLabelColour() const
{
	return mColouring.text;
}

void EmptyButton::setBackgroundColour(Colour colour)
{
	mColouring.back = colour;
}

void EmptyButton::setOnClickCallback(std::function<void()> onClickCallback)
{
	mOnClickCallback = std::move(onClickCallback);
}

void EmptyButton::setEnableRightClick(bool x)
{
	mRightClickEnabled = x;
}

std::wstring_view Button::getLabel() const noexcept
{
	return getLabelElement().getLabel();
}

void Button::setBorderStyle(EBorderStyle style) noexcept
{
	EmptyButton::setBorderStyle(style);
	if (FillLayout* const layout = static_cast<FillLayout*>(getLayout()))
	{
		if (style == EBorderStyle::None)
			layout->marginTopLeft = layout->marginBottomRight = {};
		else if (style == EBorderStyle::Bracketed)
			layout->marginTopLeft = layout->marginBottomRight = ivec2(1, 0);
		else
			layout->marginTopLeft = layout->marginBottomRight = ivec2(1, 1);
	}
}

Label& Button::getLabelElement() const noexcept
{
	assert(mLabel);
	return *mLabel;
}

bool EmptyButton::onEvent(const ConsoleEvent& e)
{
	switch (e.type)
	{
	//case EConsoleEventType::MouseButtonDown:
	//	if (static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Left)
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
				onClick(static_cast<const MouseButtonEvent&>(e));
				if (mOnClickCallback)
					mOnClickCallback();
			}
		}
		else if (static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Right && mRightClickEnabled)
		{
			const UISceneState state = getUISceneState();
			if (state.capture && state.hover)
				onRightClick(static_cast<const MouseButtonEvent&>(e));
		}
		return true;
	}
	default:
		return false;
	}
}

void EmptyButton::onClick(ModifierKeyState)
{
}

void EmptyButton::onRightClick(ModifierKeyState)
{
}

void EmptyButton::update()
{
	Element::update();
}

//bool EmptyButton::onMouseOver()
//{
//	mMouseOver = true;
//	return true;
//}
//bool EmptyButton::onMouseOut()
//{
//	mMouseOver = false;
//	return true;
//}

LayoutSizeInfo EmptyButton::measureThis() const
{
	const int minW = mBorderStyle == EBorderStyle::None ? 0 : 2;
	const int minH = mBorderStyle == EBorderStyle::None || mBorderStyle == EBorderStyle::Bracketed ? 1 : 3;

	//const ivec2 minSize{
	//	(mBorderStyle == EBorderStyle::None ? 0 : 2) + (int)mLabel.size(),
	//	(mBorderStyle == EBorderStyle::None ? 0 : 2) + 1,
	//};
	//return { minSize, vmax(minSize, ivec2(10, 0)) };
	return { ivec2(minW, minH), ivec2(minW, minH) };
}

PixelColouring EmptyButton::getCurrentPixelColouring() const noexcept
{
	const UISceneState state = getUISceneState();

	const bool hover = state.hover;
	const bool enabled = isHierarchicalEnabled();

	PixelColouring colouring = {
		.text = enabled ? (hover ? EColour::White : mColouring.text) : EColour::Grey,
		.back = mColouring.back,
	};

	if (state.capture && state.hover)
		std::swap(colouring.text, colouring.back);

	//colouring.text = EColour::Green;

	return colouring;
}

void EmptyButton::drawThis(ivec2 offset, Framebuffer& fb)
{
	const PixelColouring colouring = getCurrentPixelColouring();

	const iaabb2 globalRect = offset + getRect();

	fb.drawFilledBox(globalRect, EBorderStyle::None, colouring);
	fb.drawFilledBox(globalRect, mBorderStyle, colouring);
}
