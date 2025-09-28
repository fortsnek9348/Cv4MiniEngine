#pragma once

#include "SimdSortUtil.h"
#include "range.h"


#include <vector>
#include <functional>

namespace heck
{
	// Heap strictly for SIMD A*: Each node is a Vector of heap values and an array of i16vec2.
	// Each parent node has max <= child min.
	// A node's keys are always sorted.
	// Each node has all elements used. Extras are listed in a small buffer.
	// Push: Sort node (16), append node, merge with parent (half of 32-sort, node that parent max is non-increasing), bubble up (starting at parent!).
	// Pop: Replace root with last node, bubble down.
	// Bubble up (push's parent): If min less than parent max, merge with parent, continue bubble up.
	// Bubble down (pop's node):
	//     If max is greater than any child min,
	//         Swap with child with lowest max.
	//         Merge with all children in sequence. Note that merging will not increase the max of any *other* child because other children already contain the max.
	//         Continue bubble down with swapped child.
	// Merge: Can be done using 5 sort layers for 32 elements total.
	// Heap is quaternary. For 1000 plots, depth is at most 4, so popping will need at most 3 bubble downs: h = ceiling(log((degree-1)*n+1)/log(degree))
	// Also note that pushes will tend stay at the bottom of the heap, afaik.
	// And when a node is popped, the replacement will probably have max >= child max, so merging may be trivial until bottom layers.
	//
	// Supporting a partial node: If you want to push individual elements, there has to be a partially filled node. And it should be kept at the end of the array.
	//     Pushing: When bubbling up, merging with parent will leave the unused padding entries in the last node.
	//     Popping: Swap the padding elements in the last node with the last elements in the prior node in the array. Sort the last node. Then move to the root and continue.
	//     Not implemented right now. Doing a sort on every pop doesn't sound great.

	template<class Key, class Value, size_t kVectorWidth, size_t kDegree>
	class SimdHeap
	{
	public:
		// Somewhat disappointly, the SIMD implementation is only slightly faster.
		static constexpr bool kUseScalarImplementation = false;

		using KeyVector = heck::simd::Vector<Key, kVectorWidth>;
		using ValueVector = heck::simd::Vector<Value, kVectorWidth>;

		struct Node
		{
			KeyVector keys{};
			ValueVector values{};

			Key min() const
			{
				return keys.template getElement<0>();
			}

			Key max() const
			{
				return keys.template getElement<kVectorWidth - 1>();
			}
		};

		void clear()
		{
			mNodes.clear();
		}

		bool empty() const
		{
			return mNodes.empty();
		}
		
		void push(Node node)
		{
			node = sortNode(node);

			const size_t i = mNodes.size();
			
			mNodes.push_back(node);

			bubbleUp(mNodes, i);
		}

		Node pop()
		{
			const Node result = mNodes.front();

			mNodes.front() = mNodes.back();
			mNodes.pop_back();

			if (!mNodes.empty())
				bubbleDown(mNodes, 0);

			return result;
		}

		Key peekPopMin() const
		{
			return mNodes.front().min();
		}

		Key peekPopMax() const
		{
			return mNodes.front().max();
		}

		std::span<Node> elements()
		{
			return mNodes;
		}

		void reheapify()
		{
			const std::span localNodes = mNodes;
			for (size_t i = 1; i < localNodes.size(); ++i)
				bubbleUp(localNodes, i);
		}

		void checkHeapProperty() const
		{
			for (const size_t i : range(mNodes.size()))
				if (!std::ranges::is_sorted(mNodes[i].keys.toArray()))
					std::abort();

			for (const size_t childI : range(size_t(1), mNodes.size()))
			{
				const size_t parentI = calcParentIndex(childI);
				if (mNodes[parentI].max() > mNodes[childI].min())
					std::abort();
			}
		}

		// Merge sorted nodes.
		// Also a utility function used by SimdHybridHeap.
		static std::array<Node, 2> HECK_VECTORCALL mergeNodes(Node A, Node B)
		{
			//static std::atomic_int countNumCalls{};
			//static std::atomic_int countNumMerges{};
			//
			//countNumCalls.fetch_add(1, std::memory_order_relaxed);

			if (A.max() <= B.min())
				return { A, B };
			if (B.max() <= A.min())
				return { B, A };

			// 1.5M in one turn!
			//countNumMerges.fetch_add(1, std::memory_order_relaxed);

			if constexpr (kUseScalarImplementation)
			{
				std::array<std::tuple<Key, Value>, kVectorWidth * 2> output{};
				std::ranges::merge(
					std::views::zip(A.keys.toArray(), A.values.toArray()),
					std::views::zip(B.keys.toArray(), B.values.toArray()),
					output.begin(),
					std::less<>(),
					[](std::tuple<Key, Value> kv) { return std::get<0>(kv); },
					[](std::tuple<Key, Value> kv) { return std::get<0>(kv); }
				);
				std::array<Key, kVectorWidth * 2> outputKeys;
				std::array<Value, kVectorWidth * 2> outputValues;
				std::ranges::copy(output | std::views::keys, outputKeys.begin());
				std::ranges::copy(output | std::views::values, outputValues.begin());
				return {
					Node{ KeyVector(std::span(outputKeys).template subspan<0, kVectorWidth>()), ValueVector(std::span(outputValues).template subspan<0, kVectorWidth>()) },
					Node{ KeyVector(std::span(outputKeys).template subspan<kVectorWidth>()), ValueVector(std::span(outputValues).template subspan<kVectorWidth>()) },
				};
			}
			else
			{
				using WideKeyVector = simd::Vector<Key, kVectorWidth * 2>;
				using WideValueVector = simd::Vector<Value, kVectorWidth * 2>;
				using WideIndexVector = WideKeyVector::DefaultIndexVector;
				const auto [keys, indices] = mergeBitonic<Key, kVectorWidth * 2>({
					.keys = WideKeyVector(A.keys, B.keys),
					.indices = WideIndexVector(simd::iotaArray<typename WideIndexVector::value_type, WideIndexVector::kNumElements>()),
					});

				const auto values = WideValueVector(A.values, B.values).permute(indices);
				const auto [keysLo, keysHi] = keys.split();
				const auto [valuesLo, valuesHi] = values.split();

				assert(std::ranges::is_sorted(keys.toArray()));
				assert(std::ranges::is_sorted(keysLo.toArray()));
				assert(std::ranges::is_sorted(keysHi.toArray()));

				return {
					Node{.keys = keysLo, .values = valuesLo },
					Node{.keys = keysHi, .values = valuesHi },
				};
			}
		}
	
