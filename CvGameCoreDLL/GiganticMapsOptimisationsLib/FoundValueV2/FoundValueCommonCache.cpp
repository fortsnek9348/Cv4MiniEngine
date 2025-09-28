#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueCommonCache.h"
#include "FoundValueVectorisedCalculation.h"

#include "../../CvInfos.h"
#include "../../CvPlayerAI.h"
#include "../../CvGameCoreUtils.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>

using heck::range;
using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

static constexpr int kBarbarianCivUnitAvoidanceInvalidationBlockDimShift = std::countr_zero(8u);

static std::array<int, 3> calcYieldsForHomePlot(std::array<int, 3> aiYield, FeatureTypes featureI, BonusTypes bonusI, ImprovementTypes improvementI)
{
	// Code from AI_foundValue.
	for (const YieldTypes eYield : range<YieldTypes>(NUM_YIELD_TYPES))
	{
		int iBasePlotYield = aiYield[eYield];
		aiYield[eYield] += GC.getYieldInfo(eYield).getCityChange();

		if (featureI != NO_FEATURE)
		{
			aiYield[eYield] -= GC.getFeatureInfo(featureI).getYieldChange(eYield);
			iBasePlotYield = std::max(iBasePlotYield, aiYield[eYield]);
		}

		if (bonusI == NO_BONUS)
		{
			aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity());
		}
		else
		{
			int iBonusYieldChange = GC.getBonusInfo(bonusI).getYieldChange(eYield);
			aiYield[eYield] += iBonusYieldChange;
			iBasePlotYield += iBonusYieldChange;

			aiYield[eYield] = std::max(aiYield[eYield], GC.getYieldInfo(eYield).getMinCity());
		}

		if (improvementI != NO_IMPROVEMENT)
		{
			iBasePlotYield += GC.getImprovementInfo(improvementI).getImprovementBonusYield(bonusI, eYield);

			if (iBasePlotYield > aiYield[eYield])
			{
				aiYield[eYield] -= 2 * (iBasePlotYield - aiYield[eYield]);
			}
			else
			{
				aiYield[eYield] += aiYield[eYield] - iBasePlotYield;
			}
		}
	}

	return aiYield;
}

static int32_t packYields(std::array<int, 3> yields)
{
	if (std::ranges::min(yields) < INT8_MIN || INT8_MAX < std::ranges::max(yields))
		std::abort();
	return uint8_t(yields[0]) | (int32_t(uint8_t(yields[1])) << 8) | (int32_t(uint8_t(yields[2])) << 16);
}
// Same as CvPlot::calculateNatureYield, but with explicit bonus control.
static int calculateNatureYield(const CvPlot& plot, YieldTypes eYield, bool withBonus, bool bIgnoreFeature)
{
	BonusTypes eBonus;
	int iYield;

	if (plot.isImpassable())
	{
		return 0;
	}

	FAssertMsg(plot.getTerrainType() != NO_TERRAIN, "TerrainType is not assigned a valid value");

	iYield = GC.getTerrainInfo(plot.getTerrainType()).getYield(eYield);

	if (plot.isHills())
	{
		iYield += GC.getYieldInfo(eYield).getHillsChange();
	}

	if (plot.isPeak())
	{
		iYield += GC.getYieldInfo(eYield).getPeakChange();
	}

	if (plot.isLake())
	{
		iYield += GC.getYieldInfo(eYield).getLakeChange();
	}

	if (withBonus)
	{
		eBonus = plot.getBonusType();

		if (eBonus != NO_BONUS)
		{
			iYield += GC.getBonusInfo(eBonus).getYieldChange(eYield);
		}
	}

	if (plot.isRiver())
	{
		iYield += ((bIgnoreFeature || (plot.getFeatureType() == NO_FEATURE))
			? GC.getTerrainInfo(plot.getTerrainType()).getRiverYieldChange(eYield)
			: GC.getFeatureInfo(plot.getFeatureType()).getRiverYieldChange(eYield));
	}

	if (plot.isHills())
	{
		iYield += ((bIgnoreFeature || (plot.getFeatureType() == NO_FEATURE))
			? GC.getTerrainInfo(plot.getTerrainType()).getHillsYieldChange(eYield)
			: GC.getFeatureInfo(plot.getFeatureType()).getHillsYieldChange(eYield));
	}

	if (!bIgnoreFeature)
	{
		if (plot.getFeatureType() != NO_FEATURE)
		{
			iYield += GC.getFeatureInfo(plot.getFeatureType()).getYieldChange(eYield);
		}
	}

	return std::max(0, iYield);
}


