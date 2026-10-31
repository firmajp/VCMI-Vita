// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/* Part of VCMI engine, GNU GPL v2.0 or later. See license.txt. */
#pragma once
#include <atomic>
#include <cstdint>
namespace VitaLegacyLineStats
{
inline std::atomic<uint32_t> calls{0}, bytes{0};
}
