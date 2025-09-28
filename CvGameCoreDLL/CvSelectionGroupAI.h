#pragma once

#include "CvSelectionGroup.h"

#include "CommonShared.h"
#include "CvEnums.h"
#include "CvStructs.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
namespace GiganticMapsOptimisationsLib
{
	struct DryRunGroupState;
}
#endif

class CvUnit;

class CvSelectionGroupAI : public CvSelectionGroup
{

public:

	DllExport CvSelectionGroupAI();
	DllExport virtual ~CvSelectionGroupAI();

	void AI_init();
	void AI_uninit();
	void AI_reset();

	void AI_separate();
	void AI_seperateNonAI(UnitAITypes eUnitAI);
	void AI_seperateAI(UnitAITypes eUnitAI);

	bool AI_update();

	int AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const;
	CvUnit* AI_getBestGroupAttacker(const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce = false, bool bNoBlitz = false) const;
	CvUnit* AI_getBestGroupSacrifice(const CvPlot* pPlot, bool bPotentialEnemy, bool bForce = false, bool bNoBlitz = false) const;
	int AI_compareStacks(const CvPlot* pPlot, bool bPotentialEnemy, bool bCheckCanAttack = false, bool bCheckCanMove = false) const;
	int AI_sumStrength(const CvPlot* pAttackedPlot = nullptr, DomainTypes eDomainType = NO_DOMAIN, bool bCheckCanAttack = false, bool bCheckCanMove = false) const;
	void AI_queueGroupAttack(int iX, int iY);
	void AI_cancelGroupAttack();
	// fortsnek: const
	bool AI_isGroupAttack() const;

	// fortsnek: const
	bool AI_isControlled() const;
	// fortsnek: const
	bool AI_isDeclareWar(const CvPlot* pPlot = nullptr) const;

	// fortsnek: const
	CvPlot* AI_getMissionAIPlot() const;

	// fortsnek: const
	bool AI_isForceSeparate() const;
	void AI_makeForceSeparate();

	// fortsnek: const
	MissionAITypes AI_getMissionAIType() const;
	void AI_setMissionAI(MissionAITypes eNewMissionAI, CvPlot* pNewPlot, CvUnit* pNewUnit);
	CvUnit* AI_ejectBestDefender(CvPlot* pTargetPlot);
	// fortsnek: const
	CvUnit* AI_getMissionAIUnit() const;
	
	bool AI_isFull();

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
	void applyParallelUnitUpdateResult(const GiganticMapsOptimisationsLib::DryRunGroupState& result);
#endif

	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	int m_iMissionAIX;
	int m_iMissionAIY;

	bool m_bForceSeparate;

	MissionAITypes m_eMissionAIType;

	IDInfo m_missionAIUnit;

	bool m_bGroupAttack;
	int m_iGroupAttackX;
	int m_iGroupAttackY;
};
