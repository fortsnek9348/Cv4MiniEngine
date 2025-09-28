#pragma once

#include <CvDLLEntityIFaceBase.h>
#include <CvEnums.h>

class CvEntity
{
public:
	enum EType
	{
		kPlotBuilder,
		kUnit,
	};

	virtual ~CvEntity() = default;
	virtual EType getType() const noexcept = 0;
};

class MyCvDLLEntityIFace : public CvDLLEntityIFaceBase
{
public:
	virtual void removeEntity(CvEntity*) override;
	virtual void addEntity(CvEntity*, unsigned int uiEntAddFlags) override;
	virtual void setup(CvEntity*) override;
	virtual void setVisible(CvEntity*, bool) override;
	virtual void createCityEntity(CvCity*) override;
	virtual void createUnitEntity(CvUnit*) override;
	virtual void destroyEntity(CvEntity*&, bool bSafeDelete=true) override;
	virtual void updatePosition(CvEntity *gameEntity) override;
	virtual void setupFloodPlains(CvRiver *river) override;
	virtual bool IsSelected(const CvEntity*)  const override;
	virtual void PlayAnimation(CvEntity*, AnimationTypes eAnim, float fSpeed = 1.0f, bool bQueue = false, int iLayer = 0, float fStartPct = 0.0f, float fEndPct = 1.0f) override;
	virtual void StopAnimation(CvEntity*, AnimationTypes eAnim) override;
	virtual void StopAnimation(CvEntity * ) override;
	virtual void NotifyEntity(CvUnitEntity*, MissionTypes eMission) override;
	virtual void MoveTo(CvUnitEntity*, const CvPlot * pkPlot ) override;
	virtual void QueueMove(CvUnitEntity*, const CvPlot * pkPlot ) override;
	virtual void ExecuteMove(CvUnitEntity*, float fTimeToExecute, bool bCombat ) override;
	virtual void SetPosition(CvUnitEntity* pEntity, const CvPlot * pkPlot ) override;
	virtual void AddMission(const CvMissionDefinition* pDefinition) override;
	virtual void RemoveUnitFromBattle(CvUnit* pUnit) override;
	virtual void showPromotionGlow(CvUnitEntity* pEntity, bool show) override;
	virtual void updateEnemyGlow(CvUnitEntity* pEntity) override;
	virtual void updatePromotionLayers(CvUnitEntity* pEntity) override;
	virtual void updateGraphicEra(CvUnitEntity* pEntity, EraTypes eOldEra = NO_ERA) override;
	virtual void SetSiegeTower(CvUnitEntity *pEntity, bool show) override;
	virtual bool GetSiegeTower(CvUnitEntity *pEntity) override;
};