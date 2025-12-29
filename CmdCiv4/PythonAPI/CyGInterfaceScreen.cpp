#include "CyGInterfaceScreen.h"
#include "CyUnit.h"
#include "CvPythonExtensions.h"
#include "../Common.h"
#include "../CvInterface.h"
#include "../CvEngineEnums.h"
#include "../CvGInterfaceScreen.h"
#include "../MainInterfaceControls.h"
#include "../MainInterface.h"
#include "../TuiTextCode.h"
#include "../Minimap.h"
#include "../RichTextTable.h"
#include "../RichGuage.h"
#include "../LineGraph.h"
#include "../UITheme.h"

#include <CvGameAI.h>
#include <CvGlobals.h>
#include <CvPlayerAI.h>
#include <CvTeamAI.h>
#include <CvMessageControl.h>
#include <CyReplayInfo.h>

#include <HeckTextUI/BasicControls.h>
#include <HeckTextUI/Combobox.h>
#include <HeckTextUI/Slider.h>

#include <pybind11/stl.h>

#include <ranges>

using namespace hecktui;
using namespace cvengine;

void CyGInterfaceScreen::registerWithPython(const pybind11::module& m)
{
#define R(x, ...) def(#x, &CyGInterfaceScreen::x __VA_OPT__(,) __VA_ARGS__)
#define V(ns, x) value(#x, ns::x)
#define M(ns, x) def_readwrite(#x, &ns::x)

	pybind11::enum_<hecktui::EBorderStyle>(m, "HeckTuiBorderStyle")
		.value("NONE", hecktui::EBorderStyle::None)
		.V(hecktui::EBorderStyle, Thin)
		.V(hecktui::EBorderStyle, Thick)
		.V(hecktui::EBorderStyle, Double)
		.V(hecktui::EBorderStyle, Rounded)
		.V(hecktui::EBorderStyle, Bracketed)
		;

	pybind11::enum_<hecktui::EAlign>(m, "EAlign")
		.V(hecktui::EAlign, Start)
		.V(hecktui::EAlign, Center)
		.V(hecktui::EAlign, End)

		.V(hecktui::EAlign, Left)
		.V(hecktui::EAlign, Right)

		.V(hecktui::EAlign, Top)
		.V(hecktui::EAlign, Middle)
		.V(hecktui::EAlign, Bottom)
		;

	pybind11::enum_<hecktui::EJustilign>(m, "EJustilign")
		.V(hecktui::EJustilign, Start)
		.V(hecktui::EJustilign, Center)
		.V(hecktui::EJustilign, End)
		.V(hecktui::EJustilign, Stretch)
		.V(hecktui::EJustilign, SpaceBetween)
		;

	pybind11::class_<hecktui::RectJustilign>(m, "RectJustilign")
		.def(pybind11::init<hecktui::EJustilign, hecktui::EJustilign>())
		.M(hecktui::RectJustilign, halign)
		.M(hecktui::RectJustilign, valign)
		;

	pybind11::class_<hecktui::ivec2>(m, "ivec2")
		.def(pybind11::init<int, int>())
		.M(hecktui::ivec2, x)
		.M(hecktui::ivec2, y)
		;

	pybind11::class_<hecktui::iaabb2>(m, "iaabb2")
		.def_static("sized", &iaabb2::sized)
		.def_readwrite("position", &iaabb2::min)
		.def_readwrite("maximum", &iaabb2::max)
		.def_property_readonly("size", [](const iaabb2& rect) { return rect.size(); })
		;

	pybind11::class_<hecktui::TableLayoutConfig::Cell>(m, "TableLayoutConfigCell")
		.def(pybind11::init<hecktui::ivec2>())
		.def(pybind11::init<hecktui::ivec2, hecktui::ivec2>())
		.def(pybind11::init<hecktui::ivec2, hecktui::ivec2, hecktui::RectJustilign>())
		.M(hecktui::TableLayoutConfig::Cell, coord)
		.M(hecktui::TableLayoutConfig::Cell, span)
		.M(hecktui::TableLayoutConfig::Cell, align)
		;

	pybind11::class_<hecktui::TableLayoutConfig::RowColDesc>(m, "TableLayoutConfigRowColDesc")
		.def(pybind11::init<int, int>())
		.M(hecktui::TableLayoutConfig::RowColDesc, min)
		.M(hecktui::TableLayoutConfig::RowColDesc, weight)
		;

	pybind11::class_<hecktui::TableLayoutConfig>(m, "TableLayoutConfig")
		.def(pybind11::init())
		.M(hecktui::TableLayoutConfig, cols)
		.M(hecktui::TableLayoutConfig, rows)
		.M(hecktui::TableLayoutConfig, cells)
		.M(hecktui::TableLayoutConfig, repeatAxis)
		.M(hecktui::TableLayoutConfig, gap)
		;

	pybind11::enum_<hecktui::EColour>(m, "HeckTuiColour")
		.V(hecktui::EColour, Black)
		.V(hecktui::EColour, Red)
		.V(hecktui::EColour, Green)
		.V(hecktui::EColour, Grey100)
		.def_static("fromColourType", [](int i) { return cvengine::toTuiColour(ColorTypes(i)); })
		;



	pybind11::enum_<hecktui::EAxis>(m, "HeckTuiAxis")
		.V(hecktui::EAxis, Horizontal)
		.V(hecktui::EAxis, Vertical)
		;

	pybind11::enum_<CvGInterfaceScreen::EAutoSizeBehaviour>(m, "HeckTuiAutoSizeBehaviour")
		.V(CvGInterfaceScreen::EAutoSizeBehaviour, Minimum)
		.V(CvGInterfaceScreen::EAutoSizeBehaviour, GrowOnly)
		.V(CvGInterfaceScreen::EAutoSizeBehaviour, Maximise)
		;

	pybind11::class_<CyGInterfaceScreen>(m, "CyGInterfaceScreen")
		.def(pybind11::init<std::string, int>())
		.R(clear)
		.R(setParent)
		.R(moveToFirst)
		.R(delByPrefix)
		.R(show)
		.R(hide)
		.R(setVisible)
		.R(disable)
		.R(setEnabled)
		.R(setInitialTitle)
		.def("setAutoSizeBehaviour", [](CyGInterfaceScreen& self, CvGInterfaceScreen::EAutoSizeBehaviour b) { self.impl.setAutoSizeBehaviour(b); })
		.def("setModal", [](CyGInterfaceScreen& self, bool b) { self.impl.setModal(b); })
		.R(setControlRect)
		.R(isActive)
		.R(isPersistent)
		.R(setPersistent)
		.R(setSound)
		.R(newLabel, "Create a new label with optional widget data",
			pybind11::arg("parent"), pybind11::arg("name"), pybind11::arg("text"),
			pybind11::arg("widgetType") = WidgetTypes::WIDGET_GENERAL,
			pybind11::arg("actionIndex") = -1,
			pybind11::arg("data2") = -1
		)
		.R(newClickThroughLabel)
		.R(newPlotHelpLabel)
		.R(setLabelText)
		.R(setLabelHAlign)
		.R(setLabelWidgetData)
		.R(setLabelEnableWrapping)
		.R(newPanel)
		.R(setPanelBackgroundColour)
		.R(newBoxPanel, "Create a bordered panel.",
			pybind11::arg("parent"), pybind11::arg("name"), pybind11::arg("contentPanelName"),
			pybind11::arg("borderStyle"), pybind11::arg("borderColour") = EColour::Silver
		)
		.R(newResizableHSplit)
		.R(newResizableVSplit)
		.R(newActionButton, "Create a new action button",
			pybind11::arg("parent"), pybind11::arg("name"), pybind11::arg("widgetType"),
			pybind11::arg("actionIndex"), pybind11::arg("data2"), pybind11::arg("style") = hecktui::EBorderStyle::Rounded
		)
		.R(newEmptyActionButton)
		.R(setActionButtonLabel)
		.R(setActionButtonHAlign)
		.R(setActionButtonBackground)
		.R(setActionButtonWidgetData)
		.R(setActionButtonForceDisableShiftClickHeck)
		.R(newActionCheckBox)
		.R(setCheckBoxLabel)
		.R(setCheckBoxValue)
		.R(getCheckBoxValue)
		.R(newPlotListUnitButton)
		.R(newWorldView)
		.R(newHRule, "Create a new horizontal separator",
			pybind11::arg("parent"),
			pybind11::arg("name"),
			pybind11::arg("borderStyle") = hecktui::EBorderStyle::Thick
		)
		.R(newVRule)
		.R(newGuage)
		.R(setGuageColours)
		.R(setGuageValues)
		.R(newCombobox)
		.R(getComboboxSelectedIndex)
		.R(newTurnMessageDisplay)
		.R(updateTurnMessageDisplay)
		.R(newScrollSeekPanel)
		.R(newScrollBarPanel)
		.R(newMinimap)
		.R(clearMinimapMarkers)
		.R(addMinimapMarker)
		.R(setMinimapHandleMouseMove)
		.R(setMinimapBaseTextureToReplayInfo)
		.R(setMinimapPlotCulture)
		.R(newRichTextTable)
		.R(setRichTextTableColumnHAlign)
		.R(setRichTextTableExpandColumn)
		.R(addRichTextTableRow)
		.R(clearRichTextTableRows)
		.R(getRichTextTableActiveRowIndex)
		.R(getRichTextTableSelectedRows)
		.R(setRichTextTableSelectedRows)
		.R(setRichTextTableFlags, "Set RichTextTable flags.",
			pybind11::arg("name"),
			pybind11::arg("showHeader") = true,
			pybind11::arg("enableSelection") = true,
			pybind11::arg("enableMultiSelect") = true,
			pybind11::arg("showColLines") = true
		)
		.R(setFillLayout)
		.R(setHFlowLayout, "Set layout to HFlow.", pybind11::arg("name"), pybind11::arg("valign") = EJustilign::Stretch)
		.R(setHWrapLayout)
		.R(setVFlowLayout)
		.R(setTableLayout)
		.R(newCanvas)
		.R(drawOrthoGraphEdge)
		.R(newLineGraph)
		.R(clearLineGraph)
		.R(addLineGraphSeries)
		.R(setLineGraphXRange)
		.R(setLineGraphXLabels)
		.R(setLineGraphSeriesVisible)
		.R(setGraphProportionalMode)
		.R(newSlider)
		.R(getSliderValue)
		.R(setSpaceShip)
		.R(spaceShipLaunch)
		.R(showScreen)
		.R(hideScreen)
		;
}

