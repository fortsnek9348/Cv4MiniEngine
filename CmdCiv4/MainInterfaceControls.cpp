#include "MainInterfaceControls.h"
#include "TuiTextCode.h"
#include "CvApp.h"
#include "UITheme.h"
#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"

#include <Cv4CommonEngineLib/MyCvDLLPython.h>
#include <Cv4CommonEngineLib/CvTranslator.h>

#include <CvGameCoreDLL/CvDLLWidgetData.h>
#include <CvGameCoreDLL/CvInfos.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvPlayerAI.h>
#include <CvGameCoreDLL/CvCity.h>
#include <CvGameCoreDLL/CyArgsList.h>

#include <iostream>

using namespace hecktui;
using namespace cvengine;

bool cvengine::doPythonScreenEventHandling(const CvWidgetDataStruct& widgetData, const PythonScreenControlId& id, cvengine::NotifyCode notifyCode, cvengine::MouseFlags mouseFlags)
{
	if (widgetData.m_eWidgetType != WIDGET_PYTHON
		&& widgetData.m_eWidgetType != WIDGET_LEADERHEAD
		&& widgetData.m_eWidgetType != WIDGET_CONVERT
		&& widgetData.m_eWidgetType != WIDGET_ACTION)// || id.name.empty())
		return false;

	CyArgsList pyArgs;
	// tuple listed in ScreenInput.py.
	// NOTE: iData2 is the screen enum in Cv4MiniEngine.
	pyArgs.add(notifyCode); // self.eNotifyCode = argsList[0]
	pyArgs.add(-1); // ? self.iData = argsList[1]
	pyArgs.add(mouseFlags); // ? self.uiFlags = argsList[2]
	pyArgs.add(-1); // ? self.iItemID = argsList[3]
	pyArgs.add((int)id.screen); // self.ePythonFileEnum = argsList[4]
	pyArgs.add(""); // self.szFunctionName = argsList[5]
	const hecktui::ModifierKeyState modifierKeys = CvApp::getInstance().getLastModifierKeysState();
	pyArgs.add(modifierKeys.shift); // self.bShift = argsList[6]
	pyArgs.add(modifierKeys.ctrl); // self.bCtrl = argsList[7]
	pyArgs.add(modifierKeys.alt); // self.bAlt = argsList[8]
	pyArgs.add(0); // self.iMouseX = argsList[9]
	pyArgs.add(0); // self.iMouseY = argsList[10]
	pyArgs.add(widgetData.m_eWidgetType); // self.iButtonType = argsList[11]
	pyArgs.add(widgetData.m_iData1); // self.iData1 = argsList[12]
	pyArgs.add(widgetData.m_iData2); // self.iData2 = argsList[13]
	pyArgs.add(widgetData.m_bOption); // self.bOption = argsList[14]

	long result = 0;
	return MyCvDLLPython().callFunction(PYScreensModule, "handleInput", pyArgs.makeFunctionArgs(), &result) && result != 0;
}


std::shared_ptr<Element> cvengine::createWidgetTooltip(CvWidgetDataStruct widgetData)
{
	CvWString text;

	if (widgetData.m_eWidgetType < NUM_WIDGET_TYPES)
	{
		CvWStringBuffer descBuffer;
		CvDLLWidgetData::getInstance().parseHelp(descBuffer, widgetData);
		text = descBuffer.getCString();
	}
	else
	{
		// BUG's extended tooltips. The GPP bars on the game header bar uses this.
		CyArgsList pyArgs;
		pyArgs.add(widgetData.m_eWidgetType);
		pyArgs.add(widgetData.m_iData1);
		pyArgs.add(widgetData.m_iData2);
		pyArgs.add(widgetData.m_bOption);
		(void)MyCvDLLPython().callFunction(PYGameModule, "getWidgetHelp", pyArgs.makeFunctionArgs(), &text);
	}

	if (text.empty())
		return nullptr;

	auto boxPanel = std::make_shared<BoxPanel>(EBorderStyle::Thin, PixelColouring{ .back = EColour::Black });
	boxPanel->interiorColouring.back = EColour::Black;
	boxPanel->enableAutoMergeCardinal = false;
	auto layout = std::make_unique<FillLayout>();
	layout->marginTopLeft = { 1, 1 };
	layout->marginBottomRight = { 1, 1 };
	boxPanel->setLayout(std::move(layout));
	const auto lbl = std::make_shared<RichLabel>(L"");
	lbl->setLabel(text.c_str());
	lbl->enableWrapping = true;
	boxPanel->addChild(lbl);
	return boxPanel;
}


