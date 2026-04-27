#include "MyCvDLLEntityIFace.h"
#include "../CvTuiInterface.h"

#include <CvUnit.h>

#include <cstdlib>

constinit MyCvDLLEntityIFace MyCvDLLEntityIFace::gInstance;

class CvUnitEntity : public CvEntity
{
public:
    explicit CvUnitEntity(CvUnit* unit) : unit(unit)
    {
    }

    virtual EType getType() const noexcept override
    {
        return kUnit;
    }

    CvUnit* unit = nullptr;
};

void MyCvDLLEntityIFace::removeEntity(CvEntity*)
{
}

void MyCvDLLEntityIFace::addEntity(CvEntity*, [[maybe_unused]] unsigned int uiEntAddFlags)
{
}

void MyCvDLLEntityIFace::setup(CvEntity*)
{
}

void MyCvDLLEntityIFace::setVisible(CvEntity*, bool)
{
}

void MyCvDLLEntityIFace::createCityEntity(CvCity*)
{
}

void MyCvDLLEntityIFace::createUnitEntity(CvUnit* unit)
{
    // Assuming only one entity will be created per unit.
    if (unit->getEntity())
        std::abort();
    unit->setEntity(new CvUnitEntity(unit));
}

void MyCvDLLEntityIFace::destroyEntity(CvEntity*& entity, bool bSafeDelete)
{
    delete entity;
    if (bSafeDelete)
        entity = nullptr;
}

void MyCvDLLEntityIFace::updatePosition([[maybe_unused]] CvEntity* gameEntity)
{
}

void MyCvDLLEntityIFace::setupFloodPlains([[maybe_unused]] CvRiver* river)
{
}

bool MyCvDLLEntityIFace::IsSelected(const CvEntity* entity) const
{
    // Can be null after activating AI autoplay. Not sure if this is vanilla behaviour.
    if (entity && entity->getType() == CvEntity::kUnit)
    {
        // Civ4 probably maintains a selection state in entities.
        // Here, we'll just query the interface selection list directly.
        return cvengine::CvTuiInterface::getInstance().isUnitSelected(*static_cast<const CvUnitEntity&>(*entity).unit);
    }
    return false;
}

void MyCvDLLEntityIFace::PlayAnimation(CvEntity*, [[maybe_unused]] AnimationTypes eAnim, [[maybe_unused]] float fSpeed, [[maybe_unused]] bool bQueue, [[maybe_unused]] int iLayer, [[maybe_unused]] float fStartPct, [[maybe_unused]] float fEndPct)
{
}

void MyCvDLLEntityIFace::StopAnimation(CvEntity*, [[maybe_unused]] AnimationTypes eAnim)
{
}

void MyCvDLLEntityIFace::StopAnimation(CvEntity*)
{
}

void MyCvDLLEntityIFace::NotifyEntity(CvUnitEntity*, [[maybe_unused]] MissionTypes eMission)
{
}

void MyCvDLLEntityIFace::MoveTo(CvUnitEntity*, [[maybe_unused]] const CvPlot* pkPlot)
{
}

void MyCvDLLEntityIFace::QueueMove(CvUnitEntity*, [[maybe_unused]] const CvPlot* pkPlot)
{
}

void MyCvDLLEntityIFace::ExecuteMove(CvUnitEntity*, [[maybe_unused]] float fTimeToExecute, [[maybe_unused]] bool bCombat)
{
}

void MyCvDLLEntityIFace::SetPosition([[maybe_unused]] CvUnitEntity* pEntity, [[maybe_unused]] const CvPlot* pkPlot)
{
}

void MyCvDLLEntityIFace::AddMission([[maybe_unused]] const CvMissionDefinition* pDefinition)
{
}

void MyCvDLLEntityIFace::RemoveUnitFromBattle([[maybe_unused]] CvUnit* pUnit)
{
}

void MyCvDLLEntityIFace::showPromotionGlow([[maybe_unused]] CvUnitEntity* pEntity, [[maybe_unused]] bool show)
{
}

void MyCvDLLEntityIFace::updateEnemyGlow([[maybe_unused]] CvUnitEntity* pEntity)
{
}

void MyCvDLLEntityIFace::updatePromotionLayers([[maybe_unused]] CvUnitEntity* pEntity)
{
}

void MyCvDLLEntityIFace::updateGraphicEra([[maybe_unused]] CvUnitEntity* pEntity, [[maybe_unused]] EraTypes eOldEra)
{
}

void MyCvDLLEntityIFace::SetSiegeTower([[maybe_unused]] CvUnitEntity* pEntity, [[maybe_unused]] bool show)
{
}

bool MyCvDLLEntityIFace::GetSiegeTower([[maybe_unused]] CvUnitEntity* pEntity)
{
    std::abort();
}
