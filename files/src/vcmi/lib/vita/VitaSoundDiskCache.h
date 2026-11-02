// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../VCMIDirs.h"
#include "VitaMapWorkStats.h"
#include <boost/uuid/detail/sha1.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <SDL_mixer.h>
#include <zlib.h>
#include <psp2/io/fcntl.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace VitaSoundDiskCache
{
using Header = std::array<uint32_t, 16>;
using Chunk = std::unique_ptr<Mix_Chunk, decltype(&Mix_FreeChunk)>;
inline constexpr size_t maxPcm = 1024 * 1024 - sizeof(Header);
inline constexpr uint32_t queueBudget = 2 * 1024 * 1024;
inline bool enabled = false;
inline std::atomic<uint32_t> hits{0}, misses{0}, writes{0}, failures{0}, skipped{0}, pendingBytes{0};
inline std::mutex writerMutex;
inline std::atomic<uint32_t> secondaryHits{0}, conflictProbes{0};
inline std::atomic<uint32_t> monoHits{0}, nativeReads{0};
inline std::atomic<uint32_t> compressedHits{0}, rawHits{0}, legacyHits{0}, packedBytes{0}, pcmBytes{0}, savedBytes{0};
struct ReadResult
{
	Chunk chunk{nullptr, Mix_FreeChunk};
	bool compressed = false;
	bool mono = false;
	bool legacy = false;
	bool conflict = false;
	unsigned candidate = 0;
};

inline const boost::filesystem::path & directory()
{
	static const auto path = VCMIDirs::get().userCachePath() / "sound-pcm-v1";
	return path;
}

inline Header key(const void * data, size_t length)
{
	VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheKey);
	Header result{};
	result[0] = 0x56415743;
	result[1] = 1;
	int frequency = 0, channels = 0;
	Uint16 format = 0;
	Mix_QuerySpec(&frequency, &format, &channels);
	result[2] = frequency; result[3] = format; result[4] = channels;
	boost::uuids::detail::sha1 sha;
	sha.process_bytes(data, length);
	boost::uuids::detail::sha1::digest_type digest;
	sha.get_digest(digest);
	for(size_t i = 0; i < 5; ++i) result[7 + i] = digest[i];
	return result;
}

using Candidates = std::array<std::string, 4>;
inline Candidates slots(const Header & signature)
{
	Candidates result;
	std::array<uint32_t, 4> indices{};
	for(size_t i = 0; i < indices.size(); ++i)
	{
		auto index = signature[7 + i] % 64;
		while(std::find(indices.begin(), indices.begin() + i, index) != indices.begin() + i) index = (index + 1) % 64;
		indices[i] = index;
		char name[24];
		std::snprintf(name, sizeof(name), "slot-%02u.pcm", index);
		result[i] = (directory() / name).string();
	}
	return result;
}

inline bool matches(const Header & signature, const Header & header)
{
	auto expected = signature;
	for(auto field : {5, 6, 12, 13, 14}) expected[field] = header[field];
	return expected == header;
}

struct NativeFile
{
	NativeFile() = default;
	NativeFile(const NativeFile &) = delete;
	NativeFile & operator=(const NativeFile &) = delete;
	SceUID handle = -1;
	void close() { if(handle >= 0) { sceIoClose(handle); handle = -1; } }
	~NativeFile() { close(); }
};

inline bool readFully(SceUID handle, void * data, size_t length)
{
	auto * output = static_cast<Uint8 *>(data);
	while(length)
	{
		nativeReads.fetch_add(1);
		const auto actual = sceIoRead(handle, output, length);
		if(actual <= 0 || static_cast<size_t>(actual) > length) return false;
		output += actual; length -= actual;
	}
	return true;
}

