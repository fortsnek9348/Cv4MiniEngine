#include "inc/HeckTextUI/UIScene.h"
#include "inc/HeckTextUI/Layout.h"
#include "inc/HeckTextUI/Control.h"
#include "inc/HeckTextUI/Window.h"

#include <iostream>

#include <algorithm>
#include <ranges>
#include <format>

using namespace hecktui;

UIScene::UIScene()
{
}

void UIScene::setDim(ivec2 dim)
{
	mSceneDim = dim;
}

struct UIScene::Internals
{
	struct PickResult
	{
		std::shared_ptr<Element> ctrl;
		ivec2 globalCtrlPosition{};
	};

	template<class T>
	static T fixMouseEvent(const ConsoleEvent& e, ivec2 globalCtrlPosition)
	{
		T e2 = static_cast<const T&>(e);
		e2.coord -= globalCtrlPosition;
		return e2;
	}

	static Element* getRoot(Element* element)
	{
		while (element && element->getParent())
			element = element->getParent();
		return element;
	}

	static std::span<std::shared_ptr<Window>> getInteractibleWindows(std::span<std::shared_ptr<Window>> windows)
	{
		auto afterLastModal = std::find_if(windows.rbegin(), windows.rend(), [](const std::shared_ptr<Window>& wnd) {
			return wnd->getWindowConfig().isModal;
		}).base();

		if (afterLastModal != windows.begin())
			--afterLastModal;

		return { afterLastModal, windows.end() };
	}

	static PickResult pick(std::span<std::shared_ptr<Window>> windows, ivec2 globalMousePosition, bool ignoreMouseInteractible)
	{
		for (const auto& wnd : getInteractibleWindows(windows) | std::views::reverse)
			if (PickResult result = pick(wnd, globalMousePosition, ignoreMouseInteractible, wnd->getRect().min); result.ctrl)
				return result;
		return {};
	}

	static PickResult pick(const std::shared_ptr<Element>& ctrl, ivec2 globalMousePosition, bool ignoreMouseInteractible, ivec2 globalCtrlPosition = {})
	{
		if (!ctrl->isThisVisible())
			return {};

		const ivec2 relMousePosition = globalMousePosition - globalCtrlPosition;

		for (const auto& child : ctrl->getChildren() | std::views::reverse)
			if (const iaabb2 rect = child->getRect(); rect.contains(relMousePosition))
				if (PickResult result = pick(child, globalMousePosition, ignoreMouseInteractible, globalCtrlPosition + rect.min); result.ctrl)
					return result;

		if ((ignoreMouseInteractible || ctrl->isMouseInteractable()) && iaabb2{ .max = ctrl->getRect().size() }.contains(relMousePosition))
			return { ctrl, globalCtrlPosition };

		return {};
	}

	static ivec2 getGlobalPosition(const Element* ctrl, ivec2 relCtrlPosition = {})
	{
		if (!ctrl)
			return relCtrlPosition;
		relCtrlPosition += ctrl->getRect().min;
		if (const auto parent = ctrl->getParent())
			return getGlobalPosition(parent, relCtrlPosition);
		else
			return relCtrlPosition;
	}

	static bool canFocusHierarchy(const Element& ctrl)
	{
		const Element* ctrl2 = &ctrl;
		while (ctrl2 && !ctrl2->canFocus())
			ctrl2 = ctrl2->getParent();
		return ctrl2 && ctrl2->canFocus();
	}

	static void changeLogicalHover(Element* prev, Element* next)
	{
		if (prev != next)
		{
			if (prev)
			{
				prev->onEndMouseHover();
				auto state = prev->getUISceneState();
				state.hover = false;
				prev->onUISceneStateChanged(state);
			}

			if (next)
			{
				auto state = next->getUISceneState();
				state.hover = true;
				next->onUISceneStateChanged(state);
				next->onBeginMouseHover();
			}
		}
	}

	static void changeCapture(Element* prev, Element* next)
	{
		if (prev != next)
		{
			if (prev)
			{
				auto state = prev->getUISceneState();
				state.capture = false;
				prev->onUISceneStateChanged(state);
			}

			if (next)
			{
				auto state = next->getUISceneState();
				state.capture = true;
				next->onUISceneStateChanged(state);
			}
		}
	}

	static std::shared_ptr<Element> pickDefaultFocus(std::span<const std::shared_ptr<Window>> windows)
	{
		std::shared_ptr<Element> focus;
		for (const auto& wnd : windows)
		{
			const WindowConfig& config = wnd->getWindowConfig();
			if (config.isModal)
				focus = nullptr;
			if (config.isDefaultFocus)
				focus = wnd->getClientArea();
			// Do this last so that null focus closes a drop down.
			if (config.isFocusContext)
				focus = nullptr;
			
		}
		return focus;
	}

