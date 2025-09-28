#include "inc/CommonStuff/TradeNetworkCore.h"

using namespace heck;

using std::to_underlying;

TradeNetworkCore::TradeNetworkCore(ivec2 dimPlots, size_t numBonuses)
	: mNumBonuses(numBonuses)
	, mBPGIndexLookup(dimPlots, kNoBPG)
	, mBonusMap(dimPlots)
	, mZeroBonusesArray(numBonuses)
{
	// max is used for kNoBonus
	// So if max is 255, we support up to 254 bonuses.
	if (numBonuses >= std::numeric_limits<std::underlying_type_t<EBonus>>::max())
		std::abort();
}

// /Revealed/ bonus!
void TradeNetworkCore::setPlotBonus(ivec2 coord, EBonus bonus)
{
	if (mBonusMap[coord] != bonus)
	{
		const EBlockPlotGroup bpgI = mBPGIndexLookup[coord];
		if (bpgI != kNoBPG)
		{
			BlockPlotGroup& bpg = mBPGs[to_underlying(bpgI)];
			addBPGBonusDiff(bpg, mBonusMap[coord], -1);
			addBPGBonusDiff(bpg, bonus, +1);
		}

		mBonusMap[coord] = bonus;
	}
}

void TradeNetworkCore::addBPGBonusDiff(BlockPlotGroup& bpg, EBonus bonus, int change)
{
	if (bonus == kNoBonus || change == 0)
		return;

	if (bpg.bonusCounts.empty())
		bpg.bonusCounts.resize(mNumBonuses);
	bpg.bonusCounts[to_underlying(bonus)] += change;

	if (bpg.cpgI != kNoCPG)
	{
		CivPlotGroup& cpg = mCPGs[to_underlying(bpg.cpgI)];

		if (cpg.bonusCounts.empty())
			cpg.bonusCounts.resize(mNumBonuses);
		cpg.bonusCounts[to_underlying(bonus)] += change;
		dirtyCPGCities(bpg.cpgI);
	}
	// else, cities will be invalidated when added to a CPG.
}


