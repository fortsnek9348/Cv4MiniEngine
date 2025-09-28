#pragma once

#include "Core.h"

#include <memory>

namespace hecktui
{
	class Element;
	class Window;

	class HECKCONSOLEUI_EXPORT UIScene
	{
	public:
		UIScene();

		void setDim(ivec2 dim);

		//const std::shared_ptr<Element>& getRoot() const noexcept;

		void pushWindow(std::shared_ptr<Window> window);
		void insertWindowAfter(const Window* wnd, std::shared_ptr<Window> window);
		void removeWindow(Window* window);
		[[nodiscard]] bool empty() const;

		void update();

		ModifierKeyState exchangeLastModifierKeysState(ModifierKeyState state);
		ModifierKeyState getModifierKeyState() const noexcept;
		std::shared_ptr<hecktui::Element> getCurrentHoverControl() const;

		bool injectEvent(const ConsoleEvent& e);

		void draw(Framebuffer& fb) const;

	private:
		struct Internals;

		ivec2 mSceneDim{};
		ModifierKeyState mLastModifierKeyState{};
		MouseMoveEvent mLastMouseMoveEvent{};
		std::vector<std::shared_ptr<Window>> mWindows;
		std::weak_ptr<Element> mFocus;
		std::weak_ptr<Element> mPhysicalHover; // ignores enabled state, to allow tooltips on disabled controls
		std::weak_ptr<Element> mLogicalHover;
		std::weak_ptr<Element> mMouseCapture;

		std::shared_ptr<Element> mTooltip;
		std::weak_ptr<Element> mTooltipSource;

		void checkControlReferences() noexcept;
	};
}