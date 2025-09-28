#include "inc/CommonStuff/PQCoordSimdAStar.h"
#include "inc/CommonStuff/range.h"
#include "inc/CommonStuff/adj.h"
#include "inc/CommonStuff/SimdCoordWrapping.h"

#include <iostream>

using namespace heck;

static constexpr bool kEnableParentPointers = false;

PQCoordSimdAStar::PQCoordSimdAStar(ivec2 dim, bool wrapX, bool wrapY)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mDim(dim)
	, mWrapX(wrapX)
	, mWrapY(wrapY)
{
	mStartPlots.reserve(20);

	if constexpr (kEnableParentPointers)
		mParents.resize(mGCosts.size());
}

int PQCoordSimdAStar::toPlotIndex(ivec2 coord) const
{
	return coord.x + coord.y * mDim.x;
}

i16aabb2 PQCoordSimdAStar::reset() noexcept
{
	mHeap.clear();

	iaabb2 visitSetRect;

	std::vector<ivec2> clearWorkList;
	for (ivec2 clearStartPlotCoord : mStartPlots)
	{
		// To coord, and align to vector size (cacheline, for AVX-512).
		// We clear out vector-sized chunks.
		clearStartPlotCoord.x = clearStartPlotCoord.x / kCoordVectorLength * kCoordVectorLength;
		clearWorkList.emplace_back(clearStartPlotCoord);
		visitSetRect.combine(iaabb2::sized(clearWorkList.back(), 1));
	}

	mStartPlots.clear();

	const std::span gcosts(mGCosts);
	const int pitch = mDim.x;

	while (!clearWorkList.empty())
	{
		ivec2 coord = clearWorkList.back();
		clearWorkList.pop_back();

		if (mWrapX)
		{
			if (coord.x < 0)
				coord.x += mDim.x;
			if (coord.x >= mDim.x)
				coord.x -= mDim.x;
		}

		if (mWrapY)
		{
			if (coord.y < 0)
				coord.y += mDim.y;
			if (coord.y >= mDim.y)
				coord.y -= mDim.y;
		}

		if (coord.x < 0 || coord.y < 0)
			continue;
		if (coord.x >= pitch || coord.y * pitch >= (ptrdiff_t)mGState.size())
			continue;

		if (gcosts.subspan(coord.x + coord.y * pitch).size() >= kCoordVectorLength)
		{
			const std::span fillSpan = gcosts.subspan(coord.x + coord.y * pitch).subspan<0, kCoordVectorLength>();

			// Check if any plots were visited here.
			if (vcmplt(Vector(fillSpan), INT32_MAX).none())
				continue;

			// Only need to clear costs.
			std::ranges::fill(fillSpan, INT32_MAX);

			visitSetRect.combine(iaabb2::sized(coord, { kCoordVectorLength, 1 }));
		}
		else
		{
			const std::span fillSpan = gcosts.subspan(coord.x + coord.y * pitch);

			if (!std::ranges::any_of(fillSpan, [](int32_t cost) { return cost < INT32_MAX; }))
				continue;

			std::ranges::fill(fillSpan, INT32_MAX);

			visitSetRect.combine(iaabb2::sized(coord, { (int)fillSpan.size(), 1 }));
		}

		clearWorkList.append_range(kAdj | std::views::transform([coord](ivec2 adjD) { return coord + adjD * ivec2(kCoordVectorLength, 1); }));
	}

	//std::ranges::fill(mGCosts, kUnreachableCost);
	//std::ranges::fill(mGState, 0);

	return visitSetRect.cast<int16_t>();
}

// Adds a plot to the open set.
void PQCoordSimdAStar::addStart(ivec2 startCoord, uint32_t state)
{
	const int plotI = toPlotIndex(startCoord);
	mGCosts[plotI] = 0;
	mGState[plotI] = state;
	if constexpr (kEnableParentPointers)
		mParents[plotI] = -1;
	mHeap.push(0, { startCoord.x, startCoord.y }, Vector::Mask(1));
	//mHeap.push(0, startCoord);
	mStartPlots.push_back(startCoord);
}
// Adds multiple plots to the open set, with state = 0.
void PQCoordSimdAStar::addStart(std::span<const i16vec2> plotCoords)
{
	for (const ivec2 coord : plotCoords)
		addStart(coord, 0);
}

void PQCoordSimdAStar::updateOpenSetHeuristicValues(const IGraph& g)
{
	updateHeuristic(g);
}

