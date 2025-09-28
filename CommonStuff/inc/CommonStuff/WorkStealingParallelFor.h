#pragma once

#include <functional>
#include <utility>

namespace heck
{
	// Parallel unit updates could do with some work stealing.
	// Some units take much longer than others to update, so a "static partitioning" isn't really suitable.
	// Here, we'll start with a static partitioning, and do work stealing when a thread completes.
	// Also, the thread index is passed to the caller.

	size_t getParallelWorkStealingNumThreads();
	void parallelWorkStealingForEachN(size_t n, const std::function<void(size_t threadI, size_t taskI)>& f, bool forceSerial = false);
}