	private:
		using IndexVector = KeyVector;

		std::vector<Node> mNodes;

		static size_t calcParentIndex(size_t childI)
		{
			return (childI - 1) / kDegree;
		}

		static size_t calcFirstChildIndex(size_t parentI)
		{
			return parentI * kDegree + 1;
		}

		static void bubbleUp(std::span<Node> localNodes, size_t i)
		{
			while (i > 0)
			{
				const size_t parentI = calcParentIndex(i);
				Node& activeNode = localNodes[i];
				Node& parentNode = localNodes[parentI];
				if (activeNode.min() < parentNode.max())
				{
					std::tie(parentNode, activeNode) = mergeNodes(parentNode, activeNode);
					i = parentI;
				}
				else
					break;
			}
		}

		static void bubbleDown(std::span<Node> localNodes, size_t parentI)
		{
			// firstChildIndex(lastCompleteNodeI) + k <= localNodes.size()
			// lastCompleteNodeI * k + 1 + k <= localNodes.size()
			// lastCompleteNodeI * k <= localNodes.size() - 1 - k
			// lastCompleteNodeI <= (localNodes.size() - 1 - k) / k
			// lastCompleteNodeI = (localNodes.size() - 1) / k - 1
			// firstPartialNodeI = (localNodes.size() - 1) / k
			const size_t firstPartialNodeI = (localNodes.size() - 1) / kDegree;

			while (parentI < firstPartialNodeI)
			{
				const size_t childI = calcFirstChildIndex(parentI);
				//const Key cmp0K = std::min(localNodes[childI + 1].key, localNodes[childI].key);
				//const Key cmp1K = std::min(localNodes[childI + 3].key, localNodes[childI + 2].key);

				Node& parentNode = localNodes[parentI];

				const bool hasAnyOverlap = staticReduce<kDegree>(childI,
					std::logical_or(),
					[localNodes, limit = parentNode.max()](size_t i) { return localNodes[i].min() < limit; }
				);

				if (hasAnyOverlap)
				{
					// Parent overlaps with one of the children.

					const size_t lowestMaxI = staticReduce<kDegree>(childI,
						[localNodes](size_t a, size_t b) { return localNodes[a].max() <= localNodes[b].max() ? a : b; },
						[](size_t i) { return i; }
					);

					// Swap with lowest max.
					std::swap(parentNode, localNodes[lowestMaxI]);

					// Merge with all children in sequence such that we end up with new parent max <= children min.
					for (const size_t i : range(kDegree))
						std::tie(parentNode, localNodes[childI + i]) = mergeNodes(parentNode, localNodes[childI + i]);

					// Importantly, only the child at lowestMaxI may overlap its children. The max of other children should not have increased.

					parentI = lowestMaxI;
				}
				else
					return;
			}

			// And the last node which won't have all children.
			const size_t childI = calcFirstChildIndex(parentI);
			if (childI < localNodes.size())
			{
				const std::span childNodes = localNodes.subspan(childI);
				assert(childNodes.size() < kDegree);

				Node& parentNode = localNodes[parentI];

				const Key lowestMinK = std::ranges::min(childNodes, std::less<>(), &Node::min).min();

				if (parentNode.max() > lowestMinK)
				{
					// Parent overlaps with one of the children.

					int lowestMaxI = 0;
					Key lowestMaxK = childNodes[0].max();

					for (const int i : range(1, (int)childNodes.size()))
					{
						if (childNodes[i].max() < lowestMaxK)
						{
							lowestMaxK = childNodes[i].max();
							lowestMaxI = i;
						}
					}

					// Swap with lowest max.
					std::swap(parentNode, childNodes[lowestMaxI]);

					// Merge with all children in sequence such that we end up with new parent max <= children min.
					for (const int i : range((int)childNodes.size()))
						std::tie(parentNode, childNodes[i]) = mergeNodes(parentNode, childNodes[i]);

					// Importantly, only the child at lowestMaxI may overlap its children. The max of other children should not have increased.
				}
			}
		}

		// Sort the given node by key.
		static Node HECK_VECTORCALL sortNode(Node node)
		{
			if constexpr (kUseScalarImplementation)
			{
				typename KeyVector::Array keys = node.keys.toArray();
				typename ValueVector::Array values = node.values.toArray();
				
				// Non-deterministic!
				std::ranges::sort(std::views::zip(keys, values), std::less<>(), [](std::tuple<Key, Value> kv) { return std::get<0>(kv); });
				
				return {
					KeyVector(keys),
					ValueVector(values)
				};
			}
			else
			{
				const auto [sortedKeys, indices] = computeSimdSortedPermutation(node.keys);
				node.keys = sortedKeys;
				node.values = node.values.permute(indices);

				assert(std::ranges::is_sorted(sortedKeys.toArray()));

				return node;
			}
		}
	};
}