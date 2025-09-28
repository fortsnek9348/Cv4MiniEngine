#pragma once

#include "CvDLLEntityIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"

class CvPlot;

//
// abstract interface for CvPlotBuilder functions used by DLL
//
class CvPlotBuilder;
class DllExport CvDLLPlotBuilderIFaceBase// : public CvDLLEntityIFaceBase
{
public:
	virtual void init(CvPlotBuilder*, CvPlot*) = 0;
	virtual CvPlotBuilder* create()  = 0;

	// derived methods
	// fortsnek: moved to implementation file
	virtual void destroy(CvPlotBuilder*& pPlotBuilder, bool bSafeDelete = true);
};
