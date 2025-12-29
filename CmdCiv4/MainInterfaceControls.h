#pragma once

#include "CvInterface.h"

#include <CvGame.h>

#include <HeckTextUI/HeckTextUI.h>
#include <HeckTextUI/LayoutBuilder.h>
#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Combobox.h>
#include <HeckTextUI/Slider.h>

namespace cvengine
{
	class RichLabel : public hecktui::Label
	{
	public:
		std::vector<hecktui::Pixel> pixels;
		int maxLineWidth = 0;
		int measureWidthLimit = INT_MAX;
		int numLines = 0;
		bool enableAutoContrast = false;
		bool enableExtraSpace = false;
		//bool enableWrapping = false; // Implicitly sets minimum/preferred size to 1.
		CvWidgetDataStruct widgetData{ .m_iData1{}, .m_iData2{}, .m_bOption{}, .m_eWidgetType = WIDGET_GENERAL };

		explicit RichLabel(std::wstring_view label, bool wantMouseInteraction = true);

		std::optional<hecktui::EAxis> getThisWrappingAxis() const noexcept override;

		void setLabel(std::wstring_view label);
		//void setDefaultColouring(hecktui::PixelColouring colouring);
		//void setLabel(std::wstring_view label, hecktui::Pixel defPixel);

		virtual std::shared_ptr<Element> createTooltip() const override;

	protected:
		virtual hecktui::LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(hecktui::ivec2 position, hecktui::Framebuffer& fb) override;
	};

	// Used to identify a control for CvScreensInterface evnet handling.
	struct PythonScreenControlId
	{
		cvengine::ECvScreen screen = cvengine::ECvScreen::NO_SCREEN;
	};

	inline constexpr CvWidgetDataStruct kNoOpWidgetData{
		.m_iData1 = -1,
		.m_iData2 = -1,
		.m_bOption{},
		.m_eWidgetType = WIDGET_GENERAL,
	};

	class ActionButton : public hecktui::EmptyButton
	{
	public:
		// Button with label child.
		explicit ActionButton(std::wstring_view label, CvWidgetDataStruct widgetData, std::function<void()> onClickCallback = {});
		// Empty action button.
		explicit ActionButton(CvWidgetDataStruct widgetData, std::function<void()> onClickCallback = {});

		CvWidgetDataStruct widgetData{};

		virtual void setBorderStyle(hecktui::EBorderStyle) noexcept override;

		// Only if the button was created with a label.
		void setLabel(std::wstring_view label, hecktui::Pixel defPixelColouring);
		void setLabel(std::wstring_view label);
		void setLabelAlignment(hecktui::EAlign align);

		void setPythonScreenControlId(PythonScreenControlId);

		void forceDisableShiftClickHeck();

		RichLabel& getLabel();

		virtual void onClick(hecktui::ModifierKeyState modifierKeyState) override;
		virtual void onRightClick(hecktui::ModifierKeyState modifierKeyState) override;
		virtual bool onEvent(const hecktui::ConsoleEvent& e) override;
		virtual void onBeginMouseHover() override;
		virtual void onEndMouseHover() override;

		virtual std::shared_ptr<Element> createTooltip() const override;

	private:
		//virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

		PythonScreenControlId mPythonScreenControlId;
		std::shared_ptr<RichLabel> mLabel;
		//std::shared_ptr<hecktui::FillLayout> mLabelLayout;
		bool mForceDisableShiftClickHeck = false;
	};

	class ActionCheckBox : public hecktui::Checkbox
	{
	public:
		explicit ActionCheckBox(std::wstring_view label, CvWidgetDataStruct widgetData);

		void setPythonScreenControlId(PythonScreenControlId);

		virtual void onCheckChanged() override;

	private:
		CvWidgetDataStruct mWidgetData{};
		PythonScreenControlId mPythonScreenControlId{};
	};



	class TurnMessageDisplay : public hecktui::Element //public hecktui::ResizableElement
	{
	public:
		TurnMessageDisplay();

		void updateTurnMessageDisplay();

	protected:
		virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

	private:
		uint64_t mFirstSerial = 0;
		Element* mClientPanel = nullptr;
	};

	std::shared_ptr<ActionButton> makeActionButtonWithAutoLabel(const CvGame& game, CvInterface& interfaceController, WidgetTypes widgetType, int actionIndex, int data2);
	std::shared_ptr<ActionButton> makeActionButtonWithManualLabel(std::wstring_view label, CvWidgetDataStruct widgetType, std::function<void()> onClickCallback);
	std::shared_ptr<ActionButton> makeEmptyActionButton(CvWidgetDataStruct widgetType);
	std::shared_ptr<ActionCheckBox> makeActionCheckBoxWithAutoLabel(CvInterface& interfaceController, WidgetTypes widgetType, int actionIndex);

	std::shared_ptr<hecktui::Combobox> makePythonCombobox(WidgetTypes widgetType, int actionIndex, int data2, PythonScreenControlId pythonCtrlId);

	std::shared_ptr<hecktui::Element> createWidgetTooltip(CvWidgetDataStruct widgetData);

	bool doPythonScreenEventHandling(const CvWidgetDataStruct& widgetData, const PythonScreenControlId& id, cvengine::NotifyCode notifyCode, cvengine::MouseFlags mouseFlags);

	class PlotListUnitButton : public hecktui::EmptyButton
	{
	public:
		explicit PlotListUnitButton(CvUnit* unit);
		virtual void onClick(hecktui::ModifierKeyState modifierKeyState) override;
		virtual void onRightClick(hecktui::ModifierKeyState modifierKeyState) override;

	protected:
		virtual hecktui::LayoutSizeInfo measureThis() const noexcept override;
		virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

	private:
		CvUnit* mUnit = nullptr;
		std::shared_ptr<RichLabel> mRichLabel;
	};

	class PythonScreenSlider : public hecktui::Slider
	{
	public:
		explicit PythonScreenSlider(int max, PythonScreenControlId pythonScreenControlId);
		virtual void onSliderValueChanged() override;

	private:
		PythonScreenControlId mPythonScreenControlId;
	};
}