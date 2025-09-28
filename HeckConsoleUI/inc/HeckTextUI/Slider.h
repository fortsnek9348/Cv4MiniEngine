#pragma once

#include "Control.h"

namespace hecktui
{
	class HECKCONSOLEUI_EXPORT Slider : public Element
	{
	public:
		explicit Slider(int max);

		virtual bool onEvent(const ConsoleEvent& e);

		virtual void onSliderValueChanged();

		int getValue() const;
		void setValue(int x);

	protected:
		virtual LayoutSizeInfo measureThis() const;
		virtual void drawThis(ivec2 offset, Framebuffer& fb);

	private:
		int mValue = 0;
		int mMax = 0;

		bool mIsClickedOnThumb = false;

		int getThumbX() const;
	};
}