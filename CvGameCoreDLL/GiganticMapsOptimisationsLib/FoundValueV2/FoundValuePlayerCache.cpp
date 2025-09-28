#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValuePlayerCache.h"
#include "FoundValueVectorisedCalculation.h"
#include "FoundValueDeadlockedBonusesTracker.h"
#include "FoundValueCommonCache.h"

#include "../../CvPlayerAI.h"
#include "../../CvTeamAI.h"
#include "../../CvInfos.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;
using namespace GiganticMapsOptimisationsLib;

//static std::pair<int, int> countCityRadiusRevealedBonuses(ivec2 coord, TeamTypes teamI)
//{
//	std::array<BonusTypes, NUM_CITY_PLOTS> bonusList{};
//	size_t numBonuses = 0;
//	
//	for (const int cityPlotI : range(NUM_CITY_PLOTS))
//		if (const CvPlot* const cityPlot = plotCity(coord.x, coord.y, cityPlotI))
//			if (const BonusTypes bonusI = cityPlot->getBonusType(teamI); bonusI != NO_BONUS)
//				bonusList[numBonuses++] = bonusI;
//	
//	std::sort(bonusList.begin(), bonusList.begin() + numBonuses);
//	const int numUniqueBonuses = int(std::unique(bonusList.begin(), bonusList.begin() + numBonuses) - bonusList.begin());
//	return { (int)numBonuses, (int)numUniqueBonuses };
//}

