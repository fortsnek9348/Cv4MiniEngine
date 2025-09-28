#include "inc/HeckTextUI/Layout.h"
#include "inc/HeckTextUI/Control.h"
#include "Util.h"

#include <algorithm>
#include <ranges>
#include <numeric>
#include <iostream>

using namespace hecktui;

namespace
{
	struct HFlowLayoutMeasurer
	{
		FlowConfig config{};
		int wrapWidth{};

		// If wrapping, minimum size is one child per row and preferred is all on one row. Leave it up to phase 2 to handle whatever we end up with.
		LayoutSizeInfo layoutSizeInfo{};
		//LayoutSizeInfo rowSizeInfo{};
		//int rowI = 0;

		struct WrappingMeasurer
		{
			ivec2 nextPosition{};
			int rowHeight = 0;

			void add(ivec2 size, int wrapWidth)
			{
				const bool canWrap = nextPosition.x > 0;
				if (nextPosition.x + size.x > wrapWidth && canWrap)
				{
					nextPosition.x = 0;
					nextPosition.y += rowHeight;
					rowHeight = 0;
				}
				nextPosition.x += size.x;
				rowHeight = std::max(rowHeight, size.y);
			}
		};

		WrappingMeasurer minimumWrappingMeasurer{};
		WrappingMeasurer preferredWrappingMeasurer{};

		void add(LayoutSizeInfo info)
		{
			if (config.wrap)
			{
				// Layout as-if overlapped (massively underestimate).
				//layoutSizeInfo.preferred = vmax(layoutSizeInfo.preferred, info.preferred);
				//layoutSizeInfo.minimum = vmax(layoutSizeInfo.minimum, info.minimum);

				// Layout according to space.
				minimumWrappingMeasurer.add(info.minimum, wrapWidth);
				preferredWrappingMeasurer.add(info.preferred, wrapWidth);

				layoutSizeInfo.minimum.x = std::max(layoutSizeInfo.minimum.x, minimumWrappingMeasurer.nextPosition.x);
				layoutSizeInfo.preferred.x = std::max(layoutSizeInfo.preferred.x, preferredWrappingMeasurer.nextPosition.x);
				layoutSizeInfo.minimum.y = minimumWrappingMeasurer.nextPosition.y + minimumWrappingMeasurer.rowHeight;
				layoutSizeInfo.preferred.y = preferredWrappingMeasurer.nextPosition.y + preferredWrappingMeasurer.rowHeight;
			}
			else
			{
				// Layout on one row.
				layoutSizeInfo.preferred.x += info.preferred.x;
				layoutSizeInfo.minimum.x += info.minimum.x;
				layoutSizeInfo.preferred.y = std::max(layoutSizeInfo.preferred.y, info.preferred.y);
				layoutSizeInfo.minimum.y = std::max(layoutSizeInfo.minimum.y, info.minimum.y);
			}
		}

		void finalise()
		{
			//layoutSizeInfo.preferred = layoutSizeInfo.minimum;
			//layoutSizeInfo.preferred.y += 3;
		}

		//void nextRow()
		//{
		//	layoutSizeInfo.minimum.x = std::max(layoutSizeInfo.minimum.x, rowSizeInfo.minimum.x);
		//	layoutSizeInfo.preferred.x = std::max(layoutSizeInfo.preferred.x, rowSizeInfo.preferred.x);
		//	layoutSizeInfo.minimum.y += rowSizeInfo.minimum.y;
		//	layoutSizeInfo.preferred.y += rowSizeInfo.preferred.y;
		//	rowSizeInfo = {};
		//	++rowI;
		//}
	};

	

	size_t countElementsOnRow(bool wrap, int& space, std::span<const LayoutSizeInfo> childSizeInfos, ivec2 LayoutSizeInfo::* sizeInfoMember)
	{
		size_t i = 0;
		for (; i < childSizeInfos.size(); ++i)
		{
			const int req = (childSizeInfos[i].*sizeInfoMember).x;
			if (i == 0 || req <= space || !wrap)
				space -= req;
			else
				break;
		}
		return i;
	}

