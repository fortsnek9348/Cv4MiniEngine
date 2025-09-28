#include "inc/HeckTextUI/Window.h"
#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"

#include <ranges>
#include <iostream>

using namespace hecktui;

struct Window::CloseButton : Button
{
	using Button::Button;

	virtual void onClick(ModifierKeyState) override
	{
		static_cast<Window&>(*getParent()).setWantClose();
	}

	virtual LayoutSizeInfo measureThis() const noexcept override
	{
		const ivec2 v = Button::measureThis().minimum + ivec2(2, 0);
		return { v, v };
	}
};

Window::Window(std::wstring title, WindowConfig config) : mTitle(std::move(title)), mWindowConfig(config)
{
	mCanFocus = config.isDefaultFocus;
	mIsMouseInteractable = true;
	mClientArea = std::make_shared<Element>(std::make_unique<FillLayout>());
	addChild(mClientArea);
	if (config.canClose && config.borderStyle != EBorderStyle::None)
	{
		mBtnClose = std::make_shared<CloseButton>(L"[x]");
		mBtnClose->setBorderStyle(EBorderStyle::None);
		addChild(mBtnClose);
	}

	const int borderThickness = config.borderStyle != EBorderStyle::None ? 1 : 0;
	const bool hasTitleBar = (!mTitle.empty() || mBtnClose) && mWindowConfig.borderStyle != EBorderStyle::None;

	TableLayoutConfig layoutConfig;
	layoutConfig.cols = {
		{.min = borderThickness },
		{.min = borderThickness ? (int)mTitle.size() : 0, .weight = 1 },
		{.min = borderThickness },
	};
	layoutConfig.rows = {
		{.min = borderThickness },
		{.min = hasTitleBar ? 1 : 0 },
		{.min = hasTitleBar ? 1 : 0 },
		{.weight = 1 },
		{.min = borderThickness },
	};
	layoutConfig.cells = {
		{.coord{ 1, 3 }, .span{ 1, 1 } },
	};
	if (mBtnClose)
		layoutConfig.cells.push_back({ .coord{ 1, 1 }, .align{ EJustilign::End, EJustilign::Stretch } });

	setLayout(std::make_unique<TableLayout>(std::move(layoutConfig)));
}

WindowConfig Window::getWindowConfig() const noexcept
{
	return mWindowConfig;
}

void Window::setWindowTitle(std::wstring title)
{
	mTitle = std::move(title);
}

std::shared_ptr<Element> Window::getClientArea() noexcept
{
	return mClientArea;
}

void Window::setClientArea(std::shared_ptr<Element> root)
{
	setChild(0, root);
	mClientArea = root;
}

void Window::onWindowOpened()
{
}
void Window::onWindowClosed()
{
}

void Window::setWantClose() noexcept
{
	mWantClose = true;
}

bool Window::wantClose(const Element* focus) const noexcept
{
	return mWantClose || (mWindowConfig.isFocusContext && !contains(focus));
}

bool Window::wasMoved() const noexcept
{
	return mWasMoved;
}
bool Window::wasResized() const noexcept
{
	return mWasResized;
}

/*static void gatherWrappedElements(Element* element, std::vector<Element*>& elements)
{
	if (element->isThisVisible())
	{
		if (element->getThisWrappingAxis())
		{
			element->setRect({ .max{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max() } });
			elements.push_back(element);
		}
		for (const auto& child : element->getChildren())
			gatherWrappedElements(child.get(), elements);
	}
}*/

void hecktui::layoutElementInScene(Element& windowElement, std::function<void(Element&)> reposition)
{
	//std::vector<Element*> wrappedElements;
	//gatherWrappedElements(&windowElement, wrappedElements);
	//std::vector<ivec2> wrappedSizes(std::from_range, wrappedElements | std::views::transform([](const Element* element) {
	//	return element->getRect().size();
	//	}));
	//for (;;)
	//{
	//	windowElement.layoutPhase1Measure();
	//	//positionWindowInScene(sceneDim);
	//	reposition(windowElement);
	//	windowElement.layoutPhase2Apply();
	//
	//	bool iterationNeeded = false;
	//	for (auto&& [wrappedElement, size] : std::views::zip(wrappedElements, wrappedSizes))
	//	{
	//		int ivec2::* x = wrappedElement->getThisWrappingAxis() == EAxis::Horizontal ? &ivec2::x : &ivec2::y;
	//		const int length = wrappedElement->getRect().size().*x;
	//		if (length < size.*x)
	//		{
	//			size.*x = length;
	//			iterationNeeded = true;
	//		}
	//	}
	//
	//	if (!iterationNeeded)
	//		break;
	//}

	for (bool initial = true; windowElement.layoutPrepareIteration(initial) || initial; initial = false)
	{
		windowElement.layoutPhase1Measure();
		reposition(windowElement);
		windowElement.layoutPhase2Apply();
	}
}

