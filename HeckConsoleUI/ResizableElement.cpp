#include "inc/HeckTextUI/BasicControls.h"

using namespace hecktui;

ResizableElement::ResizableElement(EBorderStyle style) : BoxPanel(style)
{
	mIsMouseInteractable = true;
}

bool ResizableElement::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		ResizableElement& self;

		bool operator()(const MouseCaptureEvent& e) const
		{
			self.mMouseCaptureGrabDir = {};
			self.mMouseCaptureGrabPos = e.coord;

			if (self.enableLeftBorder && e.coord.x == 0)
				self.mMouseCaptureGrabDir.x = -1;
			if (self.enableTopBorder && e.coord.y == 0)
				self.mMouseCaptureGrabDir.y = -1;
			if (self.enableRightBorder && e.coord.x == self.getRect().size().x - 1)
				self.mMouseCaptureGrabDir.x = +1;
			if (self.enableBottomBorder && e.coord.y == self.getRect().size().y - 1)
				self.mMouseCaptureGrabDir.y = +1;

			return true;
		}

		bool operator()(const MouseMoveEvent& e) const
		{
			if (self.getUISceneState().capture)
			{
				const ivec2 coord = e.coord + self.getRect().min;
				iaabb2 rect = self.getRect();
				if (self.mMouseCaptureGrabDir.x < 0)
					rect.min.x = coord.x, self.mHasHResize = true;
				if (self.mMouseCaptureGrabDir.y < 0)
					rect.min.y = coord.y, self.mHasVResize = true;
				if (self.mMouseCaptureGrabDir.x > 0)
					rect.max.x = coord.x + 1, self.mHasHResize = true;
				if (self.mMouseCaptureGrabDir.y > 0)
					rect.max.y = coord.y + 1, self.mHasVResize = true;
				if (self.mMouseCaptureGrabDir == ivec2() && self.enableLeftBorder && self.enableTopBorder && self.enableRightBorder && self.enableBottomBorder)
				{
					const ivec2 newPos = coord - self.mMouseCaptureGrabPos;
					rect = { .min = newPos, .max = newPos + rect.size() };
				}
				self.setRect(rect);

				return true;
			}

			return false;
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	return dispatch(e, Handler(*this));
}

bool ResizableElement::hasHResize() const noexcept
{
	return mHasHResize;
}

bool ResizableElement::hasVResize() const noexcept
{
	return mHasVResize;
}

LayoutSizeInfo ResizableElement::measureThis() const noexcept
{
	LayoutSizeInfo base = BoxPanel::measureThis();
	base.preferred = vmax(base.preferred, getRect().size());
	return base;
}