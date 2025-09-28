#include "inc/CommonStuff/WorkStealingParallelFor.h"
#include "inc/CommonStuff/ParallelExt.h"
#include "inc/CommonStuff/range.h"

#include <thread>

namespace heck
{
	size_t getParallelWorkStealingNumThreads()
	{
		return std::max(1u, std::thread::hardware_concurrency());
	}

	void parallelWorkStealingForEachN(size_t n, const std::function<void(size_t, size_t)>& f, bool forceSerial)
	{
		if (n <= 0)
			return;

		if (forceSerial)
		{
			for (const size_t i : range(n))
				f(0, i);
			return;
		}

		const size_t numThreads = getParallelWorkStealingNumThreads();

		// TODO: Alignment will reduce false sharing, but is it worthwhile?
		struct /*alignas(std::hardware_destructive_interference_size)*/ WorkQueue
		{
			std::atomic<size_t> index{};
			size_t end{};

			std::optional<size_t> tryPop()
			{
				const size_t mine = index.fetch_add(1, std::memory_order_relaxed);

				//size_t mine = index.load(std::memory_order_relaxed);
				//while (mine < end && !index.compare_exchange_weak(mine, mine + 1))
				//	;

				if (mine < end)
					return mine;
				else
					return std::nullopt;
			}
		};

		// This could be implemented as a singular queue, but all the threads competing over a single index variable might not be good.
		std::vector<WorkQueue> queues(numThreads);

		const size_t tasksPerThread = n / numThreads;
		const size_t leftoverTasks = n % numThreads;

		size_t taskI = 0;
		for (size_t threadI = 0; threadI < numThreads; ++threadI)
		{
			queues[threadI].index.store(std::min(taskI, n), std::memory_order_relaxed);
			taskI += tasksPerThread + (threadI < leftoverTasks);
			queues[threadI].end = std::min(taskI, n);
		}
		

		parallelForEachN(numThreads, [&queues, numThreads, f](size_t threadI) {
			size_t stealI = threadI;
			for (;;)
			{
				std::optional<size_t> taskI;
				for (size_t i = 0; i < numThreads; ++i)
				{
					taskI = queues[stealI].tryPop();
					if (taskI)
						break;
					
					++stealI;
					if (stealI >= numThreads)
						stealI = 0;
				}
				if (!taskI)
					break;

				f(threadI, *taskI);
			}
			});
	}
}