inline ReadResult read(const Header & signature, const std::string & path)
{
	ReadResult result;
	NativeFile file;
	Header header{};
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheIo);
		file.handle = sceIoOpen(path.c_str(), SCE_O_RDONLY, 0);
		if(file.handle < 0 || !readFully(file.handle, header.data(), sizeof(header))) return result;
	}
	result.conflict = header[0] == signature[0] && header[1] == signature[1] && !matches(signature, header);
	const uint32_t length = header[5], checksum = header[6], packedLength = header[12], codec = header[13], revision = header[14];
	auto expected = signature;
	expected[5] = length; expected[6] = checksum; expected[12] = packedLength; expected[13] = codec; expected[14] = revision;
	if(header != expected || !length || length > maxPcm || length % 4 || header[3] != AUDIO_S16SYS || header[4] != 2 || revision > 1)
		return result;
	const bool mono = codec == 3 || codec == 4;
	const bool compressed = codec == 1 || codec == 3;
	const auto decodedLength = mono ? length / 2 : length;
	if(codec > 4 || (mono && revision != 1) || (codec == 0 && (packedLength != 0 || revision != 0))
		|| (compressed && (!packedLength || packedLength >= decodedLength))
		|| ((codec == 2 || codec == 4) && packedLength != decodedLength)) return result;
	const auto storedLength = compressed ? packedLength : decodedLength;
	std::unique_ptr<void, decltype(&SDL_free)> stored(SDL_malloc(storedLength), SDL_free);
	if(!stored) return result;
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheIo);
		if(!readFully(file.handle, stored.get(), storedLength)) return result;
		Uint8 extra;
		nativeReads.fetch_add(1);
		if(sceIoRead(file.handle, &extra, 1) != 0) return result;
		file.close();
	}
	std::unique_ptr<void, decltype(&SDL_free)> pcm(nullptr, SDL_free);
	if(compressed)
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheInflate);
		pcm.reset(SDL_malloc(decodedLength));
		uLongf actualLength = decodedLength;
		if(!pcm || uncompress(static_cast<Bytef *>(pcm.get()), &actualLength, static_cast<const Bytef *>(stored.get()), storedLength) != Z_OK || actualLength != decodedLength)
			return result;
	}
	else pcm = std::move(stored);
	if(mono)
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheExpand);
		std::unique_ptr<void, decltype(&SDL_free)> stereo(SDL_malloc(length), SDL_free);
		if(!stereo) return result;
		const auto * from = static_cast<const Uint8 *>(pcm.get());
		auto * to = static_cast<Uint8 *>(stereo.get());
		for(size_t i = 0; i < length / 4; ++i)
		{
			to[4 * i] = to[4 * i + 2] = from[2 * i];
			to[4 * i + 1] = to[4 * i + 3] = from[2 * i + 1];
		}
		pcm = std::move(stereo);
	}
	{
		VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheCheck);
		if(crc32(0, static_cast<const Bytef *>(pcm.get()), length) != checksum) return result;
	}
	result.chunk.reset(Mix_QuickLoad_RAW(static_cast<Uint8 *>(pcm.get()), length));
	if(result.chunk)
	{
		result.chunk->allocated = 1;
		pcm.release();
		result.compressed = compressed;
		result.mono = mono;
		result.legacy = revision == 0 || (compressed && storedLength > decodedLength - decodedLength / 4);
		packedBytes.fetch_add(storedLength); pcmBytes.fetch_add(length);
	}
	return result;
}

inline ReadResult readCandidates(const Header & signature, const Candidates & candidates)
{
	for(size_t i = 0; i < candidates.size(); ++i)
	{
		auto result = read(signature, candidates[i]);
		if(result.conflict) conflictProbes.fetch_add(1);
		if(result.chunk)
		{
			result.candidate = i;
			if(i) secondaryHits.fetch_add(1);
			return result;
		}
	}
	return {};
}

