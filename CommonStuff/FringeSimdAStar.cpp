#include "inc/CommonStuff/FringeSimdAStar.h"
#include "inc/CommonStuff/range.h"
#include "inc/CommonStuff/StdExt.h"

#include <fstream>
#include <iostream>

using namespace heck;

static constexpr int kFringeDivisor = 16;
static constexpr int kMinFringe = 16;
//static constexpr bool kEnableDeduplicationInPop = false;

FringeSimdAStar::FringeSimdAStar(ivec2 dim, int startPlotI)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mPitch(dim.x)
	, mStartPlotI(startPlotI)
{
	mGCosts[startPlotI] = 0;
	mGState[startPlotI] = 0;
	mHeap.reserve(1000);
	mHeap.emplace_back(0, startPlotI);
}
FringeSimdAStar::FringeSimdAStar(ivec2 dim, std::span<const int> startPlotIndices)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mPitch(dim.x)
	, mStartPlotI(-1)
{
	mHeap.reserve(1000);
	for (const int plotI : startPlotIndices)
	{
		mGCosts[plotI] = 0;
		mGState[plotI] = 0;
		mHeap.emplace_back(0, plotI);
	}
}

bool FringeSimdAStar::search(int goalPlotI, const IGraph& g)
{
	updateHeuristic(g);

	const int kAdjVector[]{
		-1 + -1 * mPitch,
		+0 + -1 * mPitch,
		+1 + -1 * mPitch,
		-1 + +0 * mPitch,
		// 0
		+1 + +0 * mPitch,
		-1 + +1 * mPitch,
		+0 + +1 * mPitch,
		+1 + +1 * mPitch,
	};

	while (!mHeap.empty())// && (goalPlotI < 0 || mHeap.frontKey() <= mGCosts[goalPlotI]))
	{
		const size_t fringeSize = std::min(std::max<size_t>(mHeap.size() / kFringeDivisor, kMinFringe), mHeap.size());
		//std::nth_element(std::execution::par, mHeap.begin(), mHeap.begin() + fringeSize, mHeap.end(), std::less<>());
		std::ranges::nth_element(mHeap, mHeap.begin() + fringeSize - 1, std::less<>());

		std::array<int32_t, kCoordVectorLength> entries;
		entries.fill(mHeap.front().plotI);

		const int32_t goalCost = mGCosts[goalPlotI];
		const bool didSomething = goalPlotI < 0 || mHeap[fringeSize - 1].fcost <= goalCost;

		for (const size_t heapChunkIndex : range((fringeSize + kCoordVectorLength - 1) / kCoordVectorLength))
		{
			const size_t iterationWidth = std::min<size_t>(kCoordVectorLength, fringeSize - heapChunkIndex * kCoordVectorLength);
			
			for (size_t i = 0; i < iterationWidth; ++i)
				entries[i] = mHeap[heapChunkIndex * kCoordVectorLength + i].plotI;

			const Vector plotIndexVector(entries);
			Vector::Mask iterationMask(std::bitset<kCoordVectorLength>((uint32_t(1) << iterationWidth) - 1));

			//if constexpr (kEnableDeduplicationInPop)
			//{
			//	// Improves things, but still pretty bad in the worst case.
			//	const __m512i conflict = _mm512_conflict_epi32(vselect(iterationMask, plotIndexVector, -1).reg.bits);
			//	iterationMask = iterationMask & ~vtest(Vector(heck::simd::Avx512IntegerReg(conflict)), UINT32_MAX);
			//}

			const Vector currentGCosts(mGCosts, plotIndexVector);
			const StateVector currentGState(mGState, plotIndexVector);

			int adjI = 0;
			for (const int adjOffset : kAdjVector)
			{
				const Vector adjVector = plotIndexVector + Vector(adjOffset);

				const Vector oldAdjGCosts(mGCosts, adjVector); // gather op, give this lots of time!
				const auto [stepCosts, stepState] = g.getStepCost(currentGState, plotIndexVector, adjVector, adjI);
				const Vector::Mask adjDomainMask = vcmplt(0, stepCosts);
				const Vector newAdjGCosts = currentGCosts + stepCosts;

				const Vector::Mask adjPushMask = vcmplt(newAdjGCosts, oldAdjGCosts) & iterationMask & adjDomainMask;

				if (adjPushMask.asBits())
				{
					const Vector heuristic = g.getHeuristic(adjVector);
					scatter(mGCosts, adjVector, newAdjGCosts, adjPushMask);
					scatter(mGState, adjVector, stepState, adjPushMask);
					//if constexpr (kEnableParentPointers)
					//	scatter(mParents, adjVector, plotIndexVector, adjPushMask);
					const Vector adjFCosts = newAdjGCosts + heuristic;
					//push(adjVector, adjFCosts, adjPushMask);

					unsigned int pushMaskBits = adjPushMask.asBits();
					while (pushMaskBits)
					{
						const int i = std::countr_zero(pushMaskBits);
						pushMaskBits &= pushMaskBits - 1;
						mHeap.emplace_back(adjFCosts.getElement(i), adjVector.getElement(i));
					}
				}

				++adjI;
			}

		}

		if (mHeap.size() >= fringeSize * 2)
		{
			std::ranges::copy(std::span(mHeap).subspan(mHeap.size() - fringeSize), mHeap.begin());
			mHeap.erase(mHeap.end() - fringeSize, mHeap.end());
		}
		else
			mHeap.erase(mHeap.begin(), mHeap.begin() + fringeSize);

		if (!didSomething)
			break;
	}

	//std::cout << maxPQOverlap << std::endl;

	return goalPlotI >= 0 && mGCosts[goalPlotI] < INT32_MAX;
}

