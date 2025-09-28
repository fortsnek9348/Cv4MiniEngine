#include "inc/CommonStuff/StatisticsRegistry.h"
#include "inc/CommonStuff/ParallelExt.h"

using namespace heck;

void CountingStatistic::toString(std::ostream& os) const
{
	os << mCount.load(std::memory_order_relaxed);
}

void TimingStatistic::toString(std::ostream& os) const
{
	os << mCount.load(std::memory_order_relaxed)
		<< ", acc=" << std::chrono::duration<double>(Clock::duration(mAccDuration.load(std::memory_order_relaxed)))
		<< ", max=" << std::chrono::duration<double>(Clock::duration(mMaxDuration.load(std::memory_order_relaxed)));
}

void TimingStatistic::addSample(Clock::duration d) noexcept
{
	mCount.fetch_add(1, std::memory_order_relaxed);
	mAccDuration.fetch_add(d.count(), std::memory_order_relaxed);
	heck::atomicMaxRelaxed(mMaxDuration, d.count());
}

std::string StatisticsRegistry::buildAllStatsString() const
{
	const std::scoped_lock lock(mMutex);
	std::ostringstream ss;
	for (const auto& [name, stat] : mStats)
	{
		ss << name << ": ";
		stat->toString(ss);
		ss << '\n';
	}
	return std::move(ss).str();
}