TradeNetworkCore::EBlockPlotGroup TradeNetworkCore::createBPG(i16vec2 blkOrigin)
{
	if (mFreeBPGIndices.empty())
	{
		const EBlockPlotGroup bpgI = static_cast<EBlockPlotGroup>(mBPGs.size());
		mBPGs.push_back({
			.blkOrigin = blkOrigin,
			});
		return bpgI;
	}
	else
	{
		const EBlockPlotGroup bpgI = mFreeBPGIndices.back();
		mFreeBPGIndices.pop_back();
		mBPGs[to_underlying(bpgI)] = {
			.blkOrigin = blkOrigin,
		};
		return bpgI;
	}
}
void TradeNetworkCore::setPlotBPG(ivec2 coord, EBlockPlotGroup bpgI)
{
	const EBonus plotBonus = mBonusMap[coord];
	const auto cityIt = mCities.find(coord);

	{
		// Remove from existing BPG.
		const EBlockPlotGroup prevBpgI = mBPGIndexLookup[coord];

		if (prevBpgI == bpgI)
			return;

		if (prevBpgI != kNoBPG)
		{
			BlockPlotGroup& bpg = mBPGs[to_underlying(prevBpgI)];
			addBPGBonusDiff(bpg, plotBonus, -1);
			// Subtract city bonuses.
			if (cityIt != mCities.end())
			{
				--bpg.numCities;
				for (const auto [bonusI, n] : cityIt->second.extraBonusCounts)
					addBPGBonusDiff(bpg, bonusI, -n);
			}
			[[maybe_unused]] const size_t numErased = std::erase(bpg.localCoords, static_cast<u8vec2>(coord - ivec2(bpg.blkOrigin)));
			assert(numErased == 1);
		}
	}

	// Add to new BPG.
	if (bpgI != kNoBPG)
	{
		BlockPlotGroup& bpg = mBPGs[to_underlying(bpgI)];

		// Add first so that any city at this plot is invalidated next.
		const u8vec2 localCoord = static_cast<u8vec2>(coord - ivec2(bpg.blkOrigin));
		assert(!std::ranges::contains(bpg.localCoords, localCoord));
		bpg.localCoords.push_back(localCoord);

		addBPGBonusDiff(bpg, plotBonus, +1);
		// Add city bonuses.
		if (cityIt != mCities.end())
		{
			++bpg.numCities;
			bpg.allCitiesDirty &= cityIt->second.dirty;
			for (const auto [bonusI, n] : cityIt->second.extraBonusCounts)
				addBPGBonusDiff(bpg, bonusI, +n);
		}
	}

	// BUG FIX: Always invalidate this city.
	if (cityIt != mCities.end())
		dirtyCity(coord);

	mBPGIndexLookup[coord] = bpgI;
}
size_t TradeNetworkCore::getNumPlotsInBPG(EBlockPlotGroup i) const
{
	return i != kNoBPG ? mBPGs[to_underlying(i)].localCoords.size() : 0;
}
TradeNetworkCore::EBlockPlotGroup TradeNetworkCore::getBPGAt(ivec2 coord) const
{
	return mBPGIndexLookup[coord];
}
void TradeNetworkCore::destroyBPG(EBlockPlotGroup bpgI)
{
	if (bpgI != kNoBPG)
	{
		BlockPlotGroup& bpg = mBPGs[to_underlying(bpgI)];

		// Detach from CPG.
		//if (bpg.cpgI != kNoCPG)
		//{
			setBPGCPG(bpgI, kNoCPG);
			// Remove from unassigned list.
			//mUnassignedBPGIndices.pop_back();
		//}
		//else
		//{
		//	// Already unassigned.
		//	//std::erase(mUnassignedBPGIndices, bpgI);
		//}

		// Clear out.
		for (const auto localCoord : bpg.localCoords)
			mBPGIndexLookup[ivec2(bpg.blkOrigin) + ivec2(localCoord)] = kNoBPG;
		bpg = {};

		// Put onto free list.
		assert(!std::ranges::contains(mFreeBPGIndices, bpgI));
		mFreeBPGIndices.push_back(bpgI);
	}
}

TradeNetworkCore::ECivPlotGroup TradeNetworkCore::createCPG()
{
	if (mFreeCPGIndices.empty())
	{
		const auto i = static_cast<ECivPlotGroup>(mCPGs.size());
		mCPGs.emplace_back();
		return i;
	}
	else
	{
		const auto i = mFreeCPGIndices.back();
		mFreeCPGIndices.pop_back();
		mCPGs[to_underlying(i)] = {};
		return i;
	}
}

size_t TradeNetworkCore::getNumActiveCPGs() const
{
	return mCPGs.size() - mFreeCPGIndices.size();
}
size_t TradeNetworkCore::getNumAllocatedCPGs() const
{
	return mCPGs.size();
}

