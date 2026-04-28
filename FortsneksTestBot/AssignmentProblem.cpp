#include "AssignmentProblem.h"
#include "DynamicArray2D.h"

#include <algorithm>
#include <limits>

// Brush up on your graph theory...
// This problem is represented as a graph with N vertices on the left and M vertices on the right, with edges connecting all combinations between the two sides.
// So, it's symmetric. Which side is tasks or workers doesn't matter. Which means it's determined by cardinality of the sides.
//
// https://en.wikipedia.org/wiki/Hungarian_method#The_algorithm_in_O(n3)_time
// https://cp-algorithms.com/graph/hungarian-algorithm.html#the-mathcalon3-algorithm

namespace
{
	// @tparam T a type large enough to represent integers on the order of J * max(|C|)
	

	// Canonical implementation where J <= W.
	std::vector<int> computeOptimalAssignmentTasksToWorkers(cvbot::Span2D<const int> costs)
	{
		// Implementation from wikipedia, edited, and for maximising cost.
		using AccCost = int;

		const int J = costs.dim.y;
		const int W = costs.dim.x;
		assert(J <= W);
		// job[w] = job assigned to w-th worker, or -1 if no job assigned
		// note: a W-th worker was added for convenience
		std::vector<int> job(W + 1, -1);
		std::vector<AccCost> ys(J);
		std::vector<AccCost> yt(W + 1);  // potentials
		// -yt[W] will equal the sum of all deltas
		constexpr AccCost ninf = std::numeric_limits<AccCost>::min();

		// Max reduced cost over edges from Z to worker w
		std::vector<AccCost> maxTo(W + 1, ninf);
		std::vector<int> prev(W + 1, -1);  // previous worker on alternating path
		std::vector<bool> inZ(W + 1);     // whether worker is in Z

		for (int jCur = 0; jCur < J; ++jCur) // assign jCur-th job
		{
			int wCur = W;
			job[wCur] = jCur;

			if (jCur > 0)
			{
				std::ranges::fill(maxTo, ninf);
				std::ranges::fill(prev, -1);
				inZ.clear();
				inZ.resize(W + 1);
			}

			while (job[wCur] != -1) // runs at most jCur + 1 times
			{
				inZ[wCur] = true;
				const int j = job[wCur];
				AccCost delta = ninf;
				int wNext{};

				for (int w = 0; w < W; ++w)
				{
					if (!inZ[w])
					{
						const AccCost deltaW = costs[{ w, j }] + ys[j] + yt[w];
						if (deltaW > maxTo[w])
						{
							maxTo[w] = deltaW;
							prev[w] = wCur;
						}
						if (maxTo[w] > delta)
						{
							delta = maxTo[w];
							wNext = w;
						}
					}
				}

				for (int w = 0; w <= W; ++w)
				{
					if (inZ[w])
					{
						ys[job[w]] -= delta;
						yt[w] += delta;
					}
					else
					{
						maxTo[w] -= delta;
					}
				}
				wCur = wNext;
			}

			// update assignments along alternating path
			while (wCur != W)
			{
				const int w = prev[wCur];
				job[wCur] = job[w];
				wCur = w;
			}
		}

		job.pop_back();
		assert(static_cast<ptrdiff_t>(job.size()) - std::ranges::count(job, -1) == static_cast<ptrdiff_t>(std::min(J, W)));
		return job;
	}

	void sanityCheckHungarian()
	{
		mybot::DynamicArray2D<int> costs({ 3, 3 });
		costs.cells = {
			// => worker
			-8, -5, -9,
			-4, -2, -4,
			-7, -3, -8,
		};
		const std::vector<int> tasks = computeOptimalAssignmentTasksToWorkers(costs.view());

		int accCost = 0;
		for (int workerI = 0; workerI < 3; ++workerI)
			accCost += costs[{ workerI, tasks[workerI] }];

		if (accCost != -15)
			throw mybot::BotFailure("computeOptimalAssignment is broken.");
	}

	const bool kTest = (sanityCheckHungarian(), false);
}

std::vector<int> mybot::computeOptimalAssignment(Span2D<const int> costs)
{
	if (costs.dim.y <= costs.dim.x)
		return computeOptimalAssignmentTasksToWorkers(costs);
	else
	{
		// W < J. Flip the problem.
		DynamicArray2D<int> costsT(ivec2{ costs.dim.y, costs.dim.x });
		for (int x = 0; x < costs.dim.x; ++x)
			for (int y = 0; y < costs.dim.y; ++y)
				costsT[{ y, x }] = costs[{ x, y }];
		// worker = [task]
		const std::vector<int> workerIndicesForTasks = computeOptimalAssignmentTasksToWorkers(costsT.view());
		// task = [worker]
		std::vector<int> taskIndicesForWorkers(costs.dim.y, -1);
		for (int taskI = 0; taskI < costs.dim.y; ++taskI)
			if (workerIndicesForTasks[taskI] >= 0)
				taskIndicesForWorkers[workerIndicesForTasks[taskI]] = taskI;
		assert(taskIndicesForWorkers.size() - std::ranges::count(taskIndicesForWorkers, -1) == static_cast<size_t>(costs.dim.x));
		return taskIndicesForWorkers;
	}
}
