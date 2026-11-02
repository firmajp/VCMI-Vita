/*
 * CMusicHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#ifdef VCMI_VITA
#include "lib/vita/VitaBattleEvents.h"
#include "lib/vita/VitaMonoWave.h"
#include "lib/vita/VitaSoundDiskCache.h"
#include "lib/vita/VitaAmbientStats.h"
#include "lib/filesystem/CFileInputStream.h"
#include "lib/filesystem/CArchiveLoader.h"
#include "lib/filesystem/CFilesystemLoader.h"
#include "lib/AsyncRunner.h"
#endif
#include "CSoundHandler.h"

#include "GameEngine.h"

#include "lib/filesystem/Filesystem.h"
#include "lib/CRandomGenerator.h"

#include <SDL_mixer.h>

#define VCMI_SOUND_NAME(x)
#define VCMI_SOUND_FILE(y) #y,

#ifdef VCMI_VITA
struct CSoundHandler::AmbientJob
{
	std::function<std::unique_ptr<CInputStream>()> open;
	std::unique_ptr<CInputStream> stream;
	std::pair<std::unique_ptr<ui8[]>, si64> source;
	VitaSoundDiskCache::Header signature{};
	VitaSoundDiskCache::ReadResult result;
	std::string error;
};
struct CSoundHandler::AmbientState
{
	CSoundHandler * owner = nullptr;
	std::map<AudioPath, std::shared_ptr<AmbientJob>> loading;
	std::map<AudioPath, int> wanted;
};

static void persistSoundChunk(const VitaSoundDiskCache::Header & signature, const std::shared_ptr<Mix_Chunk> & result)
{
	if(VitaSoundDiskCache::enabled && VitaSoundDiskCache::reserve(result->alen))
		ENGINE->async().run([signature, result]()
		{
			VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheWrite);
			if(VitaSoundDiskCache::writeCandidates(signature, *result, VitaSoundDiskCache::slots(signature)))
				VitaSoundDiskCache::writes.fetch_add(1);
			else
				VitaSoundDiskCache::failures.fetch_add(1);
			VitaSoundDiskCache::pendingBytes.fetch_sub(result->alen);
		});
}
#endif

// sounds mapped to soundBase enum
static const std::string soundsList[] = {
	"", // invalid
	"", // todo
	VCMI_SOUND_LIST
};
#undef VCMI_SOUND_NAME
#undef VCMI_SOUND_FILE

void CSoundHandler::onVolumeChange(const JsonNode & volumeNode)
{
	setVolume(volumeNode.Integer());
}

CSoundHandler::CSoundHandler():
	listener(settings.listen["general"]["sound"]),
	ambientConfig(JsonPath::builtin("config/ambientSounds.json"))
{
	listener(std::bind(&CSoundHandler::onVolumeChange, this, _1));
#ifdef VCMI_VITA
	ambientState = std::make_shared<AmbientState>();
	ambientState->owner = this;
#endif

	if(ambientConfig["allocateChannels"].isNumber())
		Mix_AllocateChannels(ambientConfig["allocateChannels"].Integer());

	if(isInitialized())
	{
		Mix_ChannelFinished([](int channel)
		{
			// It is possible for this code to be executed during ENGINE destruction.
			// In this scenario, ENGINE is already nullptr, but ~CSoundHandler is still running
			if (ENGINE)
				ENGINE->sound().soundFinishedCallback(channel);
		});
	}
}

void CSoundHandler::MixChunkDeleter::operator()(Mix_Chunk * ptr)
{
	Mix_FreeChunk(ptr);
}

CSoundHandler::~CSoundHandler()
{
#ifdef VCMI_VITA
	// GameEngine drains AsyncRunner before destroying the audio handlers.
	ambientState->owner = nullptr;
	ambientState.reset();
	VitaAmbientStats::pending.store(0);
	VitaAmbientStats::sourceBytes.store(0);
#endif
	if(isInitialized())
	{
		Mix_ChannelFinished(nullptr);
		Mix_HaltChannel(-1);

		soundChunks.clear();
		uncachedPlayingChunks.clear();
	}
}

Mix_Chunk * CSoundHandler::getSoundChunkCached(const AudioPath & sound)
{
#ifdef VCMI_VITA
	VitaBattleEvents::Scope event(VitaBattleEvents::SoundChunk);
#endif
	if (soundChunks.find(sound) == soundChunks.end())
		soundChunks[sound].first = getSoundChunk(sound);

	return soundChunks[sound].first.get();
}

// Allocate an SDL chunk and cache it.
CSoundHandler::MixChunkPtr CSoundHandler::getSoundChunk(const AudioPath & sound)
{
#ifdef VCMI_VITA
	VitaBattleEvents::Scope event(VitaBattleEvents::SoundDecode);
#endif
	try
	{
#ifdef VCMI_VITA
		auto data = [&]()
		{
			VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundRead);
			return CResourceHandler::get()->load(sound.addPrefix("SOUNDS/"))->readAll();
		}();
		VitaSoundDiskCache::Header signature{};
		if(VitaSoundDiskCache::enabled)
		{
			VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheRead);
			signature = VitaSoundDiskCache::key(data.first.get(), data.second);
			auto cached = VitaSoundDiskCache::readCandidates(signature, VitaSoundDiskCache::slots(signature));
			if(cached.chunk)
			{
				VitaSoundDiskCache::hits.fetch_add(1);
				if(cached.mono) VitaSoundDiskCache::monoHits.fetch_add(1);
				(cached.compressed ? VitaSoundDiskCache::compressedHits : VitaSoundDiskCache::rawHits).fetch_add(1);
				auto result = MixChunkPtr(cached.chunk.release(), MixChunkDeleter{});
				if(cached.legacy)
				{
					VitaSoundDiskCache::legacyHits.fetch_add(1);
					persistSoundChunk(signature, result);
				}
				return result;
			}
			VitaSoundDiskCache::misses.fetch_add(1);
		}
		Mix_Chunk * chunk = VitaMonoWave::enabled ? VitaMonoWave::load(data.first.get(), data.second) : nullptr;
		if(chunk)
			VitaMonoWave::hits.fetch_add(1, std::memory_order_relaxed);
		else
		{
			VitaMonoWave::fallback.fetch_add(1, std::memory_order_relaxed);
			VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::MixerLoad);
			chunk = Mix_LoadWAV_RW(SDL_RWFromMem(data.first.get(), data.second), 1);
		}
#else
		auto data = CResourceHandler::get()->load(sound.addPrefix("SOUNDS/"))->readAll();
		SDL_RWops * ops = SDL_RWFromMem(data.first.get(), data.second);
		Mix_Chunk * chunk = Mix_LoadWAV_RW(ops, 1); // will free ops
#endif
		if(!chunk) return {};
		auto result = MixChunkPtr(chunk, MixChunkDeleter{});
#ifdef VCMI_VITA
		persistSoundChunk(signature, result);
#endif
		return result;
	}
	catch(std::exception & e)
	{
		logGlobal->warn("Cannot get sound %s chunk: %s", sound.getOriginalName(), e.what());
		return nullptr;
	}
}

CSoundHandler::MixChunkPtr CSoundHandler::getSoundChunk(std::pair<std::unique_ptr<ui8[]>, si64> & data)
{
	try
	{
		std::vector<ui8> startBytes = std::vector<ui8>(data.first.get(), data.first.get() + std::min(static_cast<si64>(100), data.second));

		SDL_RWops * ops = SDL_RWFromMem(data.first.get(), data.second);
		Mix_Chunk * chunk = Mix_LoadWAV_RW(ops, 1); // will free ops
		if(!chunk) return {};
		return MixChunkPtr(chunk, MixChunkDeleter{});
	}
	catch(std::exception & e)
	{
		logGlobal->warn("Cannot get sound chunk: %s", e.what());
		return nullptr;
	}
}

int CSoundHandler::ambientDistToVolume(int distance) const
{
	const auto & distancesVector = ambientConfig["distances"].Vector();

	if(distance >= distancesVector.size())
		return 0;

	int volumeByDistance = static_cast<int>(distancesVector[distance].Integer());
	return volumeByDistance * ambientConfig["volume"].Integer() / 100;
}

void CSoundHandler::ambientStopSound(const AudioPath & soundId)
{
	stopSound(ambientChannels[soundId]);
	setChannelVolume(ambientChannels[soundId], volume);
}

uint32_t CSoundHandler::getSoundDurationMilliseconds(const AudioPath & sound)
{
	if(!isInitialized() || sound.empty())
		return 0;

	auto resourcePath = sound.addPrefix("SOUNDS/");

	if(!CResourceHandler::get()->existsResource(resourcePath))
		return 0;

	auto data = CResourceHandler::get()->load(resourcePath)->readAll();

	uint32_t milliseconds = 0;

	Mix_Chunk * chunk = Mix_LoadWAV_RW(SDL_RWFromMem(data.first.get(), data.second), 1);

	int freq = 0;
	Uint16 fmt = 0;
	int channels = 0;
	if(!Mix_QuerySpec(&freq, &fmt, &channels))
		return 0;

	if(chunk != nullptr)
	{
		Uint32 sampleSizeBytes = (fmt & 0xFF) / 8;
		Uint32 samples = (chunk->alen / sampleSizeBytes);
		Uint32 frames = (samples / channels);
		milliseconds = ((frames * 1000) / freq);

		Mix_FreeChunk(chunk);
	}

	return milliseconds;
}

// Plays a sound, and return its channel so we can fade it out later
int CSoundHandler::playSound(soundBase::soundID soundID)
{
	assert(soundID < soundBase::sound_after_last);
	auto sound = AudioPath::builtin(soundsList[soundID]);
	logGlobal->trace("Attempt to play sound %d with file name %s with cache", soundID, sound.getOriginalName());

	return playSoundImpl(sound, 0, true);
}

int CSoundHandler::playSoundLooped(const AudioPath & sound)
{
	return playSoundImpl(sound, -1, true);
}

int CSoundHandler::playSound(const AudioPath & sound)
{
	return playSoundImpl(sound, 0, false);
}

int CSoundHandler::playSoundImpl(const AudioPath & sound, int repeats, bool useCache)
{
#ifdef VCMI_VITA
	VitaBattleEvents::Scope event(VitaBattleEvents::SoundPlay);
#endif
	if(!isInitialized() || sound.empty())
		return -1;

	int channel;
	MixChunkPtr chunkPtr;
	Mix_Chunk * chunk = nullptr;
	if (!useCache)
	{
#ifdef VCMI_VITA
		{
			std::scoped_lock guard(oneShotMutex);
			auto found = std::find_if(oneShotChunks.begin(), oneShotChunks.end(), [&](const auto & entry) { return entry.first == sound; });
			if(found != oneShotChunks.end())
			{
				chunkPtr = found->second;
				oneShotChunks.splice(oneShotChunks.begin(), oneShotChunks, found);
				VitaBattleEvents::soundHits.fetch_add(1, std::memory_order_relaxed);
			}
		}
		if(!chunkPtr)
		{
			VitaBattleEvents::soundMisses.fetch_add(1, std::memory_order_relaxed);
			auto decoded = getSoundChunk(sound);
			std::scoped_lock guard(oneShotMutex);
			auto found = std::find_if(oneShotChunks.begin(), oneShotChunks.end(), [&](const auto & entry) { return entry.first == sound; });
			if(found != oneShotChunks.end())
			{
				chunkPtr = found->second;
				oneShotChunks.splice(oneShotChunks.begin(), oneShotChunks, found);
			}
			else
			{
				chunkPtr = std::move(decoded);
				constexpr size_t budget = 4 * 1024 * 1024;
				if(chunkPtr && chunkPtr->alen <= budget)
				{
					while(!oneShotChunks.empty() && (oneShotBytes + chunkPtr->alen > budget || oneShotChunks.size() >= 64))
					{
						oneShotBytes -= oneShotChunks.back().second->alen;
						oneShotChunks.pop_back();
						VitaBattleEvents::soundEvictions.fetch_add(1, std::memory_order_relaxed);
					}
					oneShotChunks.emplace_front(sound, chunkPtr);
					oneShotBytes += chunkPtr->alen;
					VitaBattleEvents::soundBytes.store(static_cast<uint32_t>(oneShotBytes), std::memory_order_relaxed);
				}
			}
		}
#else
		chunkPtr = getSoundChunk(sound);
#endif
		chunk = chunkPtr.get();
	}
	else
		chunk = getSoundChunkCached(sound);

	if(chunk)
	{
		channel = Mix_PlayChannel(-1, chunk, repeats);
		if(channel == -1)
		{
			logGlobal->error("Unable to play sound file %s , error %s", sound.getOriginalName(), Mix_GetError());
		}
		else
		{
			storeChunk(channel, std::move(chunkPtr));
			initCallback(channel);
		}
	}
	else
		channel = -1;

	return channel;
}

void CSoundHandler::storeChunk(int channel, MixChunkPtr chunk)
{
	std::scoped_lock lockGuard(mutexCallbacks);
	uncachedPlayingChunks[channel] = std::move(chunk);
}

int CSoundHandler::playSound(std::pair<std::unique_ptr<ui8[]>, si64> & data)
{
	int channel = -1;
	auto chunk = getSoundChunk(data);
	if(chunk)
	{
		channel = Mix_PlayChannel(-1, chunk.get(), 0);
		if(channel == -1)
		{
			logGlobal->error("Unable to play sound, error %s", Mix_GetError());
		}
		else
		{
			storeChunk(channel, std::move(chunk));
			initCallback(channel);
		}
	}
	return channel;
}

// Helper. Randomly select a sound from an array and play it
int CSoundHandler::playSoundFromSet(std::vector<soundBase::soundID> & sound_vec)
{
	return playSound(*RandomGeneratorUtil::nextItem(sound_vec, CRandomGenerator::getDefault()));
}

void CSoundHandler::stopSound(int handler)
{
	if(isInitialized() && handler != -1)
		Mix_HaltChannel(handler);
}

void CSoundHandler::pauseSound(int handler)
{
	if(isInitialized() && handler != -1)
		Mix_Pause(handler);
}

void CSoundHandler::resumeSound(int handler)
{
	if(isInitialized() && handler != -1)
		Mix_Resume(handler);
}

ui32 CSoundHandler::getVolume() const
{
	return volume;
}

// Sets the sound volume, from 0 (mute) to 100
void CSoundHandler::setVolume(ui32 percent)
{
	volume = std::min(100u, percent);

	if(isInitialized())
	{
		setChannelVolume(-1, volume);

		for(const auto & channel : channelVolumes)
			updateChannelVolume(channel.first);
	}
}

void CSoundHandler::updateChannelVolume(int channel)
{
	if(channelVolumes.count(channel))
		setChannelVolume(channel, getVolume() * channelVolumes[channel] / 100);
	else
		setChannelVolume(channel, getVolume());
}

// Sets the sound volume, from 0 (mute) to 100
void CSoundHandler::setChannelVolume(int channel, ui32 percent)
{
	Mix_Volume(channel, (MIX_MAX_VOLUME * percent) / 100);
}

void CSoundHandler::setCallback(int channel, std::function<void()> function)
{
	std::scoped_lock lockGuard(mutexCallbacks);

	auto iter = callbacks.find(channel);

	//channel not found. It may have finished so fire callback now
	if(iter == callbacks.end())
		function();
	else
		iter->second.push_back(function);
}

void CSoundHandler::resetCallback(int channel)
{
	std::scoped_lock lockGuard(mutexCallbacks);

	callbacks.erase(channel);
}

void CSoundHandler::soundFinishedCallback(int channel)
{
	std::scoped_lock lockGuard(mutexCallbacks);

	// SDL_mixer still reads the chunk after this callback returns, so it is freed later on the main thread
	auto chunk = uncachedPlayingChunks.find(channel);
	if(chunk != uncachedPlayingChunks.end())
	{
		std::shared_ptr<Mix_Chunk> finishedChunk = std::move(chunk->second);
		uncachedPlayingChunks.erase(chunk);
		ENGINE->dispatchMainThread([finishedChunk](){});
	}

	if(callbacks.count(channel) == 0)
		return;

	// store callbacks from container locally - SDL might reuse this channel for another sound
	// but do actually execution in separate thread, to avoid potential deadlocks in case if callback requires locks of its own
	auto callback = callbacks.at(channel);
	callbacks.erase(channel);

	if(!callback.empty())
	{
		ENGINE->dispatchMainThread(
			[callback]()
			{
				for(const auto & entry : callback)
					entry();
			}
			);
	}
}

void CSoundHandler::initCallback(int channel)
{
	std::scoped_lock lockGuard(mutexCallbacks);
	assert(callbacks.count(channel) == 0);
	callbacks[channel] = {};
}

void CSoundHandler::initCallback(int channel, const std::function<void()> & function)
{
	std::scoped_lock lockGuard(mutexCallbacks);
	assert(callbacks.count(channel) == 0);
	callbacks[channel].push_back(function);
}

int CSoundHandler::ambientGetRange() const
{
	return ambientConfig["range"].Integer();
}

#ifdef VCMI_VITA
// Caller holds mutex. Only one cache read may be outstanding.
bool CSoundHandler::queueAmbientSound(const AudioPath & sound)
{
	if(!ambientState->loading.empty())
		return true;
	try
	{
		auto job = std::make_shared<AmbientJob>();
		const auto prepareStarted = std::chrono::steady_clock::now();
		const auto resource = sound.addPrefix("SOUNDS/");
		const auto loaders = CResourceHandler::get()->getResourcesWithName(resource);
		if(!loaders.empty())
		{
			const auto * selected = loaders.back();
			if(const auto * archive = dynamic_cast<const CArchiveLoader *>(selected))
				job->open = archive->makeStreamOpener(resource);
			else if(const auto * files = dynamic_cast<const CFilesystemLoader *>(selected))
				if(auto path = files->getResourceName(resource))
					job->open = [file = *path]() { return std::make_unique<CFileInputStream>(file); };
		}
		VitaAmbientStats::sourcePrepare.add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - prepareStarted).count()));
		if(job->open)
		{
			std::weak_ptr<AmbientState> state = ambientState;
			auto * engine = ENGINE.get();
			ambientState->loading.emplace(sound, job);
			VitaAmbientStats::pending.store(1);
			VitaAmbientStats::sourceBytes.store(1024 * 1024);
			engine->async().run([engine, state, job, sound]()
			{
				try
				{
					constexpr si64 limit = 1024 * 1024;
					{
						VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundRead);
						job->stream = job->open();
						job->open = {};
						job->source.first.reset(new ui8[limit + 1]);
						while(job->source.second <= limit)
						{
							const auto request = std::min<si64>(16384, limit + 1 - job->source.second);
							const auto count = job->stream->read(job->source.first.get() + job->source.second, request);
							if(count < 0 || count > request)
								throw std::runtime_error("Ambient source returned invalid byte count");
							if(!count) break;
							job->source.second += count;
						}
						job->stream.reset();
					}
					if(job->source.second > 0 && job->source.second <= limit)
					{
						VitaMapWorkStats::Scope measure(VitaMapWorkStats::Kind::SoundCacheRead);
						job->signature = VitaSoundDiskCache::key(job->source.first.get(), job->source.second);
						job->result = VitaSoundDiskCache::readCandidates(job->signature, VitaSoundDiskCache::slots(job->signature));
					}
				}
				catch(const std::exception & error)
				{
					job->error = error.what();
				}
				job->stream.reset();
				job->source.first.reset();
				std::weak_ptr<AmbientJob> ready = job;
				engine->dispatchMainThread([state, ready, sound]()
				{
					auto live = state.lock();
					auto result = ready.lock();
					if(live && live->owner && result)
						live->owner->finishAmbientSound(sound, result);
				});
			});
			VitaAmbientStats::queued.fetch_add(1);
			return true;
		}
	}
	catch(const std::exception & error)
	{
		ambientState->loading.erase(sound);
		VitaAmbientStats::pending.store(0);
		VitaAmbientStats::sourceBytes.store(0);
		logGlobal->warn("Unable to queue ambient sound %s: %s", sound.getOriginalName(), error.what());
	}
	VitaAmbientStats::fallback.fetch_add(1);
	return false;
}

void CSoundHandler::finishAmbientSound(const AudioPath & sound, const std::shared_ptr<AmbientJob> & job)
{
	const auto started = std::chrono::steady_clock::now();
	std::map<AudioPath, int> wanted;
	{
		std::scoped_lock guard(mutex);
		auto found = ambientState->loading.find(sound);
		if(found == ambientState->loading.end() || found->second != job)
			return;
		ambientState->loading.erase(found);
		VitaAmbientStats::pending.store(0);
		VitaAmbientStats::sourceBytes.store(0);
		VitaAmbientStats::completed.fetch_add(1);
		if(!job->error.empty()) logGlobal->warn("Ambient cache load failed: %s", job->error);
		if(ambientState->wanted.count(sound) && !soundChunks.count(sound))
		{
			if(job->result.chunk)
			{
				auto & cached = job->result;
				MixChunkPtr chunk(cached.chunk.release(), MixChunkDeleter{});
				soundChunks[sound].first = chunk;
				VitaSoundDiskCache::hits.fetch_add(1);
				if(cached.mono) VitaSoundDiskCache::monoHits.fetch_add(1);
				(cached.compressed ? VitaSoundDiskCache::compressedHits : VitaSoundDiskCache::rawHits).fetch_add(1);
				if(cached.legacy)
				{
					VitaSoundDiskCache::legacyHits.fetch_add(1);
					persistSoundChunk(job->signature, chunk);
				}
			}
			else
			{
				VitaAmbientStats::fallback.fetch_add(1);
				getSoundChunkCached(sound);
			}
		}
		else VitaAmbientStats::discarded.fetch_add(1);
		wanted = ambientState->wanted;
	}
	ambientUpdateChannels(std::move(wanted));
	VitaAmbientStats::completion.add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()));
}
#endif

void CSoundHandler::ambientUpdateChannels(std::map<AudioPath, int> soundsArg)
{
	std::scoped_lock guard(mutex);
#ifdef VCMI_VITA
	ambientState->wanted = soundsArg;
#endif

	std::vector<AudioPath> stoppedSounds;
	for(const auto & pair : ambientChannels)
	{
		const auto & soundId = pair.first;
		const int channel = pair.second;

		if(!vstd::contains(soundsArg, soundId))
		{
			ambientStopSound(soundId);
			stoppedSounds.push_back(soundId);
		}
		else
		{
			int channelVolume = ambientDistToVolume(soundsArg[soundId]);
			channelVolumes[channel] = channelVolume;
			updateChannelVolume(channel);
		}
	}
	for(const auto & soundId : stoppedSounds)
	{
		channelVolumes.erase(ambientChannels[soundId]);
		ambientChannels.erase(soundId);
	}

	for(const auto & pair : soundsArg)
	{
		const auto & soundId = pair.first;
		const int distance = pair.second;

		if(!vstd::contains(ambientChannels, soundId))
		{
#ifdef VCMI_VITA
			if(VitaSoundDiskCache::enabled && isInitialized() && !soundId.empty() && !soundChunks.count(soundId) && queueAmbientSound(soundId))
				continue;
#endif
			int channel = playSoundLooped(soundId);
			int channelVolume = ambientDistToVolume(distance);
			channelVolumes[channel] = channelVolume;

			updateChannelVolume(channel);
			ambientChannels[soundId] = channel;
		}
	}
}

void CSoundHandler::ambientStopAllChannels()
{
	std::scoped_lock guard(mutex);
#ifdef VCMI_VITA
	ambientState->wanted.clear();
#endif

	for(const auto & ch : ambientChannels)
	{
		ambientStopSound(ch.first);
	}
	channelVolumes.clear();
	ambientChannels.clear();
}
