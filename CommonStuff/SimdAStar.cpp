#include "inc/CommonStuff/SimdAStar.h"
#include "inc/CommonStuff/CompilerMacros.h"
#include "inc/CommonStuff/range.h"

#include <iostream>

//inline constexpr int kGlobalVectorLength = kCoordVectorLength / 2;
static constexpr bool kEnableStats = false;
// Merge buckets. Doesn't help much.
// *Mask is actually reasonably occupied according to stats when using the landmark heuristic.
// *Otherwise, reduces performance because of nodes revisits if you allow later queues to be used.
static constexpr bool kEnableIterationOccupancyEnhancement = false;

static constexpr bool kEnableSorting = false;

static constexpr bool kUseParentPointers = false;

static constexpr bool kUseQueuesAsQueues = true;

// Not implemented for AVX256.
//static constexpr bool kEnableDeduplicationInPush = false;

static constexpr std::integral_constant<size_t, 15> kFCostGranularityShift{};
static constexpr int kFCostGranularity = 1u << kFCostGranularityShift;

using namespace heck;

SimdAStar::SimdAStar(ivec2 dim, int startPlotI, uint32_t initialState)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mParents(dim.x * dim.y, -1)
	, mPitch(dim.x)
{
	mStartPlotI = startPlotI;
	initQueues();
	mHotQueues.front().plots.push_back(startPlotI);
	mNumActiveQueues = 1;
	mGCosts[startPlotI] = 0;
	mGState[startPlotI] = initialState;
	mFirstHotQueueGranularCost = 0;
}

SimdAStar::SimdAStar(ivec2 dim, std::span<const int> startPlotIndices)
	: mGCosts(dim.x * dim.y, INT32_MAX)
	, mGState(dim.x * dim.y)
	, mParents(dim.x * dim.y, -1)
	, mPitch(dim.x)
{
	mStartPlotI = startPlotIndices[0];
	initQueues();
	mHotQueues.front().plots.append_range(startPlotIndices);
	mNumActiveQueues = 1;
	for (const int plotI : startPlotIndices)
		mGCosts[plotI] = 0;
	mFirstHotQueueGranularCost = 0;
}