void TradeNetworkCore::setBPGCPG(EBlockPlotGroup bpgI, ECivPlotGroup cpgI)
{
	if (bpgI == kNoBPG)
		return;
	
	auto& bpg = mBPGs[to_underlying(bpgI)];

	if (bpg.cpgI == cpgI)
		return;

	const bool hasBonuses = !bpg.bonusCounts.empty() && bpg.bonusCounts != mZeroBonusesArray;

	if (bpg.cpgI != kNoCPG)
	{
		// Detach from existing CPG.
		auto& cpg = mCPGs[to_underlying(bpg.cpgI)];
		std::erase(cpg.bpgIndices, bpgI);
		// Remove bonuses.
		if (hasBonuses)
		{
			for (auto&& [cpgN, bpgN] : std::views::zip(cpg.bonusCounts, bpg.bonusCounts))
				cpgN -= bpgN;
			dirtyCPGCities(bpg.cpgI);
		}
	}

	if (cpgI != kNoCPG)
	{
		// Add to CPG.
		auto& cpg = mCPGs[to_underlying(cpgI)];
		cpg.bpgIndices.push_back(bpgI);
		cpg.allCitiesDirty &= bpg.allCitiesDirty;
		// Add bonuses.
		if (hasBonuses)
		{
			cpg.bonusCounts.resize(mNumBonuses);
			for (auto&& [cpgN, bpgN] : std::views::zip(cpg.bonusCounts, bpg.bonusCounts))
				cpgN += bpgN;
			dirtyCPGCities(cpgI);
		}
		
	}
	//else
	//	mUnassignedBPGIndices.push_back(bpgI);

	// Always invalidate this BPG's cities.
	dirtyBPGCities(bpgI);

	bpg.cpgI = cpgI;
}
size_t TradeNetworkCore::getNumBPGsInCPG(ECivPlotGroup i) const
{
	return i == kNoCPG ? 0 : mCPGs[to_underlying(i)].bpgIndices.size();
}
TradeNetworkCore::ECivPlotGroup TradeNetworkCore::getCPGOf(EBlockPlotGroup i) const
{
	return i == kNoBPG ? kNoCPG : mBPGs[to_underlying(i)].cpgI;
}
TradeNetworkCore::ECivPlotGroup TradeNetworkCore::getCPGAt(ivec2 coord) const
{
	return getCPGOf(getBPGAt(coord));
}
std::span<const TradeNetworkCore::EBlockPlotGroup> TradeNetworkCore::getBPGsOf(ECivPlotGroup cpgI) const
{
	if (cpgI == kNoCPG)
		return {};
	else
		return mCPGs[to_underlying(cpgI)].bpgIndices;
}
std::vector<i16vec2> TradeNetworkCore::getPlotsOf(ECivPlotGroup cpgI) const
{
	std::vector<i16vec2> plots;
	for (const EBlockPlotGroup bpgI : mCPGs[to_underlying(cpgI)].bpgIndices)
	{
		const BlockPlotGroup& bpg = mBPGs[to_underlying(bpgI)];
		plots.append_range(bpg.localCoords | std::views::transform([&](ivec2 localCoord) { return i16vec2(ivec2(bpg.blkOrigin) + localCoord); }));
	}
	return plots;
}
void TradeNetworkCore::destroyCPG(ECivPlotGroup cpgI)
{
	if (cpgI != kNoCPG)
	{
		dirtyCPGCities(cpgI);

		auto& cpg = mCPGs[to_underlying(cpgI)];
		//mUnassignedBPGIndices.append_range(cpg.bpgIndices);
		cpg = {};
		assert(!std::ranges::contains(mFreeCPGIndices, cpgI));
		mFreeCPGIndices.push_back(cpgI);
	}
}

std::generator<TradeNetworkCore::CityUpdate> TradeNetworkCore::flushCityUpdates()
{
	//const size_t numBonuses = mNumBonuses;
	//std::vector<int> diff(numBonuses);
	for (const ivec2 coord : mDirtyCities)
	{
		CityInfo& city = mCities.find(coord)->second;
		const std::span<const TradeNetworkCore::BonusCount> updatedBonusCounts = getNetworkBonusCountAtPlot(coord);
		//for (size_t i = 0; i < numBonuses; ++i)
		//	diff[i] = updatedBonusCounts[i] - city.cpgBonusCounts[i];
		city.dirty = false;
		city.cpgBonusCounts.assign_range(updatedBonusCounts);
		undirtyCityInBPG(coord);
		//co_yield{ coord, diff };
		co_yield{ coord, updatedBonusCounts };
	}
	mDirtyCities.clear();
}

// Returns bonus counts. Use them!
// Be sure to flush first.
void TradeNetworkCore::newCity(ivec2 coord)
{
	const auto [it, isNew] = mCities.emplace(coord, CityInfo());
	CityInfo& city = it->second;
	if (!city.dirty)
	{
		mDirtyCities.push_back(coord);
		city.dirty = true;
	}
	// City starts with no network input bonuses.
	city.cpgBonusCounts.clear();
	city.cpgBonusCounts.resize(mNumBonuses);
	// city.extraBonusCounts unchanged

	if (isNew)
		if (const EBlockPlotGroup bpgI = getBPGAt(coord); bpgI != kNoBPG)
			++mBPGs[to_underlying(bpgI)].numCities;
}

