// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"
#include "VitaPacketTrace.h"
#include <array>
#include <chrono>

namespace VitaMapWorkStats
{
enum class Kind : size_t
{
	Invalidate, Dispatch, Reveal, Tiles, StartUI, Path, AnimationWait, EndUI, ContinueMove,
	Music, Visibility, AmbientScan, AmbientChannels, SoundRead, WaveDecode, WaveResample, MixerLoad, SoundCacheRead, SoundCacheWrite, SoundCacheKey, SoundCacheIo, SoundCacheInflate, SoundCacheCheck, SoundCacheExpand, Count
};
inline constexpr std::array<const char *, static_cast<size_t>(Kind::Count)> names{
	"invalidate", "dispatch", "reveal", "tiles", "start_ui", "path", "animation_wait", "end_ui", "continue_move",
	"music", "visibility", "ambient_scan", "ambient_channels", "sound_read", "wave_decode", "wave_resample", "mixer_load", "sound_cache_read", "sound_cache_write", "sound_cache_key", "sound_cache_io", "sound_cache_inflate", "sound_cache_check", "sound_cache_expand"
};
inline std::array<VitaNetworkStats::WorkCounter, names.size()> counters;

class Scope
{
	Kind kind;
	std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
public:
	explicit Scope(Kind kind) : kind(kind) {}
	~Scope()
	{
		const auto ended = std::chrono::steady_clock::now();
		counters.at(static_cast<size_t>(kind)).add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(ended - started).count()));
		VitaPacketTrace::record({names.at(static_cast<size_t>(kind)), VitaPacketTrace::microseconds(started), VitaPacketTrace::microseconds(ended), 0, 0, 0, 0, "map_work"});
	}
};
}
