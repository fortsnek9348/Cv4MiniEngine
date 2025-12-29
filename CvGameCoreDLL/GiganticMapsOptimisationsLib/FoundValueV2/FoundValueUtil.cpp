#include "../../CommonConfig.h"

#ifdef ENABLE_GAMECOREDLL_ENHANCEMENTS

#define HECK_GiganticMapsOptimisationsLib_FoundValueSystem_PaddedArray2D_NO_EXTERN

#include "FoundValueUtil.h"

#include "../../CvPlayerAI.h"

using namespace GiganticMapsOptimisationsLib::FoundValueSystem;
using namespace GiganticMapsOptimisationsLib;

template<class T>
PaddedArray2D<T>::PaddedArray2D(CivMapGeometry mapSizeInfo, T initValue, T outOfBoundsValue)
	: mMapSizeInfo(mapSizeInfo)
	, mAllocDim(ivec2(mapSizeInfo.dim) + kPadding * 2)
	, mOutOfBoundsValue(outOfBoundsValue)
	, mMemory(std::make_unique_for_overwrite<T[]>(mAllocDim.x * mAllocDim.y))
{
	// Map shouldn't be so small that wrapping creates more than 9 instances of a plot in mMemory.
	if (mapSizeInfo.dim.x < kPadding || mapSizeInfo.dim.y < kPadding)
		std::abort();
	fill(initValue);
}

template<class T>
CivMapGeometry PaddedArray2D<T>::getMapGeometry() const
{
	return mMapSizeInfo;
}

template<class T>
void PaddedArray2D<T>::fill(T value)
{
	std::ranges::fill(getMemorySpan(), value);
	setAllPadding();
}

template<class T>
void PaddedArray2D<T>::set(ivec2 coord, T value)
{
	setWithoutWrapMirroring(coord, value);

	// Replicate values into wrap padding.

	const bool needXWrap = static_cast<unsigned int>(coord.x - kPadding) >= static_cast<unsigned int>(mMapSizeInfo.dim.x - kPadding * 2) && mMapSizeInfo.wrapX;
	const bool needYWrap = static_cast<unsigned int>(coord.y - kPadding) >= static_cast<unsigned int>(mMapSizeInfo.dim.y - kPadding * 2) && mMapSizeInfo.wrapY;

	if (needXWrap)
	{
		if (coord.x < kPadding)
			raw(coord + ivec2(mMapSizeInfo.dim.x, 0)) = value;
		if (coord.x >= mMapSizeInfo.dim.x - kPadding)
			raw(coord - ivec2(mMapSizeInfo.dim.x, 0)) = value;
	}

	if (needYWrap)
	{
		if (coord.y < kPadding)
			raw(coord + ivec2(0, mMapSizeInfo.dim.y)) = value;
		if (coord.y >= mMapSizeInfo.dim.y - kPadding)
			raw(coord - ivec2(0, mMapSizeInfo.dim.y)) = value;

		// Don't forget the corners!
		if (needXWrap)
		{
			if (coord.x < kPadding && coord.y < kPadding)
				raw(coord + ivec2(mMapSizeInfo.dim)) = value;
			if (coord.x >= mMapSizeInfo.dim.x - kPadding && coord.y >= mMapSizeInfo.dim.y - kPadding)
				raw(coord - ivec2(mMapSizeInfo.dim)) = value;
		}
	}
}

template<class T>
T PaddedArray2D<T>::get(ivec2 coord) const
{
	assert(iaabb2{ .max = mMapSizeInfo.dim }.contains(coord));
	return raw(coord);
}

template<class T>
void PaddedArray2D<T>::setWithoutWrapMirroring(ivec2 coord, T value)
{
	assert(iaabb2{ .max = mMapSizeInfo.dim }.contains(coord));
	raw(coord) = value;
}

template<class T>
int PaddedArray2D<T>::getPitch(CivMapGeometry mapSizeInfo)
{
	return mapSizeInfo.dim.x + kPadding * 2;
}

