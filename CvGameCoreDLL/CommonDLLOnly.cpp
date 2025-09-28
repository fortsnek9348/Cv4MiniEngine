#include "CommonDLLOnly.h"

#if ENABLE_DIVERGENCE_CHECKPOINT_LOGGING

#include "CvGameAI.h"
#include "CvGlobals.h"
#include "CvMap.h"
#include "CvPlot.h"
#include "CvPlayerAI.h"
#include "CvTeamAI.h"
#include "CvGameCoreUtils.h"
#include "CvInfos.h"
#include "GiganticMapsOptimisationsLib/PlayerTradeNetworkSystem.h"

#include <CommonStuff/ParallelExt.h>
#include <CommonStuff/vec.h>
#include <CommonStuff/Array2D.h>
#include <CommonStuff/UnionFind.h>
#include <CommonStuff/adj.h>

#include <sstream>

static constexpr int kDebugGameStateLogStartTurnSlice = 0;
static constexpr int kDebugGameStateLogStartCounter = 0;

static constexpr int kDebugGameStateLogFinalTurnSlice = 780000;
static constexpr int kDebugGameStateLogFinalCounter = 0;

static constexpr int kDebugGameStateLogDetailedLogTurnSlice = -3987;
static constexpr int kDebugGameStateLogDetailedLogCounterMin = 340;
static constexpr int kDebugGameStateLogDetailedLogCounterMax = kDebugGameStateLogDetailedLogCounterMin + 1;

static constexpr int kDebugGameStateLogPeriod = 10000;

namespace
{
	struct PlotGroupInfo
	{
		std::pair<int, int> minCoord{ INT_MAX, INT_MAX };
		int numPlots = 0;
		uint64_t sumHash{};

		friend auto operator<=>(PlotGroupInfo, PlotGroupInfo) = default;
	};
}

static void dumpPlotGroups(std::ostream& os, const CvPlayer& player)
{
	const CvMap& map = GC.getMapINLINE();
	const PlayerTypes playerI = player.getID();
	const int w = map.getGridWidthINLINE();
	const int h = map.getGridHeightINLINE();

	std::unordered_map<int, PlotGroupInfo> plotGroupsHashes;

#if ENABLE_GAMECOREDLL_ENHANCEMENTS
	// if (plot group sys enabled)
	//if ((int)playerI == 1)
	//{
	//	std::clog << "XXX "
	//		<< map.plotSorenINLINE(57, 17)->tryGetPlotGroupId(playerI).value_or(-1)
	//		<< ' ' << map.plotSorenINLINE(57, 18)->tryGetPlotGroupId(playerI).value_or(-1)
	//		<< std::endl;
	//}
#endif

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const CvPlot& plot = *map.plotSorenINLINE(x, y);

#if ENABLE_GAMECOREDLL_ENHANCEMENTS
			if (const auto optId = plot.tryGetPlotGroupId(playerI))
			{
				const int id = *optId;
#else
			if (const CvPlotGroup* const plotGroup = plot.getPlotGroup(playerI))
			{
				const int id = plotGroup->getID();
#endif
				PlotGroupInfo& info = plotGroupsHashes[id];
				info.minCoord = std::min(info.minCoord, std::pair(x, y));
				info.numPlots += 1;
				info.sumHash += heck::VectorHasher()(heck::ivec2(x, y));
			}
		}
	}

	std::vector<PlotGroupInfo> plotGroupInfos(std::from_range, plotGroupsHashes | std::views::values);
	std::ranges::sort(plotGroupInfos);

	const int numBonuses = GC.getNumBonusInfos();
	for (const PlotGroupInfo& info : plotGroupInfos)
	{
		os << info.minCoord.first << ' ' << info.minCoord.second << ' ' << info.numPlots << ' ' << info.sumHash;
		for (int i = 0; i < numBonuses; ++i)
			os << ' ' << map.plotSorenINLINE(info.minCoord.first, info.minCoord.second)->getPlotGroupConnectedBonus(playerI, static_cast<BonusTypes>(i));
		os << '\n';
	}
	//os << '\n';
}

