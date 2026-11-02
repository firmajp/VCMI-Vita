// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"
#include <chrono>

namespace VitaBattleTraceStats
{
inline VitaNetworkStats::WorkCounter decisions, spells, animationWaits, targetBuild, exchange, reachabilityUpdate, reachabilityQuery;
inline std::atomic<uint32_t> active{0}, stage{0}, stack{0}, evaluated{0}, submitted{0}, waiters{0};
inline std::atomic<int32_t> action{-1};
inline std::atomic<uint32_t> queueEntries{0}, queueTicks{0}, queueBusy{0};
class Timer
{
	VitaNetworkStats::WorkCounter & counter;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
public:
	explicit Timer(VitaNetworkStats::WorkCounter & counter) : counter(counter) {}
	~Timer()
	{
		counter.add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()));
	}
};
class Decision
{
	Timer timer{decisions};
public:
	explicit Decision(uint32_t unit)
	{
		active.fetch_add(1);
		stack.store(unit);
		stage.store(1);
	}
	~Decision()
	{
		stage.store(0);
		active.fetch_sub(1);
	}
};
class AnimationWait
{
	Timer timer{animationWaits};
public:
	AnimationWait() { waiters.fetch_add(1); }
	~AnimationWait() { waiters.fetch_sub(1); }
};
}
