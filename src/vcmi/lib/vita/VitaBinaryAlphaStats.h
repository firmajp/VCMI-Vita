// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>

namespace VitaBinaryAlphaStats
{
// GUI-thread counters; the startup test finishes before rendering begins.
inline bool enabled = false;
inline uint64_t hits = 0, builds = 0, bypass = 0, evictions = 0, paletteUpdates = 0;
inline size_t bytes = 0, entries = 0;
}

void runVitaBinaryAlphaTest();
