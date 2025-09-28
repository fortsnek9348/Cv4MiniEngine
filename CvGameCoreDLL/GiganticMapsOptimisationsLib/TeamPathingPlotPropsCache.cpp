#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "TeamPathingPlotPropsCache.h"
#include "UnitPathingUtil.h"
#include "PlotDangerCache.h"

#include <CommonStuff/SimdAStarGraph.h>

#include <CommonStuff/ParallelExt.h>

#include "../CvGameCoreUtils.h"
#include "../CvInfos.h"
#include "../CvUnit.h"
#include "../CvPlayerAI.h"
#include "../CvTeamAI.h"

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

static constexpr bool kEnableLogging = false;

TeamPathingPlotPropsCache::TeamPathingPlotPropsCache(TeamTypes teamI,
	const AllBorderPlotDangerSourceCache* borderDangerCache,
	const UnitPlotDangerCache* unitPlotDangerCache)
	: mTeamI(teamI)
	, mBorderDangerCache(borderDangerCache)
	, mUnitPlotDangerCache(unitPlotDangerCache)
	, mWarBits(calcWarBits())
	, mInvalidationState(CivMapGeometry(GC.getMapINLINE()))
	, mCostPlotPropsArray(CivMapGeometry(GC.getMapINLINE()).dim, 0, heck::ISimdAStarGraph::kCoordVectorLength) // Extra allocation for VectorisedPathStepFunction.
	, mValidityPlotPropsArray(CivMapGeometry(GC.getMapINLINE()).dim, 0, heck::ISimdAStarGraph::kCoordVectorLength) // Extra allocation for VectorisedPathStepFunction.
{
	mLastBorderDangerWarBits = mWarBits;
}

void TeamPathingPlotPropsCache::onWarChange()
{
	mInvalidationState.invalidateAll();
	mWarConditionsInvalidated = true;

	mWarBits = calcWarBits();
}
void TeamPathingPlotPropsCache::onAnyVassalChange()
{
	mInvalidationState.invalidateAll();
	mWarConditionsInvalidated = true;
}
void TeamPathingPlotPropsCache::onOpenBordersChange()
{
	mInvalidationState.invalidateAll();
}
void TeamPathingPlotPropsCache::onPlayerCityDefenceModifierChanged()
{
	mInvalidationState.invalidateAll();
}
void TeamPathingPlotPropsCache::onAreaBorderObstacle()
{
	mInvalidationState.invalidateAll();
}

void TeamPathingPlotPropsCache::onBridgeBuilding()
{
	mInvalidationState.invalidateAll();
}

void TeamPathingPlotPropsCache::onTeamMet()
{
	mWarConditionsInvalidated = true;
}
void TeamPathingPlotPropsCache::onForcePeaceChanged()
{
	mWarConditionsInvalidated = true;
}
void TeamPathingPlotPropsCache::onPermanentWarPeaceChanged()
{
	mWarConditionsInvalidated = true;
}
void TeamPathingPlotPropsCache::onAI_getWarPlan_Changed()
{
	mWarConditionsInvalidated = true;
}


void TeamPathingPlotPropsCache::onPlotChange(const CvPlot& plot, EPlotChange change)
{
	// This may be called from multiple threads due to parallel unit update.
	// Just use a simple mutex for now.
	static std::mutex mutex;
	const std::lock_guard lock(mutex);

	mInvalidationState.invalidateSingle(getPlotCoord(plot));
	if (change == EPlotChange::RouteType)// && GET_TEAM(mTeamI).isBridgeBuilding())
	{
		// Need to update adjacent plots too because of river crossing.
		for (const DirectionTypes adjI : range(NUM_DIRECTION_TYPES))
			if (const CvPlot* const adjPlot = plotDirection(plot.getX_INLINE(), plot.getY_INLINE(), adjI))
				mInvalidationState.invalidateSingle(getPlotCoord(*adjPlot));
	}
}