	// Justify or align, and do min-pref lerping.
	template<class T>
	void justilignItems(std::span<T> items, int T::* outPosition, int T::* outSize, const int T::* miniSize, const int T::* prefSize, int space, EJustilign justilign)
	{
		if (items.empty())
			return;

		const int miniUsage = sum(items, 0, miniSize);
		const int prefUsage = sum(items, 0, prefSize);

		if (space <= prefUsage)
			justilign = EJustilign::Stretch;

		switch (justilign)
		{
		default:
			justilign = EJustilign::Start;
			[[fallthrough]];
		case EJustilign::Start:
		case EJustilign::Center:
		case EJustilign::End:
		{
			// There is enough space. prefRemaining > 0.
			int prefRemaining = space - prefUsage;
			int x = alignShift(prefRemaining, EAlign(justilign));

			for (auto&& [i, item] : items | std::views::enumerate)
			{
				item.*outPosition = x;
				item.*outSize = item.*prefSize;
				x += item.*outSize;
			}

			break;
		}
		case EJustilign::Stretch:
		{
			if (space <= miniUsage)
			{
				// All items will have to be scaled below individual min sizes.
				const Rational scale{
					space,
					std::max(1, miniUsage),
				};
				
				int remainder = 0;
				int x = 0;

				for (auto&& [i, item] : items | std::views::enumerate)
				{
					item.*outPosition = x;
					item.*outSize = scale.mul(item.*miniSize, remainder);
					x += item.*outSize;
				}
			}
			else if (prefUsage <= space)
			{
				// All items are to be scaled up from preferred size.
				const int prefRemaining = space - prefUsage;
				const int extraSizePerItem = int(prefRemaining / items.size());
				const int extraUnits = int(prefRemaining % items.size());

				int x = 0;

				for (auto&& [i, item] : items | std::views::enumerate)
				{
					item.*outPosition = x;
					item.*outSize = item.*prefSize;
					item.*outSize += extraSizePerItem;
					item.*outSize += i < extraUnits;
					x += item.*outSize;
				}
			}
			else
			{
				// All items will need to be lerped between minimum and preferred size.
				const Rational lerpFactor{
					space - miniUsage,
					std::max(1, prefUsage - miniUsage),
				};

				int remainder = 0;
				int x = 0;

				for (auto&& [i, item] : items | std::views::enumerate)
				{
					item.*outPosition = x;
					item.*outSize = lerpFactor.lerp(item.*miniSize, item.*prefSize, remainder);
					x += item.*outSize;
				}
			}

			break;
		}
		case EJustilign::SpaceBetween:
		{
			if (items.size() <= 1)
				break;

			const int prefRemaining = space - prefUsage;
			const int extraSizePerItem = int(prefRemaining / (items.size() - 1));
			const int extraUnits = int(prefRemaining % (items.size() - 1));

			int x = 0;

			for (auto&& [i, item] : items | std::views::enumerate)
			{
				item.*outPosition = x;
				item.*outSize = item.*prefSize;
				const int gap = extraSizePerItem + (i < extraUnits);
				x += item.*outSize + gap;
			}

			break;
		}
		}
	}

	struct RowInfo
	{
		int y = 0;
		int height = 0;
		int minHeight = 0;
		int prefHeight = 0;
		int remainingWidth = 0;
		size_t firstChildI = 0;
		size_t numChildren = 0;
	};

	

	struct LayoutResult
	{
		int remainingWidth = 0;
		int minHeight = 0;
		int prefHeight = 0;
		std::vector<RowInfo> rows;
	};

	

	LayoutResult layoutRough(bool wrap, std::span<const LayoutSizeInfo> childSizeInfos, ivec2 space,
		ivec2 LayoutSizeInfo::* xSizeInfoMember
	)
	{
		LayoutResult result;

		//int rowMinY = 0;
		//int rowPrefY = 0;
		int maxRemainingWidth = std::numeric_limits<int>::max();

		for (size_t childI = 0; childI < childSizeInfos.size(); )
		{
			RowInfo rowInfo{
				//.y = rowY,
				.remainingWidth = space.x,
				.firstChildI = childI,
				.numChildren = countElementsOnRow(wrap, rowInfo.remainingWidth, childSizeInfos.subspan(childI), xSizeInfoMember),
			};

			const int rowHeightMin = std::ranges::max(childSizeInfos.subspan(childI, rowInfo.numChildren) | std::views::transform([=](const LayoutSizeInfo& info) {
				return info.minimum.y;
			}));

			const int rowHeightPref = std::ranges::max(childSizeInfos.subspan(childI, rowInfo.numChildren) | std::views::transform([=](const LayoutSizeInfo& info) {
				return info.preferred.y;
			}));

			result.minHeight += rowHeightMin;
			result.prefHeight += rowHeightPref;

			rowInfo.y = 0; // dummy value until final layout
			rowInfo.height = 0; // dummy value until final layout
			rowInfo.minHeight = rowHeightMin;
			rowInfo.prefHeight = rowHeightPref;

			result.rows.push_back(rowInfo);

			maxRemainingWidth = std::min(maxRemainingWidth, rowInfo.remainingWidth);
			//rowMinY += rowHeightMin;
			//rowPrefY += rowHeightPref;

			childI += rowInfo.numChildren;
		}

		result.remainingWidth = maxRemainingWidth;
		return result;
	}

