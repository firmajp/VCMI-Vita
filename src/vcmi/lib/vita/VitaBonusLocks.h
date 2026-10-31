// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
#pragma once

#include <atomic>
#include <cstddef>

namespace VitaBonusLocks
{
inline std::atomic<size_t> live{0};
}
