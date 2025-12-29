#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "AIParallelUnitUpdate_UnitPostUpdateQueue.h"
#include "GameContext.h"

#include <CommonStuff/ParallelExt.h>

#include "../CvCityAI.h"
#include "../CvUnitAI.h"
#include "../CvPlayerAI.h"
#include "../CvDLLInterfaceIFaceBase.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../CvGameCoreUtils.h"
#include "../CvTeamAI.h"

#include <mutex>

using namespace GiganticMapsOptimisationsLib;

/// All the messy stuff from CvUnit::setXY goes here.

UnitPostUpdateQueue::UnitPostUpdateQueue(PlayerTypes owner)
	: mPlayer(GET_PLAYER(owner))
	, mDeferredUpdatePlotSet(CivMapGeometry(GC.getMapINLINE()).dim)
{
}

static std::array<std::mutex, 16> gMutexes;

// CvPlot::removeUnit
void UnitPostUpdateQueue::plotRemoveUnit(CvPlot* plot, CvUnit* unit, bool bUpdate)
{
	if (bUpdate)
		deferUpdateCenterUnitAndFlagDirty(plot);

	const std::lock_guard lock(gMutexes[VectorHasher()(getPlotCoord(*plot)) % gMutexes.size()]);
	plot->removeUnit(unit, false);

	// Note that removeUnit and addUnit  maintain a strict order of units, so this should be deterministic.
	
}
//pNewPlot->addUnit(this, bUpdate && !hasCargo());
void UnitPostUpdateQueue::plotAddUnit(CvPlot* plot, CvUnit* unit, bool bUpdate)
{
	if (bUpdate)
		deferUpdateCenterUnitAndFlagDirty(plot);

	const std::lock_guard lock(gMutexes[VectorHasher()(getPlotCoord(*plot)) % gMutexes.size()]);
	plot->addUnit(unit, false);
}

template<class T>
static std::atomic_ref<T> atomicLazyArrayAllocationAndIndex(std::atomic_ref<T*> ptrRef, size_t n, T defValue, size_t i)
{
	// Atomic lazy allocation
	T* localPtr = ptrRef.load(std::memory_order_relaxed);
	if (!localPtr) [[unlikely]]
	{
		T* const newPtr = new T[n];
		std::fill(newPtr, newPtr + n, defValue);
		if (!ptrRef.compare_exchange_strong(localPtr, newPtr, std::memory_order_acq_rel))
			delete[] newPtr;
		else
			localPtr = newPtr;
	}

	return std::atomic_ref(localPtr[i]);
}

template<class T>
static std::atomic_ref<T> atomicLazyArrayAllocationAndIndex(T*& ptrRef, size_t n, T defValue, size_t i)
{
	return atomicLazyArrayAllocationAndIndex(std::atomic_ref<T*>(ptrRef), n, defValue, i);
}