void TradeNetworkCore::undirtyCityInBPG(ivec2 coord)
{
	if (const EBlockPlotGroup bpgI = getBPGAt(coord); bpgI != kNoBPG)
	{
		auto& bpg = mBPGs[to_underlying(bpgI)];
		bpg.allCitiesDirty = false;
		if (bpg.cpgI != kNoCPG)
		{
			auto& cpg = mCPGs[to_underlying(bpg.cpgI)];
			cpg.allCitiesDirty = false;
		}
	}
}

void TradeNetworkCore::setCityExtraBonuses(ivec2 coord, std::span<const BonusCount> extraBonusCounts)
{
	const auto cityIt = mCities.find(coord);

	if (cityIt == mCities.end())
		return;

	std::vector<std::pair<EBonus, BonusCount>> extraBonusCountsPairs;
	for (size_t i = 0; i < extraBonusCounts.size(); ++i)
		if (extraBonusCounts[i])
			extraBonusCountsPairs.emplace_back(static_cast<EBonus>(i), extraBonusCounts[i]);

	CityInfo& city = cityIt->second;
	if (city.extraBonusCounts != extraBonusCountsPairs)
	{
		if (const EBlockPlotGroup bpgI = getBPGAt(coord); bpgI != kNoBPG)
		{
			// Apply diff to BPG and CPG.
			std::vector<BonusCount> diff(mNumBonuses);
			for (const auto [bonusI, n] : city.extraBonusCounts)
				diff[to_underlying(bonusI)] += n;
			for (size_t i = 0; i < mNumBonuses; ++i)
				diff[i] = (i < extraBonusCounts.size() ? extraBonusCounts[i] : 0) - diff[i];

			if (diff != mZeroBonusesArray)
			{
				auto& bpg = mBPGs[to_underlying(bpgI)];
				for (size_t i = 0; i < mNumBonuses; ++i)
					addBPGBonusDiff(bpg, static_cast<EBonus>(i), diff[i]);
			}
		}

		city.extraBonusCounts = std::move(extraBonusCountsPairs);
	}
}
void TradeNetworkCore::destroyCity(ivec2 coord)
{
	if (const auto it = mCities.find(coord); it != mCities.end())
	{
		// Get rid of extra bonuses.
		setCityExtraBonuses(coord, mZeroBonusesArray);

		if (const EBlockPlotGroup bpgI = getBPGAt(coord); bpgI != kNoBPG)
			--mBPGs[to_underlying(bpgI)].numCities;

		if (it->second.dirty)
			std::erase(mDirtyCities, static_cast<i16vec2>(coord)); // just in case

		mCities.erase(it);
	}
}

[[nodiscard]] bool TradeNetworkCore::hasCityAt(ivec2 coord) const
{
	return mCities.contains(coord);
}

std::span<const TradeNetworkCore::BonusCount> TradeNetworkCore::getCityNetworkBonuses(ivec2 coord) const
{
	if (const auto it = mCities.find(coord); it != mCities.end())
		return it->second.cpgBonusCounts;
	else
		return {};
}

/*
void TradeNetworkCore::compactCPGIndices()
{
	std::ranges::sort(mFreeCPGIndices);

	for (size_t i = 0; i < mFreeCPGIndices.size(); ++i)
	{
		// Trim unused indices at the end.
		while (!mFreeCPGIndices.empty() && to_underlying(mFreeCPGIndices.back()) == mCPGs.size() - 1)
		{
			mFreeCPGIndices.pop_back();
			mCPGs.pop_back();
		}

		if (i >= mFreeCPGIndices.size())
			break;

		// Last CPG is now not free.
		const auto cpgI = mFreeCPGIndices[i];
		mCPGs[to_underlying(cpgI)] = mCPGs.back();
		mCPGs.pop_back();
		// Relabel
		for (const EBlockPlotGroup bpgI : mCPGs[to_underlying(cpgI)].bpgIndices)
			mBPGs[to_underlying(bpgI)].cpgI = cpgI;
	}

	mFreeCPGIndices.clear();
}*/