PlayerCache::PlayerCache(CivMapGeometry mapGeom, const AreaMap& areaMap, PlayerTypes playerI, const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker)
	: mPlayerI(playerI)
	, mTeamI(GET_PLAYER(playerI).getTeam())
	, mFlags(mapGeom, 0, FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasForeignOwnerOrOutOfBounds)
	, mRevealedFlagAndOurCultureMultipliers(mapGeom, 0, 0)
	, mNearestCityCache(mapGeom, areaMap, playerI)
	, mStage2Data(mapGeom, 0, 0)
{
	CvMap& map = GC.getMapINLINE();
	const CvPlayerAI& player = GET_PLAYER(playerI);
	const TeamTypes teamI = mTeamI;

	// Pre-populate.
	for (const int areaId : areaMap.unwrappedAreaRectsByAreaId | std::views::keys)
		(void)mTeamCityCountCaseBitsByAreaId[areaId];
	(void)updateTeamCityCountCaseBitsMap();

	

	mGreed = 100;

	{
		for (int iI = 0; iI < GC.getNumTraitInfos(); iI++)
		{
			if (player.hasTrait((TraitTypes)iI))
			{
				mGreed += GC.getTraitInfo((TraitTypes)iI).getUpkeepModifier() / 2;
				mGreed += 20 * GC.getTraitInfo((TraitTypes)iI).getCommerceChange(COMMERCE_CULTURE);
			}
		}
	}

	mClaimThreshold = GC.getGameINLINE().getCultureThreshold((CultureLevelTypes)(std::min(2, (GC.getNumCultureLevelInfos() - 1))));
	mClaimThreshold = std::max(1, mClaimThreshold);
	mClaimThreshold *= std::max(100, mGreed);


	heck::parallelForEachN(mapGeom.dim.y, [mapGeom, &deadlockedBonusesTracker, &areaMap, playerI, teamI, &map, this](int y) {
		for (const int x : range(mapGeom.dim.x))
		{
			const CvPlot& plot = *map.plot(x, y);
			const ivec2 coord{ x, y };
			const PlayerTypes owner = plot.getOwnerINLINE();

			uint8_t flags = 0;
			if (deadlockedBonusesTracker.canFound(coord, playerI))
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_canFound;
			// kPlayerMapFlag_isPlayerAIPlotCitySite
			// kPlayerMapFlag_isCityPlotWithinPlayerAICitySites
			if (owner != NO_PLAYER && owner != playerI)
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasForeignOwnerOrOutOfBounds;
			// kPlayerMapFlag_isMyNearestCityACapital
			if (plot.area()->getCitiesPerPlayer(playerI) > 0)
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasMyCitiesOnArea;
			if (owner == playerI)
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByThisPlayer;
			if (plot.getBonusType(teamI) != NO_BONUS)
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasRevealedBonus;
			if (plot.getTeam() == teamI)
				flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByOurTeam;
			
			mFlags.setWithoutWrapMirroring(coord, flags);

			updateCultureMultiplier(plot);

			// Properties that are only read at the found value plot.
			if (areaMap.unwrappedAreaRectsByAreaId.contains(plot.getArea()))
			{
				//const auto [numBonuses, numUniqueBonuses] = countCityRadiusRevealedBonuses(coord, teamI);

				const auto it = mTeamCityCountCaseBitsByAreaId.find(plot.getArea()); 

				mStage2Data.setWithoutWrapMirroring(coord, static_cast<uint8_t>(
					(deadlockedBonusesTracker.getNumDeadlockedBonuses(coord, playerI) << FoundValueCalculationPlayerMapInterface::kStage2DataShift_cityRadiusDeadlockedBonusesCount)
					| (it != mTeamCityCountCaseBitsByAreaId.end() ? it->second * FoundValueCalculationPlayerMapInterface::kStage2DataMask_teamAreaCityCountCaseBit0 : 0)
					//| (numBonuses << FoundValueCalculationPlayerMapInterface::kStage2BonusDataShift_cityRadiusRevealedBonusCount)
					//| (numUniqueBonuses << FoundValueCalculationPlayerMapInterface::kStage2BonusDataShift_cityRadiusUniqueRevealedBonusCount)
				));
			}
		}
		});

	mFlags.setAllPadding();
	//mOurCultureMultipliers.setAllPadding();
	mStage2Data.setAllPadding();

	mCachedAICitySites.reserve(10);
	mPlayerAICitySitesDirty = true;

	mIsMaxDistanceFromCapitalDirty = true;

	//if (playerI == 17)
	//{
	//	const ivec2 coord = ivec2(map.getGridWidthINLINE(), 26) + ivec2(gGlobals.getCityPlotX()[17], gGlobals.getCityPlotY()[17]);
	//	assert(map.plot(coord.x, coord.y)->getBonusType(mTeamI) != NO_BONUS);
	//	assert(mFlags.get(coord) & FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasRevealedBonus);
	//	//assert(std::as_const(mFlags).raw(ivec2(2, 2) + ivec2(0, 26) + ivec2(gGlobals.getCityPlotX()[17], gGlobals.getCityPlotY()[17])) & FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasRevealedBonus);
	//
	//	__debugbreak();
	//}

}

void PlayerCache::onCanFoundChanged(const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker, ivec2 coord)
{
	auto bits = mFlags.get(coord);
	bits &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_canFound;
	bits |= deadlockedBonusesTracker.canFound(coord, mPlayerI) * FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_canFound;
	mFlags.set(coord, bits);
}
void PlayerCache::onDeadlockedBonusesChanged(const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker, ivec2 coord)
{
	// Value is always zero if area not in area map.
	if (mTeamCityCountCaseBitsByAreaId.contains(gGlobals.getMapINLINE().plot(coord.x, coord.y)->getArea()))
	{
		auto bits = mStage2Data.get(coord);
		bits &= ~FoundValueCalculationPlayerMapInterface::kStage2DataMask_cityRadiusDeadlockedBonusesCount;
		bits |= deadlockedBonusesTracker.getNumDeadlockedBonuses(coord, mPlayerI) << FoundValueCalculationPlayerMapInterface::kStage2DataShift_cityRadiusDeadlockedBonusesCount;
		mStage2Data.set(coord, bits);
	}
}

