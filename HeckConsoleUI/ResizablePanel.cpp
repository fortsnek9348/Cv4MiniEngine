#include "inc/HeckTextUI/BasicControls.h"
#include "inc/HeckTextUI/Layout.h"

#include <algorithm>

using namespace hecktui;

struct ResizablePanel::MyLayout : Layout
{
	ResizablePanel& parent;

	explicit MyLayout(ResizablePanel& parent) : parent(parent)
	{
	}

	virtual LayoutSizeInfo layoutPhase1Measure(ChildList children) const override
	{
		LayoutSizeInfo A = children.size() >= 1 ? children[0]->getLayoutSizeInfo() : LayoutSizeInfo();
		LayoutSizeInfo B = children.size() >= 2 ? children[1]->getLayoutSizeInfo() : LayoutSizeInfo();
		if (parent.mAxis == EAxis::Vertical)
		{
			std::swap(A.minimum.x, A.minimum.y);
			std::swap(B.minimum.x, B.minimum.y);
			std::swap(A.preferred.x, A.preferred.y);
			std::swap(B.preferred.x, B.preferred.y);
		}

		LayoutSizeInfo R{
			{ A.minimum.x + B.minimum.x + 1, std::max(A.minimum.y, B.minimum.y) },
			{ A.preferred.x + B.preferred.x + 1, std::max(A.preferred.y, B.preferred.y) },
		};

		if (parent.mAxis == EAxis::Vertical)
		{
			std::swap(R.minimum.x, R.minimum.y);
			std::swap(R.preferred.x, R.preferred.y);
		}

		return R;
	}

	virtual void layoutPhase2Apply(ivec2 space, ChildList children) const override
	{
		LayoutSizeInfo A = children.size() >= 1 ? children[0]->getLayoutSizeInfo() : LayoutSizeInfo();
		LayoutSizeInfo B = children.size() >= 2 ? children[1]->getLayoutSizeInfo() : LayoutSizeInfo();
		if (parent.mAxis == EAxis::Vertical)
		{
			std::swap(A.minimum.x, A.minimum.y);
			std::swap(B.minimum.x, B.minimum.y);
			std::swap(space.x, space.y);
		}

		//const int min = A.minimum.x;
		//const int max = (space.x - 1) - B.minimum.x;
		//if (min <= max)
		//	parent.setPhysicalSplitterPosition(std::clamp(parent.getPhysicalSplitterPosition(), min, max));
		//else
		//	parent.setPhysicalSplitterPosition((min + max) / 2);

		iaabb2 rectA{
			.min{ 0, 0 },
			.max{ parent.getPhysicalSplitterPosition(), space.y },
		};
		iaabb2 rectB{
			.min{ parent.getPhysicalSplitterPosition() + 1, 0 },
			.max{ space.x, space.y },
		};

		if (parent.mAxis == EAxis::Vertical)
		{
			std::swap(rectA.min.x, rectA.min.y);
			std::swap(rectA.max.x, rectA.max.y);
			std::swap(rectB.min.x, rectB.min.y);
			std::swap(rectB.max.x, rectB.max.y);
		}
		
		if (children.size() >= 1)
			children[0]->setRect(rectA);
		if (children.size() >= 2)
			children[1]->setRect(rectB);
	}
};

ResizablePanel::ResizablePanel(EAxis axis, int splitterPosition, bool flip) : mAxis(axis), mSplitterPosition(splitterPosition), mFlip(flip)
{
	setLayout(std::make_unique<MyLayout>(*this));
	mCanFocus = false;
	mIsMouseInteractable = true;
}

// Returns false if not handled.
bool ResizablePanel::onEvent(const ConsoleEvent& e)
{
	switch (e.type)
	{
	case EConsoleEventType::MouseButtonDown:
	{
		const auto& e2 = static_cast<const MouseButtonEvent&>(e);
		if (e2.button == EMouseButton::Left && mMouseSplitterHover)
		{
			mMouseCaptured = true;
			return true;
		}
		return false;
	}

	case EConsoleEventType::MouseMove:
	{
		const auto& e2 = static_cast<const MouseMoveEvent&>(e);
		if (mMouseCaptured)
		{
			mMouseSplitterHover = true;
			setPhysicalSplitterPosition(mAxis == EAxis::Horizontal ? e2.coord.x : e2.coord.y);
			getLayout()->layoutPhase2Apply(getRect().size(), getChildren());
			return true;
		}
		else
			mMouseSplitterHover = (mAxis == EAxis::Horizontal ? e2.coord.x : e2.coord.y) == getPhysicalSplitterPosition();
		return false;
	}
	case EConsoleEventType::MouseButtonUp:
	{
		const auto& e2 = static_cast<const MouseButtonEvent&>(e);
		if (e2.button == EMouseButton::Left)
		{
			mMouseCaptured = false;
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}

void ResizablePanel::update()
{
	Element::update();
	mMouseSplitterHover &= getUISceneState().hover;
}

LayoutSizeInfo ResizablePanel::measureThis() const noexcept
{
	return { { 3, 3 }, { 3, 3 } };
}

void ResizablePanel::drawThis(ivec2 offset, Framebuffer& fb)
{
	PixelColouring splitterColouring{};
	if (mMouseSplitterHover)
		splitterColouring.text = EColour::White;
	const int physSplitterPos = getPhysicalSplitterPosition();
	offset += getRect().min;
	if (mAxis == EAxis::Horizontal)
		fb.drawVLine(offset + ivec2{ physSplitterPos, 0 }, getRect().size().y, EBorderStyle::Double, splitterColouring);
	else
		fb.drawHLine(offset + ivec2{ 0, physSplitterPos }, getRect().size().x, EBorderStyle::Double, splitterColouring);
	//fb.drawBox(getRect() + (position - getRect().min), EBorderStyle::Double, {});
}

int ResizablePanel::getPhysicalSplitterPosition() const
{
	if (mFlip)
	{
		const int ref = (mAxis == EAxis::Horizontal ? getRect().size().x : getRect().size().y) - 1;
		return ref - mSplitterPosition;
	}
	else
		return mSplitterPosition;
}
void ResizablePanel::setPhysicalSplitterPosition(int x)
{
	if (mFlip)
	{
		const int ref = (mAxis == EAxis::Horizontal ? getRect().size().x : getRect().size().y) - 1;
		mSplitterPosition = ref - x;
	}
	else
		mSplitterPosition = x;
}