	//struct RowCtrlLayoutInfo
	//{
	//	int x = 0;
	//	int width = 0;
	//};
	//
	//void layoutFinal(const FlowConfig& config, LayoutResult rough, Layout::ChildList childList,
	//	std::span<const LayoutSizeInfo> childSizeInfos)
	//{
	//	const ivec2 LayoutSizeInfo::* const widthSizeInfoMember = rough.widthSizeInfoMember;
	//	const ivec2 LayoutSizeInfo::* const heightSizeInfoMember = rough.heightSizeInfoMember;
	//
	//	justify(std::span(rough.rows), &RowInfo::y, &RowInfo::height, rough.remainingHeight, config.justilignOfLinesAcrossContainer);
	//
	//	std::vector<RowCtrlLayoutInfo> rowCtrlLayoutInfos;
	//	rowCtrlLayoutInfos.reserve(childList.size());
	//
	//	for (const RowInfo& row : rough.rows)
	//	{
	//		rowCtrlLayoutInfos.assign_range(childSizeInfos.subspan(row.firstChildI, row.numChildren) | std::views::transform([=](const LayoutSizeInfo& info) {
	//			return RowCtrlLayoutInfo{
	//				.width = (info.*widthSizeInfoMember).x
	//			};
	//		}));
	//
	//		// Justify within layout rect.
	//		justify(std::span(rowCtrlLayoutInfos), &RowCtrlLayoutInfo::x, &RowCtrlLayoutInfo::width, row.remainingWidth, config.justilignOfItemsAlongLine);
	//
	//		for (const size_t colI : range(row.numChildren))
	//		{
	//			RowInfo tempHeightInfo{
	//				.height = (childSizeInfos[row.firstChildI + colI].*heightSizeInfoMember).y,
	//			};
	//			justify(std::span(&tempHeightInfo, 1), &RowInfo::y, &RowInfo::height, row.height - tempHeightInfo.height, config.justilignOfItemsAcrossLine);
	//
	//			iaabb2 rect{};
	//			rect.min.x = rowCtrlLayoutInfos[colI].x;
	//			rect.max.x = rect.min.x + rowCtrlLayoutInfos[colI].width;
	//			rect.min.y = row.y + tempHeightInfo.y;
	//			rect.max.y = rect.min.y + tempHeightInfo.height;
	//			childList[row.firstChildI + colI]->setRect(rect);
	//		}
	//	}
	//}
}

FlowLayout::FlowLayout(FlowConfig config) : config(std::move(config))
{
}

std::optional<EAxis> FlowLayout::getWrappingAxis() const noexcept
{
	if (config.wrap)
		return config.axis;
	else
		return std::nullopt;
}

bool FlowLayout::layoutPrepareIteration(bool initial, ivec2 space) noexcept
{
	bool needsIteration = Layout::layoutPrepareIteration(initial, space);
	if (config.wrap)
	{
		if (initial)
			mLayoutWrappingSpace = INT_MAX;
		else
		{
			const auto wrapCoord = config.axis == EAxis::Horizontal ? &ivec2::x : &ivec2::y;
			needsIteration |= space.*wrapCoord < mLayoutWrappingSpace;
			mLayoutWrappingSpace = std::min(mLayoutWrappingSpace, space.*wrapCoord);
		}
	}
	return needsIteration;
}

