// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"
#include <array>
#include <chrono>

namespace VitaSendStats
{
enum class Kind : size_t { Queue, GameSend, Notify, Wait, ConnectionLock, Encode, Transport, Finalize, Count };
inline constexpr std::array<const char *, static_cast<size_t>(Kind::Count)> names{
	"request_queue", "game_send", "request_notify", "reply_wait", "connection_lock", "encode", "transport", "finalize"
};
inline std::array<VitaNetworkStats::WorkCounter, names.size()> counters;
class Scope
{
	Kind kind;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
public:
	explicit Scope(Kind kind) : kind(kind) {}
	~Scope()
	{
		counters.at(static_cast<size_t>(kind)).add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()));
	}
};
}