bool SimdAStar::search(int goalPlotI, const IGraph& g, int32_t costLimit)
{
	if (mHeuristicUpdateEnabled)
		updateHeuristic(g);

	if constexpr (kEnableStats)
		mFCostQuantRangeCounts = {};

	//simd::Vector<int32_t, kCoordVectorLength> consecutiveIndices;
	//for (const int i : range(kCoordVectorLength))
	//	consecutiveIndices.setElement(i, i);

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

	// Okay, so we need to get these adj plots into the queues.
	// Method 1: Manual loop. Sometimes >2.0ms.
	// Method 2: For each queue index, compress, store, and append. >2.36ms
	// Method 3: Append to temp buckets. >2.26ms
	// Method 4: Batched push. >4.4ms
	// Method 5: Compress store.
	//
	// Method 1 is fastest. Just can't make it faster.
	// Unrolling anything just isn't worth it.

#define HECK_SIMD_ASTAR_PUSH_METHOD 1

#if HECK_SIMD_ASTAR_PUSH_METHOD == 3
	constexpr size_t kElementsPerTempBucket = kCoordVectorLength * std::size(kAdjVector);
	alignas(Vector) int32_t tempBuckets[kNumHotQueues * kElementsPerTempBucket]{};
	simd::Vector<int32_t, kNumHotQueues> tempBucketsLengths{};
#endif

#if HECK_SIMD_ASTAR_PUSH_METHOD == 4
	Vector adjFCostsQuantArray[8];
	Vector adjVectorArray[8];
	// __m512i adjConflictArray[8];
#endif

	// TODO: Need to handle start == goal
	while (mNumActiveQueues)
	{
		if (goalPlotI >= 0
			&& (mFirstHotQueueGranularCost * kFCostGranularity > mGCosts[goalPlotI]
			|| mFirstHotQueueGranularCost * kFCostGranularity > costLimit))
			break;

		HotQueue& currentHotQueue = mHotQueues.front();

		if constexpr (kEnableIterationOccupancyEnhancement)
		{
			if (currentHotQueue && currentHotQueue.plots.size() - kCoordVectorLength < kCoordVectorLength / 2 && mNumActiveQueues > 1)
			{
				for (int i = 1; i < 2; ++i)
				{
					if (mHotQueues[i].plots.size() - kCoordVectorLength >= kCoordVectorLength)
					{
						//#ifdef _MSC_VER
						mHotQueues[0].plots.append_range(std::span(mHotQueues[i].plots).subspan(kCoordVectorLength));
						//#else
						//mHotQueues[0].plots.insert(mHotQueues[0].plots.end(), mHotQueues[i].plots.begin() + kCoordVectorLength, mHotQueues[i].plots.end());
						//#endif
						mHotQueues[i].plots.resize(kCoordVectorLength);
						--mNumActiveQueues;
						break;
					}
				}
			}
		}

		if (!currentHotQueue)
		{
			std::ranges::rotate(mHotQueues, mHotQueues.begin() + 1);
			if constexpr (kUseQueuesAsQueues)
			{
				mHotQueues.back().plots.resize(kCoordVectorLength);
				mHotQueues.back().offset = kCoordVectorLength;
			}
			++mFirstHotQueueGranularCost;
			continue;
		}

		if constexpr (kEnableSorting)
		{
			std::ranges::stable_sort(std::span(currentHotQueue.plots).subspan(kCoordVectorLength), std::less<>(), [&](int plotI) {
				return mGCosts[plotI] + g.getHeuristic(plotI).getElement(0);
				});
		}

		//if (currentHotQueue.plots.size() > 10'000)
		//{
		//	auto plots = currentHotQueue.plots;
		//	std::ranges::sort(plots);
		//	plots.erase(std::ranges::unique(plots).begin(), plots.end());
		//	mMaxDuplication = std::max(mMaxDuplication, unsigned(currentHotQueue.plots.size() - plots.size()));
		//}

		const auto [plotIndexVector, iterationMask] = currentHotQueue.pop();

		//if (vcmpeq(plotIndexVector, 260 + 203 * mPitch).asBits() && mGCosts[260 + 203 * mPitch] == 432581)
		//	__debugbreak();

		if constexpr (kEnableStats)
			++mIterationMaskPopCounts[std::popcount(iterationMask.asBits())];

		if (!currentHotQueue)
			--mNumActiveQueues;

		const Vector currentGCosts(mGCosts, plotIndexVector);
		const StateVector currentGState(mGState, plotIndexVector);

#if HECK_SIMD_ASTAR_PUSH_METHOD == 3
		tempBucketsLengths = {};
#endif
#if HECK_SIMD_ASTAR_PUSH_METHOD == 4
		Vector::Mask adjPushMaskArray[8]{};
		
#endif
		int adjI = 0;
		for (const int adjOffset : kAdjVector)
		{
			const Vector adjVector = plotIndexVector + Vector(adjOffset);

			const Vector oldAdjGCosts(mGCosts, adjVector); // gather op, give this lots of time!
			const auto [stepCosts, stepState] = g.getStepCost(currentGState, plotIndexVector, adjVector, adjI);
			const Vector::Mask adjDomainMask = vcmplt(0, stepCosts);
			const Vector newAdjGCosts = currentGCosts + stepCosts;

			const Vector::Mask adjPushMask = vcmplt(newAdjGCosts, oldAdjGCosts) & iterationMask & adjDomainMask;
			// Is this worth checking?
			// *Yes it is.
			if (adjPushMask.asBits())
			{
				const Vector heuristic = g.getHeuristic(adjVector);

				//if (adjPushMask.asBits())
				{
					scatter(mGCosts, adjVector, newAdjGCosts, adjPushMask);
					scatter(mGState, adjVector, stepState, adjPushMask);
					if constexpr (kUseParentPointers)
						scatter(mParents, adjVector, plotIndexVector, adjPushMask);

					const Vector adjFCosts = newAdjGCosts + heuristic;

#if HECK_SIMD_ASTAR_PUSH_METHOD == 1
					push(adjVector, adjFCosts, adjPushMask);
#else
					
					Vector adjFCostsQuant = adjFCosts >> kFCostGranularityShift;
					adjFCostsQuant = vmin(vmax(adjFCostsQuant - mFirstHotQueueGranularCost, 0), kNumHotQueues - 1);

					const Vector maskedAdjFCostsQuant = vselect(adjPushMask, adjFCostsQuant, simd::Ops<simd::Avx512IntegerReg, uint32_t>::initAllBits());

#if HECK_SIMD_ASTAR_PUSH_METHOD == 2
					{
						// Method 2

						//const __m512i conflict = _mm512_conflict_epi32(maskedAdjFCostsQuant.reg);
						//const __m512i bucketOffsets = _mm512_popcnt_epi32(conflict);
						const int maxQueueI = _mm512_reduce_max_epi32(maskedAdjFCostsQuant.reg.bits);
						for (const int queueI : range(maxQueueI + 1))
						{
							//if (!vcmplt(queueI - 1, maskedAdjFCostsQuant).bits)
							//	break;

							HotQueue& queue = mHotQueues[queueI];

							const Vector::Mask mask = vcmpeq(maskedAdjFCostsQuant, queueI);
							if (mask.bits)
							{
								const int n = std::popcount(mask.bits);

								//queue.plots.resize(queue.plots.size() + n);
								//scatter(std::span(queue.plots).subspan(queue.plots.size() - n), bucketOffsets, adjVector, mask);

								// Slightly faster than scatter.
								alignas(__m512i) int32_t newPlotIndices[kCoordVectorLength]{};
								_mm512_store_si512(newPlotIndices, _mm512_maskz_compress_epi32(mask.bits, adjVector.reg.bits));
								if (!queue)
									++mNumActiveQueues;

								queue.plots.append_range(std::span(newPlotIndices).subspan(0, n));
							}
						}
					}
#elif HECK_SIMD_ASTAR_PUSH_METHOD == 3
					{
						const __m512i conflictBits = _mm512_conflict_epi32(maskedAdjFCostsQuant.reg.bits); // can't use masked conflict as it doesn't maskthe source

						const simd::Avx512IntegerReg scatterBaseBucketIndices{ _mm512_permutexvar_epi32(adjFCostsQuant.reg.bits, _mm512_castsi256_si512(tempBucketsLengths.reg.bits)) };

						const simd::Avx512IntegerReg bucketOffsetIncs{ _mm512_popcnt_epi32(conflictBits) };

						//simd::Vector<int32_t, kNumHotQueues> tempBucketsLengths2 = tempBucketsLengths;
						//for (int i = 0; i < 16; ++i)
						//	if ((adjPushMask.asBits() >> i) & 1)
						//	{
						//		assert(bucketOffsetIncs.m512i_i32[i] == std::popcount((unsigned)conflictBits.m512i_i32[i]));
						//		//tempBuckets[adjFCostsQuant.getElement(i) * kElementsPerTempBucket + tempBucketsLengths2.getElement(adjFCostsQuant.getElement(i))] = adjVector.getElement(i);
						//		tempBuckets[adjFCostsQuant.getElement(i) * kElementsPerTempBucket + tempBucketsLengths2.getElement(adjFCostsQuant.getElement(i)) + bucketOffsetIncs.m512i_i32[i]] = adjVector.getElement(i);
						//		tempBucketsLengths2.reg.m256i_i32[adjFCostsQuant.getElement(i)]++;
						//	}

						const Vector scatterIndices = (adjFCostsQuant * kElementsPerTempBucket + scatterBaseBucketIndices) + bucketOffsetIncs;
						scatter(tempBuckets, scatterIndices, adjVector, adjPushMask);
						////scatter(tempBucketsLengths, adjFCostsQuant, Vector(_mm512_add_epi32(bucketOffsetIncs, _mm512_set1_epi32(1))), adjPushMask);

						//// Convert v[i] = bucketI into v[bucketI] = sum(v[i] == bucketI).
						//// Not so simple. Really, it would be nice if you could scatter into a register (inverse permute).
						//// Instead, translate each element into a bit. There are 32 bits per element, and with 8 queues, you have 4 bits per count.
						//// Could overflow if all 16 elements go to the same queue, which we'll handle later.
						//// With the bit masks, you can then simply do a hadd to a single 32-bit int.
						//// Then expand the 4-bit numbers to 8x32-bit register (__m256i) using a shift.
						//// Then fix overflow.
						//
						////for (int i = 0; i < 16; ++i)
						////	if ((adjPushMask.asBits() >> i) & 1)
						////		++tempBucketsLengths.reg.m256i_i32[adjFCostsQuant.getElement(i)];
						//
						static_assert(kNumHotQueues == 8);
						const __m512i adjFCostsQuant0 = _mm512_cvtepi32_epi64(_mm512_castsi512_si256(adjFCostsQuant.reg.bits));
						const __m512i adjFCostsQuant1 = _mm512_cvtepi32_epi64(_mm512_extracti64x4_epi64(adjFCostsQuant.reg.bits, 1));
						const __m512i bucketBits0 = _mm512_maskz_sllv_epi64(adjPushMask.asBits() & 0xFF, _mm512_set1_epi64(1), _mm512_slli_epi64(adjFCostsQuant0, 3));
						const __m512i bucketBits1 = _mm512_maskz_sllv_epi64(adjPushMask.asBits() >> 8, _mm512_set1_epi64(1), _mm512_slli_epi64(adjFCostsQuant1, 3));


						// A byte for each count.
						const uint64_t sumBytes = _mm512_reduce_add_epi64(_mm512_add_epi64(bucketBits0, bucketBits1));
						const __m128i sumBytes128 = _mm_set_epi64x(0, sumBytes);
						const __m256i sumInts = _mm256_cvtepu8_epi32(sumBytes128);
						tempBucketsLengths.reg.bits = _mm256_add_epi32(tempBucketsLengths.reg.bits, sumInts);

						// One step of hadd, to get the bucket sums into even 32-bit element indices.
						// sumNibbles1[0] = bucketBits[0] + bucketBits[1]
						// sumNibbles1[2] = bucketBits[2] + bucketBits[3]
						// etc.
						//const __m512i sumNibbles1 = _mm512_add_epi32(bucketBits, _mm512_unpackhi_epi32(bucketBits, bucketBits));
						//// Now, expand nibbles to bytes, then each set of sums takes 64 bits and we don't have to worry about count overflow.
						//
						//const uint32_t sumNibbles = _mm512_reduce_add_epi32(bucketBits);
						//const __m256i sumInts = _mm256_and_epi32(_mm256_srlv_epi32(_mm256_set1_epi32(sumNibbles), _mm256_set_epi32(
						//	0 * 4,
						//	1 * 4,
						//	2 * 4,
						//	3 * 4,
						//	4 * 4,
						//	5 * 4,
						//	6 * 4,
						//	7 * 4
						//)), _mm256_set1_epi32(16 - 1));
						//const __m256i sumIntsFixedOverflow = _mm256_and_epi32(_mm256_cmpeq_epi32(sumInts, _mm256_setzero_si256()), _mm256_set1_epi32(16));
						//
						//tempBucketsLengths.reg = _mm256_mask_add_epi32(tempBucketsLengths.reg, 

						//	_mm512_permutexvar_epi32(adjFCostsQuant.reg, _mm512_set_epi32(
						//	1u << (4 * 0),
						//	1u << (4 * 1),
						//	1u << (4 * 2),
						//	1u << (4 * 3),
						//	1u << (4 * 4),
						//	1u << (4 * 5),
						//	1u << (4 * 6),
						//	1u << (4 * 7),
						//));
						// Take the bit masks and order them using a shuffle.
						// This instruction does 64 bits, but we have 128 bits in total (16 plots * 8 queues). So do four queues at a time.


						// Three scatters and conflict. How to write fast AVX algorithms. /s
					}
#elif HECK_SIMD_ASTAR_PUSH_METHOD == 4
					{
						adjFCostsQuantArray[adjI] = vselect(adjPushMask, adjFCostsQuant, simd::Ops<simd::Avx512IntegerReg, uint32_t>::initAllBits());
						adjVectorArray[adjI] = adjVector;
						adjPushMaskArray[adjI] = adjPushMask;
						//adjConflictArray[adjI] = _mm512_conflict_epi32(adjFCostsQuantArray[adjI].reg);
					}
#elif HECK_SIMD_ASTAR_PUSH_METHOD == 5
					{
						//for (const int i : range((int)hmin(simd::Vector<uint32_t, kCoordVectorLength>(maskedAdjFCostsQuant.reg)), (int)hmax(maskedAdjFCostsQuant) + 1))
						for (const int i : range((int)hmax(maskedAdjFCostsQuant) + 1))
						{
							HotQueue& q = mHotQueues[i];
							const Vector::Mask mask = vcmpeq(maskedAdjFCostsQuant, i);
							const int n = std::popcount(mask.asBits());
							if (mask.bits)
							{
								if (!q)
									++mNumActiveQueues;
								q.plots.resize(q.plots.size() + n);
								_mm512_mask_compressstoreu_epi32(q.plots.data() + q.plots.size() - n, mask.bits, adjVector.reg.bits);
							}
						}
					}
#endif
#endif
				}
			}

//#if HECK_SIMD_ASTAR_PUSH_METHOD == 4
				
//#endif
			++adjI;
		}

#if HECK_SIMD_ASTAR_PUSH_METHOD == 3
		{
			auto& hotQueues = mHotQueues;
			for (int i = 0; i < kNumHotQueues; ++i)
			{
				if (const int length = tempBucketsLengths.getElement(i))
				{
					if (!hotQueues[i])
						++mNumActiveQueues;
					hotQueues[i].plots.append_range(std::span(tempBuckets).subspan(i * kElementsPerTempBucket, length));
				}
			}
		}
#endif

#if HECK_SIMD_ASTAR_PUSH_METHOD == 4
		{
			// adjFCostsQuantArray[adjI] = vselect(adjPushMask, adjFCostsQuant, {});
			// adjVectorArray[adjI] = adjVector;
			// adjPushMaskArray[adjI] = adjPushMask;
			const int maxQuant = hmax(vmax(vmax(
				vmax(adjFCostsQuantArray[0], adjFCostsQuantArray[1]),
				vmax(adjFCostsQuantArray[2], adjFCostsQuantArray[3])), vmax(
					vmax(adjFCostsQuantArray[4], adjFCostsQuantArray[5]),
					vmax(adjFCostsQuantArray[6], adjFCostsQuantArray[7]))
			));


			for (const int adjJ : range(8))
			{
				//const __m512i conflictBits = adjConflictArray[adjJ];
				const __m512i conflictBits = _mm512_conflict_epi32(adjFCostsQuantArray[adjI].reg.bits);
				const __m512i offsetBits = _mm512_popcnt_epi32(conflictBits);
				for (const int queueI : range(maxQuant + 1))
				{
					HotQueue& queue = mHotQueues[queueI];

					const Vector::Mask mask = vcmpeq(adjFCostsQuantArray[adjJ], queueI) & adjPushMaskArray[adjJ];
					//if (const int n = std::popcount(mask.bits))
					if (mask.bits)
					{
						if (!queue)
							++mNumActiveQueues;
						//queue.push(adjVectorArray[adjJ], mask);

						unsigned int bits = mask.asBits();
						while (bits)
						{
							const unsigned int i = std::countr_zero(bits);
							bits &= bits - 1;
							queue.plots.push_back(adjVectorArray[adjJ].getElement(i));
						}

						//queue.plots.resize(queue.plots.size() + n);
						//scatter(std::span(queue.plots).subspan(queue.plots.size() - n), offsetBits, adjVectorArray[adjJ], mask);

						//const __m512i scatterBaseBucketIndices = _mm512_permutexvar_epi32(adjFCostsQuant.reg, _mm512_castsi256_si512(tempBucketsLengths.reg));



						//const __m512i bucketOffsetIncs = _mm512_popcnt_epi32(conflictBits);
					}

				}
			}
		}
#endif
	}

	return goalPlotI >= 0 && isVisited(goalPlotI);
}

