#include "inc/HeckTextUI/Slider.h"

#include <algorithm>

using namespace hecktui;

Slider::Slider(int max) : mMax(max)
{
	mCanFocus = true;
	mIsMouseInteractable = true;
}

bool Slider::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		Slider& self;

		bool operator()(const MouseButtonEvent& e)
		{
			if (e.button == EMouseButton::Left)
				self.mIsClickedOnThumb = e.type == EConsoleEventType::MouseButtonDown && e.coord.x == self.getThumbX();
			return true;
		}

		bool operator()(const MouseMoveEvent& e)
		{
			if (self.getUISceneState().capture && self.mIsClickedOnThumb)
			{
				const int denom = self.getRect().size().x - 2;
				if (denom >= 1)
				{
					const int x = std::clamp(e.coord.x, 1, denom + 1);
					self.mValue = (self.mMax * (x - 1) + denom / 2) / denom;
					self.onSliderValueChanged();
				}
			}
			return false;
		}

		bool operator()(const ConsoleEvent&)
		{
			return false;
		}
	};

	return dispatch(e, Handler{ *this });
}

void Slider::onSliderValueChanged()
{
}

int Slider::getValue() const
{
	return mValue;
}

void Slider::setValue(int x)
{
	mValue = x;
}

LayoutSizeInfo Slider::measureThis() const
{
	const ivec2 size{ 4, 1 };
	return { size, size };
}

void Slider::drawThis(ivec2 offset, Framebuffer& fb)
{
	const iaabb2 rect = offset + getRect();
	fb.drawHLine(rect.min, rect.size().x, { .c = L'─' });
	fb.draw(rect.min, { .c = L'├' });
	fb.draw({ rect.max.x - 1, rect.min.y }, { .c = L'┤' });
	fb.draw(rect.min + ivec2(getThumbX(), 0), { .c = L'█' });
}

int Slider::getThumbX() const
{
	// [0..max] -> [1..width-1]
	return 1 + (mValue * (getRect().size().x - 1 - 1) + mMax / 2) / mMax;
}