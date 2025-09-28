#include "inc/CommonStuff/PQSimdAStar.h"
#include "inc/CommonStuff/range.h"
#include "inc/CommonStuff/adj.h"

#include <fstream>
#include <iostream>
#include <syncstream>

using namespace heck;

static constexpr bool kEnableStats = false;
[[maybe_unused]] static constexpr bool kStrictOrder = false;

// These can be used to verify step costs in the closed set.
static constexpr bool kEnableParentPointers = false;

// If enabled, require that stepping from the parent node yields precisely the same G cost.
// Otherwise, just go with the best parent.
// Can't do strict path reconstruction given that the step function for pathing effectively creates a third pathing coordinate:
// You need to track multiple paths through a node because of the combination of movement points and plot cost adjusts (you're tracking two "pathing histories").
static constexpr bool kStrictPathReconstruction = false;

static constexpr bool kDumpHeapLog = false;
static constexpr bool kTestHeuristicConsistency = false;

PQSimdAStar::PQSimdAStar(ivec2 dim)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mPitch(dim.x)
{
	if constexpr (kEnableParentPointers)
		mParents.resize(dim.x * dim.y, -1);

	mStartPlots.reserve(20);
}

PQSimdAStar::PQSimdAStar(ivec2 dim, int startPlotI, uint32_t initialState) : PQSimdAStar(dim)
{
	addStart(startPlotI, initialState);
}
PQSimdAStar::PQSimdAStar(ivec2 dim, std::span<const int> startPlotIndices) : PQSimdAStar(dim)
{
	addStart(startPlotIndices);
}