template<class T>
template<class U>
void PaddedArray2D<T>::copyFrom(const DynamicArray2D<U>& b)
{
	assert(b.dim == ivec2(mMapSizeInfo.dim));

	for (const int y : range(mMapSizeInfo.dim.y))
	{
		const ivec2 dstCoord = ivec2(0, y) + kPadding;
		std::ranges::copy_n(b.cells.begin() + y * b.dim.x, b.dim.y,
			std::span(mMemory.get(), mAllocDim.x * mAllocDim.y).subspan(dstCoord.x + dstCoord.y * mAllocDim.x).begin());
	}

	setAllPadding();
}

template<class T>
void PaddedArray2D<T>::verify() const
{
	const auto verifyAt = [this](ivec2 storageCoord) {
		const ivec2 mapCoord = storageCoord - kPadding;
		if (raw(mapCoord) != (mMapSizeInfo.isValidCoord(mapCoord) ? get(mMapSizeInfo.wrapCoord(mapCoord)) : mOutOfBoundsValue))
			std::abort();
		};

	// Verify padding rows.
	for (const int storageY : range(kPadding))
		for (const int storageX : range(kPadding))
			verifyAt({ storageX, storageY });

	for (const int storageY : range(mAllocDim.y - kPadding, mAllocDim.y))
		for (const int storageX : range(kPadding))
			verifyAt({ storageX, storageY });

	// Verify columns.
	for (const int storageY : range(mAllocDim.y))
	{
		for (const int storageX : range(kPadding))
			verifyAt({ storageX, storageY });
		for (const int storageX : range(mAllocDim.x - kPadding, mAllocDim.x))
			verifyAt({ storageX, storageY });
	}
}

template<class T>
std::optional<ivec2> PaddedArray2D<T>::findFirstMismatch(const PaddedArray2D& b) const
{
	if (mMapSizeInfo != b.mMapSizeInfo)
		std::abort();
	for (const int y : range(mMapSizeInfo.dim.y))
		for (const int x : range(mMapSizeInfo.dim.x))
			if (get({ x, y }) != b.get({ x, y }))
				return ivec2(x, y);
	return std::nullopt;
}

template<class T>
std::span<const T> PaddedArray2D<T>::getMemorySpan() const
{
	return std::span(mMemory.get(), mAllocDim.x * mAllocDim.y);
}

template<class T>
std::span<T> PaddedArray2D<T>::getMemorySpan()
{
	return std::span(mMemory.get(), mAllocDim.x * mAllocDim.y);
}

template<class T>
T PaddedArray2D<T>::raw(ivec2 coord) const
{
	coord += kPadding;
	assert(iaabb2{ .max = mAllocDim }.contains(coord));
	return mMemory[coord.x + coord.y * mAllocDim.x];
}

template<class T>
T& PaddedArray2D<T>::raw(ivec2 coord)
{
	coord += kPadding;
	assert(iaabb2{ .max = mAllocDim }.contains(coord));
	return mMemory[coord.x + coord.y * mAllocDim.x];
}

