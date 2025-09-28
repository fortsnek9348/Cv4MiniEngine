#pragma once

#include <bit>

namespace heck
{
	// Rotate XOR Prime https://www.gkbrk.com/wiki/avalanche-diagram/
	constexpr uint64_t rxprime64(uint64_t x)
	{
		x *= 7919;
		x ^= std::rotl(x, 7);
		x *= 7723;
		x ^= std::rotl(x, 11);
		x *= 7561;
		x ^= std::rotl(x, 13);
		x *= 7411;
		x ^= std::rotl(x, 17);
		return x;
	}

	// https://www.gkbrk.com/wiki/avalanche-diagram/, https://nullprogram.com/blog/2018/07/31/
	//constexpr uint32_t prospector32(uint32_t x)
	//{
	//	x ^= x >> 15;
	//	x *= 0x2c1b3c6d;
	//	x ^= x >> 12;
	//	x *= 0x297a2d39;
	//	x ^= x >> 15;
	//	return x;
	//}

	// Should be better than prospector32
	// Bias value of 0.263985 according to algorithm used by hash-prospector.
	//constexpr uint32_t murmurhash32_mix32(uint32_t x)
	//{
	//	x ^= x >> 16;
	//	x *= 0x85ebca6bU;
	//	x ^= x >> 13;
	//	x *= 0xc2b2ae35U;
	//	x ^= x >> 16;
	//	return x;
	//}

	// https://github.com/skeeto/hash-prospector/issues/19#issuecomment-1120105785
	// This should be better than murmurhash32_mix32.
	// Bias value of 0.107043 according to algorithm used by hash-prospector.
	constexpr uint32_t theIronBornHash32(uint32_t x)
	{
		x ^= x >> 16;
		x *= 0x21f0aaad;
		x ^= x >> 15;
		x *= 0x735a2d97;
		x ^= x >> 15;
		return x;
	}

}