void TeamPathingPlotPropsCache::update()
{
	if (mWarConditionsInvalidated)
	{
		if constexpr (kEnableLogging)
			std::clog << __func__ << "[" << (int)mTeamI << "]: mWarConditionsInvalidated" << std::endl;

		for (const auto i : range<TeamTypes>(MAX_TEAMS))
		{
			if (const bool x = GET_TEAM(mTeamI).canDeclareWar(i); x != mCanDeclareWar[i])
			{
				mCanDeclareWar[i] = x;
				mInvalidationState.invalidateAll();
			}

			if (const bool x = GET_TEAM(mTeamI).AI_isSneakAttackReady(i); x != mAIIsSneakAttackReady[i])
			{
				mAIIsSneakAttackReady[i] = x;
				mInvalidationState.invalidateAll();
			}

			if (const bool x = GET_TEAM(mTeamI).AI_getWarPlan(i) == WARPLAN_LIMITED; x != mIsWarPlanLimited[i])
			{
				mIsWarPlanLimited[i] = x;
				mInvalidationState.invalidateAll();
			}
		}
	}

	const std::bitset<MAX_TEAMS> warBits = mWarBits;

	CvMap& map = GC.getMapINLINE();

	const ivec2 mapDim = mCostPlotPropsArray.dim;

	if (mInvalidationState.isAllDirty())
	{
		if constexpr (kEnableLogging)
			std::clog << __func__ << "[" << (int)mTeamI << "]: totallyInvalidated (our border version = " << mBorderPlotDangerVersion <<
				", live border version = " << mBorderDangerCache->getBlockVersioning().getVersionSerial() << ')' << std::endl;

		heck::parallelForEachN(mapDim.y, [&, mapDim, this](int y) {
			for (const int x : range(mapDim.x))
			{
				const CvPlot& plot = *map.plotINLINE(x, y);
				mCostPlotPropsArray[{ x, y }] = 0;
				mValidityPlotPropsArray[{ x, y }] = 0;
				updatePlot(plot);
				
				if (mUnitPlotDangerCache->getDefenderPlotDanger(mTeamI, { x, y }))
					mValidityPlotPropsArray[{ x, y }] |= getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger);

				if (((*mBorderDangerCache)[{ x, y }] & warBits).any())
					mValidityPlotPropsArray[{ x, y }] |= getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);

				//if (mTeamI == BARBARIAN_TEAM && ivec2(x, y) == ivec2(724, 81))
				//{
				//	std::clog << "PlotDangerCheck AAA " << danger << ' ' << (*borderSources[0])[{ x, y }] << std::endl;
				//}
			}
			});

		mLastBorderDangerWarBits = warBits;
	}
	else
	{
		if constexpr (kEnableLogging)
		{
			if (mInvalidationState.hasAnyDirty())
				std::clog << __func__ << "[" << (int)mTeamI << "]: invalidationPlotList" << std::endl;
			if (mBorderPlotDangerVersion != mBorderDangerCache->getBlockVersioning().getVersionSerial())
				std::clog << __func__ << "[" << (int)mTeamI << "]: Border danger" << std::endl;
			if (mUnitPlotDangerVersion != mUnitPlotDangerCache->getVersionSerial(mTeamI))
				std::clog << __func__ << "[" << (int)mTeamI << "]: Unit danger" << std::endl;
		}
		//for (const auto enemy : range<TeamTypes>(MAX_TEAMS))
		//{
		//	if (atWar(mTeamI, enemy))
		//	{
				//if (mTeamI == TeamTypes(0))
				//	__debugbreak();
				//const VersionedBitmap2D& bmp = borderDangers[enemy].getVersionedBitmap();

				//if (mTeamI == BARBARIAN_TEAM && int(enemy) == 0)
				//{
				//	std::clog << "PlotDangerCheck BBB " << bmp[{ 724, 81 }] << std::endl;
				//	std::clog << "PlotDangerCheck " << (kClear ? "CLEAR" : "SET") << std::endl;
				//	std::clog << "Our version " << mBorderPlotDangerVersions[enemy] << std::endl;
				//	std::clog << "Border version " << bmp.getVersionSerial() << std::endl;
				//	const auto [l1, l2] = bmp.getBlockVersionsAt({ 724, 81 });
				//	std::clog << "Block versions " << l1 << ' ' << l2 << std::endl;
				//}

		//for (const auto [coord, radius] : mInvalidationState.localInvalidations)
		for (const auto coord : mInvalidationState.getDirtyPlotList())
			//for (const int dy : range(-radius, radius + 1))
				//for (const int dx : range(-radius, radius + 1))
					if (const CvPlot* const plot = map.plotINLINE(coord.x, coord.y))
						updatePlot(*plot);
		
		

		//mLastWarBitsForBorderDanger = warBits;
		//if (mLastWarBitsForBorderDanger != warBits)
		//{
		//	// War bits changed. Need to update all the border danger.
		//	mLastWarBitsForBorderDanger = warBits;
		//	for (const int y : range(mapDim.y))
		//		for (const int x : range(mapDim.x))
		//		{
		//			mValidityPlotPropsArray[{ x, y }] &= ~getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);
		//			if (((*mBorderDangerCache)[{ x, y }] & warBits).any())
		//				mValidityPlotPropsArray[{ x, y }] |= getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);
		//		}
		//}
		//else
		{
			//for (const iaabb2 rect : bmp.getUserDirtyRects(mBorderPlotDangerVersions[enemy]))
			for (const iaabb2 rect : mBorderDangerCache->getBlockVersioning().getUserDirtyRects(mBorderPlotDangerVersion))
			{
				//if (mTeamI == BARBARIAN_TEAM && int(enemy) == 0)
					//std::clog << "Visiting border rect " << rect << std::endl;
				for (const int y : range(rect.min.y, rect.max.y))
					for (const int x : range(rect.min.x, rect.max.x))
					{

						//if constexpr (kClear)
						//	mArray[{ x, y }] &= ~kPlotPropsMask_PathValid_PlotDanger;
						//else if (bmp[{ x, y }])
						//	mArray[{ x, y }] |= kPlotPropsMask_PathValid_PlotDanger;
						mValidityPlotPropsArray[{ x, y }] &= ~getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);
						if (((*mBorderDangerCache)[{ x, y }] & warBits).any())
							mValidityPlotPropsArray[{ x, y }] |= getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);
					}
			}
			//	}
			//}
		}

		// If you get border plot danger verification problems, enable this to check that props are in sync with mBorderDangerCache and mWarBits.
		//assert(mLastBorderDangerWarBits == warBits);
		//for (const int y : range(mapDim.y))
		//	for (const int x : range(mapDim.x))
		//	{
		//		const bool cachedProp = (mValidityPlotPropsArray[{ x, y }] & getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger)) != 0;
		//		const bool cachedDanger = ((*mBorderDangerCache)[{ x, y }] & warBits).any();
		//		if (cachedProp != cachedDanger)
		//			std::abort();
		//	}

		for (const iaabb2 rect : mUnitPlotDangerCache->getDefenderDirtyRects(mTeamI, mUnitPlotDangerVersion))
		{
			//if (mTeamI == BARBARIAN_TEAM)
			//	std::clog << "Visiting unit rect " << rect << std::endl;
			for (const int y : range(rect.min.y, rect.max.y))
				for (const int x : range(rect.min.x, rect.max.x))
				{
					//if constexpr (kClear)
					//	mArray[{ x, y }] &= ~kPlotPropsMask_PathValid_PlotDanger;
					//else if (unitPlotDanger.getDefenderPlotDanger(mTeamI, { x, y }))
					//	mArray[{ x, y }] |= kPlotPropsMask_PathValid_PlotDanger;
					mValidityPlotPropsArray[{ x, y }] &= ~getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger);
					if (mUnitPlotDangerCache->getDefenderPlotDanger(mTeamI, { x, y }))
						mValidityPlotPropsArray[{ x, y }] |= getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger);
				}
		}
	}

	mWarConditionsInvalidated = false;
	mInvalidationState.reset();
	//for (const auto enemy : range<TeamTypes>(MAX_TEAMS))
	//	if (atWar(mTeamI, enemy))
	//		mBorderPlotDangerVersions[enemy] = borderDangers[enemy].getVersionedBitmap().getVersionSerial();
	mBorderPlotDangerVersion = mBorderDangerCache->getBlockVersioning().getVersionSerial();
	mUnitPlotDangerVersion = mUnitPlotDangerCache->getVersionSerial(mTeamI);
}