i16aabb2 PQSimdAStar::reset() noexcept
{
	mHeap.nodes.clear();
	mStartPlotI = -1;
	mHeuristicUpdateEnabled = true;

	iaabb2 visitSetRect;

	std::vector<ivec2> clearWorkList;
	for (const int clearStartPlotI : mStartPlots)
	{
		// To coord, and align to vector size (cacheline, for AVX-512).
		// We clear out vector-sized chunks.
		clearWorkList.emplace_back(clearStartPlotI % mPitch / kCoordVectorLength * kCoordVectorLength, clearStartPlotI / mPitch);
		visitSetRect.combine(iaabb2::sized(clearWorkList.back(), 1));
	}

	mStartPlots.clear();

	const std::span gcosts(mGCosts);
	const int pitch = mPitch;

	while (!clearWorkList.empty())
	{
		const ivec2 coord = clearWorkList.back();
		clearWorkList.pop_back();

		if (coord.x < 0 || coord.y < 0)
			continue;
		if (coord.x >= mPitch || coord.y * mPitch >= (ptrdiff_t)mGState.size())
			continue;

		if (gcosts.subspan(coord.x + coord.y * pitch).size() >= kCoordVectorLength)
		{
			const std::span fillSpan = gcosts.subspan(coord.x + coord.y * pitch).subspan<0, kCoordVectorLength>();

			// Check if any plots were visited here.
			if (!vcmplt(Vector(fillSpan), INT32_MAX).asBits())
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

	return visitSetRect.cast<int16_t>();
}

// Adds a plot to the open set.
void PQSimdAStar::addStart(int plotI, uint32_t state)
{
	mGCosts[plotI] = 0;
	mGState[plotI] = state;
	mHeap.push(0, plotI);
	mStartPlotI = plotI;
	mStartPlots.push_back(plotI);
}
// Adds multiple plots to the open set, with state = 0.
void PQSimdAStar::addStart(std::span<const int> plotIndices)
{
	for (const int plotI : plotIndices)
		addStart(plotI, 0);
	mStartPlotI = -1;
}

void PQSimdAStar::updateOpenSetHeuristicValues(const IGraph& g)
{
	updateHeuristic(g);
}

bool PQSimdAStar::search(int goalPlotI, const IGraph& g, int32_t maxCost)
{
	//if (mHeuristicUpdateEnabled)
	//	updateHeuristic(g);

	const int kAdjVector[]{
		-1 + -1 * mPitch,
		+0 + -1 * mPitch,
		+1 + -1 * mPitch,
		-1 + +0 * mPitch,
		//{ +0, +0 },
		+1 + +0 * mPitch,
		-1 + +1 * mPitch,
		+0 + +1 * mPitch,
		+1 + +1 * mPitch,
	};

	//unsigned int maxPQOverlap = 0;

	//std::ofstream heapLog;
	//if constexpr (kDumpHeapLog)
	//{
	//	heapLog.open("PQSimdAStar Heap Log.log");
	//	if (!heapLog)
	//		std::abort();
	//
	//	for (const auto& entry : mHeap.elements())
	//		heapLog << entry.key << '\n';
	//}

	[[maybe_unused]] Vector fromFCosts;

	while (!mHeap.empty() && (goalPlotI < 0 || mHeap.frontKey() <= mGCosts[goalPlotI]) && mHeap.frontKey() <= maxCost)
	//while (!mHeap.empty())
	{
		auto [plotIndexVector, iterationMask] = pop();

		//if constexpr (kDumpHeapLog)
		//	heapLog << "-1\n";

		// TODO: Proper boundary handling. For now, just don't path from boundary plots.
		const auto iterationMaskBits = iterationMask.asBits();
		Vector::Mask::Bits iterationMaskClearBits = 0;
		for (const size_t i : range(kCoordVectorLength))
		{
			const int plotI = plotIndexVector.getElement(i);
			if ((iterationMaskBits & (1 << i)) != 0 && (plotI < mPitch || plotI >= (ptrdiff_t)mGCosts.size() - mPitch || plotI % mPitch <= 0 || plotI % mPitch >= mPitch - 1))
			{
				iterationMaskClearBits |= Vector::Mask::Bits(1) << i;
				plotIndexVector.setElement(i, mPitch + 1);
			}
		}
		iterationMask &= Vector::Mask(std::bitset<kCoordVectorLength>(~iterationMaskClearBits));

		const Vector currentGCosts(mGCosts, plotIndexVector);
		const StateVector currentGState(mGState, plotIndexVector);

		//std::vector<std::tuple<Vector, Vector, Vector::Mask>> pushes;

		if constexpr (kTestHeuristicConsistency)
			fromFCosts = currentGCosts + g.getHeuristic(plotIndexVector);

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
				if constexpr (kEnableParentPointers)
					scatter(mParents, adjVector, plotIndexVector, adjPushMask);
				const Vector adjFCosts = newAdjGCosts + heuristic;
				push(adjVector, adjFCosts, adjPushMask);
				//pushes.emplace_back(adjVector, newAdjGCosts + heuristic, adjPushMask);

				//if (const Vector::Mask debugMask = (vcmpeq(adjVector, 18 + 16 * 64) | vcmpeq(adjVector, 19 + 17 * 64)) & adjPushMask; debugMask.asBits())
				//{
				//	for (const int i : range(kCoordVectorLength))
				//		if ((debugMask.asBits() >> i) & 1)
				//			std::osyncstream(std::clog) << "SimdAStar plot log for " << adjVector.getElement(i) << " (adj " << adjI << "): g=" << newAdjGCosts.getElement(i) << " state=" << stepState.getElement(i) << std::endl;
				//}

				//if constexpr (kDumpHeapLog)
				//{
				//	const Vector fcosts = adjFCosts;
				//	for (const int i : range(kCoordVectorLength))
				//		if ((adjPushMask.asBits() >> i) & 1)
				//			heapLog << fcosts.getElement(i) << '\n';
				//}

				if constexpr (kTestHeuristicConsistency)
				{
					const Vector::Mask inconsistentMask = vcmplt(adjFCosts, fromFCosts) & adjPushMask;
					if (inconsistentMask.asBits())
						std::abort();
				}
			}

			++adjI;
		}

		//int maxNewFCost = 0;
		//for (const auto& [adjVector, costs, mask] : pushes)
		//	for (const int i : range(kCoordVectorLength))
		//		if ((mask.bits >> i) & 1)
		//			maxNewFCost = std::max(maxNewFCost, costs.getElement(i));
		//
		//maxPQOverlap = std::max(maxPQOverlap, (unsigned int)std::ranges::count_if(mHeap, [maxNewFCost](Entry entry) {
		//	return maxNewFCost > entry.value;
		//	}));
		//
		//for (const auto& [adjVector, costs, mask] : pushes)
		//	push(adjVector, costs, mask);
	}

	//std::cout << maxPQOverlap << std::endl;

	return goalPlotI >= 0 && mGCosts[goalPlotI] < INT32_MAX;
}

