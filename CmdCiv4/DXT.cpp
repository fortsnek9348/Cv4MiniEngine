#include "DXT.h"
#include "Common.h"

#define STB_DXT_IMPLEMENTATION
#include <stb/stb_dxt.h>
#include <s3tc.h>

#include <CommonStuff/range.h>

using heck::range;

constexpr int kDXT1BlockSize = 8;

std::vector<uint8_t> cvengine::encodeDXT1(hecktui::ivec2 dim, std::span<const std::array<uint8_t, 3>> src)
{
	assert(dim.x % 4 == 0 && dim.y % 4 == 0);

	std::vector<uint8_t> out(dim.x * dim.y / 2);

	const int blocksPerBlockRow = dim.x / 4;

	for (const int blkY : range(dim.y / 4))
	{
		for (const int blkX : range(blocksPerBlockRow))
		{
			alignas(std::hardware_constructive_interference_size) uint8_t srcBlock[16 * 4];
			for (const int dy : range(4))
				for (const int dx : range(4))
				{
					const auto [r, g, b] = src[blkX * 4 + dx + (blkY * 4 + dy) * dim.x];
					srcBlock[(dx + dy * 4) * 4 + 0] = r;
					srcBlock[(dx + dy * 4) * 4 + 1] = g;
					srcBlock[(dx + dy * 4) * 4 + 2] = b;
					srcBlock[(dx + dy * 4) * 4 + 3] = 255;
				}

			stb_compress_dxt_block(std::span(out).subspan((blkX + blkY * blocksPerBlockRow) * kDXT1BlockSize, kDXT1BlockSize).data(), srcBlock, 0, STB_DXT_HIGHQUAL);
		}
	}

	return out;
}

std::vector<std::array<uint8_t, 3>> cvengine::decodeDXT1(hecktui::ivec2 dim, std::span<const uint8_t> src)
{
	assert(dim.x % 4 == 0 && dim.y % 4 == 0);

	std::vector<std::array<uint8_t, 3>> out(dim.x * dim.y);

	const int blocksPerBlockRow = dim.x / 4;

	for (const int blkY : range(dim.y / 4))
	{
		for (const int blkX : range(blocksPerBlockRow))
		{
			alignas(std::hardware_constructive_interference_size) unsigned long blkPixels[16];
			DecompressBlockDXT1(0, 0, 4, std::span(src).subspan((blkX + blkY * blocksPerBlockRow) * kDXT1BlockSize, kDXT1BlockSize).data(), blkPixels);

			for (const int dy : range(4))
				for (const int dx : range(4))
				{
					const unsigned long rgba = blkPixels[dx + dy * 4];
					out[blkX * 4 + dx + (blkY * 4 + dy) * dim.x] = {
						uint8_t(rgba >> 24),
						uint8_t(rgba >> 16),
						uint8_t(rgba >> 8),
					};
				}
		}
	}

	return out;
}
