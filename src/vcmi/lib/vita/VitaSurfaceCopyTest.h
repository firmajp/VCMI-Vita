// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/* Part of VCMI engine, GNU GPL v2.0 or later. See license.txt. */
#pragma once
#include <SDL.h>
#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdio>

inline void runVitaSurfaceCopyTest()
{
	unsigned cases = 0;
	for(int width : {1, 3, 15, 16, 17, 31, 32, 63, 64, 65, 127, 255, 511, 800, 1066})
		for(bool clipped : {false, true})
		{
			const int sw = width + 3, dw = width + 7, sp = sw * 4 + 20, dp = dw * 4 + 12;
			std::vector<unsigned char> source(128 + sp * 9), dest(128 + dp * 11);
			for(size_t i = 0; i < source.size(); ++i) source[i] = static_cast<unsigned char>(i * 71 + 19);
			for(size_t i = 0; i < dest.size(); ++i) dest[i] = static_cast<unsigned char>(i * 37 + 113);
			auto expected = dest;
			auto * src = SDL_CreateRGBSurfaceWithFormatFrom(source.data() + 64, sw, 9, 32, sp, SDL_PIXELFORMAT_ARGB8888);
			auto * dst = SDL_CreateRGBSurfaceWithFormatFrom(dest.data() + 64, dw, 11, 32, dp, SDL_PIXELFORMAT_ARGB8888);
			if(!src || !dst) throw std::runtime_error("Vita copy self-test surface allocation failed");
			SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
			SDL_Rect clip = clipped ? SDL_Rect{5, 3, std::max(1, width - 3), 3} : SDL_Rect{0, 0, dw, 11};
			SDL_SetClipRect(dst, &clip);
			for(int y = 0; y < 5; ++y)
				for(int x = 0; x < width; ++x)
					if(x + 3 >= clip.x && x + 3 < clip.x + clip.w && y + 2 >= clip.y && y + 2 < clip.y + clip.h)
						std::memcpy(expected.data() + 64 + (y + 2) * dp + (x + 3) * 4, source.data() + 64 + (y + 1) * sp + (x + 1) * 4, 4);
			SDL_Rect sr{1, 1, width, 5}, dr{3, 2, width, 5};
			const int result = SDL_BlitSurface(src, &sr, dst, &dr);
			SDL_FreeSurface(src);
			SDL_FreeSurface(dst);
			if(result || dest != expected) throw std::runtime_error("Vita copy self-test clipped/padded pixels differ");
			++cases;
		}
	for(bool forward : {false, true})
	{
		constexpr int pitch = 304;
		std::vector<unsigned char> pixels(128 + pitch * 11);
		for(size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<unsigned char>(i * 31 + 17);
		const auto original = pixels;
		auto expected = pixels;
		SDL_Rect sr = forward ? SDL_Rect{3, 2, 60, 7} : SDL_Rect{0, 0, 60, 7};
		SDL_Rect dr = forward ? SDL_Rect{0, 0, 60, 7} : SDL_Rect{3, 2, 60, 7};
		for(int y = 0; y < sr.h; ++y)
			std::memcpy(expected.data() + 64 + (dr.y + y) * pitch + dr.x * 4,
				original.data() + 64 + (sr.y + y) * pitch + sr.x * 4, sr.w * 4);
		auto * surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels.data() + 64, 73, 11, 32, pitch, SDL_PIXELFORMAT_ARGB8888);
		if(!surface) throw std::runtime_error("Vita copy overlap surface allocation failed");
		SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
		const int result = SDL_BlitSurface(surface, &sr, surface, &dr);
		SDL_FreeSurface(surface);
		if(result || pixels != expected) throw std::runtime_error("Vita copy self-test overlap differs");
		++cases;
	}
	std::fprintf(stderr, "Vita surface copy self-test PASS cases=%u\n", cases);
}
