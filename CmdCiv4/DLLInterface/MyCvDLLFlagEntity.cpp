#include "MyCvDLLFlagEntity.h"

constinit MyCvDLLFlagEntity MyCvDLLFlagEntity::gInstance;

class ::CvFlagEntity
{
public:
    PlayerTypes playerI = NO_PLAYER;
    CvPlot* plot = nullptr;
};

CvFlagEntity* MyCvDLLFlagEntity::create(PlayerTypes ePlayer)
{
    return new CvFlagEntity{ ePlayer };
}

PlayerTypes MyCvDLLFlagEntity::getPlayer(CvFlagEntity* pkFlag) const
{
    return pkFlag->playerI;
}

CvPlot* MyCvDLLFlagEntity::getPlot(CvFlagEntity* pkFlag) const
{
    return pkFlag->plot;
}

void MyCvDLLFlagEntity::setPlot(CvFlagEntity* pkFlag, CvPlot* pkPlot, [[maybe_unused]] bool bOffset)
{
    pkFlag->plot = pkPlot;
}

void MyCvDLLFlagEntity::updateUnitInfo([[maybe_unused]] CvFlagEntity* pkFlag, [[maybe_unused]] const CvPlot* pkPlot, [[maybe_unused]] bool bOffset)
{
}

void MyCvDLLFlagEntity::updateGraphicEra([[maybe_unused]] CvFlagEntity* pkFlag)
{
}

void MyCvDLLFlagEntity::destroy(CvFlagEntity*& pImp, bool bSafeDelete)
{
    delete pImp;
    if (bSafeDelete)
        pImp = nullptr;
}