void PQSimdAStar::push(Vector adjVector, Vector adjFCosts, Vector::Mask adjPushMask)
{
	/// Std heap
	//const unsigned int n = std::popcount(adjPushMask.asBits());
	//adjVector = hcompress(adjVector, adjPushMask);
	//adjFCosts = hcompress(adjFCosts, adjPushMask);
	////mHotQueuePlotIndices.append_range(std::span(*&adjVector.toArray()).subspan(0, n));
	////mHotQueueValues.append_range(std::span(*&adjFCosts.toArray()).subspan(0, n));
	//mHeap.append_range(std::views::zip(adjVector.toArray(), adjFCosts.toArray()) | std::views::take(n) | std::views::transform([](std::tuple<int32_t, int32_t> t) {
	//	return Entry{ std::get<1>(t), std::get<0>(t) };
	//	}));
	//
	//const std::span heapView = std::span(mHeap);
	//for (const size_t i : range(mHeap.size() - n, mHeap.size()))
	//	std::ranges::push_heap(heapView.subspan(0, i + 1), std::greater<>(), &Entry::value);

	/// DAry
	unsigned int bits = adjPushMask.asBits();
	while (bits)
	{
		const unsigned int i = std::countr_zero(bits);
		//bits &= ~(1u << i);
		bits &= bits - 1;
	
		//mHeap.push_back({ .value = adjFCosts.getElement(i), .plotI = adjVector.getElement(i) });
		//std::ranges::push_heap(mHeap, std::greater<>(), &Entry::value);

		mHeap.push(adjFCosts.getElement(i), adjVector.getElement(i));
	}
}
std::pair<PQSimdAStar::Vector, PQSimdAStar::Vector::Mask> PQSimdAStar::pop()
{
	//if (mHotQueuePlotIndices.size() < kCoordVectorLength && !mHeapPlotIndices.empty())
	//{
	//	mHotQueuePlotIndices.append_range(mHeapPlotIndices.front().toArray());
	//	mHotQueueValues.append_range(
	//}

	const size_t iterationWidth = kDumpHeapLog ? 1 : std::min<size_t>(mHeap.size(), kCoordVectorLength);
	const Vector::Mask iterationMask{ std::bitset<kCoordVectorLength>((1 << iterationWidth) - 1) };
	alignas(Vector) std::array<int32_t, kCoordVectorLength> plotIndices{};

	for (const size_t i : range(iterationWidth))
	{
		//plotIndices[i] = mHeap.front().plotI;
		//std::ranges::pop_heap(mHeap, std::greater<>(), &Entry::value);
		//mHeap.pop_back();

		plotIndices[i] = mHeap.pop().value;
	}

	if constexpr (kEnableStats)
		mNumExpanded += (unsigned int)iterationWidth;

	return { vselect(iterationMask, Vector(plotIndices), plotIndices[0]), iterationMask };
}
void PQSimdAStar::updateHeuristic(const IGraph& g)
{
	//for (auto&& [plot, value] : std::views::zip(mHotQueuePlotIndices, mHotQueueValues))
	//	value = mGCosts[plot] + g.getHeuristic(plot).getElement(0);
	//for (auto&& [plots, value] : std::views::zip(mHeapPlotIndices, mHeapValues))
	//	value = hmin(Vector(mGCosts, plots) + g.getHeuristic(plots));
	//
	//std::ranges::stable_sort(std::views::zip(mHotQueuePlotIndices, mHotQueueValues), std::less<>(), [](const std::tuple<const int32_t&, const int32_t&>& t) { return std::get<1>(t); });
	//std::ranges::make_heap(std::views::zip(mHeapPlotIndices, mHeapValues), std::greater<>(), [](const std::tuple<const Vector&, const int32_t&>& t) { return std::get<1>(t); });
	
	//for (Entry& entry : mHeap)
	//	entry.value = mGCosts[entry.plotI] + g.getHeuristic(entry.plotI).getElement(0);
	//std::ranges::make_heap(mHeap, std::greater<>(), &Entry::value);

	for (auto& entry : mHeap.elements())
	{
		//for (auto&& [key, value] : std::views::zip(entry.keys, entry.values))
			//if (key < INT_MAX)
		assert((size_t)entry.value < mGCosts.size());
		entry.key = mGCosts[entry.value] + g.getHeuristic(entry.value).getElement(0);
	}
	//mHeap.topKey = mGCosts[mHeap.topValue] + g.getHeuristic(mHeap.topValue).getElement(0);
	mHeap.reheapify();
}

//void PQSimdAStar::setHeuristicUpdateEnabled(bool x)
//{
//	mHeuristicUpdateEnabled = x;
//}

std::optional<int> PQSimdAStar::findMostDistantPlot() const
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

