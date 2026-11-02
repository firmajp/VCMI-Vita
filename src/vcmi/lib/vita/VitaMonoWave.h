// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaMapWorkStats.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <atomic>
#include <cstring>
#include <memory>
#include <vector>
#include <cstdio>
#include <algorithm>

namespace VitaMonoWave
{
inline bool enabled = false;
inline std::atomic<uint32_t> hits{0}, fallback{0};

inline Mix_Chunk * load(const void * data, size_t length)
{
	if(length < 12 || length > 16 * 1024 * 1024 || std::memcmp(data, "RIFF", 4) || std::memcmp(static_cast<const uint8_t *>(data) + 8, "WAVE", 4))
		return nullptr;
	int frequency = 0, channels = 0;
	Uint16 format = 0;
	if(!Mix_QuerySpec(&frequency, &format, &channels) || channels != 2 || format != AUDIO_S16SYS)
		return nullptr;
	SDL_AudioSpec input{};
	Uint8 * decoded = nullptr;
	Uint32 decodedLength = 0;
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::WaveDecode);
		if(!SDL_LoadWAV_RW(SDL_RWFromConstMem(data, static_cast<int>(length)), 1, &input, &decoded, &decodedLength))
			return nullptr;
	}
	std::unique_ptr<Uint8, decltype(&SDL_FreeWAV)> decodedOwner(decoded, SDL_FreeWAV);
	if(input.channels != 1 || (input.freq != 11025 && input.freq != 22050) || (frequency <= 0 || frequency > 192000) || decodedLength > 4 * 1024 * 1024)
		return nullptr;
	VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::WaveResample);
	SDL_AudioCVT cvt{};
	if(SDL_BuildAudioCVT(&cvt, input.format, 1, input.freq, AUDIO_F32SYS, 1, frequency) < 0 || cvt.len_mult <= 0)
		return nullptr;
	cvt.len = static_cast<int>(decodedLength);
	const size_t required = static_cast<size_t>(cvt.len) * cvt.len_mult;
	if(!required || required > 16 * 1024 * 1024)
		return nullptr;
	std::unique_ptr<void, decltype(&SDL_free)> work(SDL_calloc(1, required), SDL_free);
	if(!work)
		return nullptr;
	cvt.buf = static_cast<Uint8 *>(work.get());
	std::memcpy(cvt.buf, decoded, decodedLength);
	decodedOwner.reset();
	if(SDL_ConvertAudio(&cvt) != 0 || cvt.len_cvt <= 0 || cvt.len_cvt % sizeof(float) != 0 || static_cast<size_t>(cvt.len_cvt) > required)
		return nullptr;
	SDL_AudioCVT finish{};
	if(SDL_BuildAudioCVT(&finish, AUDIO_F32SYS, 1, frequency, format, 2, frequency) < 0 || finish.len_mult <= 0)
		return nullptr;
	finish.len = cvt.len_cvt;
	const size_t stereoCapacity = static_cast<size_t>(finish.len) * finish.len_mult;
	if(!stereoCapacity || stereoCapacity > 32 * 1024 * 1024)
		return nullptr;
	std::unique_ptr<void, decltype(&SDL_free)> stereo(SDL_calloc(1, stereoCapacity), SDL_free);
	if(!stereo)
		return nullptr;
	finish.buf = static_cast<Uint8 *>(stereo.get());
	std::memcpy(finish.buf, cvt.buf, finish.len);
	work.reset();
	if(SDL_ConvertAudio(&finish) != 0 || finish.len_cvt <= 0 || static_cast<size_t>(finish.len_cvt) > stereoCapacity)
		return nullptr;
	const auto stereoLength = static_cast<Uint32>(finish.len_cvt);
	if(auto * resized = SDL_realloc(stereo.get(), stereoLength))
	{
		stereo.release();
		stereo.reset(resized);
	}
	auto * chunk = Mix_QuickLoad_RAW(static_cast<Uint8 *>(stereo.get()), stereoLength);
	if(chunk)
	{
		// Mix_FreeChunk owns SDL-allocated PCM only when allocated is set.
		chunk->allocated = 1;
		stereo.release();
	}
	return chunk;
}

inline void selfTest()
{
	int frequency = 0, channels = 0;
	Uint16 format = 0;
	Mix_QuerySpec(&frequency, &format, &channels);
	std::fprintf(stderr, "Vita mono wave mixer frequency=%d format=%04x channels=%d\n", frequency, format, channels);
	bool ok = true;
	unsigned cases = 0;
	for(int rate : {11025, 22050})
		for(int bits : {8, 16})
			for(int frames : {1, 17, 255, 1024})
			{
				const auto bytes = frames * bits / 8;
				std::vector<Uint8> wave(44 + bytes + (bytes & 1), 0);
				auto put16 = [&](size_t at, Uint16 value) { wave[at] = value; wave[at + 1] = value >> 8; };
				auto put32 = [&](size_t at, Uint32 value) { put16(at, value); put16(at + 2, value >> 16); };
				std::memcpy(wave.data(), "RIFF", 4); put32(4, wave.size() - 8);
				std::memcpy(wave.data() + 8, "WAVEfmt ", 8); put32(16, 16);
				put16(20, 1); put16(22, 1); put32(24, rate); put32(28, rate * bits / 8);
				put16(32, bits / 8); put16(34, bits); std::memcpy(wave.data() + 36, "data", 4); put32(40, bytes);
				for(int i = 0; i < bytes; ++i) wave[44 + i] = static_cast<Uint8>(i * 73 + 129);
				std::unique_ptr<Mix_Chunk, decltype(&Mix_FreeChunk)> reference(Mix_LoadWAV_RW(SDL_RWFromConstMem(wave.data(), wave.size()), 1), Mix_FreeChunk);
				std::unique_ptr<Mix_Chunk, decltype(&Mix_FreeChunk)> converted(load(wave.data(), wave.size()), Mix_FreeChunk);
				if(!reference || !converted || reference->alen != converted->alen || std::memcmp(reference->abuf, converted->abuf, reference->alen))
				{
					ok = false;
					size_t difference = 0;
					if(reference && converted)
						while(difference < std::min(reference->alen, converted->alen) && reference->abuf[difference] == converted->abuf[difference]) ++difference;
					std::fprintf(stderr, "Vita mono wave mismatch rate=%d bits=%d frames=%d reference=%d converted=%d first_byte=%u error=%s\n",
						rate, bits, frames, reference ? static_cast<int>(reference->alen) : -1, converted ? static_cast<int>(converted->alen) : -1, static_cast<unsigned>(difference), SDL_GetError());
				}
				++cases;
			}
	enabled = ok;
	// Exclude test conversions from device-session measurements.
	for(auto kind : {VitaMapWorkStats::Kind::WaveDecode, VitaMapWorkStats::Kind::WaveResample})
	{
		auto & counter = VitaMapWorkStats::counters.at(static_cast<size_t>(kind));
		counter.calls.store(0); counter.totalUs.store(0); counter.maxUs.store(0);
	}
	std::fprintf(stderr, "Vita mono wave self-test %s cases=%u enabled=%d\n", ok ? "PASS" : "FAIL", cases, ok);
}
}
