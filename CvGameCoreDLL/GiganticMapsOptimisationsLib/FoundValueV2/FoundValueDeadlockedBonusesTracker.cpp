#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueDeadlockedBonusesTracker.h"

#include "../../CvInfos.h"
#include "../../CvPlayerAI.h"

#include <CommonStuff/ParallelExt.h>

#include <chrono>
#include <iostream>
#include <syncstream>

using namespace GiganticMapsOptimisationsLib;
using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

using Clock = std::chrono::steady_clock;

// Bitset version of CvPlayer::canFound.
static std::bitset<MAX_PLAYERS> calcCanFoundBits(const CvMap& map, ivec2 coord)
{
	CvPlot* pPlot;
	CvPlot* pLoopPlot;
	bool bValid;
	int iRange;
	int iDX, iDY;

	pPlot = map.plotSorenINLINE(coord.x, coord.y);

	// OCC handled by caller.

	if (pPlot->isImpassable())
	{
		return {};
	}

	if (pPlot->getFeatureType() != NO_FEATURE)
	{
		if (GC.getFeatureInfo(pPlot->getFeatureType()).isNoCity())
		{
			return {};
		}
	}

	std::bitset<MAX_PLAYERS> mask = ~std::bitset<MAX_PLAYERS>();

	if (pPlot->isOwned())
	{
		// Only the owner can found.
		mask = {};
		mask[pPlot->getOwnerINLINE()] = true;
	}

	bValid = false;

	if (!bValid)
	{
		if (GC.getTerrainInfo(pPlot->getTerrainType()).isFound())
		{
			bValid = true;
		}
	}

	if (!bValid)
	{
		if (GC.getTerrainInfo(pPlot->getTerrainType()).isFoundCoast())
		{
			if (pPlot->isCoastalLand())
			{
				bValid = true;
			}
		}
	}

	if (!bValid)
	{
		if (GC.getTerrainInfo(pPlot->getTerrainType()).isFoundFreshWater())
		{
			if (pPlot->isFreshWater())
			{
				bValid = true;
			}
		}
	}

	if (pPlot->isWater())
	{
		return {};
	}

	if (!bValid)
	{
		return {};
	}


	iRange = GC.getMIN_CITY_RANGE();

	for (iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (iDY = -(iRange); iDY <= iRange; iDY++)
		{
			pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

			if (pLoopPlot != nullptr)
			{
				if (pLoopPlot->isCity())
				{
					if (pLoopPlot->area() == pPlot->area())
					{
						return {};
					}
				}
			}
		}
	}

	return mask;
}

static std::bitset<MAX_PLAYERS> calcHasRevealedBonusBits(const CvMap& map, ivec2 coord)
{
	std::bitset<MAX_PLAYERS> mask{};

	const CvPlot& plot = *map.plot(coord.x, coord.y);
	
	if (plot.getBonusType() != NO_BONUS)
		for (const auto i : range<PlayerTypes>(MAX_PLAYERS))
			mask[i] = plot.getBonusType(GET_PLAYER(i).getTeam()) != NO_BONUS;

	return mask;
}

// CvPlayerAI::AI_countDeadlockedBonuses
std::array<uint8_t, MAX_PLAYERS> FoundValueDeadlockedBonusesTracker::AI_countDeadlockedBonuses(ivec2 coord) const
{
	CvPlot* pLoopPlot;
	CvPlot* pLoopPlot2;
	int iDX, iDY;
	int iI;

	int iMinRange = GC.getMIN_CITY_RANGE();
	int iRange = iMinRange * 2;

	const CvPlot& pPlot = *GC.getMapINLINE().plot(coord.x, coord.y);

	std::array<uint8_t, MAX_PLAYERS> counts{};

	for (iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (iDY = -(iRange); iDY <= iRange; iDY++)
		{
			if (::plotDistance(iDX, iDY, 0, 0) > CITY_PLOTS_RADIUS)
			{
				pLoopPlot = plotXY(coord.x, coord.y, iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					const std::bitset<MAX_PLAYERS> bonusMask = mPlotPropsMap[getPlotCoord(*pLoopPlot)].hasRevealedBonus;
					//if (pLoopPlot->getBonusType(getTeam()) != NO_BONUS)
					if (bonusMask.any())
					{
						if (!pLoopPlot->isCityRadius() && ((pLoopPlot->area() == pPlot.area()) || pLoopPlot->isWater()))
						{
							std::bitset<MAX_PLAYERS> bCanFound{};
							std::bitset<MAX_PLAYERS> bNeverFound = ~std::bitset<MAX_PLAYERS>();
							//potentially blockable resource
							//look for a city site within a city radius
							for (iI = 0; iI < NUM_CITY_PLOTS; iI++)
							{
								pLoopPlot2 = plotCity(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), iI);
								if (pLoopPlot2 != nullptr)
								{
									//canFound usually returns very quickly
									//if (mPlotPropsMap[getPlotCoord(*pLoopPlot2)].canFound.any())
									const std::bitset<MAX_PLAYERS> thisCanFound = mPlotPropsMap[getPlotCoord(*pLoopPlot2)].canFound;
									{
										bNeverFound &= ~thisCanFound;
										if (stepDistance(coord.x, coord.y, pLoopPlot2->getX_INLINE(), pLoopPlot2->getY_INLINE()) > iMinRange)
											bCanFound |= thisCanFound;
									}
								}
							}

							const std::bitset<MAX_PLAYERS> deadlocked = ~bNeverFound & ~bCanFound & bonusMask;

							// This way, it doesn't matter if `to_ulong` truncates bits when MAX_PLAYERS > 32.
							if (deadlocked.any())
								for (const auto playerI : range<PlayerTypes>(std::countr_zero(deadlocked.to_ulong()), MAX_PLAYERS))
									counts[playerI] += deadlocked[playerI];
						}
					}
				}
			}
		}
	}

	return counts;
}

