#include "../CommonConfig.h"

#if ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "GameContext.h"
#include "Util.h"

#include "../CvMap.h"
#include "../CvPlot.h"
#include "../CvPlayerAI.h"
//#include "../CvGameCoreUtils.h"

//#include "../CvDLLUtilityIFaceBase.h"
//#include "../CvDLLFAStarIFaceBase.h"
//#include "../FAStarNode.h"

using namespace GiganticMapsOptimisationsLib;

static bool myPlotGroupValid(const CvMap& map, ivec2 fromCoord, ivec2 toCoord, PlayerTypes owner, TeamTypes ownerTeam)
{
	const CvPlot* const pOldPlot = map.plotSorenINLINE(fromCoord.x, fromCoord.y);
	const CvPlot* const pNewPlot = map.plotSorenINLINE(toCoord.x, toCoord.y);

	if (pOldPlot->getPlotGroup(owner) == pNewPlot->getPlotGroup(owner))
	{
		if (pNewPlot->isTradeNetwork(ownerTeam))
		{
			if (pNewPlot->isTradeNetworkConnected(pOldPlot, ownerTeam))
			{
				return true;
			}
		}
	}

	return false;
}

// NOTE: pNewPlot->isTradeNetworkConnected(pOldPlot, ownerTeam) DOES NOT imply pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)
// NOTE: pNewPlot->isTradeNetworkConnected(pOldPlot, ownerTeam) is symmetric
// isTradeNetworkConnected can be trivally rewritten to display the symmetry:
/*
bool CvPlot::isTradeNetworkConnected(const CvPlot* pOldPlot, TeamTypes eTeam) const
{
	FAssertMsg(eTeam != NO_TEAM, "eTeam is not assigned a valid value");

	// false, by isTradeNetwork
	if (atWar(eTeam, getTeam()) || atWar(eTeam, pOldPlot->getTeam()))
		return false;

	// false, by isTradeNetwork
	if (isTradeNetworkImpassable(eTeam) || pOldPlot->isTradeNetworkImpassable(eTeam))
		return false;

	// false, by isTradeNetwork
	if (!isOwned())
		if (!isRevealed(eTeam, false) || !(pOldPlot->isRevealed(eTeam, false)))
			return false;

	if (isRoute() && pOldPlot->isRoute())
		return true; // implies pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)

	if (pOldPlot->isNetworkTerrain(eTeam) && isCity(true, eTeam))
		return true; // implies pOldPlot->isTradeNetwork(ownerTeam), but not pNewPlot->isTradeNetwork(ownerTeam), because it might be an unrouted fort.
	if (isNetworkTerrain(eTeam) && pOldPlot->isCity(true, eTeam))
		return true; // implies pNewPlot->isTradeNetwork(ownerTeam), but not pOldPlot->isTradeNetwork(ownerTeam), because it might be an unrouted fort.

	if (pOldPlot->isNetworkTerrain(eTeam) && isNetworkTerrain(eTeam))
		return true; // implies pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)

	if (pOldPlot->isRiverNetwork(eTeam))
		if (pOldPlot->isRiverConnection(directionXY(pOldPlot, this)))
			if (isNetworkTerrain(eTeam))
				return true; // implies pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)

	if (isRiverNetwork(eTeam))
		if (isRiverConnection(directionXY(this, pOldPlot)))
			if (pOldPlot->isNetworkTerrain(eTeam))
				return true; // implies pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)

	if (isRiverNetwork(eTeam))
		if (pOldPlot->isRiverNetwork(eTeam))
			if (isRiverConnection(directionXY(this, pOldPlot)) || pPlot->isRiverConnection(directionXY(pOldPlot, this)))
				return true; // implies pOldPlot->isTradeNetwork(ownerTeam) && pNewPlot->isTradeNetwork(ownerTeam)

	return false;
}
*/

int GiganticMapsOptimisationsLib::countTradeReachablePlots(PlayerTypes owner, int x, int y, int limit)
{
	const TeamTypes ownerTeam = GET_PLAYER(owner).getTeam();
	//FAStar& fastar = GC.getPlotGroupFinder();
	//CvDLLFAStarIFaceBase& fastarInterface = *gDLL->getFAStarIFace();

	// Init FAStar for validity checks.
	//(void)fastarInterface.startGeneratePath(&fastar, x, y, x, y, false, owner, false);

	//VisitSet visitSet;
	const CvMap& map = GC.getMapINLINE();
	const CivMapGeometry mapGeom(map);

	DynamicBitmap2D visitSet(mapGeom.dim);

	int count = 0;

	std::vector<i16vec2> stack;
	stack.reserve(100);
	stack.push_back(ivec2(x, y));
	visitSet[{ x, y }] = true;

	
	constexpr int kNumAdj = std::size(kAdj);

	while (!stack.empty())
	{
		const ivec2 coord = stack.back();
		stack.pop_back();

		++count;

		for (int i = 0; i < kNumAdj; ++i)
		{
			if (const auto optToCoord = mapGeom.tryCanonicalise(coord + kAdj[i]))
			{
				//if (!visitSet.test(optToCoord->x, optToCoord->y))
				if (!visitSet[*optToCoord])
				{
					if (myPlotGroupValid(map, coord, *optToCoord, owner, ownerTeam))
					{
						visitSet[*optToCoord] = true;
						stack.push_back(*optToCoord);
					}
				}
			}
		}

		// Everything on the stack will inc count, so we can include it.
		if (count + static_cast<int>(stack.size()) >= limit)
			return limit;
	}

	return count;
}

#endif