#include "inc/HeckTextUI/BasicControls.h"

#include <algorithm>
#include <format>

using namespace hecktui;

namespace
{
	// Scrollbar math

	int calcMaxScrollPosition(int viewSize, int contentSize)
	{
		return std::max(0, contentSize - viewSize);
	}

	int calcThumbSize(int viewSize, int contentSize, int scrollbarSize)
	{
		contentSize = std::max(contentSize, 1);
		return std::max(1, int(std::int64_t(scrollbarSize) * std::min(viewSize, contentSize) / contentSize));
		//(void)(viewSize, contentSize, scrollbarSize);
		//return 5;
	}

	int calcMaxThumbPosition(int viewSize, int contentSize, int scrollbarSize)
	{
		return scrollbarSize - calcThumbSize(viewSize, contentSize, scrollbarSize);
	}

	int calcThumbPosition(int viewSize, int contentSize, int scrollbarSize, int scrollPosition)
	{
		const int maxScrollPosition = calcMaxScrollPosition(viewSize, contentSize);
		const int maxThumbStartPosition = calcMaxThumbPosition(viewSize, contentSize, scrollbarSize);
		scrollPosition = std::clamp(scrollPosition, 0, maxScrollPosition);
		//return int((int64_t(maxThumbStartPosition) * scrollPosition + maxScrollPosition - 1) / maxScrollPosition);
		return maxScrollPosition > 0 ? int(std::int64_t(maxThumbStartPosition) * scrollPosition / maxScrollPosition) : 0;
	}

	int calcScrollPosition(int viewSize, int contentSize, int scrollbarSize, int thumbPosition)
	{
		const int maxScrollPosition = calcMaxScrollPosition(viewSize, contentSize);
		const int maxThumbStartPosition = calcMaxThumbPosition(viewSize, contentSize, scrollbarSize);
		thumbPosition = std::clamp(thumbPosition, 0, maxThumbStartPosition);

		// X = floor(Y / Z)
		// Y = [X * Z .. X * Z + Z - 1]

		// thumbPosition = floor(maxThumbStartPosition * scrollPosition / maxScrollPosition)
		// maxThumbStartPosition * scrollPosition = [thumbPosition * maxScrollPosition .. thumbPosition * maxScrollPosition + maxScrollPosition - 1]
		// scrollPosition = [thumbPosition * maxScrollPosition / maxThumbStartPosition .. (thumbPosition * maxScrollPosition + maxScrollPosition - 1) / maxThumbStartPosition]
		// scrollPosition = [ceil(thumbPosition * maxScrollPosition / maxThumbStartPosition) .. floor((thumbPosition * maxScrollPosition + maxScrollPosition - 1) / maxThumbStartPosition)]

		return maxThumbStartPosition > 0 ? int((int64_t(maxScrollPosition) * thumbPosition + maxThumbStartPosition - 1) / maxThumbStartPosition) : 0;
	}
}

Scrollbar::Scrollbar(EAxis axis) : mAxis(axis)
{
	mCanFocus = false;
	mIsMouseInteractable = true;
}

void Scrollbar::setViewSize(int x)
{
	mViewSize = x;
	//update();
}
void Scrollbar::setContentSize(int x)
{
	mContentSize = x;
	//update();
}
int Scrollbar::getScrollPosition() const noexcept
{
	const int maxScrollPosition = std::max(0, mContentSize - mViewSize);
	return std::clamp(mScrollPosition, 0, maxScrollPosition);
	//return mScrollPosition;
}

void Scrollbar::scrollPageDown()
{
	scrollDown(mViewSize);
}
void Scrollbar::scrollPageUp()
{
	scrollUp(mViewSize);
}

void Scrollbar::scrollDown(int d)
{
	mScrollPosition = (int)std::clamp<int64_t>(int64_t(mScrollPosition) + d, 0, std::numeric_limits<int>::max());

	// Defer until next update when scroll params are updated too.
	//update();
	//onScrollPositionChanged();
	clampScrollPosition();
}
void Scrollbar::scrollUp(int d)
{
	mScrollPosition = (int)std::clamp<int64_t>(int64_t(mScrollPosition) - d, 0, std::numeric_limits<int>::max());
	//update();
	//onScrollPositionChanged();
	clampScrollPosition();
}