// Screens appear to be stored globally somewhere.
CyGInterfaceScreen::CyGInterfaceScreen(std::string name, int kind) : impl(CvInterface::getInstance().grabPythonScreen(std::move(name), cvengine::ECvScreen(kind)))
{
}

void CyGInterfaceScreen::clear()
{
	impl.clear();
}

void CyGInterfaceScreen::setParent(std::string child, std::string parent)
{
	impl.at(parent)->addChild(impl.at(child));
}
void CyGInterfaceScreen::moveToFirst(std::string child)
{
	impl.at(child)->moveToFirst();
}

/*void CyGInterfaceScreen::addChild(std::string parent, std::string child)
{
	impl.at(parent)->addChild(impl.at(child));
}*/
void CyGInterfaceScreen::delByPrefix(std::string prefix)
{
	impl.delByPrefix(prefix);
}
//void CyGInterfaceScreen::delAllChildren(std::string parent)
//{
//	impl.at(parent)->removeAllChildren();
//	impl.garbageCollect();
//}
void CyGInterfaceScreen::show(std::string name)
{
	impl.at(name)->setVisible(true);
}
void CyGInterfaceScreen::hide(std::string name)
{
	impl.at(name)->setVisible(false);
}
void CyGInterfaceScreen::setVisible(std::string name, bool visible)
{
	impl.at(name)->setVisible(visible);
}
void CyGInterfaceScreen::disable(std::string name)
{
	impl.at(name)->setEnabled(false);
}
void CyGInterfaceScreen::setEnabled(std::string name, bool x)
{
	impl.at(name)->setEnabled(x);
}

