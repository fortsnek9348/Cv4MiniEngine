#include "inc/HeckTextUI/Textbox.h"

#include <algorithm>
#include <utility>

using namespace hecktui;

Textbox::Textbox(
	std::wstring placeholder,
	std::wstring text,
	Colour backColour,
	Colour placeholderColour,
	Colour textColour
)
	: mBackColour(backColour)
	, mPlaceholderColour(placeholderColour)
	, mTextColour(textColour)
	, mPlaceholder(std::move(placeholder))
	, mText(std::move(text))
	, mEditPosition(mText.size())
{
	mCanFocus = true;
	mIsMouseInteractable = true;
}

Textbox::Textbox(std::wstring text) : Textbox(
	{},
	std::move(text),
	EColour::Black,
	EColour::Grey,
	EColour::Silver
)
{
}

bool Textbox::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		Textbox& self;

		void gotoX(const MouseEvent& e) const
		{
			self.mEditPosition = std::clamp<ptrdiff_t>(e.coord.x +ptrdiff_t(self.mScrollPosition), 0, self.mText.size());
		}

		bool operator()(const MouseMoveEvent& e) const
		{
			if (self.getUISceneState().capture)
				gotoX(e);
			return true;
		}

		bool operator()(const MouseButtonEvent& e) const
		{
			if (e.type == EConsoleEventType::MouseButtonDown)
				gotoX(e);
			return true;
		}

		bool operator()(const ActionKeyEvent& e) const
		{
			switch (e.key)
			{
			case EActionKey::Home:
				self.mEditPosition = 0;
				return true;
			case EActionKey::End:
				self.mEditPosition = self.mText.size();
				return true;
			case EActionKey::Backspace:
				if (self.mEditPosition > 0)
				{
					--self.mEditPosition;
					self.mText.erase(self.mEditPosition, 1);
				}
				return true;
			case EActionKey::Delete:
				if (self.mEditPosition < self.mText.size())
					self.mText.erase(self.mEditPosition, 1);
				return true;
			case EActionKey::Left:
				if (self.mEditPosition > 0)
					--self.mEditPosition;
				return true;
			case EActionKey::Right:
				if (self.mEditPosition < self.mText.size())
					++self.mEditPosition;
				return true;
			default:
				return false;
			}
		}

		bool operator()(const CharKeyEvent& e) const
		{
			self.mText.insert(self.mEditPosition, 1, e.c);
			++self.mEditPosition;
			return true;
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	const bool handled = dispatch(e, Handler{ *this });
	if (handled)
		update();
	return handled;
}

void Textbox::update()
{
	Element::update();
	const int caretX = int(mEditPosition - mScrollPosition);
	const int width = getRect().size().x;
	if (caretX < 0)
		mScrollPosition += caretX;
	else if (caretX > width)
		mScrollPosition += caretX - width;

	if (std::cmp_less_equal(mText.size(), width))
		mScrollPosition = 0;
}

std::wstring_view Textbox::getText() const noexcept
{
	return mText;
}

void Textbox::setText(std::wstring text)
{
	mText = std::move(text);
}

LayoutSizeInfo Textbox::measureThis() const noexcept
{
	return {
		.minimum{ std::max(3, (int)mPlaceholder.size()), 1 },
		.preferred{ std::max(10, (int)mPlaceholder.size()), 1 },
	};
}
void Textbox::drawThis(ivec2 offset, Framebuffer& fb)
{
	const ivec2 dim = getRect().size();
	const ivec2 position = offset + getRect().min + ivec2(0, dim.y / 2);

	if (getUISceneState().focus)
	{
		fb.setInputCaret({
			.style = EInputCaretStyle::Bar,
			.coord = position + ivec2{ int(mEditPosition - mScrollPosition), 0 }
			});
	}

	const Colour textColour = mText.empty() ? mPlaceholderColour : mTextColour;

	fb.drawHLine(position, dim.x, EBorderStyle::None, { .text = textColour,.back = mBackColour });

	const iaabb2 rect = offset + getRect();

	if (mText.empty())
		fb.drawText(std::wstring_view(mPlaceholder).substr(mScrollPosition), rect, EAlign::Left, EAlign::Middle, { .text = textColour }, false);
	else
		fb.drawText(std::wstring_view(mText).substr(mScrollPosition), rect, EAlign::Left, EAlign::Middle, { .text = textColour }, false);

	fb.drawUnderline(position, dim.x);
}