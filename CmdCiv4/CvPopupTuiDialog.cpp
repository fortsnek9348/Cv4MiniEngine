#include "CvPopupTuiDialog.h"
#include "MainInterfaceControls.h"
#include "CvApp.h"
#include "UITheme.h"
#include "TuiTextCode.h"

#include <CvString.h>
#include <CvPopupReturn.h>
#include <CvGlobals.h>
#include <CvGameAI.h>
#include <CvInfos.h>
#include <CvGameCoreUtils.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Textbox.h>

#include <ranges>
#include <stdexcept>
#include <cwctype>

using namespace hecktui;
using namespace cvengine;
using cvengine::RichLabel;

CvPopupTuiDialog::CvPopupTuiDialog(CvPopup* popup)
	: Window({}, {
		.isDefaultFocus = false,
		.isFullscreen = false,
		.isModal = true,
		.canClose = false,
		.borderStyle = EBorderStyle::Rounded,
	}), mPopup(std::move(popup))
{
	const auto parent = getClientArea().get();
	parent->setLayout(std::make_unique<FlowLayout>(
		FlowConfig{
			.axis = EAxis::Vertical,
			.wrap = false,
			.linesCrosswiseJustilign = EJustilign::Stretch,
		}
	));

	if (mPopup->headerString.size())
	{
		auto lbl = std::make_shared<RichLabel>(CvWString(mPopup->headerString), false);
		lbl->enableWrapping = true;
		lbl->measureWidthLimit = 50; // Let's not have super wide popups all the time.
		parent->addChild(std::move(lbl));
		parent->addChild(std::make_shared<HorizontalRule>(EBorderStyle::Thin));
	}

	if (mPopup->bodyString.size())
	{
		auto lbl = std::make_shared<RichLabel>(CvWString(mPopup->bodyString), false);
		lbl->enableWrapping = true;
		lbl->measureWidthLimit = 50; // Let's not have super wide popups all the time.
		parent->addChild(std::move(lbl));
	}

	auto scrollPanel = std::make_shared<ScrollBarPanel>(false, true);
	parent->addChild(scrollPanel);
	scrollPanel->getPanel()->setLayout(std::make_unique<FlowLayout>(FlowConfig{ .axis = EAxis::Vertical, .linesCrosswiseJustilign = EJustilign::Stretch }));

	for (const auto& desc : mPopup->controls)
	{
		std::shared_ptr<Element> ctrl;

		switch (desc.type)
		{
		case CvPopup::EControlType::Label:
			ctrl = std::make_shared<RichLabel>(CvWString(desc.text));
			break;
		case CvPopup::EControlType::Button:
		{
			std::wstring prefix;
			switch (desc.widgetData.m_eWidgetType)
			{
			case WIDGET_TRAIN:
			{
				const UnitTypes eUnit = ((UnitTypes)(gGlobals.getCivilizationInfo(gGlobals.getGame().getActiveCivilizationType()).getCivilizationUnits(desc.widgetData.m_iData1)));
				if (gGlobals.getUnitInfo(eUnit).getCombat() != 0)
					prefix = L"ϡ";
				else
					prefix = L"☺";
				break;
			}
			case WIDGET_CONSTRUCT:
			{
				const BuildingTypes eBuilding = ((BuildingTypes)(GC.getCivilizationInfo(GC.getGame().getActiveCivilizationType()).getCivilizationBuildings(desc.widgetData.m_iData1)));
				if (isLimitedWonderClass(static_cast<BuildingClassTypes>(gGlobals.getBuildingInfo(eBuilding).getBuildingClassType())))
					prefix = L"Ԉ";
				else
					prefix = L"⌂"; //L"л";
				break;
			}
			case WIDGET_CREATE: // team projects and such
				prefix = L"҉";
				break;
			case WIDGET_MAINTAIN:
			{
				const CvProcessInfo& info = gGlobals.getProcessInfo(static_cast<ProcessTypes>(desc.widgetData.m_iData1));
				if (info.getProductionToCommerceModifier(COMMERCE_GOLD) != 0)
					prefix = lookupSymbolChar(static_cast<wchar_t>(gGlobals.getCommerceInfo(COMMERCE_GOLD).getChar()));
				if (info.getProductionToCommerceModifier(COMMERCE_RESEARCH) != 0)
					prefix = lookupSymbolChar(static_cast<wchar_t>(gGlobals.getCommerceInfo(COMMERCE_RESEARCH).getChar()));
				if (info.getProductionToCommerceModifier(COMMERCE_CULTURE) != 0)
					prefix = lookupSymbolChar(static_cast<wchar_t>(gGlobals.getCommerceInfo(COMMERCE_CULTURE).getChar()));
				break;
			}
			default:
				break;
			}

			std::wstring text = desc.text;

			if (!prefix.empty())
			{
				prefix += L' ';
				text.insert(text.begin(), prefix.begin(), prefix.end());
			}

			auto btn = cvengine::makeActionButtonWithManualLabel(text, desc.widgetData, [this, id = desc.iGroup] {
				submit(id);
				});
			btn->getLabel().enableWrapping = true;
			// Compact production chooser
			const bool isProductionChooser = desc.widgetData.m_eWidgetType == WIDGET_CONSTRUCT ||
				desc.widgetData.m_eWidgetType == WIDGET_CREATE ||
				desc.widgetData.m_eWidgetType == WIDGET_MAINTAIN ||
				desc.widgetData.m_eWidgetType == WIDGET_TRAIN;
			const bool isResearch = desc.widgetData.m_eWidgetType == WIDGET_RESEARCH;
			//btn->setBorderStyle(isProductionChooser ? EBorderStyle::Bracketed : EBorderStyle::Rounded);
			btn->setBorderStyle(EBorderStyle::Rounded);
			if (isProductionChooser || isResearch)
				btn->getLabel().setLabelAlignment(hecktui::EAlign::Left);
			
		
			
			ctrl = std::move(btn);
			break;
		}
		case CvPopup::EControlType::Separator:
			ctrl = std::make_shared<HorizontalRule>(EBorderStyle::Thin);
			break;
		case CvPopup::EControlType::Image:
			ctrl = std::make_shared<Label>(L"[IMAGE]");
			break;
		case CvPopup::EControlType::ListBox:
			ctrl = std::make_shared<Label>(L"[ListBox]");
			break;
		case CvPopup::EControlType::ComboBox:
			ctrl = std::make_shared<Label>(L"[ComboBox]");
			break;
		case CvPopup::EControlType::CheckBoxGroup:
			ctrl = std::make_shared<Label>(L"[CheckBoxGroup]");
			break;
		case CvPopup::EControlType::RadioButtonGroup:
			ctrl = std::make_shared<Label>(L"[RadioButtonGroup]");
			break;
		case CvPopup::EControlType::EditBox:
			ctrl = std::make_shared<Textbox>(
				L"",
				CvWString(desc.text), 
				EColour::Black,
				EColour::Gray,
				EColour::White
			);
			break;
		case CvPopup::EControlType::SpinBox:
			ctrl = std::make_shared<Textbox>(
				L"",
				desc.text,
				EColour::Black,
				EColour::Gray,
				EColour::White
			);
			break;
		default:
			std::abort();
		}

		if (!desc.enabled)
			ctrl->setEnabled(false);

		mPopupElements.push_back(ctrl.get());
		scrollPanel->getPanel()->addChild(std::move(ctrl));
	}
}