static void dumpFreshPlotGroups(std::ostream& os, CvPlayer& player)
{
	const CvMap& map = GC.getMapINLINE();
	const PlayerTypes playerI = player.getID();
	const TeamTypes teamI = player.getTeam();
	const int w = map.getGridWidthINLINE();
	const int h = map.getGridHeightINLINE();

#if ENABLE_GAMECOREDLL_ENHANCEMENTS
	// It's difficult to verify as checkpoints can easily be inside a plot group update loop over players (player plot groups are unstable).
	//player.getPlayerTradeNetworkSystem()->verify();
#endif

	heck::DynamicArray2D<heck::i16vec2> ufStorage({ w, h });
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			ufStorage[heck::ivec2(x, y)] = heck::ivec2(x, y);
	heck::ArrayUnionFind uf(ufStorage);

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const CvPlot& plot = *map.plotSorenINLINE(x, y);
			if (plot.isTradeNetwork(teamI))
				for (int iI = 0; iI < NUM_DIRECTION_TYPES; ++iI)
					if (const CvPlot* const pAdjacentPlot = plotDirection(x, y, ((DirectionTypes)iI)))
						if (pAdjacentPlot->isTradeNetwork(teamI) && plot.isTradeNetworkConnected(pAdjacentPlot, teamI))
							uf.unionise(heck::ivec2(x, y), heck::ivec2(pAdjacentPlot->getX_INLINE(), pAdjacentPlot->getY_INLINE()));
		}
	}

	struct FreshPlotGroupInfo : PlotGroupInfo
	{
		std::vector<int> bonusCounts;
	};

	std::unordered_map<heck::ivec2, FreshPlotGroupInfo, heck::VectorHasher> plotGroupsHashes;

	const int numBonuses = GC.getNumBonusInfos();
	const CvTeam& team = GET_TEAM(teamI);

	std::vector<bool> isBonusObsolete(numBonuses);
	for (int iI = 0; iI < numBonuses; ++iI)
		isBonusObsolete[iI] = team.isBonusObsolete((BonusTypes)iI);

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const CvPlot& plot = *map.plotSorenINLINE(x, y);
			if (plot.isTradeNetwork(teamI))
			{
				const heck::ivec2 rep = uf.find(heck::ivec2(x, y));

				FreshPlotGroupInfo& info = plotGroupsHashes[rep];
				info.minCoord = std::min(info.minCoord, std::pair(x, y));
				info.numPlots += 1;
				info.sumHash += heck::VectorHasher()(heck::ivec2(x, y));

				info.bonusCounts.resize(numBonuses);

				if (plot.getOwnerINLINE() == playerI)
				{
					if (const CvCity* const pPlotCity = plot.getPlotCity())
					{
						for (int iI = 0; iI < numBonuses; ++iI)
							if (!isBonusObsolete[iI])
								info.bonusCounts[iI] += pPlotCity->getFreeBonus((BonusTypes)iI);

						if (pPlotCity->isCapital())
						{
							for (int iI = 0; iI < numBonuses; ++iI)
							{
								info.bonusCounts[iI] -= player.getBonusExport((BonusTypes)iI);
								info.bonusCounts[iI] += player.getBonusImport((BonusTypes)iI);
							}
						}
					}

					if (const BonusTypes eNonObsoleteBonus = plot.getNonObsoleteBonusType(teamI); eNonObsoleteBonus != NO_BONUS)
					{
						if (team.isHasTech((TechTypes)(GC.getBonusInfo(eNonObsoleteBonus).getTechCityTrade())))
						{
							if (plot.isCity(true, teamI) ||
								((plot.getImprovementType() != NO_IMPROVEMENT) && GC.getImprovementInfo(plot.getImprovementType()).isImprovementBonusTrade(eNonObsoleteBonus)))
							{
								if (plot.isBonusNetwork(teamI))
								{
									info.bonusCounts[eNonObsoleteBonus] += 1;
								}
							}
						}
					}
				}
			}
		}
	}

	// Dump plot bonuses.
	os << "Plot network bonuses:";
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const CvPlot& plot = *map.plotSorenINLINE(x, y);
			if (plot.isTradeNetwork(teamI))
			{
				if (const BonusTypes eNonObsoleteBonus = plot.getNonObsoleteBonusType(teamI); eNonObsoleteBonus != NO_BONUS)
				{
					if (team.isHasTech((TechTypes)(GC.getBonusInfo(eNonObsoleteBonus).getTechCityTrade())))
					{
						if (plot.isCity(true, teamI) ||
							((plot.getImprovementType() != NO_IMPROVEMENT) && GC.getImprovementInfo(plot.getImprovementType()).isImprovementBonusTrade(eNonObsoleteBonus)))
						{
							if (plot.isBonusNetwork(teamI))
							{
								os << ' ' << heck::ivec2(x, y) << ' ' << (int)eNonObsoleteBonus << ';';
							}
						}
					}
				}
			}
		}
	}
	os << '\n';

	std::vector<FreshPlotGroupInfo> plotGroupInfos(std::from_range, plotGroupsHashes | std::views::values);

	std::ranges::sort(plotGroupInfos);

	
	for (const FreshPlotGroupInfo& info : plotGroupInfos)
	{
		os << info.minCoord.first << ' ' << info.minCoord.second << ' ' << info.numPlots << ' ' << info.sumHash;
		for (int i = 0; i < numBonuses; ++i)
			os << ' ' << info.bonusCounts[i];
		os << '\n';
	}
	//os << '\n';
}