static int calculateBestNatureYield(const CvPlot& plot, YieldTypes eIndex, bool withBonus)
{
	return std::max(calculateNatureYield(plot, eIndex, withBonus, false), calculateNatureYield(plot, eIndex, withBonus, true));
}

static int32_t calculateBestNaturePackedYield(const CvPlot& plot, bool withBonus)
{
	return packYields({
		calculateBestNatureYield(plot, YIELD_FOOD, withBonus),
		calculateBestNatureYield(plot, YIELD_PRODUCTION, withBonus),
		calculateBestNatureYield(plot, YIELD_COMMERCE, withBonus),
		});
}

static int32_t calculateBestNaturePackedYieldWithImprovement(const CvPlot& plot, bool withBonus, std::span<const ImprovementTypes> improvementLookup)
{
	std::array<int, 3> improvementYields{};
	if (const BonusTypes bonusI = plot.getBonusType(); withBonus && bonusI != NO_BONUS)
	{
		const CvImprovementInfo& info = GC.getImprovementInfo(improvementLookup[bonusI]);
		improvementYields = {
			info.getImprovementBonusYield(bonusI, YIELD_FOOD),
			info.getImprovementBonusYield(bonusI, YIELD_PRODUCTION),
			info.getImprovementBonusYield(bonusI, YIELD_COMMERCE),
		};
	}

	return packYields({
		calculateBestNatureYield(plot, YIELD_FOOD, withBonus) + improvementYields[YIELD_FOOD],
		calculateBestNatureYield(plot, YIELD_PRODUCTION, withBonus) + improvementYields[YIELD_PRODUCTION],
		calculateBestNatureYield(plot, YIELD_COMMERCE, withBonus) + improvementYields[YIELD_COMMERCE],
		});
}

static bool hasCivUnitInRange(PlayerTypes playerI, heck::ivec2 coord, int iMinRivalRange)
{
	CvMap& map = GC.getMapINLINE();
	for (int iDY = -iMinRivalRange; iDY <= iMinRivalRange; ++iDY)
		for (int iDX = -iMinRivalRange; iDX <= iMinRivalRange; ++iDX)
			if (const CvPlot* const pLoopPlot = map.plotINLINE(coord.x + iDX, coord.y + iDY))
				if (pLoopPlot->plotCheck(PUF_isOtherTeam, playerI))
					return true;
	return false;
}