bool SimdAStar::searchUsingPriorityQueue(int goalPlotI, const IGraph& g)
{
	struct Entry
	{
		int32_t heapValue = 0;
		int32_t plotI = 0;

		bool operator<(Entry b) const
		{
			return heapValue > b.heapValue;
		}
	};

	std::vector<Entry> heap;

	// Update heuristic. Merge all onto first queue.
	for (const size_t queueI : range(kNumHotQueues))
	{
		heap.append_range(std::span(mHotQueues[queueI].plots).subspan(0, kCoordVectorLength) | std::views::transform([&g](int32_t plotI) {
			return Entry{
				g.getHeuristic(plotI).getElement(0),
				plotI,
			};
			}));
	}

	std::ranges::make_heap(heap, std::less<>());

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

	while (!heap.empty())
	{
		if (goalPlotI >= 0 && heap.front().heapValue >= mGCosts[goalPlotI])
			break;

		const int32_t fromPlotI = heap.front().plotI;
		std::ranges::pop_heap(heap, std::less<>());
		heap.pop_back();

		const int32_t fromGCost = mGCosts[fromPlotI];
		const uint32_t fromState = mGState[fromPlotI];

		if constexpr (kEnableStats)
			++mIterationMaskPopCounts[1];

		for (const int adjI : range(8))
		{
			const int32_t adjPlotI = fromPlotI + kAdjVector[adjI];
			const IGraph::Step step = g.getStepCost(fromState, fromPlotI, adjPlotI, adjI);
			if (step.cost.getElement(0) > 0)
			{
				const int32_t adjGCost = fromGCost + step.cost.getElement(0);
				if (adjGCost < mGCosts[adjPlotI])
				{
					mGCosts[adjPlotI] = adjGCost;
					mGState[adjPlotI] = step.state.getElement(0);
					if constexpr (kUseParentPointers)
						mParents[adjPlotI] = fromPlotI;
					const int32_t heuristic = g.getHeuristic(adjPlotI).getElement(0);
					heap.push_back({ adjGCost + heuristic, adjPlotI });
					std::ranges::push_heap(heap, std::less<>());
				}
			}
		}
	}

	return goalPlotI >= 0 && isVisited(goalPlotI);
}

