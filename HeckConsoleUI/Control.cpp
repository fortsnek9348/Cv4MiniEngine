#include "inc/HeckTextUI/Control.h"
#include "Util.h"

#include <algorithm>
#include <iostream>

using namespace hecktui;

Element::Element(std::unique_ptr<Layout> layout) noexcept : mLayout(std::move(layout))
{
}

void Element::addChild(std::shared_ptr<Element> ctrl)
{
	if (ctrl->mParent)
		ctrl->orphan();
	ctrl->mParent = this;
	ctrl->updateHierarchicallyEnabled();
	ctrl->mUISceneState = {};
	mChildren.push_back(std::move(ctrl));
	mChildren.back()->update();
}

void Element::insertChildren(size_t i, std::span<const std::shared_ptr<Element>> children)
{
	for (const auto& ctrl : children)
	{
		if (ctrl->mParent)
			ctrl->orphan();
		ctrl->mParent = this;
		ctrl->updateHierarchicallyEnabled();
		ctrl->mUISceneState = {};
	}
	mChildren.insert_range(mChildren.begin() + i, children);
	for (const auto& ctrl : children)
		ctrl->update();
}

void Element::removeChild(Element* element) noexcept
{
	if (element)
	{
		std::erase_if(mChildren, [element](const std::shared_ptr<Element>& child) { return child.get() == element; });
		element->updateHierarchicallyEnabled();
	}
}

void Element::setChild(size_t i, std::shared_ptr<Element> ctrl)
{
	addChild(std::move(ctrl));
	if (i + 1 < mChildren.size())
	{
		std::swap(mChildren[i], mChildren.back());
		mChildren.back()->orphan();
	}
}

void Element::removeAllChildren() noexcept
{
	while (mChildren.size())
		mChildren.back()->orphan();
}

void Element::orphan()
{
	if (mParent)
	{
		const auto it = std::ranges::find_if(mParent->mChildren, [this](const std::shared_ptr<Element>& child) { return child.get() == this; });
		// Hold onto this!
		const auto selfPtr = std::move(*it);
		mParent->mChildren.erase(it);
		mParent = nullptr;
		mUISceneState = {};
		updateHierarchicallyEnabled();
	}
}

void Element::moveToFirst()
{
	if (mParent)
	{
		const auto it = std::ranges::find_if(mParent->mChildren, [this](const std::shared_ptr<Element>& child) { return child.get() == this; });
		assert(it != mParent->mChildren.end());
		std::rotate(mParent->mChildren.begin(), it, it + 1);
	}
}

void Element::updateHierarchicallyEnabled()
{
	const bool newValue = mEnabled && (!mParent || mParent->mHierarchicallyEnabled);
	if (newValue != mHierarchicallyEnabled)
	{
		mHierarchicallyEnabled = newValue;
		update();
		for (const auto& child : mChildren)
			child->updateHierarchicallyEnabled();
	}
}

Element* Element::getParent() const
{
	return mParent;
}

bool Element::contains(const Element* ctrl) const noexcept
{
	while (ctrl && ctrl != this)
		ctrl = ctrl->getParent();
	return ctrl == this;
}

const std::shared_ptr<Element>& Element::getChildAt(size_t i) const noexcept
{
	return mChildren[i];
}

std::span<const std::shared_ptr<Element>> Element::getChildren() const noexcept
{
	return mChildren;
}

void Element::setLayout(std::unique_ptr<Layout> layout) noexcept
{
	std::swap(mLayout, layout);
}

Layout* Element::getLayout() const noexcept
{
	return mLayout.get();
}

void Element::setRect(iaabb2 rect) noexcept
{
	mRect = rect;
}

const iaabb2& Element::getRect() const noexcept
{
	return mRect;
}

ivec2 Element::getGlobalPosition() const noexcept
{
	ivec2 p = mRect.min;
	const Element* x = mParent;
	while (x)
	{
		p += x->mRect.min;
		x = x->mParent;
	}
	return p;
}

void Element::setEnabled(bool value)
{
	if (mEnabled != value)
	{
		mEnabled = value;
		updateHierarchicallyEnabled();
	}
}
void Element::setVisible(bool value)
{
	mVisible = value;
}

bool Element::isThisEnabled() const noexcept
{
	return mEnabled;
}
bool Element::isThisVisible() const noexcept
{
	return mVisible;
}

bool Element::isHierarchicalEnabled() const noexcept
{
	//return mEnabled && (!mParent || mParent->isHierarchicalEnabled());
	return mHierarchicallyEnabled;
}
bool Element::isHierarchicalVisible() const noexcept
{
	return mVisible && (!mParent || mParent->isHierarchicalVisible());
}

