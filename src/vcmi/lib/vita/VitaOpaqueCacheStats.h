// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/* Part of VCMI engine, GNU GPL v2.0 or later. See license.txt. */
#pragma once
#include <cstdint>
#include <cstddef>

// Updated and read on the GUI thread only.
namespace VitaOpaqueCacheStats
{
inline uint64_t hits = 0, builds = 0, bypass = 0, evictions = 0;
inline size_t bytes = 0;
}
