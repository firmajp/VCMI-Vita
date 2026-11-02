/*
 * SDLImage.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "VitaRenderProfiling.h"
#include "SDLImage.h"
#include "RenderHandler.h"

#include "SDLImageLoader.h"
#include "SDLImageScaler.h"
#include "SDL_Extensions.h"

#include "render/ColorFilter.h"
#include "render/CBitmapHandler.h"
#include "render/CDefFile.h"
#include "GameEngine.h"
#include "IScreenHandler.h"

#include "lib/AsyncRunner.h"
#include "lib/CConfigHandler.h"
#include "lib/texts/TextOperations.h"

#include <tbb/parallel_for.h>

#include <SDL_image.h>
#include <SDL_surface.h>
#include <SDL_version.h>
#ifdef VCMI_VITA
#include "lib/vita/VitaOpaqueCacheStats.h"
#include "lib/vita/VitaBinaryAlphaStats.h"
#include "lib/vita/VitaTileImageStats.h"
#include <list>
#include <unordered_map>
#include <array>
#include <cstring>

namespace
{
struct OpaqueImageCache
{
	struct Entry
	{
		std::shared_ptr<const SDLImageShared> owner;
		SDL_Surface * converted;
		std::array<SDL_Color, 256> palette{};
		size_t cost;
		Entry(std::shared_ptr<const SDLImageShared> owner, SDL_Surface * converted, SDL_Palette * palette, size_t cost)
			: owner(std::move(owner)), converted(converted), cost(cost)
		{
			std::memcpy(this->palette.data(), palette->colors, 256 * sizeof(SDL_Color));
		}
		~Entry() { SDL_FreeSurface(converted); }
	};
	std::list<Entry> entries;

	SDL_Surface * get(const SDLImageShared & owner, SDL_Surface * source)
	{
		if(SDL_HasColorKey(source) || source->format->palette->ncolors != 256)
		{
			++VitaOpaqueCacheStats::bypass;
			return nullptr;
		}
		auto * palette = source->format->palette;
		for(int i = 0; i < 256; ++i)
			if(palette->colors[i].a != 255)
			{
				++VitaOpaqueCacheStats::bypass;
				return nullptr;
			}
		for(auto it = entries.begin(); it != entries.end(); ++it)
			if(it->owner.get() == &owner)
			{
				if(std::memcmp(it->palette.data(), palette->colors, 256 * sizeof(SDL_Color)) != 0)
				{
					++VitaOpaqueCacheStats::bypass;
					return nullptr;
				}
				++VitaOpaqueCacheStats::hits;
				auto * converted = it->converted;
				entries.splice(entries.begin(), entries, it);
				return converted;
			}
		auto retained = owner.weak_from_this().lock();
		constexpr size_t budget = 8 * 1024 * 1024;
		const size_t required = static_cast<size_t>(source->pitch) * source->h + static_cast<size_t>(source->w) * source->h * 4 + 2048;
		if(!retained || required > budget)
			return nullptr;
		while(!entries.empty() && (VitaOpaqueCacheStats::bytes + required > budget || entries.size() >= 8))
		{
			VitaOpaqueCacheStats::bytes -= entries.back().cost;
			entries.pop_back();
			++VitaOpaqueCacheStats::evictions;
		}
		std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> converted(SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_ARGB8888, 0), SDL_FreeSurface);
		if(!converted)
			return nullptr;
		SDL_SetSurfaceBlendMode(converted.get(), SDL_BLENDMODE_NONE);
		const size_t cost = static_cast<size_t>(source->pitch) * source->h + static_cast<size_t>(converted->pitch) * converted->h + 2048;
		if(cost > required)
		{
			return nullptr;
		}
		entries.emplace_front(std::move(retained), converted.get(), palette, cost);
		auto * result = converted.release();
		VitaOpaqueCacheStats::bytes += cost;
		++VitaOpaqueCacheStats::builds;
		return result;
	}
} opaqueImageCache;

struct BinaryAlphaCache
{
	using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
	struct Entry
	{
		std::weak_ptr<const SDLImageShared> owner;
		const SDLImageShared * identity;
		Surface mapped{nullptr, SDL_FreeSurface};
		std::array<SDL_Color, 256> palette;
		size_t cost;
	};
	std::list<Entry> entries;
	std::unordered_map<const SDLImageShared *, std::list<Entry>::iterator> index;

	void clear()
	{
		index.clear();
		entries.clear();
		VitaBinaryAlphaStats::entries = 0;
		VitaBinaryAlphaStats::bytes = 0;
	}

	SDL_Surface * get(const SDLImageShared & owner, SDL_Surface * source)
	{
		auto * palette = source->format->palette;
		if(!palette || palette->ncolors != 256 || source->format->BitsPerPixel != 8 || SDL_MUSTLOCK(source))
			return nullptr;
		if(auto found = index.find(&owner); found != index.end())
		{
			auto it = found->second;
			if(it->owner.expired())
			{
				VitaBinaryAlphaStats::bytes -= it->cost;
				entries.erase(it);
				index.erase(found);
				VitaBinaryAlphaStats::entries = entries.size();
				++VitaBinaryAlphaStats::evictions;
			}
			else
			{
				// Overlay colors can change without incrementing SDL's palette version.
				if(std::memcmp(it->palette.data(), palette->colors, sizeof(it->palette)) != 0)
				{
					for(int i = 0; i < 256; ++i)
						if(it->palette[i].a != palette->colors[i].a)
							return nullptr;
					if(SDL_SetPaletteColors(it->mapped->format->palette, palette->colors, 0, 256) != 0)
						return nullptr;
					std::memcpy(it->palette.data(), palette->colors, sizeof(it->palette));
					++VitaBinaryAlphaStats::paletteUpdates;
				}
				auto * result = it->mapped.get();
				entries.splice(entries.begin(), entries, it);
				++VitaBinaryAlphaStats::hits;
				return result;
			}
		}
		int key = -1;
		for(int i = 0; i < 256; ++i)
		{
			if(palette->colors[i].a != 0 && palette->colors[i].a != 255)
				return nullptr;
			if(key < 0 && palette->colors[i].a == 0)
				key = i;
		}
		constexpr size_t budget = 2 * 1024 * 1024;
		const size_t required = static_cast<size_t>(source->w + 3) * source->h + 4096;
		auto lifetime = owner.weak_from_this();
		if(lifetime.expired() || required > budget)
			return nullptr;
		while(!entries.empty() && (VitaBinaryAlphaStats::bytes + required > budget || entries.size() >= 256))
		{
			VitaBinaryAlphaStats::bytes -= entries.back().cost;
			index.erase(entries.back().identity);
			entries.pop_back();
			VitaBinaryAlphaStats::entries = entries.size();
			++VitaBinaryAlphaStats::evictions;
		}
		Surface mapped(SDL_CreateRGBSurfaceWithFormat(0, source->w, source->h, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
		if(!mapped || SDL_SetPaletteColors(mapped->format->palette, palette->colors, 0, 256) != 0
			|| SDL_SetSurfaceBlendMode(mapped.get(), SDL_BLENDMODE_NONE) != 0
			|| (key >= 0 && SDL_SetColorKey(mapped.get(), SDL_TRUE, key) != 0))
			return nullptr;
		const size_t cost = static_cast<size_t>(mapped->pitch) * mapped->h + 4096;
		if(cost > required)
			return nullptr;
		for(int y = 0; y < source->h; ++y)
		{
			const auto * from = static_cast<const uint8_t *>(source->pixels) + y * source->pitch;
			auto * to = static_cast<uint8_t *>(mapped->pixels) + y * mapped->pitch;
			for(int x = 0; x < source->w; ++x)
				to[x] = palette->colors[from[x]].a == 0 ? static_cast<uint8_t>(key) : from[x];
		}
		Entry entry{std::move(lifetime), &owner, std::move(mapped), {}, cost};
		std::memcpy(entry.palette.data(), palette->colors, sizeof(entry.palette));
		entries.push_front(std::move(entry));
		index.emplace(&owner, entries.begin());
		VitaBinaryAlphaStats::entries = entries.size();
		VitaBinaryAlphaStats::bytes += cost;
		++VitaBinaryAlphaStats::builds;
		return entries.front().mapped.get();
	}
} binaryAlphaCache;
struct TileImageCache
{
	using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
	struct Entry
	{
		std::weak_ptr<const SDLImageShared> owner;
		const SDLImageShared * identity;
		Surface converted{nullptr, SDL_FreeSurface};
		std::array<SDL_Color, 256> palette;
		int key;
	};
	static constexpr size_t cost = 32 * 32 * 4 + 2048;
	std::list<Entry> entries;
	std::unordered_map<const SDLImageShared *, std::list<Entry>::iterator> index;

	void clear()
	{
		index.clear(); entries.clear();
		VitaTileImageStats::bytes = VitaTileImageStats::entries = 0;
	}
	void erase(std::list<Entry>::iterator entry)
	{
		index.erase(entry->identity);
		entries.erase(entry);
		VitaTileImageStats::bytes -= cost;
		VitaTileImageStats::entries = entries.size();
		++VitaTileImageStats::evictions;
	}
	SDL_Surface * get(const SDLImageShared & owner, SDL_Surface * source)
	{
		if(source->w != 32 || source->h != 32 || source->format->BitsPerPixel != 8 || !source->format->palette
			|| source->format->palette->ncolors != 256 || SDL_MUSTLOCK(source)) return nullptr;
		Uint32 colorKey = 0;
		const int key = SDL_GetColorKey(source, &colorKey) == 0 ? static_cast<int>(colorKey) : -1;
		if(key > 255) return nullptr;
		auto * palette = source->format->palette;
		if(auto found = index.find(&owner); found != index.end())
		{
			auto entry = found->second;
			if(!entry->owner.expired() && entry->key == key && std::memcmp(entry->palette.data(), palette->colors, sizeof(entry->palette)) == 0)
			{
				auto * result = entry->converted.get();
				entries.splice(entries.begin(), entries, entry);
				++VitaTileImageStats::hits;
				return result;
			}
			erase(entry);
		}
		for(int i = 0; i < 256; ++i) if(palette->colors[i].a != 255) return nullptr;
		auto lifetime = owner.weak_from_this();
		if(lifetime.expired()) return nullptr;
		while(!entries.empty() && (VitaTileImageStats::bytes + cost > 1024 * 1024 || entries.size() >= 256)) erase(std::prev(entries.end()));
		Surface converted(SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		if(!converted || SDL_SetSurfaceBlendMode(converted.get(), SDL_BLENDMODE_NONE) != 0) return nullptr;
		std::array<Uint32, 256> lookup{};
		for(int i = 0; i < 256; ++i)
		{
			const auto & color = palette->colors[i];
			lookup[i] = SDL_MapRGBA(converted->format, color.r, color.g, color.b, 255);
		}
		Uint32 transparentColor = 0xff000000;
		while(std::find(lookup.begin(), lookup.end(), transparentColor) != lookup.end()) ++transparentColor;
		bool containsKey = false;
		for(int y = 0; y < 32; ++y)
		{
			const auto * from = static_cast<const Uint8 *>(source->pixels) + y * source->pitch;
			auto * to = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(converted->pixels) + y * converted->pitch);
			for(int x = 0; x < 32; ++x)
			{
				const bool transparent = from[x] == key;
				containsKey |= transparent;
				to[x] = transparent ? transparentColor : lookup[from[x]];
			}
		}
		if(containsKey && SDL_SetColorKey(converted.get(), SDL_TRUE, transparentColor) != 0) return nullptr;
		Entry entry{std::move(lifetime), &owner, std::move(converted), {}, key};
		std::memcpy(entry.palette.data(), palette->colors, sizeof(entry.palette));
		entries.push_front(std::move(entry));
		index.emplace(&owner, entries.begin());
		VitaTileImageStats::bytes += cost; VitaTileImageStats::entries = entries.size();
		++VitaTileImageStats::builds;
		(containsKey ? VitaTileImageStats::keyedBuilds : VitaTileImageStats::solidBuilds)++;
		return entries.front().converted.get();
	}
} tileImageCache;

}

void runVitaTileImageTest()
{
	bool ok = true;
	unsigned cases = 0;
	for(int pattern = 0; pattern < 4; ++pattern)
	{
		TileImageCache::Surface source(SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
		TileImageCache::Surface expected(SDL_CreateRGBSurfaceWithFormat(0, 41, 43, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		TileImageCache::Surface actual(SDL_CreateRGBSurfaceWithFormat(0, 41, 43, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		if(!source || !expected || !actual) { ok = false; break; }
		auto owner = std::make_shared<SDLImageShared>(source.get());
		std::array<SDL_Color, 256> colors{};
		for(int i = 0; i < 256; ++i) colors[i] = SDL_Color{static_cast<Uint8>(i * 13), static_cast<Uint8>(i * 37), static_cast<Uint8>(i * 71), 255};
		const int key = pattern == 0 ? -1 : pattern == 2 ? 7 : 0;
		if(key >= 0) { colors[25] = colors[key]; SDL_SetColorKey(source.get(), SDL_TRUE, key); }
		SDL_SetSurfaceBlendMode(source.get(), SDL_BLENDMODE_NONE);
		for(int y = 0; y < 32; ++y)
			for(int x = 0; x < 32; ++x)
				static_cast<Uint8 *>(source->pixels)[y * source->pitch + x] = pattern == 3 ? 8 + (x + y * 32) % 248 : (x + y * 32) % 256;
		for(int pass = 0; pass < 2; ++pass)
		{
			if(pass) for(auto & color : colors) color.r ^= 0x5a;
			SDL_SetPaletteColors(source->format->palette, colors.data(), 0, 256);
			auto * cached = tileImageCache.get(*owner, source.get());
			if(!cached) { ok = false; break; }
			for(int placement = 0; placement < 3; ++placement)
			{
				const size_t size = static_cast<size_t>(expected->pitch) * expected->h;
				std::memset(expected->pixels, 0x65, size); std::memset(actual->pixels, 0x65, size);
				SDL_Rect clip{5, 3, 27, 31};
				SDL_SetClipRect(expected.get(), placement ? &clip : nullptr); SDL_SetClipRect(actual.get(), placement ? &clip : nullptr);
				Rect sr = placement ? Rect(2, 3, 25, 19) : Rect(0, 0, 32, 32);
				Point dest(placement == 2 ? -1 : 3, placement == 2 ? -2 : 4);
				CSDL_Ext::blitSurface(source.get(), sr, expected.get(), dest);
				CSDL_Ext::blitSurface(cached, sr, actual.get(), dest);
				if(std::memcmp(expected->pixels, actual->pixels, size) != 0) ok = false;
				++cases;
			}
		}
		colors[9].a = 128; SDL_SetPaletteColors(source->format->palette, colors.data(), 0, 256);
		if(tileImageCache.get(*owner, source.get())) ok = false;
		++cases;
		tileImageCache.clear();
	}
	tileImageCache.clear();
	VitaTileImageStats::hits = VitaTileImageStats::builds = VitaTileImageStats::bypass = VitaTileImageStats::evictions = 0;
	VitaTileImageStats::solidBuilds = VitaTileImageStats::keyedBuilds = 0;
	VitaTileImageStats::enabled = ok;
	std::fprintf(stderr, "Vita opaque tile self-test %s cases=%u enabled=%d\n", ok ? "PASS" : "FAIL", cases, ok);
}

void runVitaBinaryAlphaTest()
{
	bool ok = true;
	unsigned cases = 0;
	for(int pattern = 0; pattern < 4 && ok; ++pattern)
	{
		BinaryAlphaCache::Surface source(SDL_CreateRGBSurfaceWithFormat(0, 17, 11, 8, SDL_PIXELFORMAT_INDEX8), SDL_FreeSurface);
		BinaryAlphaCache::Surface expected(SDL_CreateRGBSurfaceWithFormat(0, 23, 15, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		BinaryAlphaCache::Surface actual(SDL_CreateRGBSurfaceWithFormat(0, 23, 15, 32, SDL_PIXELFORMAT_ARGB8888), SDL_FreeSurface);
		if(!source || !expected || !actual) { ok = false; break; }
		auto owner = std::make_shared<SDLImageShared>(source.get());
		for(int i = 0; i < 256; ++i)
		{
			const bool transparent = pattern == 0 ? i == 0 : pattern == 1 ? i % 5 == 0 : pattern == 2 ? i == 7 : false;
			source->format->palette->colors[i] = SDL_Color{static_cast<uint8_t>(i * 13), static_cast<uint8_t>(i * 37), static_cast<uint8_t>(i * 71), static_cast<uint8_t>(transparent ? 0 : 255)};
		}
		for(int y = 0; y < source->h; ++y)
			for(int x = 0; x < source->w; ++x)
				static_cast<uint8_t *>(source->pixels)[y * source->pitch + x] = static_cast<uint8_t>(x + y * 17);
		for(int pass = 0; pass < 2 && ok; ++pass)
		{
			if(pass)
				for(int i = 0; i < 256; ++i)
					source->format->palette->colors[i].r ^= 0x5a;
			auto * mapped = binaryAlphaCache.get(*owner, source.get());
			if(!mapped) { ok = false; break; }
			for(int placement = 0; placement < 3; ++placement)
			{
				const size_t size = static_cast<size_t>(expected->pitch) * expected->h;
				std::memset(expected->pixels, 0x65, size);
				std::memset(actual->pixels, 0x65, size);
				SDL_Rect clip{3, 2, 16, 10};
				SDL_SetClipRect(expected.get(), placement ? &clip : nullptr);
				SDL_SetClipRect(actual.get(), placement ? &clip : nullptr);
				Rect sr = placement ? Rect(1, 1, 15, 9) : Rect(0, 0, 17, 11);
				Point dest(placement == 2 ? -2 : 5, placement == 2 ? -1 : 4);
				CSDL_Ext::blit8bppAlphaTo24bpp(source.get(), sr, expected.get(), dest, 255);
				SDL_Rect s = CSDL_Ext::toSDL(sr), d{dest.x, dest.y, sr.w, sr.h};
				if(SDL_BlitSurface(mapped, &s, actual.get(), &d) != 0 || std::memcmp(expected->pixels, actual->pixels, size) != 0)
					ok = false;
				++cases;
			}
		}
		source->format->palette->colors[1].a = source->format->palette->colors[1].a == 0 ? 255 : 0;
		if(binaryAlphaCache.get(*owner, source.get())) ok = false;
		++cases;
		source->format->palette->colors[1].a = 128;
		if(binaryAlphaCache.get(*owner, source.get())) ok = false;
		++cases;
		binaryAlphaCache.clear();
	}
	binaryAlphaCache.clear();
	VitaBinaryAlphaStats::hits = VitaBinaryAlphaStats::builds = VitaBinaryAlphaStats::bypass = 0;
	VitaBinaryAlphaStats::evictions = VitaBinaryAlphaStats::paletteUpdates = 0;
	VitaBinaryAlphaStats::enabled = ok;
	std::fprintf(stderr, "Vita binary alpha self-test %s cases=%u cache_enabled=%d\n", ok ? "PASS" : "FAIL", cases, ok);
}
#endif

class SDLImageLoader;

int IImage::width() const
{
	return dimensions().x;
}

int IImage::height() const
{
	return dimensions().y;
}

SDLImageShared::SDLImageShared(const CDefFile * data, size_t frame, size_t group)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	SDLImageLoader loader(this);
	data->loadFrame(frame, group, loader);

	savePalette();
}

SDLImageShared::SDLImageShared(SDL_Surface * from)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	surf = from;
	if (surf == nullptr)
		return;

	savePalette();

	surf->refcount++;
	fullSize.x = surf->w;
	fullSize.y = surf->h;
}

SDLImageShared::SDLImageShared(const ImagePath & filename, bool optimizeImage)
	: surf(nullptr),
	margins(0, 0),
	fullSize(0, 0),
	originalPalette(nullptr)
{
	surf = BitmapHandler::loadBitmap(filename);

	if(surf == nullptr)
	{
		logGlobal->error("Error: failed to load image %s", filename.getOriginalName());
		return;
	}
	else
	{
		savePalette();
		fullSize.x = surf->w;
		fullSize.y = surf->h;

		if(optimizeImage)
			optimizeSurface();
	}
}

void SDLImageShared::scaledDraw(SDL_Surface * where, SDL_Palette * palette, const Point & scaleTo, const Point & dest, const Rect * src, const ColorRGBA & colorMultiplier, uint8_t alpha, EImageBlitMode mode) const
{
#ifdef VCMI_VITA
	VitaRenderProfiling::Scope profile(VitaRenderProfiling::Kind::ScaledDraw);
#endif
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return;

	Rect sourceRect(0, 0, surf->w, surf->h);
	Point destShift(0, 0);
	Point destScale = Point(surf->w, surf->h) * scaleTo / dimensions();
	Point marginsScaled = margins * scaleTo / dimensions();

	if(src)
	{
		Rect srcUnscaled(Point(src->topLeft() * dimensions() / scaleTo), Point(src->dimensions() * dimensions() / scaleTo));

		if(srcUnscaled.x < margins.x)
			destShift.x += marginsScaled.x - src->x;

		if(srcUnscaled.y < margins.y)
			destShift.y += marginsScaled.y - src->y;

		sourceRect = Rect(srcUnscaled).intersect(Rect(margins.x, margins.y, surf->w, surf->h));

		destScale.x = std::min(destScale.x, sourceRect.w * scaleTo.x / dimensions().x);
		destScale.y = std::min(destScale.y, sourceRect.h * scaleTo.y / dimensions().y);

		sourceRect -= margins;
	}
	else
		destShift = marginsScaled;

	destShift += dest;

	SDL_SetSurfaceColorMod(surf, colorMultiplier.r, colorMultiplier.g, colorMultiplier.b);
	SDL_SetSurfaceAlphaMod(surf, alpha);

	if (alpha != SDL_ALPHA_OPAQUE || (mode != EImageBlitMode::OPAQUE && surf->format->Amask != 0))
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
	else
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);

	if (palette && surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, palette);
	}

	SDL_Rect srcRect = CSDL_Ext::toSDL(sourceRect);
	SDL_Rect dstRect = CSDL_Ext::toSDL(Rect(destShift, destScale));

	if (sourceRect.dimensions() * scaleTo / dimensions() != destScale)
		logGlobal->info("???");

#ifdef VCMI_VITA
	const bool direct = !surf->format->palette && surf->format->format == where->format->format;
	SDL_Surface * tempSurface = surf;
	if (!direct)
	{
		VitaRenderProfiling::Scope conversion(VitaRenderProfiling::Kind::SurfaceConvert);
		tempSurface = SDL_ConvertSurface(surf, where->format, 0);
	}
#else
	SDL_Surface * tempSurface = SDL_ConvertSurface(surf, where->format, 0);
#endif
	if (!tempSurface)
		throw std::runtime_error(SDL_GetError());
	int result = SDL_BlitScaled(tempSurface, &srcRect, where, &dstRect);
#ifdef VCMI_VITA
	if (!direct)
#endif
	SDL_FreeSurface(tempSurface);
	if (result != 0)
		logGlobal->error("SDL_BlitScaled failed! %s", SDL_GetError());

	if (surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, originalPalette);
	}
}

void SDLImageShared::draw(SDL_Surface * where, SDL_Palette * palette, const Point & dest, const Rect * src, const ColorRGBA & colorMultiplier, uint8_t alpha, EImageBlitMode mode) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return;

	Rect sourceRect(0, 0, surf->w, surf->h);

	Point destShift(0, 0);

	if(src)
	{
		if(src->x < margins.x)
			destShift.x += margins.x - src->x;

		if(src->y < margins.y)
			destShift.y += margins.y - src->y;

		sourceRect = Rect(*src).intersect(Rect(margins.x, margins.y, surf->w, surf->h));

		sourceRect -= margins;
	}
	else
		destShift = margins;

	destShift += dest;

	SDL_SetSurfaceColorMod(surf, colorMultiplier.r, colorMultiplier.g, colorMultiplier.b);
	SDL_SetSurfaceAlphaMod(surf, alpha);

	if (alpha != SDL_ALPHA_OPAQUE || (mode != EImageBlitMode::OPAQUE && surf->format->Amask != 0))
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
	else
		SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);

	if (palette && surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, palette);
	}

	if(surf->format->palette && mode != EImageBlitMode::OPAQUE && mode != EImageBlitMode::COLORKEY)
	{
#ifdef VCMI_VITA
		SDL_Surface * mapped = nullptr;
		const bool body = mode == EImageBlitMode::ONLY_BODY_HIDE_SELECTION || mode == EImageBlitMode::ONLY_BODY_HIDE_FLAG_COLOR
			|| mode == EImageBlitMode::ONLY_BODY_IGNORE_OVERLAY || mode == EImageBlitMode::GRAYSCALE_BODY_HIDE_SELECTION;
		if(body && VitaBinaryAlphaStats::enabled && VitaRenderProfiling::active && alpha == 255
			&& colorMultiplier.r == 255 && colorMultiplier.g == 255 && colorMultiplier.b == 255
			&& where->format->format == SDL_PIXELFORMAT_ARGB8888 && sourceRect.w > 0 && sourceRect.h > 0)
		{
			VitaRenderProfiling::Scope blit(VitaRenderProfiling::Kind::ImageBlit);
			mapped = binaryAlphaCache.get(*this, surf);
			if(mapped)
				CSDL_Ext::blitSurface(mapped, sourceRect, where, destShift);
			else
				++VitaBinaryAlphaStats::bypass;
		}
		if(!mapped)
#endif
		CSDL_Ext::blit8bppAlphaTo24bpp(surf, sourceRect, where, destShift, alpha);
	}
	else
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope blit(VitaRenderProfiling::Kind::ImageBlit);
		SDL_Surface * selected = surf;
		if(mode == EImageBlitMode::OPAQUE && alpha == 255 && colorMultiplier.r == 255 && colorMultiplier.g == 255 && colorMultiplier.b == 255
			&& surf->format->BitsPerPixel == 8 && surf->format->palette && where->format->format == SDL_PIXELFORMAT_ARGB8888
			&& static_cast<size_t>(surf->w) * surf->h >= 65536 && ENGINE->amIGuiThread())
		{
			if(auto * converted = opaqueImageCache.get(*this, surf))
				selected = converted;
		}
		if(mode == EImageBlitMode::OPAQUE && VitaTileImageStats::enabled && VitaRenderProfiling::active && alpha == 255
			&& colorMultiplier.r == 255 && colorMultiplier.g == 255 && colorMultiplier.b == 255
			&& surf->w == 32 && surf->h == 32 && surf->format->BitsPerPixel == 8 && where->format->format == SDL_PIXELFORMAT_ARGB8888)
		{
			if(auto * converted = tileImageCache.get(*this, surf)) selected = converted;
			else ++VitaTileImageStats::bypass;
		}
		CSDL_Ext::blitSurface(selected, sourceRect, where, destShift);
#else
		CSDL_Ext::blitSurface(surf, sourceRect, where, destShift);
#endif
	}

	if (surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, originalPalette);
	}
}

void SDLImageShared::optimizeSurface()
{
	assert(upscalingInProgress == false);
	if (!surf)
		return;

	SDLImageOptimizer optimizer(surf, Rect(margins, fullSize));

	optimizer.optimizeSurface(surf);
	SDL_FreeSurface(surf);

	surf = optimizer.acquireResultSurface();
	margins = optimizer.getResultDimensions().topLeft();
	fullSize = optimizer.getResultDimensions().dimensions();
}

std::shared_ptr<const ISharedImage> SDLImageShared::scaleInteger(int factor, SDL_Palette * palette, EImageBlitMode mode) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (factor <= 0)
		throw std::runtime_error("Unable to scale by integer value of " + std::to_string(factor));

	if (!surf)
		return shared_from_this();

	if (palette && surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, palette);
	}

	// simple heuristics to differentiate tileable UI elements from map object / combat assets
	EScalingAlgorithm algorithm;
	if (mode == EImageBlitMode::OPAQUE || mode == EImageBlitMode::COLORKEY || mode == EImageBlitMode::SIMPLE)
		algorithm = EScalingAlgorithm::XBRZ_OPAQUE;
	else
		algorithm = EScalingAlgorithm::XBRZ_ALPHA;

	auto result = SDLImageShared::createScaled(this, factor, algorithm);

	if (surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, originalPalette);
	}

	return result;
}

std::shared_ptr<SDLImageShared> SDLImageShared::createScaled(const SDLImageShared * from, int integerScaleFactor, EScalingAlgorithm algorithm)
{
	auto self = std::make_shared<SDLImageShared>(nullptr);
	self->upscalingInProgress = true;

	auto scaler = std::make_shared<SDLImageScaler>(from->surf, Rect(from->margins, from->fullSize), true);

	const auto & scalingTask = [self, algorithm, scaler]()
	{
		scaler->scaleSurfaceIntegerFactor(ENGINE->screenHandler().getScalingFactor(), algorithm);

		if (ENGINE->screenHandler().isSharpeningEnabled())
			scaler->sharpenResult(ENGINE->screenHandler().getSharpeningStrength());

		self->surf = scaler->acquireResultSurface();
		self->fullSize = scaler->getResultDimensions().dimensions();
		self->margins = scaler->getResultDimensions().topLeft();
		self->upscalingInProgress = false;
	};

	if(settings["video"]["asyncUpscaling"].Bool() && from->getAsyncUpscale())
		ENGINE->async().run(scalingTask);
	else
		scalingTask();

	return self;
}

bool SDLImageShared::isLoading() const
{
	return upscalingInProgress;
}

void SDLImageShared::setAsyncUpscale(bool on)
{
	asyncUpscale = on;
}

bool SDLImageShared::getAsyncUpscale() const
{
	return asyncUpscale;
}

std::shared_ptr<const ISharedImage> SDLImageShared::scaleTo(const Point & size, SDL_Palette * palette) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (palette && surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, palette);
	}

	SDLImageScaler scaler(surf, Rect(margins, fullSize), true);

	scaler.scaleSurface(size, EScalingAlgorithm::XBRZ_ALPHA);

	auto scaled = scaler.acquireResultSurface();

	if (scaled->format && scaled->format->palette) // fix color keying, because SDL loses it at this point
		CSDL_Ext::setColorKey(scaled, scaled->format->palette->colors[0]);
	else if(scaled->format && scaled->format->Amask)
		SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);//just in case
	else
		CSDL_Ext::setDefaultColorKey(scaled);//just in case

	auto ret = std::make_shared<SDLImageShared>(scaled);
	ret->fullSize = scaler.getResultDimensions().dimensions();
	ret->margins = scaler.getResultDimensions().topLeft();

	// erase our own reference
	SDL_FreeSurface(scaled);

	if (surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, originalPalette);
	}

	return ret;
}

void SDLImageShared::exportBitmap(const boost::filesystem::path& path, SDL_Palette * palette) const
{
	auto directory = path;
	directory.remove_filename();
	boost::filesystem::create_directories(directory);

	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return;

	if (palette && surf->format->palette)
	{
#ifdef VCMI_VITA
		VitaRenderProfiling::Scope bind(VitaRenderProfiling::Kind::PaletteBind);
#endif
		SDL_SetSurfacePalette(surf, palette);
	}
	IMG_SavePNG(surf, TextOperations::filesystemPathToUtf8(path).c_str());
	if (palette && surf->format->palette)
		SDL_SetSurfacePalette(surf, originalPalette);
}

bool SDLImageShared::isTransparent(const Point & coords) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return true;

	Point test = coords - margins;

	if (test.x < 0 || test.y < 0 || test.x >= surf->w || test.y >= surf->h)
		return true;

	SDL_Color color;
	SDL_GetRGBA(CSDL_Ext::getPixel(surf, test.x, test.y), surf->format, &color.r, &color.g, &color.b, &color.a);

	bool pixelTransparent = color.a < 128;
	bool pixelCyan = (color.r == 0 && color.g == 255 && color.b == 255);

	return pixelTransparent || pixelCyan;
}

Rect SDLImageShared::contentRect() const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return Rect();

	return Rect(margins, Point(surf->w, surf->h));
}

const SDL_Palette * SDLImageShared::getPalette() const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return nullptr;
	return surf->format->palette;
}

Point SDLImageShared::dimensions() const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	return fullSize;
}

std::shared_ptr<const ISharedImage> SDLImageShared::horizontalFlip() const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return shared_from_this();

	SDL_Surface * flipped = CSDL_Ext::horizontalFlip(surf);
	auto ret = std::make_shared<SDLImageShared>(flipped);
	ret->fullSize = fullSize;
	ret->margins.x = margins.x;
	ret->margins.y = fullSize.y - surf->h - margins.y;
	ret->fullSize = fullSize;

	// erase our own reference
	SDL_FreeSurface(flipped);

	return ret;
}

std::shared_ptr<const ISharedImage> SDLImageShared::verticalFlip() const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return shared_from_this();

	SDL_Surface * flipped = CSDL_Ext::verticalFlip(surf);
	auto ret = std::make_shared<SDLImageShared>(flipped);
	ret->fullSize = fullSize;
	ret->margins.x = fullSize.x - surf->w - margins.x;
	ret->margins.y = margins.y;
	ret->fullSize = fullSize;

	// erase our own reference
	SDL_FreeSurface(flipped);

	return ret;
}

std::shared_ptr<SDLImageShared> SDLImageShared::generateCropped(const std::function<SDL_Surface *(SDL_Surface *)> & generate, int border, double shearX) const
{
	// Shadows are downscaled vertically by 4 with a nearest-neighbour sampling grid, which starts at the top of the processed surface.
	// A cut at another position or height moves the grid and the shadow of the frame jumps, so top edge and height stay multiples of 4.
	// The sheared shadow moves to the left by up to shearX * its height.
	const SDLImageMargins keep{border + static_cast<int>(std::ceil(shearX * (surf->h - 1))), border, border, border};

	SDLImageOptimizer optimizer(surf, Rect(margins, fullSize));
	optimizer.optimizeSurface(surf, keep, 4);
	SDL_Surface * cut = optimizer.acquireResultSurface();
	SDL_Surface * generated = cut ? generate(cut) : nullptr;
	SDL_FreeSurface(cut);

	auto ret = std::make_shared<SDLImageShared>(generated);
	ret->fullSize = fullSize;
	ret->margins = optimizer.getResultDimensions().topLeft();
	ret->optimizeSurface();

	// erase our own reference
	SDL_FreeSurface(generated);

	return ret;
}

std::shared_ptr<SDLImageShared> SDLImageShared::drawShadow(bool doSheer) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return nullptr;

	// border: the blur reaches 1 pixel
	return generateCropped([doSheer](SDL_Surface * source) { return CSDL_Ext::drawShadow(source, doSheer); }, 4, doSheer ? CSDL_Ext::shadowShearX : 0.0);
}

std::shared_ptr<SDLImageShared> SDLImageShared::drawOutline(const ColorRGBA & color, int thickness) const
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	if (!surf)
		return nullptr;

	SDL_Color sdlColor = { color.r, color.g, color.b, color.a };
	return generateCropped([sdlColor, thickness](SDL_Surface * source) { return CSDL_Ext::drawOutline(source, sdlColor, thickness); }, thickness + 3, 0.0);
}

void SDLImageShared::setMargins(const Point & newMargins)
{
	margins = newMargins;
}

void SDLImageShared::setFullSize(const Point & newSize)
{
	fullSize = newSize;
}

// Keep the original palette, in order to do color switching operation
void SDLImageShared::savePalette()
{
	if(upscalingInProgress)
		throw std::runtime_error("Attempt to access images that is still being loaded!");

	// For some images that don't have palette, skip this
	if(surf->format->palette == nullptr)
		return;

	if(originalPalette == nullptr)
		originalPalette = SDL_AllocPalette(surf->format->palette->ncolors);

	SDL_SetPaletteColors(originalPalette, surf->format->palette->colors, 0, surf->format->palette->ncolors);
}

SDLImageShared::~SDLImageShared()
{
	SDL_FreeSurface(surf);
	if (originalPalette)
		SDL_FreePalette(originalPalette);
}
