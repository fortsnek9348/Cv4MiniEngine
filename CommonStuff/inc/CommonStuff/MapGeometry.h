#pragma once

#include "SimdVector.h"
#include "aabb.h"
#include "vec.h"

namespace heck
{
	inline int calcCoordDistance(int iFrom, int iTo, int iRange, bool bWrap)
	{
		if (iTo < iFrom)
			std::swap(iFrom, iTo);
		// iTo >= iFrom

		if (!bWrap)
			return iTo - iFrom;
		else
			return std::min(iTo - iFrom, iFrom - (iTo - iRange));
	}

	// Inclusive max input and output!
	inline int calcMinCoordDistanceFromInclusiveIntervals(int iFromMin, int iFromIMax, int iToMin, int iToIMax, int iRange, bool bWrap)
	{
		if (iToMin < iFromMin)
		{
			std::swap(iFromMin, iToMin);
			std::swap(iFromIMax, iToIMax);
		}
		// iToMin >= iFromMin

		if (!bWrap)
			return iToMin - std::min(iToMin, iFromIMax);
		else
			return std::min(iToMin - std::min(iToMin, iFromIMax), iFromMin - (iToIMax - iRange));
	}

	template<size_t k>
	inline simd::Vector<int16_t, k> HECK_VECTORCALL calcCoordDistance(simd::Vector<int16_t, k> iFrom, simd::Vector<int16_t, k> iTo, int iRange, bool bWrap)
	{
		const simd::Vector<int16_t, k> d = vabs(iFrom - iTo);

		if (bWrap)
			return vselect(vcmpgt(d, static_cast<int16_t>(iRange >> simd::imm<1>)), static_cast<int16_t>(iRange) - d, d);
		else
			return d;
	}

	struct MapGeometry
	{
		ivec2 dim{};
		bool wrapX = false;
		bool wrapY = false;

		explicit MapGeometry(ivec2 dim, bool wrapX, bool wrapY) : dim(dim), wrapX(wrapX), wrapY(wrapY)
		{
		}

		bool isValidCoord(ivec2 coord) const
		{
			//return (wrapX ? -dim.x <= coord.x && coord.x < dim.x * 2 : 0 <= coord.x && coord.x < dim.x)
			//	&& (wrapY ? -dim.y <= coord.y && coord.y < dim.y * 2 : 0 <= coord.y && coord.y < dim.y);
			return (wrapX ? true : 0 <= coord.x && coord.x < dim.x)
				&& (wrapY ? true : 0 <= coord.y && coord.y < dim.y);
		}

		[[nodiscard]] ivec2 wrapCoord(ivec2 coord) const
		{
			if (wrapX)
			{
				if (coord.x < 0)
					coord.x += dim.x;
				if (coord.x >= dim.x)
					coord.x -= dim.x;
			}
			if (wrapY)
			{
				if (coord.y < 0)
					coord.y += dim.y;
				if (coord.y >= dim.y)
					coord.y -= dim.y;
			}
			return coord;
		}

		[[nodiscard]] std::optional<ivec2> tryCanonicalise(ivec2 coord) const
		{
			if (unsigned(coord.x) >= unsigned(dim.x))
			{
				if (wrapX)
				{
					if (coord.x < 0)
						coord.x += dim.x;
					if (coord.x >= dim.x)
						coord.x -= dim.x;
				}
				else
					return std::nullopt;
			}
			if (unsigned(coord.y) >= unsigned(dim.y))
			{
				if (wrapY)
				{
					if (coord.y < 0)
						coord.y += dim.y;
					if (coord.y >= dim.y)
						coord.y -= dim.y;
				}
				else
					return std::nullopt;
			}
			return coord;
		}

		template<size_t k>
		[[nodiscard]] vec2<simd::Vector<int16_t, k>> HECK_VECTORCALL wrapCoords(vec2<simd::Vector<int16_t, k>> coord) const
		{
			if (wrapX)
			{
				coord.x = vmaskedadd(vcmplt(coord.x, 0), coord.x, static_cast<int16_t>(dim.x));
				coord.x = vmaskedsub(vcmpge(coord.x, static_cast<int16_t>(dim.x)), coord.x, static_cast<int16_t>(dim.x));
			}
			if (wrapY)
			{
				coord.y = vmaskedadd(vcmplt(coord.y, 0), coord.y, static_cast<int16_t>(dim.y));
				coord.y = vmaskedsub(vcmpge(coord.y, static_cast<int16_t>(dim.y)), coord.y, static_cast<int16_t>(dim.y));
			}
			return coord;
		}

		int minPlotDistanceBetweenRects(iaabb2 a, iaabb2 b) const
		{
			assert(!a.empty() && !b.empty());
			const int iDX = calcMinCoordDistanceFromInclusiveIntervals(a.min.x, a.max.x - 1, b.min.x, b.max.x - 1, dim.x, wrapX);
			const int iDY = calcMinCoordDistanceFromInclusiveIntervals(a.min.y, a.max.y - 1, b.min.y, b.max.y - 1, dim.y, wrapY);
			return std::max(iDX, iDY) + std::min(iDX, iDY) / 2;
		}

		int plotDistance(ivec2 a, ivec2 b) const
		{
			const int iDX = calcCoordDistance(a.x, b.x, dim.x, wrapX);
			const int iDY = calcCoordDistance(a.y, b.y, dim.y, wrapY);
			return std::max(iDX, iDY) + std::min(iDX, iDY) / 2;
		}

		template<size_t k>
		simd::Vector<int16_t, k> HECK_VECTORCALL plotDistance(vec2<simd::Vector<int16_t, k>> a, vec2<simd::Vector<int16_t, k>> b) const
		{
			const simd::Vector<int16_t, k> iDX = calcCoordDistance(a.x, b.x, dim.x, wrapX);
			const simd::Vector<int16_t, k> iDY = calcCoordDistance(a.y, b.y, dim.y, wrapY);
			return vmax(iDX, iDY) + (vmin(iDX, iDY) >> simd::imm<1>);
		}

		friend bool operator==(MapGeometry, MapGeometry) = default;
	};

	inline uint32_t packCoord(i16vec2 coord)
	{
		return uint32_t(uint16_t(coord.x)) | (uint32_t(uint16_t(coord.y)) << 16);
	}

	inline i16vec2 unpackCoord(uint32_t coord)
	{
		return { int16_t(coord), int16_t(coord >> 16) };
	}

	template<size_t kVectorWidth>
	inline simd::Vector<uint32_t, kVectorWidth> HECK_VECTORCALL packCoords(vec2<simd::Vector<int16_t, kVectorWidth>> coords)
	{
		return simd::Vector<uint32_t, kVectorWidth>(simd::Vector<uint16_t, kVectorWidth>(coords.x))
			| (simd::Vector<uint32_t, kVectorWidth>(coords.y) << std::integral_constant<size_t, 16>());
	}

	template<size_t kVectorWidth>
	inline vec2<simd::Vector<int16_t, kVectorWidth>> HECK_VECTORCALL unpackCoords(simd::Vector<uint32_t, kVectorWidth> packedCoords)
	{
		return {
			simd::Vector<int16_t, kVectorWidth>(packedCoords & 0xFFFF),
			simd::Vector<int16_t, kVectorWidth>(packedCoords >> std::integral_constant<size_t, 16>()),
		};
	}
}