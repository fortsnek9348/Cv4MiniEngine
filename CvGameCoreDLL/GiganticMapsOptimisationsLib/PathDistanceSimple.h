#pragma once

#include "Util.h"

namespace GiganticMapsOptimisationsLib
{
	class PathDistanceSimple
	{
	public:
		// Land only for now.
		explicit PathDistanceSimple(const CvMap&);

		std::optional<int> findPathLength(ivec2 start, ivec2 goal) const;

		void verify() const;

	private:
		CivMapGeometry mMapGeom;
		DynamicBitmap2D mBitmap;
	};
}