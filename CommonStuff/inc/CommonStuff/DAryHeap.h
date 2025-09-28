#pragma once

#include "SimdSortUtil.h"
#include "range.h"

#include <array>
#include <cassert>
#include <vector>

namespace heck
{
	/*
	template<class Key, class Value, unsigned int k>
	struct DAryHeap
	{
		struct alignas(64) Node
		{
			std::array<Key, k> keys{};
			std::array<Value, k> values{};
		};

		size_t numEntries = 0;
		Key topKey{};
		Value topValue{};
		std::vector<Node> nodes; // [parentI]

		//explicit DAryHeap(Key key, Value value) : numEntries(1), topKey(key), topValue(value)
		//{
		//}

		bool empty() const
		{
			return numEntries <= 0;
		}

		size_t size() const
		{
			return nodes.size();
		}

		Key frontKey() const
		{
			return topKey;
		}

		Value frontValue() const
		{
			return topValue;
		}

		void reheapify()
		{
			DAryHeap b;
			for (const Node& node : nodes)
				for (const auto& [key, value] : std::views::zip(node.keys, node.values))
					if (key < INT_MAX)
						b.push(key, value);
			*this = std::move(b);
		}

		void push(Key key, Value value)
		{
			if (nodes.size() * k + 1 >= numEntries)
			{
				nodes.emplace_back();
				nodes.back().keys.fill(INT_MAX);
			}

			size_t i = numEntries;
			++numEntries;
			keyRef(i) = key;
			valueRef(i) = value;

			// Bubble up.
			while (i > 0)
			{
				const size_t parentI = parentIndex(i);

				if (key < keyRef(parentI))
				{
					swapEntries(i, parentI);
					i = parentI;
				}
				else
					break;
			}

			//if (numEntries >= 2)
			//	assert(topKey <= nodes[0].keys[0]);
		}

		struct Entry
		{
			Key key;
			Value value;
		};

		Entry pop()
		{
			assert(!empty());

			const Entry result{ topKey, topValue };

			--numEntries;
			swapEntries(0, numEntries);
			keyRef(numEntries) = INT_MAX;

			Key* currentKeyPtr = &topKey;
			Value* currentValuePtr = &topValue;
			const Key key = *currentKeyPtr;
			const Value value = *currentValuePtr;


			const __m256i key256 = _mm256_set1_epi32(key);

			// Bubble down.
			size_t i = 0;
			for (;;)
			{
				//const size_t firstChildI = firstChildIndex(i);
				//if (firstChildI >= numEntries)
				//	break;

				const size_t nodeI = i;
				const size_t lastNodeFirstElementI = 1 + nodeI * k;
				if (numEntries <= lastNodeFirstElementI)
					break;
				//const size_t occupancy = std::min<size_t>(numEntries - lastNodeFirstElementI, k);
				//const size_t occupancy = k;

				static_assert(k == 8);

				auto& childKeys = nodes[nodeI].keys;

				if (_mm256_movemask_epi8(_mm256_cmpgt_epi32(key256, _mm256_load_si256((const __m256i*)childKeys.data()))))
				{
					const std::pair<int, Key> min01 = std::pair(childKeys[0] < childKeys[1] ? 0 : 1, std::min(childKeys[0], childKeys[1]));
					const std::pair<int, Key> min23 = std::pair(childKeys[2] < childKeys[3] ? 2 : 3, std::min(childKeys[2], childKeys[3]));
					const std::pair<int, Key> min45 = std::pair(childKeys[4] < childKeys[5] ? 4 : 5, std::min(childKeys[4], childKeys[5]));
					const std::pair<int, Key> min67 = std::pair(childKeys[6] < childKeys[7] ? 6 : 7, std::min(childKeys[6], childKeys[7]));
					const std::pair<int, Key> min0123 = min01.second < min23.second ? min01 : min23;
					const std::pair<int, Key> min4567 = min45.second < min67.second ? min45 : min67;
					const std::pair<int, Key> minI = min0123.second < min4567.second ? min0123 : min4567;

					//const auto minIt = std::min_element(childKeys.begin(), childKeys.begin() + occupancy);
					//if (*minIt < key)
					{
						//const size_t localJ = minIt - childKeys.begin();
						const size_t localJ = minI.first;
						const size_t globalJ = lastNodeFirstElementI + localJ;
						//swapEntries(i, globalJ);

						Key& childKeyPtr = childKeys[localJ];
						Value& childValuePtr = nodes[nodeI].values[localJ];
						*currentKeyPtr = std::exchange(childKeyPtr, key);
						*currentValuePtr = std::exchange(childValuePtr, value);
						currentKeyPtr = &childKeyPtr;
						currentValuePtr = &childValuePtr;

						i = globalJ;
					}
					//else
					//	break;
				}
				else
					break;
			}

			//if (numEntries >= 2)
			//	assert(topKey <= nodes[0].keys[0]);

			return result;
		}

	private:
		static size_t parentIndex(size_t childI)
		{
			return (childI - 1) / k;
		}

		static size_t firstChildIndex(size_t parentI)
		{
			return parentI * k + 1;
		}

		Key& keyRef(size_t i)
		{
			return i == 0 ? topKey : nodes[(i - 1) / k].keys[(i - 1) % k];
		}

		Value& valueRef(size_t i)
		{
			return i == 0 ? topValue : nodes[(i - 1) / k].values[(i - 1) % k];
		}

		void swapEntries(size_t a, size_t b)
		{
			std::swap(keyRef(a), keyRef(b));
			std::swap(valueRef(a), valueRef(b));
		}
	};*/

