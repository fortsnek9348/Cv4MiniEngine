#include "GameContext.h"

#if ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "Util.h"

#include "../CvPlayer.h"
#include "../CvSelectionGroupAI.h"

using namespace GiganticMapsOptimisationsLib;

heck::DynamicArray2D<uint8_t> GiganticMapsOptimisationsLib::buildAIPlotTargetMap(const CvPlayer& player, std::span<const MissionAITypes> missionAIs, const CvSelectionGroup* pSkipSelectionGroup, int iRange)
{
	const heck::MapGeometry mapGeom = CivMapGeometry(GC.getMapINLINE());
	DynamicArray2D<uint8_t> map(mapGeom.dim);

	int iLoop{};
	for (const CvSelectionGroup* pLoopSelectionGroup = player.firstSelectionGroup(&iLoop); pLoopSelectionGroup; pLoopSelectionGroup = player.nextSelectionGroup(&iLoop))
	{
		if (pLoopSelectionGroup == pSkipSelectionGroup)
			continue;
		
		if (const CvPlot* const pMissionPlot = pLoopSelectionGroup->AI_getMissionAIPlot())
		{
			const MissionAITypes eGroupMissionAI = pLoopSelectionGroup->AI_getMissionAIType();

			int numMissions = 0;

			for (const MissionAITypes missionType : missionAIs)
				if (missionType == eGroupMissionAI || missionType == NO_MISSIONAI)
					numMissions += pLoopSelectionGroup->getNumUnits();

			if (numMissions > 0)
			{
				const ivec2 origin = getPlotCoord(*pMissionPlot);
				for (int dy = -iRange; dy <= iRange; ++dy)
					for (int dx = -iRange; dx <= iRange; ++dx)
						if (const auto optCoord = mapGeom.tryCanonicalise(origin + ivec2(dx, dy)))
							map[*optCoord] = static_cast<uint8_t>(std::min<int>(map[*optCoord] + numMissions, UINT8_MAX));
			}
		}
	}

	return map;
}

#endif