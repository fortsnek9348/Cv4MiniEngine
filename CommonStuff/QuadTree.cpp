#include "inc/CommonStuff/QuadTree.h"
#include "inc/CommonStuff/range.h"
#include "inc/CommonStuff/PPM.h"
#include "inc/CommonStuff/Simd.h"

using namespace heck;

QuadTree::QuadTree(MapGeometry mapGeom, int minCellDim, int leafCapacity) : mMapGeom(mapGeom), mLeafCapacity(leafCapacity)
{
	ivec2 cellDim = mapGeom.dim;
	mHeight = 1;
	while (std::min(cellDim.x, cellDim.y) / 2 >= minCellDim)
	{
		++mHeight;
		cellDim = cellDim / 2;
	}

	//mLeafDim = cellDim;

	mNodeIsInternal.resize(((1 << (mHeight * 2)) - 1) / 3);
	mNodePoints.resize(mNodeIsInternal.size());

	//std::cout << "Height = " << mHeight << std::endl;
	//std::cout << "Leaf cell dim = " << cellDim << std::endl;
	//std::cout << "Num nodes = " << mNodeIsInternal.size() << std::endl;

	//std::cout << nodeLevel(0) << ' ' << computeNodeRect(0) << std::endl;
	//std::cout << nodeLevel(1) << ' ' << computeNodeRect(1) << std::endl;
	//std::cout << nodeLevel(2) << ' ' << computeNodeRect(2) << std::endl;
	//std::cout << nodeLevel(3) << ' ' << computeNodeRect(3) << std::endl;
	//std::cout << nodeLevel(4) << ' ' << computeNodeRect(4) << std::endl;
	//std::cout << nodeLevel(5) << ' ' << computeNodeRect(5) << std::endl;
	//std::cout << nodeLevel(6) << ' ' << computeNodeRect(6) << std::endl;
	//std::cout << nodeLevel(7) << ' ' << computeNodeRect(7) << std::endl;
	//std::cout << nodeLevel(8) << ' ' << computeNodeRect(8) << std::endl;
}

// 0 1
// 2 3

static ivec2 splitCenter(iaabb2 rect)
{
	return rect.min + rect.size() / 2;
}

static size_t calcFirstChildI(size_t nodeI)
{
	return nodeI * 4 + 1;
}

static size_t calcParentI(size_t nodeI)
{
	return (nodeI - 1) / 4;
}

//static size_t computeCompleteTreeSize(size_t numLevels)
//{
//	return ((1u << (numLevels * 2))-1)/3;
//}
//
//static unsigned int nodeLevel(size_t nodeI)
//{
//	return unsigned(std::bit_width(nodeI * (4 - 1) + 1) - 1) / 2;
//}

// This is broken
//iaabb2 computeNodeRect(size_t nodeI) const
//{
//	const unsigned int L = nodeLevel(nodeI);
//	ivec2 nodeDim = mMapGeom.dim >> L;
//	const unsigned int mapWidthCells = 1u << L;
//	const unsigned int nodeIndexInLevel = unsigned(nodeI - computeCompleteTreeSize(L));
//	const unsigned int nodeX = nodeIndexInLevel % mapWidthCells;
//	const unsigned int nodeY = nodeIndexInLevel >> L;
//	return iaabb2::sized(ivec2(nodeX, nodeY) * nodeDim, nodeDim);
//}

static iaabb2 rectQuadrant(iaabb2 rect, int quadI)
{
	const ivec2 center = splitCenter(rect);
	switch (quadI)
	{
	case 0:
		rect.max = center;
		return rect;
	case 1:
		rect.min.x = center.x;
		rect.max.y = center.y;
		return rect;
	case 2:
		rect.min.y = center.y;
		rect.max.x = center.x;
		return rect;
	case 3:
		rect.min = center;
		return rect;
	default:
		std::unreachable();
	}
}

