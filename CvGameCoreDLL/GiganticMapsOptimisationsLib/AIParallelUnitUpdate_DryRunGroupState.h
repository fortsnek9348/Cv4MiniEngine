#pragma once

#include "Util.h"
#include "AIParallelUnitUpdate_DryRunUnitState.h"

#include "../CvPlayerAI.h"
#include "../CvTeamAI.h"
#include "../CvGameCoreUtils.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../CvDLLInterfaceIFaceBase.h"
#include "../CvInfos.h"

#include <CommonStuff/CompilerMacros.h>

#include <array>
#include <bitset>
#include <vector>

namespace GiganticMapsOptimisationsLib
{
	class IPathFinder;

	struct DryRunGroupState
	{
		const CvSelectionGroupAI* liveGroup = nullptr;

		// Get this all zero-initialised.
		ActivityTypes activityType{};
		int missionTimer{};
		MissionAITypes m_eMissionAIType{};
		ivec2 m_iMissionAICoord{};
		IDInfo m_missionAIUnit{};
		bool m_bGroupAttack{};
		bool forceUpdate{};
		AutomateTypes automateType{};
		std::vector<MissionData> m_missionQueue{};

		std::vector<DryRunUnitState> units;
		

		DryRunGroupState() = default;
		explicit DryRunGroupState(const CvSelectionGroupAI& group);

		bool hasHeadMissionQueueNode() const;
		const MissionData* headMissionQueueNode() const;
		TeamTypes getTeam() const;
		PlayerTypes getOwner() const;

		DryRunUnitState* getHeadUnit()
		{
			return units.empty() ? nullptr : &units.front();
		}

		const DryRunUnitState* getHeadUnit() const
		{
			return units.empty() ? nullptr : &units.front();
		}

		MissionTypes getMissionType(size_t index) const
		{
			if (index < m_missionQueue.size())
				return m_missionQueue[index].eMissionType;
			else
				return NO_MISSION;
		}

		const CvPlot* plot() const;

		int getArea() const
		{
			return liveGroup->getArea();
		}

		bool AI_isDeclareWar(const CvPlot* pToPlot) const
		{
			// Depends on head unit, AI_getUnitAIType, cargo
			return liveGroup->AI_isDeclareWar(pToPlot);
		}

		bool canMoveOrAttackInto(GroupUpdateRecord& record, const CvPlot* pPlot, bool bDeclareWar = false) const
		{
			for (const auto& unit : units)
				if (!unit.canMoveOrAttackInto(record, *this, pPlot, bDeclareWar))
					return false;
			return true;
		}

		// Return false if serial fallback should be used.
		[[nodiscard]] bool setBlockading(bool bStart);
		// CvSelectionGroup::setMissionTimer(int iNewValue)
		void setMissionTimer(int iNewValue);
		bool isBusy() const;
		bool isCargoBusy() const;
		/// CvSelectionGroup::setActivityType
		void setActivityType(ActivityTypes eNewValue, GroupUpdateRecord& record);
		/// CvSelectionGroup::clearMissionQueue
		void clearMissionQueue(GroupUpdateRecord& record);

		/// CvSelectionGroupAI::AI_setMissionAI
		void AI_setMissionAI(GroupUpdateRecord& record, MissionAITypes eNewMissionAI, const CvPlot* pNewPlot, const CvUnit* pNewUnit);

		size_t getLengthMissionQueue() const;
		/// CvSelectionGroup::insertAtEndMissionQueue
		// The big one.
		void insertAtEndMissionQueue(MissionData mission, bool bStart, GroupUpdateRecord& record, IPathFinder& pathFinder);
		/// CvSelectionGroup::activateHeadMission
		void activateHeadMission(IPathFinder& pathFinder, GroupUpdateRecord& record);


		// Returns true iff the plot provides a heal rate depending only on city and plot owner. Unit-invariant.
		// This is always true, pretty much. Every tile has a heal rate in vanilla.
		//static bool hasUnitInvariantHealRate(const CvUnit* unit, const CvPlot* pPlot);

