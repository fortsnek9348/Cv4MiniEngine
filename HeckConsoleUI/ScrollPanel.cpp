#include "inc/HeckTextUI/BasicControls.h"

#include <algorithm>
#include <iostream>

using namespace hecktui;

namespace
{
	struct BareLayout : Layout
	{
		ivec2 scrollPosition{};
		std::optional<int> dimOverride[2];
		std::optional<int> heightOverride;

		bool limitWidth = false;
		bool limitHeight = false;

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override
		{
			LayoutSizeInfo info = children[0]->getLayoutSizeInfo();
			for (int i = 0; i < 2; ++i)
			{
				if (dimOverride[i])
				{
					info.minimum[i] = *dimOverride[i];
					info.preferred[i] = *dimOverride[i];
				}
			}
			return info;
		}
		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override
		{
			ivec2 panelSize = children[0]->getLayoutSizeInfo().preferred;
			//ivec2 usedScrollPosition = vmax(ivec2(), vmin(scrollPosition, panelSize - space));
			ivec2 usedScrollPosition = scrollPosition;

			// Always extend the panel to the given space if possible.
			panelSize += vmax(ivec2(), space - (-usedScrollPosition + panelSize));

			if (limitWidth)
			{
				usedScrollPosition.x = 0;
				panelSize.x = space.x;
			}
			
			if (limitHeight)
			{
				usedScrollPosition.y = 0;
				panelSize.y = space.y;
			}
			
			children[0]->setRect({
				.min = -usedScrollPosition,
				.max = -usedScrollPosition + panelSize,
				});
		}
	};
}

BareScrollPanel::BareScrollPanel()
{
	//addChild(std::make_shared<Element>(std::make_shared<FillLayout>()));
	addChild(std::make_shared<Element>());
	setLayout(std::make_unique<BareLayout>());
}

std::shared_ptr<hecktui::Element> BareScrollPanel::getPanel() const noexcept
{
	return getChildAt(0);
}

