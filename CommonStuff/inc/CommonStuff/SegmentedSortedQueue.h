#pragma once

#include "range.h"

#include <array>
#include <cassert>
#include <algorithm>
#include <vector>

namespace heck
{
	struct SegmentedSortedQueue
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

		static constexpr int kNumQueues = 8;
		static constexpr int kFirstQueueCapacity = 32;
		static constexpr int kCapacityGrowthShift = 2;
		static constexpr int kSpillAmountDivider = 2;

		// If true, queue max <= next queue min, always.
		// If false, the max will be allowed to extend past the next queue. Leave it to nth_element in popN to clean up the mess.
		static constexpr bool kStrictBinning = true;

		std::array<std::vector<Entry>, kNumQueues> queues;

		bool firstQueueSorted = true;

		alignas(64) std::array<int, kNumQueues + 1> queueMinKeys{};

		static size_t calcQueueCapacity(size_t queueI)
		{
			return kFirstQueueCapacity << (kCapacityGrowthShift * queueI);
		}

		static size_t getSpillStart(size_t queueI)
		{
			const size_t capacity = calcQueueCapacity(queueI);
			return capacity - capacity / kSpillAmountDivider;
		}

		SegmentedSortedQueue()
		{
			queueMinKeys.fill(INT_MAX);
		//	size_t nextQueueCapacity = kFirstQueueCapacity;
		//	for (const int i : range(kNumQueues))
		//	{
		//		queues[i].entries.resize(nextQueueCapacity);
		//		nextQueueCapacity *= kCapacityGrowthFactor;
		//	}
		}

		void pushN(std::span<Entry> entries)
		{
			size_t entriesI = 0;
			for (const int queueI : range(kNumQueues))
			{
				if (entriesI >= entries.size())
					break;
				const auto nextIt = std::partition(entries.begin() + entriesI, entries.end(), [mid = queueMinKeys[queueI + 1]](Entry entry) {
					return entry.key <= mid;
					});
				if (nextIt != entries.begin() + entriesI)
				{
					queueMinKeys[queueI] = std::min(queueMinKeys[queueI], std::ranges::min(entries.subspan(entriesI, nextIt - entries.begin() - entriesI), std::less<>()).key);
					queues[queueI].insert(queues[queueI].end(), entries.begin() + entriesI, nextIt);
					if (queueI == 0)
						firstQueueSorted = false;
				}
				trySpill(queueI);
				entriesI = nextIt - entries.begin();
			}


			//if constexpr (kStrictBinning)
			//{
			//	std::ranges::sort(entries, std::less<>());
			//
			//	size_t entriesI = 0;
			//	size_t queueI = 0;
			//
			//	while (entriesI < entries.size())
			//	{
			//		const int nextQueueKey = queueMinKeys[queueI + 1];
			//		size_t nextQueueEntryI = entriesI;
			//		while (nextQueueEntryI < entries.size() && entries[nextQueueEntryI].key < nextQueueKey)
			//			++nextQueueEntryI;
			//		queues[queueI].append_range(std::span(entries).subspan(entriesI, nextQueueEntryI - entriesI));
			//		queueMinKeys[queueI] = std::min(queueMinKeys[queueI], entries[entriesI].key);
			//		checkQueueOrder();
			//
			//		if (queueI + 1 < kNumQueues)
			//			(void)trySpill(queueI);
			//
			//		entriesI = nextQueueEntryI;
			//		++queueI;
			//	}
			//}
			//else
			//{
			//	queues[0].append_range(entries);
			//	queueMinKeys[0] = std::min(queueMinKeys[0], std::ranges::min(entries, std::less<>(), &Entry::key).key);
			//	for (const int queueI : range(kNumQueues - 1))
			//		if (!trySpill(queueI))
			//			break;
			//}
		}