	static void changeFocus(Element* prev, Element* next)
	{
		if (prev != next)
		{
			if (prev)
			{
				auto state = prev->getUISceneState();
				state.focus = false;
				prev->onUISceneStateChanged(state);
			}

			if (next)
			{
				auto state = next->getUISceneState();
				state.focus = true;
				next->onUISceneStateChanged(state);
			}
		}
	}

	static bool isInteractibleIgnoringWindows(Element* element)
	{
		return element && element->isHierarchicalEnabled() && element->isHierarchicalVisible();
	}

	static bool isInteractible(const UIScene& scene, Element* element)
	{
		return isInteractibleIgnoringWindows(element)
			&& std::ranges::contains(scene.mWindows, Internals::getRoot(element), [](const std::shared_ptr<Window>& wnd) { return wnd.get(); });
	}

	

	static std::shared_ptr<Element> pickDefaultControlFocus(const UIScene& scene, const std::shared_ptr<Element>& ctrl)
	{
		if (ctrl->canFocus() && isInteractibleIgnoringWindows(ctrl.get()))
			return ctrl;

		for (const auto& child : ctrl->getChildren())
			if (auto choice = pickDefaultControlFocus(scene, child))
				return choice;

		return nullptr;
	}
};

void UIScene::pushWindow(std::shared_ptr<Window> window)
{
	assert(!std::ranges::contains(mWindows, window));
	assert(!window->getParent());
	// Disable everything below new modal.
	if (window->getWindowConfig().isModal)
	{
		for (const auto& wnd : mWindows)
			wnd->setEnabled(false);
	}
	
	auto newFocus = Internals::pickDefaultControlFocus(*this, window);
	Internals::changeFocus(mFocus.lock().get(), newFocus.get());
	mFocus = newFocus;

	mWindows.push_back(window);

	window->onWindowOpened();
}

void UIScene::insertWindowAfter(const Window* position, std::shared_ptr<Window> window)
{
	assert(!std::ranges::contains(mWindows, window));
	assert(!window->getParent());

	const size_t insertI = position ? std::min<size_t>(std::ranges::find(mWindows, position, [](const auto& ptr) { return ptr.get(); }) - mWindows.begin() + 1, mWindows.size()) : 0;

	// Disable everything below new modal.
	if (window->getWindowConfig().isModal)
	{
		for (const auto& wnd : mWindows | std::views::take(insertI))
			wnd->setEnabled(false);
	}

	if (insertI >= mWindows.size())
	{
		auto newFocus = Internals::pickDefaultControlFocus(*this, window);
		Internals::changeFocus(mFocus.lock().get(), newFocus.get());
		mFocus = newFocus;
	}

	mWindows.insert(mWindows.begin() + insertI, window);

	window->onWindowOpened();
}

void UIScene::removeWindow(Window* window)
{
	const auto it = std::ranges::find(mWindows, window, [](const std::shared_ptr<Window>& wnd) { return wnd.get(); });
	if (it == mWindows.end())
		return;
	
	// Keep it allocated for a little longer
	const std::shared_ptr<Window> windowPtr = *it;

	mWindows.erase(it);

	if (!window)
		return;

	// Enable everything at and above new modal.
	bool enable = true;
	for (const auto& wnd : mWindows | std::views::reverse)
	{
		wnd->setEnabled(enable);
		enable &= !wnd->getWindowConfig().isModal;
	}

	// Focus removed from scene?
	if (window->contains(mFocus.lock().get()))
	{
		auto newFocus = Internals::pickDefaultControlFocus(*this, windowPtr);
		Internals::changeFocus(mFocus.lock().get(), newFocus.get());
		mFocus = newFocus;
	}

	window->onWindowClosed();
}

bool UIScene::empty() const
{
	return mWindows.empty();
}

