#pragma once

#include "CvDLLUtilityIFaceBase.h"

//#include "CvGlobals.h"	// for gDLL
#include "CvEnums.h"
#include "CvString.h"

//
// abstract interface for CvSymbol functions used by DLL
//

class CvSymbol;
class CvPlot;
class CvRoute;
class CvFeature;
class CvRiver;
class CvDLLSymbolIFaceBase
{
public:
	virtual void init(CvSymbol* , int iID, int iOffset, int iType, CvPlot* pPlot) = 0;
	virtual CvSymbol* createSymbol() = 0;
	virtual void destroy(CvSymbol*&, bool bSafeDelete=true ) = 0;
	virtual void setAlpha(CvSymbol*, float fAlpha) = 0;
	virtual void setScale(CvSymbol*, float fScale) = 0;
	virtual void Hide(CvSymbol*, bool bHide) = 0;
	virtual bool IsHidden(CvSymbol*) = 0;
	virtual void updatePosition(CvSymbol*) = 0;
	virtual int getID(CvSymbol*) = 0;	
	virtual SymbolTypes getSymbol(CvSymbol* pSym) = 0;
	virtual void setTypeYield(CvSymbol *, int iType, int count) =0;
};

class DllExport CvDLLFeatureIFaceBase
{
public:
	virtual CvFeature* createFeature() = 0;
	virtual void init(CvFeature*, int iID, int iOffset, int iType, CvPlot* pPlot) = 0;
	virtual FeatureTypes getFeature(CvFeature* pObj) = 0;
	virtual void setDummyVisibility(CvFeature *feature, const char *dummyTag, bool show) = 0;
	virtual void addDummyModel(CvFeature *feature, const char *dummyTag, const char *modelTag) = 0;
	virtual void setDummyTexture(CvFeature *feature, const char *dummyTag, const char *textureTag) = 0;
	virtual CvString pickDummyTag(CvFeature *feature, int mouseX, int mouseY) = 0;
	virtual void resetModel(CvFeature *feature) = 0;

	// derived methods
	// fortsnek: moving to implementation file
	virtual void destroy(CvFeature*& pObj, bool bSafeDelete = true);
	virtual void Hide(CvFeature* pObj, bool bHide);
	virtual bool IsHidden(CvFeature* pObj);
	virtual void updatePosition(CvFeature* pObj);
};

class DllExport CvDLLRouteIFaceBase
{
public:
	virtual CvRoute* createRoute() = 0;
	virtual void init(CvRoute*, int iID, int iOffset, int iType, CvPlot* pPlot) = 0;
	virtual RouteTypes getRoute(CvRoute* pObj) = 0;

	// derived methods
	// fortsnek: moving to implementation file
	virtual void destroy(CvRoute*& pObj, bool bSafeDelete = true);
	virtual void Hide(CvRoute* pObj, bool bHide);
	virtual bool IsHidden(CvRoute* pObj);
	virtual void updatePosition(CvRoute* pObj);
	virtual int getConnectionMask(CvRoute* pObj) = 0;
	virtual void updateGraphicEra(CvRoute *pObj) = 0;
};

class DllExport CvDLLRiverIFaceBase
{
public:
	virtual CvRiver* createRiver() = 0;
	virtual void init(CvRiver*, int iID, int iOffset, int iType, CvPlot* pPlot) = 0;

	// derived methods
	// fortsnek: moving to implementation file
	virtual void destroy(CvRiver*& pObj, bool bSafeDelete = true);
	virtual void Hide(CvRiver* pObj, bool bHide);
	virtual bool IsHidden(CvRiver* pObj);
	virtual void updatePosition(CvRiver* pObj);
};

