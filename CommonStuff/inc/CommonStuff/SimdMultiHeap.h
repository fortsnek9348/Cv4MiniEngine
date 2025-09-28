#pragma once

#include "Simd.h"

namespace heck
{
	// Dumb-simple SIMD extension of the typical binary heap.
	// Have multiple heaps in parallel and use gather/scatter on them.
	// This means that pops won't be in strict order, but we'll assume A* gets things in reasonable order.
	// kNumRoots is the SIMD width.
	template<class Key, class Value, size_t kNumRoots, size_t kDegree>
	class SimdMultiHeap
	{
	public:
		struct Node
		{
			Key key;
			Value value;
		};

		std::vector<Node> nodes;

		void clear()
		{
			nodes.clear();
		}

		bool empty() const
		{
			return nodes.empty();
		}

		size_t size() const
		{
			return nodes.size();
		}


		Key peek() const
		{
			const Key result = std::ranges::min(std::span(nodes).subspan(0, std::min(nodes.size(), kNumRoots)), std::less<>(), &Node::key).key;
			//assert(result == std::ranges::min(nodes, std::less<>(), &Node::key).key);
			return result;
		}

		std::span<Node> elements()
		{
			return nodes;
		}

		void reheapify()
		{
			const std::span localNodes = nodes;
			for (const size_t i : heck::range(kNumRoots, localNodes.size()))
				bubbleUp(localNodes, i, localNodes[i]);

			//checkHeapProperty();
		}

		void push(Key key, Value value)
		{
			//checkHeapProperty();

			size_t i = nodes.size();

			const Node activeNode{ key, value };
			nodes.push_back(activeNode);

			bubbleUp(nodes, i, activeNode);

			//checkHeapProperty();
		}

		using KeyVector = simd::Vector<Key, kNumRoots>;
		using ValueVector = simd::Vector<Value, kNumRoots>;
		using IndexVector = simd::Vector<int32_t, kNumRoots>;
		using Mask = KeyVector::Mask;

		struct PopResult
		{
			KeyVector keys;
			ValueVector values;
			Mask mask;
		};

		PopResult pop()
		{
			//checkHeapProperty();

			const auto rootIndices = vmin(IndexVector(simd::iotaArray<int32_t, kNumRoots>()), int32_t(nodes.size() - 1));
			const PopResult result{
				gatherKeys(nodes, rootIndices),
				gatherValues(nodes, rootIndices),
				Mask(makeLowNBitsSet<kNumRoots>(std::min(kNumRoots, nodes.size()))),
			};

			if (nodes.size() >= kNumRoots * 2) [[likely]]
			{
				std::ranges::copy(std::span(nodes).subspan(nodes.size() - kNumRoots), nodes.begin());
				nodes.erase(nodes.end() - kNumRoots, nodes.end());
				minHeapify(nodes);
			}
			else if (nodes.size() > kNumRoots)
			{
				nodes.erase(nodes.begin(), nodes.begin() + kNumRoots);
				// Only root nodes remain.
			}
			else
				nodes.clear();

			//checkHeapProperty();

			return result;
		}

		void checkHeapProperty() const
		{
			for (size_t i = kNumRoots; i < nodes.size(); ++i)
				if (nodes[i].key < nodes[parentIndex(i)].key)
					std::abort();
		}

	private:
		//  0 1 2 3   4  5    6  7    8  9    10 11
		// [R R R R] [C0 C0] [C1 C2] [C2 C2] [C3 C3] ...

		static size_t parentIndex(size_t childI)
		{
			return (childI - kNumRoots) / kDegree;
		}

		static size_t firstChildIndex(size_t parentI)
		{
			return parentI * kDegree + kNumRoots;
		}

