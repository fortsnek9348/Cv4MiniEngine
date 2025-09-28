#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "PathDistanceSimple.h"

#include "../CvDLLFAStarIFaceBase.h"
#include "../CvDLLUtilityIFaceBase.h"
#include "../FAStarNode.h"

#include <random>
#include <iostream>

using namespace GiganticMapsOptimisationsLib;

PathDistanceSimple::PathDistanceSimple(const CvMap& map) : mMapGeom(map), mBitmap(mMapGeom.dim)
{
	for (const int y : range(mMapGeom.dim.y))
		for (const int x : range(mMapGeom.dim.x))
		{
			const CvPlot& plot = *map.plotINLINE(x, y);

			// areaValid
			// stepValid
			// TODO: You might possibly need to handle water plots with advanced start. Otherwise, all queries are on land.
			mBitmap[{ x, y }] = plot.isWater() || plot.isImpassable();
		}
}

std::optional<int> PathDistanceSimple::findPathLength(ivec2 start, ivec2 goal) const
{
	const CivMapGeometry mapGeom = mMapGeom;

	const auto obstacle = [mapGeom, this](ivec2 coord) {
		if (!mapGeom.isValidCoord(coord))
			return true;
		return mBitmap[mapGeom.wrapCoord(coord)];
		};

	if (obstacle(start) || obstacle(goal))
		return std::nullopt;

	if (start == goal)
		return 0;

	TwoLevelLookup<int, INT_MAX> gcosts;

	gcosts[start] = 0;
	
	int queueBaseHeapValue = mapGeom.stepDistance(start, goal);
	std::array<std::vector<i16vec2>, 3> queues;
	queues[0].push_back(start);

	for (;;)
	{
		for ([[maybe_unused]] const int i : range((int)queues.size() - 1))
		{
			if (queues[0].empty())
			{
				++queueBaseHeapValue;
				std::ranges::rotate(queues, queues.begin() + 1);
			}
			else
				break;
		}

		if (queues[0].empty())
			break;

		const ivec2 fromCoord = queues[0].back();
		const int fromF = queueBaseHeapValue;
		queues[0].pop_back();

		if (fromCoord == goal)
			return fromF;

		const int fromG = fromF - mapGeom.stepDistance(fromCoord, goal);
		const int succG = fromG + 1;

		for (const ivec2 adjD : kAdj)
		{
			const ivec2 unwrappedCoord = ivec2(fromCoord) + adjD;
			if (!obstacle(unwrappedCoord))
			{
				const ivec2 succ = mapGeom.wrapCoord(unwrappedCoord);
				auto& succGStored = gcosts[succ];
				if (succG < succGStored)
				{
					succGStored = succG;
					const int succH = mapGeom.stepDistance(succ, goal);

					const int succF = succG + succH;
					assert(queueBaseHeapValue <= succF && succF < queueBaseHeapValue + (int)queues.size());
					queues[std::clamp(succF - queueBaseHeapValue, 0, (int)queues.size() - 1)].push_back(succ); // clamp just in case
				}
			}
		}
	}

	return std::nullopt;

}

void PathDistanceSimple::verify() const
{
	Clock::duration totalFAStarTime{};
	Clock::duration totalSimpleAStarTime{};

	const auto verifyPath = [&, this](ivec2 start, ivec2 goal) {
		const auto t0 = Clock::now();
		const bool trueFoundPath = gDLL->getFAStarIFace()->GeneratePath(&GC.getStepFinder(), start.x, start.y, goal.x, goal.y, false, 0, false);
		const int trueLength = trueFoundPath ? gDLL->getFAStarIFace()->GetLastNode(&GC.getStepFinder())->m_iData1 : INT_MAX;
		const auto t1 = Clock::now();
		const int simpleLength = findPathLength(start, goal).value_or(INT_MAX);
		const auto t2 = Clock::now();

		if (trueLength != simpleLength)
			return std::abort();

		totalFAStarTime += t1 - t0;
		totalSimpleAStarTime += t2 - t1;
		};

	// Gather up plots of the largest area.
	std::vector<i16vec2> plotList;
	CvMap& map = GC.getMapINLINE();
	plotList.reserve(map.numPlotsINLINE());
	const CvArea* const area = map.findBiggestArea(false);
	const int areaId = area->getID();
	for (const int y : range(mMapGeom.dim.y))
		for (const int x : range(mMapGeom.dim.x))
		{
			const CvPlot& plot = *map.plotINLINE(x, y);
			if (plot.getArea() == areaId && !plot.isImpassable())
				plotList.emplace_back(ivec2(x, y));
		}

	std::mt19937_64 gen{ 9387348 };
	std::ranges::shuffle(plotList, gen);

	size_t i = 0;
	for (const auto p : plotList | std::views::slide(2) | std::views::take(200))
	{
		if (i % 100 == 0)
			std::cout << "Simple " << i << std::endl;
		++i;
		//std::cout << p[0] << ' ' << p[1] << std::endl;
		verifyPath(p[0], p[1]);
	}

	std::clog << "Simple path distance test successful." << std::endl;
	std::clog << "Total FAStar time: " << Seconds(totalFAStarTime) << std::endl;
	std::clog << "Total Simple A* time      : " << Seconds(totalSimpleAStarTime) << std::endl;
}

#endif