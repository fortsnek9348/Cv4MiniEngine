#include "inc/HeckTextUI/Layout.h"
#include "inc/HeckTextUI/Control.h"
#include "Util.h"

#include <algorithm>
#include <ranges>
#include <numeric>
#include <iostream>

using namespace hecktui;

namespace
{
	struct RowColSize
	{
		int minimum = 0;
		int preferred = 0;
	};

	void mergeMinPrefWidth(std::span<RowColSize> layoutInfo, size_t i, size_t span, const LayoutSizeInfo& ctrlSizeInfo, const int ivec2::* x)
	{
		span = std::min(span, layoutInfo.size() - i);
		const int currentMinWidth = sum(layoutInfo.subspan(i, span) | std::views::transform(&RowColSize::minimum), 0);
		const int currentPrefWidth = sum(layoutInfo.subspan(i, span) | std::views::transform(&RowColSize::preferred), 0);
		if (ctrlSizeInfo.minimum.*x > currentMinWidth)
			layoutInfo[i + span - 1].minimum += ctrlSizeInfo.minimum.*x - currentMinWidth;
		if (ctrlSizeInfo.preferred.*x > currentPrefWidth)
			layoutInfo[i + span - 1].preferred += ctrlSizeInfo.preferred.*x - currentPrefWidth;
	}


	std::vector<RowColSize> initLayoutInfo(std::span<const TableLayoutConfig::RowColDesc> sizeInfos)
	{
		std::vector<RowColSize> rows(sizeInfos.size());

		for (const auto& [sizeInfo, layoutInfo] : std::views::zip(sizeInfos, rows))
		{
			layoutInfo.minimum = sizeInfo.min;
			layoutInfo.preferred = sizeInfo.min;
		}

		return rows;
	}

	struct MeasureResult
	{
		LayoutSizeInfo layoutSizeInfo{};
		std::vector<RowColSize> cols{}; // Assuming vertical repeat
		std::vector<RowColSize> rows{}; // Assuming vertical repeat
	};

	std::vector<RowColSize> measureRows(
		std::span<const TableLayoutConfig::Cell> cells,
		std::span<const TableLayoutConfig::RowColDesc> desc,
		std::span<const size_t> order,
		Layout::ChildList children,
		const int ivec2::* y)
	{
		std::vector<RowColSize> rows = initLayoutInfo(desc);

		for (const size_t cellI : order)
		{
			const int firstColI = cells[cellI].coord.*y;
			const int span = cells[cellI].span.*y;
			const LayoutSizeInfo ctrlSizeInfo = cellI < children.size() ? children[cellI]->getLayoutSizeInfo() : LayoutSizeInfo();
			mergeMinPrefWidth(rows, firstColI, span, ctrlSizeInfo, y);
		}

		return rows;
	}

	MeasureResult layoutPhase1Measure([[maybe_unused]] bool debug, const TableLayoutConfig& config, Layout::ChildList children)
	{
		//if (debug)
		//	std::clog << "XXX" << std::endl;

		const size_t numChildrenPerBlock = config.cells.size();
		const size_t numBlocks = numChildrenPerBlock ? (children.size() + numChildrenPerBlock - 1) / numChildrenPerBlock : 0;

		const bool flipped = config.repeatAxis != EAxis::Vertical;
		int ivec2::* const x = !flipped ? &ivec2::x : &ivec2::y;
		int ivec2::* const y = !flipped ? &ivec2::y : &ivec2::x;

		const auto& colsDesc = !flipped ? config.cols : config.rows;
		const auto& rowsDesc = !flipped ? config.rows : config.cols;

		MeasureResult result{
			.cols = initLayoutInfo(colsDesc),
		};

		result.rows.reserve(rowsDesc.size() * numBlocks);

		/// Measure the cols, by column.

		// Order cells by column.
		std::vector<size_t> order(std::from_range, range(numChildrenPerBlock));

		std::ranges::sort(order, std::less<>(), [x, &config](size_t i) {
			return config.cells[i].coord.*x + config.cells[i].span.*x;
		});

		// Measure the columns. This is different from rows because of the repeat.
		for (const size_t cellI : order)
		{
			const int firstColI = config.cells[cellI].coord.*x;
			const int span = config.cells[cellI].span.*x;
			for (const size_t blockI : range(numBlocks))
			{
				const size_t childI = blockI * numChildrenPerBlock + cellI;
				//if (debug && cellI == 10)
				//	std::clog << "XXX" << std::endl;
				const LayoutSizeInfo ctrlSizeInfo = childI < children.size() ? children[childI]->getLayoutSizeInfo() : LayoutSizeInfo();
				mergeMinPrefWidth(result.cols, firstColI, span, ctrlSizeInfo, x);
			}
		}

		result.layoutSizeInfo.minimum.*x = sum(result.cols, 0, &RowColSize::minimum);
		result.layoutSizeInfo.preferred.*x = sum(result.cols, 0, &RowColSize::preferred);

		/// Measure the rows, by block.

		// Order cells by row.
		std::ranges::sort(order, std::less<>(), [y, &config](size_t i) {
			return config.cells[i].coord.*y + config.cells[i].span.*y;
		});

		for (const size_t blockI : range(numBlocks))
		{
			const std::vector<RowColSize> rows = measureRows(
				config.cells, rowsDesc,
				order,
				children.subspan(blockI * numChildrenPerBlock, std::min(numChildrenPerBlock, children.size() - blockI * numChildrenPerBlock)),
				y
			);

			result.rows.append_range(rows);
			result.layoutSizeInfo.minimum.*y += sum(rows, 0, &RowColSize::minimum);
			result.layoutSizeInfo.preferred.*y += sum(rows, 0, &RowColSize::preferred);
		}

		//if (debug)
		//	_CrtDbgBreak();

		if (flipped)
			std::swap(result.rows, result.cols);

		return result;
	}

