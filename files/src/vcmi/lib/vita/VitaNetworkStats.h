// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>

namespace VitaNetworkStats
{
// Unsigned cumulative counters wrap modulo 2^32; consumers use window deltas.
struct WorkCounter
{
	std::atomic<uint32_t> calls{0}, totalUs{0}, maxUs{0};
	void add(uint32_t elapsed)
	{
		calls.fetch_add(1, std::memory_order_relaxed);
		totalUs.fetch_add(elapsed, std::memory_order_relaxed);
		auto previous = maxUs.load(std::memory_order_relaxed);
		while(previous < elapsed && !maxUs.compare_exchange_weak(previous, elapsed, std::memory_order_relaxed)) {}
	}
};
inline WorkCounter decode, replay;
inline std::atomic<uint32_t> bytes{0};
}
