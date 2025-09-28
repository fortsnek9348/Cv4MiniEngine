#include "inc/HeckTextUI/Layout.h"
#include "inc/HeckTextUI/Control.h"
#include "Util.h"

using namespace hecktui;

int hecktui::alignShift(int remaining, EAlign align)
{
	//if (remaining <= 0)
	//	align = EAlign::Start;
	switch (align)
	{
	case EAlign::Center:
		return remaining / 2;
	case EAlign::End:
		return remaining;
	default:
		return 0;
	}
}
std::pair<int, int> hecktui::justilignSingle(std::pair<int, int> space, int min, EJustilign align)
{
	if (align >= EJustilign::Stretch)
		return space;
	else
	{
		const int shift = alignShift(space.second - space.first - min, (EAlign)align);
		const int x = space.first + shift;
		return { x, x + min };
	}
}

iaabb2 hecktui::justilignRect(const iaabb2& space, ivec2 minSize, RectJustilign align)
{
	const auto [x0, x1] = justilignSingle({ space.min.x, space.max.x }, minSize.x, align.halign);
	const auto [y0, y1] = justilignSingle({ space.min.y, space.max.y }, minSize.y, align.valign);
	return { .min{ x0, y0 }, .max{ x1, y1 } };
}

std::optional<EAxis> Layout::getWrappingAxis() const noexcept
{
	return std::nullopt;
}

bool Layout::layoutPrepareIteration([[maybe_unused]] bool initial, [[maybe_unused]] ivec2 space) noexcept
{
	return false;
}

LayoutSizeInfo FillLayout::layoutPhase1Measure(ChildList children) const
{
	LayoutSizeInfo info{};
	for (const auto& child : children)
	{
		const LayoutSizeInfo childInfo = child->getLayoutSizeInfo();
		info.minimum = vmax(info.minimum, childInfo.minimum);
		info.preferred = vmax(info.preferred, childInfo.preferred);
	}
	info.minimum += marginTopLeft + marginBottomRight;
	info.preferred += marginTopLeft + marginBottomRight;
	return info;
}

void FillLayout::layoutPhase2Apply(ivec2 space, ChildList children) const
{
	space -= marginTopLeft + marginBottomRight;

	for (const auto& [i, child] : children | std::views::enumerate)
	{
		const LayoutSizeInfo& sizeInfo = child->getLayoutSizeInfo();
		const int selWidth = space.x < sizeInfo.preferred.x ? sizeInfo.minimum.x : sizeInfo.preferred.x;
		const int selHeight = space.y < sizeInfo.preferred.y ? sizeInfo.minimum.y : sizeInfo.preferred.y;
		child->setRect(justilignRect(
			{ .max = space },
			{ selWidth, selHeight }, size_t(i) < childAlignments.size() ? childAlignments[i] : defaultAlign
		) + marginTopLeft);
	}
}

SizeInfoOverrideLayout::SizeInfoOverrideLayout(std::unique_ptr<Layout> overriddenLayout, std::optional<int> overriddenMinWidth, std::optional<int> overriddenMinHeight)
	: overriddenLayout(std::move(overriddenLayout)), overriddenMinWidth(overriddenMinWidth), overriddenMinHeight(overriddenMinHeight)
{
}

LayoutSizeInfo SizeInfoOverrideLayout::layoutPhase1Measure(ChildList children) const
{
	LayoutSizeInfo info = overriddenLayout->layoutPhase1Measure(children);
	if (overriddenMinWidth)
	{
		info.minimum.x = *overriddenMinWidth;
		info.preferred.x = *overriddenMinWidth;
	}
	if (overriddenMinHeight)
	{
		info.minimum.y = *overriddenMinHeight;
		info.preferred.y = *overriddenMinHeight;
	}
	return info;
}
void SizeInfoOverrideLayout::layoutPhase2Apply(ivec2 space, ChildList children) const
{
	return overriddenLayout->layoutPhase2Apply(space, children);
}