	template<class Key, class Value, unsigned int k>
	struct DAryHeap2
	{
		struct Node
		{
			Key key;
			Value value;
		};

		std::vector<Node> nodes;

		//explicit DAryHeap2(Key key, Value value) : nodes{ { key, value } }
		//{
		//}

		bool empty() const
		{
			return nodes.empty();
		}

		size_t size() const
		{
			return nodes.size();
		}


		Key frontKey() const
		{
			return nodes[0].key;
		}

		Value frontValue() const
		{
			return nodes[0].value;
		}

		std::span<Node> elements()
		{
			return nodes;
		}

		void reheapify()
		{
			// Starting at index of last non-leaf.
			//const std::span localNodes = nodes;
			//for (size_t i = localNodes.size() / k - 1; i != SIZE_MAX; --i)
			//	minHeapify(localNodes, i, localNodes[i]);

			// This is actually faster... probably, as this is a 4-ary heap, pushing does less comparisons.
			//DAryHeap2 b;
			//b.nodes.reserve(nodes.size());
			//for (auto item : nodes)
			//	b.push(item.key, item.value);
			//*this = std::move(b);

			const std::span localNodes = nodes;
			for (const size_t i : heck::range(size_t(1), localNodes.size()))
				bubbleUp(localNodes, i, localNodes[i]);
		}

		void push(Key key, Value value)
		{
			size_t i = nodes.size();

			const Node activeNode{ key, value };
			nodes.push_back(activeNode);

			bubbleUp(nodes, i, activeNode);
		}

		Node pop()
		{
			const Node result = nodes.front();

			const Node activeNode = nodes.back();
			nodes.front() = activeNode;
			nodes.pop_back();

			if (!nodes.empty())
				minHeapify(nodes, 0, activeNode);

			return result;
		}

	private:
		static size_t parentIndex(size_t childI)
		{
			return (childI - 1) / k;
		}

		static size_t firstChildIndex(size_t parentI)
		{
			return parentI * k + 1;
		}

		static void bubbleUp(std::span<Node> localNodes, size_t i, Node activeNode)
		{
			// Bubble up.
			while (i > 0)
			{
				const size_t parentI = parentIndex(i);

				if (activeNode.key < localNodes[parentI].key)
				{
					localNodes[i] = localNodes[parentI];
					i = parentI;
				}
				else
					break;
			}

			localNodes[i] = activeNode;
		}

		//static std::pair<size_t, Key> cmp2(std::span<const Node> localNodes, size_t childI)
		//{
		//	const Key key0 = localNodes[childI].key;
		//	const Key key1 = localNodes[childI + 1].key;
		//	return key0 <= key1
		//		? std::pair(childI, key0)
		//		: std::pair(childI + 1, key1);
		//}