void CyGInterfaceScreen::setInitialTitle(std::wstring title)
{
	impl.setInitialTitle(std::move(title));
}

bool CyGInterfaceScreen::isActive() const
{
	return impl.isActive();
}

bool CyGInterfaceScreen::isPersistent() const
{
	return impl.isPersistent();
}
void CyGInterfaceScreen::setPersistent(bool x)
{
	return impl.setPersistent(x);
}

void CyGInterfaceScreen::setSound(std::string name)
{
	impl.setSoundName(std::move(name));
}

void CyGInterfaceScreen::setControlRect(std::string name, const hecktui::iaabb2& rect)
{
	impl.at(name)->setRect(rect);
}

void CyGInterfaceScreen::newScrollSeekPanel(std::string parent, std::string name, std::string contentPanelName, hecktui::EAxis axis)
{
	auto ctrl = std::make_shared<hecktui::ScrollSeekPanel>(axis);
	newControl(parent, name, ctrl);
	impl.set(contentPanelName, ctrl->getPanel());
}

void CyGInterfaceScreen::newScrollBarPanel(std::string parent, std::string name, std::string contentPanelName, hecktui::EAxis axis)
{
	auto ctrl = std::make_shared<hecktui::ScrollBarPanel>(axis);
	//if (name == "InfoTableScrollPanel")
	//	ctrl->debug = true;
	newControl(parent, name, ctrl);
	impl.set(contentPanelName, ctrl->getPanel());
}

