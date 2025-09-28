#pragma once

#include "IntegerTypeUtil.h"
#include "div.h"

#include <cassert>
#include <array>
#include <bit>
#include <limits>
#include <algorithm>

namespace heck
{
	template<size_t k>
	class Bitset
	{
	public:
		using Word = UnsignedTypeAtLeastBitsOrMax<k>;
		static constexpr size_t kBitsPerWord = std::numeric_limits<Word>::digits;
		static constexpr size_t kNumWords = cdiv(k, kBitsPerWord);

		Bitset() = default;
		explicit constexpr Bitset(uintmax_t uintValue) : mWords{ static_cast<Word>(uintValue) }
		{
			assert((uintValue & kLastMask) == uintValue);
			mWords.back() &= kLastMask;
		}

		template<size_t n>
			requires(n < k)
		explicit constexpr Bitset(Bitset<n> b)
		{
			std::ranges::copy(b.mWords, mWords.begin());
		}

		constexpr bool all() const
		{
			for (size_t i = 0; i + 1 < kNumWords; ++i)
				if (mWords[i] != kAllBitsMask)
					return false;
			return mWords.back() == kLastMask;
		}

		constexpr bool none() const
		{
			for (size_t i = 0; i < kNumWords; ++i)
				if (mWords[i])
					return false;
			return true;
		}

		constexpr bool any() const
		{
			return !none();
		}

		constexpr size_t count() const
		{
			size_t n = 0;
			for (size_t i = 0; i < kNumWords; ++i)
				n += std::popcount(mWords[i]);
			return n;
		}

		template<size_t kStart, size_t kCount>
		constexpr Bitset<kCount> extract() const
		{
			static_assert(kStart + kCount <= k);

			if constexpr (kCount <= 0)
				return {};

			Bitset<kCount> b;
			using WordB = Bitset<kCount>::Word;
			[&b, this] <size_t... kI>(std::index_sequence<kI...>) {
				((b.mWords[kI] = extractWord<typename Bitset<kCount>::Word, kStart + kI * Bitset<kCount>::kBitsPerWord>()), ...);
			}(std::make_index_sequence<Bitset<kCount>::kNumWords>());
			b.mWords.back() &= Bitset<kCount>::kLastMask;
			return b;
		}

		template<size_t n>
		constexpr void bitorAt(size_t startI, Bitset<n> b)
		{
			assert(startI + n <= k);

			const size_t wordI = startI / kBitsPerWord;
			const size_t bitI = startI % kBitsPerWord;

			Bitset<n + kBitsPerWord - 1> bext(b);
			bext <<= bitI;
			
			const size_t end = std::min(bext.kNumWords, kNumWords - wordI);
			for (size_t i = 0; i < end; ++i)
				mWords[wordI + i] |= bext.mWords[i];
		}

		constexpr Word toUInt() const requires(kNumWords == 1)
		{
			return mWords.front();
		}

		constexpr Bitset operator&(Bitset b) const
		{
			for (size_t i = 0; i < kNumWords; ++i)
				b.mWords[i] &= mWords[i];
			return b;
		}

		constexpr Bitset operator|(Bitset b) const
		{
			for (size_t i = 0; i < kNumWords; ++i)
				b.mWords[i] |= mWords[i];
			return b;
		}

		constexpr Bitset operator~() const
		{
			Bitset b;
			for (size_t i = 0; i < kNumWords; ++i)
				b.mWords[i] = ~mWords[i];
			b.mWords.back() &= kLastMask;
			return b;
		}

		constexpr Bitset operator<<(size_t n) const
		{
			assert(n < k);

			const size_t nWords = n / kBitsPerWord;
			const size_t nBits = n % kBitsPerWord;

			Bitset b{};
			if constexpr (kNumWords > 1)
			{
				b.mWords[nWords] = doubleWordShl(mWords[0], 0, nBits);
				for (size_t i = nWords + 1; i < kNumWords; ++i)
					b.mWords[i] = doubleWordShl(mWords[i - nWords], mWords[i - (nWords + 1)], nBits);
			}
			else
				b.mWords.front() <<= n;
			b.mWords.back() &= kLastMask;

			return b;
		}

		constexpr Bitset operator>>(size_t n) const
		{
			assert(n < k);

			const size_t nWords = n / kBitsPerWord;
			const size_t nBits = n % kBitsPerWord;

			Bitset b{};
			if constexpr (kNumWords > 1)
			{
				for (size_t i = 0; i + (nWords + 1) < kNumWords; ++i)
					b.mWords[i] = doubleWordShr(mWords[i + (nWords + 1)], mWords[i + nWords], nBits);
				// i + (nWords + 1) == kNumWords
				// i = kNumWords - (nWords + 1)
				b.mWords[kNumWords - (nWords + 1)] = doubleWordShr(0, mWords.back(), nBits);
			}
			else
				b.mWords.front() >>= n;

			return b;
		}

		constexpr Bitset& operator&=(Bitset b) { return *this = *this & b; }
		constexpr Bitset& operator|=(Bitset b) { return *this = *this & b; }
		constexpr Bitset& operator<<=(size_t n) { return *this = *this >> n; }
		constexpr Bitset& operator>>=(size_t n) { return *this = *this << n; }

	private:
		template<size_t>
		friend class Bitset;

		static constexpr Word kLastMask = kNumWords ? kLowNBitsSet<Word, k - (kNumWords - 1) * kBitsPerWord> : 0;
		static constexpr Word kAllBitsMask = Word(-1);
		std::array<Word, kNumWords> mWords{};

		constexpr static Word doubleWordShl(Word hi, Word lo, size_t n)
		{
			// This should translate to a shld.
			if (n == 0)
				return hi;
			else
				return (hi << n) | (lo >> (kBitsPerWord - n));
		}

		constexpr static Word doubleWordShr(Word hi, Word lo, size_t n)
		{
			// This should translate to a shrd.
			if (n == 0)
				return lo;
			else
				return (lo >> n) | (hi << (kBitsPerWord - n));
		}

		constexpr Word getWordOrZero(size_t i) const
		{
			return i < kNumWords ? mWords[i] : 0;
		}

		template<class WordB, size_t kStartI>
		constexpr WordB extractWord() const
		{
			if constexpr (kNumWords <= 0)
				return 0;

			constexpr size_t kWordI = kStartI / kBitsPerWord;
			constexpr size_t kBitI = kStartI % kBitsPerWord;
			constexpr size_t kCount = std::numeric_limits<WordB>::digits;
			constexpr size_t kEndWordI = std::max(kWordI, std::min(kNumWords - 1, (kStartI + kCount - 1) / kBitsPerWord));

			if constexpr (sizeof(WordB) <= sizeof(Word))
				return static_cast<WordB>(doubleWordShr(getWordOrZero(kWordI + 1), getWordOrZero(kWordI), kBitI));
			else
			{
				WordB r = mWords[kWordI] >> kBitI;
				for (size_t i = 1; i <= kEndWordI - kWordI; ++i)
					r |= WordB(mWords[i + kWordI]) << (i * kBitsPerWord - kBitI);
				return r;
			}
		}
	};
}