#include "LineGraph.h"
#include "Graphics.h"
#include "Common.h"
#include "TuiTextCode.h"

#include <CommonStuff/range.h>

#include <cmath>

using namespace hecktui;
using heck::range;

void LineGraph::clearSeries() noexcept
{
	mSeries.clear();
	mMaxY = 0;
}

static std::span<const int> safeSubspan(std::span<const int> s, size_t start, size_t end)
{
	start = std::min(start, s.size());
	end = std::max(start, std::min(end, s.size()));
	return s.subspan(start, end - start);
}

void LineGraph::addSeries(std::vector<int> values, ColorTypes colour)
{
	if (const auto view = safeSubspan(values, mFirstX, mInclLastX + 1); !view.empty())
		mMaxY = std::max(mMaxY, std::ranges::max(view));
	mSeries.push_back({ std::move(values), cvengine::toTuiColour(colour) });
}

void LineGraph::setSeriesVisible(size_t i, bool b)
{
	mSeries[i].visible = b;
}

void LineGraph::setRange(int first, int inclLast)
{
	mFirstX = first;
	mInclLastX = inclLast;

	mMaxY = 0;
	for (const auto& series : mSeries)
		if (const auto view = safeSubspan(series.values, first, inclLast + 1); !view.empty())
			mMaxY = std::max(mMaxY, std::ranges::max(view));
}

void LineGraph::setXLabels(std::vector<std::wstring> labels)
{
	mLabels = std::move(labels);
}

void LineGraph::setProportionalMode()
{
	mProportionalMode = true;
}

hecktui::LayoutSizeInfo LineGraph::measureThis() const
{
	const ivec2 size{ 3, 4 };
	return { size, size };
}

void LineGraph::drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb)
{
	const iaabb2 rect = offset + getRect();
	
	if (mProportionalMode)
		drawProportionalGraph(rect, fb);
	else
		drawLineGraph(rect, fb);
}

static constexpr auto lerp = [](float a, float b, float t) {
	return a * (1 - t) + b * t;
};

static constexpr auto rescale = [](float x, float src0, float src1, float dst0, float dst1) {
	return lerp(dst0, dst1, (x - src0) / (src1 - src0));
};

void LineGraph::drawLineGraph(iaabb2 rect, hecktui::Framebuffer& fb) const
{
	--rect.max.y;

	const ivec2 renderDim = rect.size() * ivec2(1, 2);

	if (renderDim.x <= 0 || renderDim.y <= 0)
		return;

	fb.drawFilledBox(rect, { .c = L'▀', .colour{ EColour::Black, EColour::Black } });

	const auto plot = [&](int x, int y, EColour colour) {

		const ivec2 fbCoord = rect.min + ivec2(x, y / 2);
		if (fb.getScissorRect().contains(fbCoord))
		{
			hecktui::Pixel& pixel = fb.getPixel(fbCoord);
			(y % 2 == 0 ? pixel.colour.text : pixel.colour.back) = colour;
		}
	};

	const auto drawThinLine = [&](int x0, int y0, int y1, EColour colour) {
		const int ymid = (y0 + y1);

		// This isn't perfect, but avoids 4-connected segments 99% of the time.
		if (y0 < y1)
			for (const int y : range(y0, y1))
				plot(x0 + (y * 2 >= ymid), y, colour);
		else if (y0 > y1)
			for (const int y : range(y1 + 1, y0 + 1))
				plot(x0 + (y * 2 < ymid), y, colour);
		else
			plot(x0, y0, colour);
	};

	const int seriesWidth = mInclLastX - mFirstX;

	const auto toDataX = [&](int renderX) {
		// renderX+1/2: [0..renderDim.x] -> [0..len]
		// Rounds down.
		return mFirstX + (2 * renderX + 1) * seriesWidth / float(2 * renderDim.x);
	};

	for (const auto& series : mSeries)
	{
		if (!series.visible || series.values.empty())
			continue;

		const std::span values = series.values;
		const EColour colour = series.colour;

		const auto interpolateY = [/*firstX = mFirstX,*/ maxY = mMaxY, renderDim, toDataX, values](int renderX) {

			// toDataX(renderX):=rescale(renderX, 0, renderDimX, firstX, inclLastX+1);
			// dataY = rescale(toDataX(renderX+1/2), dataX0, dataX0+1, dataY0, dataY1)
			// renderY = rescale(dataY, 0, dataMaxY, renderDimY, 0)

			const float dataX = toDataX(renderX);
			const int dataX0 = (int)dataX;
			const int dataX1 = dataX0 + 1;

			const int dataY0 = values[dataX0];
			const int dataY1 = values[std::min((size_t)dataX1, values.size() - 1)];

			const float dataY = rescale(dataX, (float)dataX0, (float)dataX1, (float)dataY0, (float)dataY1);
			const float renderY = rescale(dataY, 0, (float)maxY, (float)renderDim.y, 0);

			return int(renderY);
		};

		int prevY = interpolateY(0);

		if (prevY < 0)
			continue;

		for (const int x : range(1, renderDim.x))
		{
			const int nextY = interpolateY(x);

			if (nextY < 0)
				continue;

			drawThinLine(x - 1, prevY, nextY, colour);
			prevY = nextY;
		}

		drawThinLine(renderDim.x - 1, prevY, prevY, colour);
	}

	rect.min.y = rect.max.y;
	++rect.max.y;

	for (const auto& [i, label] : mLabels | std::views::enumerate)
	{
		// Center the label.
		int x = rect.min.x + (2*(int)i*rect.size().x - (int)label.size()*int(mLabels.size() - 1))/(2*int(mLabels.size() - 1));
		x = std::min(x, rect.max.x - (int)label.size());
		x = std::max(x, rect.min.x);
		fb.drawText(label, { .min{ x, rect.min.y }, .max{ rect.max } }, EAlign::Left, EAlign::Top, {}, false);
	}
}
void LineGraph::drawProportionalGraph(iaabb2 rect, hecktui::Framebuffer& fb) const
{
	const ivec2 renderDim = rect.size() * ivec2(1, 2);

	const int seriesWidth = mInclLastX - mFirstX;

	const auto toDataX = [&](int renderX) {
		// renderX+1/2: [0..renderDim.x] -> [0..len]
		// Rounds down.
		return mFirstX + (2 * renderX + 1) * seriesWidth / float(2 * renderDim.x);
	};

	for (const int renderX : range(rect.min.x, rect.max.x))
	{
		const int dataX = std::min<int>(std::lround(toDataX(renderX - rect.min.x)), mInclLastX);

		int total = 0;
		for (const auto& series : mSeries)
			if (series.visible)
				total += series.values[dataX];

		int acc = 0;
		int seriesI = 0;

		std::array<EColour, 2> colours{};

		for (const int renderY : range(renderDim.y))
		{
			// (acc + mSeries[seriesI].values[dataX]) / total * renderDim.y <= renderY + 0.5f
			while (std::cmp_less(seriesI + 1, mSeries.size()) && ((acc + mSeries[seriesI].values[dataX]) * int64_t(renderDim.y) * 2 <= int64_t(renderY * 2 + 1) * total || !mSeries[seriesI].visible))
			{
				if (mSeries[seriesI].visible)
					acc += mSeries[seriesI].values[dataX];
				++seriesI;
			}

			colours[renderY % 2] = mSeries[seriesI].colour;

			if (renderY % 2 == 1)
				fb.draw({ renderX, rect.min.y + renderY / 2 }, { .c = L'▀', .colour{ colours[0], colours[1] } });
		}
	}
}