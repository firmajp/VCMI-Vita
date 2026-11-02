// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>

namespace VitaVisibilityCache
{
inline bool enabled = false;
inline uint64_t hits = 0, misses = 0;
class Cache
{
	struct Entry
	{
		int x = 0, y = 0, z = 0;
		uint32_t generation = 0;
		bool visible = false;
	};
	std::array<Entry, 64 * 64> entries{};
	const void * context = nullptr;
	uint32_t generation = 0;
public:
	void begin(const void * currentContext)
	{
		context = currentContext;
		if(++generation == 0)
		{
			for(auto & entry : entries) entry.generation = 0;
			generation = 1;
		}
	}
	template<typename Loader>
	bool get(const void * currentContext, int x, int y, int z, Loader load)
	{
		if(!generation || context != currentContext)
			return load();
		auto & entry = entries[(static_cast<uint32_t>(y) & 63) * 64 + (static_cast<uint32_t>(x) & 63)];
		if(entry.generation == generation && entry.x == x && entry.y == y && entry.z == z)
		{
			++hits;
			return entry.visible;
		}
		++misses;
		const bool visible = load();
		entry = {x, y, z, generation, visible};
		return visible;
	}
};
inline void selfTest()
{
	try
	{
		auto cache = std::make_unique<Cache>();
		int contexts[2]{};
		unsigned calls = 0;
		bool visible = true;
		auto load = [&]() { ++calls; return visible; };
		bool ok = cache->get(contexts, 0, 0, 0, load);
		cache->begin(contexts);
		ok = ok && cache->get(contexts, 0, 0, 0, load) && cache->get(contexts, 0, 0, 0, load) && calls == 2;
		visible = false;
		cache->begin(contexts);
		ok = ok && !cache->get(contexts, 0, 0, 0, load);
		for(int pass = 0; pass < 4; ++pass)
		{
			const void * context = &contexts[pass & 1];
			cache->begin(context);
			for(int z = 0; z < 2; ++z)
				for(int y = -2; y < 68; ++y)
					for(int x = -2; x < 68; ++x)
					{
						const bool expected = ((x + y + z + pass + x / 64 + y / 64) & 1) != 0;
						auto source = [expected]() { return expected; };
						ok = (cache->get(context, x, y, z, source) == expected) && ok;
						ok = (cache->get(context, x, y, z, source) == expected) && ok;
					}
		}
		cache->begin(&contexts[0]);
		visible = true;
		ok = ok && cache->get(&contexts[0], 0, 0, 0, load);
		visible = false;
		ok = ok && !cache->get(&contexts[1], 0, 0, 0, load);
		enabled = ok;
	}
	catch(const std::bad_alloc &)
	{
		enabled = false;
	}
	hits = misses = 0;
	std::fprintf(stderr, "Vita visibility cache self-test %s entries=4096 bytes=%u enabled=%d\n", enabled ? "PASS" : "FAIL", static_cast<unsigned>(sizeof(Cache)), enabled);
}
}
