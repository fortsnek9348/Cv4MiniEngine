#include "inc/HeckTextUI/LayoutBuilder.h"
#include "inc/HeckTextUI/Control.h"
#include "Util.h"

using namespace hecktui;

using namespace LayoutBuilder;

static int countCols(std::initializer_list<TableBuilderCell> row)
{
	return sum(row, 0, [](const TableBuilderCell& cell) {
		return cell.span.x;
	});
}

static void parseRow(Element& element, TableLayoutConfig& config, int rowI, std::initializer_list<LayoutBuilder::TableBuilderCell> row)
{
	if (config.cols.empty())
		config.cols.resize(countCols(row));
	else
		assert(static_cast<size_t>(countCols(row)) == config.cols.size());

	int colI = 0;
	for (const auto& builderCell : row)
	{
		if (builderCell.element)
		{
			TableLayoutConfig::Cell layoutCell{
				.coord{ colI, rowI },
				.span = builderCell.span,
				.align = builderCell.justilign,
			};
			config.cells.push_back(layoutCell);
			element.addChild(builderCell.element);
		}

		for (const int y : range(builderCell.span.y))
		{
			config.rows[rowI + y].min = std::max(config.rows[rowI + y].min, builderCell.flexWeight.y);
			config.rows[rowI + y].weight += builderCell.flexWeight.y;
			for (const int x : range(builderCell.span.x))
			{
				config.cols[colI + x].min = std::max(config.cols[colI + x].min, builderCell.flexWeight.x);
				config.cols[colI + x].weight += builderCell.flexWeight.x;
			}
		}
		
		colI += builderCell.span.x;
	}
}

std::shared_ptr<Element> LayoutBuilder::table(std::initializer_list<std::initializer_list<TableBuilderCell>> rows)
{
	auto element = std::make_shared<Element>();

	TableLayoutConfig config;
	config.rows.resize(rows.size());

	for (const auto& [i, row] : rows | std::views::enumerate)
		parseRow(*element, config, int(i), row);

	element->setLayout(std::make_unique<TableLayout>(std::move(config)));
	return element;
}