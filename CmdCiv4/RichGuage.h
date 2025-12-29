#pragma once

#include "MainInterfaceControls.h"

namespace cvengine
{
	struct RichGuage : hecktui::Element
	{
		std::vector<hecktui::EColour> colours;
		std::vector<int> ratios;
		int max = 0;
		std::shared_ptr<ActionButton> actionButton;

		explicit RichGuage(std::vector<hecktui::EColour> colours, CvWidgetDataStruct widgetData);

		// Draw a guage without the side backets or text.
		static void drawBareGuage(hecktui::Framebuffer& fb, const hecktui::iaabb2& rect,
			std::span<const hecktui::EColour> colours,
			std::span<const int> ratios,
			int max
		);

	protected:
		virtual hecktui::LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;
	};
}