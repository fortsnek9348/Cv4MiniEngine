#include <CommonStuff/TradeNetworkSystem.h>
#include <CommonStuff/StatisticsRegistry.h>

#include <iostream>
#include <memory>
#include <random>
#include <fstream>
#include <unordered_map>
#include <string>

using namespace heck;

// Match this with heck::TradeNetworkCore!
constexpr int kBlockSize = 16;

static std::string subscript(int x)
{
	constexpr std::u8string_view t = u8"\u2080";
	std::string s;
	for (const char c : std::to_string(x))
	{
		if ('0' <= c && c <= '9')
		{
			s.append_range(t);
			s.back() += c - '0';
		}
		else
			s += c;
	}
	
	return s;
}

int main()
{
	struct Plot
	{
		bool isTradeNetwork = false;
		std::bitset<8> adjConnect{};
		heck::TradeNetworkCore::EBonus bonus = heck::TradeNetworkCore::kNoBonus;
	};

	struct DataSource : TradeNetworkSystem::IDataSource
	{
		heck::DynamicArray2D<Plot> plots;
		std::unordered_map<ivec2, std::vector<BonusCount>, VectorHasher> cities;

		//virtual bool isCity(ivec2 coord) const override
		//{
		//	return cities.contains(coord);
		//}
		virtual bool isTradeNetwork(ivec2 coord) const override
		{
			return plots[coord].isTradeNetwork;
		}
		virtual bool isTradeNetworkConnected(ivec2 a, ivec2, int adjI) const override
		{
			//if (adjI >= kNumAdj / 2)
			//{
			//	adjI = kInvAdjIndices[adjI];
			//	std::swap(a, b);
			//}

			return plots[a].adjConnect[adjI];
		}
		virtual EBonus getNetworkPlotBonus(ivec2 coord) const override
		{
			return plots[coord].bonus;
		}
		virtual std::vector<BonusCount> getNetworkCityExtraBonuses(ivec2 coord) const override
		{
			const auto it = cities.find(coord);
			if (it != cities.end())
				return it->second;
			else
				return {};
		}
	};

	DataSource map;
	//map.plots = heck::DynamicArray2D<Plot>(ivec2(520, 320) / 10);
	map.plots = heck::DynamicArray2D<Plot>(ivec2(16, 16) * 4);

	const ivec2 dim = map.plots.dim;
	const MapGeometry mapGeom(dim, true, true);

	const auto setAdj = [&](ivec2 coord, int adjI, bool value) {
		if (auto optAdjCoord = mapGeom.tryCanonicalise(coord + kAdj[adjI]))
		{
			if (adjI >= kNumAdj / 2)
			{
				adjI = kInvAdjIndices[adjI];
				std::swap(coord, *optAdjCoord);
			}

			map.plots[coord].adjConnect[adjI] = value;
		}
		};

	
	

	std::mt19937 rng(437638);
	constexpr int kPlotValidChance = 60;
	constexpr int kPlotAdjValidChance = 60;
	constexpr int kPlotCityChance = 10;
	constexpr int kNumBonuses = 4;
	std::uniform_int_distribution<int> percentDist(0, 99);
	std::uniform_int_distribution<int> xDist(0, dim.x - 1);
	std::uniform_int_distribution<int> yDist(0, dim.y - 1);
	std::uniform_int_distribution<int> adjDist(0, kNumAdj - 1);

	const auto genCityExtraBonuses = [&] {
		std::vector<TradeNetworkCore::BonusCount> bonuses(kNumBonuses);
		std::ranges::generate(bonuses, [&] { return std::uniform_int_distribution<int>(0, 100)(rng); });
		return bonuses;
		};

	for (int y = 0; y < dim.y; ++y)
	{
		for (int x = 0; x < dim.x; ++x)
		{
			const ivec2 coord(x, y);
			map.plots[coord].isTradeNetwork = percentDist(rng) < kPlotValidChance;
			for (int i = 0; i < kNumAdj; ++i)
				map.plots[coord].adjConnect[i] = percentDist(rng) < kPlotAdjValidChance;

			if (percentDist(rng) < kPlotCityChance)
			{
				map.cities.emplace(ivec2(x, y), genCityExtraBonuses());
			}

		}
	}

	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			map.plots[{ x, y }].isTradeNetwork = false;

	TradeNetworkSystem sys(mapGeom, kNumBonuses, map);

	for (const auto& [coord, bonuses] : map.cities)
	{
		sys.newCity(coord);
		sys.onUpdateBonusesAtPlot(coord, map);
	}

	//setAdj({ 31, 3 }, 4, true);
	//sys.onUpdatePlotGroupsForPlot({ 31, 3 }, map);

	// Test case 0: 3x3 plots where center can reach all adj, then clear connections to adj.
	// This was a failure case in Cv4MiniEngine.
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			map.plots[{ x, y }].isTradeNetwork = x == 1;
			map.plots[{ x, y }].adjConnect = ~std::bitset<8>();
			sys.onUpdatePlotGroupsForPlot({ x, y }, map);
		}
	}

	for ([[maybe_unused]] const auto cityUpdate : sys.flushCityUpdates())
		;
	sys.verify(map);

	//for (int i = 0; i < 8; ++i)
	//	setAdj({ 1, 1 }, i, false);
	map.plots[{ 1, 1 }].isTradeNetwork = false;
	sys.onUpdatePlotGroupsForPlot({ 1, 1 }, map);

	for ([[maybe_unused]] const auto cityUpdate : sys.flushCityUpdates())
		;
	sys.verify(map);

	//for (int i = 0; i < 1'000'000; ++i)
	for (int i = 0; i < 50'000; ++i)
	{
		const bool debug = i == 47;

		const ivec2 coord(xDist(rng), yDist(rng));
		if (debug)
		{
			std::cout << coord << std::endl;
			sys.dump("TestDynamicGridConnectivity dump 0.html");
		}


		if (percentDist(rng) < kPlotCityChance)
		{
			map.cities.emplace(coord, genCityExtraBonuses());
			sys.newCity(coord);
		}
		else
		{
			map.cities.erase(coord);
			sys.destroyCity(coord);
		}

		setAdj(coord, adjDist(rng), percentDist(rng) < 50);
		if (debug)
			sys.dump("TestDynamicGridConnectivity dump 1.html");
		sys.onUpdatePlotGroupsForPlot(coord, map);
		if (debug)
			sys.dump("TestDynamicGridConnectivity dump 2.html");

		sys.onUpdateBonusesAtPlot(coord, map);

		if (debug)
			sys.dump("TestDynamicGridConnectivity dump 3.html");
		
		for ([[maybe_unused]] const auto cityUpdate : sys.flushCityUpdates())
			;
		sys.verify(map);
	}

	

	

	//map.plotValid[ivec2(0, 2) * kBlockSize + ivec2(6, 4)] = true;
	//map.plotAdjValid[ivec2(0, 2) * kBlockSize + ivec2(6, 4)] = 0b0001'0001;
	//map.plotAdjValid[ivec2(0, 2) * kBlockSize + ivec2(6, 4) - ivec2(1, 1)] |= 0b1000'0000;
	//map.plotAdjValid[ivec2(0, 2) * kBlockSize + ivec2(6, 4) + ivec2(1, 0)] |= 0b0000'1000;

	//Block newBlock(calcBlockRect(map.mapGeom, ivec2(0, 2)), map);
	//const Block oldBlock = blocks[ivec2(0, 2)];


	//dumpSys("TestDynamicGridConnectivity dump.html");

	//sys.verify(map);

	std::cout << StatisticsRegistry::getInstance().buildAllStatsString() << std::endl;

	return 0;
}