LayoutSizeInfo FlowLayout::layoutPhase1Measure(ChildList children) const
{
	// todo: layoutLineJustify center

	//if (debug)
	//	_CrtDbgBreak();

	HFlowLayoutMeasurer measurer{ config, mLayoutWrappingSpace };
	//HFlowLayoutMeasurer measurer{ config, 9999 };

	for (const auto& child : children)
	{
		LayoutSizeInfo childInfo = child->getLayoutSizeInfo();
		if (config.axis == EAxis::Vertical)
		{
			childInfo.minimum = childInfo.minimum.flipped();
			childInfo.preferred = childInfo.preferred.flipped();
		}
		measurer.add(childInfo);
	}

	measurer.finalise();

	if (config.axis == EAxis::Vertical)
	{
		measurer.layoutSizeInfo.minimum = measurer.layoutSizeInfo.minimum.flipped();
		measurer.layoutSizeInfo.preferred = measurer.layoutSizeInfo.preferred.flipped();
	}

	// TODO: Could be more accurate.
	if (config.wrap)
		measurer.layoutSizeInfo.minimum = 1;

	return measurer.layoutSizeInfo;
}
void FlowLayout::layoutPhase2Apply(ivec2 space, ChildList children) const
{
	ivec2 hsize = space;

	//if (debug)
	//	std::clog << "Flow debug" << std::endl;

	if (children.empty())
		return;

	std::vector<LayoutSizeInfo> childSizeInfos(std::from_range, children | std::views::transform([](const std::shared_ptr<Element>& child) {
		return child->getLayoutSizeInfo();
	}));

	if (config.axis == EAxis::Vertical)
	{
		std::swap(hsize.x, hsize.y);
		for (auto& info : childSizeInfos)
		{
			std::swap(info.minimum.x, info.minimum.y);
			std::swap(info.preferred.x, info.preferred.y);
		}
	}

	/**
	* Try rough layout with both minimum and preferred width and height.
	* If not wrapped or wrapped and all controls fit on one row using preferred width:
	*     If space <= minimum, squash controls equally.
	*     If minimum < space < preferred, proportionally scale controls between individual minimum and preferred to hit target space exactly.
	*     If preferred <= space, use preferred widths, then justify.
	*     Clamp height to space, then justify height.
	* Else, multi-line wrapped:
	*     Use minimum width for wrapping.
	*     If space.y <= minimum.y, scale row hwights equally (then clamp control heights).
	*     If minimum.y < space.y < preferred.y, proportionally scale row heights between individual minimum and preferred to hit target space exactly.
	*     If preferred.y <= space.y, use preferred heights, then justify.
	* 
	* Unified:
	* Do rough layout, getting both the min and pref row heights, and using preferred width for wrapping.
	* Justilign rows.
	* y = 0
	* For each row:
	*     x = 0
	*     Justilign items on row.
	*/

	LayoutResult prefResult = layoutRough(config.wrap, childSizeInfos, hsize, &LayoutSizeInfo::preferred);

	justilignItems(std::span(prefResult.rows),
		&RowInfo::y, &RowInfo::height, &RowInfo::minHeight, &RowInfo::prefHeight,
		hsize.y, config.linesCrosswiseJustilign
	);

	struct ItemInfo
	{
		int x = 0;
		int width = 0;
		int minWidth = 0;
		int prefWidth = 0;
	};

	// If wrapped, always use preferred size (as in prefResult above).
	const auto widthMinSizeInfoMember = prefResult.rows.size() <= 1 ? &ItemInfo::minWidth : &ItemInfo::prefWidth;

	std::vector<ItemInfo> rowItemInfos;

	//if (debug)
	//	_CrtDbgBreak();

	for (const auto& row : prefResult.rows)
	{
		const auto childSizeInfosSpan = std::span(childSizeInfos).subspan(row.firstChildI, row.numChildren);
		rowItemInfos.assign_range(childSizeInfosSpan | std::views::transform([=](const LayoutSizeInfo& info) {
			return ItemInfo{
				.minWidth = info.minimum.x,
				.prefWidth = info.preferred.x,
			};
		}));

		// Justify within layout rect.
		justilignItems(std::span(rowItemInfos),
			&ItemInfo::x, &ItemInfo::width, widthMinSizeInfoMember, &ItemInfo::prefWidth,
			hsize.x, config.itemsFlowwiseJustilign
		);

		for (const auto& [child, childSizeInfo, rowItemInfo] : std::views::zip(children.subspan(row.firstChildI), childSizeInfosSpan, rowItemInfos))
		{
			RowInfo tempHeightInfo{
				.minHeight = childSizeInfo.minimum.y,
				.prefHeight = childSizeInfo.preferred.y,
			};
			justilignItems(std::span(&tempHeightInfo, 1),
				&RowInfo::y, &RowInfo::height, &RowInfo::minHeight, &RowInfo::prefHeight,
				row.height, config.itemsCrosswiseJustilign
			);

			const ivec2 position{ rowItemInfo.x, row.y + tempHeightInfo.y };
			const ivec2 size{ rowItemInfo.width, tempHeightInfo.height };
			child->setRect(iaabb2::sized(position, size));
		}
	}
	/*

	if (!config.wrap || prefResult.rows.size() <= 1)
	{
		const LayoutResult minResult = layoutRough(false, childSizeInfos, hsize, &LayoutSizeInfo::minimum);
		const int miniWidth = hsize.x - minResult.remainingWidth;
		const int prefWidth = hsize.x - prefResult.remainingWidth;

		const Rational widthLerpFactor{
			std::clamp(hsize.x, miniWidth, prefWidth) - miniWidth,
			std::max(1, prefWidth - miniWidth)
		};

		int widthLerpRemainder = 0;
		int x = 0;

		for (const auto& [child, childSizeInfo] : std::views::zip(children, childSizeInfos))
		{
			struct TempItem
			{
				int y{};
				int size{};
				int minSize{};
				int prefSize{};
			};

			TempItem tempHeightInfo{
				.minSize = std::min(childSizeInfo.minimum.y, hsize.y),
				.prefSize = std::min(childSizeInfo.preferred.y, hsize.y),
			};
			justilignItems(std::span(&tempHeightInfo, 1),
				&TempItem::y, &TempItem::size, &TempItem::minSize, &TempItem::prefSize,
				hsize.y, config.justilignOfItemsAcrossLine
			);

			const int width = widthLerpFactor.lerp(childSizeInfo.minimum.x, childSizeInfo.preferred.x, std::ref(widthLerpRemainder));
			const ivec2 position{ x, tempHeightInfo.y };
			const ivec2 size{ width, tempHeightInfo.size };
			child->setRect(iaabb2::sized(position, size));
			x += size.x;
		}
	}
	else
	{
		const LayoutResult minResult = layoutRough(true, childSizeInfos, hsize, &LayoutSizeInfo::minimum);
		const int miniHeight = minResult.minHeight;
		const int prefHeight = minResult.prefHeight;

		const Rational heightLerpFactor{
			std::clamp(hsize.x, miniHeight, prefHeight) - miniHeight,
			std::max(1, prefHeight - miniHeight)
		};

		justilignItems(std::span(minResult.rows), &RowInfo::y, &RowInfo::height, minResult.remainingHeight, config.justilignOfLinesAcrossContainer);

		int widthLerpRemainder = 0;
		int x = 0;
		int y = 0;

		for (const auto& [child, childSizeInfo] : std::views::zip(children, childSizeInfos))
		{
			RowInfo tempHeightInfo{
				.minHeight = std::min(childSizeInfo.preferred.y, hsize.y),
			};
			justify(std::span(&tempHeightInfo, 1), &RowInfo::y, &RowInfo::minHeight, hsize.y - tempHeightInfo.minHeight, config.justilignOfItemsAcrossLine);

			const int width = widthLerpFactor.lerp(childSizeInfo.minimum.x, childSizeInfo.preferred.x, std::ref(widthLerpRemainder));
			const ivec2 position{ x, tempHeightInfo.y };
			const ivec2 size{ width, tempHeightInfo.minHeight };
			child->setRect(iaabb2::sized(position, size));
			x += size.x;
		}
	}

	if (result.remainingWidth < 0)
	{
		result = layoutRough(config, childSizeInfos, hsize, &LayoutSizeInfo::minimum, &LayoutSizeInfo::preferred);
		if (result.remainingHeight < 0)
			result = layoutRough(config, childSizeInfos, hsize, &LayoutSizeInfo::minimum, &LayoutSizeInfo::minimum);
	}
	else
		result = layoutRough(config, childSizeInfos, hsize, &LayoutSizeInfo::preferred, &LayoutSizeInfo::minimum);

	layoutFinal(config, result, children, childSizeInfos);
	*/
	// Now, swap back to horizontal.
	if (config.axis == EAxis::Vertical)
	{
		for (const auto& child : children)
		{
			const iaabb2 rect = child->getRect();
			child->setRect({
				rect.min.flipped(),
				rect.max.flipped(),
				});
		}
	}
}