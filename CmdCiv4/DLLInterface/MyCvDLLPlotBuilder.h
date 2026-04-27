#pragma once

#include <CvGameCoreDLL/CvDLLPlotBuilderIFaceBase.h>

namespace cvengine
{
	struct MyCvDLLPlotBuilder : CvDLLPlotBuilderIFaceBase
	{
		static MyCvDLLPlotBuilder gInstance;

		virtual void init(CvPlotBuilder*, CvPlot*) override;
		virtual CvPlotBuilder* create() override;
	};
}