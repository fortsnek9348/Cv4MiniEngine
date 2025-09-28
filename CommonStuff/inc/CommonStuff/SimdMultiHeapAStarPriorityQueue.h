#pragma once

#include "SimdMultiHeap.h"
#include "SimdAStarPriorityQueue.h"
#include "MapGeometry.h"

namespace heck
{
	class SimdMultiHeapAStarPriorityQueue
	{
	public:
		using Vector = SimdAStarConfig::Vector;

		void clear()
		{
			mHeap.clear();
		}

		bool empty() const
		{
			return mHeap.empty();
		}

		int32_t peek() const
		{
			return mHeap.peek();
		}

		void HECK_VECTORCALL push(Vector fcosts, vec2<Vector> adjCoords, Vector::Mask adjPushMask)
		{
			adjPushMask.forEachBit([&, this, packedCoords = packCoords(vec2{ SimdAStarConfig::I16Vector(adjCoords.x), SimdAStarConfig::I16Vector(adjCoords.y) })](size_t i) {
				mHeap.push(fcosts.getElement(i), packedCoords.getElement(i));
				});
		}

		std::pair<vec2<Vector>, Vector::Mask> HECK_VECTORCALL pop()
		{
			const auto popResult = mHeap.pop();
			return {
				unpackCoords(popResult.values),
				popResult.mask,
			};
		}

		void update([[maybe_unused]] auto getVectorHeapValue, auto getScalarHeapValue)
		{
			for (auto& entry : mHeap.elements())
			{
				const int x = int16_t(entry.value & 0xFFFF);
				const int y = int16_t(entry.value >> 16);
				entry.key = getScalarHeapValue({ x, y });
			}
			mHeap.reheapify();
		}

	private:
		SimdMultiHeap<int32_t, uint32_t, Vector::kNumElements, 4> mHeap;
	};
}