		size_t popN(std::span<Entry> entries)
		{
			size_t index = 0;
			auto& queue0 = queues.front();
			while (queue0.size() && index < entries.size())
			{
				const size_t reqRemaining = entries.size() - index;
				if (queue0.size() <= reqRemaining)
				{ // Emptying out this queue
					if constexpr (!kStrictBinning)
					{
						// Need to check whether the max extends past the next queue.
						const int maxKey = std::ranges::max(queue0, std::less<>(), &Entry::key).key;
						if (maxKey > queueMinKeys[1])
						{
							// Just dump everything into the next queue and refill.
							queues[1].append_range(queues[0]);
							queue0.clear();
							queueMinKeys[1] = queueMinKeys[0];
							checkQueueOrder();
							refill();
							continue;
						}
					}
					std::ranges::copy(queue0, std::span(entries).subspan(index).begin());
					index += queue0.size();
					queue0.clear();
					refill();
				}
				else
				{ // Will still have some left
					if (!firstQueueSorted)
					{
						//std::ranges::nth_element(queue0, queue0.begin() + reqRemaining, std::less<>());
						std::ranges::sort(queue0, std::less<>());
						firstQueueSorted = true;
					}
					if constexpr (!kStrictBinning)
					{
						if (queue0[reqRemaining].key > queueMinKeys[1])
						{
							// Just dump everything into the next queue and refill.
							queues[1].append_range(queues[0]);
							queue0.clear();
							queueMinKeys[1] = queueMinKeys[0];
							checkQueueOrder();
							refill();
							continue;
						}
					}
					std::ranges::copy(std::span(queue0).subspan(0, reqRemaining), std::span(entries).subspan(index).begin());
					index = entries.size();
					queue0.erase(queue0.begin(), queue0.begin() + reqRemaining);
					queueMinKeys[0] = queue0.front().key;
					checkQueueOrder();
					break;
				}
			}

			return index;
		}

	private:

		void checkQueueOrder()
		{
			assert(std::ranges::is_sorted(queueMinKeys));
		}

		void refill()
		{
			for (const int queueI : range(kNumQueues - 1))
			{
				auto& thisQueue = queues[queueI];
				auto& nextQueue = queues[queueI + 1];
				assert(thisQueue.empty());
				queueMinKeys[queueI] = INT_MAX;
				if (nextQueue.empty())
					break;

				const size_t queueCapacity = calcQueueCapacity(queueI);
				const size_t maxRefillAmount = queueCapacity - queueCapacity / kSpillAmountDivider;
				const size_t availableRefillAmount = std::min(maxRefillAmount, nextQueue.size());
				//const size_t leftoverAmount = nextQueue.entries.size() - availableRefillAmount;
				std::ranges::nth_element(nextQueue, nextQueue.begin() + availableRefillAmount, std::less<>());
				thisQueue.append_range(std::span(nextQueue).subspan(0, availableRefillAmount));
				if (queueI == 0)
				{
					if (firstQueueSorted)
						std::sort(thisQueue.end() - availableRefillAmount, thisQueue.end()); // keep it sorted
				}
				queueMinKeys[queueI] = std::ranges::min(thisQueue, std::less<>(), &Entry::key).key;
				checkQueueOrder();
				nextQueue.erase(nextQueue.begin(), nextQueue.begin() + availableRefillAmount);

				if (thisQueue.empty())
				{
					queueMinKeys[queueI] = INT_MAX;
					checkQueueOrder();
					break;
				}
				else if (!nextQueue.empty())
				{
					queueMinKeys[queueI + 1] = nextQueue.front().key;
					if constexpr (!kStrictBinning)
					{
						// Might need to shift the queue down to maintain min key order.
						for (const int swapQueueI : range(queueI + 1, kNumQueues - 1))
						{
							if (queueMinKeys[swapQueueI] > queueMinKeys[swapQueueI + 1])
							{
								std::swap(queueMinKeys[swapQueueI], queueMinKeys[swapQueueI + 1]);
								std::swap(queues[swapQueueI], queues[swapQueueI + 1]);
							}
							else
								break;
						}
					}
					checkQueueOrder();
					break;
				}
				//else
				//{
				//	queueMinKeys[queueI + 1] = INT_MAX;
				//	checkQueueOrder();
				//}
			}

			checkQueueOrder();
		}

		bool trySpill(size_t queueI)
		{
			if (queues[queueI].size() >= calcQueueCapacity(queueI))
			{
				const size_t spillStart = getSpillStart(queueI);
				if (queueI != 0 || !firstQueueSorted)
				{
					if (queueI == 0)
					{
						std::ranges::sort(queues[0], std::less<>());
						firstQueueSorted = true;
					}
					else
						std::ranges::nth_element(queues[queueI], queues[queueI].begin() + spillStart, std::less<>());
				}
				queueMinKeys[queueI + 1] = std::min(queueMinKeys[queueI + 1], queues[queueI][spillStart].key);
				queues[queueI + 1].append_range(std::span(queues[queueI]).subspan(spillStart));
				queues[queueI].resize(spillStart);
				checkQueueOrder();
				return true;
			}
			else
				return false;
		}
	};
}