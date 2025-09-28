#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#include "FoundValueCommonCache.h"
#include "FoundValueUtil.h"

#include "../../CvMap.h"
#include "../../CvArea.h"

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;

AreaMap::AreaMap(CvMap& map)
{
	const CivMapGeometry mapGeom(map);
	for (const int y : range(map.getGridHeightINLINE()))
	{
		for (const int x : range(map.getGridWidthINLINE()))
		{
			if (const CvArea* const area = map.plot(x, y)->area(); area && !area->isWater())
			{
				iaabb2& unwrappedRect = unwrappedAreaRectsByAreaId[area->getID()];
				unionUnwrappedRect(mapGeom, unwrappedRect, { x, y });
			}
		}
	}
}

#endif