// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
#pragma once

#include <atomic>
#include <array>

namespace VitaLoading
{
inline std::atomic<size_t> stage{0};
inline constexpr std::array<const char *, 23> names{
"initializing",
"artifacts",
"battlefields",
"biomes",
"bonuses",
"campaignRegions",
"creatures",
"factions",
"heroClasses",
"heroes",
"mapLayers",
"objects",
"obstacles",
"resources",
"rivers",
"roads",
"scripts",
"skills",
"spellSchools",
"spells",
"templates",
"terrains",
"finishing"
};
}