CommonCache::CommonCache(CivMapGeometry mapSizeInfo, const AreaMap& areaMap)
	: mGlobalData{
		.FOOD_CONSUMPTION_PER_POPULATION = GC.getFOOD_CONSUMPTION_PER_POPULATION(),
		.FRESH_WATER_HEALTH_CHANGE = GC.getDefineINT("FRESH_WATER_HEALTH_CHANGE"),
		.maxPlotDistanceDivisor = heck::IntegerDivisionByConstantParams(GC.getMapINLINE().maxPlotDistance()),
		.maxPlotDistance = GC.getMapINLINE().maxPlotDistance(),
	}
	, mFlags(mapSizeInfo, 0, FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotOutOfBounds)
	, mAreaIds(mapSizeInfo, -1, -1)
	, mHomePlotYieldsValuations{ PaddedArray2D<int32_t>{ mapSizeInfo, 0, 0 }, PaddedArray2D<int32_t>{ mapSizeInfo, 0, 0 } }
	, mPlotYields(mapSizeInfo, 0, 0)
	, mFeatureHealthPercent(mapSizeInfo, 0, 0)
	, mBestNaturalYieldsWithImprovement{ PaddedArray2D<int32_t>{ mapSizeInfo, 0, 0 }, PaddedArray2D<int32_t>{ mapSizeInfo, 0, 0 } }
	, mBestNaturalYieldsWithBonusOnly(mapSizeInfo, 0, 0)
	, mNearestCityCache(mapSizeInfo, areaMap, NO_PLAYER)
	, mPlotBonuses(mapSizeInfo, 0, 0)
	//, mRivalUnitRange(GC.getDefineINT("MIN_BARBARIAN_CITY_STARTING_DISTANCE"))
	, mRivalUnitRange(0)
	, mUnitMovementInvalidation(mapSizeInfo, kBarbarianCivUnitAvoidanceInvalidationBlockDimShift)
	, mBonusImprovementTypes(GC.getNumBonusInfos())
{
	for (const BonusTypes eBonus : range<BonusTypes>(GC.getNumBonusInfos()))
	{
		ImprovementTypes eBonusImprovement = NO_IMPROVEMENT;

		for (const int iImprovement : range(GC.getNumImprovementInfos()))
		{
			const CvImprovementInfo& kImprovement = GC.getImprovementInfo((ImprovementTypes)iImprovement);

			if (kImprovement.isImprovementBonusMakesValid(eBonus))
			{
				eBonusImprovement = (ImprovementTypes)iImprovement;
				mGlobalData.bonusHasImprovement.set(eBonus, true);
				break;
			}
		}

		mBonusImprovementTypes[eBonus] = eBonusImprovement;
	}

	const int iMinRivalRange = mRivalUnitRange;

	//mUnitMovementInvalidation.invalidateAll();
	heck::parallelForEachN(mapSizeInfo.dim.y, [mapSizeInfo, &map = GC.getMapINLINE(), iMinRivalRange, minWaterSize = GC.getMIN_WATER_SIZE_FOR_OCEAN(), this](int y) {
		for (const int x : range(mapSizeInfo.dim.x))
		{
			const CvPlot& plot = *map.plot(x, y);
			const ivec2 coord{ x, y };

			uint16_t flags = 0;
			if (plot.isCoastalLand(minWaterSize))
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isOceanicCoastalLand;
			if (plot.isHills())
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isHills;
			if (plot.isWater())
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotWater;
			if (plot.isRiver())
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotRiver;
			if (plot.isFreshWater())
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isFreshWater;
			if (const FeatureTypes eFeature = plot.getFeatureType(); eFeature != NO_FEATURE)
			{
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotHasAnyFeature;
				if (eFeature != NO_FEATURE && GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
					flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotFeatureFoodPenaltyCondition;
			}
			if (plot.area()->getNumTiles() <= 2)
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isAreaNumPlotsAtMostTwo;
			if (plot.getOwnerINLINE() != NO_PLAYER)
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotOwned;
			if (plot.area()->getNumCities() > 0)
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_hasAnyCitiesOnArea;
			if (plot.isCityRadius()) // Conveniently tracked already
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotCityRadius;
			if (hasCivUnitInRange(BARBARIAN_PLAYER, { x, y }, iMinRivalRange))
				flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isBarbarianCivUnitAvoidance;

			mFlags.set(coord, flags);

			mAreaIds.set(coord, plot.getArea());

			onPlotGetYieldChanged(plot);

			updatePlotYieldStuff(plot, coord);

			mPlotBonuses.set(coord, static_cast<int8_t>(plot.getBonusType()));
		}
		});

	

	updateCityDistances();
}

void CommonCache::updatePlotYieldStuff(const CvPlot& plot, ivec2 coord)
{
	uint16_t flags = mFlags.get(coord);
	flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotHasAnyFeature;
	flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotFeatureFoodPenaltyCondition;

	if (const FeatureTypes eFeature = plot.getFeatureType(); eFeature != NO_FEATURE)
	{
		flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotHasAnyFeature;
		if (GC.getFeatureInfo(eFeature).getYieldChange(YIELD_FOOD) < 0)
			flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_getCityPlotFeatureFoodPenaltyCondition;
	}

	mFlags.set(coord, flags);

	const std::array<int, 3> liveYields{
		plot.getYield(YIELD_FOOD),
		plot.getYield(YIELD_PRODUCTION),
		plot.getYield(YIELD_COMMERCE),
	};

	const std::array<int, 3> homePlotYieldsWithBonus = calcYieldsForHomePlot(liveYields, plot.getFeatureType(), plot.getBonusType(),
		plot.getBonusType() != NO_BONUS ? mBonusImprovementTypes[plot.getBonusType()] : NO_IMPROVEMENT);
	const std::array<int, 3> homePlotYieldsNoBonus = plot.getBonusType() == NO_BONUS
		? homePlotYieldsWithBonus
		: calcYieldsForHomePlot(liveYields, plot.getFeatureType(), NO_BONUS, NO_IMPROVEMENT);

	mHomePlotYieldsValuations[0].set(coord, packYields(homePlotYieldsNoBonus));
	mHomePlotYieldsValuations[1].set(coord, packYields(homePlotYieldsWithBonus));

	if (const FeatureTypes eFeature = plot.getFeatureType(); eFeature != NO_FEATURE)
		mFeatureHealthPercent.set(coord, static_cast<int8_t>(GC.getFeatureInfo(eFeature).getHealthPercent()));
	else
		mFeatureHealthPercent.set(coord, 0);

	const int32_t bestNaturePackedYieldNoBonus = calculateBestNaturePackedYieldWithImprovement(plot, false, {});

	mBestNaturalYieldsWithImprovement[0].set(coord, bestNaturePackedYieldNoBonus);
	mBestNaturalYieldsWithImprovement[1].set(coord, plot.getBonusType() == NO_BONUS
		? bestNaturePackedYieldNoBonus
		: calculateBestNaturePackedYieldWithImprovement(plot, true, mBonusImprovementTypes)
	);

	mBestNaturalYieldsWithBonusOnly.set(coord, plot.getBonusType() == NO_BONUS ? bestNaturePackedYieldNoBonus : calculateBestNaturePackedYield(plot, true));
}

void CommonCache::onPlotTerrainOrFeatureChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);
	updatePlotYieldStuff(plot, coord);

	//std::clog << __FUNCTION__ << ": " << coord << std::endl;

	// Also need to update the fresh water flag in case an oasis gets nuked.
	for (const int dy : range(-1, 1 + 1))
	{
		for (const int dx : range(-1, 1 + 1))
		{
			const ivec2 adjCoord = coord + ivec2(dx, dy);
			if (const CvPlot* const adjPlot = GC.getMapINLINE().plot(adjCoord.x, adjCoord.y))
			{
				uint16_t flags = mFlags.get(adjCoord);
				flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_isFreshWater;
				if (adjPlot->isFreshWater())
					flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isFreshWater;
				mFlags.set(adjCoord, flags);
			}
		}
	}
}

