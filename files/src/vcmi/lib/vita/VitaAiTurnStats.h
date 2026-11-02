// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace VitaAiTurnStats
{
enum class Phase
{
	Update, Paths, Danger, Owners, Cluster, Decompose, Evaluate, Plan, Execute, WaitFree, Move, WaitBattle, WaitQuestion, WaitMovement, WaitVisit, Count
};

struct Counter
{
	uint64_t us = 0;
	uint64_t maxUs = 0;
	uint32_t calls = 0;
};

class Turn;
// Only the adventure AI owner thread records spans; worker execution is included in caller wall time.
inline thread_local Turn * current = nullptr;

class Turn
{
	friend class Scope;
	Turn * previous = current;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
	std::array<Counter, static_cast<size_t>(Phase::Count)> counters{};
public:
	Turn() { current = this; }
	Turn(const Turn &) = delete;
	Turn & operator=(const Turn &) = delete;
	~Turn() { current = previous; }
	void add(Phase phase, uint64_t us)
	{
		auto & counter = counters.at(static_cast<size_t>(phase));
		counter.us += us;
		if(us > counter.maxUs)
			counter.maxUs = us;
		++counter.calls;
	}

	void report(int player, int day) const
	{
		constexpr std::array<const char *, static_cast<size_t>(Phase::Count)> names = {
			"update", "paths", "danger", "owners", "cluster", "decompose", "evaluate", "plan", "execute", "wait_free", "move", "wait_battle", "wait_question", "wait_movement", "wait_visit"
		};
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
		std::array<char, 2048> line{};
		int used = std::snprintf(line.data(), line.size(), "Vita AI profile: player=%d day=%d total_ms=%.3f scope=owner_thread_nested_wall", player, day, elapsed / 1000.0);
		for(size_t i = 0; i < counters.size() && used > 0 && static_cast<size_t>(used) < line.size(); ++i)
		{
			const auto & counter = counters[i];
			const int written = std::snprintf(line.data() + used, line.size() - used, " %s_ms=%.3f %s_max_ms=%.3f %s_calls=%u",
				names[i], counter.us / 1000.0, names[i], counter.maxUs / 1000.0, names[i], counter.calls);
			if(written < 0)
				break;
			used += written;
		}
		std::fprintf(stderr, "%s\n", line.data());
	}
};

class Scope
{
	Turn * turn = current;
	Phase phase;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
public:
	explicit Scope(Phase phase) : phase(phase) {}
	Scope(const Scope &) = delete;
	Scope & operator=(const Scope &) = delete;
	~Scope()
	{
		if(!turn)
			return;
		const auto elapsed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
		turn->add(phase, elapsed);
	}
};
}