void CyGInterfaceScreen::newMinimap(std::string parent, std::string name)
{
	auto ctrl = std::make_shared<Minimap>();
	ctrl->screenEventSource = impl.getKind();
	newControl(parent, name, std::move(ctrl));
}

void CyGInterfaceScreen::clearMinimapMarkers(std::string name)
{
	static_cast<Minimap&>(*impl.at(name)).clearMinimapMarkers();
}
void CyGInterfaceScreen::addMinimapMarker(std::string name, int x, int y, int colourI, wchar_t character)
{
	static_cast<Minimap&>(*impl.at(name)).addMinimapMarker({ { x, y }, ColorTypes(colourI), character });
}
void CyGInterfaceScreen::setMinimapHandleMouseMove(std::string name)
{
	static_cast<Minimap&>(*impl.at(name)).handleMouseMove = true;
}
void CyGInterfaceScreen::setMinimapBaseTextureToReplayInfo(const std::string& name, const CyReplayInfo& replayInfo)
{
	static_cast<Minimap&>(*impl.at(name)).setMinimapBaseTextureFromReplay(*replayInfo.getReplayInfo());
}
void CyGInterfaceScreen::setMinimapPlotCulture(const std::string& name, int x, int y, ColorTypes colour)
{
	static_cast<Minimap&>(*impl.at(name)).setPlotCulture({ x, y }, colour);
}

void CyGInterfaceScreen::newRichTextTable(std::string parent, std::string name, const std::vector<std::wstring_view>& columnTitles, WidgetTypes widgetType, int actionIndex, int data2)
{
	auto ctrl = std::make_shared<RichTextTable>(CvWidgetDataStruct{ actionIndex, data2, false, widgetType }, PythonScreenControlId{ .screen = impl.getKind() });
	for (const auto& title : columnTitles)
		ctrl->addColumn(title);
	newControl(parent, name, std::move(ctrl));
}

void CyGInterfaceScreen::setRichTextTableColumnHAlign(std::string name, size_t i, hecktui::EAlign halign)
{
	static_cast<RichTextTable&>(*impl.at(name)).setColumnAlign(i, halign);
}

void CyGInterfaceScreen::setRichTextTableExpandColumn(std::string name, size_t i)
{
	static_cast<RichTextTable&>(*impl.at(name)).setExpandColumn(i);
}

void CyGInterfaceScreen::addRichTextTableRow(std::string name, const std::vector<std::wstring_view>& cellTexts, std::vector<int> cellIntKeys)
{
	static_cast<RichTextTable&>(*impl.at(name)).addRow(cellTexts, std::move(cellIntKeys));
}

void CyGInterfaceScreen::clearRichTextTableRows(std::string name)
{
	static_cast<RichTextTable&>(*impl.at(name)).clearRows();
}

int CyGInterfaceScreen::getRichTextTableActiveRowIndex(std::string name)
{
	const auto& table = static_cast<RichTextTable&>(*impl.at(name));
	const size_t index = table.getActiveRowIndex();
	return index < table.getNumRows() ? int(index) : -1;
}
std::vector<int> CyGInterfaceScreen::getRichTextTableSelectedRows(std::string name)
{
	return static_cast<RichTextTable&>(*impl.at(name)).getSelectedRowsSet() | std::views::transform([](size_t i) { return int(i); }) | std::ranges::to<std::vector<int>>();
}
void CyGInterfaceScreen::setRichTextTableSelectedRows(std::string name, std::vector<int> indices, int activeRowI)
{
	static_cast<RichTextTable&>(*impl.at(name)).setSelectedRows(indices, activeRowI);
}

