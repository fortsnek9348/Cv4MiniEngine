#include "Pathing.h"

#include <CommonStuff/UnionFind.h>

#include <ranges>

using namespace mybot;

// CvGameCoreUtils
static unsigned int coordDistance(int iFrom, int iTo, int iRange, bool bWrap)
{
	if (bWrap && std::abs(iFrom - iTo) > iRange / 2)
		return iRange - std::abs(iFrom - iTo);
	else
		return std::abs(iFrom - iTo);
}

unsigned int mybot::computeDistance(const MapGeometry& geom, ivec2 a, ivec2 b, EDistanceMetric metric) noexcept
{
	const unsigned int dx = coordDistance(a.x, b.x, geom.dim.x, geom.isWrapX);
	const unsigned int dy = coordDistance(a.y, b.y, geom.dim.y, geom.isWrapY);
	unsigned int d = std::max(dx, dy);
	if (metric != EDistanceMetric::Step)
		d += std::min(dx, dy) / 2;
	return d;
}

DynamicArray2D<MultipleSourceDistanceFieldCell> mybot::computeMultipleSourcePathLengthField(PathingMap map, PathingPlot::EFlag avoidFlags, int maxDistance, std::span<const ivec2> starts)
{
	if (!starts.empty() && starts.size() - 1 > MultipleSourceDistanceFieldCell{ .index = UINT16_MAX }.index)
		throw cvbot::BotFailure(std::string("Too many sources in ") + __func__);

	// NOTE: Ordinarly, you'd use a priority queue for an A* implementation, but in reality, you only need enough buckets to cover the max edge weight,
	//       which in this case, is 1 for step distance and 2 for cultural distance.
	//       At each iteration, each bucket represents a specific distance value.
	static constexpr size_t kNumBuckets = 2 + 1;

	unsigned int frontierDistance = 0;

	struct alignas(uint64_t) Entry
	{
		i16vec2 coord{};
		int index{};
	};

	std::array<std::vector<Entry>, kNumBuckets> buckets;
	DynamicArray2D<MultipleSourceDistanceFieldCell> field(map.geom.dim);

	for (const auto [index, start] : starts | std::views::enumerate)
	{
		field[start] = { .distance = 0, .index = static_cast<uint16_t>(index) };
		buckets[0].push_back({ static_cast<i16vec2>(start), static_cast<int>(index) });
	}

	while (!std::ranges::all_of(buckets, std::ranges::empty))
	{
		auto& frontier = buckets.front();
		while (frontier.empty())
		{
			++frontierDistance;
			std::ranges::rotate(buckets, buckets.begin() + 1);
		}
		
		const auto [coord, index] = frontier.back();
		frontier.pop_back();

		const ivec2 source = starts[index];
		const unsigned int srcPathLength = field[coord].distance;

		// This can be accelerated by SIMD if necessary.
		for (int dy = -1; dy <= 1; ++dy)
		{
			const int y = coord.y + dy;
			const int wrappedY = map.geom.wrapY(y);

			if (unsigned(wrappedY) >= unsigned(map.geom.dim.y))
				continue;

			for (int dx = -1; dx <= 1; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;

				const int x = coord.x + dx;
				const int wrappedX = map.geom.wrapX(x);

				if (unsigned(wrappedX) >= unsigned(map.geom.dim.x))
					continue;

				const ivec2 wrappedCoord{ wrappedX, wrappedY };

				if ((map.plots[wrappedCoord.y, wrappedCoord.x].flags & avoidFlags) != 0)
					continue;

				const unsigned int dstPathLength = srcPathLength + 1;

				if (static_cast<int>(dstPathLength) > maxDistance)
					continue;

				MultipleSourceDistanceFieldCell& destCell = field[wrappedCoord];

				if (dstPathLength < destCell.distance)
				{
					buckets[dstPathLength - frontierDistance].push_back({ static_cast<i16vec2>(wrappedCoord), static_cast<int>(index) });
					destCell.distance = static_cast<uint16_t>(dstPathLength);
					destCell.index = static_cast<uint16_t>(index);
				}
			}
		}
	}

	return field;
}

