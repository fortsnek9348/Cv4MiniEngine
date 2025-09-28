#include "inc/CommonStuff/SimdAStarPriorityQueue.h"

using namespace heck;

void HECK_VECTORCALL SimdAStarPriorityQueueUsingDAryHeap::push(Vector fcosts, vec2<Vector> adjCoords, Vector::Mask adjPushMask)
{
	unsigned int bits = adjPushMask.asBits();
	while (bits)
	{
		const unsigned int i = std::countr_zero(bits);
		bits &= bits - 1;
		mHeap.push(fcosts.getElement(i), (i16vec2)ivec2(adjCoords.x.getElement(i), adjCoords.y.getElement(i)));
	}
}

std::pair<vec2<SimdAStarPriorityQueueUsingDAryHeap::Vector>, SimdAStarPriorityQueueUsingDAryHeap::Vector::Mask> HECK_VECTORCALL SimdAStarPriorityQueueUsingDAryHeap::pop()
{
	const size_t iterationWidth = std::min<size_t>(mHeap.size(), Vector::kNumElements);
	const Vector::Mask iterationMask{ std::bitset<Vector::kNumElements>((1 << iterationWidth) - 1) };
	alignas(Vector) Vector::Array plotX{};
	alignas(Vector) Vector::Array plotY{};

	for (const size_t i : range(iterationWidth))
	{
		const ivec2 coord = mHeap.pop().value;
		plotX[i] = coord.x;
		plotY[i] = coord.y;
	}

	return { { plotX, plotY }, iterationMask };
}
