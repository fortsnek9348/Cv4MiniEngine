#pragma once

#include "Common.h"

#include <vector>

namespace mybot
{
	// Given a cost matrix between N tasks and M workers, produce an optimal assignment of tasks to workers.
	// There can be more tasks than workers, or more workers than tasks.
	// This function operates in O(N^2 * M) if N <= M.
	// Costs are addressed as [task][worker]. Ensure min(tasks, workers) * cost <= INT_MAX
	// Returns an array of task indices for workers.
	std::vector<int> computeOptimalAssignment(Span2D<const int> costs);
}