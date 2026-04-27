#include "MyCvDLLFeature.h"

#include <Cv4CommonEngineLib/EngineSpecificsHeader.h>

CvDLLFeatureIFaceBase& cvengine::engine_specific::getFeatureIFace()
{
	static constinit MyCvDLLFeature s;
	return s;
}

CvDLLSymbolIFaceBase& cvengine::engine_specific::getSymbolIFace()
{
	static constinit MyCvDLLSymbol s;
	return s;
}
CvDLLRouteIFaceBase& cvengine::engine_specific::getRouteIFace()
{
	static constinit MyCvDLLRoute s;
	return s;
}

CvDLLRiverIFaceBase& cvengine::engine_specific::getRiverIFace()
{
	static constinit MyCvDLLRiver s;
	return s;
}


class ::CvFeature
{
public:
	FeatureTypes type = NO_FEATURE;
};

CvFeature* MyCvDLLFeature::createFeature()
{
	return new CvFeature();
}

void MyCvDLLFeature::init(CvFeature* feature, [[maybe_unused]] int iID, [[maybe_unused]] int iOffset, int iType, [[maybe_unused]] CvPlot* pPlot)
{
	feature->type = FeatureTypes(iType);
}

FeatureTypes MyCvDLLFeature::getFeature(CvFeature* pObj)
{
	return pObj->type;
}

void MyCvDLLFeature::setDummyVisibility([[maybe_unused]] CvFeature* feature, [[maybe_unused]] const char* dummyTag, [[maybe_unused]] bool show)
{
	abort();
}

void MyCvDLLFeature::addDummyModel([[maybe_unused]] CvFeature* feature, [[maybe_unused]] const char* dummyTag, [[maybe_unused]] const char* modelTag)
{
	abort();
}

void MyCvDLLFeature::setDummyTexture([[maybe_unused]] CvFeature* feature, [[maybe_unused]] const char* dummyTag, [[maybe_unused]] const char* textureTag)
{
	abort();
}

CvString MyCvDLLFeature::pickDummyTag([[maybe_unused]] CvFeature* feature, [[maybe_unused]] int mouseX, [[maybe_unused]] int mouseY)
{
	abort();
}

void MyCvDLLFeature::resetModel([[maybe_unused]] CvFeature* feature)
{
	abort();
}

///

class ::CvSymbol
{
public:
	SymbolTypes type = NO_SYMBOL;
};

void MyCvDLLSymbol::init(CvSymbol* sym, [[maybe_unused]] int iID, [[maybe_unused]] int iOffset, int iType, [[maybe_unused]] CvPlot* pPlot)
{
	sym->type = SymbolTypes(iType);
}

CvSymbol* MyCvDLLSymbol::createSymbol()
{
	return new CvSymbol();
}

void MyCvDLLSymbol::destroy(CvSymbol*& sym, bool bSafeDelete)
{
	delete sym;
	if (bSafeDelete)
		sym = nullptr;
}

void MyCvDLLSymbol::setAlpha(CvSymbol*, [[maybe_unused]] float fAlpha)
{
}

void MyCvDLLSymbol::setScale(CvSymbol*, [[maybe_unused]] float fScale)
{
}

void MyCvDLLSymbol::Hide(CvSymbol*, [[maybe_unused]] bool bHide)
{
}

bool MyCvDLLSymbol::IsHidden(CvSymbol*)
{
	return false;
}

void MyCvDLLSymbol::updatePosition(CvSymbol*)
{
}

int MyCvDLLSymbol::getID(CvSymbol*)
{
	return 0;
}

SymbolTypes MyCvDLLSymbol::getSymbol(CvSymbol* pSym)
{
	return pSym->type;
}

void MyCvDLLSymbol::setTypeYield(CvSymbol*, [[maybe_unused]] int iType, [[maybe_unused]] int count)
{
}

///

class ::CvRoute
{
public:
	RouteTypes type = NO_ROUTE;
};

CvRoute* MyCvDLLRoute::createRoute()
{
	return new CvRoute();;
}

void MyCvDLLRoute::init(CvRoute* route, [[maybe_unused]] int iID, [[maybe_unused]] int iOffset, int iType, [[maybe_unused]] CvPlot* pPlot)
{
	route->type = RouteTypes(iType);
}

RouteTypes MyCvDLLRoute::getRoute([[maybe_unused]] CvRoute* pObj)
{
	return pObj->type;
}

int MyCvDLLRoute::getConnectionMask([[maybe_unused]] CvRoute* pObj)
{
	abort();
}

void MyCvDLLRoute::updateGraphicEra([[maybe_unused]] CvRoute* pObj)
{
	//abort();
}

//

class ::CvRiver
{
public:
};

CvRiver* MyCvDLLRiver::createRiver()
{
	return new CvRiver();
}

void MyCvDLLRiver::init(CvRiver*, [[maybe_unused]] int iID, [[maybe_unused]] int iOffset, [[maybe_unused]] int iType, [[maybe_unused]] CvPlot* pPlot)
{
}