void SimdAStar::setHeuristicUpdateEnabled(bool x)
{
	mHeuristicUpdateEnabled = x;
}

std::optional<int> SimdAStar::findMostDistancePlot() const
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

std::vector<int> SimdAStar::reconstructPath(int goalPlotI, const IGraph& g) const
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

	if constexpr (kUseParentPointers)
	{
		int accPathCostToGoal = 0;
		do
		{
			const int32_t parent = mParents[plotI];
			const ivec2 fromCoord{ parent % mPitch, parent / mPitch };
			const ivec2 toCoord{ plotI % mPitch, plotI / mPitch };
			const ivec2 d = toCoord - fromCoord;
			const int adjI1 = (d.x + 1) + (d.y + 1) * 3;
			const int adjI2 = adjI1 - (adjI1 >= 4);
			if (fromCoord + kAdjVector[adjI2] != toCoord)
				std::abort();
			const auto [stepCosts, stepState] = g.getStepCost(mGState[parent], parent, plotI, adjI2);
			const int32_t expectedGCost = mGCosts[parent] + stepCosts.getElement(0);
			const int plotH = g.getHeuristic(plotI).getElement(0);

			if (expectedGCost != mGCosts[plotI] || stepState.getElement(0) != mGState[plotI]
				|| accPathCostToGoal < plotH)
			{
				const ivec2 startCoord{ mStartPlotI % mPitch, mStartPlotI / mPitch };
				const ivec2 goalCoord{ goalPlotI % mPitch, goalPlotI / mPitch };
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
				std::clog << "First hot queue value: " << mFirstHotQueueGranularCost * kFCostGranularity << std::endl;
				std::clog << "From plot fcost: " << mGCosts[parent] + parentH << std::endl;
				std::abort();
			}

			plotI = parent;
			path.push_back(plotI);

			accPathCostToGoal += stepCosts.getElement(0);

			// Sanity check
			if (path.size() > 1'000'000)
				std::abort();
		} while (plotI != mStartPlotI);
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

		while (plotI != mStartPlotI)
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
		}
	}
	std::ranges::reverse(path);

	return path;
}