void CommonCache::onPlotGetYieldChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);

	const std::array<int, 3> liveYields{
		plot.getYield(YIELD_FOOD),
		plot.getYield(YIELD_PRODUCTION),
		plot.getYield(YIELD_COMMERCE),
	};

	mPlotYields.set(coord, packYields(liveYields));
	updatePlotYieldStuff(plot, coord);
}

void CommonCache::onPlotOwnerChanged(const CvPlot& plot, [[maybe_unused]] PlayerTypes from, PlayerTypes to)
{
	const ivec2 coord = getPlotCoord(plot);
	uint16_t flags = mFlags.get(coord);
	flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotOwned;
	if (to != NO_PLAYER)
		flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotOwned;
	mFlags.set(coord, flags);
}

void CommonCache::onAfterCityCreated(const CvPlot& plot)
{
	mNearestCityCache.onAfterCityCreated(plot, NO_PLAYER);
}

void CommonCache::onAfterCityDestroyed(const CvPlot& plot)
{
	mNearestCityCache.onAfterCityDestroyed(plot, NO_PLAYER);
}

void CommonCache::onCityRadiusCountChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);
	uint16_t flags = mFlags.get(coord);
	flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotCityRadius;
	if (plot.isCityRadius())
		flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isCityPlotCityRadius;
	mFlags.set(coord, flags);
}

