#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValuePlayerNearestCityCache.h"
#include "FoundValueVectorisedCalculation.h"
#include "FoundValueCommonCache.h"

#include "../../CvCity.h"
#include "../../CvPlayerAI.h"

#include <CommonStuff/ParallelExt.h>

#include <iostream>

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;
using namespace GiganticMapsOptimisationsLib;

PlayerNearestCityCache::PlayerNearestCityCache(CivMapGeometry mapGeom, const AreaMap& areaMap, PlayerTypes filterPlayerI)
    : mFilterPlayerI(filterPlayerI)
	, mMapGeom(mapGeom)
    , mGlobalCityDistanceField(mapGeom, iaabb2{ .max = mapGeom.dim })
	, mDistanceValueField(mapGeom, 0, kNoCities)
{
	for (const auto [areaId, unwrappedRect] : areaMap.unwrappedAreaRectsByAreaId)
		mAreaCityDistanceFields.emplace(areaId, CityDistanceField(mapGeom, unwrappedRect));

	CvMap& map = GC.getMapINLINE();
	for (const i16vec2 coord : gatherCityLocations(filterPlayerI))
	{
		mGlobalCityDistanceField.setIsCity(coord, true);
		mAreaCityDistanceFields.at(map.plot(coord.x, coord.y)->getArea()).setIsCity(coord, true);
	}

	if (filterPlayerI != NO_PLAYER)
		mCapital = GET_PLAYER(filterPlayerI).getCapitalCity();

	mIsDirty = true;
}

void PlayerNearestCityCache::onAfterCityCreated(const CvPlot& plot, PlayerTypes playerI)
{
	if (mFilterPlayerI == NO_PLAYER || playerI == mFilterPlayerI)
	{
		mIsDirty = true;
		const ivec2 coord = getPlotCoord(plot);
		mGlobalCityDistanceField.setIsCity(coord, true);
		mAreaCityDistanceFields.at(plot.getArea()).setIsCity(coord, true);
	}
}

void PlayerNearestCityCache::onAfterCityDestroyed(const CvPlot& plot, PlayerTypes playerI)
{
	if (mFilterPlayerI == NO_PLAYER || playerI == mFilterPlayerI)
	{
		mIsDirty = true;

		const ivec2 coord = getPlotCoord(plot);
		mGlobalCityDistanceField.setIsCity(coord, false);
		mAreaCityDistanceFields.at(plot.getArea()).setIsCity(coord, false);
	}
}

void PlayerNearestCityCache::onCapitalChanged(const CvCity* city)
{
	mCapital = city;
	mIsDirty = true;
}

static uint16_t tieBreakCapital(const CivMapGeometry& mapGeom, heck::ivec2 coord, heck::ivec2 nearest, const CvCity* capitalCity, heck::ivec2 capitalCoord, bool isSettledArea)
{
	const int cityDist = mapGeom.plotDistance(coord, nearest);
	if (capitalCity)
	{
		const int capitalDist = mapGeom.plotDistance(coord, capitalCoord);
		if (cityDist == capitalDist/* && nearest != capitalCoord*/) [[unlikely]]
		{
			CvMap& map = GC.getMapINLINE();
			const CvPlot& nearestPlot = *map.plotINLINE(nearest.x, nearest.y);

			// If the area is settled, then the capital must be on the same area as the query plot (which the nearest city is also on).
			if (!isSettledArea || capitalCity->getArea() == nearestPlot.getArea())
			{
				// CvMap::findCity: if (iValue < iBestValue). So in the case of ties, the earlier city in the array is returned.
				const int nearestId = nearestPlot.getPlotCity()->getID();
				const int capitalId = map.plotINLINE(capitalCoord.x, capitalCoord.y)->getPlotCity()->getID();
				return uint16_t((nearestId & FLTA_INDEX_MASK) < (capitalId & FLTA_INDEX_MASK) ? cityDist : capitalDist | kNearestCityIsCapitalFlag);
			}
			// else, capital is not searched by findCity due to area filter.
		}
	}
	
	//return uint16_t(cityDist | (nearest == capitalCoord ? kNearestCityIsCapitalFlag : 0));
	return uint16_t(cityDist);
}