void QuadTree::insert(i16vec2 coord)
{
	size_t nodeI = 0;
	iaabb2 nodeRect{ .max = mMapGeom.dim };
	unsigned int level = 0;
	for (;;)
	{
		if (mNodeIsInternal[nodeI])
		{
		insertHere:
			const ivec2 center = splitCenter(nodeRect);
			const int childI = (center.x <= coord.x) + (center.y <= coord.y) * 2;
			nodeI = calcFirstChildI(nodeI) + childI;
			nodeRect = rectQuadrant(nodeRect, childI);
			++level;
		}
		else if (mNodePoints[nodeI].size() < (unsigned)mLeafCapacity || level + 1 >= (unsigned)mHeight)
		{
			mNodePoints[nodeI].push_back(coord);
			break;
		}
		else
		{
			mNodeIsInternal[nodeI] = true;
			// Two data structure choices: Leave points in the internal nodes, or move them to leafs. Keeping them in internal nodes is a little faster.
			//for (const i16vec2 internalCoord : mNodePoints[nodeI])
			//	insert(internalCoord);
			//mNodePoints[nodeI].clear();
			goto insertHere;
		}
	}
}
void QuadTree::erase(i16vec2 coord)
{
	size_t nodeI = 0;
	iaabb2 nodeRect{ .max = mMapGeom.dim };
	for (;;)
	{
		std::erase(mNodePoints[nodeI], coord);

		if (mNodeIsInternal[nodeI])
		{
			const ivec2 center = splitCenter(nodeRect);
			const int childI = (center.x <= coord.x) + (center.y <= coord.y) * 2;
			nodeI = calcFirstChildI(nodeI) + childI;
			nodeRect = rectQuadrant(nodeRect, childI);
		}
		else
			break;
	}

	// Now turn empty internal nodes into leaf nodes.
	for (;;)
	{
		const size_t firstChildI = calcFirstChildI(nodeI);
		if (mNodeIsInternal[nodeI])
		{
			if (!mNodeIsInternal[firstChildI + 0] &&
				!mNodeIsInternal[firstChildI + 1] &&
				!mNodeIsInternal[firstChildI + 2] &&
				!mNodeIsInternal[firstChildI + 3] &&
				mNodePoints[firstChildI + 0].empty() &&
				mNodePoints[firstChildI + 1].empty() &&
				mNodePoints[firstChildI + 2].empty() &&
				mNodePoints[firstChildI + 3].empty())
			{
				mNodeIsInternal[nodeI] = false;
			}
			else
				break;
		}

		if (nodeI <= 0)
			break;

		nodeI = calcParentI(nodeI);
	}
}

i16vec2 QuadTree::findNearest(ivec2 queryPoint, i16vec2 hint) const
{
	const iaabb2 queryRect = iaabb2::sized(queryPoint, 1);

	struct Entry
	{
		unsigned int nodeI = 0;
		i16aabb2 rect{};
		int minDist = 0; // Could extend this to per-rectpoint.
	};

	// Stack is faster than priority queue
	Entry stack[4 * 32]{};
	unsigned int stackSize = 1;
	stack[0] = { .rect = i16aabb2{.max = i16vec2(mMapGeom.dim) } };

	const MapGeometry mapGeom = mMapGeom;

	int bestDist = hint.x < 0 ? INT16_MAX : mapGeom.plotDistance(hint, queryPoint);
	i16vec2 bestCoord = hint;

	[[maybe_unused]] int numIts = 0;

	while (stackSize > 0)
	{
		++numIts;

		const auto [nodeI, nodeRect, nodeMinDist] = stack[--stackSize];

		//if (bestDist < mapGeom.minPlotDistanceBetweenRects(iaabb2::sized(queryPoint, 1), nodeRect.cast<int>()))
		if (bestDist < static_cast<int16_t>(nodeMinDist))
			continue;

		for (const i16vec2 coord : mNodePoints[nodeI])
		{
			const int dist = mapGeom.plotDistance(coord, queryPoint);
			if (dist < bestDist)
			{
				bestDist = dist;
				bestCoord = coord;
			}
		}

		if (mNodeIsInternal[nodeI])
		{
			const iaabb2 rect = nodeRect.cast<int>();
			const iaabb2 childRects[]{
				rectQuadrant(rect, 0),
				rectQuadrant(rect, 1),
				rectQuadrant(rect, 2),
				rectQuadrant(rect, 3),
			};
			const size_t firstChildI = calcFirstChildI(nodeI);
			int childIndices[]{
				0,
				1,
				2,
				3,
			};
			int childDists[]{
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[0]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[1]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[2]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[3]),
			};

			//std::ranges::sort(std::views::zip(childDists, childIndices), std::greater<>(), [](auto t) { return std::get<0>(t); });

			const auto cmp = [&](int a, int b) {
				if (childDists[a] < childDists[b])
				{
					std::swap(childDists[a], childDists[b]);
					std::swap(childIndices[a], childIndices[b]);
				}
				};

			// Sort network
			cmp(0, 2);
			cmp(1, 3);
			cmp(0, 1);
			cmp(2, 3);
			cmp(1, 2);

			for (const int i : range(4))
				if (childDists[i] < bestDist)
					stack[stackSize++] = { (unsigned int)firstChildI + childIndices[i], childRects[childIndices[i]].cast<int16_t>(), childDists[i] };
		}
		//else
		//{
		//	for (const ivec2 coord : mNodePoints[nodeI])
		//		bestDist = std::min(bestDist, mapGeom.plotDistance(coord, queryPoint));
		//}

	}

	//std::cout << numIts << ' ';

	return bestCoord;
}