void CommonCache::onUnitMoved(const CvUnit& unit, ivec2 from, ivec2 to)
{
	// Only for civ units.
	if (unit.getOwnerINLINE() != BARBARIAN_PLAYER)
	{
		if (from.x >= 0)
			mUnitMovementInvalidation.invalidateStepRadius(from, mRivalUnitRange);
		if (to.x >= 0)
			mUnitMovementInvalidation.invalidateStepRadius(to, mRivalUnitRange);
	}
}

void CommonCache::onAreaCityCountChanged(const CvArea&, int from, int to)
{
	if ((from != 0) == (to != 0))
		return;

	//mAreCityDistancesDirty = true;
	mAreaCityPresenceDirty = true;
}

void CommonCache::onPlotBonusChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);
	mPlotBonuses.set(coord, static_cast<int8_t>(plot.getBonusType()));
	updatePlotYieldStuff(plot, coord);
}

void CommonCache::update([[maybe_unused]] PlayerTypes playerI)
{
	updateAreaCityPresence();

	// Any-player city distances and barbarian civ unit avoidance is only needed by the barbarian player.
	//if (playerI == BARBARIAN_PLAYER)
	{
		updateBarbarianCivUnitAvoidance();
		updateCityDistances();
	}
}

FoundValueCalculationCommonMapInterface CommonCache::createCommonMapInterface(ivec2 foundValueCoord) const
{
	return {
		.pitch = mFlags.getPitch(mFlags.getMapGeometry()),
		.addressingCoord = foundValueCoord + mFlags.getMapOffset(),
		.flags = mFlags.getMemorySpan(),
		.areaIdsMap = mAreaIds.getMemorySpan(),
		.homePlotYieldValuationMaps{
			mHomePlotYieldsValuations[0].getMemorySpan(),
			mHomePlotYieldsValuations[1].getMemorySpan(),
			},
		.plotYieldMap = mPlotYields.getMemorySpan(),
		.featureHealthPercentMap = mFeatureHealthPercent.getMemorySpan(),
		.bestNatureYieldWithImprovementMap{
			mBestNaturalYieldsWithImprovement[0].getMemorySpan(),
			mBestNaturalYieldsWithImprovement[1].getMemorySpan(),
			},
		.bestNatureYieldMap{
			mBestNaturalYieldsWithImprovement[0].getMemorySpan(),
			mBestNaturalYieldsWithBonusOnly.getMemorySpan(),
			},
		.barbarianNearestCityDistanceValue = mNearestCityCache.getDistanceValueField().getMemorySpan(),
		.plotBonuses = mPlotBonuses.getMemorySpan(),
	};
}

FoundValueCalculationCommonGlobalDataInterface CommonCache::createCommonGlobalDataInterface() const
{
	return mGlobalData;
}