void FringeSimdAStar::updateHeuristic(const IGraph& g)
{
	for (auto& entry : mHeap)
		entry.fcost = mGCosts[entry.plotI] + g.getHeuristic(entry.plotI).getElement(0);
}

std::optional<int> FringeSimdAStar::findMostDistancePlot() const
{
	int bestG = -1;
	int bestI = -1;

	for (size_t i = 0; i < mGCosts.size() / kCoordVectorLength; ++i)
	{
		const Vector v{ std::span(mGCosts).subspan(i * kCoordVectorLength).subspan<0, kCoordVectorLength>() };
		const Vector modified = vselect(vcmplt(v, INT32_MAX), v, -1);
		if (vcmplt(bestG, modified).asBits())
		{
			bestG = hmax(v);
			bestI = int(i * kCoordVectorLength + std::countr_zero(vcmpeq(v, bestG).asBits()));
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
		return bestI;
	else
		return std::nullopt;
}

std::vector<int> FringeSimdAStar::reconstructPath(int goalPlotI, const IGraph& g) const
{
	if (!isVisited(goalPlotI))
		return {};

	int plotI = goalPlotI;

	std::vector<int> path{ plotI };

	static constexpr ivec2 kAdjVector[]{
		{ -1, -1 },
		{ +0, -1 },
		{ +1, -1 },
		{ -1, +0 },
		//{ +0, +0 },
		{ +1, +0 },
		{ -1, +1 },
		{ +0, +1 },
		{ +1, +1 },
	};

	//if constexpr (kEnableParentPointers)
	//{
	//	int accPathCostToGoal = 0;
	//	do
	//	{
	//		const int32_t parent = mParents[plotI];
	//		const ivec2 fromCoord{ parent % mPitch, parent / mPitch };
	//		const ivec2 toCoord{ plotI % mPitch, plotI / mPitch };
	//		const ivec2 d = toCoord - fromCoord;
	//		const int adjI1 = (d.x + 1) + (d.y + 1) * 3;
	//		const int adjI2 = adjI1 - (adjI1 >= 4);
	//		if (fromCoord + kAdjVector[adjI2] != toCoord)
	//			std::abort();
	//		const auto [stepCosts, stepState] = g.getStepCost(mGState[parent], parent, plotI, adjI2);
	//		const int32_t expectedGCost = mGCosts[parent] + stepCosts.getElement(0);
	//		const int plotH = g.getHeuristic(plotI).getElement(0);
	//
	//		if (expectedGCost != mGCosts[plotI] || stepState.getElement(0) != mGState[plotI]
	//			|| accPathCostToGoal < plotH)
	//		{
	//			const ivec2 startCoord{ mStartPlotI % mPitch, mStartPlotI / mPitch };
	//			const ivec2 goalCoord{ goalPlotI % mPitch, goalPlotI / mPitch };
	//			const int parentH = g.getHeuristic(parent).getElement(0);
	//
	//			std::clog << "Path reconstruction step mismatch!" << std::endl;
	//			if (accPathCostToGoal < plotH)
	//				std::clog << "Heuristic was not admissible." << std::endl;
	//			std::clog << "From " << fromCoord << " to " << toCoord << std::endl;
	//			std::clog << "Start " << startCoord << " to goal " << goalCoord << std::endl;
	//			std::clog << "AdjI = " << adjI2 << std::endl;
	//			std::clog << "From cost:state = " << mGCosts[parent] << ':' << mGState[parent] << std::endl;
	//			std::clog << "Stepped  cost:state = " << expectedGCost << ':' << stepState.getElement(0) << std::endl;
	//			std::clog << "Expected cost:state = " << mGCosts[plotI] << ':' << mGState[plotI] << std::endl;
	//			std::clog << "Cost(start, goal): " << mGCosts[goalPlotI] << std::endl;
	//			std::clog << "Cost(to, goal)     : " << accPathCostToGoal << std::endl;
	//			std::clog << "AccCost(from, goal): " << accPathCostToGoal + stepCosts.getElement(0) << std::endl;
	//			std::clog << "h(to, goal)        : " << plotH << std::endl;
	//			std::clog << "h(from, goal)      : " << parentH << std::endl;
	//			std::clog << "From plot fcost: " << mGCosts[parent] + parentH << std::endl;
	//			std::abort();
	//		}
	//
	//		plotI = parent;
	//		path.push_back(plotI);
	//
	//		accPathCostToGoal += stepCosts.getElement(0);
	//
	//		// Sanity check
	//		if (path.size() > 1'000'000)
	//			std::abort();
	//	} while (plotI != mStartPlotI);
	//}
	//else
	{
		// We're doing SIMD fixed to 8 elements here. The graph interface may be using 16-element vectors.
		using AdjVector = simd::Vector<int32_t, 8>;
		using AdjStateVector = simd::Vector<uint32_t, 8>;

		const AdjVector adjVector(std::array<int, 8>{
			-1 + -1 * mPitch,
				+0 + -1 * mPitch,
				+1 + -1 * mPitch,
				-1 + +0 * mPitch,
				+1 + +0 * mPitch,
				-1 + +1 * mPitch,
				+0 + +1 * mPitch,
				+1 + +1 * mPitch,
		});

		const Vector adjIndices{ std::array<int32_t, Vector::kNumElements>{ 7, 6, 5, 4, 3, 2, 1, 0 } };

		do
		{
			const AdjVector adjPlotIndices = AdjVector(plotI) + adjVector;
			const AdjVector adjGCosts(mGCosts, adjPlotIndices);
			const AdjStateVector adjGState(mGState, adjPlotIndices);
			const std::bitset<kCoordVectorLength> adjBitMask((1 << 8) - 1);
			const AdjVector stepCosts = (AdjVector)g.getStepCost(
				vselect(StateVector::Mask(adjBitMask), StateVector(adjGState), adjGState.getElement(7)),
				vselect(Vector::Mask(adjBitMask), Vector(adjPlotIndices), adjPlotIndices.getElement(7)),
				plotI, adjIndices).cost;
			const AdjVector::Mask adjMask = vcmplt(0, stepCosts) & vcmplt(adjGCosts, INT32_MAX);
			const AdjVector thisGCosts = adjGCosts + stepCosts;
			//const int bestThisGCost = hmin(vselect(adjMask, thisGCosts, AdjVector(INT32_MAX)));
			//const int bestThisGCostI = std::countr_zero(vcmpeq(bestThisGCost, thisGCosts).asBits());
			const int bestThisGCostI = std::countr_zero(vcmpeq(vselect(adjMask, thisGCosts, AdjVector(INT32_MAX)), mGCosts[plotI]).asBits());
			if (bestThisGCostI >= 8)
				std::abort();
			plotI = adjPlotIndices.getElement(bestThisGCostI);
			path.push_back(plotI);

			// Sanity check
			if (path.size() > 1'000'000)
				std::abort();
		} while (plotI != mStartPlotI);
	}
	std::ranges::reverse(path);

	return path;
}

void FringeSimdAStar::dumpStats(std::ostream&) const
{
}