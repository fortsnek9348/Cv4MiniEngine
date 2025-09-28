#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_GroupUpdateRecord.h"

#include <CommonStuff/Hashing.h>

#include <syncstream>
#include <iostream>

static constexpr bool kEnableUseSerialFallbackLogging = false;

using namespace GiganticMapsOptimisationsLib;

GroupUpdateRecord::GroupUpdateRecord(PlayerTypes ownerPlayerI, int groupId, unsigned int seed) : mOwnerPlayerI(ownerPlayerI), mGroupId(groupId)
{
	mRNG.disableThreadCheck();
	mRNG.reseed(static_cast<uint32_t>(heck::rxprime64((uint64_t(groupId) << 32) | seed)));
}

void GroupUpdateRecord::setUseSerialFallback(std::string reason)
{
	if (!getSerialFallbackReason())
	{
		std::swap(mSerialFallbackReason, reason);
		if constexpr (kEnableUseSerialFallbackLogging)
			std::osyncstream(std::clog) << "Group " << mGroupId << " useSerialFallback because " << mSerialFallbackReason << std::endl;
	}
}

void GroupUpdateRecord::setIsPlotSetDirtyFlag()
{
	mIsPlotSetDirtyFlag = true;
}

void GroupUpdateRecord::setDirtyPlotListButtonsAndSelectionButtons()
{
	mDirtyPlotListButtonsAndSelectionButtons = true;
}

unsigned short GroupUpdateRecord::getRand(unsigned short n, const char* text)
{
	return mRNG.get(n, text);
}

int GroupUpdateRecord::read_AI_unitTargetMissionAIs(PlayerTypes owner, const CvUnit* targetUnit, std::span<const MissionAITypes> missionAITypes, const CvSelectionGroup* exclusion)
{
	if (std::ranges::contains(missionAITypes, NO_MISSIONAI))
		setUseSerialFallback("AI_plotTargetMissionAIs NO_MISSIONAI");
	if (!getSerialFallbackReason())
	{
		auto it = std::ranges::find(mTargetUnitMissionReads, targetUnit->getIDInfo(), &TargetUnitMissionAIAccess::targetUnitIdInfo);
		if (it == mTargetUnitMissionReads.end())
			it = mTargetUnitMissionReads.emplace(mTargetUnitMissionReads.end(), targetUnit->getIDInfo());
		for (const MissionAITypes type : missionAITypes)
			it->missionAIMask[type] = true;
	}
	return GET_PLAYER(owner).AI_unitTargetMissionAIs(targetUnit, missionAITypes.data(), (int)missionAITypes.size(), exclusion);
}

int GroupUpdateRecord::read_AI_plotTargetMissionAIs(PlayerTypes owner, const CvPlot* pPlot, MissionAITypes eMissionAI, const CvSelectionGroup* pSkipSelectionGroup, int iRange)
{
	// Unsupported.
	if (eMissionAI == NO_MISSIONAI)
		setUseSerialFallback("AI_plotTargetMissionAIs NO_MISSIONAI");
	if (!getSerialFallbackReason())
		markPlotMissionRead(getPlotCoord(*pPlot), static_cast<PlotMiscAccess::EType>((int)PlotMiscAccess::kFirstGroupMissionAIAtThisPlot + (int)eMissionAI));
	return GET_PLAYER(owner).AI_plotTargetMissionAIs(pPlot, eMissionAI, pSkipSelectionGroup, iRange);
}

//void GroupUpdateRecord::onMissionAIChanged(MissionAITypes eMissionAI, ivec2 pMissionAIPlot, IDInfo pMissionAIUnit)
//{
//	if (eMissionAI == NO_MISSIONAI)
//		return;
//
//	if (pMissionAIUnit != IDInfo())
//	{
//		UnitGeneralMissionAccess value{ pMissionAIUnit };
//		value.mask[eMissionAI] = true;
//		if (!std::ranges::contains(unitGeneralMissionWrites, value))
//			unitGeneralMissionWrites.push_back(value);
//	}
//
//	if (pMissionAIPlot != INVALID_PLOT_COORD)
//	{
//		PlotMissionAccess value{ (i16vec2)pMissionAIPlot };
//		value.mask[eMissionAI] = true;
//		if (!std::ranges::contains(plotMissionAccessWrites, value))
//			plotMissionAccessWrites.push_back(value);
//	}
//}