std::bitset<MAX_TEAMS> TeamPathingPlotPropsCache::calcWarBits() const
{
	std::bitset<MAX_TEAMS> warBits{};
	for (const auto i : range<TeamTypes>(MAX_TEAMS))
		if (atWar(mTeamI, i))
			warBits[i] = true;
	return warBits;
}

void TeamPathingPlotPropsCache::verify() const
{
	for (const auto i : range<TeamTypes>(MAX_TEAMS))
	{
		if (GET_TEAM(mTeamI).canDeclareWar(i) != mCanDeclareWar[i])
		{
			std::clog << "mCanDeclareWar is wrong." << std::endl;
			std::abort();
		}

		if (GET_TEAM(mTeamI).AI_isSneakAttackReady(i) != mAIIsSneakAttackReady[i])
		{
			std::clog << "mAIIsSneakAttackReady is wrong." << std::endl;
			std::abort();
		}

		if ((GET_TEAM(mTeamI).AI_getWarPlan(i) == WARPLAN_LIMITED) != mIsWarPlanLimited[i])
		{
			std::clog << "mIsWarPlanLimited is wrong." << std::endl;
			std::abort();
		}
	}


	if (mWarBits != calcWarBits())
	{
		std::clog << "mWarBits is wrong." << std::endl;
		std::abort();
	}

	CvMap& map = GC.getMapINLINE();

	PlayerTypes playerI = NO_PLAYER;
	for (const auto x : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (GET_PLAYER(x).getTeam() == mTeamI)
		{
			playerI = x;
			break;
		}
	}

	//std::vector<const VersionedBitmap2D*> borderSources;
	//borderSources.reserve(MAX_TEAMS);
	//for (const auto i : range<TeamTypes>(MAX_TEAMS))
	//	if (atWar(mTeamI, i))
	//		borderSources.push_back(&borderDangers[i].getVersionedBitmap());

	const std::bitset<MAX_TEAMS> warBits = calcWarBits();

	const ivec2 mapDim = mCostPlotPropsArray.dim;

	for (const int y : range(mapDim.y))
	{
		for (const int x : range(mapDim.x))
		{
			const CvPlot& plot = *map.plot(x, y);
			const PlotProps cachedValidityProps = mValidityPlotPropsArray[{ x, y }];

			const bool borderDangerCachedProp = cachedValidityProps & getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger);
			const bool unitDangerCachedProp = cachedValidityProps & getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger);

			//if (borderDangerCache)
			{
				const bool cached = ((*mBorderDangerCache)[{ x, y }] & warBits).any();

				if (cached != borderDangerCachedProp)
				{
					std::clog << "Border plot danger bit is wrong at " << ivec2(x, y) << " for team " << +mTeamI << std::endl;
					std::clog << "warBits           = " << warBits << std::endl;
					std::clog << "borderDangerCache = " << (*mBorderDangerCache)[{ x, y }] << std::endl;
					std::clog << "borderDangerCachedProp = " << borderDangerCachedProp << std::endl;
					
					mBorderDangerCache->verify({ x, y });

					std::abort();
				}
			}

			//if (unitPlotDangerCache)
			{
				const bool cached = mUnitPlotDangerCache->getDefenderPlotDanger(mTeamI, { x, y });

				if (cached != unitDangerCachedProp)
				{
					std::clog << "Unit plot danger bit is wrong at " << ivec2(x, y) << " for team " << +mTeamI << std::endl;
					std::clog << "warBits             = " << warBits << std::endl;
					std::clog << "unitPlotDangerCache = " << cached << std::endl;
					std::clog << "unitDangerCachedProp = " << unitDangerCachedProp << std::endl;
					
					std::abort();
				}
			}

			const bool livePlotDanger = GET_PLAYER(playerI).AI_getPlotDanger(&plot) != 0;

			const bool propPlotDanger = borderDangerCachedProp || unitDangerCachedProp;

			if (livePlotDanger != propPlotDanger)
			{
				std::clog << "Plot danger is wrong at " << ivec2(x, y) << " for team " << +mTeamI << std::endl;
				std::clog << "livePlotDanger         = " << livePlotDanger << std::endl;
				std::clog << "borderDangerCachedProp = " << borderDangerCachedProp << std::endl;
				std::clog << "unitDangerCachedProp   = " << unitDangerCachedProp << std::endl;
				std::clog << "warBits                = " << warBits << std::endl;
				std::abort();
			}

			// Okay, if we're given both danger caches, then we now know that both props and the caches are correct for plot danger.

			const PlotProps liveValidityProps = calcValidityPlotPropsPartOne(plot)
				| (borderDangerCachedProp * getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger))
				| (unitDangerCachedProp * getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger));

			if (cachedValidityProps != liveValidityProps)
			{
				std::wclog << L"Cached validity plot props are wrong at plot " << getFullPlotDescription(plot) << " for team " << +mTeamI << std::endl;
				std::clog << "Cached: " << std::bitset<32>(cachedValidityProps) << std::endl;
				std::clog << "Live  : " << std::bitset<32>(liveValidityProps) << std::endl;
				std::abort();
			}

			const PlotProps cachedCostProps = mCostPlotPropsArray[{ x, y }];
			const PlotProps liveCostProps = calcCostPlotProps(plot);

			if (cachedCostProps != liveCostProps)
			{
				std::wclog << L"Cached cost plot props are wrong at plot " << getFullPlotDescription(plot) << " for team " << +mTeamI << std::endl;
				std::clog << "Cached: " << std::bitset<32>(cachedCostProps) << std::endl;
				std::clog << "Live  : " << std::bitset<32>(liveCostProps) << std::endl;
				std::abort();
			}
		}
	}

	std::clog << "TeamPathingPlotPropsCache verify successful." << std::endl;
}