void SimdAStar::dumpStats([[maybe_unused]] std::ostream& os) const
{
	if constexpr (kEnableStats)
	{
		os << "One-queue pushes: " << mNumOneQueuePushes << std::endl;
		os << "Three-queue pushes: " << mNumThreeQueuePushes << std::endl;
		os << "Manual pushes: " << mNumManualPushes << std::endl;
		os << "FCost quant ranges:" << std::endl;
		for (const int i : range(kNumHotQueues))
			os << '\t' << i << ": " << mFCostQuantRangeCounts[i] << std::endl;
		os << "Iteration mask pop counts:" << std::endl;
		int numExpanded = 0;
		for (const int i : range(kCoordVectorLength + 1))
		{
			os << '\t' << i << ": " << mIterationMaskPopCounts[i] << std::endl;
			numExpanded += mIterationMaskPopCounts[i] * i;
		}
		// Ideally, this is the same as number of isVisited.
		os << "Num plots expanded:" << numExpanded << std::endl;

		const size_t numVisited = std::ranges::count_if(mGCosts, [](int32_t g) { return g < INT32_MAX; });
		os << "Num plots visited:" << numVisited << std::endl;
		os << "Max duplication: " << mMaxDuplication << std::endl;
	}
}

SimdAStar::HotQueue::HotQueue() : offset(kCoordVectorLength)
{
	// Ensure at least this many entries so that vector loads don't need bounds checks.
	plots.resize(kCoordVectorLength);
}

