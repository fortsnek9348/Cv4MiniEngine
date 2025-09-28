#pragma once

#include "Array2D.h"
#include "Bitmap2D.h"
#include "vec.h"

#include <optional>
#include <vector>
#include <span>
#include <functional>
#include <unordered_map>
#include <generator>

namespace heck
{
	// The core of the player trade network system. Handles bonus distribution within plot groups.
	// Contains the bonus change craziness to within a single class.
	// What we have here is that plots produce bonuses and cities consume them, according to plot groups.
	// Firaxis code meticulously keeps cities up to date with plot groups. This is very bug prone when rewriting the plot group system, so we contain the problem here.
	//
	// Plot group management will organise plot groups into BlockPlotGroups and CivPlotGroups.
	// CivPlotGroups are groups of BlockPlotGroups. BlockPlotGroups are groups of plots, and are contained within one block.
	// Changes in groups change the bonuses available at cities.
	// Changes in city bonuses will be deferred until the caller flushes, where the caller will then update CvCity instances.
	//
	// NOTE: This class has no dependency on Civ code.
	class TradeNetworkCore
	{
	public:
		enum class [[nodiscard]] EBonus : uint8_t;
		enum class [[nodiscard]] ECivPlotGroup : unsigned int;
		enum class [[nodiscard]] EBlockPlotGroup : unsigned int;

		static constexpr EBonus kNoBonus = static_cast<EBonus>(-1);
		static constexpr EBlockPlotGroup kNoBPG = static_cast<EBlockPlotGroup>(-1);
		static constexpr ECivPlotGroup kNoCPG = static_cast<ECivPlotGroup>(-1);

		using BonusCount = int;

		explicit TradeNetworkCore(ivec2 dimPlots, size_t numBonuses);

		// /Revealed/ bonus!
		void setPlotBonus(ivec2 coord, EBonus bonus);

		EBlockPlotGroup createBPG(i16vec2 blkOrigin);
		// Plots must be within [blkOrigin..blkOrigin+256).
		void setPlotBPG(ivec2 coord, EBlockPlotGroup);
		[[nodiscard]] size_t getNumPlotsInBPG(EBlockPlotGroup) const;
		EBlockPlotGroup getBPGAt(ivec2 coord) const;
		void destroyBPG(EBlockPlotGroup);

		ECivPlotGroup createCPG();
		size_t getNumActiveCPGs() const;
		size_t getNumAllocatedCPGs() const;
		void setBPGCPG(EBlockPlotGroup, ECivPlotGroup);
		[[nodiscard]] size_t getNumBPGsInCPG(ECivPlotGroup) const;
		ECivPlotGroup getCPGOf(EBlockPlotGroup) const;
		ECivPlotGroup getCPGAt(ivec2 coord) const;
		std::span<const EBlockPlotGroup> getBPGsOf(ECivPlotGroup) const;
		std::vector<i16vec2> getPlotsOf(ECivPlotGroup) const;
		void destroyCPG(ECivPlotGroup);

		struct CityUpdate
		{
			ivec2 cityCoord;
			std::span<const int> bonusCounts;
		};

		// Coroutine!
		[[nodiscard]] std::generator<CityUpdate> flushCityUpdates();

		// Returns current CPG bonus counts. Use them!
		// City will be added to dirty list and you'll get its bonuses in flushCityBonusChanges.
		void newCity(ivec2 coord);
		void setCityExtraBonuses(ivec2 coord, std::span<const BonusCount> extraBonusCounts);
		void destroyCity(ivec2 coord);
		[[nodiscard]] bool hasCityAt(ivec2 coord) const;

		std::span<const BonusCount> getCityNetworkBonuses(ivec2 coord) const;

		//void compactCPGIndices();
		std::span<const BonusCount> getNetworkBonusCountAtPlot(ivec2 coord) const;
		std::span<const BonusCount> getCPGBonusCounts(TradeNetworkCore::ECivPlotGroup) const;

		void verify(
			std::function<EBonus(ivec2 coord)> getPlotBonus,
			std::function<std::vector<BonusCount>(ivec2 cityCoord)> getCityExtraBonuses
		) const;

	private:
		struct BlockPlotGroup
		{
			i16vec2 blkOrigin{};
			ECivPlotGroup cpgI = kNoCPG;
			std::vector<u8vec2> localCoords{};
			std::vector<BonusCount> bonusCounts{};
			bool allCitiesDirty = false;
			unsigned int numCities = 0;
		};

		struct CivPlotGroup
		{
			std::vector<EBlockPlotGroup> bpgIndices{};
			std::vector<BonusCount> bonusCounts{};
			bool allCitiesDirty = false;
		};

		struct CityInfo
		{
			bool dirty = false;
			std::vector<std::pair<EBonus, BonusCount>> extraBonusCounts{};
			std::vector<BonusCount> cpgBonusCounts{};
		};

		size_t mNumBonuses{};

		DynamicArray2D<EBlockPlotGroup> mBPGIndexLookup; // 650KB for 520x320
		DynamicArray2D<EBonus> mBonusMap; // 162.5KB for 520x320

		std::vector<BlockPlotGroup> mBPGs;
		std::vector<CivPlotGroup> mCPGs;

		std::vector<EBlockPlotGroup> mFreeBPGIndices;
		std::vector<ECivPlotGroup> mFreeCPGIndices;

		std::unordered_map<ivec2, CityInfo, VectorHasher> mCities;
		std::vector<i16vec2> mDirtyCities;

		std::vector<BonusCount> mZeroBonusesArray;

		void addBPGBonusDiff(BlockPlotGroup& bpg, EBonus bonus, int change);

		void dirtyCPGCities(ECivPlotGroup);
		void dirtyBPGCities(EBlockPlotGroup);
		void dirtyCity(ivec2 coord);
		void undirtyCityInBPG(ivec2 coord);
	};
}