bool PQCoordSimdAStar::search(ivec2 goalPlotCoord, IGraph& g, int32_t maxCost)
{
	const int pitch = mDim.x;

	const ivec2 lastCoord = mDim - 1;
	const int goalPlotI = toPlotIndex(goalPlotCoord);

	while (!mHeap.empty() && (goalPlotI < 0 || mHeap.peek() <= mGCosts[goalPlotI]) && mHeap.peek() <= maxCost)
	//while (!mHeap.empty() && (goalPlotI < 0 || mHeap.frontKey() <= mGCosts[goalPlotI]) && mHeap.frontKey() <= maxCost)
	{
		const auto [plotCoords, iterationMask] = mHeap.pop();

		const Vector fromX = plotCoords.x;
		const Vector fromY = plotCoords.y;

		// Assert: Plot coords in range.
		assert(vcmplt(fromX, 0).none() && vcmple(mDim.x, fromX).none());
		assert(vcmplt(fromY, 0).none() && vcmple(mDim.y, fromY).none());

		const Vector fromPlotIndices = fromX + fromY * pitch;

		// True if any from plot is on the edge of the map, and thus, may need wrapping handling.
		const bool onEdge = ((vcmpeq(vmin(fromX, fromY), 0) | vcmpeq(fromX, lastCoord.x) | vcmpeq(fromY, lastCoord.y)) & iterationMask).any();

		const auto handleWrapping = [onEdge, dim = mDim, wrapX = mWrapX, wrapY = mWrapY, fromX, fromY](Vector unwrappedX, Vector unwrappedY) {

			if (!onEdge)
				return SimdCoordWrappingResult{ unwrappedX, unwrappedY, Vector::Mask::kAll };

			return wrapCoordinates({ unwrappedX, unwrappedY }, dim, wrapX, wrapY, { fromX, fromY });
		};

		const Vector fromGCosts(mGCosts, fromPlotIndices);
		const StateVector fromGState(mGState, fromPlotIndices);

		// TODO: Check fromGCosts matches heap entries? Would be best to store G costs on the heap.

		g.setStepFrom(fromGState, fromPlotIndices);

		for (const int adjI : range((int)std::size(kAdj)))
		{
			const auto [toX, toY, adjValidMask] = handleWrapping(fromX + kAdj[adjI].x, fromY + kAdj[adjI].y);
			const Vector toPlotIndices = toX + toY * pitch;

			const Vector oldAdjGCosts(mGCosts, toPlotIndices); // gather op, give this lots of time!
			const auto [stepCosts, stepState] = g.getStepCostTo(toPlotIndices, adjI);
			const Vector::Mask adjDomainMask = vcmplt(0, stepCosts);
			const Vector newAdjGCosts = fromGCosts + stepCosts;

			const Vector::Mask adjPushMask = vcmplt(newAdjGCosts, oldAdjGCosts) & iterationMask & adjDomainMask & adjValidMask;

			if (adjPushMask.any())
			{
				const Vector heuristic = g.getHeuristic(toPlotIndices);
				
				const Vector adjFCosts = newAdjGCosts + heuristic;
				mHeap.push(adjFCosts, { toX, toY }, adjPushMask);
				//push({ toX, toY }, adjFCosts, adjPushMask);

				scatter(mGCosts, toPlotIndices, newAdjGCosts, adjPushMask);
				scatter(mGState, toPlotIndices, stepState, adjPushMask);

				if constexpr (kEnableParentPointers)
					scatter(mParents, toPlotIndices, adjI, adjPushMask);

				// If you need to debug possible inconsistent A* state
				//const ivec2 testCenter(36, 22);
				//const Vector testDist = vmax(vabs(toX - testCenter.x), vabs(toY - testCenter.y));
				//
				//for (const int i : range(kCoordVectorLength))
				//{
				//	if (adjPushMask.asBitset()[i] && testDist.getElement(i) <= 2)
				//	{
				//		const auto state = stepState.getElement(i);
				//		std::clog << "PQCoordSimdAStar set plot " << ivec2(toX.getElement(i), toY.getElement(i)) << ": "
				//			<< newAdjGCosts.getElement(i) << ' '
				//			<< (state >> 12) << ':' << state % (1 << 12)
				//			<< " from " << ivec2(fromX.getElement(i), fromY.getElement(i))
				//			<< std::endl;
				//	}
				//}
			}
		}
	}

	return goalPlotI >= 0 && mGCosts[goalPlotI] < kUnreachableCost;
}

