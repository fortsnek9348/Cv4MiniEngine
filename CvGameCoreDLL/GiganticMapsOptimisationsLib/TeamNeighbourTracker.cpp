#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "TeamNeighbourTracker.h"
#include "Util.h"

#include "../CvPlot.h"
#include "../CvPlayerAI.h"
#include "../CvGameCoreUtils.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>

using namespace GiganticMapsOptimisationsLib;

// Note that verification is no slower than vanilla because vanilla recalculated this from scratch anyway.
static constexpr bool kEnableVerification = false;

// Vanilla code from CvTeamAI.
[[maybe_unused]] static int AI_calculateAdjacentLandPlots(TeamTypes plotTeamI, TeamTypes adjTeamI)
{
	int iCount{};
	//for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	heck::parallelForEachN(GC.getMapINLINE().numPlotsINLINE(), [&](int iI) {
		CvPlot* pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (!(pLoopPlot->isWater()))
		{
			if ((pLoopPlot->getTeam() == plotTeamI) && pLoopPlot->isAdjacentTeam(adjTeamI, true))
			{
				std::atomic_ref(iCount).fetch_add(1, std::memory_order_relaxed);
			}
		}
		});
	return iCount;
}

// For when you really need a sanity check.
//static DynamicArray2D<int8_t> gOwnerMap;

TeamNeighbourTracker::TeamNeighbourTracker()
{
	const CvMap& map = GC.getMapINLINE();

	//gOwnerMap = DynamicArray2D<int8_t>{ CivMapGeometry(map).dim, -1 };

	for (const int plotI : range(map.numPlotsINLINE()))
	{
		const CvPlot* const pLoopPlot = map.plotByIndexINLINE(plotI);

		const TeamTypes plotTeamI = pLoopPlot->getTeam();

		if (!pLoopPlot->isWater() && plotTeamI != NO_TEAM)
		{
			//gOwnerMap[getPlotCoord(*pLoopPlot)] = (int8_t)plotTeamI;
			std::bitset<MAX_TEAMS> adjTeams{};
			for (const DirectionTypes dirI : range(NUM_DIRECTION_TYPES))
			{
				if (CvPlot* const pAdjacentPlot = plotDirection(pLoopPlot->getX_INLINE(), pLoopPlot->getY_INLINE(), dirI); pAdjacentPlot && !pAdjacentPlot->isWater())
				{
					if (const TeamTypes adjTeamI = pAdjacentPlot->getTeam(); adjTeamI != NO_TEAM)
					{
						if (!adjTeams[adjTeamI])
						{
							adjTeams[adjTeamI] = true;
							++mAdjacencyCounts[plotTeamI][adjTeamI];
						}
					}
				}
			}
		}
	}

	// Complete verification.
	if constexpr (kEnableVerification)
		for (const TeamTypes plotTeamI : range<TeamTypes>(MAX_TEAMS))
			for (const TeamTypes adjTeamI : range<TeamTypes>(MAX_TEAMS))
				(void)getAdjacentLandPlotsCount(plotTeamI, adjTeamI);
}

// mAdjacencyCounts[plotTeamI][adjTeamI]
// Adjacency is counted by whether a team is adjacent to a plot or not (boolean check). Not by the number of edges between plots.
// So to update, you need to check the 5x5 region around the plot.