std::pair<SimdAStar::Vector, SimdAStar::Vector::Mask> SimdAStar::HotQueue::at(size_t i) const
{
	assert(i >= offset - kCoordVectorLength);
	Vector v{ std::span(plots).subspan(i).subspan<0, kCoordVectorLength>() };
	if constexpr (kUseQueuesAsQueues)
		if (i >= offset - kCoordVectorLength)
			return { v, Vector::Mask::kAll << (offset - std::min<size_t>(i, offset)) };
		else
			return {};
	else
		return { v, Vector::Mask::kAll << (kCoordVectorLength - std::min<size_t>(i, kCoordVectorLength)) };
}

std::pair<SimdAStar::Vector, SimdAStar::Vector::Mask> SimdAStar::HotQueue::pop()
{
	if constexpr (kUseQueuesAsQueues)
	{
		const size_t loadStart = std::min(offset, plots.size() - kCoordVectorLength);
		const size_t maskStart = offset;
		Vector::Mask mask = Vector::Mask::kAll << (maskStart - loadStart);
		Vector v{ std::span(plots).subspan(loadStart).subspan<0, kCoordVectorLength>() };
		offset = loadStart + kCoordVectorLength;

		//if constexpr (kEnableDeduplicationInPop)
		//{
		//	// Improves things, but still pretty bad in the worst case.
		//	const __m512i conflict = _mm512_conflict_epi32(vselect(mask, v, -1).reg.bits);
		//	mask = mask & ~vtest(Vector(heck::simd::Avx512IntegerReg(conflict)), UINT32_MAX);
		//}

		return { v, mask };
	}
	else
	{
		const size_t start = plots.size() - kCoordVectorLength;
		const size_t eraseStart = std::max<size_t>(start, kCoordVectorLength);
		Vector v{ std::span(plots).subspan(start).subspan<0, kCoordVectorLength>() };
		plots.erase(plots.begin() + eraseStart, plots.end());

		Vector::Mask mask = Vector::Mask::kAll << (eraseStart - start);

		//if constexpr (kEnableDeduplicationInPop)
		//{
		//	// Improves things, but still pretty bad in the worst case.
		//	const __m512i conflict = _mm512_conflict_epi32(vselect(mask, v, -1).reg.bits);
		//	mask = mask & ~vtest(Vector(heck::simd::Avx512IntegerReg(conflict)), UINT32_MAX);
		//}

		return { v, mask };
	}
}