void BareScrollPanel::scroll(ivec2 d)
{
	BareLayout& layout = static_cast<BareLayout&>(*getLayout());

	//const ivec2 panelSize = getPanel()->getRect().size();
	//const ivec2 maxScroll = vmax(ivec2(), getRect().size() - panelSize);
	//layout.scrollPosition.x = (int)std::clamp<int64_t>(int64_t(layout.scrollPosition.x) + d.x, 0, maxScroll.x);
	//layout.scrollPosition.y = (int)std::clamp<int64_t>(int64_t(layout.scrollPosition.y) + d.y, 0, maxScroll.y);
	clampScrollPosition();
	layout.scrollPosition.x = (int)std::clamp<std::int64_t>(int64_t(layout.scrollPosition.x) + d.x, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
	layout.scrollPosition.y = (int)std::clamp<std::int64_t>(int64_t(layout.scrollPosition.y) + d.y, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
	clampScrollPosition();
}

void BareScrollPanel::setScrollPosition(ivec2 p)
{
	BareLayout& layout = static_cast<BareLayout&>(*getLayout());
	layout.scrollPosition = p;
}

void BareScrollPanel::clampScrollPosition()
{
	BareLayout& layout = static_cast<BareLayout&>(*getLayout());
	const ivec2 panelSize = getPanel()->getRect().size();
	const ivec2 maxScroll = panelSize - getRect().size();
	layout.scrollPosition = vmax(ivec2(), vmin(layout.scrollPosition, maxScroll));
}

void BareScrollPanel::setLayoutSizeOverride(std::optional<int> w, std::optional<int> h)
{
	BareLayout& layout = static_cast<BareLayout&>(*getLayout());
	layout.dimOverride[0] = w;
	layout.dimOverride[1] = h;
}

void BareScrollPanel::setPanelSizeLimitEnable(bool w, bool h)
{
	BareLayout& layout = static_cast<BareLayout&>(*getLayout());
	layout.limitWidth = w;
	layout.limitHeight = h;
}

ivec2 BareScrollPanel::getScrollPosition() const
{
	const BareLayout& layout = static_cast<BareLayout&>(*getLayout());
	return layout.scrollPosition;
}

namespace
{
	struct MyLayout : Layout
	{
		BareScrollPanel& bareScrollPanel;
		Scrollbar* hscrollbar = nullptr;
		Scrollbar* vscrollbar = nullptr;

		// Layout solving element state
		bool enableLayoutWithHScrollBar = false;
		bool enableLayoutWithVScrollBar = false;

		explicit MyLayout(BareScrollPanel& bareScrollPanel, Scrollbar* hscrollbar, Scrollbar* vscrollbar)
			: bareScrollPanel(bareScrollPanel), hscrollbar(hscrollbar), vscrollbar(vscrollbar)
		{
		}

		virtual std::optional<EAxis> getWrappingAxis() const noexcept override
		{
			// TODO: Not quite right for 2D scrolling, but it'll do for now.
			if (vscrollbar)
				return EAxis::Vertical;
			else if (hscrollbar)
				return EAxis::Horizontal;
			else
				return std::nullopt;
		}

		virtual bool layoutPrepareIteration(bool initial, ivec2 space) noexcept override
		{
			bool iterationNeeded = Layout::layoutPrepareIteration(initial, space);
			if (initial)
			{
				enableLayoutWithHScrollBar = false;
				enableLayoutWithVScrollBar = false;
				if (hscrollbar)
					hscrollbar->setVisible(false);
				if (vscrollbar)
					vscrollbar->setVisible(false);
			}
			else
			{
				const auto [showHBar, showVBar] = getBarShow(space, bareScrollPanel.getPanel()->getLayoutSizeInfo().preferred);
				//if (showHBar && showVBar && !enableLayoutWithHScrollBar && !enableLayoutWithVScrollBar)
				//{
				//	// For something like a table, it may be the case that with the given space, you need the H bar,
				//	// but enabling the H bar may also require the V bar because there is not yet enough space allocated for the H bar at the bottom.
				//	// So enable one at a time to work around this.
				//	// This happened with Info Screen / Statistics.
				//	// Possibly a hack.
				//	enableLayoutWithHScrollBar = true;
				//}
				//else
				{
					enableLayoutWithHScrollBar |= showHBar;
					enableLayoutWithVScrollBar |= showVBar;
				}

				iterationNeeded |= hscrollbar && hscrollbar->isThisVisible() != enableLayoutWithHScrollBar;
				iterationNeeded |= vscrollbar && vscrollbar->isThisVisible() != enableLayoutWithVScrollBar;
				if (hscrollbar)
					hscrollbar->setVisible(enableLayoutWithHScrollBar);
				if (vscrollbar)
					vscrollbar->setVisible(enableLayoutWithVScrollBar);
			}
			return iterationNeeded;
		}

		// TODO: Force layout visibility for non-visible controls.
		static constexpr int kHScrollBarThickness = 1;
		static constexpr int kVScrollBarThickness = 2;

		std::array<bool, 2> getBarShow(ivec2 space, ivec2 req) const
		{
			bool showHBar = hscrollbar && req.x > space.x;
			bool showVBar = vscrollbar && req.y > space.y;

			const bool newEnableLayoutWithHScrollBar = showHBar || enableLayoutWithHScrollBar;
			const bool newEnableLayoutWithVScrollBar = showVBar || enableLayoutWithVScrollBar;

			// Only check again if the first iteration didn't enable a scroll bar.
			// This is so that, for something like a table, it may be the case that with the given space, you need the H bar and not the V bar,
			// but enabling the H bar may also trigger the V bar because there is not yet enough space allocated for the H bar at the bottom.
			// So enable one at a time to work around this.
			// This happened with Info Screen / Statistics.
			if (newEnableLayoutWithHScrollBar != enableLayoutWithHScrollBar || newEnableLayoutWithVScrollBar != enableLayoutWithVScrollBar)
				return { showHBar, showVBar };

			showHBar |= hscrollbar && req.x > space.x - showVBar * kVScrollBarThickness;
			showVBar |= vscrollbar && req.y > space.y - showHBar * kHScrollBarThickness;
			
			//if ((showHBar || enableLayoutWithHScrollBar) == enableLayoutWithHScrollBar && (showVBar || enableLayoutWithVScrollBar) == enableLayoutWithVScrollBar)
			//{
			//	showHBar |= hscrollbar && req.x > space.x - showVBar * kVScrollBarThickness;
			//	showVBar |= vscrollbar && req.y > space.y - showHBar * kHScrollBarThickness;
			//}

			return { showHBar, showVBar };
		}

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override
		{
			const BareScrollPanel& viewport = static_cast<const BareScrollPanel&>(*children[0]);
			LayoutSizeInfo info = viewport.getLayoutSizeInfo();

			// Dynamically measure depending on given space, as-if this layout is a wrapping layout.
			//const auto [showHBar, showVBar] = getBarShow(measureSpace, info.preferred);
			const bool showHBar = enableLayoutWithHScrollBar;
			const bool showVBar = enableLayoutWithVScrollBar;

			if (showHBar)
				info.preferred.y += kHScrollBarThickness;
			if (showVBar)
				info.preferred.x += kVScrollBarThickness;

			//info.minimum = vmin(ivec2(
			//	vscrollbar ? kVScrollBarThickness + 1 : info.minimum.x,
			//	hscrollbar ? kHScrollBarThickness + 1 : info.minimum.y
			//), info.preferred);

			if (vscrollbar)
			{
				info.minimum.x = std::max(info.minimum.x, kVScrollBarThickness + 1);
				info.minimum.y = std::min(info.minimum.y, 2);
			}

			if (hscrollbar)
			{
				info.minimum.y = std::max(info.minimum.y, kHScrollBarThickness + 1);
				info.minimum.x = std::min(info.minimum.x, 2);
			}

			info.minimum = vmin(info.minimum, info.preferred);

			return info;
		}

		virtual void layoutPhase2Apply(ivec2 space, [[maybe_unused]] ChildList children) const override
		{
			//BareScrollPanel& viewport = static_cast<BareScrollPanel&>(*children[0]);

			const LayoutSizeInfo panelInfo = bareScrollPanel.getPanel()->getLayoutSizeInfo();
			//const LayoutSizeInfo hbarInfo = hscrollbar ? hscrollbar->getLayoutSizeInfo() : LayoutSizeInfo();
			//const LayoutSizeInfo vbarInfo = vscrollbar ? vscrollbar->getLayoutSizeInfo() : LayoutSizeInfo();

			// Use the container size, not viewport's current size!
			// This gets reduced by showing scrollbars.
			ivec2 viewSize = space;

			//const auto [showHBar, showVBar] = getBarShow(viewSize, panelInfo.preferred);
			
			
			if (enableLayoutWithHScrollBar)
				viewSize.y -= kHScrollBarThickness;
			if (enableLayoutWithVScrollBar)
				viewSize.x -= kVScrollBarThickness;

			if (hscrollbar)
			{
				//hscrollbar->setVisible(showHBar);
				hscrollbar->setContentSize(panelInfo.preferred.x);
				hscrollbar->setViewSize(viewSize.x);
			}

			if (vscrollbar)
			{
				//vscrollbar->setVisible(showVBar);
				vscrollbar->setContentSize(panelInfo.preferred.y);
				vscrollbar->setViewSize(viewSize.y);
			}

			const ivec2 scrollPosition{
				hscrollbar ? hscrollbar->getScrollPosition() : 0,
				vscrollbar ? vscrollbar->getScrollPosition() : 0,
			};

			bareScrollPanel.setScrollPosition(scrollPosition);
			
			bareScrollPanel.setRect({ .max = viewSize });
			if (hscrollbar)
				hscrollbar->setRect({ .min{ 0, space.y - kHScrollBarThickness }, .max{ viewSize.x, space.y } });
			if (vscrollbar)
				vscrollbar->setRect({ .min{ space.x - kVScrollBarThickness, 0 }, .max{ space.x, viewSize.y } });

			
		}
	};
}

ScrollBarPanel::ScrollBarPanel(bool enableHorizScrolling, bool enableVertScrolling) : mEnableHorizScrolling(enableHorizScrolling), mEnableVertScrolling(enableVertScrolling)
{
	// Okay, we'll let UIScene send wheel events to non-mIsMouseInteractable. That way, windows are draggable via scroll panels.
	//mIsMouseInteractable = true; // scroll wheel
	addChild(std::make_shared<BareScrollPanel>());
	static_cast<BareScrollPanel&>(*getChildAt(0)).setPanelSizeLimitEnable(!enableHorizScrolling, !enableVertScrolling);
	//static_cast<BareScrollPanel&>(*getChildAt(0)).setPanelSizeLimitEnable(true, false);

	auto hscrollbar = enableHorizScrolling ? std::make_shared<Scrollbar>(EAxis::Horizontal) : nullptr;
	if (enableHorizScrolling)
		addChild(hscrollbar);
	auto vscrollbar = enableVertScrolling ? std::make_shared<Scrollbar>(EAxis::Vertical) : nullptr;
	if (enableVertScrolling)
		addChild(vscrollbar);
	setLayout(std::make_unique<MyLayout>(static_cast<BareScrollPanel&>(*getChildAt(0)), hscrollbar.get(), vscrollbar.get()));
}

ScrollBarPanel::ScrollBarPanel(EAxis axis) : ScrollBarPanel(axis == EAxis::Horizontal, axis == EAxis::Vertical)
{
	mIsMouseInteractable = true;
}

std::shared_ptr<hecktui::Element> ScrollBarPanel::getPanel() const noexcept
{
	return static_cast<BareScrollPanel&>(*getChildAt(0)).getPanel();
}

bool ScrollBarPanel::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		ScrollBarPanel& self;

		bool operator()(const MouseScrollEvent& e) const
		{
			if (self.mEnableHorizScrolling || self.mEnableVertScrolling)
			{
				int delta = e.scroll * getDefaultVerticalScrollAmount();
				const EAxis scrollAxis = self.mEnableVertScrolling ? EAxis::Vertical : EAxis::Horizontal;
				if (scrollAxis == EAxis::Horizontal)
					static_cast<MyLayout&>(*self.getLayout()).hscrollbar->scrollDown(delta * 2);
				else
					static_cast<MyLayout&>(*self.getLayout()).vscrollbar->scrollDown(delta);
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

/*
bool ScrollBarPanel::layoutPrepareIteration(bool initial) noexcept
{
	auto& layout = static_cast<MyLayout&>(*getLayout());
	if (initial)
	{
		layout.enableLayoutWithHScrollBar = false;
		layout.enableLayoutWithVScrollBar = false;
		if (layout.hscrollbar)
			layout.hscrollbar->setVisible(false);
		if (layout.vscrollbar)
			layout.vscrollbar->setVisible(false);
		return Element::layoutPrepareIteration(initial);
	}
	else
	{
		const bool iterationNeeded = layout.hscrollbar && layout.hscrollbar->isThisVisible() != layout.enableLayoutWithHScrollBar
			|| layout.vscrollbar && layout.vscrollbar->isThisVisible() != layout.enableLayoutWithVScrollBar;
		const bool iterationNeededForBase = Element::layoutPrepareIteration(initial);
		if (layout.hscrollbar)
			layout.hscrollbar->setVisible(layout.enableLayoutWithHScrollBar);
		if (layout.vscrollbar)
			layout.vscrollbar->setVisible(layout.enableLayoutWithVScrollBar);
		return iterationNeeded || iterationNeededForBase;
	}
}*/

//bool ScrollBarPanel::layoutCheckNeedIterationForThis() const noexcept
//{
//	//const auto& layout = static_cast<MyLayout&>(*getLayout());
//	//return layout.hscrollbar && layout.hscrollbar->isThisVisible() != layout.enableLayoutWithHScrollBar
//	//	|| layout.vscrollbar && layout.vscrollbar->isThisVisible() != layout.enableLayoutWithVScrollBar
//	//	|| Element::layoutCheckNeedIterationForThis();
//	return false;
//}

void ScrollBarPanel::update()
{
	Element::update();
}

void ScrollBarPanel::scrollToEnd()
{
	auto& layout = static_cast<MyLayout&>(*getLayout());
	(layout.vscrollbar ? layout.vscrollbar : layout.hscrollbar)->scrollDown(std::numeric_limits<int>::max());
}

void ScrollBarPanel::setLayoutSizeOverride(std::optional<int> x, std::optional<int> y)
{
	static_cast<BareScrollPanel&>(*getChildAt(0)).setLayoutSizeOverride(x, y);
}

ivec2 ScrollBarPanel::getClampedScrollPosition() const
{
	//const auto& layout = static_cast<const MyLayout&>(*getLayout());
	//return {
	//	layout.hscrollbar ? layout.hscrollbar->getScrollPosition() : 0,
	//	layout.vscrollbar ? layout.vscrollbar->getScrollPosition() : 0,
	//};
	return static_cast<BareScrollPanel&>(*getChildAt(0)).getScrollPosition();
}

namespace
{
	struct SeekScrollLayout : TableLayout
	{
		using TableLayout::TableLayout;
	};
}

ScrollSeekPanel::ScrollSeekPanel(EAxis axis) : mAxis(axis)
{
	mIsMouseInteractable = true;

	// Okay, we'll let UIScene send wheel events to non-mIsMouseInteractable. That way, windows are draggable via scroll panels.
	//mIsMouseInteractable = true; // scroll wheel
	auto btn0 = std::make_shared<Button>(axis == EAxis::Horizontal ? L" < " : L"Ʌ", [this] {
		scroll(-getDefaultVerticalScrollAmount() * 2);
	});
	btn0->setBorderStyle(EBorderStyle::None);
	auto btn1 = std::make_shared<Button>(axis == EAxis::Horizontal ? L" > " : L"V", [this] {
		scroll(getDefaultVerticalScrollAmount() * 2);
	});
	btn1->setBorderStyle(EBorderStyle::None);
	addChild(std::move(btn0));
	addChild(std::make_shared<BareScrollPanel>());
	static_cast<BareScrollPanel&>(*getChildAt(1)).setPanelSizeLimitEnable(axis == EAxis::Vertical, axis == EAxis::Horizontal);
	addChild(std::move(btn1));
	
	TableLayoutConfig config{
		.cols{ {.weight = 1 } },
		.rows{ {},  {.weight = 1 }, {} },
		.cells{ {.coord{ 0, 0 } }, {.coord{ 0, 1 } }, {.coord{ 0, 2 } } },
	};
	if (axis != EAxis::Vertical)
	{
		std::swap(config.cols, config.rows);
		for (auto& cell : config.cells)
			cell.coord = cell.coord.flipped();
	}

	setLayout(std::make_unique<SeekScrollLayout>(std::move(config)));

	setScrollAxisSizeOverride(3);
}

std::shared_ptr<hecktui::Element> ScrollSeekPanel::getPanel() const noexcept
{
	return static_cast<BareScrollPanel&>(*getChildAt(1)).getPanel();
}

bool ScrollSeekPanel::onEvent(const ConsoleEvent& e)
{
	struct Handler
	{
		ScrollSeekPanel& self;

		bool operator()(const MouseScrollEvent& e) const
		{
			ivec2 d{};
			d[(int)self.mAxis] = e.scroll * getDefaultVerticalScrollAmount();
			static_cast<BareScrollPanel&>(*self.getChildAt(1)).scroll(d);
			return true;
		}

		bool operator()(const ConsoleEvent&) const
		{
			return false;
		}
	};

	return dispatch(e, Handler(*this));
}

void ScrollSeekPanel::update()
{
	Element::update();
	//const int viewportSize = getRect().size()[(int)mAxis] - getChildAt(0)->getRect().size()[(int)mAxis] - getChildAt(2)->getRect().size()[(int)mAxis];
	//mScrollPosition = std::clamp(mScrollPosition, 0, std::max(0, getPanel()->getRect().size()[(int)mAxis]));
}

void ScrollSeekPanel::scrollToEnd()
{
	scroll(std::numeric_limits<int>::max());
}

void ScrollSeekPanel::scroll(int d)
{
	ivec2 v{};
	v[(int)mAxis] = d;
	static_cast<BareScrollPanel&>(*getChildAt(1)).scroll(v);
}

void ScrollSeekPanel::setScrollAxisSizeOverride(std::optional<int> d)
{
	std::optional<int> dimOverride[2]{};
	dimOverride[(int)mAxis] = d;
	static_cast<BareScrollPanel&>(*getChildAt(1)).setLayoutSizeOverride(dimOverride[0], dimOverride[1]);
}