void PlayerNearestCityCache::update()
{
	if (!mIsDirty)
		return;
	
	mIsDirty = false;

	bool changed = mGlobalCityDistanceField.update();
	for (CityDistanceField& field : mAreaCityDistanceFields | std::views::values)
		changed |= field.update();

	if (changed)
	{
		const ivec2 capitalCoord = mCapital ? getPlotCoord(*mCapital->plot()) : i16vec2(-1);
		// Apply updates to input array.
		if ((mFilterPlayerI == NO_PLAYER ? GC.getGameINLINE().getNumCities() : GET_PLAYER(mFilterPlayerI).getNumCities()) > 0)
		{
			// TODO: This is probably worth vectorising.
			heck::parallelForEachN(mMapGeom.dim.y, [mapGeom = mMapGeom, capitalCoord, filterPlayerI = mFilterPlayerI, &map = GC.getMapINLINE(), this](int y) {
			//heck::serialForEachN(mMapGeom.dim.y, [mapGeom = mMapGeom, capitalCoord, filterPlayerI = mFilterPlayerI, &map = GC.getMapINLINE(), this](int y) {
				const CvArea* prevArea = nullptr;
				const CityDistanceField* prevAreaField{};
				for (const int x : range(mapGeom.dim.x))
				{
					if (const CvArea* const area = map.plotINLINE(x, y)->area())
					{
						const bool isSettledArea = (filterPlayerI == NO_PLAYER ? area->getNumCities() : area->getCitiesPerPlayer(filterPlayerI)) > 0;
						if (area != prevArea && isSettledArea)
						{
							prevArea = area;
							prevAreaField = &mAreaCityDistanceFields.at(area->getID());
						}

						if (const std::optional<i16vec2> nearestCityCoord = (isSettledArea ? *prevAreaField : mGlobalCityDistanceField).tryGetNearestCityAt({ x, y }))
							mDistanceValueField.set({ x, y }, tieBreakCapital(mapGeom, { x, y }, *nearestCityCoord, mCapital, capitalCoord, isSettledArea));
						else
							mDistanceValueField.set({ x, y }, kNoCities);
					}
				}
				});
		}
		else
			mDistanceValueField.fill(kNoCities);
	}
}
void PlayerNearestCityCache::verify() const
{
	if (mIsDirty)
		std::abort();

	const std::vector<i16vec2> cityCoords = gatherCityLocations(mFilterPlayerI);
	mGlobalCityDistanceField.verify(cityCoords);
	CvMap& map = GC.getMapINLINE();
	int itIndex{};
	for (const CvArea* area = map.firstArea(&itIndex); area; area = map.nextArea(&itIndex))
		if (const auto it = mAreaCityDistanceFields.find(area->getID()); it != mAreaCityDistanceFields.end())
			it->second.verify(cityCoords | std::views::filter([area, &map](ivec2 coord) { return map.plot(coord.x, coord.y)->area() == area; }) | std::ranges::to<std::vector>());

	mDistanceValueField.verify();

	// Verify that mDistanceValueField matches mAreaCityDistanceFields and mGlobalCityDistanceField.
	const ivec2 capitalCoord = mCapital ? getPlotCoord(*mCapital->plot()) : i16vec2(-1);
	heck::parallelForEachN(mMapGeom.dim.y, [mapGeom = mMapGeom, capitalCoord, filterPlayerI = mFilterPlayerI, &map = GC.getMapINLINE(), this](int y) {
		for (const int x : range(mapGeom.dim.x))
		{
			if (const CvArea* const area = map.plot(x, y)->area())
			{
				const bool isSettledArea = (filterPlayerI == NO_PLAYER ? area->getNumCities() : area->getCitiesPerPlayer(filterPlayerI)) > 0;
				const std::optional<i16vec2> nearestCityCoord = (isSettledArea ? mAreaCityDistanceFields.at(area->getID()) : mGlobalCityDistanceField).tryGetNearestCityAt({ x, y });
				if (nearestCityCoord)
				{
					if (mDistanceValueField.get({ x, y }) != tieBreakCapital(mapGeom, { x, y }, *nearestCityCoord, mCapital, capitalCoord, isSettledArea))
						std::abort();

					// Sanity check.
					if (mDistanceValueField.get({ x, y }) & kNearestCityIsCapitalFlag)
						if ((mapGeom.plotDistance({ x, y }, capitalCoord) | kNearestCityIsCapitalFlag) != mDistanceValueField.get({ x, y }))
							std::abort();
				}
				else
				{
					if (mDistanceValueField.get({ x, y }) != kNoCities)
						std::abort();
				}
			}
		}
		});

	//if (mFilterPlayerI == 4)
	//{
	//	const ivec2 testCoord{ 72, 48 };
	//	const CvCity* const pNearestCity = GC.getMapINLINE().findCity(testCoord.x, testCoord.y, mFilterPlayerI);
	//	const int dist = mMapGeom.plotDistance(testCoord, getPlotCoord(*pNearestCity->plot()));
	//	const int capDist = mMapGeom.plotDistance(testCoord, capitalCoord);
	//	const bool isCapital = pNearestCity == mCapital;
	//	const uint16_t expectedDistanceValueFieldBits = dist | (isCapital ? kNearestCityIsCapitalFlag : 0);
	//	const int cachedValue = mDistanceValueField.get(testCoord);
	//
	//	std::clog << "Nearest city at " << getPlotCoord(*pNearestCity->plot()) << " (area = " << pNearestCity->getArea() << ')' << std::endl;
	//	std::clog << "Capital at " << capitalCoord << " (area = " << mCapital->getArea() << ')' << std::endl;
	//	std::clog << "Nearest city ID " << pNearestCity->getID() << std::endl;
	//	std::clog << "Capital ID " << mCapital->getID() << std::endl;
	//	std::clog << "Nearest city index " << (pNearestCity->getID() & FLTA_INDEX_MASK) << std::endl;
	//	std::clog << "Capital index " << (mCapital->getID() & FLTA_INDEX_MASK) << std::endl;
	//	std::clog << "dist = " << dist << std::endl;
	//	std::clog << "capDist = " << capDist << std::endl;
	//	std::clog << "isCapital = " << isCapital << std::endl;
	//	std::clog << "expectedDistanceValueFieldBits = " << expectedDistanceValueFieldBits << std::endl;
	//	std::clog << "cachedValue = " << cachedValue << std::endl;
	//
	//	if (expectedDistanceValueFieldBits != cachedValue)
	//		std::abort();
	//}
}

const PaddedArray2D<uint16_t>& PlayerNearestCityCache::getDistanceValueField() const
{
	return mDistanceValueField;
}

#endif