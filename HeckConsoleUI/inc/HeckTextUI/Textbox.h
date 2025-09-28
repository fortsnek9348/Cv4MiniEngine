#pragma once

#include "Control.h"

#include <string>
#include <functional>

namespace hecktui
{
	class HECKCONSOLEUI_EXPORT Textbox : public Element
	{
	public:
		explicit Textbox(
			std::wstring placeholder,
			std::wstring text,
			Colour backColour,
			Colour placeholderColour,
			Colour textColour
		);

		explicit Textbox(std::wstring text);

		virtual bool onEvent(const ConsoleEvent& e);

		virtual void update();

		std::wstring_view getText() const noexcept;
		void setText(std::wstring);

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept;
		virtual void drawThis(ivec2 offset, Framebuffer& fb);

	private:
		Colour mBackColour{};
		Colour mPlaceholderColour{};
		Colour mTextColour{};
		std::wstring mPlaceholder;
		std::wstring mText;
		size_t mScrollPosition = 0;
		size_t mEditPosition = 0;
	};
}