void PlayerCache::onPlotOwnerChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);
	auto flags = mFlags.get(coord);
	flags &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasForeignOwnerOrOutOfBounds;
	flags &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByThisPlayer;
	flags &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByOurTeam;
	const PlayerTypes owner = plot.getOwnerINLINE();
	if (owner != NO_PLAYER && owner != mPlayerI)
		flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasForeignOwnerOrOutOfBounds;
	if (owner == mPlayerI)
		flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByThisPlayer;
	if (plot.getTeam() == GET_PLAYER(mPlayerI).getTeam())
		flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isOwnedByOurTeam;
	mFlags.set(coord, flags);

	updateCultureMultiplier(plot);
}

void PlayerCache::onAfterCityCreated(const CvPlot& plot, PlayerTypes owner)
{
	mNearestCityCache.onAfterCityCreated(plot, owner);

	if (owner == mPlayerI)
		if (const CvCity* const capital = GET_PLAYER(mPlayerI).getCapitalCity())
		{
			//[[maybe_unused]] const int oldValue = mMaxDistanceFromCapital;
			mMaxDistanceFromCapital = std::max(mMaxDistanceFromCapital, mFlags.getMapGeometry().plotDistance(getPlotCoord(*capital->plot()), getPlotCoord(plot)));
			//if (const int updatedValue = calculateMaxDistanceFromCapital(); updatedValue != mMaxDistanceFromCapital)
			//	std::abort();
		}
	
	//if (const int updatedValue = calculateMaxDistanceFromCapital(); updatedValue != mMaxDistanceFromCapital)
	//	std::abort();

	//mIsMaxDistanceFromCapitalDirty = true;
}
void PlayerCache::onAfterCityDestroyed(const CvPlot& plot, PlayerTypes owner)
{
	mNearestCityCache.onAfterCityDestroyed(plot, owner);

	if (owner == mPlayerI)
		if (const CvCity* const capital = GET_PLAYER(mPlayerI).getCapitalCity())
			if (mFlags.getMapGeometry().plotDistance(getPlotCoord(*capital->plot()), getPlotCoord(plot)) >= mMaxDistanceFromCapital)
				mIsMaxDistanceFromCapitalDirty = true;
}

void PlayerCache::onCapitalChanged(const CvCity* to)
{
	mNearestCityCache.onCapitalChanged(to);
	mIsMaxDistanceFromCapitalDirty = true;
}

void PlayerCache::onRevealedBonusChanged(const CvPlot& plot)
{
	const ivec2 coord = getPlotCoord(plot);
	const TeamTypes teamI = GET_PLAYER(mPlayerI).getTeam();
	//for (const int cityPlotI : range(NUM_CITY_PLOTS))
	//{
	//	if (const CvPlot* const cityPlot = plotCity(coord.x, coord.y, cityPlotI))
	//	{
	//		const auto [numBonuses, numUniqueBonuses] = countCityRadiusRevealedBonuses(getPlotCoord(*cityPlot), teamI);
	//		auto bits = mStage2BonusData.get(coord);
	//		bits &= ~FoundValueCalculationPlayerMapInterface::kStage2BonusDataMask_cityRadiusRevealedBonusCount;
	//		bits &= ~FoundValueCalculationPlayerMapInterface::kStage2BonusDataMask_cityRadiusUniqueRevealedBonusCount;
	//		bits |= numBonuses << FoundValueCalculationPlayerMapInterface::kStage2BonusDataShift_cityRadiusRevealedBonusCount;
	//		bits |= numUniqueBonuses << FoundValueCalculationPlayerMapInterface::kStage2BonusDataShift_cityRadiusUniqueRevealedBonusCount;
	//		mFlags.set(coord, bits);
	//	}
	//}

	const bool isRevealed = plot.getBonusType(teamI) != NO_BONUS;
	auto flags = mFlags.get(coord);
	flags &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasRevealedBonus;
	if (isRevealed)
		flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasRevealedBonus;
	mFlags.set(coord, flags);
}

