#pragma once

#include <CvGameCoreDLL/CommonShared.h>
#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pybind11.h>

#include <HeckTextUI/Layout.h>

#include <string>
#include <vector>

class CyUnit;
class CyReplayInfo;

namespace cvengine
{
	class CvGInterfaceScreen;

	class CyGInterfaceScreen
	{
	public:
		static void registerWithPython(const pybind11::module& m);

		explicit CyGInterfaceScreen(std::string name, int kind);

		CvGInterfaceScreen& impl;

		void clear();

		//void addChild(std::string parent, std::string child);
		void setParent(std::string child, std::string parent);
		void moveToFirst(std::string child);
		void delByPrefix(std::string prefix);
		//void delAllChildren(std::string parent);
		void show(std::string name);
		void hide(std::string name);
		void setVisible(std::string name, bool visible);
		void disable(std::string name);
		void setEnabled(std::string name, bool x);
		void setInitialTitle(std::wstring title);
		bool isActive() const;

		bool isPersistent() const;
		void setPersistent(bool);

		void setSound(std::string name);

		// Rarely used. Used for tech screen.
		void setControlRect(std::string name, const hecktui::iaabb2& rect);

		void newLabel             /**/(std::string parent, std::string name, std::wstring text, WidgetTypes widgetType, int actionIndex, int data2);
		void newClickThroughLabel /**/(std::string parent, std::string name, std::wstring text);
		void setLabelText(std::string name, std::wstring text);
		void setLabelHAlign(std::string name, hecktui::EAlign align);
		void setLabelWidgetData(std::string name, WidgetTypes widgetType, int actionIndex, int data2);
		void setLabelEnableWrapping(std::string name);

		void newPlotHelpLabel     /**/(std::string parent, std::string name);
		void newPanel             /**/(std::string parent, std::string name);
		void setPanelBackgroundColour(std::string name, std::optional<hecktui::EColour> colour);
		void newBoxPanel(const std::string& parent, std::string name, std::string contentPanelName, hecktui::EBorderStyle style, hecktui::EColour colour);

		void newResizableHSplit   /**/(std::string parent, std::string name, int initialSize, bool isBottomSize);
		void newResizableVSplit   /**/(std::string parent, std::string name, int initialSize, bool isRightSize);

		void newActionButton      /**/(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex, int data2, hecktui::EBorderStyle style);
		void newEmptyActionButton /**/(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex, int data2);
		void setActionButtonLabel(std::string name, std::wstring text);
		void setActionButtonHAlign(std::string name, hecktui::EAlign align);
		void setActionButtonBackground(std::string name, hecktui::EColour colour);
		void setActionButtonWidgetData(std::string name, WidgetTypes widgetType, int actionIndex, int data2);
		void setActionButtonForceDisableShiftClickHeck(std::string name);

		void newActionCheckBox    /**/(std::string parent, std::string name, WidgetTypes widgetType, int actionIndex);
		void setCheckBoxLabel     /**/(std::string name, std::wstring label);
		void setCheckBoxValue     /**/(std::string name, bool value);
		bool getCheckBoxValue     /**/(std::string name);
		void newPlotListUnitButton/**/(std::string parent, std::string name, CyUnit unit);
		void newWorldView         /**/(std::string parent, std::string name);
		void newHRule             /**/(std::string parent, std::string name, hecktui::EBorderStyle style);
		void newVRule             /**/(std::string parent, std::string name);

		void newGuage             /**/(std::string parent, std::string name, std::vector<ColorTypes> colours, WidgetTypes widgetType);
		void setGuageColours(std::string name, std::vector<ColorTypes> colours);
		void setGuageValues(std::string name, std::wstring textcode, int max, std::vector<int> ratios);

		void newCombobox(std::string parent, std::string name, std::vector<std::wstring> items, size_t initialSelectionI, WidgetTypes widgetType, int actionIndex, int data2);
		int getComboboxSelectedIndex(std::string name);

		void newTurnMessageDisplay(std::string parent, std::string name);
		void updateTurnMessageDisplay(std::string name);

		void newScrollSeekPanel(std::string parent, std::string name, std::string contentPanelName, hecktui::EAxis axis);
		void newScrollBarPanel(std::string parent, std::string name, std::string contentPanelName, hecktui::EAxis axis);

		void newMinimap(std::string parent, std::string name);
		void clearMinimapMarkers(std::string name);
		void addMinimapMarker(std::string name, int x, int y, int colourI, wchar_t character);
		void setMinimapHandleMouseMove(std::string name);
		void setMinimapBaseTextureToReplayInfo(const std::string& name, const CyReplayInfo& replayInfo);
		void setMinimapPlotCulture(const std::string& name, int x, int y, ColorTypes colour);

		void newRichTextTable(std::string parent, std::string name, const std::vector<std::wstring_view>& columnTitles, WidgetTypes widgetType, int actionIndex, int data2);
		void setRichTextTableColumnHAlign(std::string name, size_t i, hecktui::EAlign halign);
		void setRichTextTableExpandColumn(std::string name, size_t i);
		void addRichTextTableRow(std::string name, const std::vector<std::wstring_view>& cellTexts, std::vector<int> cellIntKeys);
		void clearRichTextTableRows(std::string name);
		int getRichTextTableActiveRowIndex(std::string name);
		std::vector<int> getRichTextTableSelectedRows(std::string name);
		void setRichTextTableSelectedRows(std::string name, std::vector<int> indices, int activeRowI);
		void setRichTextTableFlags(std::string name, bool showHeader, bool enableSelection, bool enableMultiSelect, bool showColLines);

		void newControl(const std::string& parent, std::string name, std::shared_ptr<hecktui::Element> ctrl);

		void setFillLayout(std::string ctrlName);
		void setHFlowLayout(std::string ctrlName, hecktui::EJustilign valign);
		void setHWrapLayout(std::string ctrlName);
		void setVFlowLayout(std::string ctrlName);

		void setTableLayout(std::string ctrlName, hecktui::TableLayoutConfig config);

		void newCanvas(std::string parent, std::string name);
		void drawOrthoGraphEdge(std::string ctrlName, hecktui::ivec2 from, hecktui::ivec2 to);

		void newLineGraph(const std::string& parent, std::string name);
		void clearLineGraph(const std::string& ctrlName);
		void addLineGraphSeries(const std::string& ctrlName, std::vector<int> values, ColorTypes colour);
		void setLineGraphXRange(const std::string& ctrlName, int firstX, int inclLastX);
		void setLineGraphXLabels(const std::string& ctrlName, std::vector<std::wstring> labels);
		void setLineGraphSeriesVisible(const std::string& ctrlName, size_t index, bool b);
		void setGraphProportionalMode(const std::string& ctrlName);

		void newSlider(const std::string& parent, std::string name, int max);
		int getSliderValue(const std::string& ctrlName);

		bool setSpaceShip(int activeProjectI);
		void spaceShipLaunch();

		void showScreen(PopupStates popupState, bool passInput);
		void hideScreen();
	};
}