		static void bubbleUp(std::span<Node> localNodes, size_t i, Node activeNode)
		{
			// Bubble up.
			while (i >= kNumRoots)
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

		
		

		static IndexVector firstChildIndex(IndexVector parentI)
		{
			if constexpr (std::has_single_bit(kDegree))
				return (parentI << std::integral_constant<size_t, std::countr_zero(kDegree)>()) + kNumRoots; // Can't trust the optimiser...
			else
				return parentI * kDegree + kNumRoots;
		}

		static KeyVector gatherKeys(std::span<const Node> localNodes, IndexVector nodeIndices)
		{
			//std::array<Key, kNumRoots> v{};
			//for (const size_t i : range(kNumRoots))
			//	v[i] = localNodes[nodeIndices.getElement(i)].key;
			//return KeyVector(v);
			return KeyVector(std::as_bytes(localNodes), nodeIndices, std::integral_constant<size_t, sizeof(Node)>());
		}

		static ValueVector gatherValues(std::span<const Node> localNodes, IndexVector nodeIndices)
		{
			//std::array<Value, kNumRoots> v{};
			//for (const size_t i : range(kNumRoots))
			//	v[i] = localNodes[nodeIndices.getElement(i)].value;
			//return ValueVector(v);
			return ValueVector(std::as_bytes(localNodes).subspan(offsetof(Node, value)), nodeIndices, std::integral_constant<size_t, sizeof(Node)>());
		}

		static void scatterNodes(std::span<Node> localNodes, IndexVector nodeIndices, KeyVector keys, ValueVector values, Mask mask)
		{
			scatter(std::as_writable_bytes(localNodes), nodeIndices, keys, mask, std::integral_constant<size_t, sizeof(Node)>());
			scatter(std::as_writable_bytes(localNodes).subspan(offsetof(Node, value)), nodeIndices, values, mask, std::integral_constant<size_t, sizeof(Node)>());
			//for (const size_t i : range(kNumRoots))
			//	if ((mask.asBits() >> i) & 1)
			//	{
			//		localNodes[nodeIndices.getElement(i)].key = keys.getElement(i);
			//		localNodes[nodeIndices.getElement(i)].value = values.getElement(i);
			//	}
		}

		static void minHeapify(std::span<Node> localNodes)
		{
			//if (localNodes.size() <= kNumRoots)
			//	return;
			assert(localNodes.size() >= kNumRoots);

			// firstChildIndex(lastCompleteNodeI + kNumRoots - 1) + k <= localNodes.size()
			// (lastCompleteNodeI + kNumRoots - 1) * k + kNumRoots + k <= localNodes.size()
			// (lastCompleteNodeI + kNumRoots - 1) * k <= localNodes.size() - kNumRoots - k
			// (lastCompleteNodeI + kNumRoots - 1) <= (localNodes.size() - kNumRoots - k) / k
			// (lastCompleteNodeI + kNumRoots - 1) = (localNodes.size() - kNumRoots) / k - 1
			// lastCompleteNodeI = (localNodes.size() - kNumRoots) / k - kNumRoots
			// firstPartialNodeI = (localNodes.size() - kNumRoots) / k - kNumRoots + 1
			[[maybe_unused]] const size_t firstPartialNodeI = parentIndex(localNodes.size()) - kNumRoots + 1;

			IndexVector parentIndices(simd::iotaArray<int32_t, kNumRoots>());
			typename KeyVector::Mask activeMask = KeyVector::Mask::kAll;
			//parentIndices = vselect(activeMask, parentIndices, 0); // Sanitise for unmasked gather

			typename IndexVector::Mask partialMask{};

			const KeyVector activeKeys = gatherKeys(localNodes, parentIndices);
			const ValueVector activeValues = gatherValues(localNodes, parentIndices);

			//static_assert(k == 4);

			//const auto doBubbleDownIteration = [&] {
			//	
			//	};

			//while ((vcmplt(parentIndices, (int)firstPartialNodeI) | ~activeMask).all() && activeMask.any())
			while (activeMask.any())
			{
				// In case this is called with some incomplete nodes, clamp the child indices for the unmasked gather.
				IndexVector firstChildIndices = firstChildIndex(parentIndices);

				const typename IndexVector::Mask completeParents = activeMask & vcmple(firstChildIndices + kDegree, (int)localNodes.size());
				partialMask |= activeMask & ~completeParents;
				activeMask = completeParents;

				firstChildIndices = vselect(activeMask, firstChildIndices, 0);

				std::array<KeyVector, kDegree> childKeys;
				for (int i = 0; i < (int)kDegree; ++i)
					childKeys[i] = gatherKeys(localNodes, firstChildIndices + i);

				const KeyVector lowestKeys = staticReduce<kDegree>(0,
					[](KeyVector a, KeyVector b) { return vmin(a, b); },
					[childKeys](size_t i) { return childKeys[i]; }
				);

				const typename KeyVector::Mask bubbleDownMask = vcmplt(lowestKeys, activeKeys) & activeMask;

				if (bubbleDownMask.any())
				{
					const KeyVector lowestIndices = staticReduce<kDegree>(0,
						[](std::pair<KeyVector, IndexVector> a, std::pair<KeyVector, IndexVector> b) {
							return std::pair(vmin(b.first, a.first), vselect(vcmplt(b.first, a.first), b.second, a.second));
						},
						[firstChildIndices, childKeys](size_t i) { return std::pair(childKeys[i], firstChildIndices + (int)i); }
					).second;

					// localNodes[parentI] = localNodes[lowestI];
					// parentI = lowestI;

					const ValueVector lowestValues = gatherValues(localNodes, lowestIndices);
					scatterNodes(localNodes, parentIndices, lowestKeys, lowestValues, bubbleDownMask);
					parentIndices = vselect(bubbleDownMask, lowestIndices, parentIndices);
					activeMask &= bubbleDownMask;
				}
				else
				{
					//scatterNodes(localNodes, parentIndices, activeKeys, activeValues, activeMask);
					//activeMask = {};
					break;
				}
			}

			scatterNodes(localNodes, parentIndices, activeKeys, activeValues, KeyVector::Mask::kAll);

			// Last iteration. We'll probably have some incomplete parent nodes.
			// Find out which nodes have all children.
			//const typename KeyVector::Mask lastCompleteParentsMask = activeMask & vcmple(firstChildIndex(parentIndices) + kDegree, (int)localNodes.size());
			//typename KeyVector::Mask partialParentsMask = activeMask & ~lastCompleteParentsMask;
			//activeMask = lastCompleteParentsMask;
			//if (doBubbleDownIteration()) // Don't return yet, might have a partial parent left
			//	partialParentsMask |= activeMask;
			
				
			// Now there's just partial parents left. Probably just one.
			// Do each of them using a scalar loop.

			//activeMask = partialParentsMask;
			partialMask.forEachBit([&](size_t rootI) {
				size_t parentI = parentIndices.getElement(rootI);
				const Node activeNode{
					activeKeys.getElement(rootI),
					activeValues.getElement(rootI),
				};

				while (parentI < localNodes.size())
				{
					const size_t childI = firstChildIndex(parentI);
					if (childI < localNodes.size())
					{
						const std::span childNodes = localNodes.subspan(childI, std::min(kDegree, localNodes.size() - childI));
						const auto minIt = std::ranges::min_element(childNodes, std::less<>(), &Node::key);
						if (activeNode.key > minIt->key)
						{
							localNodes[parentI] = *minIt;
							//*minIt = activeNode;
							//continue;
							parentI = childI + (minIt - childNodes.begin());
						}
						else
							break;
					}
					else
						break;
				}

				localNodes[parentI] = activeNode;
				});
		}
	};
}