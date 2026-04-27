#pragma once

#include <CvDLLSymbolIFaceBase.h>

class MyCvDLLFeature : public CvDLLFeatureIFaceBase
{
public:
	// Inherited via CvDLLFeatureIFaceBase
	virtual CvFeature* createFeature() override;
	virtual void init(CvFeature*, int iID, int iOffset, int iType, CvPlot* pPlot) override;
	virtual FeatureTypes getFeature(CvFeature* pObj) override;
	virtual void setDummyVisibility(CvFeature* feature, const char* dummyTag, bool show) override;
	virtual void addDummyModel(CvFeature* feature, const char* dummyTag, const char* modelTag) override;
	virtual void setDummyTexture(CvFeature* feature, const char* dummyTag, const char* textureTag) override;
	virtual CvString pickDummyTag(CvFeature* feature, int mouseX, int mouseY) override;
	virtual void resetModel(CvFeature* feature) override;
};

class MyCvDLLSymbol : public CvDLLSymbolIFaceBase
{
public:
	// Inherited via CvDLLSymbolIFaceBase
	virtual void init(CvSymbol*, int iID, int iOffset, int iType, CvPlot* pPlot) override;
	virtual CvSymbol* createSymbol() override;
	virtual void destroy(CvSymbol*&, bool bSafeDelete) override;
	virtual void setAlpha(CvSymbol*, float fAlpha) override;
	virtual void setScale(CvSymbol*, float fScale) override;
	virtual void Hide(CvSymbol*, bool bHide) override;
	virtual bool IsHidden(CvSymbol*) override;
	virtual void updatePosition(CvSymbol*) override;
	virtual int getID(CvSymbol*) override;
	virtual SymbolTypes getSymbol(CvSymbol* pSym) override;
	virtual void setTypeYield(CvSymbol*, int iType, int count) override;
};

class MyCvDLLRoute : public CvDLLRouteIFaceBase
{
public:
	// Inherited via CvDLLRouteIFaceBase
	virtual CvRoute* createRoute() override;
	virtual void init(CvRoute*, int iID, int iOffset, int iType, CvPlot* pPlot) override;
	virtual RouteTypes getRoute(CvRoute* pObj) override;
	virtual int getConnectionMask(CvRoute* pObj) override;
	virtual void updateGraphicEra(CvRoute* pObj) override;
};

class MyCvDLLRiver : public CvDLLRiverIFaceBase
{
public:
	// Inherited via CvDLLRiverIFaceBase
	virtual CvRiver* createRiver() override;
	virtual void init(CvRiver*, int iID, int iOffset, int iType, CvPlot* pPlot) override;
};