void TeamNeighbourTracker::onPlotOwnerChanged(const CvPlot& plot, TeamTypes oldOwner)
{
	if (plot.isWater())
		return;

	const CvMap& map = gGlobals.getMapINLINE();
	const TeamTypes newOwner = plot.getTeam();

	if (oldOwner == newOwner)
		return;

	Array2D<TeamTypes, 5> localGrid(NO_TEAM);

	static constexpr iaabb2 kRelRect{ .min = -2, .max = 3 };
	static constexpr ivec2 kCenterLocal(2, 2);
	const ivec2 centerCoord = getPlotCoord(plot);


	//std::clog << "onPlotOwnerChanged " << centerCoord << ' ' << (int)oldOwner << ' ' << (int)newOwner << std::endl;
	//
	//if (centerCoord == ivec2(68, 27) && (int)oldOwner == 3 && (int)newOwner == 5)
	//	_CrtDbgBreak();
	//
	//if (gOwnerMap[centerCoord] != oldOwner)
	//	std::abort();
	//gOwnerMap[centerCoord] = (int8_t)newOwner;
	//
	//const ivec2 mapDim = gOwnerMap.dim;
	//for (int y = 0; y < mapDim.y; ++y)
	//	for (int x = 0; x < mapDim.x; ++x)
	//	{
	//		const CvPlot* const mapPlot = map.plotSorenINLINE(x, y);
	//		if (gOwnerMap[{ x, y }] != (mapPlot && !mapPlot->isWater() ? mapPlot->getTeam() : NO_TEAM))
	//			std::abort();
	//	}

	for (int dy = kRelRect.min.y; dy < kRelRect.max.y; ++dy)
		for (int dx = kRelRect.min.x; dx < kRelRect.max.x; ++dx)
			if (const CvPlot* const localPlot = map.plotINLINE(centerCoord.x + dx, centerCoord.y + dy); localPlot && !localPlot->isWater())
				localGrid[ivec2(dx, dy) - kRelRect.min] = localPlot->getTeam();

	[[maybe_unused]] const auto getAdjCountMaskAt = [&localGrid](ivec2 localCoord) {
		std::bitset<MAX_TEAMS> mask{};
	
		for (int dy = -1; dy <= +1; ++dy)
			for (int dx = -1; dx <= +1; ++dx)
				if (dx || dy)
					if (const TeamTypes teamI = localGrid[localCoord + ivec2(dx, dy)]; teamI != NO_TEAM)
						mask[teamI] = true;
	
		return mask;
		};

	const auto hasAdjTeam = [&localGrid](ivec2 localCoord, TeamTypes adjTeamI) {
		for (int dy = -1; dy <= +1; ++dy)
			for (int dx = -1; dx <= +1; ++dx)
				if (dx || dy)
					if (localGrid[localCoord + ivec2(dx, dy)] == adjTeamI)
						return true;
		return false;
		};

	[[maybe_unused]] const auto getAdjCounts = [&localGrid, &hasAdjTeam](TeamTypes adjTeamI) {
		std::array<uint8_t, MAX_TEAMS> counts{};
		for (int dy = -1; dy <= +1; ++dy)
			for (int dx = -1; dx <= +1; ++dx)
				//if (dx || dy)
				{
					const ivec2 plotLocalCoord = kCenterLocal + ivec2(dx, dy);
					if (const TeamTypes plotTeamI = localGrid[plotLocalCoord]; plotTeamI != NO_TEAM)
						if (hasAdjTeam(plotLocalCoord, adjTeamI))
							++counts[plotTeamI];
				}
		return counts;
		};

	// Update [*][old/new].
	for (const TeamTypes adjTeamI : { oldOwner, newOwner })
	{
		if (adjTeamI != NO_TEAM)
		{
			localGrid[kCenterLocal] = oldOwner;
			const std::array<uint8_t, MAX_TEAMS> oldCounts = getAdjCounts(adjTeamI);
			localGrid[kCenterLocal] = newOwner;
			const std::array<uint8_t, MAX_TEAMS> newCounts = getAdjCounts(adjTeamI);
			for (int teamI = 0; teamI < MAX_TEAMS; ++teamI)
				mAdjacencyCounts[teamI][adjTeamI] += newCounts[teamI] - oldCounts[teamI];
		}
	}
	
	// Update [old/new][* != old/new].
	{
		const std::bitset<MAX_TEAMS> adjMask = getAdjCountMaskAt(kCenterLocal);
		if (oldOwner != NO_TEAM)
			for (int teamI = 0; teamI < MAX_TEAMS; ++teamI)
				if (teamI != oldOwner && teamI != newOwner) // Don't double count
					mAdjacencyCounts[oldOwner][teamI] -= adjMask[teamI];
		if (newOwner != NO_TEAM)
			for (int teamI = 0; teamI < MAX_TEAMS; ++teamI)
				if (teamI != oldOwner && teamI != newOwner) // Don't double count
					mAdjacencyCounts[newOwner][teamI] += adjMask[teamI];
	}

	// Exhaustive way of doing it.
	//for (int teamI = 0; teamI < MAX_TEAMS; ++teamI)
	//{
	//	for (int teamJ = 0; teamJ < MAX_TEAMS; ++teamJ)
	//	{
	//		localGrid[kCenterLocal] = oldOwner;
	//		int count1 = 0;
	//		for (int dy = -1; dy <= +1; ++dy)
	//		{
	//			for (int dx = -1; dx <= +1; ++dx)
	//			{
	//				const ivec2 plotLocalCoord = kCenterLocal + ivec2(dx, dy);
	//				if (const TeamTypes plotTeamI = localGrid[plotLocalCoord]; plotTeamI == teamI)
	//					if (hasAdjTeam(plotLocalCoord, (TeamTypes)teamJ))
	//						++count1;
	//			}
	//		}
	//
	//		localGrid[kCenterLocal] = newOwner;
	//		int count2 = 0;
	//		for (int dy = -1; dy <= +1; ++dy)
	//		{
	//			for (int dx = -1; dx <= +1; ++dx)
	//			{
	//				const ivec2 plotLocalCoord = kCenterLocal + ivec2(dx, dy);
	//				if (const TeamTypes plotTeamI = localGrid[plotLocalCoord]; plotTeamI == teamI)
	//					if (hasAdjTeam(plotLocalCoord, (TeamTypes)teamJ))
	//						++count2;
	//			}
	//		}
	//
	//		mAdjacencyCounts[teamI][teamJ] += count2 - count1;
	//
	//		//const int expectedCount = AI_calculateAdjacentLandPlots((TeamTypes)teamI, (TeamTypes)teamJ);
	//		//if (expectedCount != mAdjacencyCounts[teamI][teamJ])
	//		//	throw std::runtime_error("TeamNeighbourTracker verification error.");
	//	}
	//}

	//std::clog << "XXX: " << AI_calculateAdjacentLandPlots((TeamTypes)3, (TeamTypes)4) << std::endl;
}

