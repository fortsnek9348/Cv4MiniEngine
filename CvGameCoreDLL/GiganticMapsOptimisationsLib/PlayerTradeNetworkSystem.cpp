#include "../CommonConfig.h"

#if ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "PlayerTradeNetworkSystem.h"

#include "../CvPlayerAI.h"
#include "../CvTeamAI.h"
#include "../CvInfos.h"

#include <bit>
#include <iostream>

using namespace GiganticMapsOptimisationsLib;

static constexpr bool kEnableCityBonusVerifyOnInit = true;

static constexpr bool kEnableFullVerifyOnInit = true;
static constexpr bool kEnableFullVerifyOnTurn = false;
// Verification is deferred until read accesses, after any updates have been applied.
static constexpr bool kEnableFullVerifyBeforeRead = false;
// City bonuses verification is always enabled.

static constexpr int kDebugPlayer = 10;
static constexpr ivec2 kDebugCoordFrom{ 59, 40 };
static constexpr int kDebugAdj = 0;
static std::string gLastStateTemp;

// For when one of the bits in the internal bitmaps doesn't match up. Use this to log changes.
// Note that these get called by verification too.
[[maybe_unused]] static void debugCheckPlayerIsTradeNetwork()
{
	CvPlayer& player = GET_PLAYER((PlayerTypes)kDebugPlayer);
	const TeamTypes teamI = player.getTeam();
	//CvTeam& team = GET_TEAM(teamI);
	const CvPlot& plotA = *GC.getMapINLINE().plotSorenINLINE(kDebugCoordFrom.x, kDebugCoordFrom.y);

	const TeamTypes plotATeamI = plotA.getTeam();

	std::ostringstream ss;
	ss << "atWar = " << atWar(teamI, plotATeamI) << '\n';
	ss << "getBlockadedCount = " << plotA.getBlockadedCount(teamI) << '\n';
	ss << "isTradeNetworkImpassable = " << plotA.isTradeNetworkImpassable(teamI) << '\n';
	ss << "isOwned = " << plotA.isOwned() << '\n';
	ss << "isRevealed = " << plotA.isRevealed(teamI, false) << '\n';
	ss << "isBonusNetwork = " << plotA.isBonusNetwork(teamI) << '\n';
	ss << "isRoute = " << plotA.isRoute() << '\n';
	ss << "isRiverNetwork = " << plotA.isRiverNetwork(teamI) << '\n';
	ss << "isNetworkTerrain = " << plotA.isNetworkTerrain(teamI) << '\n';
	ss << "isTerrainTrade = " << GET_TEAM(teamI).isTerrainTrade(plotA.getTerrainType()) << '\n';
	ss << "isWater check = " << (plotA.isWater() && plotATeamI == teamI) << '\n';
	ss << std::flush;
	gLastStateTemp = std::move(ss).str();
	std::clog << __FUNCTION__ << ": " << getPlotCoord(plotA) << " = "
		<< plotA.isTradeNetwork(teamI) << '\n'
		<< gLastStateTemp << std::endl;
	//if (!plotA.isTradeNetwork(teamI))
	//	_CrtDbgBreak();
}

