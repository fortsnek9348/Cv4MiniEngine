#include "../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "WholeMapSimpleDijkstra.h"
#include "Util.h"
#include "UnitPathingUtil.h"

#include <CommonStuff/ParallelExt.h>

#include "../CvSelectionGroup.h"

using namespace GiganticMapsOptimisationsLib;

struct WholeMapSimpleDijkstra::Internals
{
	heck::MapGeometry mapGeom;
	//DynamicBitmap2D validBitmap;
	//DynamicBitmap2D destValidBitmap;

	// Steps clamped to UINT16_MAX. Only extreme maps would need more than this, right? Watch out for gigantic mazes...
	// Although, at UINT16_MAX steps, a 10-move unit would need 6.5K turns to reach it.
	DynamicArray2D<uint16_t> steps;

	std::vector<heck::i16vec2> queue0;
	std::vector<heck::i16vec2> queue1;

	const CvSelectionGroup* theGroup = nullptr;
	uint32_t theFlags = 0;

	Internals() : Internals(CivMapGeometry(GC.getMapINLINE()))
	{
	}

	Internals(CivMapGeometry mapGeom)
		: mapGeom(mapGeom)
		//, validBitmap(mapGeom.dim)
		//, destValidBitmap(mapGeom.dim)
		, steps(mapGeom.dim, UINT16_MAX)
	{
	}

	void visitAllReachable(const CvSelectionGroup& group, heck::ivec2 start, uint32_t flags)
	{
		const CvMap& map = GC.getMapINLINE();

		theGroup = &group;
		theFlags = flags;

		//heck::parallelForEachN(heck::cdiv(map.numPlotsINLINE(), DynamicBitmap2D::kBitsPerWord), [&map, &group, start, flags, numPlots = map.numPlotsINLINE(), this](size_t wordI) {
		//	DynamicBitmap2D::Word moveFromValidWord{};
		//	DynamicBitmap2D::Word destValidword{};
		//
		//	for (size_t i = 0; i < DynamicBitmap2D::kBitsPerWord; ++i)
		//	{
		//		const int plotI = static_cast<int>(wordI * DynamicBitmap2D::kBitsPerWord + i);
		//		if (plotI < numPlots)
		//		{
		//			CvPlot* const plot = map.plotByIndexINLINE(plotI);
		//			// "2, 0" activates plot danger, if enabled. Do we want plot danger? That's for the caller to decide.
		//			moveFromValidWord |= calc_pathFromValid(group, start, flags, 2, 0, getPlotCoord(*plot)) << i;
		//			destValidword |= calc_pathDestValid(group, flags, getPlotCoord(*plot)) << i;
		//		}
		//	}
		//
		//	validBitmap.words[wordI] = moveFromValidWord;
		//	destValidBitmap.words[wordI] = destValidword;
		//	});

		// Okay, all bits set. Now it's time to start the graph search.
		queue0.clear();
		queue1.clear();
		queue0.push_back(start);

		uint16_t dist = 0;

		steps[start] = dist;

		const heck::MapGeometry localMapGeom = mapGeom;

		const bool isWaterPathing = group.getDomainType() == DOMAIN_SEA;

		for (;;)
		{
			const uint16_t nextDist = dist < UINT16_MAX ? dist + 1 : dist;

			while (!queue0.empty())
			{
				const heck::ivec2 fromCoord = queue0.back();
				queue0.pop_back();

				//if (validBitmap[fromCoord])
				if (calc_pathFromValid(group, start, flags, 2, 0, fromCoord))
				{
					const bool fromWater = isWaterPathing && map.plotINLINE(fromCoord.x, fromCoord.y)->isWater();

					for (int adjI = 0; adjI < 8; ++adjI)
					{
						if (const auto toCoord = localMapGeom.tryCanonicalise(fromCoord + heck::kAdj[adjI]))
						{
							uint16_t& toSteps = steps[*toCoord];
							if (nextDist < toSteps)
							{
								if (fromWater
									&& map.plotINLINE(toCoord->x, toCoord->y)->isWater()
									&& !map.plotINLINE(fromCoord.x, toCoord->y)->isWater()
									&& !map.plotINLINE(toCoord->x, fromCoord.y)->isWater()
									)
									continue; // Prevent land bridge crossing, as in pathValid.

								toSteps = nextDist;
								queue1.push_back(*toCoord);
							}
						}
					}
				}
			}

			if (queue1.empty())
				break;

			std::swap(queue0, queue1);

			dist = nextDist;
		}
	}
	bool isReachableGoal(heck::ivec2 coord) const
	{
		return steps[coord] < UINT16_MAX
			//&& destValidBitmap[coord];
			&& calc_pathDestValid(*theGroup, theFlags, coord);
	}
	int getPathStepDistance(heck::ivec2 coord) const
	{
		return steps[coord];
	}
};

WholeMapSimpleDijkstra::WholeMapSimpleDijkstra() : mInternals(std::make_unique<Internals>())
{
}
WholeMapSimpleDijkstra::WholeMapSimpleDijkstra(WholeMapSimpleDijkstra&&) noexcept = default;
WholeMapSimpleDijkstra& WholeMapSimpleDijkstra::operator=(WholeMapSimpleDijkstra&&) noexcept = default;
WholeMapSimpleDijkstra::~WholeMapSimpleDijkstra() = default;

void WholeMapSimpleDijkstra::visitAllReachable(const CvSelectionGroup& group, heck::ivec2 start, uint32_t flags)
{
	return mInternals->visitAllReachable(group, start, flags);
}
bool WholeMapSimpleDijkstra::isReachableGoal(heck::ivec2 coord) const // Checks pathDestValid too.
{
	return mInternals->isReachableGoal(coord);
}
int WholeMapSimpleDijkstra::getPathStepDistance(heck::ivec2 coord) const
{
	return mInternals->getPathStepDistance(coord);
}

#endif