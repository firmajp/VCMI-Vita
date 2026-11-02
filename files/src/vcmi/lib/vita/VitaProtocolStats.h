// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"

namespace VitaProtocolStats
{
inline std::atomic<uint32_t> requests{0}, sent{0}, acknowledgements{0}, rejected{0}, slowPacks{0};
inline std::atomic<uint32_t> ackNoopApply{0};
inline std::atomic<int32_t> lastSent{-1}, lastAcknowledged{-1};
inline std::atomic<const char *> requestType{"none"}, packType{"none"}, slowPackType{"none"};
inline VitaNetworkStats::WorkCounter sendWork, sendWait, sendNoWait, packBefore, packState, packAfter;
}