void GroupUpdateRecord::addDependencyOnUnitMovement(PlayerTypes ourUnitsOwner, const CvUnit* unit)
{
	if (unit->getOwner() == ourUnitsOwner)
		addDependencyOnOurUnitMovementAt(unit->plot());
}

void GroupUpdateRecord::addDependencyOnOurUnitMovementAt(const CvPlot* plot)
{
	if (plot)
		markPlotMissionRead(getPlotCoord(*plot), PlotMiscAccess::kOurUnitMovement);
}

void GroupUpdateRecord::addDependencyOnInvisibilitySightAt(const CvPlot* plot)
{
	markPlotMissionRead(getPlotCoord(*plot), PlotMiscAccess::kInvisibilitySight);
}

bool GroupUpdateRecord::isVisibleEnemyUnit(const CvPlot* plot, const CvUnit& ourUnit)
{
	return countVisibleUnitsAtPlot(PUF_isEnemy, ourUnit.getOwnerINLINE(), plot, ourUnit) != 0;
}

int GroupUpdateRecord::getNumVisibleEnemyDefenders(const CvPlot* plot, const CvUnit& ourUnit)
{
	/// int CvPlot::getNumVisibleEnemyDefenders(const CvUnit* pUnit) const
	/// return plotCount(PUF_canDefendEnemy, pUnit->getOwnerINLINE(), pUnit->isAlwaysHostile(this), NO_PLAYER, NO_TEAM, PUF_isVisible, pUnit->getOwnerINLINE());
	 
	return countVisibleUnitsAtPlot(PUF_canDefendEnemy, ourUnit.getOwnerINLINE(), plot, ourUnit);
}

int GroupUpdateRecord::getNumVisiblePotentialEnemyDefenders(const CvPlot* plot, const CvUnit& ourUnit)
{
	/// int CvPlot::getNumVisiblePotentialEnemyDefenders(const CvUnit* pUnit) const
	return countVisibleUnitsAtPlot(PUF_canDefendPotentialEnemy, ourUnit.getOwnerINLINE(), plot, ourUnit);
}

int GroupUpdateRecord::countVisibleUnitsAtPlot(bool (*enemyFunc)(const CvUnit* pUnit, int iData1, int iData2), int iData1, const CvPlot* plot, const CvUnit& ourUnit)
{
	//const PlayerTypes ourPlayer = ourUnit.getOwnerINLINE();
	const TeamTypes ourTeam = ourUnit.getTeam();

	const bool ourUnitIsAlwaysHostile = ourUnit.isAlwaysHostile(plot);

	//bool hasRegularVisibleUnit = false;
	//bool hasAnyVisibleUnit = false;
	//bool hasInvisibleUnit = false;

	int count = 0;

	for (auto* node = plot->headUnitNode(); node; node = plot->nextUnitNode(node))
	{
		// CvPlot::isVisibleEnemyUnit/PUF_isVisible appears to only depend on if any enemy units pass the invisibility sight check. Not actual plot visibility!
		const CvUnit* const unit = ::getUnit(node->m_data);
		if (enemyFunc(unit, iData1, ourUnitIsAlwaysHostile))
		{
			if ([[maybe_unused]] const bool isVisible = !checkEnemyUnitIsInvisible(unit, ourTeam, false))
			{
				//hasAnyVisibleUnit |= isVisible;
				//++count;
				//if (unit->getInvisibleType() == NO_INVISIBLE)
				//	hasRegularVisibleUnit = true;

				++count;
			}
		}
	}

	/// This could be used if you only need count != 0.
	// If the only enemy units we see are because of invisibility sight, or we don't see any units because of a lack of invisibility sight...
	// (if we depend on invisibility sight)
	//if ((hasAnyVisibleUnit && !hasRegularVisibleUnit) || (!hasAnyVisibleUnit && hasInvisibleUnit))
	//{
	//	// The enemy units at the plot can only be seen with invisibility sight. So we need to take a dependency on that
	//	// (if invisibility-seeing units can move during the parallel update).
	//	if (!std::ranges::contains(plotInvisibilitySightDependencies, getPlotCoord(*plot)))
	//		plotInvisibilitySightDependencies.push_back(getPlotCoord(*plot));
	//}
	/// But probably not all that important if invisible units tend to roam alone.


	return count;
}