void UIScene::update()
{
	// Add new context windows.
	std::vector<std::shared_ptr<Window>> newContextWindows;
	for (const auto& wnd : mWindows)
		if (auto ctxWnd = wnd->takeDeferredContextWindow())
			newContextWindows.push_back(std::move(ctxWnd));

	// It would be odd to have more than one, but anyway...
	for (auto& ctxWnd : newContextWindows)
	{
		ctxWnd->layoutPhase1Measure();
		ctxWnd->positionWindowInScene(mSceneDim);
		pushWindow(std::move(ctxWnd));
	}

	// Purge context windows.
	const std::vector<std::shared_ptr<Window>> ctxWindowsToClose = mWindows | std::views::filter([focus = mFocus.lock().get()](const std::shared_ptr<Window>& wnd) {
		return (wnd->getWindowConfig().isFocusContext || wnd->getWindowConfig().allowSceneToCloseWindow) && wnd->wantClose(focus);
	}) | std::ranges::to<std::vector>();
	for (const std::shared_ptr<Window>& ctxWnd : ctxWindowsToClose)
		removeWindow(ctxWnd.get());
	

	checkControlReferences();

	// Re-inject in case of scene changes.
	// NOTE: Do it before layout to fix quirky plot help text, where when you move the world view using the arrow keys, the plot help text changes, and needs relayout after.
	//       *But*, we may need updated element rects to handle this event... But that's not a problem yet.
	injectEvent(mLastMouseMoveEvent);

	for (const auto& wnd : mWindows)
		wnd->layoutWindow(mSceneDim);

	if (mTooltip)
	{
		if (const auto source = mTooltipSource.lock())
		{
			if (mPhysicalHover.lock() != source)
				mTooltip = nullptr;
		}
		else
			mTooltip = nullptr;
	}

	if (!mTooltip)
	{
		if (const auto hover = mPhysicalHover.lock())
		{
			mTooltip = hover->createTooltip();
			if (mTooltip)
			{
				mTooltipSource = mPhysicalHover;

				layoutElementInScene(*mTooltip, [sceneDim = mSceneDim](Element& element) {
					element.setRect(iaabb2{ .max = element.getLayoutSizeInfo().preferred }.intersection({ .max = sceneDim }));
					});

				//mTooltip->layoutPhase1Measure();
				//mTooltip->setRect({ .max = mSceneDim });
				//mTooltip->layoutPhase2Apply();
				//mTooltip->layoutPhase1Measure();
				//mTooltip->setRect({ .max = mTooltip->getLayoutSizeInfo().preferred });
				//mTooltip->layoutPhase2Apply();

				const iaabb2 srcRect = hover->getRect() + Internals::getGlobalPosition(hover->getParent());
				const ivec2 size = mTooltip->getRect().size();
				ivec2 position(srcRect.min.x, srcRect.max.y);
				if (size.x <= mSceneDim.x)
					position.x = std::clamp(position.x, 0, mSceneDim.x - size.x);
				if (position.y + size.y > mSceneDim.y)
					position.y = srcRect.min.y - size.y;
				mTooltip->setRect({ .min = position, .max = position + size });
			}
		}
	}
}

ModifierKeyState UIScene::exchangeLastModifierKeysState(ModifierKeyState state)
{
	return std::exchange(mLastModifierKeyState, state);
}
ModifierKeyState UIScene::getModifierKeyState() const noexcept
{
	return mLastModifierKeyState;
}

std::shared_ptr<hecktui::Element> UIScene::getCurrentHoverControl() const
{
	return mLogicalHover.lock();
}

void UIScene::checkControlReferences() noexcept
{
	if (const auto ctrl = mFocus.lock(); !Internals::isInteractible(*this, ctrl.get()))
	{
		Internals::changeFocus(mFocus.lock().get(), nullptr);
		mFocus.reset();
	}
	if (!mFocus.lock())
		mFocus = Internals::pickDefaultFocus(mWindows);

	if (const auto ctrl = mMouseCapture.lock(); !Internals::isInteractible(*this, ctrl.get()))
	{
		mMouseCapture.reset();
		Internals::changeCapture(ctrl.get(), nullptr);
	}
}

