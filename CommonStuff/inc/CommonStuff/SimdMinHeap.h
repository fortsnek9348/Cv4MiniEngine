#pragma once

#include <vector>

namespace heck
{
	template<class KeyVector, class ValueVector>
	class SimdMinHeap
	{
	public:
		static_assert(KeyVector::kNumElements == ValueVector::kNumElements);

		static const size_t kElementsPerVector = KeyVector::kNumElements;

		bool empty() const
		{
			return mEntries.empty();
		}

		void reserve(size_t n)
		{
			mEntries.reserve(n / kElementsPerVector + 1);
		}

		void push(KeyVector keys, ValueVector values)
		{
			mEntries.emplace_back(keys, values);

			size_t childI = mEntries.size() - 1;

			const std::span<Entry> entries = mEntries;

			while (childI > 0)
			{
				const size_t parentI = (childI - 1) / 2;
				const Entry oldParent = entries[parentI];
				const Entry oldChild = entries[childI];
				const auto mask = vcmplt(oldChild.keys, oldParent.keys);
				if (!mask.asBits())
					break;

				entries[parentI] = {
					.keys = vselect(mask, oldChild.keys, oldParent.keys),
					.values = vselect(mask, oldChild.values, oldParent.values),
				};

				entries[childI] = {
					.keys = vselect(mask, oldParent.keys, oldChild.keys),
					.values = vselect(mask, oldParent.values, oldChild.values),
				};

				childI = parentI;
			}
		}
		
		ValueVector pop()
		{
			const ValueVector values = mEntries.front().values;
			mEntries.front() = mEntries.back();
			mEntries.pop_back();

			const std::span<Entry> entries = mEntries;

			size_t parentI = 0;


		}

	private:
		struct Entry
		{
			KeyVector keys{};
			ValueVector values{};
		};

		std::vector<Entry> mEntries;
	};
}