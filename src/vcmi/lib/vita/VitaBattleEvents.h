// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/* Part of VCMI engine, GNU GPL v2.0 or later. See license.txt. */
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace VitaBattleEvents
{
enum Kind { Activate, BlockUI, AnimationInit, RangedInit, SoundChunk, PendingPaint, FullPaint, SoundDecode, SoundPlay, BitmapText, FieldPaint, ControllerPrompts, BattleWindowShow, QuickSpellPaint, UnitActionPaint, TownPaint, Count };
inline constexpr std::array<const char *, Count> names{
	"activate_stack", "block_ui", "animation_init", "ranged_init", "sound_chunk", "pending_paint", "full_paint", "sound_decode", "sound_play", "bitmap_text", "field_paint", "controller_prompts", "battle_window_show", "quick_spell_paint", "unit_action_paint", "town_paint"};
struct Counter { std::atomic<uint32_t> calls{0}, totalUs{0}, maxUs{0}; };
inline std::array<Counter, Count> counters;
inline std::atomic<uint32_t> deferredGUI{0}, requests{0}, coalesced{0};
inline std::atomic<uint32_t> soundHits{0}, soundMisses{0}, soundEvictions{0}, soundBytes{0};
inline std::atomic<uint32_t> fontPaletteUpdates{0}, fontPaletteUnchanged{0};
inline std::atomic<uint32_t> pendingFull{0}, pendingAncestor{0}, pendingInactive{0};
inline std::atomic<uint32_t> panelFull{0}, panelBorder{0};
inline std::atomic<uint32_t> deferredMap{0}, deferredTown{0};
static_assert(std::atomic<uint32_t>::is_always_lock_free);
class Scope
{
	Kind kind;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
public:
	explicit Scope(Kind kind) : kind(kind) {}
	~Scope()
	{
		const auto elapsed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
		auto & c = counters[kind];
		c.totalUs.fetch_add(elapsed, std::memory_order_relaxed);
		auto maximum = c.maxUs.load(std::memory_order_relaxed);
		while(maximum < elapsed && !c.maxUs.compare_exchange_weak(maximum, elapsed, std::memory_order_relaxed)) {}
		c.calls.fetch_add(1, std::memory_order_relaxed);
	}
};
}
