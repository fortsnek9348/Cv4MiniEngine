#pragma once

#include "CvEnums.h"

//
// abstract class containing entity-related functions that the DLL needs
//
class CvEntity;
class CvUnitEntity;
class CvCity;
class CvUnit;
class CvMissionDefinition;
class CvPlot;
class CvRiver;

class CvDLLEntityIFaceBase
{
public:
	virtual void removeEntity(CvEntity*)  = 0;
	virtual void addEntity(CvEntity*, unsigned int uiEntAddFlags)  = 0;
	virtual void setup(CvEntity*)  = 0;
	virtual void setVisible(CvEntity*, bool)  = 0;
	virtual void createCityEntity(CvCity*)  = 0;
	virtual void createUnitEntity(CvUnit*)  = 0;
	virtual void destroyEntity(CvEntity*&, bool bSafeDelete=true)  = 0;
	virtual void updatePosition(CvEntity *gameEntity)  = 0;
	virtual void setupFloodPlains(CvRiver *river) = 0;

	virtual bool IsSelected(const CvEntity*)  const = 0;
	virtual void PlayAnimation(CvEntity*, AnimationTypes eAnim, float fSpeed = 1.0f, bool bQueue = false, int iLayer = 0, float fStartPct = 0.0f, float fEndPct = 1.0f)  = 0;
	virtual void StopAnimation(CvEntity*, AnimationTypes eAnim)  = 0;
	virtual void StopAnimation(CvEntity*) = 0;
	virtual void NotifyEntity(CvUnitEntity*, MissionTypes eMission) = 0;
	virtual void MoveTo(CvUnitEntity*, const CvPlot * pkPlot )  = 0;
	virtual void QueueMove(CvUnitEntity*, const CvPlot * pkPlot )  = 0;
	virtual void ExecuteMove(CvUnitEntity*, float fTimeToExecute, bool bCombat )  = 0;
	virtual void SetPosition(CvUnitEntity* pEntity, const CvPlot * pkPlot )  = 0;
	virtual void AddMission(const CvMissionDefinition* pDefinition) = 0;
	virtual void RemoveUnitFromBattle(CvUnit* pUnit) = 0;
	virtual void showPromotionGlow(CvUnitEntity* pEntity, bool show) = 0;
	virtual void updateEnemyGlow(CvUnitEntity* pEntity) = 0;
	virtual void updatePromotionLayers(CvUnitEntity* pEntity) = 0;;
	virtual void updateGraphicEra(CvUnitEntity* pEntity, EraTypes eOldEra = NO_ERA) = 0;
	virtual void SetSiegeTower(CvUnitEntity *pEntity, bool show) = 0;
	virtual bool GetSiegeTower(CvUnitEntity* pEntity) = 0;
};