void UnitPostUpdateQueue::setRevealed(CvPlot* plot, TeamTypes eTeam, bool bUpdatePlotGroup)
{
	constexpr bool bNewValue = true;
	constexpr bool bTerrainOnly = false;
	//constexpr TeamTypes eFromTeam = NO_TEAM;

	FAssertMsg(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	FAssertMsg(std::to_underlying(eTeam) < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	struct Evil : CvPlot
	{
		static auto get()
		{
			return &Evil::m_abRevealed;
		}
	};

	const bool oldRevealed = atomicLazyArrayAllocationAndIndex<bool>(plot->*Evil::get(), MAX_TEAMS, false, eTeam).exchange(true, std::memory_order_relaxed);

	CvCity* const pCity = plot->getPlotCity();

	if (oldRevealed != bNewValue)
	{
		if (CvArea* const area = plot->area())
		{
			struct Evil2 : CvArea
			{
				static auto get()
				{
					return &Evil2::m_aiNumRevealedTiles;
				}
			};

			std::atomic_ref((area->*Evil2::get())[eTeam]).fetch_add(1, std::memory_order_relaxed);
		}

		if (bUpdatePlotGroup)
			for (int iI = 0; iI < MAX_PLAYERS; ++iI)
				if (GET_PLAYER((PlayerTypes)iI).getTeam() == eTeam && GET_PLAYER((PlayerTypes)iI).isAlive())
					mDeferredUpdatePlotSet.setThreadSafe(getPlotCoord(*plot), std::memory_order_relaxed);
					//updatePlotGroup((PlayerTypes)iI);

		// We're not active.
		//if (eTeam == GC.getGameINLINE().getActiveTeam())
		//{
		//	updateSymbols();
		//	updateFog();
		//	updateVisibility();
		//
		//	gDLL->getInterfaceIFace()->setDirty(MinimapSection_DIRTY_BIT, true);
		//	gDLL->getInterfaceIFace()->setDirty(GlobeLayer_DIRTY_BIT, true);
		//}

		// No python allowed.
		//if (isRevealed(eTeam, false))
		//{
		//	// ONEVENT - PlotRevealed
		//	CvEventReporter::getInstance().plotRevealed(this, eTeam);
		//}

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
		if (bNewValue)
			gGlobals.enhancedDLLGameContext->onPlotChange(GiganticMapsOptimisationsLib::EGamePlotChangeEvent::RevealedByTeam, *plot, eTeam, eTeam);
#endif
	}

	if constexpr (!bTerrainOnly)
	{
		if constexpr (bNewValue)
		{
			//if (eFromTeam == NO_TEAM)
			{
				//setRevealedOwner(eTeam, plot->getOwnerINLINE());
				//setRevealedImprovementType(eTeam, plot->getImprovementType());
				//setRevealedRouteType(eTeam, plot->getRouteType());

				struct Evil3 : CvPlot
				{
					static auto aiRevealedOwner()
					{
						return &Evil3::m_aiRevealedOwner;
					}

					static auto aeRevealedImprovementType()
					{
						return &Evil3::m_aeRevealedImprovementType;
					}

					static auto aeRevealedRouteType()
					{
						return &Evil3::m_aeRevealedRouteType;
					}
				};

				// Does the AI even use these values?
				atomicLazyArrayAllocationAndIndex<char>(plot->*Evil3::aiRevealedOwner(), MAX_TEAMS, -1, eTeam)
					.store(static_cast<char>(plot->getOwnerINLINE()), std::memory_order_relaxed);
				atomicLazyArrayAllocationAndIndex<short>(plot->*Evil3::aeRevealedImprovementType(), MAX_TEAMS, NO_IMPROVEMENT, eTeam)
					.store(static_cast<short>(plot->getImprovementType()), std::memory_order_relaxed);
				atomicLazyArrayAllocationAndIndex<short>(plot->*Evil3::aeRevealedRouteType(), MAX_TEAMS, NO_ROUTE, eTeam)
					.store(static_cast<short>(plot->getRouteType()), std::memory_order_relaxed);

				if (pCity)
					pCity->setRevealed(eTeam, true);
			}
			//else
			//{
			//	if (getRevealedOwner(eFromTeam, false) == getOwnerINLINE())
			//	{
			//		setRevealedOwner(eTeam, getRevealedOwner(eFromTeam, false));
			//	}
			//
			//	if (getRevealedImprovementType(eFromTeam, false) == getImprovementType())
			//	{
			//		setRevealedImprovementType(eTeam, getRevealedImprovementType(eFromTeam, false));
			//	}
			//
			//	if (getRevealedRouteType(eFromTeam, false) == getRouteType())
			//	{
			//		setRevealedRouteType(eTeam, getRevealedRouteType(eFromTeam, false));
			//	}
			//
			//	if (pCity != nullptr)
			//	{
			//		if (pCity->isRevealed(eFromTeam, false))
			//		{
			//			pCity->setRevealed(eTeam, true);
			//		}
			//	}
			//}
		}
		//else
		//{
		//	setRevealedOwner(eTeam, NO_PLAYER);
		//	setRevealedImprovementType(eTeam, NO_IMPROVEMENT);
		//	setRevealedRouteType(eTeam, NO_ROUTE);
		//
		//	if (pCity != nullptr)
		//	{
		//		pCity->setRevealed(eTeam, false);
		//	}
		//}
	}
}



static void changeInvisibleVisibilityCount(CvPlot* plot, TeamTypes eTeam, InvisibleTypes eSeeInvisible, int iChange)
{
	struct Evil : CvPlot
	{
		static auto get()
		{
			return &Evil::m_apaiInvisibleVisibilityCount;
		}
	};

	// m_apaiInvisibleVisibilityCount is a nested array: short[eTeam][eSeeInvisible]

	const std::atomic_ref<short*> ourTeamCounts = atomicLazyArrayAllocationAndIndex<short*>(std::atomic_ref<short**>(plot->*Evil::get()), MAX_TEAMS, nullptr, eTeam);
	const std::atomic_ref<short> ourCount = atomicLazyArrayAllocationAndIndex<short>(ourTeamCounts, GC.getNumInvisibleInfos(), 0, eSeeInvisible);
	const int oldCount = ourCount.fetch_add(static_cast<short>(iChange), std::memory_order_relaxed);
	const bool bOldInvisibleVisible = oldCount > 0;
	const bool bNewInvisibleVisible = oldCount + iChange > 0;

	if (bOldInvisibleVisible != bNewInvisibleVisible)
	{
		//if (eTeam == GC.getGameINLINE().getActiveTeam())
		//{
		//	updateCenterUnit();
		//}

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS
		gGlobals.enhancedDLLGameContext->onPlotChange(GiganticMapsOptimisationsLib::EGamePlotChangeEvent::InvisibilitySight, *plot, eTeam, eTeam);
#endif
	}
}

void UnitPostUpdateQueue::changeVisibilityCount(CvPlot* plot, TeamTypes eTeam, int iChange, InvisibleTypes eSeeInvisible, bool bUpdatePlotGroups)
{
	FAssertMsg(eTeam >= 0, "eTeam is expected to be non-negative (invalid Index)");
	FAssertMsg(std::to_underlying(eTeam) < MAX_TEAMS, "eTeam is expected to be within maximum bounds (invalid Index)");

	if (iChange == 0)
		return;

	struct Evil1 : CvPlot
	{
		static auto get()
		{
			return &Evil1::m_aiVisibilityCount;
		}
	};

	const std::atomic_ref<short> ourVisibilityCount = atomicLazyArrayAllocationAndIndex<short>(plot->*Evil1::get(), MAX_TEAMS, 0, eTeam);
	const int oldVisibilityCount = ourVisibilityCount.fetch_add(static_cast<short>(iChange), std::memory_order_relaxed);
	const bool bOldVisible = oldVisibilityCount > 0;
	const bool bNewVisible = oldVisibilityCount + iChange > 0;

	if (eSeeInvisible != NO_INVISIBLE)
		changeInvisibleVisibilityCount(plot, eTeam, eSeeInvisible, iChange);

	if (bOldVisible == bNewVisible)
		return;

	if (bNewVisible)
	{
		setRevealed(plot, eTeam, bUpdatePlotGroups);

		const ivec2 coord = getPlotCoord(*plot);
		for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
		{
			if (CvPlot* const pAdjacentPlot = plotDirection(coord.x, coord.y, ((DirectionTypes)iI)))
			{
				//pAdjacentPlot->updateRevealedOwner(eTeam);

				// updateRevealedOwner tests if any adjacent plot is visible and updates the owner.
				// And as we are that adjacent visible plot, this always happens.

				struct Evil2 : CvPlot
				{
					static auto get()
					{
						return &Evil2::m_aiRevealedOwner;
					}
				};

				atomicLazyArrayAllocationAndIndex<char>(pAdjacentPlot->*Evil2::get(), MAX_TEAMS, -1, eTeam)
					.store(static_cast<char>(pAdjacentPlot->getOwnerINLINE()), std::memory_order_relaxed);
			}
		}

		if (const TeamTypes otherTeam = plot->getTeam(); otherTeam != NO_TEAM)
			mDeferredTeamMeets.fetch_or(uint64_t(1) << otherTeam, std::memory_order_relaxed);
	}

	if (CvCity* const pCity = plot->getPlotCity())
	{
		struct Evil : CvCity
		{
			static auto get()
			{
				return &Evil::m_bInfoDirty;
			}
		};

		// setInfoDirty
		std::atomic_ref(pCity->*Evil::get()).store(true, std::memory_order_relaxed);
	}

	for (int iI = 0; iI < MAX_TEAMS; ++iI)
		// Just in case, add another check so that we're definitely not stealing visibility from ourselves, to avoid potential races.
		if (GET_TEAM((TeamTypes)iI).isStolenVisibility(eTeam) && GET_TEAM((TeamTypes)iI).isAlive() && iI != eTeam)
			changeStolenVisibilityCount(plot, (TeamTypes)iI, bNewVisible ? 1 : -1);

	// We're not active.
	//if (eTeam == GC.getGameINLINE().getActiveTeam())
	//{
	//	updateFog();
	//	updateMinimapColor();
	//	updateCenterUnit();
	//}
}

void UnitPostUpdateQueue::changeStolenVisibilityCount(CvPlot* plot, TeamTypes eTeam, int iChange)
{
	struct Evil : CvPlot
	{
		static auto get()
		{
			return &Evil::m_aiStolenVisibilityCount;
		}

		static auto aiVisibilityCount()
		{
			return &Evil::m_aiVisibilityCount;
		}
	};

	const int oldStolenVisibilityCount = atomicLazyArrayAllocationAndIndex<short>(plot->*Evil::get(), MAX_TEAMS, -1, eTeam)
		.fetch_add(static_cast<short>(iChange), std::memory_order_relaxed);
	const int newStolenVisibilityCount = oldStolenVisibilityCount + iChange;

	// Array is already allocated.
	const int visibilityCount = std::atomic_ref(std::atomic_ref(plot->*Evil::aiVisibilityCount()).load(std::memory_order_relaxed)[eTeam]).load(std::memory_order_relaxed);
	const bool bOldVisible = visibilityCount > 0 || oldStolenVisibilityCount > 0;
	const bool bNewVisible = visibilityCount > 0 || newStolenVisibilityCount > 0;

	if (bOldVisible != bNewVisible)
	{
		// Do all of this later. Note that this potentially affects the active player too!
		//if (bNewVisible)
		//{
		//	setRevealed(eTeam, true, false, NO_TEAM, true);
		//}
		//
		//pCity = getPlotCity();
		//
		//if (pCity != nullptr)
		//{
		//	pCity->setInfoDirty(true);
		//}
		//
		//if (eTeam == GC.getGameINLINE().getActiveTeam())
		//{
		//	updateFog();
		//	updateMinimapColor();
		//	updateCenterUnit();
		//}

		// Just take a global lock. This is probably exceedingly rare anyway.
		static std::mutex mutex;
		const std::lock_guard lock(mutex);
		mPendingStolenVisibilityUpdates[getPlotCoord(*plot)][eTeam] = true;
	}
}

//pOldPlot->changeAdjacentSight(getTeam(), visibilityRange(), false, this, true);
void UnitPostUpdateQueue::changeAdjacentSight(CvPlot* plot, int iRange, bool bIncrement, CvUnit* pUnit, bool bUpdatePlotGroups)
{
	// This is the most complex function.

	bool bAerial = pUnit->getDomainType() == DOMAIN_AIR;

	const DirectionTypes eFacingDirection = bAerial ? NO_DIRECTION : pUnit->getFacingDirection(true);

	//fill invisible types
	std::vector<InvisibleTypes> aSeeInvisibleTypes;
	aSeeInvisibleTypes.reserve(pUnit->getNumSeeInvisibleTypes() + 1);

	for (const int i : range(pUnit->getNumSeeInvisibleTypes()))
		aSeeInvisibleTypes.push_back(pUnit->getSeeInvisibleType(i));

	if (aSeeInvisibleTypes.empty())
		aSeeInvisibleTypes.push_back(NO_INVISIBLE);

	//check one extra outer ring
	if (!bAerial)
		iRange++;

	const ivec2 coord = getPlotCoord(*plot);

	const TeamTypes eTeam = mPlayer.getTeam();

	for (int i = 0;i<(int)aSeeInvisibleTypes.size();i++)
	{
		for (int dx = -iRange; dx <= iRange; dx++)
		{
			for (int dy = -iRange; dy <= iRange; dy++)
			{
				//check if in facing direction
				if (bAerial || plot->shouldProcessDisplacementPlot(dx, dy, iRange - 1, eFacingDirection))
				{
					bool outerRing = false;
					if (abs(dx) == iRange || abs(dy) == iRange)
					{
						outerRing = true;
					}

					//check if anything blocking the plot
					if (bAerial || plot->canSeeDisplacementPlot(eTeam, dx, dy, dx, dy, true, outerRing))
					{
						if (CvPlot* const pPlot = plotXY(coord.x, coord.y, dx, dy))
							changeVisibilityCount(pPlot, eTeam, bIncrement ? 1 : -1, aSeeInvisibleTypes[i], bUpdatePlotGroups);
					}
				}

				if (eFacingDirection != NO_DIRECTION)
				{
					if (abs(dx) <= 1 && abs(dy) <= 1) //always reveal adjacent plots when using line of sight
					{
						if (CvPlot* const pPlot = plotXY(coord.x, coord.y, dx, dy))
						{
							changeVisibilityCount(pPlot, eTeam, 1, aSeeInvisibleTypes[i], bUpdatePlotGroups);
							changeVisibilityCount(pPlot, eTeam, -1, aSeeInvisibleTypes[i], bUpdatePlotGroups);
						}
					}
				}
			}
		}
	}
}
//GET_PLAYER(getOwnerINLINE()).changeNumOutsideUnits(-1);
void UnitPostUpdateQueue::changeNumOutsideUnits(int delta)
{
	mAccChangeNumOutsideUnits.fetch_add(delta, std::memory_order_relaxed);
}
void UnitPostUpdateQueue::changeMilitaryHappinessUnits(CvCity* city, int delta)
{
	struct Evil : CvCity
	{
		static auto get()
		{
			return &Evil::m_iMilitaryHappinessUnits;
		}
	};

	std::atomic_ref(city->*Evil::get()).fetch_add(delta, std::memory_order_relaxed);
	onSiegingUnitMoveFromToWorkingPlot(city, nullptr);
}
void UnitPostUpdateQueue::deferUpdateMinimapColor(CvPlot* plot)
{
	mDeferredUpdatePlotSet.setThreadSafe(getPlotCoord(*plot), std::memory_order_relaxed);
}
void UnitPostUpdateQueue::deferDirtyPlotList(CvPlot*)
{
	// Do nothing. Just do this unconditionally at the end.
}
void UnitPostUpdateQueue::onSiegingUnitMoveFromToWorkingPlot(CvCity* workingCity, const CvPlot*)
{
	// if (canSiege(pWorkingCity->getTeam()))
	// {
	// 	pWorkingCity->AI_setAssignWorkDirty(true);
	// }
	// if (canSiege(pWorkingCity->getTeam()))
	// {
	// 	pWorkingCity->verifyWorkingPlot(pWorkingCity->getCityPlotIndex(pLoopPlot));
	// }

	// In either case, the `canSiege` check passed, which means this is an enemy unit of the working city.
	// Invalidate the cite without further checks.
	
	struct Evil : CvCityAI
	{
		static auto get()
		{
			return &Evil::m_bAssignWorkDirty;
		}
	};

	std::atomic_ref(static_cast<CvCityAI&>(*workingCity).*Evil::get()).store(true, std::memory_order_relaxed);
}

//GET_TEAM((TeamTypes)iI).meet(getTeam(), true);
void UnitPostUpdateQueue::deferTeamMeet(TeamTypes otherTeamI)
{
	mDeferredTeamMeets.fetch_or(uint64_t(1) << otherTeamI, std::memory_order_relaxed);
}

void UnitPostUpdateQueue::deferEntityQueueMove(const CvUnit*, const CvPlot*)
{
	// TODO: Not implemented. We don't have entities yet.
}
void UnitPostUpdateQueue::deferEntitySetPosition(const CvUnit*, const CvPlot*)
{
	// TODO: Not implemented. We don't have entities yet.
}

//pOldPlot->updateCenterUnit();
//pOldPlot->setFlagDirty(true);
void UnitPostUpdateQueue::deferUpdateCenterUnitAndFlagDirty(const CvPlot* plot)
{
	mDeferredUpdatePlotSet.setThreadSafe(getPlotCoord(*plot), std::memory_order_relaxed);
}

//GET_PLAYER(getOwnerINLINE()).updateGroupCycle(this);
void UnitPostUpdateQueue::deferUpdateGroupCycle(CvUnit* unit)
{
	if (!mPlayer.isUnitInGroupCycle(unit))
	{
		// Unlikely.
		static std::mutex mutex;
		const std::lock_guard lock(mutex);
		mDeferredUpdateGroupCycleUnits.push_back(unit);
	}
}

//setInfoBarDirty(true);
void UnitPostUpdateQueue::deferSetInfoBarDirty(CvUnit* unit)
{
	struct Evil : CvUnitAI
	{
		static auto get()
		{
			return &Evil::m_bInfoBarDirty;
		}
	};

	std::atomic_ref(unit->*Evil::get()).store(true, std::memory_order_relaxed);
}

//gDLL->getEntityIFace()->updateEnemyGlow(getUnitEntity());
void UnitPostUpdateQueue::deferUpdateEnemyGlow(CvUnit*)
{
	// TODO: Not implemented. We don't have entities yet.
}

/// This function is not thread-safe. Only called once at the end.
void UnitPostUpdateQueue::doPostUpdateStuff()
{
	// DynamicBitmap2D mDeferredUpdatePlotSet;
	const ivec2 dim = mDeferredUpdatePlotSet.dim;
	CvMap& map = GC.getMapINLINE();
	heck::parallelForEachN(dim.y, [&, dim](int y) {
		for (const int x : range(dim.x))
		{
			if (mDeferredUpdatePlotSet[{ x, y }])
			{
				CvPlot* const plot = map.plotINLINE(x, y);
				plot->updateCenterUnit();
				plot->setFlagDirty(true);
				plot->updateMinimapColor();
			}
		}
		});

	// std::atomic_int mAccChangeNumOutsideUnits = 0;
	mPlayer.changeNumOutsideUnits(mAccChangeNumOutsideUnits);

	// std::atomic_uint64_t mDeferredTeamMeets{};
	for (const TeamTypes otherTeamI : range<TeamTypes>(MAX_TEAMS))
		if (((mDeferredTeamMeets.load(std::memory_order_relaxed) >> otherTeamI) & 1) != 0)
			GET_TEAM(otherTeamI).meet(mPlayer.getTeam(), true);

	// std::vector<CvUnit*> mDeferredUpdateGroupCycleUnits;
	// Sort for determinism.
	std::ranges::sort(mDeferredUpdateGroupCycleUnits, std::less<>(), &CvUnit::getID);
	for (CvUnit* const unit : mDeferredUpdateGroupCycleUnits)
		mPlayer.updateGroupCycle(unit);

	// std::unordered_map<i16vec2, std::bitset<MAX_TEAMS>> mPendingStolenVisibilityUpdates;
	using KV = std::pair<i16vec2, std::bitset<MAX_TEAMS>>;
	std::vector<KV> orderedPendingStolenVisibilityUpdates(std::from_range, mPendingStolenVisibilityUpdates);
	// Sort for determinism.
	std::ranges::sort(orderedPendingStolenVisibilityUpdates, std::less<>(), [](const KV& kv) {
		return std::pair(kv.first.x, kv.first.y);
		});

	for (const auto [coord, teams] : orderedPendingStolenVisibilityUpdates)
	{
		CvPlot* const plot = map.plotINLINE(coord.x, coord.y);

		for (const TeamTypes otherTeamI : range<TeamTypes>(MAX_TEAMS))
		{
			// CvPlot::changeStolenVisibilityCount(TeamTypes eTeam, int iChange)
			if (plot->isVisible(otherTeamI, false))
			{
				plot->setRevealed(otherTeamI, true, false, NO_TEAM, true);
			}

			if (otherTeamI == GC.getGameINLINE().getActiveTeam())
			{
				plot->updateFog();
				plot->updateMinimapColor();
				plot->updateCenterUnit();
			}
		}

		if (CvCity* const pCity = plot->getPlotCity())
			pCity->setInfoDirty(true);
	}
	
	gDLL->getInterfaceIFace()->verifyPlotListColumn();
	gDLL->getInterfaceIFace()->setDirty(PlotListButtons_DIRTY_BIT, true);
	// CvUnit::setMoves
	gDLL->getInterfaceIFace()->setDirty(InfoPane_DIRTY_BIT, true);
}

#endif