static bool PUF_isEnemyByTeam(const CvUnit* pUnit, int iData1, int iData2)
{
	FAssertMsg(iData1 != -1, "Invalid data argument, should be >= 0");
	FAssertMsg(iData2 != -1, "Invalid data argument, should be >= 0");

	TeamTypes eOtherTeam = (TeamTypes)iData1;
	TeamTypes eOurTeam = GET_PLAYER(pUnit->getCombatOwner(eOtherTeam, pUnit->plot())).getTeam();

	if (pUnit->canCoexistWithEnemyUnit(eOtherTeam))
	{
		return false;
	}

	return (iData2 ? eOtherTeam != eOurTeam : atWar(eOtherTeam, eOurTeam));
}

static bool canEnterTerritory(TeamTypes unitTeamI, TeamTypes ownerTeamI)
{
	if (ownerTeamI == NO_TEAM)
	{
		return true;
	}

	if (GET_TEAM(unitTeamI).isFriendlyTerritory(ownerTeamI))
	{
		return true;
	}

	if (atWar(unitTeamI, ownerTeamI))
	{
		return true;
	}

	//if (isRivalTerritory())
	//{
	//	return true;
	//}
	//
	//if (alwaysInvisible())
	//{
	//	return true;
	//}
	//
	//if (m_pUnitInfo->isHiddenNationality())
	//{
	//	return true;
	//}

	//if (!bIgnoreRightOfPassage)
	{
		if (GET_TEAM(unitTeamI).isOpenBorders(ownerTeamI))
		{
			return true;
		}
	}

	return false;
}