ActionButton::ActionButton(std::wstring_view label, CvWidgetDataStruct widgetData, std::function<void()> onClickCallback)
	: EmptyButton(std::move(onClickCallback))
	, widgetData(widgetData)
	, mLabel(std::make_shared<RichLabel>(L"", false))
{
	mLabel->colouring = { .text = theme::kButtonDefaultLabelColour, .back = hecktui::kTransparent };
	mLabel->setLabel(label);
	mLabel->setLabelAlignment(EAlign::Center);
	addChild(mLabel);
	setLayout(std::make_unique<FillLayout>());
	setBorderStyle(mBorderStyle);
	setLabelColour(theme::kButtonDefaultBorderColour);
	setBackgroundColour(kTransparent);
	setEnableRightClick(true);
}

ActionButton::ActionButton(CvWidgetDataStruct widgetData, std::function<void()> onClickCallback)
	: EmptyButton(std::move(onClickCallback))
	, widgetData(widgetData)
{
	setBackgroundColour(kTransparent);
	setEnableRightClick(true);
}

void ActionButton::setBorderStyle(EBorderStyle style) noexcept
{
	EmptyButton::setBorderStyle(style);
	FillLayout* const layout = static_cast<FillLayout*>(getLayout());
	layout->marginTopLeft = layout->marginBottomRight =
		style == EBorderStyle::None ? ivec2() :
		style == EBorderStyle::Bracketed ? ivec2(1, 0)
		: ivec2(1, 1);
}

void ActionButton::setLabel(std::wstring_view label, hecktui::Pixel defPixelColouring)
{
	mLabel->colouring = defPixelColouring.colour;
	mLabel->setLabel(label);
}

void ActionButton::setLabel(std::wstring_view label)
{
	mLabel->setLabel(label);
}

void ActionButton::setLabelAlignment(hecktui::EAlign align)
{
	mLabel->setLabelAlignment(align);
}

void ActionButton::setPythonScreenControlId(PythonScreenControlId id)
{
	mPythonScreenControlId = std::move(id);
}

void ActionButton::forceDisableShiftClickHeck()
{
	mForceDisableShiftClickHeck = true;
}

RichLabel& ActionButton::getLabel()
{
	return *mLabel;
}

void ActionButton::onClick([[maybe_unused]] ModifierKeyState modifierKeyState)
{
	if (doPythonScreenEventHandling(widgetData, mPythonScreenControlId, cvengine::NOTIFY_CLICKED, cvengine::MOUSE_LBUTTON | cvengine::MOUSE_LBUTTONUP))
		return;

	(void)CvDLLWidgetData::getInstance().executeAction(widgetData);
	// Don't trigger a game update if this is a no-op.
	if (widgetData.m_eWidgetType != WIDGET_PYTHON && widgetData.m_eWidgetType != WIDGET_GENERAL)
		CvTuiInterface::getInstance().getTuiMainInterface()->onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::Action);
}

void ActionButton::onRightClick(ModifierKeyState modifierKeyState)
{
	if (!mForceDisableShiftClickHeck && CvApp::getInstance().isShiftClickHackEnabled())
	{
		modifierKeyState.shift = true;
		const auto oldState = CvApp::getInstance().getUI().exchangeLastModifierKeysState(modifierKeyState);
		onClick(modifierKeyState);
		(void)CvApp::getInstance().getUI().exchangeLastModifierKeysState(oldState);
	}
	else
	{
		// Alt action. Used by foreign advisor bonus page. Right-click on leader headers.
		// NOTE: No capture state, can drag mouse and release on the button.
		
		(void)doPythonScreenEventHandling(widgetData, mPythonScreenControlId, cvengine::NOTIFY_CLICKED, cvengine::MOUSE_RBUTTON | cvengine::MOUSE_RBUTTONUP);
		
		(void)CvDLLWidgetData::getInstance().executeAltAction(widgetData);
		CvTuiInterface::getInstance().getTuiMainInterface()->onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::Action);
	}
}

