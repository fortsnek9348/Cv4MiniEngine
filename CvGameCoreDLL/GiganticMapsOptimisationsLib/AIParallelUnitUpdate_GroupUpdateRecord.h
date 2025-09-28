#pragma once

#include "AIParallelUnitUpdate_DryRunGroupState.h"

#include <CommonStuff/CompilerMacros.h>

#include <atomic>
#include <array>
#include <bitset>
#include <vector>

namespace GiganticMapsOptimisationsLib
{
	class IPathFinder
	{
	public:
		virtual bool findPath(ivec2 start, ivec2 goal, unsigned int flags, int maxTurns, const CvSelectionGroup* group, int remainingMP) = 0;
		virtual const CvPlot* getNextStepPlot() const = 0;
		virtual const CvPlot* getPathEndTurnPlot() const = 0;
		virtual int getLastNodeMP() const = 0;
		virtual int getLastNodeTurns() const = 0;
		//virtual PathingMapReadDependency getPathingMapReadDependency(ivec2 goal) const = 0;
		virtual ~IPathFinder() = default;
	};

	// Record of all dependencies and the results of a unit update.
	// Implicit, not recorded: Before/after EPlotMissionAccess changes for our own units. This information is readily available by looking at live game state and dry run result.
	class GroupUpdateRecord
	{
	public:
		GroupUpdateRecord() = default;
		explicit GroupUpdateRecord(PlayerTypes ownerPlayerI, int groupId, unsigned int seed);

		struct PlotMiscAccess
		{
			// For when accessing whatever's at a plot.
			// Mostly for AI_plotTargetMissionAIs.
			enum EType
			{
				// Reads only.
				// Writes are implicitly determined by dependency enumeration.
				kFirstGroupMissionAIAtThisPlot = 0,
				//kGroupActivitiesAtThisPlot = NUM_MISSIONAI_TYPES,

				// Writes are implicitly determined by dependency enumeration.
				kOurUnitMovement = NUM_MISSIONAI_TYPES,

				// Reads only.
				// Writes are implicitly determined by dependency enumeration.
				// Dependencies on plot invisibility state. Both being invisibile and not invisible.
				kInvisibilitySight,

				// Read/Write
				// At the plot of the city.
				kCityWorkersHave,

				//kThisUnitMovement,

				kNum,
			};

			i16vec2 target{};
			std::bitset<kNum> mask{};

			friend bool operator==(PlotMiscAccess, PlotMiscAccess) = default;
		};

		// For when accessing info about a specific CvSelectionGroup.
		// Only our groups.
		struct SelectionGroupAccess
		{
			enum EType
			{
				kMissionType,
				kMissionAI,
				kActivity,
				kMissionXY,
				kNum,
			};

			int id = -1;
			std::bitset<kNum> mask{};

			friend bool operator==(SelectionGroupAccess, SelectionGroupAccess) = default;
		};

		void setUseSerialFallback(std::string reason);

		void setIsPlotSetDirtyFlag();
		void setDirtyPlotListButtonsAndSelectionButtons();

		unsigned short getRand(unsigned short n, const char* text);

		int read_AI_unitTargetMissionAIs(PlayerTypes owner, const CvUnit* targetUnit, std::span<const MissionAITypes> missionAITypes, const CvSelectionGroup* exclusion);
		int read_AI_plotTargetMissionAIs(PlayerTypes owner, const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup = nullptr, int iRange = 0);
		//void onMissionAIChanged(MissionAITypes eMissionAI, ivec2 pMissionAIPlot, IDInfo pMissionAIUnit);
		void addDependencyOnUnitMovement(PlayerTypes ourUnitsOwner, const CvUnit* unit);
		void addDependencyOnOurUnitMovementAt(const CvPlot* plot);
		void addDependencyOnInvisibilitySightAt(const CvPlot* plot);
		bool isVisibleEnemyUnit(const CvPlot* plot, const CvUnit& ourUnit);
		int getNumVisibleEnemyDefenders(const CvPlot* plot, const CvUnit& ourUnit);
		int getNumVisiblePotentialEnemyDefenders(const CvPlot* plot, const CvUnit& ourUnit);

		void addDependencyOnCityWorkersHave(const CvCity* city);

		void addReadDependencyOnGroup(const CvSelectionGroup* group, SelectionGroupAccess::EType type);

		bool CvCityAI_AI_isDanger(const CvCity& city);
		bool CvCityAI_AI_isDefended(CvCity& city, int extra = 0);
		int AI_getPlotDanger(PlayerTypes playerI, const CvPlot* plot, int range = -1);

		void markPlotMissionRead(i16vec2 coord, PlotMiscAccess::EType access);

	public:
		// Access for dependency enumeration.

		const char* getSerialFallbackReason() const
		{
			return mSerialFallbackReason.empty() ? nullptr : mSerialFallbackReason.c_str();
		}
		bool isDirtyPlotListButtonsAndSelectionButtons() const
		{
			return mDirtyPlotListButtonsAndSelectionButtons;
		}
		bool isStartingPlotSetDirtyFlag() const
		{
			return mIsPlotSetDirtyFlag;
		}
			
