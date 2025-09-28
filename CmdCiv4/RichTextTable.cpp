#include "RichTextTable.h"
#include "TuiTextCode.h"
#include "MainInterfaceControls.h"
#include "Common.h"

#include <HeckTextUI/BasicControls.h>

#include <CommonStuff/range.h>

#include <ranges>
#include <iostream>

using namespace hecktui;
using heck::range;

constexpr int kVScrollBarWidth = 2;

struct RichTextTable::BareScrollableTable : Element
{
	//std::shared_ptr<ScrollBarPanel> scrollPanel = std::make_shared<ScrollBarPanel>(true, true);
	// Header must sync with scrollPanel H scroll.

	explicit BareScrollableTable(const RichTextTable& table, std::shared_ptr<Element> header, std::shared_ptr<Element> tableDataPanel)
	{
		addChild(std::move(header));
		const auto scrollPanel = std::make_shared<ScrollBarPanel>(true, true);
		addChild(scrollPanel);
		setLayout(std::make_unique<MyLayout>(table));

		scrollPanel->getPanel()->addChild(std::move(tableDataPanel));
		scrollPanel->getPanel()->setLayout(std::make_unique<FillLayout>());
	}

	Element& getHeader() const
	{
		return *getChildAt(0);
	}

	struct MyLayout : Layout
	{
		const RichTextTable& table;

		explicit MyLayout(const RichTextTable& table) : table(table)
		{
		}

		virtual bool layoutPrepareIteration([[maybe_unused]] bool initial, [[maybe_unused]] ivec2 space) noexcept override
		{
			//if (!initial)
			//{
			//	auto& header = *table.getChildAt(0)->getChildAt(0);
			//	auto& scrollPanel = static_cast<ScrollBarPanel&>(*table.getChildAt(0)->getChildAt(1));
			//
			//	const int hscrollPosition = scrollPanel.getClampedScrollPosition().x;
			//
			//	//std::clog << hscrollPosition << std::endl;
			//
			//	const LayoutSizeInfo headerInfo = header.getLayoutSizeInfo();
			//
			//	const int tableViewWidth = space.x - kVScrollBarWidth; // HACK: Subtract off vscrollbar width to get space for TableDataPanel.
			//	const int y = headerInfo.preferred.y;
			//
			//	// Do this here rather than in layoutPhase2Apply, to get the final scroll position.
			//	header.setRect(iaabb2::sized({ -hscrollPosition, 0 }, { std::max(headerInfo.preferred.x, hscrollPosition + tableViewWidth), y }));
			//}
			return false;
		}

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override
		{
			LayoutSizeInfo headerInfo = children[0]->getLayoutSizeInfo();
			LayoutSizeInfo info = children[1]->getLayoutSizeInfo();
			info.minimum.y += headerInfo.minimum.y;
			info.preferred.y += headerInfo.preferred.y;
			return info;
		}

		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override
		{
			auto& header = *children[0];
			auto& scrollPanel = static_cast<ScrollBarPanel&>(*children[1]);

			const int hscrollPosition = scrollPanel.getClampedScrollPosition().x;
			//
			////std::clog << hscrollPosition << std::endl;
			//
			const LayoutSizeInfo headerInfo = header.getLayoutSizeInfo();
			//
			const int tableViewWidth = space.x - kVScrollBarWidth; // HACK: Subtract off vscrollbar width to get space for TableDataPanel.
			const int y = headerInfo.preferred.y;
			//
			header.setRect(iaabb2::sized({ -hscrollPosition, 0 }, { std::max(headerInfo.preferred.x, hscrollPosition + tableViewWidth), y }));
			// Expand last column.
			//header.setRect(iaabb2::sized({ -hscrollPosition, 0 }, { space.x + hscrollPosition,headerInfo.preferred.y }));
			scrollPanel.setRect({ .min{ 0, y }, .max = space });
		}
	};
};


static constexpr int kColGap = 1;

struct RichTextTable::HeaderPanel : Element
{
	explicit HeaderPanel(const RichTextTable& table)
	{
		setLayout(std::make_unique<MyLayout>(table));
	}

	// TODO: Really need to add gap support to FlowLayout.
	struct MyLayout : Layout
	{
		const RichTextTable& table;

		explicit MyLayout(const RichTextTable& table) : table(table)
		{
		}

		virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override
		{
			LayoutSizeInfo info{};
			
			for (const auto& child : children)
			{
				const LayoutSizeInfo childInfo = child->getLayoutSizeInfo();
				info.minimum.y = std::max(info.minimum.y, childInfo.minimum.y);
				info.preferred.y = std::max(info.preferred.y, childInfo.preferred.y);
			}

			int x = 0;
			for (const auto& [colI, child] : children | std::views::enumerate)
			{
				const LayoutSizeInfo childInfo = child->getLayoutSizeInfo();
				const int width = std::max(childInfo.preferred.x, table.mColumns[colI].contentMaxWidth);
				x += width + kColGap;
			}

			info.minimum.x = info.preferred.x = std::max(0, x - kColGap);

			if (!table.mShowHeader)
			{
				info.minimum.y = 0;
				info.preferred.y = 0;
			}

			return info;
		}

		const HeaderPanel& getHeaderPanel() const
		{
			return static_cast<const HeaderPanel&>(static_cast<BareScrollableTable&>(*table.getChildAt(0)).getHeader());
		}

		virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override
		{
			const int extraSpace = std::max(0, space.x - getHeaderPanel().getLayoutSizeInfo().minimum.x);
			int x = 0;
			for (const auto& [colI, child] : children | std::views::enumerate)
			{
				const LayoutSizeInfo childInfo = child->getLayoutSizeInfo();
				const bool isExpandCol = (size_t)colI == table.mExpandColI;
				const int width = std::max(childInfo.preferred.x, table.mColumns[colI].contentMaxWidth) + (isExpandCol ? extraSpace : 0);
				child->setRect(iaabb2::sized({ x, 0 }, { width, childInfo.preferred.y }));
				x += width + kColGap;
			}
		}
	};
};

struct RichTextTable::TableDataPanel : Element
{
	RichTextTable& table;

	explicit TableDataPanel(RichTextTable& table) : table(table)
	{
		mCanFocus = true;
		mIsMouseInteractable = true;
	}

	const HeaderPanel& getHeaderPanel() const
	{
		return static_cast<const HeaderPanel&>(static_cast<BareScrollableTable&>(*table.getChildAt(0)).getHeader());
	}

	virtual bool onEvent(const ConsoleEvent& e) override
	{
		if (!table.mEnableSelection)
			return false;

		if (e.type == EConsoleEventType::MouseButtonDown)
		{
			const auto& e2 = static_cast<const MouseButtonEvent&>(e);
			if (e2.button == EMouseButton::Left)
			{
				const bool multiSelect = table.mEnableMultiSelect && e2.ctrl;
				const bool rangeSelect = table.mEnableMultiSelect && e2.shift;
				const size_t virtualRowI = e2.coord.y;

				table.mActiveRowI = SIZE_MAX;

				if (virtualRowI < table.mRows.size())
				{
					table.updateSortedRowList();

					const size_t physicalRowI = table.mSortedRowList[virtualRowI];

					const bool isSelected = table.mSelectedRows.contains(physicalRowI);
					if (rangeSelect)
					{
						// Translate physical mSelectionAnchorRowI to virtual.
						size_t virtualAnchorRowI = std::ranges::find(table.mSortedRowList, table.mSelectionAnchorRowI) - table.mSortedRowList.begin();
						if (virtualAnchorRowI >= table.mRows.size())
						{
							virtualAnchorRowI = 0;
							table.mSelectionAnchorRowI = table.mSortedRowList[0];
						}

						if (!multiSelect)
							table.mSelectedRows.clear();
						table.mSelectedRows.insert_range(range(std::min(virtualRowI, virtualAnchorRowI), std::max(virtualRowI, virtualAnchorRowI) + 1)
							| std::views::transform([this](size_t virtualRowI) { return table.mSortedRowList[virtualRowI]; }));
					}
					else
					{
						table.mSelectionAnchorRowI = physicalRowI;

						if (multiSelect)
						{
							if (isSelected)
								table.mSelectedRows.erase(physicalRowI);
							else
							{
								table.mSelectedRows.insert(physicalRowI);
								table.mActiveRowI = physicalRowI;
							}
						}
						else
						{
							table.mSelectedRows.clear();
							table.mSelectedRows.insert(physicalRowI);
							table.mActiveRowI = physicalRowI;
						}
					}

					(void)doPythonScreenEventHandling(table.mWidgetData, table.mPythonScreenControlId,
						CvEngineEnums::NOTIFY_LISTBOX_ITEM_SELECTED, CvEngineEnums::MOUSE_LBUTTON | CvEngineEnums::MOUSE_LBUTTONDOWN);
				}

				return true;
			}
		}

		return false;
	}