bool ActionButton::onEvent(const hecktui::ConsoleEvent& e)
{
	//if (e.type == EConsoleEventType::MouseButtonUp && static_cast<const MouseButtonEvent&>(e).button == EMouseButton::Right)
	//{
	//	// Alt action. Used by foreign advisor bonus page. Right-click on leader headers.
	//	// NOTE: No capture state, can drag mouse and release on the button.
	//	
	//	(void)doPythonScreenEventHandling(widgetData, mPythonScreenControlId, cvengine::NOTIFY_CLICKED, cvengine::MOUSE_RBUTTON | cvengine::MOUSE_RBUTTONUP);
	//
	//	(void)CvDLLWidgetData::getInstance().executeAltAction(widgetData);
	//	CvInterface::getInstance().onGameStateChanged(CvInterface::EGameStateChangeReason::Action);
	//	return true;
	//}
	//else
		return EmptyButton::onEvent(e);
}

void ActionButton::onBeginMouseHover()
{
	EmptyButton::onBeginMouseHover();
	(void)doPythonScreenEventHandling(widgetData, mPythonScreenControlId, cvengine::NOTIFY_CURSOR_MOVE_ON, cvengine::MouseFlags());
}
void ActionButton::onEndMouseHover()
{
	EmptyButton::onEndMouseHover();
	(void)doPythonScreenEventHandling(widgetData, mPythonScreenControlId, cvengine::NOTIFY_CURSOR_MOVE_OFF, cvengine::MouseFlags());
}

std::shared_ptr<Element> ActionButton::createTooltip() const
{
	return createWidgetTooltip(widgetData);
}

/*void ActionButton::drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb)
{
	EmptyButton::drawThis(offset, fb);

}*/

// Used for the on/off checkbox strings, as the text XML doesn't store them without the "Turn On/Off" string.
static std::wstring guessCommonString(const std::wstring& keyA, const std::wstring& keyB)
{
	const std::wstring textA = CvTranslator::getInstance().getText(keyA);
	const std::wstring textB = CvTranslator::getInstance().getText(keyB);

	const size_t commonStartLength = std::ranges::mismatch(textA, textB).in1 - textA.begin();
	const size_t commonEndLength = std::mismatch(textA.rbegin(), textA.rend(), textB.rbegin(), textB.rend()).first - textA.rbegin();

	if (commonStartLength >= commonEndLength)
		return heck::trim(textA.substr(0, commonStartLength));
	else
		return heck::trim(textA.substr(textA.size() - commonEndLength));
}

static std::wstring makeActionLabel(const CvWidgetDataStruct& widgetInfo)
{
	const CvCity* const selCity = CvTuiInterface::getInstance().getHeadSelectedCity();

	std::wstring name;

	// Figure out a label for the button. This is something that Civ4 doesn't provide directly.
	switch (widgetInfo.m_eWidgetType)
	{
	case WIDGET_ACTION:
		name = gGlobals.getActionInfo(widgetInfo.m_iData1).getDescription();
		break;
	case WIDGET_CREATE_GROUP:
	case WIDGET_DELETE_GROUP:
	//case WIDGET_CONSCRIPT:
	
	
	{
		CvWidgetDataStruct temp = widgetInfo;
		CvWStringBuffer descBuffer;
		CvDLLWidgetData::getInstance().parseHelp(descBuffer, temp);
		name = descBuffer.getCString();
		break;
	}

	case WIDGET_EMPHASIZE:
		name = gGlobals.getEmphasizeInfo((EmphasizeTypes)widgetInfo.m_iData1).getDescription();
		break;
	case WIDGET_AUTOMATE_PRODUCTION:
		name = guessCommonString(L"TXT_KEY_MISC_OFF_PROD_AUTO", L"TXT_KEY_MISC_ON_PROD_AUTO");
		break;

	case WIDGET_AUTOMATE_CITIZENS:
		name = guessCommonString(L"TXT_KEY_MISC_OFF_CITIZEN_AUTO", L"TXT_KEY_MISC_ON_CITIZEN_AUTO");
		break;
	case WIDGET_TRAIN:
	{
		const auto infoI = (UnitTypes)gGlobals.getCivilizationInfo(selCity->getCivilizationType()).getCivilizationUnits(widgetInfo.m_iData1);
		name = gGlobals.getUnitInfo(infoI).getDescription();
		break;
	}
	case WIDGET_CONSTRUCT:
	{
		const auto infoI = (BuildingTypes)gGlobals.getCivilizationInfo(selCity->getCivilizationType()).getCivilizationBuildings(widgetInfo.m_iData1);
		name = gGlobals.getBuildingInfo(infoI).getDescription();
		break;
	}
	case WIDGET_CREATE:
		name = gGlobals.getProjectInfo((ProjectTypes)widgetInfo.m_iData1).getDescription();
		break;
	case WIDGET_MAINTAIN:
		name = gGlobals.getProcessInfo((ProcessTypes)widgetInfo.m_iData1).getDescription();
		break;
	case WIDGET_CHANGE_PERCENT:
	case WIDGET_CHANGE_SPECIALIST:
		name = widgetInfo.m_iData2 < 0 ? L"[-]" : L"[+]";
		break;

		// Do it in python...
	//case WIDGET_TECH_TREE:
	//	name = gGlobals.getTechInfo((TechTypes)widgetInfo.m_iData1).getDescription();
	//	break;

	case WIDGET_LEADERHEAD:
	{
		const CvPlayer& player = CvPlayerAI::getPlayerNonInl((PlayerTypes)widgetInfo.m_iData1);
		name = CvTranslator::changeTextColor(player.getName(), gGlobals.getPlayerColorInfo(player.getPlayerColor()).getTextColorType());
		break;
	}

	case WIDGET_CLOSE_SCREEN:
		name = CvTranslator::getInstance().getText(L"TXT_KEY_PEDIA_SCREEN_EXIT");
		break;
	
	default:
		// empty, no label child control
		break;
	}

	return name;
}