		static void minHeapify(std::span<Node> localNodes, size_t parentI, Node activeNode)
		{
			// firstChildIndex(lastCompleteNodeI) + k <= localNodes.size()
			// lastCompleteNodeI * k + 1 + k <= localNodes.size()
			// lastCompleteNodeI * k <= localNodes.size() - 1 - k
			// lastCompleteNodeI <= (localNodes.size() - 1 - k) / k
			// lastCompleteNodeI = (localNodes.size() - 1) / k - 1
			// firstPartialNodeI = (localNodes.size() - 1) / k
			const size_t firstPartialNodeI = (localNodes.size() - 1) / k;

			//static_assert(k == 4);

			while (parentI < firstPartialNodeI)
			{
				const size_t childI = firstChildIndex(parentI);
				//const Key cmp0K = std::min(localNodes[childI + 1].key, localNodes[childI].key);
				//const Key cmp1K = std::min(localNodes[childI + 3].key, localNodes[childI + 2].key);

				// MSVC terrible codegen
				//const auto [lowestI, lowestK] = staticReduce<k>(childI,
				//	[](std::pair<int, Key> a, std::pair<int, Key> b) { return a.second <= b.second ? a : b; },
				//	[localNodes](size_t i) { return std::pair((int)i, localNodes[i].key); }
				//);

				const int lowestI = staticReduce<k>(childI,
					[localNodes](int a, int b) { return localNodes[a].key <= localNodes[b].key ? a : b; },
					[](size_t i) { return int(i); }
				);

				const Key lowestK = localNodes[lowestI].key;

				//const auto [cmp0I, cmp0K] = cmp2(localNodes, childI);
				//const auto [cmp1I, cmp1K] = cmp2(localNodes, childI + 2);
				//const size_t lowestI = cmp0K <= cmp1K ? cmp0I : cmp1I;
				//if (activeNode.key > std::min(cmp0K, cmp1K))
				if (activeNode.key > lowestK)
				{
					//const size_t cmp0I = childI + (localNodes[childI + 1].key < localNodes[childI].key);
					//const size_t cmp1I = childI + 2 + (localNodes[childI + 3].key < localNodes[childI + 2].key);
					localNodes[parentI] = localNodes[lowestI];
					//localNodes[lowestI] = activeNode;
					parentI = lowestI;
				}
				else
				{
					localNodes[parentI] = activeNode;
					return;
				}
			}

			const size_t childI = firstChildIndex(parentI);
			if (childI < localNodes.size())
			{
				const std::span childNodes = localNodes.subspan(childI);
				assert(childNodes.size() < k);
				const auto minIt = std::ranges::min_element(childNodes, std::less<>(), &Node::key);
				if (activeNode.key > minIt->key)
				{
					localNodes[parentI] = *minIt;
					*minIt = activeNode;
					return;
				}
			}

			localNodes[parentI] = activeNode;
		}
	};

