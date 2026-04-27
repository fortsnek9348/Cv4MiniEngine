#include "MyCvDLLPlotBuilder.h"
#include "MyCvDLLEntityIFace.h"

using namespace cvengine;

constinit MyCvDLLPlotBuilder MyCvDLLPlotBuilder::gInstance;

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