		/// bool CvSelectionGroup::isWaiting() const
		bool isWaiting() const;

		/// bool CvSelectionGroup::readyToMove(bool bAny) const
		bool readyToMove(bool bAny = false) const
		{
			return (((bAny) ? canAnyMove() : canAllMove()) && (headMissionQueueNode() == nullptr) && (activityType == ACTIVITY_AWAKE) && !isBusy() && !isCargoBusy());
		}

		/// bool CvSelectionGroup::canStartMission(int iMission, int iData1, int iData2, CvPlot* pPlot, bool bTestVisible, bool bUseCache)
		// bTestVisible = false
		// bUseCache = false
		[[nodiscard]] bool canStartMission(int iMission, int iData1, int iData2, const CvPlot* pPlot, GroupUpdateRecord& record);

		/// void CvSelectionGroup::startMission()
		void startMission(GroupUpdateRecord& record, IPathFinder& pathFinder);
		int getNumUnits() const;
		DomainTypes getDomainType() const;
		/// bool CvSelectionGroup::canAllMove() const
		bool canAllMove() const;
		/// bool CvSelectionGroup::canAnyMove() const
		bool canAnyMove() const;
		UnitAITypes getHeadUnitAI() const;
		MissionAITypes AI_getMissionAIType() const;
		/// CvSelectionGroup::getBestBuildRoute
		RouteTypes getBestBuildRoute(const CvPlot* pPlot, BuildTypes* peBestBuild = nullptr) const;
		bool atPlot(const CvPlot* plot) const;
		bool at(int x, int y) const;
		/// CvUnit* CvSelectionGroupAI::AI_getBestGroupAttacker(const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce, bool bNoBlitz) const
		DryRunUnitState* AI_getBestGroupAttacker(GroupUpdateRecord& record, const CvPlot* pPlot, bool bPotentialEnemy, int& iUnitOdds, bool bForce = false, bool bNoBlitz = false);
		/// bool generatePath(const CvPlot* pFromPlot, const CvPlot* pToPlot, PathingArg pathingArg = 0, bool bReuse = false, int* piPathTurns = nullptr) const;	// Exposed to Python
		bool generatePathFromTo(GroupUpdateRecord& record, IPathFinder& pathFinder, const CvPlot* pFromPlot, const CvPlot* pToPlot, PathingArg pathingArg = 0, bool bReuse = false, int* piPathTurns = nullptr);
		bool generatePath(GroupUpdateRecord& record, IPathFinder& pathFinder, const CvPlot* pToPlot, PathingArg pathingArg = 0, bool bReuse = false, int* piPathTurns = nullptr);
		//const CvPlot* getPathFirstPlot() const;
		int getX() const;
		int getY() const;
		// Returns true if attack was made...
		/// bool CvSelectionGroup::groupAttack(int iX, int iY, int iFlags, bool& bFailedAlreadyFighting)
		bool groupAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags, bool& bFailedAlreadyFighting);
		/// bool CvSelectionGroup::canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const
		bool canEnterArea(TeamTypes eTeam, const CvArea* pArea, bool bIgnoreRightOfPassage) const;
		// Returns true if group was bumped...
		/// bool CvSelectionGroup::groupDeclareWar(CvPlot* pPlot, bool bForce)
		bool groupDeclareWar(GroupUpdateRecord& record, const CvPlot* pPlot, bool bForce = false) const;
		/// bool CvSelectionGroup::isAmphibPlot(const CvPlot* pPlot) const
		bool isAmphibPlot(const CvPlot* pPlot) const;
		// Returns true if attempted an amphib landing...
		/// bool CvSelectionGroup::groupAmphibMove(CvPlot* pPlot, int iFlags)
		bool groupAmphibMove(GroupUpdateRecord& record, const CvPlot* pPlot, int iFlags);
		/// void CvSelectionGroup::groupMove(CvPlot* pPlot, bool bCombat, CvUnit* pCombatUnit, bool bEndMove)
		void groupMove(GroupUpdateRecord& record, IPathFinder& pathFinder, const CvPlot* pPlot, bool bCombat, const CvUnit* pCombatUnit, bool bEndMove = false);
		/// bool CvSelectionGroup::groupPathTo(int iX, int iY, int iFlags)
		bool groupPathTo(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags);
		/// bool CvSelectionGroup::groupRoadTo(int iX, int iY, int iFlags)
		bool groupRoadTo(GroupUpdateRecord& record, IPathFinder& pathFinder, int iX, int iY, int iFlags);
		/// bool CvSelectionGroup::groupBuild(BuildTypes eBuild)
		bool groupBuild(GroupUpdateRecord& record, BuildTypes eBuild);
		/// void CvSelectionGroup::continueMission(int iSteps)
		void continueMission(GroupUpdateRecord& record, IPathFinder& pathFinder, int iSteps);