// Be sure to flush first.
std::span<const TradeNetworkCore::BonusCount> TradeNetworkCore::getNetworkBonusCountAtPlot(ivec2 coord) const
{
	if (const auto cpgI = getCPGAt(coord); cpgI != kNoCPG && !mCPGs[to_underlying(cpgI)].bonusCounts.empty())
		return mCPGs[to_underlying(cpgI)].bonusCounts;
	else
		return mZeroBonusesArray;
}

std::span<const TradeNetworkCore::BonusCount> TradeNetworkCore::getCPGBonusCounts(TradeNetworkCore::ECivPlotGroup cpgI) const
{
	const auto& v = mCPGs[to_underlying(cpgI)].bonusCounts;
	return v.empty() ? mZeroBonusesArray : v;
}

void TradeNetworkCore::dirtyCPGCities(ECivPlotGroup i)
{
	if (i != kNoCPG)
	{
		auto& cpg = mCPGs[to_underlying(i)];
		if (!cpg.allCitiesDirty)
		{
			cpg.allCitiesDirty = true;
			for (const auto bpgI : cpg.bpgIndices)
				dirtyBPGCities(bpgI);
		}
	}
}
void TradeNetworkCore::dirtyBPGCities(EBlockPlotGroup i)
{
	if (i != kNoBPG)
	{
		auto& bpg = mBPGs[to_underlying(i)];
		if (!bpg.allCitiesDirty)
		{
			bpg.allCitiesDirty = true;
			if (bpg.numCities)
				for (const auto localCoord : bpg.localCoords)
					dirtyCity(ivec2(bpg.blkOrigin) + ivec2(localCoord));
		}
	}
}
void TradeNetworkCore::dirtyCity(ivec2 coord)
{
	if (const auto it = mCities.find(coord); it != mCities.end())
	{
		if (!it->second.dirty)
		{
			it->second.dirty = true;
			mDirtyCities.push_back(coord);
		}
	}
}

