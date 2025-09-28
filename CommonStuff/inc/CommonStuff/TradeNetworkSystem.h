#pragma once

#include "TradeNetworkCore.h"
#include "MapGeometry.h"

#include "adj.h"
#include "Array2D.h"
#include "Bitmap2D.h"
#include "MapGeometry.h"
#include "UnionFind.h"

#include <unordered_set>

namespace heck
{
	// Agnostic implementation of trade network plot group management.
	// Feed it with update events and then flush the city changes afterwards.
	class TradeNetworkSystem
	{
	public:
		using EBonus = TradeNetworkCore::EBonus;

		struct IDataSource
		{
			using EBonus = TradeNetworkCore::EBonus;
			using BonusCount = TradeNetworkCore::BonusCount;

			//[[nodiscard]] virtual bool isCity(ivec2 coord) const = 0;
			[[nodiscard]] virtual bool isTradeNetwork(ivec2 coord) const = 0;
			[[nodiscard]] virtual bool isTradeNetworkConnected(ivec2 a, ivec2 b, int adjI) const = 0;
			[[nodiscard]] virtual EBonus getNetworkPlotBonus(ivec2 coord) const = 0;
			[[nodiscard]] virtual std::vector<BonusCount> getNetworkCityExtraBonuses(ivec2 coord) const = 0;

			virtual ~IDataSource() = default;
		};

		explicit TradeNetworkSystem(MapGeometry mapGeom, size_t numBonuses, const IDataSource& dataSource);

		
		// CvPlayer::updatePlotGroups
		void onUpdateAllPlotGroups(const IDataSource& dataSource);
		// CvPlot::updatePlotGroup
		// Called when plot groups may have been connected or disconnected.
		void onUpdatePlotGroupsForPlot(ivec2 coord, const IDataSource& dataSource);
		// Call when any of these change: revealed bonus, CvCity::getFreeBonus, getBonusImport/Export at the capital.
		// Or rather, call inside CvPlot::updatePlotGroupBonus.
		// Also call when owner changes, when capital changes, and when a owner city is created or destroyed at this plot.
		void onUpdateBonusesAtPlot(ivec2 coord, const IDataSource& dataSource);

		// New city at the plot starting with zero network bonuses.
		// Flush to get bonus update.
		// `getNetworkCityExtraBonuses` does not change here. Call `onUpdateBonusesAtPlot` after this.
		void newCity(ivec2 coord);
		std::span<const TradeNetworkCore::BonusCount> getCityNetworkBonuses(ivec2 coord) const;
		void destroyCity(ivec2 coord);

		// Be sure to call this!
		[[nodiscard]] std::generator<TradeNetworkCore::CityUpdate> flushCityUpdates();

		bool isInPlotGroup(ivec2 coord) const;
		bool isSamePlotGroup(ivec2 a, ivec2 b) const;
		int getPlotGroupNumBonuses(ivec2 coord, EBonus) const;
		int getPlotGroupHasBonus(ivec2 coord, EBonus) const;

		//void compactCPGIndices();
		size_t getNumActiveCPGs() const;
		size_t getNumAllocatedCPGs() const;
		bool isFreeCpgIndex(TradeNetworkCore::ECivPlotGroup) const;
		std::vector<i16vec2> getPlotsOfCPG(TradeNetworkCore::ECivPlotGroup) const;
		std::span<const int> getCPGBonusCounts(TradeNetworkCore::ECivPlotGroup) const;

		// For testing.
		TradeNetworkCore::EBlockPlotGroup getBPGAt(ivec2 coord) const;
		TradeNetworkCore::ECivPlotGroup getCPGAt(ivec2 coord) const;

		void dump(const char* filename) const;

		void verify(const IDataSource& dataSource) const;

	private:
		
		using EBlockPlotGroup = TradeNetworkCore::EBlockPlotGroup;
		using ECivPlotGroup = TradeNetworkCore::ECivPlotGroup;
		static constexpr auto kNoBPG = TradeNetworkCore::kNoBPG;
		static constexpr auto kNoCPG = TradeNetworkCore::kNoCPG;

		struct [[nodiscard]] BPGLink
		{
			EBlockPlotGroup from{};
			EBlockPlotGroup to{};
		};