DynamicArray2D<MultipleSourceDistanceFieldCell> mybot::computeMultipleSourceDistanceField(PathingMap map, EDistanceMetric metric, std::span<const ivec2> starts)
{
	if (!starts.empty() && starts.size() - 1 > MultipleSourceDistanceFieldCell{ .index = UINT16_MAX }.index)
		throw cvbot::BotFailure(std::string("Too many sources in ") + __func__);

	// NOTE: Ordinarly, you'd use a priority queue for an A* implementation, but in reality, you only need enough buckets to cover the max edge weight,
	//       which in this case, is 1 for step distance and 2 for cultural distance.
	//       At each iteration, each bucket represents a specific distance value.
	static constexpr size_t kNumBuckets = 2 + 1;

	unsigned int frontierDistance = 0;

	struct alignas(uint64_t) Entry
	{
		i16vec2 coord{};
		int index{};
	};

	std::array<std::vector<Entry>, kNumBuckets> buckets;
	DynamicArray2D<MultipleSourceDistanceFieldCell> field(map.geom.dim);

	for (const auto [index, start] : starts | std::views::enumerate)
	{
		field[start] = { .distance = 0, .index = static_cast<uint16_t>(index) };
		buckets[0].push_back({ static_cast<i16vec2>(start), static_cast<int>(index) });
	}

	while (!std::ranges::all_of(buckets, std::ranges::empty))
	{
		auto& frontier = buckets.front();
		while (frontier.empty())
		{
			++frontierDistance;
			std::ranges::rotate(buckets, buckets.begin() + 1);
		}

		const auto [coord, index] = frontier.back();
		frontier.pop_back();

		const ivec2 source = starts[index];

		// This can be accelerated by SIMD if necessary.
		for (int dy = -1; dy <= 1; ++dy)
		{
			const int y = coord.y + dy;
			const int wrappedY = map.geom.wrapY(y);

			if (unsigned(wrappedY) >= unsigned(map.geom.dim.y))
				continue;

			for (int dx = -1; dx <= 1; ++dx)
			{
				if (dx == 0 && dy == 0)
					continue;

				const int x = coord.x + dx;
				const int wrappedX = map.geom.wrapX(x);

				if (unsigned(wrappedX) >= unsigned(map.geom.dim.x))
					continue;

				const ivec2 wrappedCoord{ wrappedX, wrappedY };

				const unsigned int distance = computeDistance(map.geom, source, wrappedCoord, metric);

				MultipleSourceDistanceFieldCell& destCell = field[wrappedCoord];

				if (distance < destCell.distance)
				{
					buckets[distance - frontierDistance].push_back({ static_cast<i16vec2>(wrappedCoord), static_cast<int>(index) });
					destCell.distance = static_cast<uint16_t>(distance);
					destCell.index = static_cast<uint16_t>(index);
				}
			}
		}
	}

	return field;
}

DynamicArray2D<unsigned int> mybot::computeGlobalReachability(PathingMap map, PathingPlot::EFlag avoidFlags)
{
	DynamicArray2D<unsigned int> field(map.geom.dim);
	for (size_t i = 0, n = field.cells.size(); i < n; ++i)
		field.cells[i] = static_cast<unsigned int>(i);

	heck::ArrayUnionFind<decltype(field.cells)> uf(field.cells);

	for (int y = 0; y < map.geom.dim.y; ++y)
	{
		for (int x = 0; x < map.geom.dim.x; ++x)
		{
			if ((map.plots[y, x].flags & avoidFlags) != 0)
			{
				field[{ x, y }] = 0;
				continue;
			}

			const unsigned int indexA = x + y * map.geom.dim.x;

			for (int dy = -1; dy <= 1; ++dy)
			{
				const int y2 = y + dy;
				const int wrappedY = map.geom.wrapY(y2);

				for (int dx = -1; dx <= 1; ++dx)
				{
					const int x2 = x + dx;
					const int wrappedX = map.geom.wrapX(x2);

					if ((map.plots[wrappedY, wrappedX].flags & avoidFlags) != 0)
						continue;

					uf.unionise(indexA, wrappedX + wrappedY * map.geom.dim.x);
				}
			}
		}
	}

	// Flatten.
	for (size_t i = 0, n = field.cells.size(); i < n; ++i)
		if (field.cells[i])
			field.cells[i] = uf.find(static_cast<unsigned int>(i));

	return field;
}