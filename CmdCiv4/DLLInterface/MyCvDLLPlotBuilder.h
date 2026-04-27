#pragma once

#include <CvDLLPlotBuilderIFaceBase.h>

namespace cvengine
{
	struct MyCvDLLPlotBuilder : CvDLLPlotBuilderIFaceBase
	{
		virtual void init(CvPlotBuilder*, CvPlot*) override;
		virtual CvPlotBuilder* create() override;
	};
}