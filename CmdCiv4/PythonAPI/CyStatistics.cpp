#include "CyStatistics.h"


#include <CvEventReporter.h>
#include <CvStatistics.h>

void CyStatistics::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyStatistics::x)

    pybind11::class_<CyStatistics>(m, "CyStatistics")
        .def(pybind11::init())
        .R(getPlayerNumCitiesBuilt)
        .R(getPlayerNumCitiesRazed)
        .R(getPlayerReligionFounded)
        .R(getPlayerNumUnitsBuilt)
        .R(getPlayerNumUnitsKilled)
        .R(getPlayerNumUnitsLost)
        .R(getPlayerNumBuildingsBuilt)
        ;
}

int CyStatistics::getPlayerNumCitiesBuilt(int playerI)
{
    // I'm not sure if you're supposed to access the member directly, but CvEventReporter has specifically friended us.
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumCitiesBuilt();
}

int CyStatistics::getPlayerNumCitiesRazed(int playerI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumCitiesRazed();
}

bool CyStatistics::getPlayerReligionFounded(int playerI, int religionInfoI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getReligionFounded((ReligionTypes)religionInfoI);
}

int CyStatistics::getPlayerNumUnitsBuilt(int playerI, int unitInfoI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumUnitsBuilt(unitInfoI);
}

int CyStatistics::getPlayerNumUnitsKilled(int playerI, int unitInfoI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumUnitsKilled(unitInfoI);
}

int CyStatistics::getPlayerNumUnitsLost(int playerI, int unitInfoI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumUnitsWasKilled(unitInfoI);
}

int CyStatistics::getPlayerNumBuildingsBuilt(int playerI, int buildingInfoI)
{
    return EVENT_REPORTER.m_kStatistics.getPlayerRecord(playerI)->getNumBuildingsBuilt((BuildingTypes)buildingInfoI);
}