// Caller holds writerMutex through destination selection and publication.
inline bool writePayload(const Header & signature, const Mix_Chunk & chunk, const std::string & path)
{
	if(!chunk.alen || chunk.alen > maxPcm || chunk.alen % 4 || signature[3] != AUDIO_S16SYS || signature[4] != 2)
		return false;
	const auto temporary = (directory() / "pending.tmp").string();
	Header header = signature;
	header[5] = chunk.alen;
	header[6] = crc32(0, chunk.abuf, chunk.alen);
	bool mono = true;
	for(size_t i = 0; i < chunk.alen; i += 4)
		if(chunk.abuf[i] != chunk.abuf[i + 2] || chunk.abuf[i + 1] != chunk.abuf[i + 3]) { mono = false; break; }
	std::unique_ptr<void, decltype(&SDL_free)> monoPcm(nullptr, SDL_free);
	if(mono)
	{
		monoPcm.reset(SDL_malloc(chunk.alen / 2));
		if(monoPcm)
		{
			auto * to = static_cast<Uint8 *>(monoPcm.get());
			for(size_t i = 0; i < chunk.alen / 4; ++i)
			{
				to[2 * i] = chunk.abuf[4 * i];
				to[2 * i + 1] = chunk.abuf[4 * i + 1];
			}
		}
		else mono = false;
	}
	const auto * source = mono ? static_cast<const Uint8 *>(monoPcm.get()) : chunk.abuf;
	const auto sourceLength = mono ? chunk.alen / 2 : chunk.alen;
	uLongf compressedLength = compressBound(sourceLength);
	std::unique_ptr<void, decltype(&SDL_free)> packed(SDL_malloc(compressedLength), SDL_free);
	const bool compressed = packed && compress2(static_cast<Bytef *>(packed.get()), &compressedLength, source, sourceLength, Z_BEST_SPEED) == Z_OK && compressedLength <= sourceLength - sourceLength / 4;
	const auto * payload = compressed ? static_cast<const Uint8 *>(packed.get()) : source;
	const auto payloadLength = compressed ? compressedLength : sourceLength;
	header[12] = payloadLength; header[13] = mono ? (compressed ? 3 : 4) : (compressed ? 1 : 2); header[14] = 1;
	FILE * file = std::fopen(temporary.c_str(), "wb");
	if(!file) return false;
	const bool complete = std::fwrite(header.data(), sizeof(header), 1, file) == 1
		&& std::fwrite(payload, 1, payloadLength, file) == payloadLength && std::fflush(file) == 0;
	const bool closed = std::fclose(file) == 0;
	if(complete && closed)
	{
		if(std::rename(temporary.c_str(), path.c_str()) == 0) { savedBytes.fetch_add(chunk.alen - payloadLength); return true; }
		// Some Vita filesystems require removing an existing cache slot first.
		std::remove(path.c_str());
		if(std::rename(temporary.c_str(), path.c_str()) == 0) { savedBytes.fetch_add(chunk.alen - payloadLength); return true; }
	}
	std::remove(temporary.c_str());
	return false;
}

inline bool write(const Header & signature, const Mix_Chunk & chunk, const std::string & path)
{
	std::scoped_lock guard(writerMutex);
	return writePayload(signature, chunk, path);
}

inline bool writeCandidates(const Header & signature, const Mix_Chunk & chunk, const Candidates & candidates)
{
	std::scoped_lock guard(writerMutex);
	size_t available = candidates.size();
	for(size_t i = 0; i < candidates.size(); ++i)
	{
		NativeFile file;
		file.handle = sceIoOpen(candidates[i].c_str(), SCE_O_RDONLY, 0);
		Header header{};
		const bool present = file.handle >= 0 && readFully(file.handle, header.data(), sizeof(header));
		if(present && matches(signature, header))
		{
			file.close();
			return writePayload(signature, chunk, candidates[i]);
		}
		if(available == candidates.size() && (!present || header[0] != signature[0] || header[1] != signature[1])) available = i;
	}
	return writePayload(signature, chunk, candidates[available == candidates.size() ? 0 : available]);
}

inline bool reserve(uint32_t bytes)
{
	if(!bytes || bytes > maxPcm) { skipped.fetch_add(1); return false; }
	auto previous = pendingBytes.load();
	while(previous <= queueBudget - bytes)
		if(pendingBytes.compare_exchange_weak(previous, previous + bytes)) return true;
	skipped.fetch_add(1);
	return false;
}