void PlayerCache::onPlayerAICitySitesChanged()
{
	mPlayerAICitySitesDirty = true;
}

void PlayerCache::onCultureChanged(const CvPlot& plot)
{
	updateCultureMultiplier(plot);
}

void PlayerCache::onRevealedChanged(const CvPlot& plot)
{
	updateCultureMultiplier(plot);
}

void PlayerCache::onAreaCityCountChanged(int from, int to)
{
	// kPlayerMapFlag_hasMyCitiesOnArea
	const bool check1 = (from == 0) != (to == 0);
	const bool check2 = updateTeamCityCountCaseBitsMap();
	if (check1 || check2)
	{
		CvMap& map = GC.getMapINLINE();
		heck::parallelForEachN(map.getGridHeightINLINE(), [&map, this](int y) {
			for (const int x : range(map.getGridWidthINLINE()))
			{
				const CvPlot& plot = *map.plot(x, y);
				const ivec2 coord{ x, y };

				{
					auto flags = mStage2Data.get(coord);
					flags &= ~FoundValueCalculationPlayerMapInterface::kStage2DataMask_teamAreaCityCountCaseBit0;
					flags &= ~FoundValueCalculationPlayerMapInterface::kStage2DataMask_teamAreaCityCountCaseBit1;
					if (const auto it = mTeamCityCountCaseBitsByAreaId.find(plot.getArea()); it != mTeamCityCountCaseBitsByAreaId.end())
						flags |= it->second * FoundValueCalculationPlayerMapInterface::kStage2DataMask_teamAreaCityCountCaseBit0;
					mStage2Data.setWithoutWrapMirroring(coord, flags);
				}

				{
					auto flags = mFlags.get(coord);
					flags &= ~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasMyCitiesOnArea;
					if (plot.area()->getCitiesPerPlayer(mPlayerI))
						flags |= FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasMyCitiesOnArea;
					mFlags.set(coord, flags);
				}
			}
			});

		if (check2)
			mStage2Data.setAllPadding();
	}
}


void PlayerCache::update()
{
	mNearestCityCache.update();

	const CvPlayerAI& player = GET_PLAYER(mPlayerI);

	if (mPlayerAICitySitesDirty)
	{
		mPlayerAICitySitesDirty = false;

		for (const ivec2 coord : mCachedAICitySites)
		{
			// Clear out the old bits.
			mFlags.fetchAndWithoutWrapMirroring(coord, static_cast<uint8_t>(~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isPlayerAIPlotCitySite), std::memory_order_relaxed);
			for (const int cityPlotI : range(NUM_CITY_PLOTS))
				if (const CvPlot* const cityPlot = plotCity(coord.x, coord.y, cityPlotI))
					mFlags.fetchAndWithoutWrapMirroring(getPlotCoord(*cityPlot), static_cast<uint8_t>(~FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isCityPlotWithinPlayerAICitySites), std::memory_order_relaxed);
		}

		mCachedAICitySites.clear();

		for (const int i : range(player.AI_getNumCitySites()))
		{
			const CvPlot* const plot = player.AI_getCitySite(i);
			const ivec2 coord = getPlotCoord(*plot);
			mCachedAICitySites.push_back(coord);
			mFlags.fetchOrWithoutWrapMirroring(coord, FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isPlayerAIPlotCitySite, std::memory_order_relaxed);
			for (const int cityPlotI : range(NUM_CITY_PLOTS))
				if (const CvPlot* const cityPlot = plotCity(coord.x, coord.y, cityPlotI))
					mFlags.fetchOrWithoutWrapMirroring(getPlotCoord(*cityPlot), FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_isCityPlotWithinPlayerAICitySites, std::memory_order_relaxed);
		}

		mFlags.setAllPadding();
	}

	if (mIsMaxDistanceFromCapitalDirty)
	{
		mIsMaxDistanceFromCapitalDirty = false;
		mMaxDistanceFromCapital = calculateMaxDistanceFromCapital();
	}

	for (const BonusTypes eBonus : range<BonusTypes>(GC.getNumBonusInfos()))
	{
		const int bonusVal = player.AI_bonusVal(eBonus);
		const bool value = player.getNumTradeableBonuses(eBonus) == 0 || bonusVal > 10 || GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0;
		mGoodBonusConditionFlags[eBonus / 16] &= ~(1 << (eBonus % 16));
		if (value)
			mGoodBonusConditionFlags[eBonus / 16] |= 1 << (eBonus % 16);

		mBonusValuationFirstSeen[eBonus] = static_cast<int16_t>(bonusVal * (player.getNumTradeableBonuses(eBonus) == 0 ? 80 : 20));
		mBonusValuationExtra[eBonus] = static_cast<int16_t>(bonusVal * 20);
	}

	mFlags.verify();
}