	virtual LayoutSizeInfo measureThis() const override
	{
		ivec2 size{
			std::max(0, int(table.mColumns.size()) - 1) * kColGap,
			int(table.mRows.size()),
		};

		const HeaderPanel& headerPanel = getHeaderPanel();

		for (const auto& [colI, col] : table.mColumns | std::views::enumerate)
			size.x += std::max(col.contentMaxWidth, headerPanel.getChildAt(colI)->getLayoutSizeInfo().preferred.x);

		// Small min size so that tables don't eat up autosize row height in the corp screen.
		//return { .minimum{ 3, 3 }, .preferred = size };
		return { size, size };
	}
	virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
	{
		const iaabb2 globalRect = offset + getRect();
		const iaabb2 scissorRect = fb.getScissorRect();

		const int numTableRows = (int)table.mRows.size();
		const int firstVisibleRowI = std::max(0, scissorRect.min.y - globalRect.min.y);
		const int firstBelowVisibleRowI = std::max(0, scissorRect.max.y - globalRect.min.y);
		const int numVisibleRows = std::min(firstBelowVisibleRowI, numTableRows) - std::min(firstVisibleRowI, numTableRows);

		const HeaderPanel& headerPanel = getHeaderPanel();

		table.updateSortedRowList();

		for (const int displayRowI : range(firstVisibleRowI, firstVisibleRowI + numVisibleRows))
		{
			const size_t rowI = table.mSortedRowList[displayRowI];
			const Colour backColour =
				rowI == table.mActiveRowI ? EColour::Grey82 :
				table.mSelectedRows.contains(rowI) ? EColour::Grey23 : hecktui::kTransparent;
			const Colour foreColour =
				rowI == table.mActiveRowI ? EColour::Black : EColour::Silver;

			const PixelColouring defColouring{ .text = foreColour, .back = backColour };

			const Row& row = table.mRows[rowI];
			for (const auto [colI, pixelsIndexSpan] : row.colStarts | std::views::pairwise | std::views::enumerate)
			{
				const iaabb2 headerRect = headerPanel.getChildAt(colI)->getRect();
				const int x = headerRect.min.x;
				if (table.mShowColLines && colI > 0)
					fb.draw(globalRect.min + ivec2(x - 1, displayRowI), { .c = L'│', .colour = defColouring  });

				fb.drawHLine(globalRect.min + ivec2(x, displayRowI), globalRect.size().x, { .c = L' ', .colour = defColouring });
				fb.blit(iaabb2::sized(globalRect.min + ivec2(x, displayRowI), { headerRect.size().x, 1 }),
					row.getCellPixels(colI), table.mColumns[colI].halign, EAlign::Center, false);
			}
		}
	}
};

RichTextTable::RichTextTable(CvWidgetDataStruct widgetData, PythonScreenControlId pythonScreenControlId)
	: mWidgetData(widgetData), mPythonScreenControlId(std::move(pythonScreenControlId))
{
	addChild(std::make_shared<BareScrollableTable>(*this, std::make_shared<HeaderPanel>(*this), std::make_shared<TableDataPanel>(*this)));
	setLayout(std::make_unique<FillLayout>());
}

void RichTextTable::addColumn(std::wstring_view titleTextcode)
{
	Column col{
		.headerButton = makeActionButtonWithManualLabel(titleTextcode, {.m_iData1{}, .m_iData2{}, .m_bOption{}, .m_eWidgetType = WIDGET_GENERAL }, [colI = mColumns.size(), this] {
			toggleSort(colI);
		}),
		.sortLabelCtrl = std::make_shared<Label>(),
	};

	col.sortLabelCtrl->setLabelAlignment(EAlign::Right);
	col.headerButton->addChild(col.sortLabelCtrl);

	auto& header = static_cast<HeaderPanel&>(static_cast<BareScrollableTable&>(*getChildAt(0)).getHeader());
	header.addChild(col.headerButton);

	mColumns.push_back(std::move(col));
}

void RichTextTable::setColumnAlign(size_t i, EAlign halign)
{
	mColumns[i].halign = halign;
}

void RichTextTable::setExpandColumn(size_t i)
{
	mExpandColI = i;
}