template<class T>
void PaddedArray2D<T>::setAllPadding()
{
	//const size_t pitch = mAllocDim.x;
	if (mMapSizeInfo.wrapX)
	{
		for (const int y : range(mMapSizeInfo.dim.y))
		{
			const int storageY = kPadding + y;
			// Set left padding to right columns.
			copyLine(ivec2(0, storageY), ivec2(kPadding + mMapSizeInfo.dim.x - kPadding, storageY), kPadding);
			// Set right padding to left columns.
			copyLine(ivec2(kPadding + mMapSizeInfo.dim.x, storageY), ivec2(kPadding, storageY), kPadding);
		}
	}
	else
	{
		for (const int y : range(mMapSizeInfo.dim.y))
		{
			const int storageY = kPadding + y;
			fillLine(ivec2(0, storageY), mOutOfBoundsValue, kPadding);
			fillLine(ivec2(kPadding + mMapSizeInfo.dim.x, storageY), mOutOfBoundsValue, kPadding);
		}
	}

	if (mMapSizeInfo.wrapY)
	{
		// Set top padding to bottom rows.
		for (const int i : range(kPadding))
			copyLine(ivec2(0, i), ivec2(0, mMapSizeInfo.dim.y + i), mAllocDim.x);
		// Set bottom padding to top rows.
		for (const int i : range(kPadding))
			copyLine(ivec2(0, kPadding + mMapSizeInfo.dim.y + i), ivec2(0, kPadding + i), mAllocDim.x);

		// Note that this handles the corners too whether wrapX or !wrapX.
	}
	else
	{
		for (const int i : range(kPadding))
			fillLine(ivec2(kPadding, i), mOutOfBoundsValue, mAllocDim.x - kPadding * 2);
		for (const int i : range(kPadding))
			fillLine(ivec2(kPadding, kPadding + mMapSizeInfo.dim.y + i), mOutOfBoundsValue, mAllocDim.x - kPadding * 2);
	}

	// Corner padding.
	if (!mMapSizeInfo.wrapX || !mMapSizeInfo.wrapY)
	{
		for (const int i : range(kPadding))
		{
			fillLine(ivec2(0, i), mOutOfBoundsValue, kPadding);
			fillLine(ivec2(mAllocDim.x - kPadding, i), mOutOfBoundsValue, kPadding);
		}
		for (const int i : range(kPadding))
		{
			fillLine(ivec2(0, kPadding + mMapSizeInfo.dim.y + i), mOutOfBoundsValue, kPadding);
			fillLine(ivec2(mAllocDim.x - kPadding, kPadding + mMapSizeInfo.dim.y + i), mOutOfBoundsValue, kPadding);
		}
	}
}

template<class T>
void PaddedArray2D<T>::copyLine(ivec2 dstStorageCoord, ivec2 srcStorageCoord, size_t length)
{
	const std::span<T> storage = std::span(mMemory.get(), mAllocDim.x * mAllocDim.y);
	const size_t pitch = mAllocDim.x;
	std::ranges::copy_n(storage.subspan(srcStorageCoord.x + srcStorageCoord.y * pitch).begin(), length, storage.subspan(dstStorageCoord.x + dstStorageCoord.y * pitch).begin());
}

template<class T>
void PaddedArray2D<T>::fillLine(ivec2 dstStorageCoord, T value, size_t length)
{
	const std::span<T> storage = std::span(mMemory.get(), mAllocDim.x * mAllocDim.y);
	const size_t pitch = mAllocDim.x;
	std::ranges::fill_n(storage.subspan(dstStorageCoord.x + dstStorageCoord.y * pitch).begin(), length, value);
}


namespace GiganticMapsOptimisationsLib::FoundValueSystem
{
	template class PaddedArray2D<int16_t>;
	template void PaddedArray2D<int16_t>::copyFrom(const DynamicArray2D<uint16_t>&);

	template class PaddedArray2D<uint16_t>;
	template class PaddedArray2D<int8_t>;
	template class PaddedArray2D<uint8_t>;
	template class PaddedArray2D<int32_t>;
}

std::vector<i16vec2> FoundValueSystem::gatherCityLocations(PlayerTypes filterPlayerI)
{
	std::vector<i16vec2> coords;
	coords.reserve(100);
	for (const PlayerTypes playerI : range<PlayerTypes>(MAX_PLAYERS))
	{
		if (filterPlayerI == NO_PLAYER || playerI == filterPlayerI)
		{
			const CvPlayerAI& player = GET_PLAYER(playerI);
			int itIndex{};
			for (const CvCity* city = player.firstCity(&itIndex); city; city = player.nextCity(&itIndex))
				coords.push_back(getPlotCoord(*city->plot()));
		}
	}
	return coords;
}

#endif