FoundValueDeadlockedBonusesTracker::FoundValueDeadlockedBonusesTracker(CivMapGeometry mapGeom)
	: mMapGeom(mapGeom)
	, mPlotPropsMap(mapGeom.dim)
	, mDeadlockedCountsMap(mapGeom.dim)
{
	const auto t0 = Clock::now();

	const CvMap& map = GC.getMapINLINE();

	if (GC.getUSE_CANNOT_FOUND_CITY_CALLBACK() || GC.getUSE_CAN_FOUND_CITIES_ON_WATER_CALLBACK())
		std::abort();

	std::bitset<MAX_PLAYERS> occMask = ~std::bitset<MAX_PLAYERS>();

	if (GC.getGameINLINE().isFinalInitialized() && GC.getGameINLINE().isOption(GAMEOPTION_ONE_CITY_CHALLENGE))
		for (const auto playerI : range<PlayerTypes>(MAX_PLAYERS))
			if (const CvPlayerAI& player = GET_PLAYER(playerI); player.isHuman() && player.getNumCities() > 0)
				occMask[playerI] = false;

	heck::parallelForEachN(mapGeom.dim.y, [&, this, width = mPlotPropsMap.dim.x](const int y) {
		for (const int x : range(width))
		{
			mPlotPropsMap[{ x, y }] = {
				.canFound = calcCanFoundBits(map, { x, y }) & occMask,
				.hasRevealedBonus = calcHasRevealedBonusBits(map, { x, y }),
			};
		}
		});

	heck::parallelForEachN(mapGeom.dim.y, [&, this, width = mPlotPropsMap.dim.x](const int y) {
		for (const int x : range(width))
			mDeadlockedCountsMap[{ x, y }] = AI_countDeadlockedBonuses({ x, y });
		});

	const auto t1 = Clock::now();

	std::clog << "FoundValueDeadlockedBonusesTracker initialised in " << std::chrono::duration<double, std::milli>(t1 - t0) << std::endl;
}

void FoundValueDeadlockedBonusesTracker::onPlotOwnerChanged(ivec2 coord)
{
	//updateCanFound(coord);
	onAfterCityCreatedOrDestroyed(coord);
}
void FoundValueDeadlockedBonusesTracker::onAfterCityCreatedOrDestroyed(ivec2 coord)
{
	{
		// Follow the logic at the end of CvPlayer::canFound...
		const CvPlot* const pPlot = GC.getMapINLINE().plot(coord.x, coord.y);

		const int iRange = GC.getMIN_CITY_RANGE();

		for (int iDX = -(iRange); iDX <= iRange; iDX++)
		{
			for (int iDY = -(iRange); iDY <= iRange; iDY++)
			{
				const CvPlot* const pLoopPlot = plotXY(pPlot->getX_INLINE(), pPlot->getY_INLINE(), iDX, iDY);

				if (pLoopPlot != nullptr)
				{
					if (pLoopPlot->area() == pPlot->area())
					{
						updateCanFound(getPlotCoord(*pLoopPlot));
					}
				}
			}
		}
	}

	{
		// `if (!pLoopPlot->isCityRadius()` check in `AI_countDeadlockedBonuses`.
		int iDX, iDY;

		int iMinRange = GC.getMIN_CITY_RANGE();
		int iRange = iMinRange * 2 + CITY_PLOTS_RADIUS; // Add extra, as this affects city radius.

		for (iDX = -(iRange); iDX <= iRange; iDX++)
		{
			for (iDY = -(iRange); iDY <= iRange; iDY++)
			{
				//if (::plotDistance(iDX, iDY, 0, 0) > CITY_PLOTS_RADIUS)
				{
					updateDeadlockedBonuses(coord + ivec2(iDX, iDY));
				}
			}
		}
	}
}
void FoundValueDeadlockedBonusesTracker::onVisibleHasBonusChanged(ivec2 coord)
{
	mPlotPropsMap[coord].hasRevealedBonus = calcHasRevealedBonusBits(GC.getMapINLINE(), coord);

	int iDX, iDY;

	int iMinRange = GC.getMIN_CITY_RANGE();
	int iRange = iMinRange * 2;

	for (iDX = -(iRange); iDX <= iRange; iDX++)
	{
		for (iDY = -(iRange); iDY <= iRange; iDY++)
		{
			//if (::plotDistance(iDX, iDY, 0, 0) > CITY_PLOTS_RADIUS)
			{
				updateDeadlockedBonuses(coord + ivec2(iDX, iDY));
			}
		}
	}
}

