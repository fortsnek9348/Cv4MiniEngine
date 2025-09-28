#pragma once

#include "MainInterfaceControls.h"

#include <HeckTextUI/BasicControls.h>

#include <unordered_set>

class ActionButton;

class RichTextTable : public hecktui::Element
{
public:
	explicit RichTextTable(CvWidgetDataStruct widgetData, PythonScreenControlId pythonScreenControlId);

	void addColumn(std::wstring_view titleTextcode);
	void setColumnAlign(size_t i, hecktui::EAlign halign);
	void setExpandColumn(size_t i);

	void clearRows();
	void addRow(std::span<const std::wstring_view> cells, std::vector<int> sortIntegers);
	size_t getNumRows() const;
	const std::unordered_set<size_t>& getSelectedRowsSet() const;
	size_t getActiveRowIndex() const;

	void setSelectedRows(std::span<const int> indices, size_t activeRowI);

	void setTableFlags(bool showHeader, bool enableSelection, bool enableMultiSelect, bool showColLines);

private:
	struct BareScrollableTable;
	struct HeaderPanel;
	struct TableDataPanel;

	bool mShowHeader = true;
	bool mEnableSelection = true;
	bool mEnableMultiSelect = true;
	bool mShowColLines = true;

	CvWidgetDataStruct mWidgetData{};
	PythonScreenControlId mPythonScreenControlId{};

	struct Column
	{
		std::shared_ptr<ActionButton> headerButton;
		std::shared_ptr<hecktui::Label> sortLabelCtrl;
		int contentMaxWidth = 0;
		hecktui::EAlign halign = hecktui::EAlign::Left;
	};

	std::vector<Column> mColumns;
	size_t mExpandColI = 0;

	struct Row
	{
		std::vector<hecktui::Pixel> pixels{};
		std::vector<size_t> colStarts{};
		std::vector<int> sortIntegers;

		std::span<const hecktui::Pixel> getCellPixels(size_t colI) const;
	};

	std::vector<Row> mRows;

	size_t mActiveRowI = SIZE_MAX;
	size_t mSelectionAnchorRowI = SIZE_MAX;
	std::unordered_set<size_t> mSelectedRows;

	size_t mSortColumnI = 0;
	int mSortMode = 0; // No sort, asc, desc
	std::vector<size_t> mSortedRowList; // empty if dirty

	void toggleSort(size_t colI);
	void updateSortedRowList();
};