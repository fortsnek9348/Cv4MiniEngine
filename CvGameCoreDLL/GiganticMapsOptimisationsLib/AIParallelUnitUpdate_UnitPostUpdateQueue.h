#pragma once

#include "Util.h"

#include "../CvEnums.h"

#include <atomic>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <bitset>

class CvPlot;
class CvUnit;
class CvCity;
class CvPlayer;

namespace GiganticMapsOptimisationsLib
{
	class UnitPostUpdateQueue
	{
	public:
		explicit UnitPostUpdateQueue(PlayerTypes owner);

		/// All these functions are thread-safe.

		// CvPlot::removeUnit
		void plotRemoveUnit(CvPlot* plot, CvUnit* unit, bool bUpdate);
		//pNewPlot->addUnit(this, bUpdate && !hasCargo());
		void plotAddUnit(CvPlot* plot, CvUnit* unit, bool bUpdate);
		//pOldPlot->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);
		void changeAdjacentSight(CvPlot* plot, int visibilityRange, bool bIncrement, CvUnit* pUnit, bool bUpdatePlotGroups);
		//GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(-1);
		void changeNumOutsideUnits(int delta);
		void changeMilitaryHappinessUnits(CvCity* city, int delta);
		void deferUpdateMinimapColor(CvPlot*);
		void deferDirtyPlotList(CvPlot*);
		void onSiegingUnitMoveFromToWorkingPlot(CvCity* workingCity, const CvPlot* plot);

		//GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
		void deferTeamMeet(TeamTypes otherTeamI);

		void deferEntityQueueMove(const CvUnit* unit, const CvPlot*);
		void deferEntitySetPosition(const CvUnit* unit, const CvPlot*);

		//pOldPlot->updateCenterUnit();
		//pOldPlot->setFlagDirty(true);
		void deferUpdateCenterUnitAndFlagDirty(const CvPlot*);

		//GET_PLAYER(getOwnerINLINE()).updateGroupCycle(this);
		void deferUpdateGroupCycle(CvUnit*);

		//setInfoBarDirty(true);
		void deferSetInfoBarDirty(CvUnit*);

		//gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
		void deferUpdateEnemyGlow(CvUnit*);

		/// This function is not thread-safe. Only called once at the end.
		void doPostUpdateStuff();

	private:
		CvPlayer& mPlayer;
		DynamicBitmap2D mDeferredUpdatePlotSet;
		std::atomic_int mAccChangeNumOutsideUnits = 0;
		std::atomic_uint64_t mDeferredTeamMeets{};
		std::vector<CvUnit*> mDeferredUpdateGroupCycleUnits;

		std::unordered_map<i16vec2, std::bitset<MAX_TEAMS>, VectorHasher> mPendingStolenVisibilityUpdates;

		void setRevealed(CvPlot* plot, TeamTypes eTeam, bool bUpdatePlotGroup);
		void changeVisibilityCount(CvPlot* plot, TeamTypes eTeam, int iChange, InvisibleTypes eSeeInvisible, bool bUpdatePlotGroups);
		void changeStolenVisibilityCount(CvPlot* plot, TeamTypes eTeam, int iChange);
	};
}