std::vector<i16vec2> PQSimdAStar::reconstructPath(int startPlotI, int goalPlotI, const IGraph& g) const
{
	if (!isVisited(goalPlotI))
		return {};

	int plotI = goalPlotI;

	const auto toPlotCoord = [pitch = mPitch](int plotI) {
		return ivec2(plotI % pitch, plotI / pitch);
		};

	std::vector<i16vec2> path{ (i16vec2)toPlotCoord(plotI) };

	if constexpr (kEnableParentPointers)
	{
		int accPathCostToGoal = 0;
		while (plotI != startPlotI)
		{
			const int32_t parent = mParents[plotI];
			const ivec2 fromCoord = toPlotCoord(parent);
			const ivec2 toCoord = toPlotCoord(plotI);
			const ivec2 d = toCoord - fromCoord;
			const int adjI1 = (d.x + 1) + (d.y + 1) * 3;
			const int adjI2 = adjI1 - (adjI1 >= 4);
			if (fromCoord + kAdj[adjI2] != toCoord)
				std::abort();
			const auto [stepCosts, stepState] = g.getStepCost(mGState[parent], parent, plotI, adjI2);
			const int32_t expectedGCost = mGCosts[parent] + stepCosts.getElement(0);
			const int plotH = g.getHeuristic(plotI).getElement(0);
	
			if constexpr (kStrictPathReconstruction)
			{
				if (expectedGCost != mGCosts[plotI] || stepState.getElement(0) != mGState[plotI]
					|| accPathCostToGoal < plotH)
				{
					const ivec2 startCoord = toPlotCoord(startPlotI);
					const ivec2 goalCoord = toPlotCoord(goalPlotI);
					const int parentH = g.getHeuristic(parent).getElement(0);

					std::clog << "Path reconstruction step mismatch!" << std::endl;
					if (accPathCostToGoal < plotH)
						std::clog << "Heuristic was not admissible." << std::endl;
					std::clog << "From " << fromCoord << " to " << toCoord << std::endl;
					std::clog << "Start " << startCoord << " to goal " << goalCoord << std::endl;
					std::clog << "AdjI = " << adjI2 << std::endl;
					std::clog << "From cost:state = " << mGCosts[parent] << ':' << mGState[parent] << std::endl;
					std::clog << "Stepped  cost:state = " << expectedGCost << ':' << stepState.getElement(0) << std::endl;
					std::clog << "Expected cost:state = " << mGCosts[plotI] << ':' << mGState[plotI] << std::endl;
					std::clog << "Cost(start, goal): " << mGCosts[goalPlotI] << std::endl;
					std::clog << "Cost(to, goal)     : " << accPathCostToGoal << std::endl;
					std::clog << "AccCost(from, goal): " << accPathCostToGoal + stepCosts.getElement(0) << std::endl;
					std::clog << "h(to, goal)        : " << plotH << std::endl;
					std::clog << "h(from, goal)      : " << parentH << std::endl;
					std::clog << "From plot fcost: " << mGCosts[parent] + parentH << std::endl;
					std::abort();
				}
			}
	
			plotI = parent;
			path.push_back(toPlotCoord(plotI));
	
			accPathCostToGoal += stepCosts.getElement(0);
	
			// Sanity check
			if (path.size() > 1'000'000)
				std::abort();
		}
	}
	else
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

		while (plotI != startPlotI)
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
			int bestThisGCostI;
			if constexpr (kStrictPathReconstruction)
			{
				bestThisGCostI = std::countr_zero(vcmpeq(vselect(adjMask, thisGCosts, AdjVector(INT32_MAX)), mGCosts[plotI]).asBits());
				if (bestThisGCostI >= 8)
					std::abort();
			}
			else
			{
				const AdjVector maskedGCosts = vselect(adjMask, thisGCosts, AdjVector(INT32_MAX));
				const int bestThisGCost = hmin(maskedGCosts);
				if (bestThisGCost >= INT32_MAX)
					std::abort();
				bestThisGCostI = std::countr_zero(vcmpeq(bestThisGCost, maskedGCosts).asBits());
			}
			plotI = adjPlotIndices.getElement(bestThisGCostI);
			path.push_back(toPlotCoord(plotI));

			// Sanity check
			if (path.size() > 1'000'000)
				std::abort();
		}
	}
	std::ranges::reverse(path);

	return path;
}

void PQSimdAStar::dumpStats(std::ostream& os) const
{
	if constexpr (kEnableStats)
	{
		// Ideally, this is the same as number of isVisited.
		os << "Num plots expanded:" << mNumExpanded << std::endl;

		const size_t numVisited = std::ranges::count_if(mGCosts, [](int32_t g) { return g < INT32_MAX; });
		os << "Num plots visited:" << numVisited << std::endl;
	}
}