void Window::layoutWindow(ivec2 sceneDim)
{
	//if (!mWindowConfig.isFullscreen)
	//	std::clog << "Window layout start." << std::endl;

	layoutElementInScene(*this, [sceneDim](Element& element) {
		static_cast<Window&>(element).positionWindowInScene(sceneDim);
		});
}

void Window::positionWindowInScene(ivec2 sceneDim)
{
	if (mWindowConfig.isFullscreen)
		setRect({ .max = sceneDim });
	else
	{
		const ivec2 layoutDim = getLayoutSizeInfo().preferred;
		setRect(iaabb2::sized((sceneDim - layoutDim) / 2, layoutDim).intersection(iaabb2::sized({}, sceneDim)));
	}
}

bool Window::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		Window& self;

		bool operator()(const MouseCaptureEvent& e) const
		{
			//std::clog << "Capture" << std::endl;

			self.mMouseCaptureGrabDir = {};
			self.mMouseCaptureGrabPos = e.coord;

			if (self.mWindowConfig.borderStyle != EBorderStyle::None)
			{
				if (e.coord.x == 0)
					self.mMouseCaptureGrabDir.x = -1;
				if (e.coord.y == 0)
					self.mMouseCaptureGrabDir.y = -1;
				if (e.coord.x == self.getRect().size().x - 1)
					self.mMouseCaptureGrabDir.x = +1;
				if (e.coord.y == self.getRect().size().y - 1)
					self.mMouseCaptureGrabDir.y = +1;
			}

			return true;
		}

		bool operator()(const MouseMoveEvent& e) const
		{
			if (!self.getWindowConfig().isStatic && self.getUISceneState().capture)
			{
				if (self.mWindowConfig.borderStyle != EBorderStyle::None)
				{
					
					const ivec2 coord = e.coord + self.getRect().min;
					//std::clog << coord.y << std::endl;
					iaabb2 rect = self.getRect();
					if (self.mMouseCaptureGrabDir.x < 0)
						rect.min.x = coord.x;
					if (self.mMouseCaptureGrabDir.y < 0)
						rect.min.y = coord.y;
					if (self.mMouseCaptureGrabDir.x > 0)
						rect.max.x = coord.x + 1;
					if (self.mMouseCaptureGrabDir.y > 0)
						rect.max.y = coord.y + 1;
					if (self.mMouseCaptureGrabDir == ivec2())
					{
						const ivec2 newPos = coord - self.mMouseCaptureGrabPos;
						rect = { .min = newPos, .max = newPos + rect.size() };
					}
					const bool changed = rect != self.getRect();
					self.setRect(rect);

					if (changed)
					{
						self.mWasMoved |= self.mMouseCaptureGrabDir == ivec2();
						self.mWasResized |= self.mMouseCaptureGrabDir != ivec2();
					}
				}
			}

			return true;
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	return dispatch(e, Handler(*this));
}

std::shared_ptr<Window> Window::takeDeferredContextWindow() noexcept
{
	return std::move(mDeferredContextWindow);
}


void Window::drawThis(ivec2 offset, Framebuffer& fb)
{
	const bool hasTitleBar = (!mTitle.empty() || mBtnClose) && mWindowConfig.borderStyle != EBorderStyle::None;

	if (mWindowConfig.borderStyle != EBorderStyle::None)
	{
		fb.drawFilledBox(offset + getRect(), mWindowConfig.borderStyle, { .back = EColour::Black });
		if (hasTitleBar)
			fb.drawHLine(offset + getRect().min + ivec2(1, 2), getRect().size().x - 2, EBorderStyle::Thin, {});
		fb.drawText(mTitle, offset + getRect().shrunk(ivec2(1, 1)), EAlign::Center, EAlign::Top, {}, false);
	}
}

void Window::deferContextWindow(std::shared_ptr<Window> wnd)
{
	std::swap(mDeferredContextWindow, wnd);
}