std::shared_ptr<ActionButton> cvengine::makeActionButtonWithAutoLabel(const CvGame& game, WidgetTypes widgetType, int actionIndex, int data2)
{
	CvWidgetDataStruct widgetInfo{
		.m_iData1 = actionIndex, // Probably from python code.
		.m_iData2 = data2, // Probably from python code.
		.m_bOption = false, // ??
		.m_eWidgetType = widgetType,
	};

	auto btn = std::make_shared<ActionButton>(makeActionLabel(widgetInfo), widgetInfo);

	if (widgetType == WIDGET_ACTION)
		btn->setEnabled(game.canHandleAction(actionIndex, CvTuiInterface::getInstance().getGotoPlot(), false, false));

	return btn;
}

std::shared_ptr<ActionButton> cvengine::makeActionButtonWithManualLabel(std::wstring_view label, CvWidgetDataStruct widgetType, std::function<void()> onClickCallback)
{
	return std::make_shared<ActionButton>(label, widgetType, std::move(onClickCallback));
}

std::shared_ptr<ActionButton> cvengine::makeEmptyActionButton(CvWidgetDataStruct widgetType)
{
	return std::make_shared<ActionButton>(widgetType, nullptr);
}

ActionCheckBox::ActionCheckBox(std::wstring_view label, CvWidgetDataStruct widgetData)
	: hecktui::Checkbox(ECheckStyle::AsciiX, std::wstring(label))
	, mWidgetData(widgetData)
{
}

void ActionCheckBox::setPythonScreenControlId(PythonScreenControlId screen)
{
	mPythonScreenControlId = screen;
}

void ActionCheckBox::onCheckChanged()
{
	(void)doPythonScreenEventHandling(mWidgetData, mPythonScreenControlId, cvengine::NOTIFY_CLICKED, cvengine::MOUSE_LBUTTON | cvengine::MOUSE_LBUTTONUP);
	(void)CvDLLWidgetData::getInstance().executeAction(mWidgetData);
	CvTuiInterface::getInstance().getTuiMainInterface()->onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::Action);
}

std::shared_ptr<ActionCheckBox> cvengine::makeActionCheckBoxWithAutoLabel(WidgetTypes widgetType, int actionIndex)
{
	const CvWidgetDataStruct widgetInfo{
		.m_iData1 = actionIndex,
		.m_iData2 = -1,
		.m_bOption = false,
		.m_eWidgetType = widgetType,
	};

	return std::make_shared<ActionCheckBox>(makeActionLabel(widgetInfo), widgetInfo);
}

namespace
{
	class PythonCombobox : public Combobox
	{
	public:
		explicit PythonCombobox(CvWidgetDataStruct widgetData, PythonScreenControlId pythonCtrlId) : mWidgetData(widgetData), mPythonScreenControlId(std::move(pythonCtrlId))
		{
		}

		virtual void onSelectionChanged() override
		{
			(void)doPythonScreenEventHandling(mWidgetData, mPythonScreenControlId, cvengine::NOTIFY_LISTBOX_ITEM_SELECTED, cvengine::MOUSE_LBUTTON | cvengine::MOUSE_LBUTTONUP);
		}