void RichTextTable::clearRows()
{
	mRows.clear();
	mActiveRowI = SIZE_MAX;
	mSelectionAnchorRowI = SIZE_MAX;
	mSelectedRows.clear();
	for (auto& col : mColumns)
		col.contentMaxWidth = 0;
	mSortedRowList.clear();
}
void RichTextTable::addRow(std::span<const std::wstring_view> cells, std::vector<int> sortIntegers)
{
	Row row{
		.sortIntegers = std::move(sortIntegers)
	};
	row.colStarts.reserve(cells.size() + 1);

	for (const auto& [colI, textcode] : cells | std::views::enumerate)
	{
		row.colStarts.push_back(row.pixels.size());
		row.pixels.append_range(renderCiv4TextCode(textcode, { .colour{ .text = kTransparent, .back = kTransparent } }));
		const int width = int(row.pixels.size() - row.colStarts.back());
		mColumns[colI].contentMaxWidth = std::max(mColumns[colI].contentMaxWidth, width);
	}

	row.colStarts.push_back(row.pixels.size());

	mRows.push_back(std::move(row));

	mSortedRowList.clear();
}

size_t RichTextTable::getNumRows() const
{
	return mRows.size();
}

const std::unordered_set<size_t>& RichTextTable::getSelectedRowsSet() const
{
	return mSelectedRows;
}

size_t RichTextTable::getActiveRowIndex() const
{
	return mActiveRowI;
}

void RichTextTable::setSelectedRows(std::span<const int> indices, size_t activeRowI)
{
	mSelectedRows.clear();
	mSelectedRows.insert_range(indices | std::views::transform([](int i) { return size_t(i); }));
	mActiveRowI = activeRowI;
}

void RichTextTable::setTableFlags(bool showHeader, bool enableSelection, bool enableMultiSelect, bool showColLines)
{
	enableMultiSelect &= enableSelection;

	mShowHeader = showHeader;
	mEnableSelection = enableSelection;
	mEnableMultiSelect = enableMultiSelect;
	mShowColLines = showColLines;

	if (!showHeader)
	{
		mSortMode = 0;
		mSortedRowList.clear();
	}

	if (!enableMultiSelect)
		if (mSelectedRows.size())
			mSelectedRows = { *mSelectedRows.begin() };

	if (!enableSelection)
	{
		mSelectedRows.clear();
		mActiveRowI = SIZE_MAX;
	}
}

void RichTextTable::toggleSort(size_t colI)
{
	if (mSortColumnI < mColumns.size())
		mColumns[mSortColumnI].sortLabelCtrl->setLabel({});

	if (colI == mSortColumnI)
		mSortMode = (mSortMode + 1) % 3;
	else
		mSortMode = 1;

	mSortColumnI = colI;

	if (mSortColumnI < mColumns.size())
		mColumns[mSortColumnI].sortLabelCtrl->setLabel(mSortMode <= 0 ? L"" : mSortMode <= 1 ? L"▲" : L"▼");

	mSortedRowList.clear();
}

std::span<const hecktui::Pixel> RichTextTable::Row::getCellPixels(size_t colI) const
{
	const size_t pixelsStartI = colStarts[colI];
	const size_t pixelsEndI = colStarts[colI + 1];
	return std::span(pixels).subspan(pixelsStartI, pixelsEndI - pixelsStartI);
}

void RichTextTable::updateSortedRowList()
{
	if (mSortedRowList.size() != mRows.size())
	{
		mSortedRowList.assign_range(range(mRows.size()));

		const auto cmpRows = [&, colI = mSortColumnI](size_t a, size_t b) {
			// Compare as (sortInteger, text).

			const int aInt = mRows[a].sortIntegers[colI];
			const int bInt = mRows[b].sortIntegers[colI];

			if (aInt != bInt)
				return aInt <=> bInt;

			const auto aText = mRows[a].getCellPixels(colI) | std::views::transform(&hecktui::Pixel::c);
			const auto bText = mRows[b].getCellPixels(colI) | std::views::transform(&hecktui::Pixel::c);
			return std::lexicographical_compare_three_way(aText.begin(), aText.end(), bText.begin(), bText.end());
		};

		switch (mSortMode)
		{
		case 0:
		default:
			break;
		case 1:
			std::ranges::stable_sort(mSortedRowList, [&](size_t a, size_t b) { return cmpRows(a, b) < 0; });
			break;
		case 2:
			std::ranges::stable_sort(mSortedRowList, [&](size_t a, size_t b) { return cmpRows(a, b) > 0; });
			break;
		}
	}
}