static bool canEnterAreaAsNormalUnit(TeamTypes unitTeamI, TeamTypes ownerTeamI, const CvArea* area)
{
	if (!canEnterTerritory(unitTeamI, ownerTeamI))
	{
		return false;
	}

	if (unitTeamI == BARBARIAN_TEAM)// && DOMAIN_LAND == getDomainType())
	{
		if (ownerTeamI != NO_TEAM && ownerTeamI != unitTeamI)
		{
			if (area && area->isBorderObstacle(ownerTeamI))
			{
				return false;
			}
		}
	}

	return true;
}

// Update everything except plot danger.
void TeamPathingPlotPropsCache::updatePlot(const CvPlot& plot)
{
	// Update everything except plot danger.
	const PlotProps props = calcValidityPlotPropsPartOne(plot);
	const ivec2 coord = getPlotCoord(plot);
	mValidityPlotPropsArray[coord] =
		(mValidityPlotPropsArray[coord] & (getPlotPropMask(kValidityPlotPropBitIndex_BorderPlotDanger) | getPlotPropMask(kValidityPlotPropBitIndex_UnitPlotDanger)))
		| props;

	mCostPlotPropsArray[coord] = calcCostPlotProps(plot);
}

TeamPathingPlotPropsCache::PlotProps TeamPathingPlotPropsCache::calcCostPlotProps(ivec2 coord) const
{
	const CvPlot& plot = *GC.getMapINLINE().plot(coord.x, coord.y);
	return calcCostPlotProps(plot);
}

