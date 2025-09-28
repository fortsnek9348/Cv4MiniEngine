#pragma once

#include "range.h"

#include <cassert>
#include <array>
#include <vector>
#include <span>
#include <bit>
#include <algorithm>

namespace heck
{
	struct HeapOfHeaps
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

		static constexpr size_t kChunkSize = 16;
		static constexpr size_t kStagingLimit = kChunkSize * 4;

		size_t stagingStart = 0;
		int stagingMin = INT_MAX;
		std::vector<Entry> storage;

		std::vector<uint32_t> bigHeapEntries;

		void pushN(std::span<const Entry> entries)
		{
			storage.append_range(entries);
			stagingMin = std::min(stagingMin, std::ranges::min(entries, std::less<>()).key);
		}

		void push(Entry entry)
		{
			storage.push_back(entry);
			stagingMin = std::min(stagingMin, entry.key);
		}

		size_t popN(std::span<Entry> entries)
		{
			doStagingHeapify();

			size_t numOut = 0;
			while (!entries.empty())
			{
				//int lowestKey = INT_MAX;
				//for (uint32_t bigHeapStart : bigHeapEntries)
				//{
				//	do
				//		lowestKey = std::min(lowestKey, storage[bigHeapStart].key);
				//	while (++bigHeapStart % kChunkSize != 0);
				//}
				//if (!std::span(storage).subspan(stagingStart).empty())
				//	lowestKey = std::min(lowestKey, std::ranges::min(std::span(storage).subspan(stagingStart), std::less<>()).key);

				

				if (bigHeapEntries.empty() || stagingMin < storage[bigHeapEntries.front()].key)
				{
					if (const std::optional<Entry> entry = tryPopStaging())
						entries.front() = *entry;
					else
						break;
				}
				else
				{
					entries.front() = storage[bigHeapEntries.front()];
					if (++bigHeapEntries.front() % kChunkSize == 0)
						popHeapEmptyChunk();
					else
						bubbleDownIncreasedKey();
				}

				//const int poppedKey = entries.front().key;

				//assert(poppedKey == lowestKey);
				

				entries = entries.subspan(1);
				++numOut;
			}

			return numOut;
		}

	private:
		void checkDuplicateHeap()
		{
			//auto bigHeapEntries2 = bigHeapEntries;
			//std::ranges::sort(bigHeapEntries2);
			//assert(std::ranges::adjacent_find(bigHeapEntries2) == bigHeapEntries2.end());
		}
		void doStagingHeapify()
		{
			const std::span<Entry> staging = std::span(storage).subspan(stagingStart);
			if (staging.size() >= kStagingLimit)
			{
				//std::ranges::nth_element(littleHeap, littleHeap.end() - kLittleHeapLimit, std::less<>());

				const size_t numChunks = staging.size() / kChunkSize;

				//std::ranges::partial_sort(staging, staging.begin() + numChunks * kChunkSize, std::less<>());
				std::ranges::sort(staging, std::less<>());
				for (const size_t i : range(numChunks))
				{
					bigHeapEntries.push_back(uint32_t(stagingStart + i * kChunkSize));
					pushHeapNewChunk();
				}

				stagingStart += numChunks * kChunkSize;

				stagingMin = stagingStart < storage.size() ? storage[stagingStart].key : INT_MAX;
			}
		}

		std::optional<Entry> tryPopStaging()
		{
			const std::span<Entry> staging = std::span(storage).subspan(stagingStart);
			const auto stagingMinIt = std::ranges::min_element(staging, std::less<>());
			if (stagingMinIt != staging.end())
			{
				const Entry entry = *stagingMinIt;
				*stagingMinIt = staging.back();
				if (staging.size() >= 2)
					stagingMin = std::ranges::min(staging, std::less<>()).key;
				else
					stagingMin = INT_MAX;

				storage.pop_back();
				return entry;
			}
			else
				return std::nullopt;
		}

