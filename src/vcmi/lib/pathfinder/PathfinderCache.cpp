/*
 * PathfinderCache.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PathfinderCache.h"

#include "CGPathNode.h"
#include "PathfinderOptions.h"

#include "../callback/IGameInfoCallback.h"
#include "../mapObjects/CGHeroInstance.h"
#ifdef VCMI_VITA
#include <chrono>
#include <cstdio>
#endif

std::shared_ptr<PathfinderConfig> PathfinderCache::createConfig(const CGHeroInstance * h, CPathsInfo & out)
{
	auto config = std::make_shared<SingleHeroPathfinderConfig>(out, *cb, h);
	config->options = options;

	return config;
}

std::shared_ptr<CPathsInfo> PathfinderCache::buildPaths(const CGHeroInstance * h)
{
#ifdef VCMI_VITA
	const auto started = std::chrono::steady_clock::now();
#endif
	auto result = acquirePaths(h);
	auto config = createConfig(h, *result);

	cb->calculatePaths(config);
#ifdef VCMI_VITA
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
	if (milliseconds >= 50)
		std::fprintf(stderr, "Vita pathfinder: build_ms=%lld hero=%d\n", static_cast<long long>(milliseconds), h->id.getNum());
#endif
	return result;
}

std::shared_ptr<CPathsInfo> PathfinderCache::acquirePaths(const CGHeroInstance * h)
{
	if(reusablePaths.empty())
		return std::make_shared<CPathsInfo>(cb->getMapSize(), h);

	auto result = std::move(reusablePaths.back());
	reusablePaths.pop_back();
	result->prepareForReuse(h);
	return result;
}

PathfinderCache::PathfinderCache(const IGameInfoCallback * cb, const PathfinderOptions & options)
	: cb(cb)
	, options(options)
{
}

void PathfinderCache::invalidatePaths()
{
	std::lock_guard lock(pathCacheMutex);
	reusablePaths.reserve(reusablePaths.size() + pathCache.size());
	for(auto & entry : pathCache)
	{
		if(entry.second.use_count() == 1)
			reusablePaths.push_back(std::move(entry.second));
	}
	pathCache.clear();
}

std::shared_ptr<const CPathsInfo> PathfinderCache::getPathsInfo(const CGHeroInstance * h)
{
	std::lock_guard lock(pathCacheMutex);

	auto iter = pathCache.find(h);
	if(iter == std::end(pathCache) || iter->second->heroBonusTreeVersion != h->getTreeVersion())
	{
		if(iter != std::end(pathCache) && iter->second.use_count() == 1)
		{
			reusablePaths.reserve(reusablePaths.size() + 1);
			reusablePaths.push_back(std::move(iter->second));
			pathCache.erase(iter);
		}

		auto result = buildPaths(h);
		pathCache[h] = result;

		return result;
	}
	else
		return iter->second;
}