static std::string serialisePlayer(PlayerTypes playerI)
{
	CvPlayerAI& player = GET_PLAYER(playerI);
	std::ostringstream os;
	os << "Player " << (int)playerI << '\n';
	for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		os << "Commerce " << player.getCommerceRate(CommerceTypes(i)) << '\n';

	os << "Own#=" << player.getNumCities()
		<< " DMM=" << player.getDistanceMaintenanceModifier()
		<< '\n';

	for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; iPlayer++)
	{
		CvPlayer& kLoopPlayer = GET_PLAYER((PlayerTypes)iPlayer);
		if (kLoopPlayer.getTeam() != player.getTeam() && GET_TEAM(kLoopPlayer.getTeam()).isVassal(player.getTeam()))
			os << "Vas#=" << kLoopPlayer.getNumCities() << '\n';
	}

	os << '\n';

	const int numBonuses = GC.getNumBonusInfos();

	int itIndex{};
	for (const CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
	{
		os << "City " << city->getID();
		os << ' ' << heck::ivec2(city->getX_INLINE(), city->getY_INLINE());
		os << " pop=" << city->getPopulation();
		if (city->isGovernmentCenter())
			os << " gov";
		os << " M=" << city->getMaintenanceTimes100();
		os << " disM=" << city->calculateDistanceMaintenanceTimes100();
		os << " numM=" << city->calculateNumCitiesMaintenanceTimes100();
		os << ' ' << city->calculateColonyMaintenanceTimes100();
		os << ' ' << city->calculateCorporationMaintenanceTimes100();
		os << ' ' << city->getMaintenanceModifier();
		os << ' ' << city->getOccupationTimer();
		os << ' ' << city->isWeLoveTheKingDay();
		os << ' ' << city->getTradeYield(YieldTypes::YIELD_FOOD);
		os << ' ' << city->getTradeYield(YieldTypes::YIELD_PRODUCTION);
		os << ' ' << city->getTradeYield(YieldTypes::YIELD_COMMERCE);
		os << ' ' << city->getYieldRate(YIELD_PRODUCTION);
		for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
		{
			os << " b=" << city->getBaseCommerceRateTimes100(CommerceTypes(i));
			os << " t=" << city->getTotalCommerceRateModifier(CommerceTypes(i));
			os << " p=" << city->getProductionToCommerceModifier(CommerceTypes(i));
			os << " r=" << city->getCommerceRateTimes100(CommerceTypes(i));
			
		}
		for (int i = 0; i < numBonuses; ++i)
			os << ' ' << city->getNumBonuses(static_cast<BonusTypes>(i));

		os << ' ' << city->calculateNumCitiesMaintenanceTimes100();
		os << ' ' << city->calculateNumCitiesMaintenanceTimes100();
		os << ' ' << city->calculateNumCitiesMaintenanceTimes100();

		os << '\n';
	}

	os << "Imports - Exports:";
	for (int i = 0; i < numBonuses; ++i)
		os << ' ' << player.getBonusImport((BonusTypes)i) - player.getBonusExport((BonusTypes)i);
	os << '\n';


	// Plot groups for dead players seem to hang around...
	if (player.isAlive())
	{
		os << "Plot group state\n";
		dumpPlotGroups(os, player);

		//os << "Union-find result\n";
		//dumpFreshPlotGroups(os, player);
	}

	return std::move(os).str();
}