//void SimdAStar::HotQueue::push(Vector plotIndices, Vector::Mask mask)
//{
//	plotIndices = hcompress(plotIndices, mask);
//	const int n = std::popcount(mask.asBits());
//	const auto array = plotIndices.toArray();
//	plots.append_range(std::span(array).subspan(0, n));
//}

SimdAStar::HotQueue::operator bool() const
{
	if constexpr (kUseQueuesAsQueues)
		return plots.size() > offset;
	else
		return plots.size() > kCoordVectorLength;
}

void SimdAStar::initQueues()
{
	mNumActiveQueues = 0;
	mHotQueues = {};
	for (HotQueue& q : mHotQueues)
		std::ranges::fill(q.plots, mStartPlotI);
}

HECK_FORCEINLINE void SimdAStar::push(Vector adjVector, Vector adjFCosts, Vector::Mask adjPushMask)
{
	Vector adjFCostsQuant = adjFCosts >> kFCostGranularityShift;

	adjFCostsQuant = vmin(vmax(adjFCostsQuant - mFirstHotQueueGranularCost, 0), kNumHotQueues - 1);

	//if constexpr (kEnableDeduplicationInPush)
	//{
	//	// Improves things, but still pretty bad in the worst case.
	//	const __m512i conflict = _mm512_conflict_epi32(vselect(adjPushMask, adjVector, -1).reg.bits);
	//	adjPushMask = adjPushMask & ~vtest(Vector(heck::simd::Avx512IntegerReg(conflict)), UINT32_MAX);
	//}

	if constexpr (kEnableStats)
	{
		if (adjPushMask.asBits())
		{
			//++mFCostQuantRangeCounts[std::min(hmax(vselect(adjPushMask, adjFCostsQuant, -1)) - hmin(vselect(adjPushMask, adjFCostsQuant, INT_MAX)) + 1, kNumHotQueues - 1)];

			for (const int i : range(kCoordVectorLength))
				mFCostQuantRangeCounts[adjFCostsQuant.getElement(i)] += (adjPushMask.asBits() >> i) & 1;

		}
		else
		{
			//++mFCostQuantRangeCounts[0];
			return;
		}
	}



	//[[maybe_unused]] const int32_t firstObliviousFCostQuant = hcompress(adjFCostsQuant, adjPushMask).getElement<0>();

	// Check the range of quant costs, ignoring the push mask.
	//if ((vcmpeq(adjFCostsQuant, firstObliviousFCostQuant) & adjPushMask).asBits() == adjPushMask.asBits())
	//{
	//	// All equal.
	//	// One-queue.
	//	++mNumOneQueuePushes;
	//	const int queueI = std::clamp(firstObliviousFCostQuant - mFirstHotQueueGranularCost, 0, kNumHotQueues - 1);
	//	if (!mHotQueues[queueI])
	//		++mNumActiveQueues;
	//	mHotQueues[queueI].push(adjVector, adjPushMask);
	//}
	//else if (vcmplt(UVector(adjFCostsQuant - (firstObliviousFCostQuant - 1)), 3).all())
	//{
	//	// firstObliviousFCostQuant - 1 and firstObliviousFCostQuant and firstObliviousFCostQuant + 1
	//	// Three-queue.
	//	++mNumThreeQueuePushes;
	//	for (const int d : range(-1, 2))
	//	{
	//		const int queueI = std::clamp(firstObliviousFCostQuant + d - mFirstHotQueueGranularCost, 0, kNumHotQueues - 1);
	//		if (!mHotQueues[queueI])
	//			++mNumActiveQueues;
	//		mHotQueues[queueI].push(adjVector, vcmpeq(adjFCostsQuant, firstObliviousFCostQuant + d) & adjPushMask);
	//	}
	//}
	//else// [[unlikely]]

	// Not actually unlikely. It's actually faster to always do this, unfortunately.
	{
		// ...just do it manually.


		//[[maybe_unused]] const int32_t firstObliviousFCostQuant = adjFCostsQuant.getElement<0>();
		//if ((adjPushMask & vcmpeq(adjFCostsQuant, adjFCostsQuant.getElement<0>())).all())
		//{
		//	++mNumOneQueuePushes;
		//	const int queueI = firstObliviousFCostQuant;
		//	if (!mHotQueues[queueI])
		//		++mNumActiveQueues;
		//	mHotQueues[queueI].plots.append_range(adjVector.toArray());
		//}
		//else
		//if (adjPushMask.all())
		//{
		//	for (int i = 0; i < kCoordVectorLength; ++i)
		//	{
		//		const int queueI = adjFCostsQuant.getElement(i);
		//		if (!mHotQueues[queueI])
		//			++mNumActiveQueues;
		//		mHotQueues[queueI].plots.push_back(adjVector.getElement(i));
		//	}
		//}
		//else
		{
			if constexpr (kEnableStats)
				++mNumManualPushes;
			auto& queues = mHotQueues;

			// This is slower as well...
			//const Vector maskedAdjFCostsQuant = vselect(adjPushMask, adjFCostsQuant, simd::Ops<simd::Avx512IntegerReg, uint32_t>::initAllBits());
			//const int maxQueueI = hmax(maskedAdjFCostsQuant);
			//if (vcmpeq(adjFCostsQuant, maxQueueI).bits == adjPushMask.bits)
			//{
			//	const int queueI = maxQueueI;
			//	const int n = std::popcount(adjPushMask.bits);
			//	HotQueue& queue = queues[queueI];
			//	if (!queue)
			//		++mNumActiveQueues;
			//	queue.plots.resize(queue.plots.size() + n);
			//	_mm512_mask_compressstoreu_epi32(queue.plots.data() + queue.plots.size() - n, adjPushMask.bits, adjVector.reg.bits);
			//}
			//else
			{
				unsigned int bits = adjPushMask.asBits();
				while (bits)
				{
					const unsigned int i = std::countr_zero(bits);
					//bits &= ~(1u << i);
					bits &= bits - 1;

					//const int queueI = std::clamp(adjFCostsQuant.getElement(i) - mFirstHotQueueGranularCost, 0, kNumHotQueues - 1);
					const int queueI = adjFCostsQuant.getElement(i);
					HotQueue& queue = queues[queueI];
					if (!queue)
						++mNumActiveQueues;
					queue.plots.push_back(adjVector.getElement(i));
				}
			}

			// Slower than above.
			//const Vector compressedPlots = hcompress(adjVector, adjPushMask);
			//const Vector compressedQueueIndices = hcompress(adjFCostsQuant, adjPushMask);
			//const int n = std::popcount(adjPushMask.bits);
			//for (const int i : range(n))
			//{
			//	const int queueI = compressedQueueIndices.getElement(i);
			//	HotQueue& queue = queues[queueI];
			//	if (!queue)
			//		++mNumActiveQueues;
			//	queue.plots.push_back(compressedPlots.getElement(i));
			//}
		}
	}
}

