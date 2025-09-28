#pragma once

#include <CvDLLFlagEntityIFaceBase.h>

class MyCvDLLFlagEntity : public CvDLLFlagEntityIFaceBase
{
public:


	// Inherited via CvDLLFlagEntityIFaceBase
	virtual CvFlagEntity* create(PlayerTypes ePlayer) override;
	virtual PlayerTypes getPlayer(CvFlagEntity* pkFlag) const override;
	virtual CvPlot* getPlot(CvFlagEntity* pkFlag) const override;
	virtual void setPlot(CvFlagEntity* pkFlag, CvPlot* pkPlot, bool bOffset) override;
	virtual void updateUnitInfo(CvFlagEntity* pkFlag, const CvPlot* pkPlot, bool bOffset) override;
	virtual void updateGraphicEra(CvFlagEntity* pkFlag) override;
	virtual void destroy(CvFlagEntity*& pImp, bool bSafeDelete) override;
};