[[maybe_unused]] static void debugCheckPlayerTradeNetworkSystem()
{
	CvPlayer& player = GET_PLAYER((PlayerTypes)kDebugPlayer);
	const TeamTypes teamI = player.getTeam();
	//CvTeam& team = GET_TEAM(teamI);
	const CvPlot& plotA = *GC.getMapINLINE().plotSorenINLINE(kDebugCoordFrom.x, kDebugCoordFrom.y);
	//const int adjI = kDebugAdj;
	const CvPlot& plotB = *GC.getMapINLINE().plotINLINE(kDebugCoordFrom.x + heck::kAdj[kDebugAdj].x, kDebugCoordFrom.y + heck::kAdj[kDebugAdj].y);

	const TeamTypes plotATeamI = plotA.getTeam();
	const TeamTypes plotBTeamI = plotB.getTeam();

	std::ostringstream ss;
	ss << "atWar = " << atWar(teamI, plotATeamI) << ' ' << atWar(teamI, plotBTeamI) << '\n';
	ss << "isTradeNetworkImpassable = " << plotA.isTradeNetworkImpassable(teamI) << ' ' << plotB.isTradeNetworkImpassable(teamI) << '\n';
	ss << "isOwned = " << plotA.isOwned() << ' ' << plotB.isOwned() << '\n';
	ss << "isRevealed = " << plotA.isRevealed(teamI, false) << ' ' << plotB.isRevealed(teamI, false) << '\n';
	ss << "isRoute = " << plotA.isRoute() << ' ' << plotB.isRoute() << '\n';
	ss << "isCity = " << plotA.isCity(true, teamI) << ' ' << plotB.isCity(true, teamI) << '\n';
	ss << "isCity(false) = " << plotA.isCity(false, teamI) << ' ' << plotB.isCity(false, teamI) << '\n';
	ss << "isNetworkTerrain = " << plotA.isNetworkTerrain(teamI) << ' ' << plotB.isNetworkTerrain(teamI) << '\n';
	ss << "isRiverNetwork = " << plotA.isRiverNetwork(teamI) << ' ' << plotB.isRiverNetwork(teamI) << '\n';
	const auto dirAB = directionXY(&plotA, &plotB);
	const auto dirBA = directionXY(&plotB, &plotA);
	ss << "isRiverConnection = " << plotA.isRiverConnection(dirAB) << ' ' << plotB.isRiverConnection(dirBA) << '\n';
	ss << std::flush;
	gLastStateTemp = std::move(ss).str();
	std::clog << __FUNCTION__ << ": "
		<< plotA.isTradeNetworkConnected(&plotB, teamI)
		<< plotB.isTradeNetworkConnected(&plotA, teamI) << '\n'
		<< gLastStateTemp << std::endl;
}

PlayerTradeNetworkSystem::CivDataSource::CivDataSource(PlayerTypes playerI)
	: playerI(playerI)
	, teamI(GET_PLAYER(playerI).getTeam())
	, map(GC.getMapINLINE())
{
}

//bool PlayerTradeNetworkSystem::CivDataSource::isCity(ivec2 coord) const
//{
//	const CvPlot& plot = *map.plotSorenINLINE(coord.x, coord.y);
//	return plot.isCity() && plot.getOwnerINLINE() == playerI;
//}
bool PlayerTradeNetworkSystem::CivDataSource::isTradeNetwork(ivec2 coord) const
{
	//if ((int)playerI == kDebugPlayer && coord == kDebugCoordFrom)
	//	debugCheckPlayerIsTradeNetwork();
	return map.plotSorenINLINE(coord.x, coord.y)->isTradeNetwork(teamI);
}
bool PlayerTradeNetworkSystem::CivDataSource::isTradeNetworkConnected(ivec2 a, ivec2 b, [[maybe_unused]] int adjI) const
{
	//if ((int)playerI == kDebugPlayer && a == kDebugCoordFrom && adjI == kDebugAdj)
	//	debugCheckPlayerTradeNetworkSystem();
	return map.plotSorenINLINE(a.x, a.y)->isTradeNetworkConnected(map.plotSorenINLINE(b.x, b.y), teamI);
}
heck::TradeNetworkCore::EBonus PlayerTradeNetworkSystem::CivDataSource::getNetworkPlotBonus(ivec2 coord) const
{
	// How this works is that only plots we own produce bonuses for the network (updatePlotGroupBonus uses the owner's plot group).
	const CvPlot& plot = *map.plotSorenINLINE(coord.x, coord.y);
	if (plot.getOwnerINLINE() != playerI)
		return heck::TradeNetworkCore::kNoBonus;

	BonusTypes plotBonus = NO_BONUS;

	if (const BonusTypes eNonObsoleteBonus = plot.getNonObsoleteBonusType(plot.getTeam()); eNonObsoleteBonus != NO_BONUS)
	{
		if (GET_TEAM(plot.getTeam()).isHasTech((TechTypes)(GC.getBonusInfo(eNonObsoleteBonus).getTechCityTrade())))
		{
			if (plot.isCity(true, plot.getTeam()) ||
				((plot.getImprovementType() != NO_IMPROVEMENT) && GC.getImprovementInfo(plot.getImprovementType()).isImprovementBonusTrade(eNonObsoleteBonus)))
			{
				if (plot.isBonusNetwork(plot.getTeam()))
				{
					plotBonus = eNonObsoleteBonus;
				}
			}
		}
	}

	return static_cast<EBonus>(plotBonus);
}
std::vector<heck::TradeNetworkCore::BonusCount> PlayerTradeNetworkSystem::CivDataSource::getNetworkCityExtraBonuses(ivec2 coord) const
{
	const CvPlot& plot = *map.plotSorenINLINE(coord.x, coord.y);
	if (plot.getOwnerINLINE() != playerI)
		return {};

	if (const CvCity* const pPlotCity = plot.getPlotCity())
	{
		const int numBonuses = GC.getNumBonusInfos();
		std::vector<heck::TradeNetworkCore::BonusCount> cityExtraBonuses(numBonuses);

		for (int iI = 0; iI < numBonuses; ++iI)
			if (!GET_TEAM(teamI).isBonusObsolete((BonusTypes)iI))
				cityExtraBonuses[iI] += pPlotCity->getFreeBonus((BonusTypes)iI);

		if (pPlotCity->isCapital())
		{
			const CvPlayer& player = GET_PLAYER(playerI);
			for (int iI = 0; iI < numBonuses; ++iI)
			{
				cityExtraBonuses[iI] -= player.getBonusExport((BonusTypes)iI);
				cityExtraBonuses[iI] += player.getBonusImport((BonusTypes)iI);
			}
		}

		return cityExtraBonuses;
	}

	return {};
}