FoundValueCalculationPlayerMapInterface PlayerCache::createPlayerMapInterface(ivec2 foundValueCoord) const
{
	return {
		.addressingCoord = foundValueCoord + mFlags.getMapOffset(),
		.playerI = mPlayerI,
		.pitch = mFlags.getPitch(mFlags.getMapGeometry()),
		.flags = mFlags.getMemorySpan(),
		.revealedAndOurCultureMultipliers = mRevealedFlagAndOurCultureMultipliers.getMemorySpan(),
		.nearestCityDistanceValues = mNearestCityCache.getDistanceValueField().getMemorySpan(),
		.stage2Data = mStage2Data.getMemorySpan(),
	};
}

FoundValueCalculationPlayerGlobalDataInterface PlayerCache::createPlayerGlobalsInterface() const
{
	const CvPlayer& player = GET_PLAYER(mPlayerI);
	return {
		.isBarbarian = mPlayerI == BARBARIAN_PLAYER,
		.playerHasCapital = !!player.getCapitalCity(),
		.weHaveCityDistances = (mPlayerI == BARBARIAN_PLAYER ? GC.getGameINLINE().getNumCities() : player.getNumCities()) > 0,
		.playerNumCities = player.getNumCities(),
		.iGreed = mGreed,
		.iMaxDistanceFromCapital = mMaxDistanceFromCapital,
		
		// Per-player
		// if ((player.getNumTradeableBonuses(eBonus) == 0) || (player.AI_bonusVal(eBonus) > 10) || (GC.getBonusInfo(eBonus).getYieldChange(YIELD_FOOD) > 0))
		.playerGoodBonusConditionFlags{ mGoodBonusConditionFlags },

		// Allocate extra for out-of-bounds gather.
		.playerBonusValuationFirstSeen = mBonusValuationFirstSeen,
		.playerBonusValuationExtra = mBonusValuationExtra,
	};
}

