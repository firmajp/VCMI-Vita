// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"
namespace VitaAmbientStats
{
inline std::atomic<uint32_t> queued{0}, completed{0}, fallback{0}, discarded{0}, pending{0}, sourceBytes{0};
inline VitaNetworkStats::WorkCounter completion, sourcePrepare;
}
