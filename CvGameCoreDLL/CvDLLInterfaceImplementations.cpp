#include "CvGameCoreDLL.h"

#include "CvDLLSymbolIFaceBase.h"
#include "CvDLLPlotBuilderIFaceBase.h"
#include "CvDLLFlagEntityIFaceBase.h"
#include "CvGlobals.h"

void CvDLLFeatureIFaceBase::destroy(CvFeature*& pObj, bool bSafeDelete) { gDLL->getSymbolIFace()->destroy((CvSymbol*&)pObj, bSafeDelete); }
void CvDLLFeatureIFaceBase::Hide(CvFeature* pObj, bool bHide) { gDLL->getSymbolIFace()->Hide((CvSymbol*)pObj, bHide); }
bool CvDLLFeatureIFaceBase::IsHidden(CvFeature* pObj) { return gDLL->getSymbolIFace()->IsHidden((CvSymbol*)pObj); }
void CvDLLFeatureIFaceBase::updatePosition(CvFeature* pObj) { gDLL->getSymbolIFace()->updatePosition((CvSymbol*)pObj); }

void CvDLLRouteIFaceBase::destroy(CvRoute*& pObj, bool bSafeDelete) { gDLL->getSymbolIFace()->destroy((CvSymbol*&)pObj, bSafeDelete); }
void CvDLLRouteIFaceBase::Hide(CvRoute* pObj, bool bHide) { gDLL->getSymbolIFace()->Hide((CvSymbol*)pObj, bHide); }
bool CvDLLRouteIFaceBase::IsHidden(CvRoute* pObj) { return gDLL->getSymbolIFace()->IsHidden((CvSymbol*)pObj); }
void CvDLLRouteIFaceBase::updatePosition(CvRoute* pObj) { gDLL->getSymbolIFace()->updatePosition((CvSymbol*)pObj); }

void CvDLLRiverIFaceBase::destroy(CvRiver*& pObj, bool bSafeDelete) { gDLL->getRouteIFace()->destroy((CvRoute*&)pObj, bSafeDelete); }
void CvDLLRiverIFaceBase::Hide(CvRiver* pObj, bool bHide) { gDLL->getRouteIFace()->Hide((CvRoute*)pObj, bHide); }
bool CvDLLRiverIFaceBase::IsHidden(CvRiver* pObj) { return gDLL->getRouteIFace()->IsHidden((CvRoute*)pObj); }
void CvDLLRiverIFaceBase::updatePosition(CvRiver* pObj) { gDLL->getRouteIFace()->updatePosition((CvRoute*)pObj); }

void CvDLLPlotBuilderIFaceBase::destroy(CvPlotBuilder*& pPlotBuilder, bool bSafeDelete) {
		gDLL->getEntityIFace()->destroyEntity((CvEntity*&)pPlotBuilder, bSafeDelete);
	}

void CvDLLFlagEntityIFaceBase::setVisible(CvFlagEntity* pEnt, bool bVis) { gDLL->getEntityIFace()->setVisible((CvEntity*)pEnt, bVis); }