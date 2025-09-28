#pragma once

#include "adj.h"
#include "Simd.h"
#include "MapGeometry.h"

namespace heck
{
	namespace SimdAStarConfig
	{
		inline constexpr int kCoordVectorLength = 16;
		using Vector = simd::Vector<int32_t, kCoordVectorLength>;
		using I16Vector = simd::Vector<int16_t, kCoordVectorLength>;
		using StateVector = simd::Vector<uint32_t, kCoordVectorLength>;

		struct Step
		{
			Vector cost;
			StateVector state;
		};

		inline constexpr vec2<Vector> kAdjCoordsVector{
			Vector::Array{
				kAdj[0].x,
				kAdj[1].x,
				kAdj[2].x,
				kAdj[3].x,
				kAdj[4].x,
				kAdj[5].x,
				kAdj[6].x,
				kAdj[7].x,
			},
			Vector::Array{
				kAdj[0].y,
				kAdj[1].y,
				kAdj[2].y,
				kAdj[3].y,
				kAdj[4].y,
				kAdj[5].y,
				kAdj[6].y,
				kAdj[7].y,
			},
		};
	}

	class ISimdAStarGraph
	{
	public:
		static constexpr int kCoordVectorLength = SimdAStarConfig::kCoordVectorLength;
		using Vector = SimdAStarConfig::Vector;
		using StateVector = SimdAStarConfig::StateVector;

		using Step = SimdAStarConfig::Step;

		//virtual Step HECK_VECTORCALL getStepCost(StateVector fromState, Vector fromPlotIndices, Vector toPlotIndices, Vector adjI) const = 0;
		virtual void HECK_VECTORCALL setStepFrom(StateVector fromState, Vector fromPlotIndices) = 0;
		virtual Step HECK_VECTORCALL getStepCostTo(Vector toPlotIndices, Vector adjI) const = 0;
		virtual Vector HECK_VECTORCALL getHeuristic(Vector fromPlotIndices) const = 0;
		virtual int32_t getSingleHeuristic(int plotI) const = 0;
		virtual ~ISimdAStarGraph() = default;

		using AdjOffsetArray = std::array<int32_t, std::size(kAdj)>;

		static AdjOffsetArray calcAdjOffsets(int pitch)
		{
			AdjOffsetArray a{};
			for (size_t i = 0; i < a.size(); ++i)
				a[i] = kAdj[i].x + kAdj[i].y * pitch;
			return a;
		}
	};

	//inline uint32_t packCoord(i16vec2 coord)
	//{
	//	return uint32_t(uint16_t(coord.x)) | (uint32_t(uint16_t(coord.y)) << 16);
	//}
	//
	//template<size_t kVectorWidth>
	//inline simd::Vector<uint32_t, kVectorWidth> packCoords(vec2<simd::Vector<int32_t, kVectorWidth>> coords)
	//{
	//	return simd::Vector<uint32_t, kVectorWidth>(coords.x) | (simd::Vector<uint32_t, kVectorWidth>(coords.y) << std::integral_constant<size_t, 16>());
	//}
	//
	//template<size_t kVectorWidth>
	//inline vec2<simd::Vector<int32_t, kVectorWidth>> unpackCoords(simd::Vector<uint32_t, kVectorWidth> packedCoords)
	//{
	//	return {
	//		simd::Vector<int32_t, kVectorWidth>(packedCoords & 0xFFFF),
	//		simd::Vector<int32_t, kVectorWidth>(packedCoords >> std::integral_constant<size_t, 16>()),
	//	};
	//}
}