	void squash(std::span<const TableLayoutConfig::RowColDesc> descs, std::span<int> widths, int target)
	{
		const int initial = sum(widths);

		if (initial > target && initial > 0 && target >= 0)
		{
			const auto descsRep = std::views::repeat(descs) | std::views::join | std::views::take(widths.size());

			// Squash proportional first, and then autosized.
			// Or, just proportional.
			int initialAutoSized = 0;
			int initialProportional = 0;
			for (const auto& [desc, width] : std::views::zip(descsRep, widths))
				(desc.weight <= 0 ? initialAutoSized : initialProportional) += width;

			// targetAutoSized + targetProportional == target
			const int targetProportional = std::max(0, target - initialAutoSized);
			[[maybe_unused]] const int targetAutoSized = target - targetProportional;

			int remainderProportional = 0;
			[[maybe_unused]] int remainderAutoSized = 0;

			for (auto&& [desc, width] : std::views::zip(descsRep, widths))
			{
				if (desc.weight <= 0)
					;//width = Rational{ .numer = targetAutoSized, .denom = initialAutoSized }.mul(width, std::ref(remainderAutoSized));
				else
					width = Rational{ .numer = targetProportional, .denom = initialProportional }.mul(width, std::ref(remainderProportional));
			}
		}
	}

