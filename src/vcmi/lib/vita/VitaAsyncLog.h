// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "VitaNetworkStats.h"
#include "../VCMIDirs.h"
#include <psp2/io/fcntl.h>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <system_error>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>
#include <string>
#include <algorithm>

inline bool vitaAsyncLogVerified = false;

class VitaAsyncLog
{
	static constexpr size_t blockSize = 64 * 1024, blockCount = 4;
	struct Block { std::array<char, blockSize> bytes; size_t length = 0; };
	std::array<Block, blockCount> blocks;
	std::array<char, blockSize> output;
	std::mutex mutex;
	std::condition_variable changed;
	std::thread writer;
	size_t head = 0, tail = 0, count = 0;
	bool stopping = false;
	SceUID descriptor = -1;
	FILE * stream = nullptr;
	std::atomic<uint32_t> queuedBytes{0}, writtenBytes{0}, droppedBytes{0}, errors{0};
	VitaNetworkStats::WorkCounter writeWork;

	int enqueue(const char * data, int length)
	{
		if(length <= 0) return length;
		const size_t required = (static_cast<size_t>(length) + blockSize - 1) / blockSize;
		{
			std::scoped_lock guard(mutex);
			if(stopping || required > blockCount - count)
			{
				droppedBytes.fetch_add(length);
				return length;
			}
			size_t remaining = length;
			while(remaining)
			{
				auto & block = blocks[tail];
				block.length = std::min(remaining, blockSize);
				std::memcpy(block.bytes.data(), data, block.length);
				queuedBytes.fetch_add(block.length);
				data += block.length; remaining -= block.length;
				tail = (tail + 1) % blockCount; ++count;
			}
		}
		changed.notify_one();
		return length;
	}
	void run()
	{
		for(;;)
		{
			size_t length;
			{
				std::unique_lock guard(mutex);
				changed.wait(guard, [this] { return stopping || count; });
				if(!count) break;
				const auto & block = blocks[head];
				length = block.length;
				std::memcpy(output.data(), block.bytes.data(), length);
				head = (head + 1) % blockCount; --count;
				queuedBytes.fetch_sub(length);
			}
			const auto started = std::chrono::steady_clock::now();
			size_t done = 0;
			while(done < length)
			{
				const auto actual = sceIoWrite(descriptor, output.data() + done, length - done);
				if(actual <= 0 || static_cast<size_t>(actual) > length - done) { errors.fetch_add(1); break; }
				done += actual; writtenBytes.fetch_add(actual);
			}
			writeWork.add(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()));
		}
		if(sceIoClose(descriptor) < 0) errors.fetch_add(1);
		descriptor = -1;
	}
	void stop()
	{
		{
			std::scoped_lock guard(mutex);
			stopping = true;
		}
		changed.notify_one();
		if(writer.joinable()) writer.join();
	}
	static int writeCallback(void * cookie, const char * data, int length)
	{
		return static_cast<VitaAsyncLog *>(cookie)->enqueue(data, length);
	}
	static int closeCallback(void * cookie)
	{
		auto * owner = static_cast<VitaAsyncLog *>(cookie);
		owner->stream = nullptr;
		owner->stop();
		return 0;
	}
public:
	struct Snapshot { uint32_t queued, written, dropped, errors, calls, us, maxUs; };
	explicit VitaAsyncLog(const char * path)
	{
		descriptor = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
		if(descriptor < 0) return;
		try { writer = std::thread([this] { run(); }); }
		catch(const std::system_error & error)
		{
			sceIoClose(descriptor); descriptor = -1;
			std::fprintf(stderr, "Vita async log worker unavailable: %s\n", error.what());
			return;
		}
		catch(const std::bad_alloc &)
		{
			sceIoClose(descriptor); descriptor = -1;
			std::fprintf(stderr, "Vita async log worker allocation failed\n");
			return;
		}
		stream = ::funopen(this, nullptr, writeCallback, nullptr, closeCallback);
		if(!stream) stop();
	}
	static std::unique_ptr<VitaAsyncLog> create(const char * path)
	{
		try { return std::make_unique<VitaAsyncLog>(path); }
		catch(const std::system_error & error)
		{
			std::fprintf(stderr, "Vita async log initialization failed: %s\n", error.what());
		}
		catch(const std::bad_alloc &)
		{
			std::fprintf(stderr, "Vita async log allocation failed\n");
		}
		return {};
	}
	VitaAsyncLog(const VitaAsyncLog &) = delete;
	VitaAsyncLog & operator=(const VitaAsyncLog &) = delete;
	~VitaAsyncLog()
	{
		if(stream) std::fclose(stream);
		stop();
	}
	FILE * get() const { return stream; }
	Snapshot snapshot() const
	{
		return {queuedBytes.load(), writtenBytes.load(), droppedBytes.load(), errors.load(), writeWork.calls.load(), writeWork.totalUs.load(), writeWork.maxUs.load()};
	}
};

inline void runVitaAsyncLogTest()
{
	const auto path = (VCMIDirs::get().userCachePath() / "async-log-selftest.bin").string();
	auto sink = VitaAsyncLog::create(path.c_str());
	std::unique_ptr<std::array<char, 64 * 1024>> buffer(new(std::nothrow) std::array<char, 64 * 1024>);
	if(!sink || !sink->get() || !buffer)
	{
		std::fprintf(stderr, "Vita async log self-test FAIL allocation_or_open\n");
		return;
	}
	std::string expected = "VCMI async log test 123\n";
	std::vector<char> payload(80 * 1024);
	for(size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(i * 73 + 19);
	expected.append(payload.data(), payload.size()); expected += "\nend\n";
	bool ok = std::setvbuf(sink->get(), buffer->data(), _IOFBF, buffer->size()) == 0;
	ok = ok && std::fprintf(sink->get(), "VCMI async log test %d\n", 123) > 0;
	ok = ok && std::fwrite(payload.data(), 1, payload.size(), sink->get()) == payload.size();
	ok = ok && std::fprintf(sink->get(), "\nend\n") > 0 && std::fflush(sink->get()) == 0;
	ok = ok && sink->snapshot().dropped == 0;
	sink.reset(); // fclose drains the worker before releasing the caller's stdio buffer.
	std::vector<char> actual(expected.size());
	FILE * file = std::fopen(path.c_str(), "rb");
	if(file)
	{
		ok = ok && std::fread(actual.data(), 1, actual.size(), file) == actual.size() && std::fgetc(file) == EOF && !std::ferror(file);
		std::fclose(file);
		ok = ok && std::memcmp(actual.data(), expected.data(), actual.size()) == 0;
	}
	else ok = false;
	std::remove(path.c_str());
	vitaAsyncLogVerified = ok;
	std::fprintf(stderr, "Vita async log self-test %s bytes=%u\n", ok ? "PASS" : "FAIL", static_cast<unsigned>(expected.size()));
}