void CyGInterfaceScreen::setRichTextTableFlags(std::string name, bool showHeader, bool enableSelection, bool enableMultiSelect, bool showColLines)
{
	auto& table = static_cast<RichTextTable&>(*impl.at(name));
	table.setTableFlags(showHeader, enableSelection, enableMultiSelect, showColLines);
}

void CyGInterfaceScreen::newControl(const std::string& parent, std::string name, std::shared_ptr<hecktui::Element> ctrl)
{
	//if (name == "PlayerListScrollPanel")
	//	ctrl->debug = true;
	impl.at(parent)->addChild(ctrl);
	impl.set(std::move(name), std::move(ctrl));
}

void CyGInterfaceScreen::newLabel(std::string parent, std::string name, std::wstring text, WidgetTypes widgetType, int actionIndex, int data2)
{
	auto ctrl = std::make_shared<RichLabel>(text);
	ctrl->widgetData = { .m_iData1 = actionIndex, .m_iData2 = data2, .m_bOption = false, .m_eWidgetType = widgetType };
	newControl(parent, name, std::move(ctrl));
}
void CyGInterfaceScreen::newClickThroughLabel(std::string parent, std::string name, std::wstring text)
{
	auto ctrl = std::make_shared<RichLabel>(L"", false);
	ctrl->colouring = { hecktui::kTransparent, hecktui::kTransparent };
	ctrl->setLabel(text);
	newControl(parent, name, std::move(ctrl));
}
void CyGInterfaceScreen::newPlotHelpLabel(std::string parent, std::string name)
{
	auto ctrl = std::make_shared<RichLabel>(L"", false);
	// TODO: Plot help should have a min size of zero, stretch to the size of the world view, and cut off the top lines.
	ctrl->enableExtraSpace = true;
	newControl(parent, name, std::move(ctrl));
	impl.setPlotHelpTarget(std::move(name));
}
void CyGInterfaceScreen::setLabelText(std::string name, std::wstring text)
{
	static_cast<RichLabel&>(*impl.at(name)).setLabel(std::move(text));
}
void CyGInterfaceScreen::setLabelHAlign(std::string name, hecktui::EAlign align)
{
	static_cast<RichLabel&>(*impl.at(name)).setLabelAlignment(align);
}
void CyGInterfaceScreen::setLabelWidgetData(std::string name, WidgetTypes widgetType, int actionIndex, int data2)
{
	static_cast<RichLabel&>(*impl.at(name)).widgetData = { .m_iData1 = actionIndex, .m_iData2 = data2, .m_bOption = false, .m_eWidgetType = widgetType };
}
void CyGInterfaceScreen::setLabelEnableWrapping(std::string name)
{
	static_cast<RichLabel&>(*impl.at(name)).enableWrapping = true;
}

void CyGInterfaceScreen::setActionButtonLabel(std::string name, std::wstring text)
{
	static_cast<ActionButton&>(*impl.at(name)).setLabel(std::move(text));
}

void CyGInterfaceScreen::setActionButtonHAlign(std::string name, hecktui::EAlign align)
{
	static_cast<ActionButton&>(*impl.at(name)).setLabelAlignment(align);
}

void CyGInterfaceScreen::setActionButtonBackground(std::string name, hecktui::EColour colour)
{
	static_cast<ActionButton&>(*impl.at(name)).setBackgroundColour(colour);
}

void CyGInterfaceScreen::setActionButtonWidgetData(std::string name, WidgetTypes widgetType, int actionIndex, int data2)
{
	static_cast<ActionButton&>(*impl.at(name)).widgetData = { .m_iData1 = actionIndex, .m_iData2 = data2, .m_bOption = false, .m_eWidgetType = widgetType };
}

void CyGInterfaceScreen::setActionButtonForceDisableShiftClickHeck(std::string name)
{
	static_cast<ActionButton&>(*impl.at(name)).forceDisableShiftClickHeck();
}

namespace
{
	struct Panel : Element
	{
		std::optional<PixelColouring> colouring{};

		virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
		{
			// If not set, don't overwrite the background.
			if (colouring)
				fb.drawFilledBox(offset + getRect(), { .colour = *colouring });
		}
	};
}