		struct [[nodiscard]] Block
		{
			std::vector<EBlockPlotGroup> bpgs;

			//std::array<std::vector<BPGLink>, kNumAdj> adjLinks;
		};

		heck::MapGeometry mMapGeom;
		heck::MapGeometry mMapBlocksGeom;

		heck::DynamicBitmap2D mIsTradeNetworkCache;
		heck::DynamicArray2D<uint8_t> mIsTradeNetworkConnectedCache;

		TradeNetworkCore mCore;

		heck::DynamicArray2D<Block> mBlocks;

		std::unordered_multimap<EBlockPlotGroup, EBlockPlotGroup> mAdjacencyLookup;
		std::unordered_multimap<EBlockPlotGroup, EBlockPlotGroup> mRevAdjacencyLookup;
		//std::unordered_multimap<EBlockPlotGroup, decltype(mAdjacencyLookup)::iterator> mAdjacencyBPGRefsLookup;

		// Removes the bonus count from cities.
		void destroyAllCvPlotGroups();

		enum class [[nodiscard]] EBlockUpdateStatus
		{
			NoChange,
			Trivial,
			Failed,
			General,
		};

		struct BlockUpdateEntry
		{
			heck::ivec2 blkCoord{};
			bool bordersDirty = false;
		};

		void update(std::optional<heck::ivec2> invalidatedMapCoord, const IDataSource& dataSource);
		std::vector<BlockUpdateEntry> updateBlocks(std::optional<heck::ivec2> invalidatedMapCoord, const IDataSource& dataSource);
		void updateBlockConnections(std::span<const BlockUpdateEntry>);
		void updateCPGs(std::span<const BlockUpdateEntry> updatedBlkList);

		void updateWholeBitmap(std::vector<heck::ivec2>* dirtyBlkCoordsOut, const IDataSource& dataSource);
		// Returns true iff something changed.
		[[nodiscard]] bool updatePlotBits(heck::ivec2 mapCoord, const IDataSource& dataSource);
		[[nodiscard]] bool updateAdjPlotBits(heck::ivec2 mapCoord, const IDataSource& dataSource);

		[[nodiscard]] bool isTradeNetworkAndIsConnected(heck::ivec2 mapCoord, int adjI) const;

		heck::ivec2 getBlockDim(heck::ivec2 blkCoord) const;
		bool isOnBlockBorder(heck::ivec2 mapCoord) const;

		EBlockUpdateStatus updateBlock(heck::ivec2 blkCoord, std::optional<heck::ivec2> optMapCoord);


		EBlockUpdateStatus rebuildBlock(heck::ivec2 blkCoord);
		Block buildBlock(heck::ivec2 blkCoord);
		EBlockUpdateStatus tryBlockTrivialUpdate(heck::ivec2 blkCoord, heck::ivec2 localCoord);
		//EBlockUpdateStatus tryBlockTrivialUpdateCaseRemoveFromBPG(heck::ivec2 blkCoord, heck::ivec2 localCoord);
		//EBlockUpdateStatus tryBlockTrivialUpdateCaseAddToBPG(heck::ivec2 blkCoord, heck::ivec2 localCoord);
		void deleteBPG(ivec2 blkCoord, EBlockPlotGroup bpgI);

		//void addOneWayAdj(EBlockPlotGroup, EBlockPlotGroup);
		//void delAdj(EBlockPlotGroup, EBlockPlotGroup);
		//void delOneWayAdj(EBlockPlotGroup, EBlockPlotGroup);
		void delBPGAdj(EBlockPlotGroup);
		void delBlockAdj(heck::ivec2 blkCoord);
		auto getAdj(EBlockPlotGroup a) const;
		
		// Returns true iff changed.
		[[nodiscard]] bool updateInterBlockConnections(heck::ivec2 blkCoord);
		// Returns true iff changed.
		[[nodiscard]] bool updateInterBlockConnections(heck::ivec2 blkCoord, int adjI);

		void doLocalCPGUpdates(std::span<const heck::ivec2> localBlkList, std::span<const heck::ivec2> ringBlkList, std::unordered_set<ECivPlotGroup>& dirtyCPGs);
		void doGlobalCPGUpdates(std::span<const heck::ivec2> localBlkList, std::unordered_set<ECivPlotGroup>& dirtyCPGs);
	};

}