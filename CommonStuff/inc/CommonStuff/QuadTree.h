#pragma once

#include "aabb.h"
#include "CompilerMacros.h"
#include "MapGeometry.h"
#include "Simd.h"

#include <vector>

namespace heck
{
	struct Image;
	
	class QuadTree
	{
	public:
		explicit QuadTree(MapGeometry mapGeom, int minCellDim, int leafCapacity);

		void insert(i16vec2 coord);
		void erase(i16vec2 coord);

		i16vec2 findNearest(ivec2 queryPoint, i16vec2 hint) const;

		static constexpr ivec2 kSimdQueryDim{ 8, 8 };
		static constexpr unsigned int kSimdQuerySize = kSimdQueryDim.x * kSimdQueryDim.y;
		using I16Vector = simd::Vector<int16_t, kSimdQuerySize>;
		using I16Vector2 = vec2<I16Vector>;

		I16Vector2 HECK_VECTORCALL findNearestSIMD(ivec2 queryPoint, I16Vector2 hints) const;

		void debugDraw(Image& img) const;

	private:
		MapGeometry mMapGeom;
		int mHeight = 0; // 1 == just the root node
		int mLeafCapacity = 0;

		std::vector<bool> mNodeIsInternal;
		std::vector<std::vector<i16vec2>> mNodePoints; // 128KB for 1040x640 min dim 8

		void debugDrawNode(Image& img, size_t nodeI, iaabb2 rect, int depth) const;
	};
}