void TradeNetworkCore::verify(
	std::function<EBonus(ivec2 coord)> getPlotBonus,
	std::function<std::vector<BonusCount>(ivec2 cityCoord)> getCityExtraBonuses
) const
{
	// Verify bonuses
	const ivec2 dim = mBonusMap.dim;
	for (int y = 0; y < dim.y; ++y)
		for (int x = 0; x < dim.x; ++x)
			if (mBonusMap[{ x, y }] != getPlotBonus({ x, y }))
				std::abort();
	
	// Verify city extra bonuses.
	for (const auto& [coord, info] : mCities)
	{
		std::vector<int> extraBonusCounts(mNumBonuses);
		for (const auto [bonusI, n] : info.extraBonusCounts)
			extraBonusCounts[to_underlying(bonusI)] += n;
		if (!std::ranges::equal(extraBonusCounts, getCityExtraBonuses(coord)))
			std::abort();
	}

	// Verify plot assignments.
	// This also verifies that localCoords are unique.
	{
		DynamicArray2D<EBlockPlotGroup> verifyBPGIndexLookup(mBPGIndexLookup.dim, kNoBPG);
		for (size_t i = 0; i < mBPGs.size(); ++i)
		{
			const auto& bpg = mBPGs[i];
			for (const auto localCoord : bpg.localCoords)
			{
				const ivec2 coord = ivec2(bpg.blkOrigin) + ivec2(localCoord);
				if (verifyBPGIndexLookup[coord] != kNoBPG)
					std::abort();
				verifyBPGIndexLookup[coord] = static_cast<EBlockPlotGroup>(i);
			}
		}
		if (verifyBPGIndexLookup != mBPGIndexLookup)
			std::abort();
	}

	// Verify city counts and allCitiesDirty.
	for (size_t i = 0; i < mBPGs.size(); ++i)
	{
		const auto& bpg = mBPGs[i];
		unsigned int cityCount = 0;
		unsigned int dirtyCityCount = 0;
		for (const auto localCoord : bpg.localCoords)
		{
			const ivec2 coord = ivec2(bpg.blkOrigin) + ivec2(localCoord);
			cityCount += mCities.contains(coord);
			dirtyCityCount += mCities.contains(coord) && mCities.at(coord).dirty;
		}
		if (cityCount != bpg.numCities)
			std::abort();
		if (bpg.allCitiesDirty && dirtyCityCount != bpg.numCities)
			std::abort();
	}

	// Verify BPG bonus counts.
	for (size_t i = 0; i < mBPGs.size(); ++i)
	{
		std::vector<BonusCount> verificationBonusCounts(mNumBonuses);
		const auto& bpg = mBPGs[i];
		for (const auto localCoord : bpg.localCoords)
		{
			const ivec2 coord = ivec2(bpg.blkOrigin) + ivec2(localCoord);
			if (const EBonus bonus = getPlotBonus(coord); bonus != kNoBonus)
				++verificationBonusCounts[to_underlying(bonus)];
			if (const auto cityExtraBonuses = getCityExtraBonuses(coord); !cityExtraBonuses.empty())
				for (auto&& [x, y] : std::views::zip(verificationBonusCounts, cityExtraBonuses))
					x += y;
		}
		if (verificationBonusCounts != bpg.bonusCounts)
			// If BPG array is empty, then it can mean zero bonuses.
			if (!bpg.bonusCounts.empty() || verificationBonusCounts != mZeroBonusesArray)
				std::abort();
	}

	// Verify CPG BPG membership.
	std::vector<ECivPlotGroup> cpgIndices(mBPGs.size(), kNoCPG);
	for (size_t i = 0; i < mCPGs.size(); ++i)
	{
		const auto& cpg = mCPGs[i];
		for (const auto bpgI : cpg.bpgIndices)
		{
			if (cpgIndices[to_underlying(bpgI)] != kNoCPG)
				std::abort();
			cpgIndices[to_underlying(bpgI)] = static_cast<ECivPlotGroup>(i);
		}
	}
	if (!std::ranges::equal(cpgIndices, mBPGs, std::equal_to<>(), {}, &BlockPlotGroup::cpgI))
		std::abort();

	// Verify CPG bonus counts.
	for (size_t i = 0; i < mCPGs.size(); ++i)
	{
		std::vector<BonusCount> verificationBonusCounts(mNumBonuses);
		const auto& cpg = mCPGs[i];
		for (const auto bpgI : cpg.bpgIndices)
		{
			const auto& bpg = mBPGs[to_underlying(bpgI)];
			for (auto&& [x, y] : std::views::zip(verificationBonusCounts, bpg.bonusCounts))
				x += y;
		}
		if (verificationBonusCounts != cpg.bonusCounts)
			// If CPG array is empty, then it can mean zero bonuses.
			if (!cpg.bonusCounts.empty() || verificationBonusCounts != mZeroBonusesArray)
				std::abort();
	}

	// Check that free BPGs are not in use.
	for (const auto i : mFreeBPGIndices)
		if (cpgIndices[to_underlying(i)] != kNoCPG)
			std::abort();

	// Check that free CPGs are empty.
	for (const auto i : mFreeCPGIndices)
		if (!mCPGs[to_underlying(i)].bpgIndices.empty())
			std::abort();

	// Check dirty cities.
	{
		size_t numDirtyCities = 0;
		for (const CityInfo& city : mCities | std::views::values)
			numDirtyCities += city.dirty;
		if (numDirtyCities != mDirtyCities.size())
			std::abort();
		for (const auto coord : mDirtyCities)
			if (!mCities.at(coord).dirty)
				std::abort();
	}

	// Check cities are flushed.
	for (const auto& [coord, city] : mCities)
	{
		const auto& cityBonusCounts = city.cpgBonusCounts;
		const auto& cpgBonusCounts = getNetworkBonusCountAtPlot(coord);
		if (!std::ranges::equal(cityBonusCounts.empty() ? mZeroBonusesArray : cityBonusCounts, cpgBonusCounts))
			std::abort();
	}
}