void PlayerCache::verify(const AreaMap& areaMap, const FoundValueDeadlockedBonusesTracker& deadlockedBonusesTracker) const
{
	if (mPlayerAICitySitesDirty)
		std::abort();

	PlayerCache b(mFlags.getMapGeometry(), areaMap, mPlayerI, deadlockedBonusesTracker);
	b.update();

	if (mClaimThreshold != b.mClaimThreshold)
		std::abort();
	if (mFlags != b.mFlags)
	{
		const ivec2 coord = mFlags.findFirstMismatch(b.mFlags).value();
		std::clog << "Mismatch at " << coord << '\n';
		using Bitset = std::bitset<sizeof(typename decltype(mFlags)::value_type) * CHAR_BIT>;
		std::clog << "Cached: " << Bitset(mFlags.get(coord)) << '\n';
		std::clog << "Live  : " << Bitset(b.mFlags.get(coord)) << '\n';
		std::clog << std::flush;
		//if ((mFlags.get(coord) ^ b.mFlags.get(coord)) & FoundValueCalculationPlayerMapInterface::kPlayerMapFlag_hasMyCitiesOnArea
		std::abort();
	}
	if (mRevealedFlagAndOurCultureMultipliers != b.mRevealedFlagAndOurCultureMultipliers)
		std::abort();
	mNearestCityCache.verify();
	if (mStage2Data != b.mStage2Data)
	{
		const ivec2 coord = mStage2Data.findFirstMismatch(b.mStage2Data).value();
		std::clog << "Mismatch at " << coord << '\n';
		using Bitset = std::bitset<sizeof(typename decltype(mStage2Data)::value_type) * CHAR_BIT>;
		std::clog << "Cached: " << Bitset(mStage2Data.get(coord)) << '\n';
		std::clog << "Live  : " << Bitset(b.mStage2Data.get(coord)) << '\n';
		std::clog << std::flush;
		std::abort();
	}
	if (mCachedAICitySites != b.mCachedAICitySites)
		std::abort();
	if (mTeamCityCountCaseBitsByAreaId != b.mTeamCityCountCaseBitsByAreaId)
		std::abort();
	if (mMaxDistanceFromCapital != b.mMaxDistanceFromCapital)
	{
		std::clog << "mMaxDistanceFromCapital is wrong. cached = " << mMaxDistanceFromCapital << ", live = " << b.mMaxDistanceFromCapital << std::endl;
		std::abort();
	}
}

void PlayerCache::updateCultureMultiplier(const CvPlot& plot)
{
	int cultureMultiplier{};
	if (!plot.isOwned() || plot.getOwnerINLINE() == mPlayerI)
	{
		cultureMultiplier = 100;
	}
	else
	{
		const int iOurCulture = plot.getCulture(mPlayerI);
		const int iOtherCulture = std::max(1, plot.getCulture(plot.getOwnerINLINE()));
		cultureMultiplier = 100 * (iOurCulture + mClaimThreshold);
		cultureMultiplier /= iOtherCulture + mClaimThreshold;
		cultureMultiplier = std::min(100, cultureMultiplier);
	}
	mRevealedFlagAndOurCultureMultipliers.set(getPlotCoord(plot), uint8_t(cultureMultiplier | (plot.isRevealed(mTeamI, false) ? 0x80 : 0)));

	//if (ivec2(getPlotCoord(plot)) == ivec2(723, 85) + ivec2(1, 2))
	//	std::cout << "XXX: " << cultureMultiplier << std::endl;
}

bool PlayerCache::updateTeamCityCountCaseBitsMap()
{
	const CvTeamAI& team = GET_TEAM(GET_PLAYER(mPlayerI).getTeam());
	CvMap& map = GC.getMapINLINE();
	bool changed = false;
	for (auto&& [areaId, bits] : mTeamCityCountCaseBitsByAreaId)
	{
		const CvArea* const area = map.getArea(areaId);

		const int iTeamAreaCities = team.countNumCitiesByArea(area);

		unsigned int newBits = 0;

		if (area->getNumCities() == iTeamAreaCities)
			newBits = 0b00;
		else if (area->getNumCities() == iTeamAreaCities + GET_TEAM(BARBARIAN_TEAM).countNumCitiesByArea(area))
			newBits = 0b01;
		else if (iTeamAreaCities > 0)
			newBits = 0b10;
		else
			newBits = 0b11;

		changed |= std::exchange(bits, newBits) != newBits;
	}
	return changed;
}

int PlayerCache::calculateMaxDistanceFromCapital() const
{
	int d = 0;

	if (const CvCity* const capital = GET_PLAYER(mPlayerI).getCapitalCity())
	{
		const CivMapGeometry mapGeom = mFlags.getMapGeometry();
		const ivec2 capitalCoord = getPlotCoord(*capital->plot());
		for (const ivec2 coord : gatherCityLocations(mPlayerI))
			d = std::max(d, mapGeom.plotDistance(capitalCoord, coord));
	}

	return d;
}

#endif