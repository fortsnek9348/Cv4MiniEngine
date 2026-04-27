#include "CyGlobeLayerManager.h"

void CyGlobeLayerManager::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyGlobeLayerManager::x)

	pybind11::class_<CyGlobeLayerManager>(m, "CyGlobeLayerManager")
		.def(pybind11::init())
		.R(getCurrentLayer)
		.R(getCurrentLayerID)
		.R(getCurrentLayerName)
		.R(getLayer)
		.R(getLayerID)
		.R(getNumLayers)
		.R(setCurrentLayer)
		;
}

CyGlobeLayer CyGlobeLayerManager::getCurrentLayer()
{
	std::abort();
}

int CyGlobeLayerManager::getCurrentLayerID()
{
	std::abort();
}

std::string CyGlobeLayerManager::getCurrentLayerName()
{
	std::abort();
}

CyGlobeLayer CyGlobeLayerManager::getLayer([[maybe_unused]] int i)
{
	std::abort();
}

int CyGlobeLayerManager::getLayerID([[maybe_unused]] const std::string& layer)
{
	std::abort();
}

int CyGlobeLayerManager::getNumLayers()
{
	return 0;
}

void CyGlobeLayerManager::setCurrentLayer()
{
	std::abort();
}