/*
void HECK_VECTORCALL PQCoordSimdAStar::push(vec2<Vector> adjCoords, Vector adjFCosts, Vector::Mask adjPushMask)
{
	unsigned int bits = adjPushMask.asBits();
	while (bits)
	{
		const unsigned int i = std::countr_zero(bits);
		bits &= bits - 1;
		mHeap.push(adjFCosts.getElement(i), (i16vec2)ivec2(adjCoords.x.getElement(i), adjCoords.y.getElement(i)));
	}
}
std::pair<vec2<PQCoordSimdAStar::Vector>, PQCoordSimdAStar::Vector::Mask> HECK_VECTORCALL PQCoordSimdAStar::pop()
{
	const size_t iterationWidth = std::min<size_t>(mHeap.size(), kCoordVectorLength);
	const Vector::Mask iterationMask{ std::bitset<kCoordVectorLength>((1 << iterationWidth) - 1) };
	alignas(Vector) std::array<int32_t, kCoordVectorLength> plotX;
	alignas(Vector) std::array<int32_t, kCoordVectorLength> plotY;
	// Fill coordinates with something valid for adjacency.
	plotX.fill(1);
	plotY.fill(1);

	for (const size_t i : range(iterationWidth))
	{
		const ivec2 coord = mHeap.pop().value;
		plotX[i] = coord.x;
		plotY[i] = coord.y;
	}
	
	return { { plotX, plotY }, iterationMask };
}*/
void PQCoordSimdAStar::updateHeuristic(const IGraph& g)
{
	const int pitch = mDim.x;
	mHeap.update(
		[pitch, &g, this](vec2<Vector> coords) {
			const Vector plotI = coords.x + coords.y * pitch;
			return Vector(mGCosts, plotI) + g.getHeuristic(coords.x + coords.y * pitch);
		},
		[pitch, &g, this](ivec2 coord) {
			const int plotI = coord.x + coord.y * pitch;
			return mGCosts[plotI] + g.getSingleHeuristic(plotI);
		}
	);
	//for (auto& entry : mHeap.elements())
	//{
	//	const int plotI = entry.value.x + entry.value.y * pitch;
	//	entry.key = mGCosts[plotI] + g.getSingleHeuristic(plotI);
	//}
	//mHeap.reheapify();
}

std::optional<i16vec2> PQCoordSimdAStar::findMostDistantPlot() const
{
	int bestG = -1;
	int bestI = -1;

	for (size_t i = 0; i < mGCosts.size() / kCoordVectorLength; ++i)
	{
		const Vector v{ std::span(mGCosts).subspan(i * kCoordVectorLength).subspan<0, kCoordVectorLength>() };
		const Vector modified = vselect(vcmplt(v, INT32_MAX), v, -1);
		if (vcmplt(bestG, modified).any())
		{
			bestG = hmax(v);
			bestI = int(i * kCoordVectorLength + vcmpeq(v, bestG).countr_zero());
		}
	}

	for (size_t i = mGCosts.size() / kCoordVectorLength * kCoordVectorLength; i < mGCosts.size(); ++i)
	{
		if (mGCosts[bestI] > bestG)
		{
			bestG = mGCosts[bestI];
			bestI = (int)i;
		}
	}

	if (bestI >= 0)
		return (i16vec2)ivec2(bestI % mDim.x, bestI / mDim.x);
	else
		return std::nullopt;
}

heck::ISimdAStarGraph::Step HECK_VECTORCALL PQCoordSimdAStar::getGCostsAndUnitStatesAroundPlot(ivec2 toCoord) const
{
	const Vector X = toCoord.x + SimdAStarConfig::kAdjCoordsVector.x;
	const Vector Y = toCoord.y + SimdAStarConfig::kAdjCoordsVector.y;
	const SimdCoordWrappingResult wrappingResult = wrapCoordinates({ X, Y }, mDim, mWrapX, mWrapY, { toCoord.x, toCoord.y });
	const Vector indices = wrappingResult.X + wrappingResult.Y * mDim.x;
	return {
		vselect(wrappingResult.validMask, Vector(std::span(mGCosts), indices), kUnreachableCost),
		StateVector(std::span(mGState), indices),
	};
}

void PQCoordSimdAStar::dumpStats(std::ostream& os) const
{
	os << "Open set size = " << mHeap.size() << '\n';
}