	std::vector<int> computeSizes(int containerSpace, std::span<const TableLayoutConfig::RowColDesc> descs, std::span<const RowColSize> measurements, size_t numBlocks)
	{
		assert(descs.size() * numBlocks == measurements.size());

		std::vector<int> sizes(measurements.size());

		int autosizeMin = 0;
		int autosizePref = 0;
		int proportionalMin = 0;
		int proportionalPref = 0;

		const auto descRep = std::views::repeat(descs) | std::views::take(numBlocks) | std::views::join;

		int sumWeight = 0;

		for (const auto& [desc, measurement] : std::views::zip(descRep, measurements))
		{
			const bool isAutoSize = desc.weight <= 0;
			(isAutoSize ? autosizeMin : proportionalMin) += measurement.minimum;
			(isAutoSize ? autosizePref : proportionalPref) += measurement.preferred;
			if (!isAutoSize)
				sumWeight += desc.weight;
		}
		
		autosizePref = std::max(autosizePref, autosizeMin);
		proportionalPref = std::max(proportionalPref, proportionalMin);

		// size = lerp(min, pref, autosizeLerpFactor)
		Rational autosizeLerpFactor{};

		const int autosizeMax = std::max(0, containerSpace - proportionalMin);

		if (autosizeMin >= autosizeMax)
		{
			// Not enough space for minimums. Assign minimum sizes, then squash afterwards.
			autosizeLerpFactor = { .numer = 0, .denom = 1 };
		}
		else if (autosizePref <= autosizeMax)
		{
			// Plenty of space. Enough to use preferred sizes for autosize columns.
			autosizeLerpFactor = { .numer = 1, .denom = 1 };
		}
		else // if (autosizeMin <= autosizeMax)
		{
			// Not enough space for preferred sizes, but enough for minimum. Lerp.
			assert(autosizeMax < autosizePref);
			// autosizeMax = min + (pref - min) * autosizeLerpFactor
			// autosizeLerpFactor = (autosizeMax - min) / (pref - min)
			autosizeLerpFactor = { .numer = autosizeMax - autosizeMin, .denom = autosizePref - autosizeMin };
		}

		const int remainingProportionalSpace = containerSpace - std::min(autosizeMax, autosizePref);
		// Check there's enough space left, if there should be.
		assert(autosizeMin >= autosizeMax || remainingProportionalSpace >= proportionalMin);

		int autosizeAssignmentRemainder = 0;
		int proportionalAssignmentRemainder = 0;

		int sumInitialProportional = 0;

		for (auto&& [desc, measurement, size] : std::views::zip(descRep, measurements, sizes))
		{
			if (const bool isAutoSize = desc.weight <= 0; isAutoSize)
				size = autosizeLerpFactor.lerp(measurement.minimum, measurement.preferred, std::ref(autosizeAssignmentRemainder));
			else // Proportional
			{
				size = Rational{ .numer = desc.weight, .denom = sumWeight }.mul(remainingProportionalSpace, std::ref(proportionalAssignmentRemainder));
				sumInitialProportional += size;
			}
		}

		if (sumInitialProportional)
			assert(sumInitialProportional == remainingProportionalSpace);

		
		// TODO: This doesn't take into account span. But does produce a valid layout.
		//       The most simple example is the mini specialist UI in the domestic advisor. The middle row tries to center two buttons.
		//       But, the proportional layout assigns a width smaller than what the measure phase thinks is the minimum (which comes from the name row).
		//       Solving this properly might not be trivial. This stuff isn't exactly well-researched.
		//       Maybe a way is to take the proportional columns, grow them to remove all (cell) violation, and then proportionally reduce slack to fit within container.
		//       The key may be to determine how much slack each column has, as shrinking a column may reduce slack in other columns due to span.
		//       The depednency between columns is the non-trivial part.
		//       If a span crosses over an auto-size column or gap, then the width can be subtracted from minimum cell width as a preprocess.
		//       But solving this properly isn't important in practice as most uses of span can be replaced with nested layouts.

		// Proportional columns: Count up spare width (max(0, width - min)) and violated width (max(0, min - width)).
		int spareWidth = 0;
		int violatedWidth = 0;
		for (const size_t i : range(sizes.size()))
		{
			const bool isAutoSize = descs[i % descs.size()].weight <= 0;
			if (!isAutoSize)
			{
				const int min = measurements[i].minimum;
				const int size = sizes[i];
				spareWidth += std::max(0, size - min);
				violatedWidth += std::max(0, min - size);
			}
		}

		if (violatedWidth && spareWidth)
		{
			// Okay, some width was violated, and there is spare width to share.
			// Solve:
			// sum(lerp(0, colViol[i], t)) == spareWidth, clamp t to [0..1].
			// sum(colViol[i] * t) == spareWidth, clamp t to [0..1].
			// sum(colViol[i]) * t == spareWidth, clamp t to [0..1].
			// t = min(1, spareWidth / sum(colViol[i]))
			// Violated column width = width + colViol[i] * t

			// removedViol = sum(colViol[i]) * t
			// sum(colSpare[i]) * u == removedViol
			// u = removedViol / sum(colSpare[i])

			// Non-violated column width = width - colSpare[i] * u

			const int removedViol = std::min(spareWidth, violatedWidth);
			const Rational t{ removedViol, violatedWidth };
			const Rational u{ removedViol, spareWidth };
			int violRemainder = 0;
			int spareRemainder = 0;

			for (const size_t i : range(sizes.size()))
			{
				const bool isAutoSize = descs[i % descs.size()].weight <= 0;
				if (!isAutoSize)
				{
					const int min = measurements[i].minimum;
					int& size = sizes[i];
					if (min >= size)
						size += t.mul(min - size, violRemainder);
					else
						size -= u.mul(size - min, spareRemainder);
				}
			}

			assert(violRemainder == 0 && spareRemainder == 0);
		}

		// Force proportional columns to minimum widths.
		//for (const size_t i : range(sizes.size()))
		//{
		//	const bool isAutoSize = descs[i % descs.size()].weight <= 0;
		//	const int min = measurements[i].minimum;
		//	int& size = sizes[i];
		//	if (!isAutoSize && size < min)
		//	{
		//		int needed = min - size;
		//
		//		size_t stealColI = i + 1 < sizes.size() ? i + 1 : i - 1;
		//		for ([[maybe_unused]] const size_t stealI : range(size_t(1), sizes.size()))
		//		{
		//			const bool isStealingFromAutoSize = descs[stealColI % descs.size()].weight <= 0;
		//			if (!isStealingFromAutoSize && sizes[stealColI] > measurements[stealColI].minimum)
		//			{
		//				const int n = std::min(needed, sizes[stealColI] - measurements[stealColI].minimum);
		//				needed -= n;
		//				sizes[stealColI] -= n;
		//				//size += n;
		//				if (needed <= 0)
		//					break;
		//			}
		//
		//			if (stealColI > i)
		//			{
		//				const size_t d = (stealColI - i) * 2;
		//				if (stealColI >= d)
		//					stealColI -= d;
		//				else
		//					stealColI += 1;
		//			}
		//			else
		//			{
		//				const size_t d = (i - stealColI) * 2 + 1;
		//				if (sizes.size() - stealColI > d)
		//					stealColI += d;
		//				else
		//					stealColI -= 1;
		//			}
		//		}
		//
		//		size = min - needed;
		//	}
		//}

		// This doesn't look all that great for the "GameHeaderBar" on the main interface.
		// It's useful for screens that are too big though. Per-layout config?
		squash(descs, sizes, containerSpace);

		return sizes;
	}

