// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace VitaPacketTrace
{
using Clock = std::chrono::steady_clock;
inline uint64_t microseconds(Clock::time_point time)
{
	return std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
}
inline thread_local uint64_t animationWaitUs = 0;
class AnimationWaitScope
{
	Clock::time_point started = Clock::now();
public:
	~AnimationWaitScope()
	{
		animationWaitUs += microseconds(Clock::now()) - microseconds(started);
	}
};
struct Entry
{
	const char * type = "none";
	uint64_t started = 0, ended = 0, animationWait = 0;
	uint32_t before = 0, state = 0, after = 0;
	const char * group = "packet";
};
struct History
{
	std::array<Entry, 32> entries{};
	uint64_t count = 0;
};
inline std::mutex mutex;
inline History history;
inline void record(const Entry & entry)
{
	if(entry.ended - entry.started < 5000)
		return;
	std::scoped_lock guard(mutex);
	history.entries[history.count % history.entries.size()] = entry;
	++history.count;
}
class Scope
{
	const char * name;
	Clock::time_point started = Clock::now();
public:
	explicit Scope(const char * name) : name(name) {}
	~Scope()
	{
		record({name, microseconds(started), microseconds(Clock::now()), 0, 0, 0, 0, "turn_ui"});
	}
};
inline History snapshot()
{
	std::scoped_lock guard(mutex);
	return history;
}
}
