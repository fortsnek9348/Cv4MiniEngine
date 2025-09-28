#pragma once

#include "NoCopy.h"
#include "CompilerMacros.h"

#include <atomic>
#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <mutex>

namespace heck
{
	class IStatistic
	{
	public:
		virtual void toString(std::ostream& os) const = 0;
		virtual ~IStatistic() = default;
	};

	class CountingStatistic : public IStatistic
	{
	public:
		// Thread-safe.
		HECK_FORCEINLINE void inc()
		{
			mCount.fetch_add(1, std::memory_order_relaxed);
		}

		virtual void toString(std::ostream& os) const override;

	private:
		std::atomic<uint64_t> mCount{};
	};

	class TimingStatistic : public IStatistic
	{
		using Clock = std::chrono::steady_clock;

	public:
		struct [[nodiscard]] Scope : heck::NoCopyNoMove
		{
			TimingStatistic& stat;
			const Clock::time_point start = Clock::now();

			HECK_FORCEINLINE explicit Scope(TimingStatistic& stat) : stat(stat)
			{
			}

			HECK_FORCEINLINE ~Scope() noexcept
			{
				stat.addSample(Clock::now() - start);
			}
		};

		// Thread-safe.
		HECK_FORCEINLINE Scope scope()
		{
			return Scope(*this);
		}

		// Thread-safe.
		void addSample(Clock::duration) noexcept;

		virtual void toString(std::ostream& os) const override;

	private:
		std::atomic<uint64_t> mCount{};
		std::atomic<Clock::duration::rep> mAccDuration{};
		std::atomic<Clock::duration::rep> mMaxDuration{};

		
	};

	class StatisticsRegistry
	{
	public:
		// Yeah, I'll let this be a global.
		static StatisticsRegistry& getInstance()
		{
			static StatisticsRegistry g;
			return g;
		}

		// Thread-safe.
		template<std::derived_from<IStatistic> T>
		[[nodiscard]] T& grab(std::string name)
		{
			const std::scoped_lock lock(mMutex);
			return static_cast<T&>(*mStats.emplace(std::move(name), std::make_unique<T>()).first->second);
		}

		// Thread-safe, but stat values may be inconsistent with concurrent modifications.
		std::string buildAllStatsString() const;

	private:
		mutable std::mutex mMutex;
		std::map<std::string, std::unique_ptr<IStatistic>> mStats;
	};
}