void CyGInterfaceScreen::newPanel(std::string parent, std::string name)
{
	newControl(parent, name, std::make_shared<Panel>());
}
void CyGInterfaceScreen::setPanelBackgroundColour(std::string name, std::optional<hecktui::EColour> colour)
{
	static_cast<Panel&>(*impl.at(name)).colouring = PixelColouring{ .back = colour ? *colour : kTransparent };
}
void CyGInterfaceScreen::newBoxPanel(const std::string& parent, std::string name, std::string contentPanelName, hecktui::EBorderStyle style, hecktui::EColour colour)
{
	auto ctrl = std::make_shared<BoxPanel>(style, PixelColouring{ .text = colour });
	auto layout = std::make_unique<FillLayout>();
	layout->marginTopLeft = layout->marginBottomRight = { 1, 1 };
	ctrl->setLayout(std::move(layout));
	newControl(parent, name, std::move(ctrl));
	newControl(name, std::move(contentPanelName), std::make_shared<Panel>());
}
void CyGInterfaceScreen::newResizableHSplit(std::string parent, std::string name, int initialSize, bool isBottomSize)
{
	newControl(parent, name, std::make_shared<hecktui::ResizablePanel>(hecktui::EAxis::Vertical, initialSize, isBottomSize));
}
void CyGInterfaceScreen::newResizableVSplit(std::string parent, std::string name, int initialSize, bool isRightSize)
{
	newControl(parent, name, std::make_shared<hecktui::ResizablePanel>(hecktui::EAxis::Horizontal, initialSize, isRightSize));
}
void CyGInterfaceScreen::newActionButton(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex, int data2, hecktui::EBorderStyle style)
{
	auto ctrl = makeActionButtonWithAutoLabel(gGlobals.getGame(), CvInterface::getInstance(), widgetType, actionIndex, data2);
	ctrl->setBorderStyle(style);
	if (widgetType == WIDGET_CLOSE_SCREEN)
	{
		ctrl->setOnClickCallback([&impl = impl] {
			impl.setWantClose(true);
		});
	}
	//else if (widgetType == WIDGET_PYTHON) // Cv4MiniEngine change. Only WIDGET_PYTHON controls get event handling in python screens.
	{
		ctrl->setPythonScreenControlId({
			.screen = impl.getKind(),
			//.name = name,
			});
	}
	newControl(parent, name, std::move(ctrl));
}

void CyGInterfaceScreen::newEmptyActionButton(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex, int data2)
{
	auto ctrl = makeEmptyActionButton({ .m_iData1 = actionIndex, .m_iData2 = data2, .m_bOption = false, .m_eWidgetType = widgetType });
	ctrl->setBorderStyle(EBorderStyle::None);
	newControl(parent, name, std::move(ctrl));
}
void CyGInterfaceScreen::newActionCheckBox    /**/(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex)
{
	auto ctrl = makeActionCheckBoxWithAutoLabel(CvInterface::getInstance(), widgetType, actionIndex);
	ctrl->setPythonScreenControlId({
			.screen = impl.getKind(),
			//.name = name,
		});
	ctrl->setColouring({
		.text = theme::kButtonDefaultLabelColour,
		.back = EColour::Black,
		});
	newControl(parent, name, std::move(ctrl));
}
void CyGInterfaceScreen::setCheckBoxLabel     /**/(std::string name, std::wstring label)
{
	static_cast<hecktui::Checkbox&>(*impl.at(name)).setLabel(std::move(label));
}
void CyGInterfaceScreen::setCheckBoxValue     /**/(std::string name, bool value)
{
	static_cast<hecktui::Checkbox&>(*impl.at(name)).value = value;
}
bool CyGInterfaceScreen::getCheckBoxValue     /**/(std::string name)
{
	return static_cast<hecktui::Checkbox&>(*impl.at(name)).value;
}
void CyGInterfaceScreen::newPlotListUnitButton(std::string parent, std::string name, CyUnit unit)
{
	newControl(parent, name, std::make_shared<PlotListUnitButton>(unit.getUnit()));
}
void CyGInterfaceScreen::newWorldView(std::string parent, std::string name)
{
	newControl(parent, name, buildMainInterfaceWorldViewComponent(CvInterface::getInstance().getWorldView()));
}

void CyGInterfaceScreen::newHRule(std::string parent, std::string name, hecktui::EBorderStyle style)
{
	newControl(parent, name, std::make_shared<hecktui::HorizontalRule>(style));
}
void CyGInterfaceScreen::newVRule(std::string parent, std::string name)
{
	newControl(parent, name, std::make_shared<hecktui::VerticalRule>(hecktui::EBorderStyle::Thick));
}