		struct TargetUnitMissionAIAccess
		{
			IDInfo targetUnitIdInfo{}; // Note that it doesn't have to be our units.
			std::bitset<NUM_MISSIONAI_TYPES> missionAIMask{};

			friend bool operator==(TargetUnitMissionAIAccess, TargetUnitMissionAIAccess) = default;
		};

		std::span<const PlotMiscAccess> getPlotMissionAccessReads() const
		{
			return mPlotMissionAccessReads;
		}
		//std::span<const PlotMiscAccess> getPlotMissionAccessWrites() const;
		std::span<const SelectionGroupAccess> getGroupAccesses() const
		{
			return mGroupAccesses;
		}
		std::span<const TargetUnitMissionAIAccess> getTargetUnitMissionReads() const
		{
			return mTargetUnitMissionReads;
		}

	private:
		PlayerTypes mOwnerPlayerI{};
		// For logging.
		int mGroupId = 0;

		CvRandom mRNG;

		// If non-null, then everything below can be ignored.
		// Includes: Declare war, capture city, kill unit, load/unload, separate/merge groups, settle
		std::string mSerialFallbackReason{};

		//MissionTypes mission = NO_MISSION;
		// If set, then this was the group's original mission at the start plot.
		//std::bitset<NUM_MISSIONAI_TYPES> missionAIConflictAvoidanceInvalidationBits{};
		// Depends only on the units moved.
		//uint32_t moveTeamPlotPropsWriteMask = 0;
		//PathingMapReadDependency pathingMapReadDependency; // End turn plot is thisTurnMoves.back().
		// From CvSelectionGroup::setActivityType
		bool mDirtyPlotListButtonsAndSelectionButtons = false;
		// If true, CvPlot::setFlagDirty on our starting plot.
		bool mIsPlotSetDirtyFlag = false;
		// If true, call pLoopUnit->NotifyEntity(MISSION_IDLE) for each unit.
		// Meh... there are no entities right now.
		//bool entityNotifyIdle = false;


		// MISSION_MOVE_TO_UNIT
		//std::vector<const CvUnit*> otherUnitMovementDependencies;

		// Animal canMoveInto
		// MISSION_MOVE_TO_UNIT plot()->getBestDefender(getOwner())->getGroup() == &liveGroup
		// Invalidated if our units move onto or leave this plot.
		//std::vector<i16vec2> plotUnitMovementDependencies;

		

		std::vector<PlotMiscAccess> mPlotMissionAccessReads;
		//std::vector<PlotMiscAccess> mPlotMissionAccessWrites;

		
		std::vector<SelectionGroupAccess> mGroupAccesses;

		// Reads by AI_unitTargetMissionAIs.
		// Invalidated when the target unit or the target mission AI of one of our units changes.
		std::vector<TargetUnitMissionAIAccess> mTargetUnitMissionReads;

		// Writes are implicitly determined by dependency enumeration.
		//std::vector<UnitGeneralMissionAccess> unitGeneralMissionWrites;

		//void invalidateMissionConflictAvoidance(MissionAITypes mission)
		//{
		//	if (mission != NO_MISSIONAI)
		//		missionConflictAvoidanceInvalidationBits[mission] = true;
		//}

		// PUF_isEnemy
		//static bool isEnemyUnit(TeamTypes ourTeamI, bool ourUnitIsAlwaysHostile, const CvUnit* enemyUnit);

		int countVisibleUnitsAtPlot(bool (*enemyFunc)(const CvUnit* pUnit, int iData1, int iData2), int iData1, const CvPlot* plot, const CvUnit& ourUnit);

		// Takes dependency on invisibility sight only if needed.
		bool checkEnemyUnitIsInvisible(const CvUnit* pUnit, TeamTypes ourTeam, bool checkCargo = true);

		
		//void markPlotMissionWrite(i16vec2 coord, PlotMiscAccess::EType access);
	};

	struct GroupDryRunResult
	{
		void setUseSerialFallback(const char* reason);
		const char* getSerialFallbackReason() const;

		bool returnValue = false;

		/// Set/used by parallel MIS only.
		bool parallelMISSelect = false;
		uint8_t parallelMISRemovalStep{};
		//bool parallelMISUpdateDone = false;
		
		DryRunGroupState dryRunGroupState;
		GroupUpdateRecord groupUpdateRecord;

		//i16aabb2 pathingRect{};
		//uint32_t pathingTeamPlotPropsReadMask = 0;
		//uint32_t movementTeamPlotPropsWriteMask = 0;
	};

	// NOTE: In this function, not a single byte of live game state is written. Everything is local.
	GroupDryRunResult dryrun_CvSelectionGroupAI_AI_update(const CvSelectionGroupAI& group, IPathFinder& pathFinder, uint32_t updateProcessingSeed);
}