bool Element::canFocus() const noexcept
{
	return mCanFocus;
}
bool Element::isMouseInteractable() const noexcept
{
	return mIsMouseInteractable;
}

//bool Element::hasSizeAxisDependence() const noexcept
//{
//	// We're assuming that every control affects the size of a layout.
//	return mLayout && (mLayout->isWrappingLayout() && mChildren.size() >= 2 || std::ranges::any_of(mChildren, &Element::hasSizeAxisDependence));
//}
//int Element::calcHeightFromWidth(int w, int max) const
//{
//	if (hasSizeAxisDependence())
//	{
//		const auto r = range(max) | std::views::transform([w, this](int h) { return calcWidthFromHeight(h, w); });
//		const auto it = std::ranges::lower_bound(r, w);
//		return it != r.end() ? *it : max;
//	}
//	else
//		return mRect.size().y;
//}
//int Element::calcWidthFromHeight(int h, int max) const
//{
//	return mRect.size().x;
//}

std::optional<EAxis> Element::getThisWrappingAxis() const noexcept
{
	if (mLayout)
		return mLayout->getWrappingAxis();
	else
		return std::nullopt;
}

bool Element::layoutPrepareIteration(bool initial) noexcept
{
	bool iterationNeeded = false;
	for (const auto& child : mChildren)
		iterationNeeded |= child->layoutPrepareIteration(initial);
	if (mLayout)
		iterationNeeded |= mLayout->layoutPrepareIteration(initial, initial ? INT_MAX : getRect().size());
	return iterationNeeded;
}

void Element::layoutPhase1Measure()
{
	if (!mVisible)
		mLayoutSizeInfo = {};
	else
	{
		for (const auto& child : mChildren)
			child->layoutPhase1Measure();

		if (mLayout)
		{
			//mLayout->measureSpace = getRect().size();

			//if (debug)
			//	std::clog << "XXX " << mLayout->measureSpace.x << ", " << mLayout->measureSpace.y << std::endl;

			mLayoutSizeInfo = mLayout->layoutPhase1Measure(mChildren);
		}
		else
		{
			const iaabb2 bounds = iaabb2::combineOneOrMore(mChildren | std::views::transform(&Element::getRect), iaabb2());
			mLayoutSizeInfo = LayoutSizeInfo{
				.minimum = bounds.max,
				.preferred = bounds.max,
			};
		}

		mLayoutSizeInfo = vmax(mLayoutSizeInfo, measureThis());
	}
}
const LayoutSizeInfo& Element::getLayoutSizeInfo() const noexcept
{
	return mLayoutSizeInfo;
}
void Element::layoutPhase2Apply()
{
	if (!mVisible)
		return;
	if (mLayout)
		mLayout->layoutPhase2Apply(mRect.size(), mChildren);
	for (const auto& child : mChildren)
		child->layoutPhase2Apply();
	update();
}

// Returns false if not handled.
bool Element::onEvent(const ConsoleEvent&)
{
	return false;
}

void Element::onBeginMouseHover()
{
}

void Element::onEndMouseHover()
{
}

std::shared_ptr<Element> Element::createTooltip() const
{
	return nullptr;
}

//bool Control::onMouseOver()
//{
//	return false;
//}
//bool Control::onMouseOut()
//{
//	return false;
//}

void Element::onUISceneStateChanged(UISceneState s)
{
	if (mUISceneState != s)
	{
		mUISceneState = s;
		update();
	}
}

Element::UISceneState Element::getUISceneState() const noexcept
{
	return mUISceneState;
}

void Element::update()
{
}

void Element::draw(ivec2 offset, Framebuffer& fb)
{
	if (!mVisible)
		return;

	const ivec2 position = mRect.min + offset;
	const iaabb2 oldStencil = fb.intersectScissorRect(mRect + offset);
	drawThis(offset, fb);
	for (const auto& child : mChildren)
		child->draw(position, fb);
	fb.setScissorRect(oldStencil);
}

Element::~Element() noexcept
{
	while (mChildren.size())
	{
		const auto copy = mChildren.back();
		copy->orphan();
	}
}

LayoutSizeInfo Element::measureThis() const
{
	return {};
}

void Element::drawThis(ivec2, Framebuffer&)
{
}

void Element::deferContextWindow(std::shared_ptr<Window> wnd)
{
	if (mParent)
		mParent->deferContextWindow(std::move(wnd));
}
