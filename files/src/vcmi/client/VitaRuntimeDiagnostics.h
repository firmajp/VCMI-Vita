// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
#pragma once

#ifdef VCMI_VITA
#include <atomic>
#include <cstdint>
#include <SDL_timer.h>

namespace VitaRuntime
{
enum Phase : uint32_t
{
	Idle, FetchInput, UpdateLock, ReclaimAssets, UpdateGame,
	HandleInput, ClearLayers, DrawWindows, UploadScreen, FrameCallbacks,
	UpdateCursor, Present, FrameDelay, ControllerPanStart, ControllerPan,
	ControllerPanEnd
};
inline std::atomic<uint32_t> phase{Idle};
inline std::atomic<uint32_t> lastProgress{0};
inline std::atomic<uint32_t> frames{0};
inline std::atomic<bool> enabled{false};
inline std::atomic<bool> battleSnapshotRequested{false};
inline void mark(Phase value)
{
	phase.store(value, std::memory_order_relaxed);
	lastProgress.store(SDL_GetTicks(), std::memory_order_release);
}
}
#endif