/// bool CvUnit::isInvisible(TeamTypes eTeam, bool bDebug, bool bCheckCargo) const
bool GroupUpdateRecord::checkEnemyUnitIsInvisible(const CvUnit* pUnit, TeamTypes ourTeam, bool checkCargo)
{
	if (pUnit->getTeam() == ourTeam)
	{
		return false;
	}

	if (pUnit->alwaysInvisible())
	{
		return true;
	}

	if (checkCargo && pUnit->isCargo())
	{
		return true;
	}

	if (pUnit->getInvisibleType() == NO_INVISIBLE)
	{
		return false;
	}

	// Now we depend on invisibility sight.
	markPlotMissionRead(getPlotCoord(*pUnit->plot()), PlotMiscAccess::kInvisibilitySight);

	return !(pUnit->plot()->isInvisibleVisible(ourTeam, pUnit->getInvisibleType()));
}

void GroupUpdateRecord::markPlotMissionRead(i16vec2 coord, PlotMiscAccess::EType access)
{
	const auto it = std::ranges::find(mPlotMissionAccessReads, coord, &PlotMiscAccess::target);
	if (it != mPlotMissionAccessReads.end())
		it->mask[access] = true;
	else
		mPlotMissionAccessReads.emplace_back(coord).mask[access] = true;
}

//void GroupUpdateRecord::markPlotMissionWrite(i16vec2 coord, PlotMiscAccess::EType access)
//{
//	const auto it = std::ranges::find(mPlotMissionAccessWrites, coord, &PlotMiscAccess::target);
//	if (it != mPlotMissionAccessWrites.end())
//		it->mask[access] = true;
//	else
//		mPlotMissionAccessWrites.emplace_back(coord).mask[access] = true;
//}

void GroupUpdateRecord::addDependencyOnCityWorkersHave(const CvCity* city)
{
	markPlotMissionRead(getPlotCoord(*city->plot()), PlotMiscAccess::kCityWorkersHave);
}

void GroupUpdateRecord::addReadDependencyOnGroup(const CvSelectionGroup* group, SelectionGroupAccess::EType type)
{
	// If it's not our group, it's not going to be modified.
	if (group->getOwnerINLINE() != mOwnerPlayerI)
		return;

	const auto it = std::ranges::find(mGroupAccesses, group->getID(), &SelectionGroupAccess::id);
	if (it != mGroupAccesses.end())
		it->mask[(int)type] = true;
	else
		mGroupAccesses.emplace_back(group->getID()).mask[(int)type] = true;
}

bool GroupUpdateRecord::CvCityAI_AI_isDanger(const CvCity& city)
{
	// TODO: This probably depends on invisible unit visibility.
	return city.AI_isDanger();
}

bool GroupUpdateRecord::CvCityAI_AI_isDefended(CvCity& city, int extra)
{
	// This depends on our unit list at this plot (and UnitAI).
	addDependencyOnOurUnitMovementAt(city.plot());
	return city.AI_isDefended(extra);
}

int GroupUpdateRecord::AI_getPlotDanger(PlayerTypes playerI, const CvPlot* plot, int range)
{
	// Movement of our own damaged units if the enemy might attack with siege.
	addDependencyOnOurUnitMovementAt(plot);

	// TODO: This probably depends on invisible unit visibility.
	return GET_PLAYER(playerI).AI_getPlotDanger(plot, range);
}

// PUF_isEnemy
//bool GroupUpdateRecord::isEnemyUnit(TeamTypes ourTeamI, bool ourUnitIsAlwaysHostile, const CvUnit* enemyUnit)
//{
//	if (enemyUnit->canCoexistWithEnemyUnit(ourTeamI))
//		return false;
//
//	const TeamTypes eOtherTeam = GET_PLAYER(enemyUnit->getCombatOwner(ourTeamI, enemyUnit->plot())).getTeam();
//	return ourUnitIsAlwaysHostile ? ourTeamI != eOtherTeam : atWar(ourTeamI, eOtherTeam);
//}

#endif