static void serialisePlot(std::ostream& os, const CvPlot& plot)
{
	os << (int)plot.getOwnerINLINE();
	os << ' ' << (int)plot.getImprovementType();
	os << ' ' << plot.getNumUnits();
}

void heck::diverganceCheckpoint(const char* funcName, std::string str)
{
	const int turnSlice = GC.getGameINLINE().getTurnSlice();

	static int gDebugGameStateLogCounterTurnSlice = -1;
	static int gDebugGameStateLogCounter = 0;

	// Avoid indeterminate state.
	if (gPlotGroupUpdateNesting)
		return;

	if (turnSlice != gDebugGameStateLogCounterTurnSlice)
	{
		gDebugGameStateLogCounterTurnSlice = turnSlice;
		gDebugGameStateLogCounter = 0;
	}

	//{
	//	const PlayerTypes playerI = PlayerTypes(1);
	//	CvPlayer& player = GET_PLAYER(playerI);
	//	if (CvCity* const city = player.getCity(16385))
	//	{
	//		static std::string line;
	//		std::ostringstream newLine;
	//		newLine << "City " << city->getID();
	//		newLine << ' ' << city->getX_INLINE();
	//		newLine << ' ' << city->getY_INLINE();
	//		newLine << ' ' << city->getMaintenanceTimes100();
	//		newLine << ' ' << city->calculateDistanceMaintenanceTimes100();
	//		newLine << ' ' << city->calculateNumCitiesMaintenanceTimes100();
	//		newLine << ' ' << city->calculateColonyMaintenanceTimes100();
	//		newLine << ' ' << city->calculateCorporationMaintenanceTimes100();
	//		newLine << ' ' << city->getMaintenanceModifier();
	//		newLine << ' ' << GET_PLAYER(playerI).calculateInflationRate();
	//		newLine << ' ' << GET_PLAYER(playerI).getCorporationMaintenanceModifier();
	//		
	//		const int numCorps = GC.getNumCorporationInfos();
	//		for (int i = 0; i < numCorps; ++i)
	//			newLine << " C=" << city->isActiveCorporation(CorporationTypes(i));
	//		for (int i = 0; i < numCorps; ++i)
	//		{
	//			if (city->isActiveCorporation(CorporationTypes(i)))
	//			{
	//				newLine << " C$=" << city->calculateCorporationMaintenanceTimes100(CorporationTypes(i));
	//				
	//				int iNumBonuses = 0;
	//				for (int j = 0; j < GC.getNUM_CORPORATION_PREREQ_BONUSES(); ++j)
	//				{
	//					BonusTypes eBonus = (BonusTypes)GC.getCorporationInfo(CorporationTypes(i)).getPrereqBonus(j);
	//					if (NO_BONUS != eBonus)
	//					{
	//						iNumBonuses += city->getNumBonuses(eBonus);
	//					}
	//				}
	//
	//				newLine << ' ' << iNumBonuses;
	//			}
	//		}
	//		newLine << '\n';
	//		
	//
	//		if (newLine.view() != line)
	//		{
	//			std::clog << newLine.view();
	//			line = std::move(newLine).str();
	//		}
	//
	//		const int oldValue = city->getMaintenanceTimes100();
	//		city->updateMaintenance();
	//		const int newValue = city->getMaintenanceTimes100();
	//		if (oldValue == 595 && newValue != oldValue)
	//		{
	//			std::clog << "New value = " << newValue << std::endl;
	//			__debugbreak();
	//		}
	//	}
	//	
	//}

	if (std::pair(kDebugGameStateLogStartTurnSlice, kDebugGameStateLogStartCounter) <= std::pair(turnSlice, gDebugGameStateLogCounter)
		&& std::pair(turnSlice, gDebugGameStateLogCounter) <= std::pair(kDebugGameStateLogFinalTurnSlice, kDebugGameStateLogFinalCounter)
		&& gDebugGameStateLogCounter % kDebugGameStateLogPeriod == 0)
	{
/*#if ENABLE_PlayerTradeNetworkSystem
		if (turnSlice == 291 && gDebugGameStateLogCounter == 124)
		{
			GET_PLAYER(PlayerTypes(1)).getPlayerTradeNetworkSystem()->onUpdateAllPlotGroups();
			std::clog << "Updated all plot groups for player 1.\n";
		}
#endif*/


		std::vector<std::string> playerBlobs(MAX_PLAYERS);
		parallelForEachN(MAX_PLAYERS, [&](int i) { playerBlobs[i] = serialisePlayer(static_cast<PlayerTypes>(i)); });

		//const CvMap& map = GC.getMapINLINE();
		//std::vector<std::string> plotRowBlobs(map.getGridHeight());
		//parallelForEachN(map.getGridHeight(), [&](int y) {
		//	const int w = map.getGridWidthINLINE();
		//	std::ostringstream os;
		//	for (int x = 0; x < w; ++x)
		//	{
		//		serialisePlot(os, *map.plotSorenINLINE(x, y));
		//		os << ';';
		//	}
		//	plotRowBlobs[y] = std::move(os).str();
		//	});

		std::string blob;
		blob.reserve(1000);

		for (const std::string& s : playerBlobs)
			blob += s;
		//for (const std::string& s : plotRowBlobs)
		//{
		//	blob += s;
		//	blob += '\n';
		//}

		const size_t hash = std::hash<std::string_view>()(blob);

		std::clog << "DIVERGANCE CHECKPOINT: " << turnSlice << ' ' << gDebugGameStateLogCounter << ' ' << hash << ": " << funcName << ' ' << str << std::endl;

		// 364 57 
		if (turnSlice == kDebugGameStateLogDetailedLogTurnSlice
			&& kDebugGameStateLogDetailedLogCounterMin <= gDebugGameStateLogCounter && gDebugGameStateLogCounter <= kDebugGameStateLogDetailedLogCounterMax)
		{
			std::clog << blob << std::endl;
/*#if ENABLE_PlayerTradeNetworkSystem
			GET_PLAYER(PlayerTypes(18)).getPlayerTradeNetworkSystem()->dump("DiverganceCheckpoint PlayerTradeNetworkSystem dump.html");
			GET_PLAYER(PlayerTypes(18)).getPlayerTradeNetworkSystem()->verify();
			std::clog << "Verified all plot groups for player 18.\n";
#endif*/
			__debugbreak();
		}
	}

	if (std::pair(turnSlice, gDebugGameStateLogCounter) > std::pair(kDebugGameStateLogFinalTurnSlice, kDebugGameStateLogFinalCounter))
		std::quick_exit(0);

	++gDebugGameStateLogCounter;
}

#endif