TeamPathingPlotPropsCache::PlotProps TeamPathingPlotPropsCache::calcValidityPlotPropsForVerification(ivec2 coord, PlayerTypes playerI) const
{
	if (mWarBits != calcWarBits())
		std::abort();

	const bool borderDanger = ((*mBorderDangerCache)[coord] & mWarBits).any();
	const bool unitDanger = mUnitPlotDangerCache->getDefenderPlotDanger(mTeamI, coord);

	const CvPlot& plot = *GC.getMapINLINE().plot(coord.x, coord.y);

	// This is only called in verification, so double check.
	const bool truePlotDanger = GET_PLAYER(playerI).AI_getPlotDanger(&plot);
	if ((borderDanger || unitDanger) != truePlotDanger)
	{
		std::clog << "Cached plot danger is wrong at " << coord << '!' << std::endl;
		std::clog << "borderDanger   = " << borderDanger << std::endl;
		std::clog << "unitDanger     = " << unitDanger << std::endl;
		std::clog << "truePlotDanger = " << truePlotDanger << std::endl;

		mUnitPlotDangerCache->verify(mTeamI, coord);

		std::clog << "unitDanger is correct according to mUnitPlotDangerCache." << std::endl;

		std::abort();
	}
	
	//if (GET_PLAYER(playerI).AI_getPlotDanger(&plot))
	//	props |= getPlotPropMask(kValidityPlotPropBitIndex_PlotDanger);
	return calcValidityPlotPropsPartOne(plot)
		| (borderDanger << kValidityPlotPropBitIndex_BorderPlotDanger)
		| (unitDanger << kValidityPlotPropBitIndex_UnitPlotDanger)
		;
}

TeamPathingPlotPropsCache::PlotProps TeamPathingPlotPropsCache::calcCostPlotProps(const CvPlot& plot) const
{
	const TeamTypes ownerTeam = plot.getTeam();
	PlotProps props{};
	props |= std::max(0, std::min(200, plot.defenseModifier(mTeamI, false)));
	props |= PlotProps(std::max<int>(0, plot.getRouteType())) << kCostPlotPropBitIndex_RouteType;

	if (!plot.isRoute()) // fast path
		props |= getPlotPropMask(kCostPlotPropBitIndex_AdjNoRoute);
	else
	{
		const bool hasBridgeBuilding = GET_TEAM(mTeamI).isBridgeBuilding();
		for (const size_t adjI : range(std::size(kAdj)))
		{
			const CvPlot* const adjPlot = plotDirection(plot.getX_INLINE(), plot.getY_INLINE(), kAdjIndexToDirectionType[adjI]);
			const bool crossable = adjPlot && adjPlot->isRoute() && (hasBridgeBuilding || !plot.isRiverCrossing(kAdjIndexToDirectionType[adjI]));
			if (!crossable)
				props |= PlotProps(1) << (kCostPlotPropBitIndex_AdjNoRoute + adjI);
		}
	}

	if (ownerTeam != mTeamI)
		props |= getPlotPropMask(kCostPlotPropBitIndex_NotMyTeam);

	if (atWar(ownerTeam, mTeamI))
		props |= getPlotPropMask(kCostPlotPropBitIndex_IsEnemyTeam);

	CvGlobals& globals = GC;
	int iRegularCost = plot.getFeatureType() == NO_FEATURE
		? globals.getTerrainInfo(plot.getTerrainType()).getMovementCost()
		: globals.getFeatureInfo(plot.getFeatureType()).getMovementCost();
	if (plot.isHills())
		iRegularCost += globals.getHILLS_EXTRA_MOVEMENT();

	props |= iRegularCost << kCostPlotPropBitIndex_RegularCost;

	props |= isForest(plot.getFeatureType()) << kCostPlotPropBitIndex_ForestHalfCostMovement;
	props |= plot.isHills() << kCostPlotPropBitIndex_HillsHalfCostMovement;

	return props;
}

