#pragma once

#include <HeckTextUI/BasicControls.h>

#include <CvEnums.h>

class LineGraph : public hecktui::Element
{
public:

	void clearSeries() noexcept;
	void addSeries(std::vector<int> values, ColorTypes colour);
	void setSeriesVisible(size_t i, bool);
	void setRange(int first, int inclLast);
	void setXLabels(std::vector<std::wstring> labels);
	void setProportionalMode();

protected:
	virtual hecktui::LayoutSizeInfo measureThis() const override;
	virtual void drawThis(hecktui::ivec2 offset, hecktui::Framebuffer& fb) override;

private:
	struct Series
	{
		std::vector<int> values;
		//ColorTypes colour = NO_COLOR;
		hecktui::EColour colour{};
		bool visible = true;
	};

	std::vector<Series> mSeries;
	std::vector<std::wstring> mLabels;
	int mFirstX = 0;
	int mInclLastX = 0;
	int mMaxY = 0;

	bool mProportionalMode = false;

	void drawLineGraph(hecktui::iaabb2 rect, hecktui::Framebuffer& fb) const;
	void drawProportionalGraph(hecktui::iaabb2 rect, hecktui::Framebuffer& fb) const;
};