	/*
	template<class T>
	struct allocator : std::allocator<T>
	{
		using std::allocator<T>::allocator;

		using _Ty = T;

		_CONSTEXPR20 void deallocate(_Ty* const _Ptr, const size_t _Count) {
			_STL_ASSERT(_Ptr != nullptr || _Count == 0, "null pointer cannot point to a block of non-zero size");
			// no overflow check on the following multiply; we assume _Allocate did that check
			_STD _Deallocate<64>(_Ptr, sizeof(_Ty) * _Count);
		}

		_NODISCARD_RAW_PTR_ALLOC _CONSTEXPR20 __declspec(allocator) _Ty* allocate(_CRT_GUARDOVERFLOW const size_t _Count) {
			return static_cast<_Ty*>(_STD _Allocate<64>(std::_Get_size_of_n<sizeof(_Ty)>(_Count)));
		}

		_NODISCARD_RAW_PTR_ALLOC constexpr std::allocation_result<_Ty*> allocate_at_least(
			_CRT_GUARDOVERFLOW const size_t _Count) {
			return { allocate(_Count), _Count };
		}
	};

	// http://phk.freebsd.dk/B-Heap/queue.html
	template<class Key, class Value, unsigned int kPageSize>
	struct BHeap
	{
		struct Entry
		{
			Key key{};
			Value value{};
		};



		bool empty() const
		{
			return size() == 0;
		}

		size_t size() const
		{
			return nodes.size() - (1 - kAdjust);
		}


		Key frontKey() const
		{
			return nodes[1 - kAdjust].key;
		}

		Value frontValue() const
		{
			return nodes[1 - kAdjust].value;
		}

		void reheapify()
		{
			BHeap b;
			for (const Entry& node : elements())
				b.push(node.key, node.value);
			*this = std::move(b);
		}

		void push(Key key, Value value)
		{
			size_t i = nodes.size();

			const Entry activeNode{ key, value };

			nodes.push_back(activeNode);

			const std::span localNodes = nodes;

			// Bubble up.
			while (i > 1 - kAdjust)
			{
				const size_t parentI = parentIndex(i);

				if (key < localNodes[parentI].key)
				{
					localNodes[i] = localNodes[parentI];
					localNodes[parentI] = activeNode;
				}
				else
					break;

				i = parentI;
			}
		}

		Entry pop()
		{
			const Entry result = nodes[1 - kAdjust];

			const Entry activeNode = nodes.back();

			nodes[1 - kAdjust] = activeNode;

			nodes.pop_back();

			const std::span localNodes = nodes;

			const size_t n = localNodes.size();

			// Bubble down.
			size_t i = 1 - kAdjust;
			for (;;)
			{
				const size_t firstChildI = firstChildIndex(i);
				const size_t swapChildI = firstChildI + (!isDegreeOneParent(i) && firstChildI + 1 < n && localNodes[firstChildI + 1].key < localNodes[firstChildI].key);

				if (swapChildI < n && activeNode.key > localNodes[swapChildI].key)
				{
					localNodes[i] = localNodes[swapChildI];
					localNodes[swapChildI] = activeNode;
				}
				else
					break;

				i = swapChildI;
			}

			return result;
		}

		std::span<Entry> elements()
		{
			return std::span(nodes).subspan(1 - kAdjust);
		}

	private:
		std::vector<Entry, allocator<Entry>> nodes{ Entry{} };

		// These are all written for one-based arrays.
		// So an adjustment is applied before/after.

		//static constexpr size_t kAdjust = 1;
		static constexpr size_t kAdjust = 0;

		static constexpr size_t parentIndex(size_t childI)
		{
			childI += kAdjust;

			const size_t offset = childI % kPageSize;
			if (offset < 2)
			{
				const size_t childPageI = childI / kPageSize;
				const size_t parentPageI = (childPageI - 1) / (kPageSize / 2);
				const size_t parentOffset = (childPageI - 1) % (kPageSize / 2);
				return (parentPageI * kPageSize + kPageSize / 2) + parentOffset - kAdjust;
			}
			else if (offset < 4)
			{
				return childI > 3 ? childI - 2 - kAdjust : 1 - kAdjust;
			}
			else
			{
				return (childI - offset) + offset / 2 - kAdjust;
			}
		}

		static_assert(kPageSize != 8 || parentIndex(40 - kAdjust) == 12 - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(41 - kAdjust) == 12 - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(42 - kAdjust) == 40 - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(16 - kAdjust) == 5  - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(17 - kAdjust) == 5  - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(32 - kAdjust) == 7  - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(33 - kAdjust) == 7  - kAdjust);
		static_assert(kPageSize != 8 || parentIndex(39 - kAdjust) == 35 - kAdjust);

		static constexpr size_t firstChildIndex(size_t parentI)
		{
			parentI += kAdjust;

			const size_t offset = parentI % kPageSize;
			const size_t pageBase = parentI - offset;
			if (offset < 2)
				return parentI > 1 ? parentI + 2 - kAdjust : 2 - kAdjust;
			else if (offset < kPageSize / 2)
				return pageBase + offset * 2 - kAdjust;
			else
			{
				const size_t parentPageI = parentI / kPageSize;
				const size_t leafI = parentI % (kPageSize / 2);
				const size_t childPageI = parentPageI * (kPageSize / 2) + 1 + leafI;
				return childPageI * kPageSize - kAdjust;
			}
		}

		static_assert(kPageSize != 8 || firstChildIndex(1  - kAdjust) == 2  - kAdjust);
		static_assert(kPageSize != 8 || firstChildIndex(4  - kAdjust) == 8  - kAdjust);
		static_assert(kPageSize != 8 || firstChildIndex(5  - kAdjust) == 16 - kAdjust);
		static_assert(kPageSize != 8 || firstChildIndex(12 - kAdjust) == 40 - kAdjust);

		static constexpr bool isDegreeOneParent(size_t parentI)
		{
			parentI += kAdjust;
			const size_t offset = parentI % kPageSize;
			return parentI > 1 && offset < 2;
		}

		static_assert(kPageSize != 8 || !isDegreeOneParent(1 - kAdjust));
		static_assert(kPageSize != 8 || isDegreeOneParent(8  - kAdjust));
		static_assert(kPageSize != 8 || isDegreeOneParent(9  - kAdjust));
	};*/

	// Radix Heap: https://ssp.impulsetrain.com/radix-heap.html
	// Requires a consistent heuristic.

	// Sequence Heap: https://ae.iti.kit.edu/documents/people/sanders/papers/spqjea.pdf
	// How the heck does that work.

	template<class Key, class Value>
	struct StdHeap
	{
		struct Node
		{
			Key key;
			Value value;
		};

		std::vector<Node> nodes;

		bool empty() const
		{
			return nodes.empty();
		}

		size_t size() const
		{
			return nodes.size();
		}

		Key frontKey() const
		{
			return nodes.front().key;
		}

		Value frontValue() const
		{
			return nodes.front().value;
		}

		std::span<Node> elements()
		{
			return nodes;
		}

		void reheapify()
		{
			std::ranges::make_heap(nodes, std::greater<>(), &Node::key);
		}

		void push(Key key, Value value)
		{
			nodes.emplace_back(key, value);
			std::ranges::push_heap(nodes, std::greater<>(), &Node::key);
		}

		using Entry = Node;

		Entry pop()
		{
			const Entry result = nodes.front();

			std::ranges::pop_heap(nodes, std::greater<>(), &Node::key);
			nodes.pop_back();

			return result;
		}
	};
}