static std::vector<EColour> toTuiColours(std::span<const ColorTypes> colours)
{
	return colours | std::views::transform([](ColorTypes c) { return cvengine::toTuiColour(c); }) | std::ranges::to<std::vector>();
}

void CyGInterfaceScreen::newGuage(std::string parent, std::string name, std::vector<ColorTypes> colours, WidgetTypes widgetType)
{
	newControl(parent, name, std::make_shared<RichGuage>(toTuiColours(colours), CvWidgetDataStruct{
		.m_iData1 = -1,
		.m_iData2 = -1,
		.m_bOption = false,
		.m_eWidgetType = widgetType
		}));
}
void CyGInterfaceScreen::setGuageColours(std::string name, std::vector<ColorTypes> colours)
{
	auto& guage = static_cast<RichGuage&>(*impl.at(name));
	guage.colours = toTuiColours(colours);
}
void CyGInterfaceScreen::setGuageValues(std::string name, std::wstring textcode, int max, std::vector<int> ratios)
{
	auto& guage = static_cast<RichGuage&>(*impl.at(name));
	guage.actionButton->setLabel(textcode, { .colour{.text = Colour(), .back = Colour() } });
	guage.actionButton->getLabel().enableAutoContrast = true;
	guage.max = max;
	guage.ratios = std::move(ratios);
}

void CyGInterfaceScreen::newCombobox(std::string parent, std::string name, std::vector<std::wstring> items, size_t initialSelectionI, WidgetTypes widgetType, int actionIndex, int data2)
{
	auto ctrl = makePythonCombobox(widgetType, actionIndex, data2, { .screen = impl.getKind() });
	ctrl->setListItems(std::move(items));
	ctrl->setSelectionIndex(initialSelectionI);
	newControl(parent, name, std::move(ctrl));
}

int CyGInterfaceScreen::getComboboxSelectedIndex(std::string name)
{
	return (int)static_cast<Combobox&>(*impl.at(name)).getSelectionIndex();
}

void CyGInterfaceScreen::newTurnMessageDisplay(std::string parent, std::string name)
{
	newControl(parent, name, std::make_shared<TurnMessageDisplay>());
}

void CyGInterfaceScreen::updateTurnMessageDisplay(std::string ctrlName)
{
	static_cast<TurnMessageDisplay&>(*impl.at(ctrlName)).updateTurnMessageDisplay();
}

void CyGInterfaceScreen::setFillLayout(std::string ctrlName)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FillLayout>());
}
void CyGInterfaceScreen::setHFlowLayout(std::string ctrlName, EJustilign valign)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
		.axis = hecktui::EAxis::Horizontal,
		.itemsCrosswiseJustilign = valign,
		}));
}
void CyGInterfaceScreen::setHWrapLayout(std::string ctrlName)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
		.axis = hecktui::EAxis::Horizontal,
		.wrap = true,
		.itemsCrosswiseJustilign = EJustilign::Stretch,
		}));
}
void CyGInterfaceScreen::setVFlowLayout(std::string ctrlName)
{
	impl.at(ctrlName)->setLayout(std::make_unique<hecktui::FlowLayout>(hecktui::FlowConfig{
		.axis = hecktui::EAxis::Vertical,
		.linesCrosswiseJustilign = EJustilign::Stretch
		}));
}
void CyGInterfaceScreen::setTableLayout(std::string ctrlName, hecktui::TableLayoutConfig config)
{
	auto layout = std::make_unique<hecktui::TableLayout>(std::move(config));
	if (ctrlName == "" && impl.getKind() == cvengine::ECvScreen::SPACE_SHIP_SCREEN)
		layout->debug = true;
	impl.at(ctrlName)->setLayout(std::move(layout));
}

namespace
{
	struct CanvasElement : Element
	{
		struct OrthoGraphEdge
		{
			ivec2 from{};
			ivec2 to{};
		};

		hecktui::Pixel defPixel{
			.autoMergeBorder = true,
		};

		std::vector<OrthoGraphEdge> edges;

		CanvasElement()
		{
			mCanFocus = false;
			mIsMouseInteractable = false;
		}

