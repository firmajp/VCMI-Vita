// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
// Vita update 2026-11-01; see MODIFICATIONS.md.
/* Part of VCMI engine, GNU GPL v2.0 or later. See license.txt. */
#pragma once
#include <SDL.h>
#include <SDL_image.h>
#include <chrono>
#include <cstdio>

namespace VitaBootScreen
{
inline SDL_Window * window = nullptr;
inline SDL_Renderer * renderer = nullptr;
inline SDL_Texture * background = nullptr;
inline bool imageAttempted = false;
inline bool active = false;

inline void finish()
{
	if(background) SDL_DestroyTexture(background);
	background = nullptr;
	imageAttempted = false;
	active = false;
}

inline void discard()
{
	finish();
	if(renderer) SDL_DestroyRenderer(renderer);
	if(window) SDL_DestroyWindow(window);
	renderer = nullptr;
	window = nullptr;
}

inline void draw(SDL_Renderer * target)
{
	if(!background && !imageAttempted)
	{
		imageAttempted = true;
		auto * image = IMG_Load("app0:config/vitaBoot.png");
		if(image)
		{
			background = SDL_CreateTextureFromSurface(target, image);
			SDL_FreeSurface(image);
		}
		if(!background) std::fprintf(stderr, "Vita boot artwork fallback: %s\n", SDL_GetError());
	}
	SDL_SetRenderDrawColor(target, 13, 7, 24, 255);
	SDL_RenderClear(target);
	if(background) SDL_RenderCopy(target, background, nullptr, nullptr);
}

inline void drawLoading(SDL_Renderer * target)
{
	draw(target);
	int width = 0, height = 0;
	SDL_RenderGetLogicalSize(target, &width, &height);
	SDL_BlendMode previousBlend = SDL_BLENDMODE_NONE;
	SDL_GetRenderDrawBlendMode(target, &previousBlend);
	SDL_SetRenderDrawBlendMode(target, SDL_BLENDMODE_BLEND);
	SDL_Rect card{width * 8 / 100, height / 2, width * 65 / 100, height * 43 / 100};
	SDL_SetRenderDrawColor(target, 13, 10, 29, 218);
	SDL_RenderFillRect(target, &card);
	SDL_SetRenderDrawColor(target, 212, 160, 91, 190);
	SDL_RenderDrawRect(target, &card);
	SDL_SetRenderDrawBlendMode(target, previousBlend);
}

inline void begin()
{
	const auto start = std::chrono::steady_clock::now();
	SDL_SetMainReady();
	SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
	SDL_SetHint(SDL_HINT_VITA_TOUCH_MOUSE_DEVICE, "0");
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		std::fprintf(stderr, "Vita boot splash init fallback: %s\n", SDL_GetError());
		return;
	}
	window = SDL_CreateWindow("VCMI", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 960, 544, SDL_WINDOW_FULLSCREEN);
	if(window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!renderer)
	{
		std::fprintf(stderr, "Vita boot splash renderer fallback: %s\n", SDL_GetError());
		discard();
		SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
		return;
	}
	SDL_RenderSetLogicalSize(renderer, 960, 544);
	active = true;
	draw(renderer);
	SDL_RenderPresent(renderer);
	std::fprintf(stderr, "Vita boot splash: first_frame_ms=%lld\n", static_cast<long long>(
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()));
}
}