// Resizing a container will usually make the scroll position invalid, but it must be preserved across
// layout iterations as the original scroll position may end up valid again in the last iteration.
void Scrollbar::clampScrollPosition()
{
	const int newScrollPosition = getScrollPosition();
	if (mScrollPosition != newScrollPosition)
	{
		mScrollPosition = newScrollPosition;
		onScrollPositionChanged();
	}
}

bool Scrollbar::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		Scrollbar& self;

		bool operator()(const MouseMoveEvent& e) const
		{
			if (self.getUISceneState().capture)
			{
				if (self.mCaptureIsThumb)
				{
					//checkDebugBreakKey();

					const int scrollbarSize = self.getRect().size()[(int)self.mAxis];
					const int captureThumbPosition = calcThumbPosition(self.mViewSize, self.mContentSize, scrollbarSize, self.mCaptureScrollPosition);
					const int deltaThumbPosition = e.coord[(int)self.mAxis] - self.mCaptureMouseCoord;
					const int newThumbPosition = captureThumbPosition + deltaThumbPosition;
					const int newScrollPosition = calcScrollPosition(self.mViewSize, self.mContentSize, scrollbarSize, newThumbPosition);

					if (self.mScrollPosition != newScrollPosition)
					{
						self.mScrollPosition = newScrollPosition;
						self.onScrollPositionChanged();
					}

					//debugLog(std::format(L"{} {}\n", newThumbPosition, newScrollPosition));
				}

				return true;
			}

			self.mHoverThumb = self.getThumbRect().contains(e.coord);

			return true;
		}

		bool operator()(const MouseButtonEvent& e) const
		{
			if (e.type == EConsoleEventType::MouseButtonDown && e.button == EMouseButton::Left)
			{
				const iaabb2 rc = self.getThumbRect();
				if (!rc.contains(e.coord))
				{
					if (e.coord[(int)self.mAxis] < rc.min[(int)self.mAxis])
						self.scrollPageUp();
					else
						self.scrollPageDown();
				}
				return true;
			}

			return false;
		}

		bool operator()(const MouseCaptureEvent& e) const
		{
			self.mCaptureMouseCoord = e.coord[(int)self.mAxis];
			self.mCaptureScrollPosition = self.mScrollPosition;
			self.mCaptureIsThumb = self.getThumbRect().contains(e.coord);
			return true;
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	return dispatch(e, Handler(*this));
}

void Scrollbar::update()
{
	Element::update();
}

LayoutSizeInfo Scrollbar::measureThis() const noexcept
{
	// Double thickness for vertical scrollbars.
	const ivec2 v = mAxis == EAxis::Vertical ? ivec2(2, 5) : ivec2(5, 1);
	return { v, v };
}
void Scrollbar::drawThis(ivec2 offset, Framebuffer& fb)
{
	//const int scrollbarSize = getRect().size()[(int)mAxis];
	//const int thumbPosition = calcThumbPosition(mViewSize, mContentSize, scrollbarSize, mScrollPosition);
	//const int thumbSize = calcThumbSize(mViewSize, mContentSize, scrollbarSize);

	clampScrollPosition();

	const Pixel gutterPixel{
		//.c = L'▒',
		.c = L'░',
		.colour = {.text = EColour::Silver },
	};


	const Pixel thumbPixel{
		.c = L'█',
		.colour = {.text = (mHoverThumb && getUISceneState().hover) || (getUISceneState().capture && mCaptureIsThumb) ? EColour::White : EColour::Silver },
	};

	const iaabb2 trackRect = offset + getRect();
	const iaabb2 thumbRect = getThumbRect() + trackRect.min;

	fb.drawFilledBox(trackRect, gutterPixel);
	fb.drawFilledBox(thumbRect, thumbPixel);
}

void Scrollbar::onScrollPositionChanged()
{
}

iaabb2 Scrollbar::getThumbRect() const
{
	iaabb2 rc{};
	rc.min.x = calcThumbPosition(mViewSize, mContentSize, getRect().size()[(int)mAxis], mScrollPosition);
	rc.max.x = rc.min.x + calcThumbSize(mViewSize, mContentSize, getRect().size()[(int)mAxis]);
	rc.min.y = 0;
	rc.max.y = 1;

	if (mAxis == EAxis::Vertical)
	{
		rc.max.y = getRect().size().x;
		rc.min = rc.min.flipped();
		rc.max = rc.max.flipped();
	}
	else
		rc.max.y = getRect().size().y;

	return rc;
}