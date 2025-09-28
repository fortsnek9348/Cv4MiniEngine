#pragma once

#include "Control.h"

#include <functional>

namespace hecktui
{
	class Button;

	struct WindowConfig
	{
		bool isDefaultFocus = false; // If focus is about to be cleared, the top-most isDefaultFocus Window, that is not below a modal dialog, will get the focus.
		bool isFullscreen = false; // UIScene will always maxmise window.
		bool isModal = false; // UIScene will disable all prior windows in the stack.
		bool canClose = false; // Close button in the title bar. Esc closes too.
		bool isStatic = false;
		bool isFocusContext = false; // UIScene will close this window if focus leaves.
		bool allowSceneToCloseWindow = false; // UIScene will close this window if wantClose.
		EBorderStyle borderStyle = EBorderStyle::None; // If no border, then no title bar either.
	};

	// This will position a window/tooltip using an iterative loop, which will handle wrapped layouts.
	void layoutElementInScene(Element& element, std::function<void(Element&)> reposition);

	class HECKCONSOLEUI_EXPORT Window : public Element
	{
	public:
		explicit Window(std::wstring title, WindowConfig config);

		WindowConfig getWindowConfig() const noexcept;
		void setWindowTitle(std::wstring title);

		std::shared_ptr<Element> getClientArea() noexcept;
		void setClientArea(std::shared_ptr<Element>);

		virtual void onWindowOpened();
		virtual void onWindowClosed();

		void setWantClose() noexcept;
		
		virtual bool wantClose(const Element* focus) const noexcept;
		bool wasMoved() const noexcept;
		bool wasResized() const noexcept;
		

		virtual bool onEvent(const ConsoleEvent&) override;

		void layoutWindow(ivec2 sceneDim);
		// Called between layout phases. Optionally set window position and size here.
		virtual void positionWindowInScene(ivec2 sceneDim);

		std::shared_ptr<Window> takeDeferredContextWindow() noexcept;

	protected:
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

		virtual void deferContextWindow(std::shared_ptr<Window> wnd) override;

	private:
		struct CloseButton;

		std::wstring mTitle;
		WindowConfig mWindowConfig{};
		std::shared_ptr<Element> mClientArea;
		std::shared_ptr<CloseButton> mBtnClose;

		ivec2 mMouseCaptureGrabDir{};
		ivec2 mMouseCaptureGrabPos{};

		bool mWantClose = false;
		bool mWasMoved = false;
		bool mWasResized = false;

		std::shared_ptr<Window> mDeferredContextWindow;
	};
}