		virtual void drawThis(ivec2 offset, hecktui::Framebuffer& fb) override
		{
			offset += getRect().min;
			for (auto [from, to] : edges)
			{
				from += offset;
				to += offset;

				defPixel.c = L'─';
				if (from.y == to.y)
					fb.drawHLine(from, to.x - from.x, defPixel);
				else
				{
					const int midX = (from.x + to.x) / 2;
					const ivec2 midFrom{ midX, from.y };
					const ivec2 midTo{ midX, to.y };
					fb.drawHLine(from, midX - from.x, defPixel);
					fb.drawHLine(midTo, to.x - midX, defPixel);
					defPixel.c = L'│';
					fb.drawVLine(midFrom, to.y - from.y, defPixel);
					defPixel.c = from.y < to.y ? L'┐' : L'┘';
					fb.draw(midFrom, defPixel);
					defPixel.c = from.y < to.y ? L'└' : L'┌';
					fb.draw(midTo, defPixel);
				}
				defPixel.c = L'→';
				fb.draw(to, defPixel);

				if (fb.getScissorRect().contains(from))
					fb.getPixel(from).autoMergeBorder = false;
			}
		}
	};
}

void CyGInterfaceScreen::newCanvas(std::string parent, std::string name)
{
	newControl(parent, name, std::make_shared<CanvasElement>());
}

void CyGInterfaceScreen::drawOrthoGraphEdge(std::string ctrlName, hecktui::ivec2 from, hecktui::ivec2 to)
{
	static_cast<CanvasElement&>(*impl.at(ctrlName)).edges.emplace_back(from, to);
}

void CyGInterfaceScreen::newLineGraph(const std::string& parent, std::string name)
{
	newControl(parent, name, std::make_shared<LineGraph>());
}
void CyGInterfaceScreen::clearLineGraph(const std::string& ctrlName)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).clearSeries();
}
void CyGInterfaceScreen::addLineGraphSeries(const std::string& ctrlName, std::vector<int> values, ColorTypes colour)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).addSeries(std::move(values), colour);
}
void CyGInterfaceScreen::setLineGraphXRange(const std::string& ctrlName, int firstX, int inclLastX)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).setRange(firstX, inclLastX);
}
void CyGInterfaceScreen::setLineGraphXLabels(const std::string& ctrlName, std::vector<std::wstring> labels)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).setXLabels(std::move(labels));
}
void CyGInterfaceScreen::setLineGraphSeriesVisible(const std::string& ctrlName, size_t index, bool b)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).setSeriesVisible(index, b);
}
void CyGInterfaceScreen::setGraphProportionalMode(const std::string& ctrlName)
{
	static_cast<LineGraph&>(*impl.at(ctrlName)).setProportionalMode();
}

void CyGInterfaceScreen::newSlider(const std::string& parent, std::string name, int max)
{
	newControl(parent, name, std::make_shared<PythonScreenSlider>(max, PythonScreenControlId{ .screen = impl.getKind() }));
}
int CyGInterfaceScreen::getSliderValue(const std::string& ctrlName)
{
	return static_cast<PythonScreenSlider&>(*impl.at(ctrlName)).getValue();
}

bool CyGInterfaceScreen::setSpaceShip([[maybe_unused]] int activeProjectI)
{
	CvGame& game = gGlobals.getGame();
	// Cv4MiniEngine extension.
	return CvTeamAI::getTeamNonInl(game.getActiveTeam()).hasLaunched();
}

void CyGInterfaceScreen::spaceShipLaunch()
{
	CvGame& game = gGlobals.getGame();
	const VictoryTypes victoryI = game.getSpaceVictory(); // This is called first
	CvPlayerAI::getPlayerNonInl(game.getActivePlayer()).clearSpaceShipPopups();
	hideScreen(); // Probably.
	if (!CvTeamAI::getTeamNonInl(game.getActiveTeam()).hasLaunched())
	{
		// Also calls CvTeam::getCompletedSpaceshipProjects. Just for art? Doesn't seem to check for completed projects.

		CvMessageControl::getInstance().sendLaunch(gGlobals.getGame().getActivePlayer(), victoryI);
	}
}

void CyGInterfaceScreen::showScreen(PopupStates popupState, bool passInput)
{
	CvInterface::getInstance().showScreen(impl.getKind(), popupState, passInput);
}

void CyGInterfaceScreen::hideScreen()
{
	impl.setWantClose(true);
}