void SimdAStar::updateHeuristic(const IGraph& g)
{
	Vector minCost(INT32_MAX);

	for (const HotQueue& q : mHotQueues)
	{
		for (ptrdiff_t i = q.plots.size() - kCoordVectorLength; i > 0; i -= kCoordVectorLength)
		{
			const auto [plotIndices, iterationMask] = q.at(i);
			const Vector oldAdjGCosts(mGCosts, plotIndices, iterationMask); // gather op, give this lots of time!
			const Vector adjFCosts = oldAdjGCosts + g.getHeuristic(plotIndices);
			minCost = vmin(minCost, vselect(iterationMask, adjFCosts, minCost));
		}
	}

	if (hmin(minCost) >= INT32_MAX)
	{
		mFirstHotQueueGranularCost = 0;
		mHotQueues = {};
		mNumActiveQueues = 0;
		return;
	}

	mFirstHotQueueGranularCost = hmin(minCost) / kFCostGranularity;

	std::array<HotQueue, kNumHotQueues> myHotQueues(std::move(mHotQueues));

	initQueues();

	for (HotQueue& q : myHotQueues)
	{
		while (q)
		{
			const auto [plotIndices, iterationMask] = q.pop();
			const Vector oldAdjGCosts(mGCosts, plotIndices); // gather op, give this lots of time!
			const Vector adjFCosts = oldAdjGCosts + g.getHeuristic(plotIndices);
			push(plotIndices, adjFCosts, iterationMask);
		}
	}
}