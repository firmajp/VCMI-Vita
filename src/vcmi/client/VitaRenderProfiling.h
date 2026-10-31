// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/*
 * Part of VCMI engine, licensed under GNU GPL v2.0 or later.
 * See license.txt in the project root.
 */
#pragma once

#ifdef VCMI_VITA
#include <array>
#include <cstdint>
#include <SDL_timer.h>

namespace VitaRenderProfiling
{
enum class Kind : uint8_t
{
	ImageBlit, IndexedAlpha, ScaledDraw, SurfaceConvert, PaletteBind,
	BattleBackground, BattleObjects, MapUpdate, MapCompose,
	VideoDecode, VideoConvert, VideoOpen, VideoBufferAlloc,
	BattleBase, BattleCacheCopy, BattleCacheRebuild, BattleHighlights,
	BattleHoverRange, BattleTargetRange, BattleShootingRange,
	InputPoll, InputPreprocess, InputTask, Count
};
inline constexpr std::array<const char *, static_cast<size_t>(Kind::Count)> names{
	"image_sdl_blit", "indexed_alpha", "scaled_draw", "surface_convert", "palette_bind",
	"battle_background", "battle_objects", "map_update", "map_compose",
	"video_decode", "video_frame_total", "video_open", "video_buffer_alloc",
	"battle_base", "battle_cache_copy", "battle_cache_rebuild", "battle_highlights",
	"battle_hover_range", "battle_target_range", "battle_shooting_range",
	"input_sdl_poll", "input_preprocess", "input_user_task"
};
struct Counter
{
	uint64_t calls = 0, samples = 0, ticks = 0;
	uint64_t maxTicks = 0;
	uint32_t sampleState = 0x9e3779b9;
};
using Counters = std::array<Counter, static_cast<size_t>(Kind::Count)>;
// Worker threads do not contribute to the GUI frame's nested wall-time counters.
inline thread_local Counters * active = nullptr;

class Scope
{
	Counter * counter = nullptr;
	uint64_t started = 0;
public:
	explicit Scope(Kind kind)
	{
		if (!active)
			return;
		counter = &active->at(static_cast<size_t>(kind));
		++counter->calls;
		bool measure = true;
		if (kind <= Kind::PaletteBind)
		{
			// Fixed strides can repeatedly select the same sprite in a frame.
			auto & state = counter->sampleState;
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			measure = (state & 15) == 0;
		}
		if (measure)
		{
			++counter->samples;
			started = SDL_GetPerformanceCounter();
		}
	}
	~Scope()
	{
		if (started)
		{
			const auto elapsed = SDL_GetPerformanceCounter() - started;
			counter->ticks += elapsed;
			if (elapsed > counter->maxTicks)
				counter->maxTicks = elapsed;
		}
	}
	Scope(const Scope &) = delete;
	Scope & operator=(const Scope &) = delete;
};
}
#endif
