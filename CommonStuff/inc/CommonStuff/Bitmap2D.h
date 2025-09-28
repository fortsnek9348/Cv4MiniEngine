#pragma once

#include "range.h"
#include "vec.h"
#include "div.h"

#include <bitset>
#include <vector>

namespace heck
{
	template<size_t kN>
	struct Bitmap2D : std::bitset<kN * kN>
	{
		bool operator[](ivec2 coord) const
		{
			return std::bitset<kN * kN>::operator[](coord.x + coord.y * kN);
		}

		auto&& operator[](ivec2 coord)
		{
			return std::bitset<kN * kN>::operator[](coord.x + coord.y * kN);
		}
	};

	struct DynamicBitmap2D
	{
		static constexpr unsigned int kBitsPerWord = 64;
		using Word = uint64_t;

		ivec2 dim{};
		std::vector<Word> words;

		DynamicBitmap2D() = default;

		explicit DynamicBitmap2D(ivec2 dim) : dim(dim), words(heck::cdiv(dim.x * dim.y, kBitsPerWord))
		{
		}

		void clearBits()
		{
			std::ranges::fill(words, Word());
		}

		void setBits()
		{
			std::ranges::fill(words, Word(-1));
		}

		int findXSpanLength(ivec2 coord) const
		{
			const int n = fastFindXSpanLength(coord);
			assert(slowFindXSpanLength(coord) == n);
			return n;
		}

		int findYSpanLength(ivec2 coord) const
		{
			for (const int y : range(coord.y, dim.y))
				if (!(*this)[ivec2(coord.x, y)])
					return y - coord.y;
			return dim.y - coord.y;
		}

		void clearXSpanAtYExclMax(int y, int minX, int exclMaxX)
		{
			assert(0 < minX && minX <= exclMaxX && exclMaxX <= dim.x);
			if (exclMaxX <= minX)
				return;
			const size_t startI = minX + y * dim.x;
			const size_t inclLastI = exclMaxX - 1 + y * dim.x;
			const size_t startWordI = startI / kBitsPerWord;
			const size_t startBitI = startI % kBitsPerWord;
			const size_t inclLastWordI = inclLastI / kBitsPerWord;
			const size_t inclLastBitI = inclLastI % kBitsPerWord;
			
			const Word firstMask = ~((Word(1) << startBitI) - 1);
			const Word lastMask = Word(~Word(0)) >> (kBitsPerWord - (inclLastBitI + 1));

			if (inclLastWordI <= startWordI)
				words[startWordI] &= firstMask & lastMask;
			else
			{
				words[startWordI] &= firstMask;
				std::fill(words.begin() + startWordI + 1, words.begin() + inclLastI, Word());
				words[inclLastWordI] &= lastMask;
			}
		}

		bool operator[](ivec2 coord) const
		{
			const size_t i = coord.x + coord.y * dim.x;
			return bool((words[i / kBitsPerWord] >> (i % kBitsPerWord)) & 1);
		}

		decltype(auto) operator[](ivec2 coord)
		{
			struct Proxy
			{
				const Word mask = 0;
				Word* const word = nullptr;

				Proxy& operator=(bool x)
				{
					if (x)
						*word |= mask;
					else
						*word &= ~mask;
					return *this;
				}

				operator bool() const
				{
					return bool(*word & mask);
				}
			};

			const size_t i = coord.x + coord.y * dim.x;
			return Proxy{
				Word(1) << (i % kBitsPerWord),
				&words[i / kBitsPerWord],
			};
		}

		void setThreadSafe(ivec2 coord, std::memory_order memorder)
		{
			const size_t i = coord.x + coord.y * dim.x;
			const size_t wordI = i / kBitsPerWord;
			const size_t bitI = i % kBitsPerWord;
			std::atomic_ref(words[wordI]).fetch_or(Word(1) << bitI, memorder);
		}

		void setThreadSafe(ivec2 coord, bool value, std::memory_order memorder)
		{
			const size_t i = coord.x + coord.y * dim.x;
			const size_t wordI = i / kBitsPerWord;
			const size_t bitI = i % kBitsPerWord;
			if (value)
				std::atomic_ref(words[wordI]).fetch_or(Word(value) << bitI, memorder);
			else
				std::atomic_ref(words[wordI]).fetch_and(~(Word(1) << bitI), memorder);
		}

		[[nodiscard]] bool getThreadSafe(ivec2 coord, std::memory_order memorder) const
		{
			const size_t i = coord.x + coord.y * dim.x;
			const size_t wordI = i / kBitsPerWord;
			const size_t bitI = i % kBitsPerWord;
			return bool((std::atomic_ref(words[wordI]).load(memorder) >> bitI) & 1);
		}

	private:
		int fastFindXSpanLength(ivec2 coord) const
		{
			// We're using STL special optimisations.
			const size_t maxLength = dim.x - coord.x;
			const size_t startI = coord.x + coord.y * dim.x;
			const size_t startWordI = startI / kBitsPerWord;
			const size_t startBitI = startI % kBitsPerWord;

			if (maxLength <= 0)
				return 0;

			const Word firstWord = words[startWordI];
			const Word firstMask = (Word(1) << startBitI) - 1;
			if (~(firstWord | firstMask))
				return (int)std::min<size_t>(startWordI * kBitsPerWord + std::countr_one(firstWord | firstMask) - startI, maxLength);

			const size_t endWordI = (startI + maxLength) / kBitsPerWord + 1; // might be one extra
			for (size_t wordI = startWordI + 1; wordI < endWordI; ++wordI)
				if (const Word word = words[wordI]; ~word)
					return (int)std::min<size_t>(wordI * kBitsPerWord + std::countr_one(word) - startI, maxLength);

			return (int)maxLength;
		}

		int slowFindXSpanLength(ivec2 coord) const
		{
			for (const int x : range(coord.x, dim.x))
				if (!(*this)[ivec2(x, coord.y)])
					return x - coord.x;
			return dim.x - coord.x;
		}
	};
}