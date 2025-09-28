#pragma once

#include <pybind11/pybind11.h>

class CyGlobeLayer
{
public:
};

class CyGlobeLayerManager
{
public:
	static void registerWithPython(const pybind11::module& m);

    CyGlobeLayer getCurrentLayer();
    int getCurrentLayerID();
    std::string getCurrentLayerName();
    CyGlobeLayer getLayer(int i);
    int getLayerID(const std::string& layer);
    int getNumLayers();
    void setCurrentLayer();
};