#pragma once

#include "Control.h"

#include <string>
#include <functional>

namespace hecktui
{
	class HECKCONSOLEUI_EXPORT ResizablePanel: public Element
	{
	public:
		explicit ResizablePanel(EAxis axis, int splitterPosition, bool flip = false);

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void update() override;

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

	private:
		struct MyLayout;

		EAxis mAxis{};
		int mSplitterPosition = 0;
		bool mFlip = false;

		bool mMouseCaptured = false;
		bool mMouseSplitterHover = false;

		int getPhysicalSplitterPosition() const;
		void setPhysicalSplitterPosition(int x);
	};

	

	class HECKCONSOLEUI_EXPORT Label : public Element
	{
	public:
		explicit Label(std::wstring label = {});

		virtual std::optional<EAxis> getThisWrappingAxis() const noexcept override;

		virtual bool layoutPrepareIteration(bool initial) noexcept override;

		std::wstring_view getLabel() const noexcept;
		void setLabel(std::wstring label);
		void setLabelAlignment(EAlign halign);

		PixelColouring colouring{};
		bool enableWrapping = false;

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;
		//virtual bool layoutCheckNeedIterationForThis() const noexcept override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

		std::wstring mLabel;
		EAlign mHAlign = EAlign::Left;

		// Layout solving element state
		int mLayoutWrappingWidth = 0;
	};

	class HECKCONSOLEUI_EXPORT EmptyButton : public Element
	{
	public:
		explicit EmptyButton(std::function<void()> onClickCallback = {});

		virtual void setBorderStyle(EBorderStyle) noexcept;
		void setLabelColour(Colour colour);
		Colour getLabelColour() const;
		void setBackgroundColour(Colour colour);
		void setOnClickCallback(std::function<void()> onClickCallback);
		void setEnableRightClick(bool);

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void onClick(ModifierKeyState modifierKeyState);
		virtual void onRightClick(ModifierKeyState modifierKeyState);

		virtual void update() override;

	protected:
		virtual LayoutSizeInfo measureThis() const override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

		PixelColouring getCurrentPixelColouring() const noexcept;

	protected:
		EBorderStyle mBorderStyle = EBorderStyle::Rounded;

	private:
		PixelColouring mColouring{};
		bool mRightClickEnabled = false;
		std::function<void()> mOnClickCallback;
	};

	// Contains a Label as a child, and a fill layout.
	class HECKCONSOLEUI_EXPORT Button : public EmptyButton
	{
	public:
		explicit Button(std::wstring label, std::function<void()> onClickCallback = {});

		// Dynamically adds a label child control and sets our layout.
		void setLabel(std::wstring label);
		void setLabelHAlign(EAlign align);

		virtual void setBorderStyle(EBorderStyle) noexcept override;

		Label& getLabelElement() const noexcept;

		std::wstring_view getLabel() const noexcept;

	private:
		std::shared_ptr<Label> mLabel;
		//FillLayout* mLabelLayout = nullptr;
	};

	

	enum ECheckStyle
	{
		None,
		AsciiX,
		Tick,
		TickBox,
		Bullet,
		Radio,
		Border,
	};

	class HECKCONSOLEUI_EXPORT Checkbox : public Element
	{
	public:
		explicit Checkbox(ECheckStyle style, std::wstring label, std::function<void(bool)> onCheckChangedCallback = {});

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void update() override;

		virtual void onCheckChanged();

		void setLabel(std::wstring label);
		std::wstring_view getLabel() const noexcept;

		void setColouring(PixelColouring);

		void setIsRadioButton(bool enable);

		bool value = false;

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

	private:
		PixelColouring mDefaultColouring{};
		ECheckStyle mStyle{};
		bool mIsRadioButton = false;
		std::wstring mLabel;
		std::function<void(bool)> mOnCheckChangedCallback;
	};

	class HECKCONSOLEUI_EXPORT RadioButton : public Checkbox
	{
	public:
		explicit RadioButton(ECheckStyle style, std::wstring label, std::function<void(bool)> onCheckChangedCallback = {});
	};

