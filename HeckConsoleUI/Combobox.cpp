#include "inc/HeckTextUI/Combobox.h"
#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"
#include "inc/HeckTextUI/Window.h"
#include "Util.h"

using namespace hecktui;

//
// /---------------------+---\   [silence multi-line comment warning]
// |Label                | v |
// \---------------------+---/
//
//
// [Label                | v ]
//

Combobox::Combobox(EComboboxStyle style) : mStyle(style)
{
	//auto layout = std::make_shared<FillLayout>();
	//layout->defaultAlign = RectJustilign{ EJustilign::End, EJustilign::Center };
	//layout->marginTopLeft = { 1, 0 };
	//setLayout(std::move(layout));
	//addChild(std::move(btn));

	setBorderStyle(style == EComboboxStyle::Bulky ? EBorderStyle::Rounded : EBorderStyle::Bracketed);
}

void Combobox::onClick(ModifierKeyState)
{
	if (mIsDropDownActive)
		mIsDropDownActive = false;
	else
		launchDropDown();
}

void Combobox::setListItems(std::vector<std::wstring> items)
{
	const bool isEmptySel = mSelectionI >= mItems.size();
	std::swap(mItems, items);
	mSelectionI = std::numeric_limits<std::size_t>::max();
	setSelectionIndex(isEmptySel || mItems.empty() ? 0 : std::min(mSelectionI, mItems.size() - 1));

	mMaxWidth = (int)std::ranges::max(mItems | std::views::transform([](std::wstring_view s) { return s.size(); }));
}

std::size_t Combobox::getSelectionIndex() const
{
	return mSelectionI;
}

void Combobox::setSelectionIndex(std::size_t i)
{
	if (mSelectionI != i)
	{
		mSelectionI = i;
		//onSelectionChanged();
	}
}

void Combobox::onSelectionChanged()
{
}

LayoutSizeInfo Combobox::measureThis() const
{
	LayoutSizeInfo sizeInfo = EmptyButton::measureThis();
	sizeInfo.minimum.x += mMaxWidth + 2;
	sizeInfo.preferred.x += mMaxWidth + 2;
	return sizeInfo;
}

void Combobox::drawThis(ivec2 offset, Framebuffer& fb)
{
	EmptyButton::drawThis(offset, fb);
	const iaabb2 rect = getRect() + offset;
	iaabb2 arrowRect = rect;
	arrowRect.min.x = arrowRect.max.x - 3;
	const PixelColouring colouring{ kTransparent, kTransparent };
	if (mStyle == EComboboxStyle::Bulky)
		fb.drawBox(arrowRect, EBorderStyle::Rounded, colouring);
	fb.drawText(L"▼", arrowRect, EAlign::Center, EAlign::Center, colouring, false);
	fb.drawText(mSelectionI < mItems.size() ? mItems[mSelectionI] : std::wstring_view(), rect.shrunk({ 1, 0 }), EAlign::Start, EAlign::Center, colouring, false);
}

struct Combobox::Internals
{
	struct DropDownItem : Button
	{
		Combobox& list;
		size_t index = 0;

		explicit DropDownItem(Combobox& list, size_t index) : Button(list.mItems[index]), list(list), index(index)
		{
			setBorderStyle(EBorderStyle::None);
			setLabelHAlign(EAlign::Left);
			//if (list.mSelectionI == index)
			//	setBackgroundColour(EColour::Grey11);
		}

		virtual void onClick(ModifierKeyState) override
		{
			list.setSelectionIndex(index);
			list.onSelectionChanged();
			list.mIsDropDownActive = false;
		}
	};

	struct DropDown : Window
	{
		Combobox& list;

		explicit DropDown(Combobox& list) : Window({}, {
			.isDefaultFocus = true,
			.isFocusContext = true,
			.borderStyle = EBorderStyle::None,
			}), list(list)
		{
			auto client = getClientArea();
			mBoxPanel = std::make_shared<BoxPanel>(EBorderStyle::Rounded);
			//boxPanel->enableTopBorder = false;
			//mBoxPanel->enableAutoMergeCardinal = false;
			auto scrollContainerPanel = std::make_shared<Element>();
			client->addChild(mBoxPanel);
			client->addChild(scrollContainerPanel);
			client->setLayout(std::make_unique<FillLayout>());
			auto scrollPanel = std::make_shared<ScrollBarPanel>(EAxis::Vertical);
			scrollContainerPanel->addChild(scrollPanel);
			{
				auto layout = std::make_unique<FillLayout>();
				mScrollContainerFillLayout = layout.get();
				layout->marginTopLeft = { 1, 0 };
				layout->marginBottomRight = { 1, 1 };
				scrollContainerPanel->setLayout(std::move(layout));
			}
			auto listPanel = scrollPanel->getPanel();
			for (const size_t i : range(list.mItems.size()))
				listPanel->addChild(std::make_shared<DropDownItem>(list, i));
			listPanel->setLayout(std::make_unique<FlowLayout>(FlowConfig{
				.axis = EAxis::Vertical,
				.itemsFlowwiseJustilign = EJustilign::Stretch,
				.linesCrosswiseJustilign = EJustilign::Stretch,
				}));
		}

		virtual void onWindowOpened() override
		{
			list.mIsDropDownActive = true;
		}

		virtual void onWindowClosed() override
		{
			list.mIsDropDownActive = false;
		}

		virtual void positionWindowInScene(ivec2 sceneDim) override
		{
			const iaabb2 boxRect = iaabb2::sized(list.getGlobalPosition(), list.getRect().size());
			const LayoutSizeInfo sizeInfo = getLayoutSizeInfo();
			int uiHeight = sizeInfo.preferred.y;

			const bool dropDown = boxRect.max.y + uiHeight <= sceneDim.y || sceneDim.y - boxRect.max.y >= boxRect.min.y;

			// Clamp.
			if (dropDown)
			{
				uiHeight = std::min(uiHeight, sceneDim.y - boxRect.max.y);
				
				// Dynamically adjust layout depending on whether we're above or below the combobox.
				mBoxPanel->enableTopBorder = false;
				mBoxPanel->enableBottomBorder = true;
				mScrollContainerFillLayout->marginTopLeft = { 1, 0 };
				mScrollContainerFillLayout->marginBottomRight = { 1, 1 };
			}
			else
			{
				uiHeight = std::min(uiHeight, boxRect.min.y);
				
				mBoxPanel->enableTopBorder = true;
				mBoxPanel->enableBottomBorder = false;
				mScrollContainerFillLayout->marginTopLeft = { 1, 1 };
				mScrollContainerFillLayout->marginBottomRight = { 1, 0 };
			}

			const ivec2 uiPosition(boxRect.min.x, dropDown ? boxRect.max.y : boxRect.min.y - uiHeight);

			setRect(iaabb2::sized(uiPosition, { boxRect.size().x, uiHeight }));
		}

		virtual bool wantClose(const Element* focus) const noexcept override
		{
			return (!contains(focus) && !list.contains(focus)) || !list.mIsDropDownActive;
		}

	private:
		std::shared_ptr<BoxPanel> mBoxPanel;
		FillLayout* mScrollContainerFillLayout = nullptr;
	};
};

void Combobox::launchDropDown()
{
	deferContextWindow(std::make_shared<Internals::DropDown>(*this));
}