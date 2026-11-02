// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
namespace VitaSpellAiProbe
{
enum class Phase : uint32_t { Idle, Checks, Targets, Baseline, BaselineQueue, State, Cast, Damage, FullEval, Score, Select, Submit, Release };
inline std::atomic<uint32_t> phase{0}, progress{0}, stack{0}, total{0};
inline std::atomic<int32_t> spell{-1}, candidate{-1};
inline void mark(Phase value, int32_t spellId = -1, int32_t index = -1)
{
	spell.store(spellId, std::memory_order_relaxed);
	candidate.store(index, std::memory_order_relaxed);
	phase.store(static_cast<uint32_t>(value), std::memory_order_release);
	progress.fetch_add(1, std::memory_order_relaxed);
}
struct Session
{
	explicit Session(uint32_t unit)
	{
		stack.store(unit, std::memory_order_relaxed);
		total.store(0, std::memory_order_relaxed);
		mark(Phase::Checks);
	}
	~Session() { mark(Phase::Idle); }
};
}