// TODO: Similar to CvGInterfaceScreen.
void CvPopupTuiDialog::positionWindowInScene(ivec2 sceneDim)
{
	if (getWindowConfig().isFullscreen)
		return Window::positionWindowInScene(sceneDim);

	if (!wasMoved() && !wasResized())
	{
		const ivec2 measurement = getLayoutSizeInfo().preferred;
		//const ivec2 size{ 50, measurement.y };
		//ivec2 size{ std::clamp(measurement.x, 0, sceneDim.x), std::clamp(measurement.y, 0, sceneDim.y) };

		ivec2 size = measurement;

		//const ivec2 position = (sceneDim - size) / 2;

		const iaabb2 spaceRect = iaabb2{ .max = sceneDim }.shrunk({ 1, 1 });

		size.x = std::clamp(measurement.x, 0, spaceRect.size().x);
		size.y = std::clamp(measurement.y, 0, spaceRect.size().y);

		setRect(hecktui::justilignRect(spaceRect, size, { EJustilign::End, EJustilign::Start }));
	}
}

bool CvPopupTuiDialog::submit(int btnId)
{
	PopupReturn result;
	for (const auto& [i, desc] : mPopup->controls | std::views::enumerate)
	{
		switch (desc.type)
		{
		case CvPopup::EControlType::EditBox:
			result.setEditBoxString(static_cast<const Textbox&>(*mPopupElements[i]).getText(), desc.iGroup);
			break;
		case CvPopup::EControlType::SpinBox:
		{
			const CvWString text = (CvWString)static_cast<const Textbox&>(*mPopupElements[i]).getText();
			size_t end = 0;
			int value = 0;
			try
			{
				value = std::stoi(text, &end);
			}
			catch (const std::runtime_error&)
			{
				return false;
			}
			if (!std::ranges::all_of(std::wstring_view(text).substr(end), [](wchar_t c) { return std::iswspace(c); }))
				return false;
			result.setCurrentSpinBoxValue(std::clamp(value, 0, desc.spinBoxMax), desc.iGroup);
			break;
		}
		default:
			break;
		}
	}

	result.setButtonClicked(btnId);

	mPopup->submit(std::move(result));

	setWantClose();
	CvApp::getInstance().getUI().removeWindow(this);

	return true;
}

CvPopup& CvPopupTuiDialog::getPopup()
{
	return *mPopup;
}

bool CvPopupTuiDialog::onEvent(const hecktui::ConsoleEvent& e)
{
	if (e.type == EConsoleEventType::ActionKeyPressed && static_cast<const ActionKeyEvent&>(e).key == EActionKey::Enter && mPopup->optEnterSubmitBtnId)
	{
		submit(*mPopup->optEnterSubmitBtnId);
		return true;
	}
	else if (e.type == EConsoleEventType::ActionKeyPressed && static_cast<const ActionKeyEvent&>(e).key == EActionKey::Esc && mPopup->enableEscCancel)
	{
		mPopup->escCancel();
		setWantClose();
		CvApp::getInstance().getUI().removeWindow(this);
		return true;
	}
	else
		return Window::onEvent(e);
}

void CvPopupTuiDialog::setWantClose()
{
	Window::setWantClose();
}
bool CvPopupTuiDialog::isWantClose() const
{
	return Window::wantClose(nullptr);
}

std::optional<PopupReturn> InternalPopup::launchModal(std::unique_ptr<InternalPopup> popup)
{
	const auto wnd = std::make_shared<CvPopupTuiDialog>(popup.get());
	popup->window = wnd;
	CvApp::getInstance().getUI().pushWindow(wnd);
	while (!wnd->wantClose(nullptr))
		CvApp::getInstance().getUI().updateUI();
	CvApp::getInstance().getUI().removeWindow(wnd.get());
	return std::move(popup->optResult);
}

void InternalPopup::onPopupDisplayed()
{
}
void InternalPopup::submit(const PopupReturn& x)
{
	optResult = x;
}
void InternalPopup::escCancel()
{
	optResult = std::nullopt;
}