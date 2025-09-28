#pragma once

#include "SimdAStarGraph.h"
//#include "SimdHybridHeap.h"
#include "DAryHeap.h"
#include "vec.h"

namespace heck
{
	class SimdAStarPriorityQueueUsingDAryHeap
	{
	public:
		using Vector = SimdAStarConfig::Vector;

		void clear()
		{
			mHeap.nodes.clear();
		}

		bool empty() const
		{
			return mHeap.empty();
		}

		int32_t peek() const
		{
			return mHeap.frontKey();
		}

		size_t size() const
		{
			return mHeap.nodes.size();
		}

		void HECK_VECTORCALL push(Vector fcosts, vec2<Vector> adjCoords, Vector::Mask adjPushMask)
		{
			adjPushMask.forEachBit([&, this](size_t i) {
				mHeap.push(fcosts.getElement(i), (i16vec2)ivec2(adjCoords.x.getElement(i), adjCoords.y.getElement(i)));
				});
		}

		std::pair<vec2<Vector>, Vector::Mask> HECK_VECTORCALL pop()
		{
			const size_t iterationWidth = std::min<size_t>(mHeap.size(), Vector::kNumElements);
			const Vector::Mask iterationMask{ UnsignedTypeAtLeastBits<Vector::kNumElements>((UnsignedTypeAtLeastBits<Vector::kNumElements>(1) << iterationWidth) - 1) };
			alignas(Vector) Vector::Array plotX;
			alignas(Vector) Vector::Array plotY;
			// Dummy values to avoid adj out of bounds.
			plotX.fill(1);
			plotY.fill(1);

			for (const size_t i : range(iterationWidth))
			{
				const ivec2 coord = mHeap.pop().value;
				plotX[i] = coord.x;
				plotY[i] = coord.y;
			}

			return { { plotX, plotY }, iterationMask };
		}

		void update([[maybe_unused]] auto getVectorHeapValue, auto getScalarHeapValue)
		{
			for (auto& entry : mHeap.elements())
				entry.key = getScalarHeapValue(entry.value);
			mHeap.reheapify();
		}

	private:
		DAryHeap2<int32_t, i16vec2, 4> mHeap;
	};

	class SimdAStarPriorityQueueUsingStdHeap
	{
	public:
		using Vector = SimdAStarConfig::Vector;

		void clear()
		{
			mHeap.nodes.clear();
		}

		bool empty() const
		{
			return mHeap.empty();
		}

		int32_t peek() const
		{
			return mHeap.frontKey();
		}

		void HECK_VECTORCALL push(Vector fcosts, vec2<Vector> adjCoords, Vector::Mask adjPushMask)
		{
			adjPushMask.forEachBit([&, this](size_t i) {
				mHeap.push(fcosts.getElement(i), (i16vec2)ivec2(adjCoords.x.getElement(i), adjCoords.y.getElement(i)));
				});
		}

		std::pair<vec2<Vector>, Vector::Mask> HECK_VECTORCALL pop()
		{
			const size_t iterationWidth = std::min<size_t>(mHeap.size(), Vector::kNumElements);
			using MaskUInt = UnsignedTypeAtLeastBits<Vector::kNumElements>;
			const Vector::Mask iterationMask{ MaskUInt((MaskUInt(1) << iterationWidth) - 1) };
			alignas(Vector) Vector::Array plotX;
			alignas(Vector) Vector::Array plotY;
			// Dummy values to avoid adj out of bounds.
			plotX.fill(1);
			plotY.fill(1);

			for (const size_t i : range(iterationWidth))
			{
				const ivec2 coord = mHeap.pop().value;
				plotX[i] = coord.x;
				plotY[i] = coord.y;
			}

			return { { plotX, plotY }, iterationMask };
		}

		void update([[maybe_unused]] auto getVectorHeapValue, auto getScalarHeapValue)
		{
			for (auto& entry : mHeap.elements())
				entry.key = getScalarHeapValue(entry.value);
			mHeap.reheapify();
		}

	private:
		StdHeap<int32_t, i16vec2> mHeap;
	};

	

	// Need to fix SIMD code after SIMD overhaul.
#if 0
	template<size_t kVectorWidth, size_t kDegree, bool kStrictOrder>
	class SimdAStarPriorityQueueUsingSimdHeap
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
			mHeap.push(fcosts, packCoords(adjCoords), adjPushMask);
		}

		std::pair<vec2<Vector>, Vector::Mask> HECK_VECTORCALL pop()
		{
			const auto result = mHeap.pop();
			auto coords = unpackCoords(result.values);
			// Avoid adj out of bounds for unused elements.
			coords.x = vselect(result.mask, coords.x, 1);
			coords.y = vselect(result.mask, coords.y, 1);
			return { coords, result.mask };
		}

		void update(auto getVectorHeapValue, [[maybe_unused]] auto getScalarHeapValue)
		{
			mHeap.update([getVectorHeapValue](simd::Vector<uint32_t, kVectorWidth> packedCoords) {
				return getVectorHeapValue(unpackCoords(packedCoords));
				});
		}

	private:
		SimdHybridHeap<int32_t, uint32_t, kVectorWidth, kDegree, kStrictOrder> mHeap;
	};
#endif
}