	private:
		CvWidgetDataStruct mWidgetData{};
		PythonScreenControlId mPythonScreenControlId;
	};
}

std::shared_ptr<hecktui::Combobox> cvengine::makePythonCombobox(WidgetTypes widgetType, int actionIndex, int data2, PythonScreenControlId pythonCtrlId)
{
	return std::make_shared<PythonCombobox>(CvWidgetDataStruct{ actionIndex, data2, false, widgetType }, std::move(pythonCtrlId));
}


TurnMessageDisplay::TurnMessageDisplay() : mFirstSerial(CvTuiInterface::getInstance().getTuiMainInterface()->getFirstTurnMessageSerial())
{
	auto scrollPanel = std::make_shared<ScrollBarPanel>(EAxis::Vertical);
	scrollPanel->setLayoutSizeOverride({}, 2);
	addChild(scrollPanel);
	mClientPanel = scrollPanel->getPanel().get();
	mClientPanel->setLayout(std::make_unique<FlowLayout>(FlowConfig{
		.axis = EAxis::Vertical,
		.linesCrosswiseJustilign = EJustilign::Stretch,
		}));

	setLayout(std::make_unique<FillLayout>());
}

void TurnMessageDisplay::updateTurnMessageDisplay()
{
	const CvTuiMainInterface& ingameInterfaceController = *CvTuiInterface::getInstance().getTuiMainInterface();
	AudioSystem& audioSys = *CvApp::getInstance().audioSystem;

	while (mFirstSerial < ingameInterfaceController.getFirstTurnMessageSerial() && mClientPanel->getChildren().size())
	{
		mClientPanel->getChildren().back()->orphan();
		++mFirstSerial;
	}

	mFirstSerial = ingameInterfaceController.getFirstTurnMessageSerial();

	struct MessageRichLabel : RichLabel
	{
		heck::ivec2 location{};

		explicit MessageRichLabel(const CvTalkingHeadMessage& msg) : RichLabel(std::wstring_view()), location(msg.getX(), msg.getY())
		{
			colouring = { .text = msg.getFlashColor() != NO_COLOR ? cvengine::toTuiColour(msg.getFlashColor()) : EColour::Silver, .back = kTransparent };
			setLabel(msg.getDescription());
		}

		virtual bool onEvent(const ConsoleEvent& e)
		{
			if (RichLabel::onEvent(e))
				return true;

			// Same code in CvGTurnLogWindow.
			if (e.type == hecktui::EConsoleEventType::MouseButtonDown && static_cast<const hecktui::MouseButtonEvent&>(e).button == hecktui::EMouseButton::Left)
			{
				if (location.x >= 0)
					CvTuiInterface::getInstance().lookAtPlot(location);
				return true;
			}

			return false;
		}
	};

	const auto& newMessages = ingameInterfaceController.getTurnMessages().subspan(mClientPanel->getChildren().size());
	std::vector<std::shared_ptr<Element>> newChildren;
	for (const CvTalkingHeadMessage& msg : newMessages)
	{
		audioSys.playSound(msg.getSound());
		newChildren.push_back(std::make_shared<MessageRichLabel>(msg));
	}
	
	//if (!newMessages.empty())
	//	static_cast<ScrollBarPanel&>(*getChildAt(0)).scrollToEnd();

	std::ranges::reverse(newChildren);
	mClientPanel->insertChildren(0, newChildren);
}

void TurnMessageDisplay::drawThis(ivec2 offset, hecktui::Framebuffer& fb)
{
	//fb.drawFilledBox(offset + getRect(), { .colour{.text = EColour::Silver, .back = EColour::Black } });
	Element::drawThis(offset, fb);
}

PythonScreenSlider::PythonScreenSlider(int max, PythonScreenControlId pythonScreenControlId) : Slider(max), mPythonScreenControlId(std::move(pythonScreenControlId))
{
}
void PythonScreenSlider::onSliderValueChanged()
{
	(void)doPythonScreenEventHandling({ .m_iData1 = -1, .m_iData2 = -1, .m_bOption{}, .m_eWidgetType = WIDGET_PYTHON },
		mPythonScreenControlId, cvengine::NOTIFY_SLIDER_NEWSTOP, cvengine::MOUSE_LBUTTON | cvengine::MOUSE_LBUTTONDOWN);
}