TeamPathingPlotPropsCache::PlotProps TeamPathingPlotPropsCache::calcValidityPlotPropsPartOne(const CvPlot& plot) const
{
	const TeamTypes ownerTeam = plot.getTeam();
	const bool isOwned = ownerTeam != NO_TEAM;

	PlotProps props{};

	if (atWar(ownerTeam, mTeamI))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_IsEnemyTeam);

	if (isOwned)
		props |= getPlotPropMask(kValidityPlotPropBitIndex_IsOwned);

	if (!plot.isRevealed(mTeamI, false) || (isOwned && ownerTeam != mTeamI))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_NotSafeTerritory);

	//if (mTeamI == BARBARIAN_TEAM) // Only need for animals, but I suppose some animals won't be barbs if you world builder'd them in.
	if (isOwned || plot.getBonusType() != NO_BONUS || plot.getImprovementType() != NO_IMPROVEMENT || plot.getNumUnits() > 0)
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsAnimalAvoid);

	if (plot.isCity() && atWar(ownerTeam, mTeamI))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsEnemyCity);

	if (plot.plotCheck(PUF_isEnemyByTeam, mTeamI, false, NO_PLAYER, NO_TEAM, PUF_isVisible, mTeamI))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnit);
	else
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnit);
	
	// NOTE: Originally has "true" here for always hostile, and path verification never complained. Changing to the city/fort check in `CvUnit::isAlwaysHostile`.
	if (plot.plotCheck(PUF_isEnemyByTeam, mTeamI, !plot.isCity(true, mTeamI), NO_PLAYER, NO_TEAM, PUF_isVisible, mTeamI))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsVisibleEnemyUnitForAlwaysHostile);
	else
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_NotIsVisibleEnemyUnitForAlwaysHostile);

	const bool inDomain = !plot.isImpassable() && !plot.isWater();
	if (!inDomain)
	{
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_AlwaysHostile);
		//props |= kPlotPropsMask_PathValid_CanMoveInto_IsCantEnterArea_AlwaysHostile;
		
		// Ensure this isn't set for impassble land so that we can use kPlotPropsMask_WaterDiagonalSpecialCase.
	}


	if (!canEnterAreaAsNormalUnit(mTeamI, ownerTeam, plot.area()))
	{
		// Replicate this set of checks.
		//if (!(GET_TEAM(getTeam()).canDeclareWar(ePlotTeam)))
		//{
		//	return false;
		//}
		//
		//if (isHuman())
		//{
		//	if (!bDeclareWar)
		//	{
		//		return false;
		//	}
		//}
		//else
		//{
		//	if (GET_TEAM(getTeam()).AI_isSneakAttackReady(ePlotTeam))
		//	{
		//		if (!(getGroup()->AI_isDeclareWar(pPlot)))
		//		{
		//			return false;
		//		}
		//	}
		//	else
		//	{
		//		return false;
		//	}
		//}

		if (!mCanDeclareWar[ownerTeam])
		{
			props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_False);
			props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited);
			props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_True);
		}
		else
		{
			if (mAIIsSneakAttackReady[ownerTeam])
			{
				// if EUnitWarAIType::False: return false
				// if EUnitWarAIType::Limited && !bLimitedWar: return false
				// else... return true

				const bool bLimitedWar = ownerTeam != NO_TEAM && mIsWarPlanLimited[ownerTeam];

				props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_False);
				if (!bLimitedWar)
					props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited);
			}
			else
			{
				props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_False);
				props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_Limited);
				props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_IsCantEnterArea_UnitWarAIType_True);
			}
		}
	}

	// if (!pPlot->isWater() && !canMoveAllTerrain())
	// {
	// 	if (!pPlot->isFriendlyCity(*this, true) || !pPlot->isCoastalLand())
	// 	{
	// 		return false;
	// 	}
	// }

	static const auto isAlwaysHostile = [](TeamTypes unitTeam, bool unitInfoAlwaysHostile, const CvPlot& pPlot) {
		if (!unitInfoAlwaysHostile)
		{
			return false;
		}

		if (pPlot.isCity(true, unitTeam))
		{
			return false;
		}

		return true;
	};

	static const auto getCombatOwnerTeam = [](TeamTypes unitTeam, bool alwaysHostile, TeamTypes eForTeam, const CvPlot& pPlot) {
		if (eForTeam != NO_TEAM && unitTeam != eForTeam && eForTeam != BARBARIAN_TEAM)
		{
			if (isAlwaysHostile(unitTeam, alwaysHostile, pPlot))
			{
				return BARBARIAN_TEAM;
			}
		}

		return unitTeam;
	};

	static const auto isFriendlyCity = [](const CvPlot& plot, TeamTypes unitTeamI, bool alwaysHostile) {
		if (!plot.isCity(true, unitTeamI))
		{
			return false;
		}

		const bool alwaysHostileAtPlot = alwaysHostile && !plot.isCity(true, unitTeamI);

		//if (isVisibleEnemyUnit(&kUnit))
		if (plot.plotCheck(PUF_isEnemyByTeam, unitTeamI, alwaysHostileAtPlot, NO_PLAYER, NO_TEAM, PUF_isVisible, unitTeamI))
		{
			return false;
		}

		const TeamTypes ePlotTeam = plot.getTeam();

		if (ePlotTeam != NO_TEAM)
		{
			if (alwaysHostileAtPlot ? ePlotTeam != unitTeamI : atWar(ePlotTeam, unitTeamI))
			{
				return false;
			}

			const TeamTypes eTeam = getCombatOwnerTeam(unitTeamI, alwaysHostile, ePlotTeam, plot);

			if (eTeam == ePlotTeam)
			{
				return true;
			}

			if (GET_TEAM(eTeam).isOpenBorders(ePlotTeam))
			{
				return true;
			}

			if (GET_TEAM(ePlotTeam).isVassal(eTeam))
			{
				return true;
			}
		}

		return false;
		};

	const bool isImpassable = plot.isImpassable();

	if (isImpassable)
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_Impassable);

	const bool isSeaUnitOceanWaterDomain = plot.isWater();// && !isImpassable;
	const bool isSeaUnitCoastHuggerWaterDomain = isSeaUnitOceanWaterDomain && (plot.getTerrainType() != kOceanTerrainType || ownerTeam == mTeamI);
	const bool isCoastalLand = plot.isCoastalLand();
	const bool isSeaUnitCoastalLandAlwaysHostileFriendlyCity = isCoastalLand && isFriendlyCity(plot, mTeamI, true);
	const bool isSeaUnitCoastalLandRegularFriendlyCity = isCoastalLand && (isSeaUnitCoastalLandAlwaysHostileFriendlyCity || isFriendlyCity(plot, mTeamI, false));
	
	if (!(isSeaUnitCoastHuggerWaterDomain || isSeaUnitCoastalLandRegularFriendlyCity))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastHuggerDomain_Regular);

	if (!(isSeaUnitOceanWaterDomain || isSeaUnitCoastalLandRegularFriendlyCity))
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotOceanDomain_Regular);

	if (isSeaUnitCoastalLandRegularFriendlyCity != isSeaUnitCoastalLandAlwaysHostileFriendlyCity) // isSeaUnitCoastalLandRegularFriendlyCity && !isSeaUnitCoastalLandAlwaysHostileFriendlyCity
		props |= getPlotPropMask(kValidityPlotPropBitIndex_CanMoveInto_SeaUnit_NotCoastalCity_AlwaysHostile);

	
	if (isSeaUnitCoastHuggerWaterDomain)
	{
		// Check for impassable water diagonal.
		// Reuse road bits to indicate forbidden steps.
		//props &= ~(0b1111 << kPlotPropsBitIndex_AdjNoRoute);
		//assert((props & kPlotPropsCmp_WaterDiagonalSpecialCase) == 0); // We don't have water cities
		CvMap& map = GC.getMapINLINE();
		const ivec2 coord = getPlotCoord(plot);
		// This order matches up with kWaterDiagonalSpecialCaseAdjMask in VectorisedPathStepFunction.
		constexpr std::array kDiags{ ivec2(-1, -1), ivec2(+1, -1), ivec2(-1, +1), ivec2(+1, +1) };
		for (const size_t i : range(kDiags.size()))
		{
			const ivec2 d = kDiags[i];
			const ivec2 adjCoord = coord + d;
			if (const CvPlot* const adjPlot = map.plot(adjCoord.x, adjCoord.y))
			{
				if (adjPlot->isWater())
				{
					// if (pFromPlot->isWater() && pToPlot->isWater())
					// {
					// 	if (!(GC.getMapINLINE().plotINLINE(parent->m_iX, node->m_iY)->isWater()) && !(GC.getMapINLINE().plotINLINE(node->m_iX, parent->m_iY)->isWater()))
					// 	{
					// 		return FALSE;
					// 	}
					// }

					if (!map.plotINLINE(coord.x, adjCoord.y)->isWater() && !map.plotINLINE(adjCoord.x, coord.y)->isWater())
						//props |= kPlotPropsCmp_WaterDiagonalSpecialCase | (1 << (kPlotPropsBitIndex_AdjNoRoute + i));
						props |= getPlotPropMask(kValidityPlotPropBitIndex_WaterDiagonalForbid0) << i;
				}
			}
		}
	}

	return props;
}

#endif