bool UIScene::injectEvent(const ConsoleEvent& e)
{
	checkControlReferences();

	bool handled = false;

	switch (e.type)
	{
	case EConsoleEventType::MouseMove:
	{
		mLastMouseMoveEvent = static_cast<const MouseMoveEvent&>(e);
		mLastModifierKeyState = mLastMouseMoveEvent;

		for (int i = 0; i < 2; ++i)
		{
			auto [physHoverCtrl, hoverCtrlPos] = Internals::pick(mWindows, mLastMouseMoveEvent.coord, false);

			const auto mouseCaptureCtrl = mMouseCapture.lock();

			// If mouse captured, only allow hover over the mouse capture control.
			// Handy when dragging windows when the mouse is near python buttons with expensive hover handling.
			if (mouseCaptureCtrl && mouseCaptureCtrl != physHoverCtrl)
				physHoverCtrl = nullptr;

			const auto logicalHoverCtrl = physHoverCtrl && physHoverCtrl->isHierarchicalEnabled() ? physHoverCtrl : nullptr;

			mPhysicalHover = physHoverCtrl;
			const auto oldHover = std::move(mLogicalHover);
			mLogicalHover = logicalHoverCtrl;
			Internals::changeLogicalHover(oldHover.lock().get(), logicalHoverCtrl.get());

			const auto target = mouseCaptureCtrl ? mouseCaptureCtrl : logicalHoverCtrl;

			if (Internals::isInteractible(*this, target.get()))
			{
				const bool handledThisTime = target->onEvent(Internals::fixMouseEvent<MouseMoveEvent>(e, Internals::getGlobalPosition(target.get(), {})));
				if (!handledThisTime)
					break;

				// Something may have moved. Continue on to update hover ctrl.
				handled = true;
			}
		}
		
		break;
	}

	case EConsoleEventType::MouseButtonDown:
	case EConsoleEventType::MouseButtonDoubleClick:
	case EConsoleEventType::MouseButtonUp:
	{
		const auto& e2 = static_cast<const MouseButtonEvent&>(e);

		//std::clog << (int)e2.type << std::endl;

		// Fix that bug where pressing the exit button on a screen sometimes teleports you elsewhere on the map (because the UI thought you clicked the minimap underneath).
		static_cast<MouseEvent&>(mLastMouseMoveEvent) = static_cast<const MouseEvent&>(e);

		mLastModifierKeyState = e2;
		if (const auto ctrl = mMouseCapture.lock())
		{
			if (ctrl->isHierarchicalEnabled())
			{
				handled = ctrl->onEvent(Internals::fixMouseEvent<MouseButtonEvent>(e, Internals::getGlobalPosition(ctrl.get(), {})));
				if (!e2.left && !e2.right)
				{
					mMouseCapture.reset();
					Internals::changeCapture(ctrl.get(), nullptr);
				}
			}
		}
		else
		{
			mMouseCapture.reset();

			if (const auto [ctrl2, ctrlPos] = Internals::pick(mWindows, e2.coord, false); ctrl2)
			{
				//if (e2.button == EMouseButton::Middle)
				//	_CrtDbgBreak();
				if (ctrl2->isHierarchicalEnabled())
				{
					const MouseButtonEvent fixedEvent = Internals::fixMouseEvent<MouseButtonEvent>(e, ctrlPos);
					if (e2.type != EConsoleEventType::MouseButtonUp)
					{
						if (e2.button == EMouseButton::Left || e2.button == EMouseButton::Right)
						{
							mMouseCapture = ctrl2;
							Internals::changeCapture(nullptr, ctrl2.get());
							MouseCaptureEvent captureEvent{ fixedEvent };
							captureEvent.type = EConsoleEventType::MouseCapture;
							ctrl2->onEvent(captureEvent);
						}

						if (Internals::canFocusHierarchy(*ctrl2))
						{
							Internals::changeFocus(mFocus.lock().get(), ctrl2.get());
							mFocus = ctrl2;
						}
						else
						{
							Internals::changeFocus(mFocus.lock().get(), nullptr);
							mFocus.reset();
						}
					}
					handled = ctrl2->onEvent(fixedEvent);
				}
			}
			else if (e2.type != EConsoleEventType::MouseButtonUp)
			{
				Internals::changeFocus(mFocus.lock().get(), nullptr);
				mFocus.reset();
			}
		}
		break;
	}

	case EConsoleEventType::MouseScroll:
		mLastModifierKeyState = static_cast<const MouseScrollEvent&>(e);
		if (const auto [ctrl, ctrlPos] = Internals::pick(mWindows, static_cast<const MouseScrollEvent&>(e).coord, false); ctrl)
		{
			Element* eventCtrl = ctrl.get();
			ivec2 eventPos = ctrlPos;
			while (eventCtrl && !handled)
			{
				if (eventCtrl->isHierarchicalEnabled())
					handled = eventCtrl->onEvent(Internals::fixMouseEvent<MouseScrollEvent>(e, eventPos));
				// Don't want disabled controls swallowing up events.
				//else if (eventCtrl->isMouseInteractable())
				//	break;
				eventPos -= ctrl->getRect().min;
				eventCtrl = eventCtrl->getParent();
			}
		}
		break;

	case EConsoleEventType::ActionKeyPressed:
	case EConsoleEventType::CharKeyPressed:
		if (e.type == EConsoleEventType::ActionKeyPressed)
			mLastModifierKeyState = static_cast<const ActionKeyEvent&>(e);
		else
			mLastModifierKeyState = static_cast<const CharKeyEvent&>(e);
		for (;;)
		{
			if (const auto ctrl = mFocus.lock(); Internals::isInteractible(*this, ctrl.get()))
			{
				Element* handler = ctrl.get();
				while (handler && !handler->onEvent(e))
					handler = handler->getParent();
				break;
			}

			if (auto defFocus = Internals::pickDefaultFocus(mWindows))
				mFocus = std::move(defFocus);
			else
				break;
		}
		break;

	case EConsoleEventType::None:
	default:
		break;
	}

	checkControlReferences();

	return handled;
}

void UIScene::draw(Framebuffer& fb) const
{
	for (const auto& wnd : mWindows)
		wnd->draw({}, fb);

	if (mTooltip)
		mTooltip->draw({}, fb);
}