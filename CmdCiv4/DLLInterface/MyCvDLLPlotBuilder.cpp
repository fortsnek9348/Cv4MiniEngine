#include "MyCvDLLPlotBuilder.h"
#include "MyCvDLLEntityIFace.h"

#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

using namespace cvengine;

CvDLLPlotBuilderIFaceBase& cvengine::engine_specific::getPlotBuilderIFace()
{
	static constinit MyCvDLLPlotBuilder s;
	return s;
}

class ::CvPlotBuilder : public CvEntity
{
public:
	virtual EType getType() const noexcept override
	{
		return kPlotBuilder;
	}
};

void MyCvDLLPlotBuilder::init(CvPlotBuilder*, CvPlot*)
{
}
CvPlotBuilder* MyCvDLLPlotBuilder::create()
{
	return new CvPlotBuilder();
}
