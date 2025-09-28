#pragma once

#include <cassert>
#include <array>
#include <vector>
#include <span>
#include <bit>

namespace heck
{
	// https://ssp.impulsetrain.com/radix-heap.html
	struct RadixHeap
	{
		struct Entry
		{
			int key = 0;
			int data = 0;

			bool operator<(Entry b) const
			{
				return key < b.key;
			}

			bool operator>(Entry b) const
			{
				return key > b.key;
			}

			friend bool operator==(Entry, Entry) = default;
		};

		static constexpr int kNumBuckets = 31;

		int lastDeleted = 0;
		std::array<std::vector<Entry>, kNumBuckets> buckets;
		uint32_t bucketsNonEmpty = 0;
		//std::array<uint32_t, kNumBuckets> bucketSortEnds{};

		bool empty() const
		{
			return bucketsNonEmpty == 0;
		}

		void pushN(std::span<const Entry> entries)
		{
			for (const Entry entry : entries)
			{
				assert(entry.key >= lastDeleted);
				const int bucketI = std::bit_width(unsigned(entry.key ^ lastDeleted));
				buckets[bucketI].push_back(entry);
				bucketsNonEmpty |= uint32_t(1) << bucketI;
			}
		}

		size_t popN(std::span<Entry> entries)
		{
			size_t index = 0;
			while (index < entries.size())
			{
				//size_t bucketI = 0;
				//while (bucketI < kNumBuckets && buckets[bucketI].empty())
				//	++bucketI;
				const size_t bucketI = std::countr_zero(bucketsNonEmpty);
				if (bucketI >= kNumBuckets)
					break;

				const size_t reqN = std::min(buckets[bucketI].size(), entries.size() - index);

				//if (bucketSortEnds[bucketI] < reqN)
				{
					std::ranges::nth_element(buckets[bucketI], buckets[bucketI].begin() + reqN - 1, std::less<>());
					//std::ranges::sort(buckets[bucketI], std::greater<>());
					//bucketSortEnds[bucketI] = buckets[bucketI].size();
				}

				//const int maxDel = std::ranges::max(std::span(buckets[bucketI]).subspan(0, reqN), std::less<>()).key;
				const int maxDel = buckets[bucketI][reqN - 1].key;


				//const auto delIt = std::ranges::min_element(buckets[bucketI], std::less<>(), &Entry::key);
				const bool redist = lastDeleted != maxDel;
				lastDeleted = maxDel;
				//entries[index++] = *delIt;

				std::copy_n(buckets[bucketI].begin(), reqN, entries.begin() + index);
				index += reqN;

				//*delIt = buckets[bucketI].back();
				//buckets[bucketI].pop_back();
				//if (buckets[bucketI].empty())
				//	bucketsNonEmpty &= ~(uint32_t(1) << bucketI);

				if (redist)
				{
					pushN(std::span(buckets[bucketI]).subspan(reqN));
					buckets[bucketI].clear();
					bucketsNonEmpty &= ~(uint32_t(1) << bucketI);
					//bucketSortEnds[bucketI] = 0;
				}
				else
				{
					std::copy_n(buckets[bucketI].end() - reqN, reqN, buckets[bucketI].begin());
					buckets[bucketI].erase(buckets[bucketI].end() - reqN, buckets[bucketI].end());
					if (buckets[bucketI].empty())
					{
						bucketsNonEmpty &= ~(uint32_t(1) << bucketI);
						//bucketSortEnds[bucketI] = 0;
					}
				}

			}

			return index;
		}
	};
}