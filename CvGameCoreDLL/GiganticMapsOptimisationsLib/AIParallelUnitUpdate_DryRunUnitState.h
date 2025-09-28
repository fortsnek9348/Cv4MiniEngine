#pragma once

#include "Util.h"

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
	class GroupUpdateRecord;
	struct DryRunGroupState;
	class IPathFinder;

	struct DryRunUnitState
	{
		const CvUnitAI& liveUnit;
		int m_iMoves = liveUnit.getMoves();
		i16vec2 coord{ ivec2{ liveUnit.getX_INLINE(), liveUnit.getY_INLINE() } };
		//const UnitAITypes unitAIType = liveUnit.AI_getUnitAIType();

		explicit DryRunUnitState(const CvUnitAI& unit) : liveUnit(unit)
		{
		}

		const CvPlot* plot() const
		{
			return GC.getMapINLINE().plot(coord.x, coord.y);
		}

		bool atPlot(const CvPlot* plot) const
		{
			return plot && getPlotCoord(*plot) == coord;
		}

		bool at(int x, int y) const
		{
			return (ivec2)coord == ivec2(x, y);
		}

		TeamTypes getTeam() const
		{
			return liveUnit.getTeam();
		}

		int getExtraFriendlyHeal() const
		{
			return liveUnit.getExtraFriendlyHeal();
		}

		int getExtraNeutralHeal() const
		{
			return liveUnit.getExtraNeutralHeal();
		}
		int getExtraEnemyHeal() const
		{
			return liveUnit.getExtraEnemyHeal();
		}

		bool isEnemy(TeamTypes otherTeam, const CvPlot* plot = nullptr) const
		{
			return liveUnit.isEnemy(otherTeam, plot);
		}

		bool isFortifyable() const
		{
			return liveUnit.isFortifyable();
		}

		bool isHurt() const
		{
			return liveUnit.isHurt();
		}

		/// CvUnit::canSleep, which depends on modified group state.
		bool canSleep(const DryRunGroupState& group) const;
		/// CvUnit::canFortify, which depends on modified group state.
		bool canFortify(const DryRunGroupState& group) const;
		/// bool CvUnit::canHeal(const CvPlot* pPlot) const
		bool canHeal(const DryRunGroupState& group, const CvPlot* pPlot, GroupUpdateRecord& record) const;
		/// bool CvUnit::canSentry(const CvPlot* pPlot) const
		bool canSentry(const DryRunGroupState& group, const CvPlot* pPlot) const;
		/// CvUnit::isWaiting, which depends on modified group state.
		bool isWaiting(const DryRunGroupState& group) const;

		/// bool CvUnit::canDefend(const CvPlot* pPlot) const
		bool canDefend(const CvPlot* pPlot = nullptr) const
		{
			return liveUnit.canDefend(pPlot);
		}

		/// bool CvUnit::canFight() const
		bool canFight() const
		{
			return liveUnit.canFight();
		}

		bool canHold() const
		{
			return true; // Yep. Just return true.
		}

		/// void CvUnit::changeMoves(int iChange)
		void changeMoves(int iChange)
		{
			setMoves(getMoves() + iChange);
		}

		/// int CvUnit::getMoves() const
		int getMoves() const
		{
			return m_iMoves;
		}


		/// void CvUnit::setMoves(int iNewValue)
		void setMoves(int iNewValue)
		{
			if (getMoves() != iNewValue)
			{
				//pPlot = plot();

				m_iMoves = iNewValue;

				FAssert(getMoves() >= 0);

				//if (getTeam() == GC.getGameINLINE().getActiveTeam())
				//{
				//	if (pPlot != nullptr)
				//	{
				//		pPlot->setFlagDirty(true);
				//	}
				//}
				//
				//if (IsSelected())
				//{
				//	gDLL->getFAStarIFace()->ForceReset(&GC.getInterfacePathFinder());
				//
				//	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
				//}
				//
				//if (pPlot == gDLL->getInterfaceIFace()->getSelectionPlot())
				//{
				//	gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
				//}
			}
		}

		/// bool CvUnit::canMove() const
		bool canMove() const
		{
			if (liveUnit.isDead())
			{
				return false;
			}

			if (getMoves() >= liveUnit.maxMoves())
			{
				return false;
			}

			if (liveUnit.getImmobileTimer() > 0)
			{
				return false;
			}

			return true;
		}

		bool isDelayedDeath() const
		{
			return liveUnit.isDelayedDeath();
		}

		bool isNoCapture() const
		{
			return liveUnit.isNoCapture();
		}

		bool canMoveOrAttackInto(GroupUpdateRecord& record, const DryRunGroupState& group, const CvPlot* pPlot, bool bDeclareWar = false) const
		{
			return canMoveInto(record, group, pPlot, false, bDeclareWar) || canMoveInto(record, group, pPlot, true, bDeclareWar);
		}

		bool canMoveInto(GroupUpdateRecord& record, const DryRunGroupState& group, const CvPlot* plot, bool attack = false, bool bDeclareWar = false, bool bIgnoreLoad = false) const;

		bool canLoad(GroupUpdateRecord& record, const CvPlot* plot) const;

		/// void CvUnit::setBlockading(bool bNewValue)
		void setBlockading(GroupUpdateRecord& record, bool bNewValue);

		bool isCargo() const
		{
			return liveUnit.isCargo();
		}

		/// void CvUnit::move(CvPlot* pPlot, bool bShow)
		void move(GroupUpdateRecord& record, const DryRunGroupState& group, const CvPlot* pPlot, bool bShow);

		/// CvUnit::setXY(int iX, int iY, bool bGroup, bool bUpdate, bool bShow, bool bCheckPlotVisible)
		void setXY(GroupUpdateRecord& record, int iX, int iY, bool bGroup = false, bool bUpdate = true, bool bShow = false, bool bCheckPlotVisible = false);

		bool canCoexistWithEnemyUnit(TeamTypes teamI) const
		{
			return liveUnit.canCoexistWithEnemyUnit(teamI);
		}

		// Careful with this... doesn't usually depend on plot unit list, but can for air units.
		bool shouldLoadOnMove(const CvPlot* plot) const
		{
			return liveUnit.shouldLoadOnMove(plot);
		}

		bool hasCargo() const
		{
			return liveUnit.hasCargo();
		}

		PlayerTypes getOwner() const
		{
			return liveUnit.getOwnerINLINE();
		}

		PlayerTypes getOwnerINLINE() const
		{
			return liveUnit.getOwnerINLINE();
		}

		bool canBuild(const CvPlot* pPlot, BuildTypes eBuild, bool bTestVisible = false) const
		{
			return liveUnit.canBuild(pPlot, eBuild, bTestVisible);
		}

		// Returns true if build finished...
		/// bool CvUnit::build(BuildTypes eBuild)
		bool build(GroupUpdateRecord& record, BuildTypes eBuild) const;

		bool AI_update(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder);

		DomainTypes getDomainType() const
		{
			return liveUnit.getDomainType();
		}

		bool canMoveAllTerrain() const
		{
			return liveUnit.canMoveAllTerrain();
		}

		const CvUnit* getTransportUnit() const
		{
			return liveUnit.getTransportUnit();
		}

		bool AI_afterAttack(GroupUpdateRecord& record);
		void AI_animalMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		bool AI_anyAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iOddsThreshold, int iMinStack = 0, bool bFollow = false);
		void AI_attackCityLemmingMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		void AI_barbAttackMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		void AI_barbAttackSeaMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		bool AI_bombardCity(GroupUpdateRecord& record);
		void AI_cityDefenseMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		bool AI_connectPlot(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvPlot* pPlot, int iRange = 0);
		bool AI_guardCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bLeave = false, bool bSearch = false, int iMaxPath = INT_MAX);
		// TODO: This needs priority ordering of bonuses to be efficient.
		bool AI_improveBonus(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iMinValue = 0, const CvPlot** ppBestPlot = nullptr, BuildTypes* peBestBuild = nullptr, int* piBestValue = nullptr);
		bool AI_rangeAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange);
		bool AI_targetCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iFlags = 0);
		void AI_workerMove(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

		// Returns true if a mission was pushed...
		bool AI_guardCityBestDefender(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);
		
		bool AI_guardCityMinDefender(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bSearch = true);
		bool AI_canGroupWithAIType(UnitAITypes eUnitAI) const;
		bool AI_group(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, 
			UnitAITypes eUnitAI, int iMaxGroup = -1, int iMaxOwnUnitAI = -1, int iMinUnitAI = -1, bool bIgnoreFaster = false, bool bIgnoreOwnUnitType = false, bool bStackOfDoom = false, int iMaxPath = INT_MAX, bool bAllowRegrouping = false);
		bool AI_allowGroup(GroupUpdateRecord& record, DryRunGroupState& group, const CvUnit* pUnit, UnitAITypes eUnitAI) const;

		bool canJoinGroup(GroupUpdateRecord& record, const CvPlot* pPlot, CvSelectionGroup* pSelectionGroup) const;

		// We don't attack yet. So it should always be false.
		bool isMadeAttack() const
		{
			return liveUnit.isMadeAttack();
		}

		UnitAITypes AI_getUnitAIType() const
		{
			// This isn't modified, so far...
			return liveUnit.AI_getUnitAIType();
		}

		bool AI_isCityAIType() const
		{
			return liveUnit.AI_isCityAIType();
		}

		bool canBuildRoute() const
		{
			return liveUnit.canBuildRoute();
		}

		/// bool CvUnitAI::AI_retreatToCity(bool bPrimary, bool bAirlift, int iMaxPath)
		bool AI_retreatToCity(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder, bool bPrimary = false, bool bAirlift = false, int iMaxPath = INT_MAX);

		bool AI_plotValid(const CvPlot* plot) const
		{
			struct Evil : CvUnitAI
			{
				static auto get()
				{
					return &Evil::AI_plotValid;
				}
			};

			return (liveUnit.*Evil::get())(plot);
		}

		int movesLeft() const
		{
			return std::max(0, liveUnit.maxMoves() - getMoves());
		}

		/// bool CvUnitAI::AI_load(UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI, int iMinCargo, int iMinCargoSpace, int iMaxCargoSpace, int iMaxCargoOurUnitAI, int iFlags, int iMaxPath)
		bool AI_load(GroupUpdateRecord& record, DryRunGroupState& group, IPathFinder& pathFinder,
			UnitAITypes eUnitAI, MissionAITypes eMissionAI, UnitAITypes eTransportedUnitAI = NO_UNITAI,
			int iMinCargo = -1, int iMinCargoSpace = -1, int iMaxCargoSpace = -1, int iMaxCargoOurUnitAI = -1, int iFlags = 0, int iMaxPath = INT_MAX);

		const CvArea* area() const
		{
			return liveUnit.area();
		}

		bool isBarbarian() const
		{
			return liveUnit.isBarbarian();
		}

		/// bool CvUnit::canLoadUnit(const CvUnit* pUnit, const CvPlot* pPlot) const
		// This is okay to use without taking a dependency. Because load/unload is forbidden during the unit update, available cargo space won't change.
		bool canLoadUnitInto(const CvUnit* transportUnit, const CvPlot* pPlot) const
		{
			return liveUnit.canLoadUnit(transportUnit, pPlot);
		}

		SpecialUnitTypes getSpecialUnitType() const
		{
			return liveUnit.getSpecialUnitType();
		}

		// Check enemy visibility at a plot with another one of our units.
		// Typically used for "move to unit" missions.
		bool isVisibleEnemyUnitAt(const CvUnit* targetUnit, GroupUpdateRecord& record);

		/// int CvUnitAI::AI_calculatePlotWorkersNeeded(CvPlot* pPlot, BuildTypes eBuild)
		// Note that this depends on improvement build progress.
		int AI_calculatePlotWorkersNeeded(const CvPlot* pPlot, BuildTypes eBuild) const
		{
			struct Evil : CvUnitAI
			{
				static auto get()
				{
					return &Evil::AI_calculatePlotWorkersNeeded;
				}
			};
			return (liveUnit.*Evil::get())(pPlot, eBuild);
		}

		/// BuildTypes CvUnitAI::AI_betterPlotBuild(CvPlot* pPlot, BuildTypes eBuild)
		BuildTypes AI_betterPlotBuild(GroupUpdateRecord& record, const CvPlot* pPlot, BuildTypes eBuild) const;

		// Returns true if a mission was pushed...
		bool AI_connectCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

		bool AI_improveCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvCity* pCity);

		bool AI_bestCityBuild(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, const CvCity* pCity, const CvPlot** ppBestPlot, BuildTypes* peBestBuild, const CvPlot* pIgnorePlot, const CvUnit* pUnit);

		int AI_finalOddsThreshold(GroupUpdateRecord& record, const CvPlot* pPlot, int iOddsThreshold) const;

		bool AI_potentialEnemy(TeamTypes eTeam, const CvPlot* pPlot = nullptr) const
		{
			struct Evil : CvUnitAI
			{
				static auto get()
				{
					return &Evil::AI_potentialEnemy;
				}
			};
			return (liveUnit.*Evil::get())(eTeam, pPlot);
		}

		bool canRangeStrike() const;

		int AI_searchRange(int iRange) const;

		bool AI_heal(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iDamagePercent = 0, int iMaxPath = INT_MAX);

		int healTurns(GroupUpdateRecord& record, const CvPlot* pPlot) const;
		int healRate(GroupUpdateRecord& record, const CvPlot* pPlot) const;
		bool isAlwaysHeal() const;

		bool hasMoved() const
		{
			return getMoves() > 0;
		}

		bool AI_moveIntoCity(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange);

		bool AI_patrol(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

		bool AI_pillageRange(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iBonusValueThreshold = 0);
		int AI_pillageValue(GroupUpdateRecord& record, const CvPlot* pPlot, int iBonusValueThreshold) const;

		bool AI_follow(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

		bool AI_followBombard(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

		bool canBombard(const CvPlot* pPlot) const;

		bool AI_cityAttack(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, int iRange, int iOddsThreshold, bool bFollow = false);

		bool canPillage(const CvPlot* pPlot) const
		{
			return liveUnit.canPillage(pPlot);
		}

		bool isBlockading() const
		{
			return liveUnit.isBlockading();
		}

		bool isCombat() const
		{
			return liveUnit.isCombat();
		}

		bool isPotentialEnemy(TeamTypes eTeam, const CvPlot* pPlot) const
		{
			if (nullptr == pPlot)
			{
				pPlot = plot();
			}

			return (::isPotentialEnemy(GET_PLAYER(getCombatOwner(eTeam, pPlot)).getTeam(), eTeam));
		}

		PlayerTypes getCombatOwner(TeamTypes eForTeam, const CvPlot* pPlot) const
		{
			if (eForTeam != NO_TEAM && getTeam() != eForTeam && eForTeam != BARBARIAN_TEAM)
			{
				if (isAlwaysHostile(pPlot))
				{
					return BARBARIAN_PLAYER;
				}
			}

			return getOwnerINLINE();
		}

		bool isAlwaysHostile(const CvPlot* pPlot) const
		{
			if (!liveUnit.getUnitInfo().isAlwaysHostile())
			{
				return false;
			}

			if (nullptr != pPlot && pPlot->isCity(true, getTeam()))
			{
				return false;
			}

			return true;
		}

		int baseMoves() const
		{
			return liveUnit.baseMoves();
		}

		int AI_stackOfDoomExtra() const
		{
			struct Hack : CvUnitAI
			{
				static auto get()
				{
					return &Hack::AI_stackOfDoomExtra;
				}
			};
			return (liveUnit.*Hack::get())();
		}

		bool AI_guardFort(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group, bool bSearch = true);

		int AI_getPlotDefendersNeeded(const CvPlot* pPlot, int iExtra) const;

		bool AI_guardCityAirlift(GroupUpdateRecord& record, DryRunGroupState& group);

		bool AI_safety(GroupUpdateRecord& record, IPathFinder& pathFinder, DryRunGroupState& group);

	private:
		bool hasUnitInvariantHealRate(const CvPlot* pPlot) const;
	};
}