		void pushHeapNewChunk()
		{
			const std::span bigHeapEntriesLocal = bigHeapEntries;
			const uint32_t bigHeapEntry = bigHeapEntriesLocal.back();
			const int theKey = storage[bigHeapEntry].key;

			size_t index = bigHeapEntriesLocal.size() - 1;

			

			while (index > 0)
			{
				const size_t parentI = (index - 1) / 2;
				if (theKey < storage[bigHeapEntriesLocal[parentI]].key)
				{
					bigHeapEntriesLocal[index] = bigHeapEntriesLocal[parentI];
					bigHeapEntriesLocal[parentI] = bigHeapEntry;
					index = parentI;
				}
				else
					break;
			}

			checkDuplicateHeap();
		}

		void popHeapEmptyChunk()
		{
			const std::span bigHeapEntriesLocal = bigHeapEntries;
			bigHeapEntriesLocal[0] = bigHeapEntriesLocal.back();
			bigHeapEntries.pop_back();
			if (bigHeapEntriesLocal.size() - 1 > 1)
			{
				//size_t index = 0;
				//
				//
				//
				//// index * 2 + 2 < size
				//// index * 2 < size - 2
				//// index < ceil((size - 2) / 2))
				//const size_t firstLoopLimit = (bigHeapEntriesLocal.size() - 1 - 2 + 1) / 2;
				//
				//while (index < firstLoopLimit)
				//{
				//	const size_t L = index * 2 + 1;
				//	//const size_t R = index * 2 + 2;
				//	const size_t childI = L + (storage[bigHeapEntriesLocal[L + 1]].key < storage[bigHeapEntriesLocal[L]].key);
				//	bigHeapEntriesLocal[index] = bigHeapEntriesLocal[childI];
				//	index = childI;
				//}
				//
				//if (index * 2 + 1 < bigHeapEntriesLocal.size())
				//{
				//	const size_t childI = index * 2 + 1;
				//	bigHeapEntriesLocal[index] = bigHeapEntriesLocal[childI];
				//	index = childI;
				//}
				//
				//assert(index == bigHeapEntriesLocal.size() - 1);
				bubbleDownIncreasedKey();
			}

			
			checkDuplicateHeap();
		}

		void bubbleDownIncreasedKey()
		{
			const std::span bigHeapEntriesLocal = bigHeapEntries;

			if (bigHeapEntriesLocal.size() > 1)
			{
				const uint32_t bigHeapEntry = bigHeapEntriesLocal[0];
				const int theKey = storage[bigHeapEntry].key;

				size_t index = 0;

				// index * 2 + 2 < size
				// index * 2 < size - 2
				// index < ceil((size - 2) / 2))
				const size_t firstLoopLimit = (bigHeapEntriesLocal.size() - 2 + 1) / 2;

				while (index < firstLoopLimit)
				{
					const size_t L = index * 2 + 1;
					//const size_t R = index * 2 + 2;
					const size_t childI = L + (storage[bigHeapEntriesLocal[L + 1]].key < storage[bigHeapEntriesLocal[L]].key);
					if (storage[bigHeapEntriesLocal[childI]].key >= theKey)
					{
						checkDuplicateHeap();
						return;
					}
					bigHeapEntriesLocal[index] = bigHeapEntriesLocal[childI];
					bigHeapEntriesLocal[childI] = bigHeapEntry;
					index = childI;
				}

				if (index * 2 + 1 < bigHeapEntriesLocal.size())
				{
					const size_t childI = index * 2 + 1;
					if (storage[bigHeapEntriesLocal[childI]].key >= theKey)
					{
						checkDuplicateHeap();
						return;
					}
					bigHeapEntriesLocal[index] = bigHeapEntriesLocal[childI];
					bigHeapEntriesLocal[childI] = bigHeapEntry;
					//index = childI;
				}
			}

			checkDuplicateHeap();
		}
	};
}