int TeamNeighbourTracker::getAdjacentLandPlotsCount(TeamTypes plotTeamI, TeamTypes adjTeamI) const
{
	const int count = mAdjacencyCounts[plotTeamI][adjTeamI];
	if constexpr (kEnableVerification)
		if (AI_calculateAdjacentLandPlots(plotTeamI, adjTeamI) != count)
			throw std::runtime_error("TeamNeighbourTracker verification error.");
	return count;
}

///

// Vanilla code from CvPlayerAI.
static int AI_calculateStolenCityRadiusPlots(PlayerTypes thisPlayerI, PlayerTypes neighbourPlayerI)
{
	int iCount;
	int iI;
	iCount = 0;
	for (iI = 0; iI < GC.getMapINLINE().numPlotsINLINE(); iI++)
	{
		const CvPlot* const pLoopPlot = GC.getMapINLINE().plotByIndexINLINE(iI);

		if (pLoopPlot->getOwnerINLINE() == neighbourPlayerI)
		{
			if (pLoopPlot->isPlayerCityRadius(thisPlayerI))
			{
				++iCount;
			}
		}
	}
	return iCount;
}

// Simple to track. Depends only on city position and plot owners. Watch out for overlapping city radius though.
StolenCityRadiusPlotsTracker::StolenCityRadiusPlotsTracker()
{
	const CvMap& map = GC.getMapINLINE();


	for (const PlayerTypes thisPlayerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		// Count plots only once per city owner.
		std::vector<bool> visitSet(map.numPlotsINLINE());

		const CvPlayerAI& player = GET_PLAYER(thisPlayerI);
		int itIndex = 0;
		for (const CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
		{
			for (const int cityPlotI : range(NUM_CITY_PLOTS))
			{
				if (const CvPlot* const pLoopPlot = plotCity(city->getX_INLINE(), city->getY_INLINE(), cityPlotI))
				{
					const int plotI = map.plotNumINLINE(pLoopPlot);
					if (!visitSet[plotI])
					{
						visitSet[plotI] = true;
						if (const PlayerTypes neighbourPlayerI = pLoopPlot->getOwnerINLINE(); neighbourPlayerI != NO_PLAYER)
							++mCounts[thisPlayerI][neighbourPlayerI];
					}
				}
			}
		}
	}

	if constexpr (kEnableVerification)
		verify();
}

void StolenCityRadiusPlotsTracker::verify() const
{
	for (const PlayerTypes thisPlayerI : range<PlayerTypes>(MAX_PLAYERS))
		for (const PlayerTypes neighbourPlayerI : range<PlayerTypes>(MAX_PLAYERS))
			if (const int liveCount = AI_calculateStolenCityRadiusPlots(thisPlayerI, neighbourPlayerI); liveCount != mCounts[thisPlayerI][neighbourPlayerI])
				throw std::runtime_error("StolenCityRadiusPlotsTracker initial verification failed.");
}

void StolenCityRadiusPlotsTracker::onPlotOwnerChanged(const CvPlot& plot, PlayerTypes oldOwner)
{
	const PlayerTypes newOwner = plot.getOwnerINLINE();

	// Check whether this plot *was* stolen from somebody.
	// Can simply enumerate the city plots around the changed plot. This will get all the cities in range.
	//std::bitset<MAX_PLAYERS> visitSet{};
	//for (const int cityPlotI : range(NUM_CITY_PLOTS))
	//{
	//	if (const CvPlot* const pLoopPlot = plotCity(plot.getX_INLINE(), plot.getY_INLINE(), cityPlotI); pLoopPlot && pLoopPlot->isCity())
	//	{
	//		const PlayerTypes cityOwnerPlayerI = pLoopPlot->getOwnerINLINE();
	//		if (!visitSet[cityOwnerPlayerI])
	//		{
	//			visitSet[cityOwnerPlayerI] = true;
	//			if (oldOwner != NO_PLAYER /*&& cityOwnerPlayerI != oldOwner*/)
	//				--mCounts[cityOwnerPlayerI][oldOwner];
	//			if (newOwner != NO_PLAYER /*&& cityOwnerPlayerI != newOwner*/)
	//				++mCounts[cityOwnerPlayerI][newOwner];
	//		}
	//	}
	//}

	for (const auto thisPlayerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (plot.isPlayerCityRadius(thisPlayerI))
		{
			for (const auto neighbourPlayerI :{ oldOwner, newOwner })
			{
				if (neighbourPlayerI != NO_PLAYER)
				{
					const bool wasCounted = oldOwner == neighbourPlayerI;
					const bool nowCounted = newOwner == neighbourPlayerI;
					mCounts[thisPlayerI][neighbourPlayerI] += nowCounted - wasCounted;
				}
			}
		}
	}

	if constexpr (kEnableVerification)
		verify();
}

void StolenCityRadiusPlotsTracker::onChangePlayerCityRadiusCount(const CvPlot& plot, PlayerTypes playerI, int from, int to)
{
	if (const PlayerTypes owner = plot.getOwnerINLINE(); owner != NO_PLAYER)
		mCounts[playerI][owner] += bool(to) - bool(from);

	if constexpr (kEnableVerification)
		verify();
}

/*static int countOwnedCitiesInRadius(PlayerTypes cityOwnerPlayerI, const CvPlot& stolenPlot)
{
	int count = 0;
	for (const int cityPlotI : range(NUM_CITY_PLOTS))
		if (const CvPlot* const pLoopPlot = plotCity(stolenPlot.getX_INLINE(), stolenPlot.getY_INLINE(), cityPlotI))
			if (pLoopPlot->isCity() && pLoopPlot->getOwnerINLINE() == cityOwnerPlayerI)
				++count;
	return count;
}

void StolenCityRadiusPlotsTracker::onCityCreate(const CvCity& city)
{
	assert(city.plot()->isCity()); // sanity check

	const PlayerTypes cityOwnerPlayerI = city.getOwnerINLINE();
	for (const int cityPlotI : range(NUM_CITY_PLOTS))
	{
		if (const CvPlot* const pLoopPlot = plotCity(city.getX_INLINE(), city.getY_INLINE(), cityPlotI))
		{
			if (const PlayerTypes neighbourPlayerI = pLoopPlot->getOwnerINLINE(); neighbourPlayerI != NO_PLAYER)
			{
				// Now, because plots are only counted once per city owner rather than once per city,
				// we now need to check whether this plot has already been counted by another city.
				// Check whether there are any other cities in the radius of this plot that have the same owner.
				if (countOwnedCitiesInRadius(cityOwnerPlayerI, *pLoopPlot) == 1)
					++mCounts[cityOwnerPlayerI][neighbourPlayerI];
			}
		}
	}
}

void StolenCityRadiusPlotsTracker::onCityDestroy(const CvCity& city)
{
	const PlayerTypes cityOwnerPlayerI = city.getOwnerINLINE();
	for (const int cityPlotI : range(NUM_CITY_PLOTS))
		if (const CvPlot* const pLoopPlot = plotCity(city.getX_INLINE(), city.getY_INLINE(), cityPlotI))
			if (const PlayerTypes neighbourPlayerI = pLoopPlot->getOwnerINLINE(); neighbourPlayerI != NO_PLAYER)
				if (countOwnedCitiesInRadius(cityOwnerPlayerI, *pLoopPlot) == 1) // If this is the last city.
					--mCounts[cityOwnerPlayerI][neighbourPlayerI];
}*/

int StolenCityRadiusPlotsTracker::getStolenCityRadiusPlots(PlayerTypes playerI, PlayerTypes neighbourPlayerI) const
{
	const int count = mCounts[playerI][neighbourPlayerI];
	if constexpr(kEnableVerification)
		if (AI_calculateStolenCityRadiusPlots(playerI, neighbourPlayerI) != count)
			std::abort();
	return count;
}

#endif