#pragma once

#include "../Util.h"

namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	template<class T>
	class PaddedArray2D
	{
	public:
		using value_type = T;

		// This padding is needed so that found value calculation can obliviously load vectors at coordinate offsets at the edge of the map.
		static inline constexpr int kPadding = CITY_PLOTS_RADIUS;

		explicit PaddedArray2D(CivMapGeometry mapSizeInfo, T initValue, T outOfBoundsValue);

		CivMapGeometry getMapGeometry() const;

		void fill(T value);

		void set(ivec2 coord, T value);

		T get(ivec2 coord) const;

		void setWithoutWrapMirroring(ivec2 coord, T value);

		T fetchOrWithoutWrapMirroring(ivec2 coord, T value, std::memory_order memOrder)
		{
			return std::atomic_ref(raw(coord)).fetch_or(value, memOrder);
		}

		T fetchAndWithoutWrapMirroring(ivec2 coord, T value, std::memory_order memOrder)
		{
			return std::atomic_ref(raw(coord)).fetch_and(value, memOrder);
		}

		T fetchAddWithoutWrapMirroring(ivec2 coord, T value, std::memory_order memOrder)
		{
			return std::atomic_ref(raw(coord)).fetch_add(value, memOrder);
		}

		static int getPitch(CivMapGeometry mapSizeInfo);
		static ivec2 getMapOffset()
		{
			return kPadding;
		}

		std::span<const T> getMemorySpan() const;

		template<class U>
		void copyFrom(const DynamicArray2D<U>& b);

		void setAllPadding();

		void verify() const;

		friend bool operator==(const PaddedArray2D& a, const PaddedArray2D& b)
		{
			return a.mMapSizeInfo == b.mMapSizeInfo && a.mAllocDim == b.mAllocDim && std::ranges::equal(a.getMemorySpan(), b.getMemorySpan());
		}

		std::optional<ivec2> findFirstMismatch(const PaddedArray2D& b) const;

		

	private:
		CivMapGeometry mMapSizeInfo;
		ivec2 mAllocDim;
		T mOutOfBoundsValue{};
		std::unique_ptr<T[]> mMemory;

		
		std::span<T> getMemorySpan();

		T raw(ivec2 coord) const;

		T& raw(ivec2 coord);

		

		void copyLine(ivec2 dstStorageCoord, ivec2 srcStorageCoord, size_t length);
		void fillLine(ivec2 dstStorageCoord, T value, size_t length);
	};

#ifndef HECK_GiganticMapsOptimisationsLib_FoundValueSystem_PaddedArray2D_NO_EXTERN
	extern template class PaddedArray2D<int16_t>;
	extern template void PaddedArray2D<int16_t>::copyFrom(const DynamicArray2D<uint16_t>&);
	extern template class PaddedArray2D<uint16_t>;
	extern template class PaddedArray2D<uint8_t>;
	extern template class PaddedArray2D<int8_t>;
	extern template class PaddedArray2D<int32_t>;
#endif

	std::vector<i16vec2> gatherCityLocations(PlayerTypes filterPlayerI);
}