QuadTree::I16Vector2 HECK_VECTORCALL QuadTree::findNearestSIMD(ivec2 queryPoint, I16Vector2 hints) const
{
	const iaabb2 queryRect = iaabb2::sized(queryPoint, kSimdQueryDim);

	static constexpr I16Vector2 kQueryOffsets = [] {
		std::array<int16_t, kSimdQuerySize> X{};
		std::array<int16_t, kSimdQuerySize> Y{};
		for (const int y : range(kSimdQueryDim.y))
			for (const int x : range(kSimdQueryDim.x))
			{
				X[x + y * kSimdQueryDim.x] = static_cast<int16_t>(x);
				Y[x + y * kSimdQueryDim.x] = static_cast<int16_t>(y);
			}
		return I16Vector2(X, Y);
		}();

	const I16Vector2 queryRectPoints = I16Vector2(static_cast<int16_t>(queryPoint.x), static_cast<int16_t>(queryPoint.y)) + kQueryOffsets;

	struct Entry
	{
		unsigned int nodeI = 0;
		i16aabb2 rect{};
		int minDist = 0; // Could extend this to per-rectpoint.
	};

	// Stack is faster than priority queue
	Entry stack[4 * 32]{};
	unsigned int stackSize = 1;
	stack[0] = { .rect = i16aabb2{.max = i16vec2(mMapGeom.dim) } };

	const MapGeometry mapGeom = mMapGeom;

	I16Vector bestDist = vselect(vcmplt(hints.x, 0), INT16_MAX, mapGeom.plotDistance(hints, queryRectPoints));
	//I16Vector bestDist = INT16_MAX;
	I16Vector2 bestCoord = hints;

	[[maybe_unused]] int numIts = 0;

	while (stackSize > 0)
	{
		++numIts;

		const auto [nodeI, nodeRect, nodeMinDist] = stack[--stackSize];

		//if (bestDist < mapGeom.minPlotDistanceBetweenRects(iaabb2::sized(queryPoint, 1), nodeRect.cast<int>()))
		if (vcmplt(bestDist, static_cast<int16_t>(nodeMinDist)).all())
			continue;

		for (const i16vec2 coord : mNodePoints[nodeI])
		{
			const I16Vector2 coordBroadcasted(coord.x, coord.y);
			const I16Vector dist = mapGeom.plotDistance(coordBroadcasted, queryRectPoints);
			const I16Vector::Mask mask = vcmplt(dist, bestDist);
			bestDist = vselect(mask, dist, bestDist);
			bestCoord.x = vselect(mask, coordBroadcasted.x, bestCoord.x);
			bestCoord.y = vselect(mask, coordBroadcasted.y, bestCoord.y);
		}

		if (mNodeIsInternal[nodeI])
		{
			const iaabb2 rect = nodeRect.cast<int>();
			const iaabb2 childRects[]{
				rectQuadrant(rect, 0),
				rectQuadrant(rect, 1),
				rectQuadrant(rect, 2),
				rectQuadrant(rect, 3),
			};
			const size_t firstChildI = calcFirstChildI(nodeI);
			int childIndices[]{
				0,
				1,
				2,
				3,
			};
			int childDists[]{
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[0]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[1]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[2]),
				mapGeom.minPlotDistanceBetweenRects(queryRect, childRects[3]),
			};

			//std::ranges::sort(std::views::zip(childDists, childIndices), std::greater<>(), [](auto t) { return std::get<0>(t); });

			const auto cmp = [&](int a, int b) {
				if (childDists[a] < childDists[b])
				{
					std::swap(childDists[a], childDists[b]);
					std::swap(childIndices[a], childIndices[b]);
				}
				};

			// Sort network
			cmp(0, 2);
			cmp(1, 3);
			cmp(0, 1);
			cmp(2, 3);
			cmp(1, 2);

			for (const int i : range(4))
				if (vcmplt(static_cast<int16_t>(childDists[i]), bestDist).any())
					stack[stackSize++] = { (unsigned int)firstChildI + childIndices[i], childRects[childIndices[i]].cast<int16_t>(), childDists[i] };
		}
		//else
		//{
		//	for (const ivec2 coord : mNodePoints[nodeI])
		//		bestDist = std::min(bestDist, mapGeom.plotDistance(coord, queryPoint));
		//}

	}

	//std::cout << numIts << ' ';

	return bestCoord;
}

void QuadTree::debugDraw(Image& img) const
{
	debugDrawNode(img, 0, { .max = mMapGeom.dim }, 0);

	for (const std::vector<i16vec2>& points : mNodePoints)
		for (const ivec2 point : points)
			img.drawPixel(point, { 255, 255, 255 }, 255);
}

void QuadTree::debugDrawNode(Image& img, size_t nodeI, iaabb2 rect, int depth) const
{
	img.drawRect(rect, { 255, 0, 0 }, 255);
	if (depth + 1 < mHeight && mNodeIsInternal[nodeI])
	{
		const size_t childI = calcFirstChildI(nodeI);
		debugDrawNode(img, childI + 0, rectQuadrant(rect, 0), depth + 1);
		debugDrawNode(img, childI + 1, rectQuadrant(rect, 1), depth + 1);
		debugDrawNode(img, childI + 2, rectQuadrant(rect, 2), depth + 1);
		debugDrawNode(img, childI + 3, rectQuadrant(rect, 3), depth + 1);
	}
}