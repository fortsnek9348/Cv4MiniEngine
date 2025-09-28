#pragma once

#include <pybind11/pybind11.h>

class CyStatistics
{
public:
	static void registerWithPython(const pybind11::module& m);

	// Used by the info screen.
	int getPlayerNumCitiesBuilt(int playerI);
	int getPlayerNumCitiesRazed(int playerI);
	bool getPlayerReligionFounded(int playerI, int religionInfoI);
	int getPlayerNumUnitsBuilt(int playerI, int unitInfoI);
	int getPlayerNumUnitsKilled(int playerI, int unitInfoI);
	int getPlayerNumUnitsLost(int playerI, int unitInfoI);
	int getPlayerNumBuildingsBuilt(int playerI, int buildingInfoI);
};