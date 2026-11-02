// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/* Vita town composition cache, 2026-10-31. GNU GPL v2.0 or later; see license.txt. */
#pragma once
#include <atomic>
#include <cstdint>

namespace VitaTownCacheStats
{
// Timings are updated by the GUI; widget teardown can also clear the memory count.
inline uint64_t hits = 0, builds = 0, bypass = 0, failures = 0;
inline uint64_t buildingVisits = 0, buildingSkips = 0;
inline uint64_t partialBuilds = 0, paintPixels = 0;
inline uint64_t rebuildUs = 0, maxRebuildUs = 0, copyUs = 0;
inline std::atomic<uint32_t> bytes{0};
}