PlayerTradeNetworkSystem::PlayerTradeNetworkSystem(PlayerTypes playerI)
	: mPlayerI(playerI)
	, mGenericSys(CivMapGeometry(GC.getMapINLINE()), GC.getNumBonusInfos(), CivDataSource(playerI))
{
	const int numBonuses = GC.getNumBonusInfos();
	const CvPlayer& player = GET_PLAYER(playerI);

	//constexpr int debugPlayerI = 2;
	//constexpr int debugCityId = 8192;

	//if (int(playerI) == debugPlayerI)
	//{
	//	const CvCity& city = *player.getCity(debugCityId);
	//	std::clog << int(playerI) << ", " << city.getID() << std::endl;
	//	std::clog << "City Bonuses: ";
	//	for (int i = 0; i < numBonuses; ++i)
	//		std::clog << city.getNumBonuses(static_cast<BonusTypes>(i)) << ' ';
	//	std::clog << std::endl;
	//	
	//	const CvMap& map = GC.getMapINLINE();
	//	CvPlotGroup* cityPlotGroup{};
	//	int itIndex{};
	//	for (CvPlotGroup* const plotGroup = player.firstPlotGroup(&itIndex); plotGroup; player.nextPlotGroup(&itIndex))
	//	{
	//		for (auto* node = plotGroup->headPlotsNode(); node; node = plotGroup->nextPlotsNode(node))
	//		{
	//			CvPlot& plot = *map.plotSorenINLINE(node->m_data.iX, node->m_data.iY);
	//			if (city.plot() == &plot)
	//			{
	//				cityPlotGroup = plotGroup;
	//				goto done;
	//			}
	//		}
	//	}
	//done:
	//	std::clog << "PlotGroup: " << cityPlotGroup->getID() << std::endl;
	//	std::clog << "Old PlotGroup Bonuses: ";
	//	for (int i = 0; i < numBonuses; ++i)
	//		std::clog << cityPlotGroup->getNumBonuses(static_cast<BonusTypes>(i)) << ' ';
	//	std::clog << std::endl;
	//	std::clog << "PlotGroup num plots: " << cityPlotGroup->getLengthPlots() << std::endl;
	//	std::vector<heck::ivec2> plotCoords;
	//	for (auto* node = cityPlotGroup->headPlotsNode(); node; node = cityPlotGroup->nextPlotsNode(node))
	//		plotCoords.push_back(ivec2(node->m_data.iX, node->m_data.iY));
	//	std::ranges::sort(plotCoords, heck::VectorComparator());
	//	std::clog << "PlotGroup plots hash: " << std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(plotCoords.data()), std::span(plotCoords).size_bytes())) << std::endl;
	//
	//	std::vector<int> ourBonusCounts(numBonuses);
	//	CivDataSource dataSource(mPlayerI);
	//	for (const ivec2 coord : plotCoords)
	//	{
	//		CvPlot& plot = *map.plotSorenINLINE(coord.x, coord.y);
	//		if (dataSource.getNetworkPlotBonus(coord) != heck::TradeNetworkCore::kNoBonus)
	//			++ourBonusCounts[(int)dataSource.getNetworkPlotBonus(coord)];
	//		const std::vector<int> cityOutputBonusCounts = dataSource.getNetworkCityExtraBonuses(coord);
	//		if (plot.getPlotCity())
	//			if (plot.getOwnerINLINE() == playerI)
	//				for (int i = 0; i < (int)cityOutputBonusCounts.size(); ++i)
	//					ourBonusCounts[i] += cityOutputBonusCounts[i];
	//	}
	//
	//	std::clog << "Calculated plot group bonuses: ";
	//	for (int i = 0; i < numBonuses; ++i)
	//		std::clog << ourBonusCounts[i] << ' ';
	//	std::clog << std::endl;
	//}

	struct OriginalCityInfo
	{
		int cityId{};
		std::vector<int> bonusCounts{};
	};

	std::vector<OriginalCityInfo> originalCityInfos;
	originalCityInfos.reserve(player.getNumCities());
	int itIndex{};
	for (CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
	{
		originalCityInfos.emplace_back(city->getID(), range<BonusTypes>(numBonuses) | std::views::transform([city](BonusTypes bonusI) {
			return city->getNumBonuses(bonusI);
			}) | std::ranges::to<std::vector>());
	}

	// This will change every city to have a network input of zero bonuses.
	// Also, save the bonus counts of each city, so we can verify.
	destroyAllCvPlotGroups();

	for ([[maybe_unused]] const auto& [cityId, bonusCounts] : originalCityInfos)
	{
		const CvCity& city = *player.getCity(cityId);
		mGenericSys.newCity(getPlotCoord(*city.plot()));
		mGenericSys.onUpdateBonusesAtPlot(getPlotCoord(*city.plot()), CivDataSource(mPlayerI)); // Very important!
	}

	// This will flush for every city. mGenericSys should compute the updated city input bonuses with the assumption that their current input is zero.
	flushCityUpdates();

	if constexpr (kEnableFullVerifyOnInit)
		mGenericSys.verify(CivDataSource(mPlayerI));

	//if (int(playerI) == debugPlayerI)
	//{
	//	const CvCity& city = *player.getCity(debugCityId);
	//	const auto cpgI = mGenericSys.getCPGAt(getPlotCoord(*city.plot()));
	//	const auto& bonusCounts = mGenericSys.getCPGBonusCounts(cpgI);
	//	std::clog << "New PlotGroup Bonuses        : ";
	//	for (int i = 0; i < numBonuses; ++i)
	//		std::clog << bonusCounts[i] << ' ';
	//	std::clog << std::endl;
	//	std::clog << "New city Bonuses             : ";
	//	for (int i = 0; i < numBonuses; ++i)
	//		std::clog << city.getNumBonuses(static_cast<BonusTypes>(i)) << ' ';
	//	std::clog << std::endl;
	//	std::vector<heck::ivec2> plotCoords = mGenericSys.getPlotsOfCPG(cpgI);
	//	std::clog << "New PlotGroup num plots: " << plotCoords.size() << std::endl;
	//	std::ranges::sort(plotCoords, heck::VectorComparator());
	//	std::clog << "New PlotGroup plots hash: " << std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(plotCoords.data()), std::span(plotCoords).size_bytes())) << std::endl;
	//}

	// Now check that city bonuses match.
	if constexpr (kEnableCityBonusVerifyOnInit)
	{
		for (const auto& [cityId, bonusCounts] : originalCityInfos)
		{
			const CvCity& city = *player.getCity(cityId);
			for (int i = 0; i < numBonuses; ++i)
			{
				const int oldN = bonusCounts[i];
				const int newN = city.getNumBonuses(static_cast<BonusTypes>(i));
				if (oldN != newN)
					throw std::runtime_error("PlayerTradeNetworkSystem city bonus count mismatch!");
			}
		}
	}

	//// Check existing groups.
	//if constexpr (kVerifyInitialPlotGroups)
	//{
	//	// Pick any plot as the representative.
	//	std::unordered_map<heck::i16vec2, BlockPlotGroupRef, VectorHasher> representativePlotToRoot;
	//	for (const auto& [representativeRoot, blockPlotGroupRefs] : mRepresentativeRootToBlockPlotGroups)
	//	{
	//		const heck::i16vec2 coord = mBlocks[blockPlotGroupRefs[0].blkCoord].blockPlotGroups[blockPlotGroupRefs[0].blkPlotGroupIndex].plotCoords.front();
	//		representativePlotToRoot[coord] = representativeRoot;
	//	}
	//
	//	// Should be the same number of CvPlotGroups (let's hope they're all non-empty).
	//	if (std::cmp_not_equal(player.getNumPlotGroups(), representativePlotToRoot.size()))
	//		std::abort();
	//
	//	std::unordered_set<const CvPlotGroup*> plotGroups;
	//
	//	for (const auto [representativePlotCoord, representativeRoot] : representativePlotToRoot)
	//	{
	//		const CvPlot& plot = *map.plotSorenINLINE(representativePlotCoord.x, representativePlotCoord.y);
	//		const CvPlotGroup* const plotGroup = plot.getPlotGroup(playerI);
	//
	//		// There should be a plot group.
	//		if (!plotGroup)
	//			std::abort();
	//
	//		// Check duplicate.
	//		if (!plotGroups.insert(plotGroup).second)
	//			std::abort();
	//
	//		std::vector<heck::i16vec2> plotGroupPlotCoords(std::from_range, plotGroup->getDirectPlotCoords());
	//
	//		// We shouldn't have any of these yet.
	//		if (!plotGroup->getBlockPlotGroupRefs().empty())
	//			std::abort();
	//
	//		// Compare plot lists.
	//		std::vector<heck::i16vec2> blockPlotGroupPlotCoords;
	//		for (const BlockPlotGroupRef ref : mRepresentativeRootToBlockPlotGroups.at(representativeRoot))
	//			blockPlotGroupPlotCoords.append_range(mBlocks[ref.blkCoord].blockPlotGroups[ref.blkPlotGroupIndex].plotCoords);
	//
	//		std::ranges::sort(plotGroupPlotCoords, VectorComparator());
	//		std::ranges::sort(blockPlotGroupPlotCoords, VectorComparator());
	//		
	//		// Must be equal.
	//		if (!std::ranges::equal(plotGroupPlotCoords, blockPlotGroupPlotCoords))
	//			std::abort();
	//	}
	//
	//	// Should have the same number of plot groups.
	//	if (std::cmp_not_equal(plotGroups.size(), player.getNumPlotGroups()))
	//		std::abort();
	//}
	//
	//// Delete all existing plot groups.
	//std::vector<int> ids;
	//ids.reserve(player.getNumPlotGroups());
	//int itIndex{};
	//for (CvPlotGroup* plotGroup = player.firstPlotGroup(&itIndex); plotGroup; plotGroup = player.nextPlotGroup(&itIndex))
	//{
	//	plotGroup->disconnectAllPlots();
	//	ids.push_back(plotGroup->getID());
	//}
	//for (const int id : ids)
	//	player.deletePlotGroup(id);
	//
	//// Create new plot groups.
	//// (now, we could emplace block plot groups refs into existing plot groups, but just in case, recreate them all)
	//for (const auto& [representativeRoot, blockPlotGroupRefs] : mRepresentativeRootToBlockPlotGroups)
	//{
	//	CvPlotGroup* const newPlotGroup = player.addPlotGroup();
	//	newPlotGroup->addBlockPlotGroupRefs(blockPlotGroupRefs);
	//}
}

void PlayerTradeNetworkSystem::onUpdateAllPlotGroups()
{
	//if (int(mPlayerI) == 5)
	//	std::clog << __FUNCTION__ << ": player = " << (int)mPlayerI << std::endl;

	mGenericSys.onUpdateAllPlotGroups(CivDataSource(mPlayerI));
	flushCityUpdates();
	//if (int(mPlayerI) == 5)
	//	verify();
	mPendingVerify = true;

	//if ((int)mPlayerI == 10)
	//	verify();
}

void PlayerTradeNetworkSystem::onUpdatePlotGroupsForPlot(const CvPlot& plot)
{
	//if (int(mPlayerI) == 5 && getPlotCoord(plot) == i16vec2(21, 12))
	//	std::clog << __FUNCTION__ << ": player = " << (int)mPlayerI << ' ' << getPlotCoord(plot) << std::endl;
	mGenericSys.onUpdatePlotGroupsForPlot(getPlotCoord(plot), CivDataSource(mPlayerI));
	flushCityUpdates();
	mPendingVerify = true;

	//if ((int)mPlayerI == 10)
	//	verify();
}

void PlayerTradeNetworkSystem::onUpdateBonusesAtPlot(const CvPlot& plot)
{
	// void CvPlot::updatePlotGroupBonus(bool bAdd)
	//if (int(mPlayerI) == 5 && getPlotCoord(plot) == i16vec2(21, 12))
	//	std::clog << __FUNCTION__ << ": player = " << (int)mPlayerI << ' ' << getPlotCoord(plot) << std::endl;
	//const heck::ivec2 coord = getPlotCoord(plot);
	mGenericSys.onUpdateBonusesAtPlot(getPlotCoord(plot), CivDataSource(mPlayerI));
	flushCityUpdates();
	mPendingVerify = true;

	// Can't verify here as CvPlot::setRouteType calls this before `onUpdatePlotGroupsForPlot`.
	//if constexpr (kEnableFullVerifyEveryUpdate)
	//	verify();

	//if ((int)mPlayerI == 10)
	//	verify();
}

void PlayerTradeNetworkSystem::newCity(CvCity* city)
{
	if (city)
	{
		const CvPlot& plot = *city->plot();
		//if (int(mPlayerI) == 5 && getPlotCoord(plot) == i16vec2(21, 12))
		//	std::clog << __FUNCTION__ << ": player = " << (int)mPlayerI << ' ' << getPlotCoord(plot) << std::endl;
		const heck::ivec2 coord = getPlotCoord(plot);
		mGenericSys.newCity(coord);
		onUpdateBonusesAtPlot(plot);
		flushCityUpdates();
		mPendingVerify = true;
	}

	//if ((int)mPlayerI == 10)
	//	verify();
}
void PlayerTradeNetworkSystem::destroyCity(CvCity* city)
{
	if (city)
	{
		const heck::ivec2 coord = getPlotCoord(*city->plot());
		
		//if (int(mPlayerI) == 5 && getPlotCoord(plot) == i16vec2(21, 12))
		//	std::clog << __FUNCTION__ << ": player = " << (int)mPlayerI << ' ' << getPlotCoord(plot) << std::endl;
		//const std::span<const heck::TradeNetworkCore::BonusCount> counts = mGenericSys.getCityNetworkBonuses(coord);
		//for (size_t i = 0; i < counts.size(); ++i)
		//	city->changeNumBonuses(static_cast<BonusTypes>(i), -counts[i]);
		const size_t numBonuses = GC.getNumBonusInfos();
		for (size_t i = 0; i < numBonuses; ++i)
			city->setNumBonuses(static_cast<BonusTypes>(i), 0);
		mGenericSys.destroyCity(coord);
		flushCityUpdates();
		mPendingVerify = true;
	}

	//if ((int)mPlayerI == 10)
	//	verify();
}

// !!a->getPlotGroup()
bool PlayerTradeNetworkSystem::isInPlotGroup(const CvPlot& plot) const
{
	if constexpr (kEnableFullVerifyBeforeRead)
		if (mPendingVerify)
			mPendingVerify = false, verify();
	return mGenericSys.isInPlotGroup(getPlotCoord(plot));
}
// a->getPlotGroup() == b->getPlotGroup()
bool PlayerTradeNetworkSystem::isSamePlotGroup(const CvPlot& a, const CvPlot& b) const
{
	if constexpr (kEnableFullVerifyBeforeRead)
		if (mPendingVerify)
			mPendingVerify = false, verify();
	return mGenericSys.isSamePlotGroup(getPlotCoord(a), getPlotCoord(b));
}
// a->getPlotGroup() ? a->getPlotGroup()->getNumBonuses(bonusI) : 0
int PlayerTradeNetworkSystem::getPlotGroupNumBonuses(const CvPlot& a, BonusTypes bonusI) const
{
	if constexpr (kEnableFullVerifyBeforeRead)
		if (mPendingVerify)
			mPendingVerify = false, verify();
	return mGenericSys.getPlotGroupNumBonuses(getPlotCoord(a), static_cast<heck::TradeNetworkCore::EBonus>(bonusI));
}
// a->getPlotGroup() && a->getPlotGroup()->hasBonus(bonusI)
int PlayerTradeNetworkSystem::getPlotGroupHasBonus(const CvPlot& a, BonusTypes bonusI) const
{
	return getPlotGroupNumBonuses(a, bonusI) > 0;
}

std::optional<int> PlayerTradeNetworkSystem::tryGetPlotGroupId(const CvPlot& plot) const
{
	const auto cpgI = mGenericSys.getCPGAt(getPlotCoord(plot));
	if (cpgI != heck::TradeNetworkCore::kNoCPG)
		return std::to_underlying(cpgI);
	else
		return std::nullopt;
}

void PlayerTradeNetworkSystem::writeAsStreamableFFreeListTrashArray(FDataStreamBase& stream) const
{
	// WriteStreamableFFreeListTrashArray(m_plotGroups, pStream);

	// We don't have a plot groups array. Fake it, as-if we made CvPlotGroup instances from PlayerTradeNetworkSystem CPGs, in order.
	// See also: CvPlot::write.

	const int numCPGs = static_cast<int>(mGenericSys.getNumAllocatedCPGs());

	// Must be pow-2, by FFreeListTrashArray<T>::init.
	const unsigned int trashArrayNumSlots = std::bit_ceil(static_cast<unsigned int>(numCPGs));
		
	std::vector<int> freeIndices;
	for (int i = 0; i < numCPGs; ++i)
		if (mGenericSys.isFreeCpgIndex(static_cast<heck::TradeNetworkCore::ECivPlotGroup>(i)))
			freeIndices.push_back(i);

	if (freeIndices.size() != mGenericSys.getNumAllocatedCPGs() - mGenericSys.getNumActiveCPGs())
		std::abort();

	stream.Write(static_cast<int>(trashArrayNumSlots)); // pStream->Write(flist.getNumSlots());
	stream.Write(static_cast<int>(numCPGs - 1)); // pStream->Write(flist.getLastIndex());
	stream.Write(static_cast<int>(freeIndices.empty() ? FFreeList::FREE_LIST_INDEX : freeIndices.front())); // pStream->Write(flist.getFreeListHead());
	stream.Write(static_cast<int>(freeIndices.size())); // pStream->Write(flist.getFreeListCount());
	stream.Write(static_cast<int>((numCPGs + 1) << FLTA_ID_SHIFT)); // pStream->Write(flist.getCurrentID());

	// Fake the free list.
	std::vector<int> linkedFreeList(trashArrayNumSlots, FFreeList::INVALID_INDEX);
	for (const auto [a, b] : std::views::adjacent<2>(freeIndices))
		linkedFreeList[a] = b;
	stream.Write(static_cast<int>(linkedFreeList.size()), linkedFreeList.data()); // pStream->Write( flist.getNextFreeIndex( i ) );

	const int numActiveCPGs = numCPGs - static_cast<int>(freeIndices.size());
	stream.Write(numActiveCPGs);

	const int numBonuses = GC.getNumBonusInfos();

	//int numCPGsWritten = 0;

	for (int i = 0; i < numCPGs; ++i)
	{
		const auto cpgI = static_cast<heck::TradeNetworkCore::ECivPlotGroup>(i);
		if (mGenericSys.isFreeCpgIndex(cpgI))
			continue;

		//++numCPGsWritten;

		// CvPlotGroup::write
		// unsigned int uiFlag = 0;
		// 
		// 
		// pStream->Write(m_iID);
		// 
		// pStream->Write(m_eOwner);
		// 
		// FAssertMsg((0 < GC.getNumBonusInfos()), "GC.getNumBonusInfos() is not greater than zero but an array is being allocated in CvPlotGroup::write");
		// pStream->Write(GC.getNumBonusInfos(), m_paiNumBonuses);
		// 
		// m_plots.Write(pStream);

		stream.Write(static_cast<unsigned int>(0)); // pStream->Write(uiFlag);		// flag for expansion
		const int id = i + 1;
		stream.Write(static_cast<int>(i | (id << FLTA_ID_SHIFT))); // pStream->Write(m_iID);
		stream.Write(static_cast<int>(mPlayerI)); // pStream->Write(m_eOwner);
		stream.Write(numBonuses, mGenericSys.getCPGBonusCounts(cpgI).data());

		const std::vector<ivec2> plots(std::from_range, mGenericSys.getPlotsOfCPG(cpgI));
		stream.Write(static_cast<int>(plots.size()));
		static_assert(std::endian::native == std::endian::little);
		static_assert(sizeof(plots[0]) == sizeof(XYCoords));
		stream.Write(static_cast<int>(plots.size() * sizeof(plots[0])), reinterpret_cast<const unsigned char*>(plots.data()));
	}

	//if (numCPGsWritten != numActiveCPGs)
	//	throw std::runtime_error("PlayerTradeNetworkSystem is corrupt. Contains empty plot groups.");
}

void PlayerTradeNetworkSystem::onTurn()
{
	if constexpr (kEnableFullVerifyOnTurn)
		verify();
}

void PlayerTradeNetworkSystem::verify() const
{
	if ((int)mPlayerI == 10 && GC.getGameINLINE().getGameTurn() == 32)
		mGenericSys.dump("PlayerTradeNetworkSystem pre-verify dump.html");
	mGenericSys.verify(CivDataSource(mPlayerI));
}

void PlayerTradeNetworkSystem::dump(const char* filename) const
{
	mGenericSys.dump(filename);
}

/// private

// Removes the bonus count from cities.
void PlayerTradeNetworkSystem::destroyAllCvPlotGroups()
{
	const CvPlayer& player = GET_PLAYER(mPlayerI);
	//const CvMap& map = GC.getMapINLINE();

	//const int numBonuses = GC.getNumBonusInfos();

	//int itIndex{};
	//for (CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
	//{
	//	const CvPlotGroup* const plotGroup
	//	for (int i = 0; i < numBonuses; ++i)
	//		city->changeNumBonuses
	//}

	//std::vector<CityInfo> cityInfos;
	
	while (player.getNumPlotGroups() != 0)
	{
		int itIndex{};
		CvPlotGroup* const plotGroup = player.firstPlotGroup(&itIndex);

		// Remove changes from plots.
		//for (auto* node = plotGroup->headPlotsNode(); node; node = plotGroup->nextPlotsNode(node))
		//{
		//	CvPlot& plot = *map.plotSorenINLINE(node->m_data.iX, node->m_data.iY);
		//	//if (CvCity* const city = plot.getPlotCity())
		//	//	if (plot.getOwnerINLINE() == mPlayerI)
		//	//	{
		//	//		std::vector<int> bonusCounts(numBonuses);
		//	//		for (int i = 0; i < numBonuses; ++i)
		//	//		{
		//	//			bonusCounts[i] = city->getNumBonuses(static_cast<BonusTypes>(i));
		//	//			//city->changeNumBonuses(static_cast<BonusTypes>(i), -plotGroup->getNumBonuses(static_cast<BonusTypes>(i)));
		//	//		}
		//	//		//cityInfos.emplace_back(city, std::move(bonusCounts));
		//	//	}
		//	//map.plotSorenINLINE(node->m_data.iX, node->m_data.iY)->setPlotGroup(mPlayerI, nullptr);
		//	plot.setSerialisationFakePlotGroupID(mPlayerI, FFreeList::INVALID_INDEX);
		//}
		// Clear plot list. This will delete the group.
		for (auto* node = plotGroup->headPlotsNode(); node; node = plotGroup->deletePlotsNode(node))
			;
	}

	//return cityInfos;
}

void PlayerTradeNetworkSystem::flushCityUpdates()
{
	const CvMap& map = GC.getMapINLINE();
	for (const heck::TradeNetworkCore::CityUpdate& cityUpdate : mGenericSys.flushCityUpdates())
	{
		const CvPlot& plot = *map.plotSorenINLINE(cityUpdate.cityCoord.x, cityUpdate.cityCoord.y);
		assert(plot.getOwnerINLINE() == mPlayerI);
		CvCity& city = *plot.getPlotCity();
		for (const auto [i, n] : std::views::enumerate(cityUpdate.bonusCounts))
			//if (d)
			//	city.changeNumBonuses(static_cast<BonusTypes>(i), d);
			city.setNumBonuses(static_cast<BonusTypes>(i), n);
	}
}

#endif