	class HECKCONSOLEUI_EXPORT BoxPanel : public Element
	{
	public:
		explicit BoxPanel(EBorderStyle style, PixelColouring colouring = {});
		EBorderStyle style = EBorderStyle::Thin;
		PixelColouring borderColouring{};
		PixelColouring interiorColouring{ .text = kTransparent, .back = kTransparent };
		bool enableLeftBorder = true;
		bool enableRightBorder = true;
		bool enableTopBorder = true;
		bool enableBottomBorder = true;
		bool enableAutoMergeCardinal = true;
	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;
	};

	class HECKCONSOLEUI_EXPORT HorizontalRule : public BoxPanel
	{
	public:
		explicit HorizontalRule(EBorderStyle style, PixelColouring colouring = {});
	};

	class HECKCONSOLEUI_EXPORT VerticalRule : public BoxPanel
	{
	public:
		explicit VerticalRule(EBorderStyle style, PixelColouring colouring = {});
	};

	class HECKCONSOLEUI_EXPORT Scrollbar : public Element
	{
	public:
		explicit Scrollbar(EAxis axis);

		void setViewSize(int);
		void setContentSize(int);
		int getScrollPosition() const noexcept;

		void scrollPageDown();
		void scrollPageUp();
		void scrollDown(int d);
		void scrollUp(int d);

		void clampScrollPosition();

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void update() override;

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(ivec2 offset, Framebuffer& fb) override;

		virtual void onScrollPositionChanged();

	private:
		EAxis mAxis{};
		int mScrollPosition = 0;
		int mViewSize = 0;
		int mContentSize = 0;

		int mCaptureMouseCoord = 0;
		int mCaptureScrollPosition = 0;
		bool mCaptureIsThumb = false;
		bool mHoverThumb = false;

		iaabb2 getThumbRect() const;
	};

	class HECKCONSOLEUI_EXPORT BareScrollPanel : public Element
	{
	public:
		BareScrollPanel();

		std::shared_ptr<hecktui::Element> getPanel() const noexcept;

		void scroll(ivec2 d);
		void setScrollPosition(ivec2 p);
		void clampScrollPosition();

		void setLayoutSizeOverride(std::optional<int> w, std::optional<int> h);

	
		void setPanelSizeLimitEnable(bool w, bool h);

		ivec2 getScrollPosition() const;
	};

	class HECKCONSOLEUI_EXPORT ScrollBarPanel : public Element
	{
	public:
		explicit ScrollBarPanel(bool enableHorizScrolling, bool enableVertScrolling);
		explicit ScrollBarPanel(EAxis axis);

		std::shared_ptr<hecktui::Element> getPanel() const noexcept;

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void update() override;

		void scrollToEnd();

		void setLayoutSizeOverride(std::optional<int> w, std::optional<int> h);

		ivec2 getClampedScrollPosition() const;

	//protected:
	//	virtual bool layoutCheckNeedIterationForThis() const noexcept override;

	private:
		bool mEnableHorizScrolling = false;
		bool mEnableVertScrolling = false;
	};

	class HECKCONSOLEUI_EXPORT ScrollSeekPanel : public Element
	{
	public:
		explicit ScrollSeekPanel(EAxis axis);

		std::shared_ptr<hecktui::Element> getPanel() const noexcept;

		virtual bool onEvent(const ConsoleEvent& e) override;

		virtual void update() override;

		void scrollToEnd();
		void scroll(int d);

		void setScrollAxisSizeOverride(std::optional<int>);

	private:
		EAxis mAxis{};
	};

	class HECKCONSOLEUI_EXPORT ResizableElement : public BoxPanel
	{
	public:
		explicit ResizableElement(EBorderStyle style);

		virtual bool onEvent(const ConsoleEvent&) override;

		bool hasHResize() const noexcept;
		bool hasVResize() const noexcept;

	protected:
		virtual LayoutSizeInfo measureThis() const noexcept override;

	private:
		ivec2 mMouseCaptureGrabDir{};
		ivec2 mMouseCaptureGrabPos{};
		bool mHasHResize = false;
		bool mHasVResize = false;
	};
}