		void updateMissionTimer(int iSteps);

		/// CvSelectionGroup::pushMission
		// missionDataCoord == iData1, iData2
		// manual = false
		// append = false
		// eMissionAI = NO_MISSIONAI
		// pMissionAIPlot = nullptr
		// pMissionAIUnit = nullptr
		void pushPathingMission(GroupUpdateRecord& record, IPathFinder& pathFinder, MissionTypes missionType, ivec2 missionDataCoord, unsigned int flags = 0,
			bool bAppend = false, bool bManual = false, MissionAITypes eMissionAI = NO_MISSIONAI, const CvPlot* pMissionAIPlot = nullptr, const CvUnit* pMissionAIUnit = nullptr);
		void pushGenericMission(GroupUpdateRecord& record, IPathFinder& pathFinder, MissionTypes missionType, int iData1 = -1, int iData2 = -1, unsigned int flags = 0,
			bool bAppend = false, bool bManual = false, MissionAITypes eMissionAI = NO_MISSIONAI, const CvPlot* pMissionAIPlot = nullptr, const CvUnit* pMissionAIUnit = nullptr);
		void pushSkipMission(GroupUpdateRecord& record, IPathFinder& pathFinder);


		/// inline void CvSelectionGroupAI::AI_cancelGroupAttack()
		void AI_cancelGroupAttack();
		/// inline bool CvSelectionGroupAI::AI_isGroupAttack() const
		bool AI_isGroupAttack() const;
		/// CLLNode<MissionData>* CvSelectionGroup::deleteMissionQueueNode(CLLNode<MissionData>* pNode)
		void deleteHeadMissionQueueNode(GroupUpdateRecord& record, IPathFinder& pathFinder);
		/// void CvSelectionGroup::deactivateHeadMission()
		void deactivateHeadMission(GroupUpdateRecord& record);

		bool isAutomated() const
		{
			return automateType != NO_AUTOMATE;
		}

		AutomateTypes getAutomateType() const
		{
			return automateType;
		}

		/// bool CvSelectionGroup::canFight() const
		bool canFight() const
		{
			return std::ranges::any_of(units, &DryRunUnitState::canFight);
		}

		bool alwaysInvisible() const
		{
			return liveGroup->alwaysInvisible();
		}

		bool canDefend() const
		{
			return liveGroup->canDefend();
		}

		/// int CvSelectionGroupAI::AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const
		int AI_attackOdds(const CvPlot* pPlot, bool bPotentialEnemy) const
		{
			return liveGroup->AI_attackOdds(pPlot, bPotentialEnemy);
		}

		bool isForceUpdate() const
		{
			return forceUpdate;
		}

		void setForceUpdate(bool x)
		{
			forceUpdate = x;
		}

		// Returns true iff AI_ejectBestDefender would return non-null.
		bool has_AI_ejectBestDefender(const CvPlot* pDefendPlot) const;

	private:
		std::vector<MissionData> buildMissionQueue() const;
	};

	
}