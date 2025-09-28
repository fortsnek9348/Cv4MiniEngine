#pragma once

#include "Layout.h"

namespace hecktui
{
	class UIScene;
	class Window;

	class HECKCONSOLEUI_EXPORT Element
	{
	public:
		explicit Element(std::unique_ptr<Layout> layout = nullptr) noexcept;
		Element(const Element&) = delete;
		Element& operator=(const Element&) = delete;

		void addChild(std::shared_ptr<Element>);
		void insertChildren(size_t i, std::span<const std::shared_ptr<Element>> children);
		void removeChild(Element* element) noexcept;
		void setChild(size_t i, std::shared_ptr<Element>);
		void removeAllChildren() noexcept;
		void orphan();

		void moveToFirst();

		Element* getParent() const;

		bool contains(const Element*) const noexcept;

		const std::shared_ptr<Element>& getChildAt(size_t i) const noexcept;
		std::span<const std::shared_ptr<Element>> getChildren() const noexcept;

		void setLayout(std::unique_ptr<Layout> layout) noexcept;
		Layout* getLayout() const noexcept;

		void setRect(iaabb2 rect) noexcept;
		const iaabb2& getRect() const noexcept;

		ivec2 getGlobalPosition() const noexcept;

		void setEnabled(bool value);
		void setVisible(bool value);

		bool isThisEnabled() const noexcept;
		bool isThisVisible() const noexcept;

		bool isHierarchicalEnabled() const noexcept;
		bool isHierarchicalVisible() const noexcept;

		bool canFocus() const noexcept;
		bool isMouseInteractable() const noexcept;

		
		// Horizontal or Vertical only for controls that have wrapped /content/.
		// nullopt for everything else including non-wrapping containers of such controls.
		// UIScene will collect up wrapping elements and track them during layout.
		// Wrapping controls should use getRect().size()[axis] to compute preferred size.
		// Overriders must call the base function when they don't have their own wrapping.
		virtual std::optional<EAxis> getThisWrappingAxis() const noexcept;

		// Overriders should call base implementation first.
		// Classes that have state persistent over layout solving should reset that state (wrapping and optional scrollbars).
		// If initial, returns true iff another solver iteration is needed.
		// Else, return value ignored.
		[[nodiscard]] virtual bool layoutPrepareIteration(bool initial) noexcept;
		void layoutPhase1Measure();
		const LayoutSizeInfo& getLayoutSizeInfo() const noexcept;
		// Returns true iff another solver iteration is needed.
		void layoutPhase2Apply();
		

		// Returns false if not handled.
		virtual bool onEvent(const ConsoleEvent& e);

		virtual void onBeginMouseHover();
		virtual void onEndMouseHover();

		// https://javascript.info/mousemove-mouseover-mouseout-mouseenter-mouseleave
		// MouseOver/MouseOut are not hierarchical. They track the single hover control in UIScene.
		// MouseEnter/MouseLeave are the hierarchical events.
		// Return false to bubble up.
		//virtual [[nodiscard]] bool onMouseOver();
		//virtual [[nodiscard]] bool onMouseOut();

		virtual std::shared_ptr<Element> createTooltip() const;

		struct UISceneState
		{
			bool hover = false;
			bool capture = false;
			bool focus = false;

			friend bool operator==(UISceneState, UISceneState) = default;
		};

		// Called by UIScene only.
		void onUISceneStateChanged(UISceneState);

		UISceneState getUISceneState() const noexcept;

		// Called in various cases.
		// UIScene onUISceneStateChanged/Hover change.
		// isHierarchicalEnabled change.
		// Size change.
		virtual void update();
		
		void draw(ivec2 offset, Framebuffer& fb);

		virtual ~Element() noexcept;

		bool debug = false;

	protected:
		// The `minimum` size returned is indepdnent of `space`. It is the true minimum width and minimum height this control's own content can ever have.
		// The `preferred` size returns may be wrapped within the given `space`.
		//     One or both dimensions may exceed `space`. In the case of wrapping, a single word may be wider than the given space, or a minimum number of lines is needed.
		// `preferred` has the important property that one dimension decreases as the other increases. Ie: it's a monotonic function.
		virtual LayoutSizeInfo measureThis() const;

		// Returns true iff another solver iteration is needed.
		// Overriders should combine result with base implementation.
		//[[nodiscard]] virtual bool layoutApplyThis() noexcept;

		virtual void drawThis(ivec2 offset, Framebuffer& fb);

		// To continue to isolate controls from the UIScene, when controls need to pop up a context window, they'll call this instead.
		// Default implementation will bubble up to the parent. The Window implementation will queue one up. Only one.
		// The UIScene can then query Windows for new context windows.
		virtual void deferContextWindow(std::shared_ptr<Window> wnd);

		bool mCanFocus = false;
		// Mouse transparency.
		// If false, this control is totally transparent to the mouse.
		// Child controls can still have mouse interaction.
		// This could be used to put labels on top of buttons, etc.
		bool mIsMouseInteractable = false;

	private:
		Element* mParent = nullptr;
		std::vector<std::shared_ptr<Element>> mChildren;
		std::unique_ptr<Layout> mLayout;

		iaabb2 mRect{};
		LayoutSizeInfo mLayoutSizeInfo;

		bool mEnabled = true;
		bool mVisible = true;
		bool mHierarchicallyEnabled = true;
		UISceneState mUISceneState{};

		void updateHierarchicallyEnabled();
	};
}