	void layoutCell(
		std::span<const int> colPositions, std::span<const int> colSizes,
		std::span<const int> rowPositions, std::span<const int> rowSizes,
		const TableLayoutConfig::Cell& cell, Element& ctrl)
	{
		const ivec2 position{ colPositions[cell.coord.x], rowPositions[cell.coord.y] };
		const ivec2 lastPosition{ colPositions[cell.coord.x + cell.span.x - 1], rowPositions[cell.coord.y + cell.span.y - 1] };
		const ivec2 size{ colSizes[cell.coord.x + cell.span.x - 1], rowSizes[cell.coord.y + cell.span.y - 1] };
		const ivec2 end = lastPosition + size;
		const ivec2 space = end - position;
		const LayoutSizeInfo ctrlSizeInfo = ctrl.getLayoutSizeInfo();

		const iaabb2 rect = justilignRect({ .max = space }, vmin(ctrlSizeInfo.preferred, space), cell.align);

		ctrl.setRect(rect + position);
	}
}

TableLayout::TableLayout(TableLayoutConfig config) : config(std::move(config))
{
}

LayoutSizeInfo TableLayout::layoutPhase1Measure(ChildList children) const
{
	const MeasureResult measureResult = ::layoutPhase1Measure(debug, config, children);
	LayoutSizeInfo info = measureResult.layoutSizeInfo;
	const ivec2 totalGap = config.gap * ivec2(int(measureResult.cols.size() - 1), int(measureResult.rows.size() - 1));
	info.minimum += totalGap;
	info.preferred += totalGap;
	return info;
}
void TableLayout::layoutPhase2Apply(ivec2 space, ChildList children) const
{
	//if (debug)
	//	std::clog << "XXX" << std::endl;

	const MeasureResult measureResult = ::layoutPhase1Measure(debug, config, children);

	const size_t numChildrenPerBlock = config.cells.size();
	const size_t numBlocks = numChildrenPerBlock ? (children.size() + numChildrenPerBlock - 1) / numChildrenPerBlock : 0;

	const ivec2 totalGap = config.gap * ivec2(int(measureResult.cols.size() - 1), int(measureResult.rows.size() - 1));

	const std::vector<int> colWidths = computeSizes(space.x - totalGap.x, config.cols, measureResult.cols, config.repeatAxis == EAxis::Vertical ? 1 : numBlocks);
	const std::vector<int> rowHeights = computeSizes(space.y - totalGap.y, config.rows, measureResult.rows, config.repeatAxis != EAxis::Vertical ? 1 : numBlocks);

	std::vector<int> colPositions = colWidths;
	std::vector<int> rowPositions = rowHeights;

	for (int& size : colPositions)
		size += config.gap.x;
	for (int& size : rowPositions)
		size += config.gap.y;
	// Extra entries to store the final manual size.
	colPositions.push_back(0);
	std::exclusive_scan(colPositions.begin(), colPositions.end(), colPositions.begin(), 0);
	rowPositions.push_back(0);
	std::exclusive_scan(rowPositions.begin(), rowPositions.end(), rowPositions.begin(), 0);

	const auto& repeatAxisPositions = config.repeatAxis == EAxis::Vertical ? rowPositions : colPositions;
	const auto& repeatAxisSizes = config.repeatAxis == EAxis::Vertical ? rowHeights : colWidths;

	const size_t stripsPerBlock = (config.repeatAxis == EAxis::Vertical ? config.rows : config.cols).size();

	// Okay. Now we have the final layout. Assign control rects.

	for (const size_t blockI : range(numBlocks))
	{
		const auto blockAxisPositions = std::span(repeatAxisPositions).subspan(blockI * stripsPerBlock, stripsPerBlock + 1);
		const auto blockAxisSizes = std::span(repeatAxisSizes).subspan(blockI * stripsPerBlock, stripsPerBlock);

		for (const size_t cellI : range(numChildrenPerBlock))
		{
			if (const size_t childI = blockI * numChildrenPerBlock + cellI; childI < children.size())
			{
				layoutCell(
					config.repeatAxis == EAxis::Vertical ? colPositions : blockAxisPositions,
					config.repeatAxis == EAxis::Vertical ? colWidths : blockAxisSizes,
					config.repeatAxis == EAxis::Vertical ? blockAxisPositions : rowPositions,
					config.repeatAxis == EAxis::Vertical ? blockAxisSizes : rowHeights,
					config.cells[cellI],
					*children[childI]
				);
			}
		}
	}
}