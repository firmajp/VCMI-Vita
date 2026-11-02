// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
namespace VitaMapUiStats
{
inline std::atomic<uint32_t> layerRebuilds{0}, layerHits{0}, barUpdates{0}, barHits{0};
inline std::atomic<uint32_t> helpBuilds{0}, helpHits{0}, heroInfoBuilds{0}, heroInfoHits{0};
inline std::atomic<uint32_t> objectImageAllocations{0}, objectImageReuses{0}, objectChecksumAllocations{0}, objectChecksumReuses{0};
inline uint64_t minimapBuilds = 0, minimapHits = 0, minimapBypass = 0, minimapScaleTicks = 0, minimapCopyTicks = 0;
inline std::atomic<uint32_t> minimapChanged{0}, minimapUnchanged{0};
inline uint32_t tileSampleState = 0x63d83595;
inline uint64_t checksumSamples = 0, checksumTicks = 0, paintSamples = 0, paintTicks = 0;
inline uint64_t objectPartsChecked = 0, objectPartsOutside = 0;
inline uint64_t composeRuns = 0, composeTiles = 0;
inline uint64_t tileChecks = 0, tilePaints = 0, animationDeferrals = 0;
}