void CommonCache::verify(const AreaMap& areaMap) const
{
	// Call `update` first.
	if (mUnitMovementInvalidation.hasAnyDirty())
		std::abort();

	// Verify padding.
	mFlags.verify();
	mAreaIds.verify();
	mHomePlotYieldsValuations[0].verify();
	mHomePlotYieldsValuations[1].verify();
	mPlotYields.verify();
	mFeatureHealthPercent.verify();
	mBestNaturalYieldsWithImprovement[0].verify();
	mBestNaturalYieldsWithImprovement[1].verify();
	mPlotBonuses.verify();

	CommonCache fresh(mFlags.getMapGeometry(), areaMap);
	for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
		fresh.update(playerI);

	const auto verifyArray = [&]<class T>(const PaddedArray2D<T>& cached, const PaddedArray2D<T>& fresh) {
		if (cached == fresh)
			return;

		for (const int y : range(cached.getMapGeometry().dim.y))
		{
			for (const int x : range(cached.getMapGeometry().dim.x))
			{
				if (cached.get({ x, y }) != fresh.get({ x, y }))
				{
					std::wclog << L"Mismatch at " << ivec2(x, y) << L": " << getFullPlotDescription(*GC.getMapINLINE().plot(x, y)) << std::endl;
					std::clog << "Cached = " << +cached.get({ x, y }) << " = " << std::bitset<32>(+cached.get({ x, y })) << std::endl;
					std::clog << "Fresh  = " << +fresh.get({ x, y }) << " = " << std::bitset<32>(+fresh.get({ x, y })) << std::endl;
					std::abort();
				}
			}
		}


		std::abort();
		};

	verifyArray(mFlags, fresh.mFlags);
	verifyArray(mAreaIds, fresh.mAreaIds);
	verifyArray(mHomePlotYieldsValuations[0], fresh.mHomePlotYieldsValuations[0]);
	verifyArray(mHomePlotYieldsValuations[1], fresh.mHomePlotYieldsValuations[1]);
	verifyArray(mPlotYields, fresh.mPlotYields);
	verifyArray(mFeatureHealthPercent, fresh.mFeatureHealthPercent);
	verifyArray(mBestNaturalYieldsWithImprovement[0], fresh.mBestNaturalYieldsWithImprovement[0]); //
	verifyArray(mBestNaturalYieldsWithImprovement[1], fresh.mBestNaturalYieldsWithImprovement[1]);
	verifyArray(mPlotBonuses, fresh.mPlotBonuses);

	mNearestCityCache.verify();
}

void CommonCache::updateBarbarianCivUnitAvoidance()
{
	std::vector<i16aabb2> rects;
	rects.reserve(100);

	mUnitMovementInvalidation.getAllDirtyRects(rects);
	mUnitMovementInvalidation.reset();

	std::for_each(std::execution::par, rects.begin(), rects.end(), [this](i16aabb2 rect) {
		// TODO: Is it worth clearing then setting flags after finding civ units?
		for (const int y : range(rect.min.y, rect.max.y))
		{
			for (const int x : range(rect.min.x, rect.max.x))
			{
				const ivec2 coord{ x, y };
				uint16_t flags = mFlags.get(coord);
				flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_isBarbarianCivUnitAvoidance;
				if (hasCivUnitInRange(BARBARIAN_PLAYER, { x, y }, mRivalUnitRange))
					flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_isBarbarianCivUnitAvoidance;
				mFlags.set(coord, flags);
			}
		}
		});
}
void CommonCache::updateCityDistances()
{
	mNearestCityCache.update();
}
void CommonCache::updateAreaCityPresence()
{
	if (mAreaCityPresenceDirty)
	{
		mAreaCityPresenceDirty = false;
		const CivMapGeometry mapGeom = mFlags.getMapGeometry();
		heck::parallelForEachN(mapGeom.dim.y, [mapGeom, &map = GC.getMapINLINE(), this](int y) {
			for (const int x : range(mapGeom.dim.x))
			{
				uint16_t flags = mFlags.get({ x, y });
				flags &= ~FoundValueCalculationCommonMapInterface::kCommonMapFlag_hasAnyCitiesOnArea;
				if (map.plotINLINE(x, y)->area()->getNumCities() > 0)
					flags |= FoundValueCalculationCommonMapInterface::kCommonMapFlag_hasAnyCitiesOnArea;
				mFlags.set({ x, y }, flags);
			}
		});
	}
}

#endif