std::span<const i16vec2> FoundValueDeadlockedBonusesTracker::getCanFoundDirtyList() const
{
	return mCanFoundDirtyList;
}
std::span<const i16vec2> FoundValueDeadlockedBonusesTracker::getDeadlockedBonusCountDirtyList() const
{
	return mDeadlockedBonusCountDirtyList;
}
void FoundValueDeadlockedBonusesTracker::clearDirtyLists()
{
	mCanFoundDirtyList.clear();
	mDeadlockedBonusCountDirtyList.clear();
}

bool FoundValueDeadlockedBonusesTracker::canFound(ivec2 coord, PlayerTypes playerI) const
{
	return mPlotPropsMap[coord].canFound[playerI];
}

int FoundValueDeadlockedBonusesTracker::getNumDeadlockedBonuses(ivec2 coord, PlayerTypes playerI) const
{
	return mDeadlockedCountsMap[coord][playerI];
}

void FoundValueDeadlockedBonusesTracker::verify() const
{
	if (mCanFoundDirtyList.size() || mDeadlockedBonusCountDirtyList.size())
		std::clog << "Somebody didn't clear the dirty lists." << std::endl;

	std::atomic_bool failed{ false };

	heck::parallelForEachN(mPlotPropsMap.dim.y, [&failed, &map = GC.getMapINLINE(), this](int y) {
		for (const int x : range(mPlotPropsMap.dim.x))
		{
			for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
			{
				const bool liveValue = GET_PLAYER(playerI).canFound(x, y);
				if (mPlotPropsMap[{ x, y }].canFound[playerI] != liveValue)
				{
					failed.store(true);
					std::osyncstream(std::clog) << "canFound mismatch at plot " << x << ", " << y << " for player " << (int)playerI << std::endl;
					return;
				}
			}

			if (mPlotPropsMap[{ x, y }].hasRevealedBonus != calcHasRevealedBonusBits(map, { x, y }))
			{
				failed.store(true);
				std::osyncstream(std::clog) << "hasRevealedBonus mismatch at plot " << x << ", " << y
					<< ": cached = " << mPlotPropsMap[{ x, y }].hasRevealedBonus  << ", live = " << calcHasRevealedBonusBits(map, { x, y })
					<< ", bonus = " << +map.plotINLINE(x, y)->getBonusType() << std::endl;
				return;
			}
		}
		});

	if (!failed.load())
	{
		heck::parallelForEachN(mPlotPropsMap.dim.y, [&failed, &map = GC.getMapINLINE(), this](int y) {
			for (const int x : range(mPlotPropsMap.dim.x))
				for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
				{
					const int liveValue = GET_PLAYER(playerI).AI_countDeadlockedBonuses(map.plot(x, y));
					if (getNumDeadlockedBonuses({ x, y }, playerI) != liveValue)
					{
						failed.store(true);
						std::osyncstream(std::clog) << "Deadlocked bonuses count mismatch at plot " << x << ", " << y << " for player " << (int)playerI << std::endl;
						if (AI_countDeadlockedBonuses({ x, y })[playerI] != liveValue)
							std::osyncstream(std::clog) << "Internal evaluation function also mismatches live value!" << std::endl;
						return;
					}
				}
			});
	}

	if (failed.load())
		std::abort();
}

void FoundValueDeadlockedBonusesTracker::updateCanFound(ivec2 coord)
{
	const auto newCanFoundBits = calcCanFoundBits(GC.getMapINLINE(), coord);
	if (mPlotPropsMap[coord].canFound != newCanFoundBits)
	{
		mPlotPropsMap[coord].canFound = newCanFoundBits;
		mCanFoundDirtyList.push_back(coord);
	}
}

void FoundValueDeadlockedBonusesTracker::updateDeadlockedBonuses(ivec2 coord)
{
	if (const CvPlot* const plot = GC.getMapINLINE().plot(coord.x, coord.y))
	{
		coord = getPlotCoord(*plot);
		const auto newCounts = AI_countDeadlockedBonuses(coord);
		if (mDeadlockedCountsMap[coord] != newCounts)
		{
			mDeadlockedCountsMap[coord] = newCounts;
			mDeadlockedBonusCountDirtyList.push_back(coord);
		}
	}
}

#endif