inline void selfTest()
{
	boost::system::error_code error;
	boost::filesystem::create_directories(directory(), error);
	if(error)
	{
		std::fprintf(stderr, "Vita sound disk cache self-test FAIL directory_error=%d\n", error.value());
		return;
	}
	const auto path = (directory() / "selftest.pcm").string();
	const auto signature = key("generated-cache-test", 20);
	std::array<Uint8, 1024> pcm{};
	for(size_t i = 0; i < pcm.size(); ++i) pcm[i] = static_cast<Uint8>(i * 73 + 19);
	Mix_Chunk fixture{};
	fixture.abuf = pcm.data(); fixture.alen = pcm.size();
	bool ok = write(signature, fixture, path);
	auto loaded = read(signature, path);
	ok = ok && loaded.chunk && loaded.compressed && loaded.chunk->alen == pcm.size() && std::memcmp(loaded.chunk->abuf, pcm.data(), pcm.size()) == 0;
	auto changed = signature; changed[7] ^= 1;
	ok = ok && !read(changed, path).chunk;
	FILE * compressedFile = std::fopen(path.c_str(), "r+b");
	if(compressedFile)
	{
		const bool corrupt = std::fseek(compressedFile, sizeof(Header), SEEK_SET) == 0 && std::fputc(0, compressedFile) != EOF;
		const bool closed = std::fclose(compressedFile) == 0;
		ok = ok && corrupt && closed && !read(signature, path).chunk;
	}
	else ok = false;
	Header legacy = signature;
	legacy[5] = pcm.size(); legacy[6] = crc32(0, pcm.data(), pcm.size());
	FILE * legacyFile = std::fopen(path.c_str(), "wb");
	if(legacyFile)
	{
		const bool written = std::fwrite(legacy.data(), sizeof(legacy), 1, legacyFile) == 1 && std::fwrite(pcm.data(), 1, pcm.size(), legacyFile) == pcm.size();
		const bool closed = std::fclose(legacyFile) == 0;
		auto raw = read(signature, path);
		ok = ok && written && closed && raw.chunk && raw.legacy && raw.chunk->alen == pcm.size() && std::memcmp(raw.chunk->abuf, pcm.data(), pcm.size()) == 0;
	}
	else ok = false;
	FILE * file = std::fopen(path.c_str(), "r+b");
	if(file)
	{
		const bool corrupt = std::fseek(file, sizeof(Header), SEEK_SET) == 0 && std::fputc(pcm[0] ^ 1, file) != EOF;
		const bool closed = std::fclose(file) == 0;
		ok = ok && corrupt && closed && !read(signature, path).chunk;
	}
	else ok = false;
	file = std::fopen(path.c_str(), "wb");
	if(file)
	{
		const bool truncated = std::fwrite(signature.data(), 1, 7, file) == 7;
		const bool closed = std::fclose(file) == 0;
		ok = ok && truncated && closed && !read(signature, path).chunk;
	}
	else ok = false;
	for(size_t i = 0; i < pcm.size(); i += 4) { pcm[i + 2] = pcm[i]; pcm[i + 3] = pcm[i + 1]; }
	const bool monoWritten = write(signature, fixture, path);
	auto mono = read(signature, path);
	ok = ok && monoWritten && mono.chunk && mono.mono && mono.compressed && mono.chunk->alen == pcm.size() && std::memcmp(mono.chunk->abuf, pcm.data(), pcm.size()) == 0;
	std::array<Uint8, 512> weakMono{};
	uint32_t random = 0x13579bdf;
	for(auto & sample : weakMono)
	{
		random ^= random << 13; random ^= random >> 17; random ^= random << 5;
		sample = static_cast<Uint8>(random);
	}
	std::copy_n(weakMono.begin(), 64, weakMono.begin() + 448);
	for(size_t i = 0; i < weakMono.size() / 2; ++i)
	{
		pcm[4 * i] = pcm[4 * i + 2] = weakMono[2 * i];
		pcm[4 * i + 1] = pcm[4 * i + 3] = weakMono[2 * i + 1];
	}
	const bool weakWritten = write(signature, fixture, path);
	auto weak = read(signature, path);
	ok = ok && weakWritten && weak.chunk && weak.mono && !weak.compressed && !weak.legacy
		&& weak.chunk->alen == pcm.size() && std::memcmp(weak.chunk->abuf, pcm.data(), pcm.size()) == 0;
	std::array<Uint8, 1024> oldPayload{};
	uLongf oldLength = oldPayload.size();
	const bool oldCompressed = compress2(oldPayload.data(), &oldLength, weakMono.data(), weakMono.size(), Z_BEST_SPEED) == Z_OK;
	Header oldHeader = signature;
	oldHeader[5] = pcm.size(); oldHeader[6] = crc32(0, pcm.data(), pcm.size());
	oldHeader[12] = oldLength; oldHeader[13] = 3; oldHeader[14] = 1;
	FILE * oldFile = std::fopen(path.c_str(), "wb");
	if(oldFile)
	{
		const bool written = std::fwrite(oldHeader.data(), sizeof(oldHeader), 1, oldFile) == 1
			&& std::fwrite(oldPayload.data(), 1, oldLength, oldFile) == oldLength;
		const bool closed = std::fclose(oldFile) == 0;
		auto old = read(signature, path);
		ok = ok && oldCompressed && oldLength < weakMono.size() && oldLength > weakMono.size() * 3 / 4
			&& written && closed && old.chunk && old.compressed && old.legacy
			&& old.chunk->alen == pcm.size() && std::memcmp(old.chunk->abuf, pcm.data(), pcm.size()) == 0;
	}
	else ok = false;
	fixture.alen = 4;
	const bool tinyWritten = write(signature, fixture, path);
	auto tiny = read(signature, path);
	ok = ok && tinyWritten && tiny.chunk && tiny.mono && !tiny.compressed && tiny.chunk->alen == 4 && std::memcmp(tiny.chunk->abuf, pcm.data(), 4) == 0;
	Candidates alternatives;
	for(size_t i = 0; i < alternatives.size(); ++i)
	{
		alternatives[i] = (directory() / ("selftest-choice-" + std::to_string(i) + ".pcm")).string();
		std::remove(alternatives[i].c_str());
	}
	const std::array<Uint8, 4> first{pcm[0], pcm[1], pcm[2], pcm[3]};
	const bool firstWritten = writeCandidates(signature, fixture, alternatives);
	auto secondSignature = signature; secondSignature[7] ^= 1;
	pcm[0] ^= 0x3c; pcm[2] ^= 0x3c;
	const bool secondWritten = writeCandidates(secondSignature, fixture, alternatives);
	auto firstLoaded = readCandidates(signature, alternatives);
	auto secondLoaded = readCandidates(secondSignature, alternatives);
	ok = ok && firstWritten && secondWritten && firstLoaded.chunk && secondLoaded.chunk && secondLoaded.candidate == 1
		&& firstLoaded.chunk->alen == 4 && secondLoaded.chunk->alen == 4
		&& std::memcmp(firstLoaded.chunk->abuf, first.data(), 4) == 0 && std::memcmp(secondLoaded.chunk->abuf, pcm.data(), 4) == 0;
	for(const auto & alternative : alternatives) std::remove(alternative.c_str());
	std::remove(path.c_str());
	secondaryHits.store(0); conflictProbes.store(0);
	nativeReads.store(0);
	packedBytes.store(0); pcmBytes.store(0); savedBytes.store(0);
	for(auto kind : {VitaMapWorkStats::Kind::SoundCacheKey, VitaMapWorkStats::Kind::SoundCacheIo, VitaMapWorkStats::Kind::SoundCacheInflate, VitaMapWorkStats::Kind::SoundCacheCheck, VitaMapWorkStats::Kind::SoundCacheExpand})
	{
		auto & counter = VitaMapWorkStats::counters.at(static_cast<size_t>(kind));
		counter.calls.store(0); counter.totalUs.store(0); counter.maxUs.store(0);
	}
	enabled = ok;
	std::fprintf(stderr, "Vita sound disk cache self-test %s cases=11 enabled=%d\n", ok ? "PASS" : "FAIL", ok);
}
}
