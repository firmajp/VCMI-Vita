/*
 * GameEngine.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
// Vita update 2026-11-01; see MODIFICATIONS.md.
#include "StdInc.h"
#include "GameEngine.h"
#ifdef VCMI_VITA
#include "VitaVersion.h"
#include "VitaRuntimeDiagnostics.h"
#include "VitaRenderProfiling.h"
#include "../lib/vita/VitaOpaqueCacheStats.h"
#include "../lib/vita/VitaBattleEvents.h"
#include "../lib/vita/VitaTownCacheStats.h"
#include <malloc.h>
#include "../lib/vita/VitaUidDiagnostics.h"
#include "../lib/vita/VitaBonusLocks.h"
#include <psp2/ctrl.h>
#include <typeinfo>
#include <psp2/kernel/sysmem.h>
#endif
#include "GameLibrary.h"
#include "Discord.h"

#include "gui/CIntObject.h"
#include "gui/CursorHandler.h"
#include "gui/ShortcutHandler.h"
#include "gui/FramerateManager.h"
#include "gui/WindowHandler.h"
#include "gui/EventDispatcher.h"
#include "events/InputHandler.h"

#include "media/CMusicHandler.h"
#include "media/CSoundHandler.h"
#include "media/CVideoHandler.h"
#include "media/CEmptyVideoPlayer.h"

#include "adventureMap/AdventureMapInterface.h"
#include "render/Canvas.h"
#include "render/Colors.h"
#include "render/IFont.h"
#include "render/EFont.h"
#include "render/ScreenHandler.h"
#include "render/RenderHandler.h"
#include "GameEngineUser.h"
#include "battle/BattleInterface.h"

#include "../lib/AsyncRunner.h"
#include "../lib/CConfigHandler.h"
#include "../lib/texts/TextOperations.h"
#include "../lib/texts/CGeneralTextHandler.h"

std::unique_ptr<GameEngine> ENGINE;

static thread_local bool inGuiThread = false;

#ifdef VCMI_VITA
namespace
{
class VitaFrameProfiler
{
	FILE * log = nullptr;
	uint64_t frequency = SDL_GetPerformanceFrequency();
	uint64_t reportStart = 0, frameStart = 0, phaseStart = 0;
	uint64_t maximumFrame = 0;
	std::array<uint64_t, 13> phaseTicks{}, framePhaseTicks{};
	uint64_t lastHitch = 0;
	unsigned frameCount = 0, overBudget = 0;
	unsigned sceneChanges = 0, sampleCount = 0, droppedSamples = 0;
	uint64_t lastFinish = 0;
	std::array<uint64_t, 1024> frameIntervals{};
	VitaRenderProfiling::Counters renderCounters{};
	std::array<uint64_t, static_cast<size_t>(VitaRenderProfiling::Kind::Count)> frameScopeStart{};
	VitaRuntime::Phase currentPhase = VitaRuntime::Idle;
	const char * sceneName = "unknown";
public:
	void scene(const char * name)
	{
		if (name == sceneName) return;
		sceneName = name;
		++sceneChanges;
		if (log)
		{
			std::fprintf(log, "scene_change ticks=%u scene=%s\n", SDL_GetTicks(), sceneName);
			std::fflush(log);
		}
	}
	void begin()
	{
		frameStart = phaseStart = SDL_GetPerformanceCounter();
		VitaRenderProfiling::active = &renderCounters;
		for (size_t i = 0; i < renderCounters.size(); ++i)
			frameScopeStart[i] = renderCounters[i].ticks;
		framePhaseTicks.fill(0);
		if (!reportStart)
		{
			reportStart = frameStart;
			log = std::fopen(VCMI_VITA_PERFORMANCE_LOG, "w");
			if (log) std::fprintf(log, "VCMI " VCMI_VITA_VERSION "; target=30 FPS; times in ms; averages per frame\n");
		}
		currentPhase = VitaRuntime::FetchInput;
	}
	void phase(VitaRuntime::Phase next)
	{
		const auto now = SDL_GetPerformanceCounter();
		phaseTicks.at(currentPhase) += now - phaseStart;
		framePhaseTicks.at(currentPhase) += now - phaseStart;
		phaseStart = now;
		currentPhase = next;
	}
	void finish()
	{
		VitaRenderProfiling::active = nullptr;
		const auto now = SDL_GetPerformanceCounter();
		phaseTicks.at(currentPhase) += now - phaseStart;
		framePhaseTicks.at(currentPhase) += now - phaseStart;
		const auto duration = now - frameStart;
		const auto interval = lastFinish ? now - lastFinish : duration;
		lastFinish = now;
		if (sampleCount < frameIntervals.size()) frameIntervals[sampleCount++] = interval;
		else ++droppedSamples;
		maximumFrame = std::max(maximumFrame, duration);
		++frameCount;
		if (duration > frequency * 35 / 1000) ++overBudget;
		if (log && duration > frequency * 80 / 1000 && now - lastHitch >= frequency)
		{
			std::fprintf(log, "hitch ticks=%u total_ms=%.2f scene=%s", SDL_GetTicks(), 1000.0 * duration / frequency, sceneName);
			for (size_t i = 1; i < framePhaseTicks.size(); ++i)
				std::fprintf(log, " phase%u=%.3f", static_cast<unsigned>(i), 1000.0 * framePhaseTicks[i] / frequency);
			std::fprintf(log, "\n");
			for (size_t i = static_cast<size_t>(VitaRenderProfiling::Kind::PaletteBind) + 1; i < renderCounters.size(); ++i)
			{
				const auto elapsed = renderCounters[i].ticks - frameScopeStart[i];
				if (elapsed)
					std::fprintf(log, "hitch_scope name=%s measured_ms=%.3f\n", VitaRenderProfiling::names[i], 1000.0 * elapsed / frequency);
			}
			std::fflush(log);
			lastHitch = now;
		}
		if (now - reportStart < frequency * 10) return;
		if (log)
		{
			static constexpr std::array<const char *, 13> names{
				"idle", "input_poll", "interface_lock", "reclaim", "game_update", "input_dispatch",
				"layers", "draw", "upload", "callbacks", "cursor", "present", "limiter"};
			std::fprintf(log, "ticks=%u frames=%u fps=%.2f max_frame=%.2f frames_over_35ms=%u",
				SDL_GetTicks(), frameCount, static_cast<double>(frameCount) * frequency / (now - reportStart),
				1000.0 * maximumFrame / frequency, overBudget);
			for (size_t i = 1; i < names.size(); ++i)
				std::fprintf(log, " %s=%.3f", names[i], 1000.0 * phaseTicks[i] / frequency / frameCount);
			std::fprintf(log, " scene=%s scene_changes=%u\n", sceneName, sceneChanges);
			std::fprintf(log, "frame_samples_us ticks=%u values=", SDL_GetTicks());
			for (unsigned i = 0; i < sampleCount; ++i)
				std::fprintf(log, "%s%llu", i ? "," : "", static_cast<unsigned long long>(frameIntervals[i] * 1000000 / frequency));
			std::fprintf(log, "\n");
			std::sort(frameIntervals.begin(), frameIntervals.begin() + sampleCount);
			const auto percentileMs = [&](unsigned percent) {
				return 1000.0 * frameIntervals[(sampleCount * percent + 99) / 100 - 1] / frequency;
			};
			const unsigned slowCount = (sampleCount + 99) / 100;
			uint64_t slowTicks = 0;
			for (unsigned i = sampleCount - slowCount; i < sampleCount; ++i) slowTicks += frameIntervals[i];
			const auto heap = mallinfo();
			std::fprintf(log, "distribution samples=%u dropped=%u p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f low1_fps=%.3f heap_used=%d heap_free_arena=%d scene_changes=%u scene=%s\n",
				sampleCount, droppedSamples, percentileMs(50), percentileMs(95), percentileMs(99),
				slowTicks ? static_cast<double>(slowCount) * frequency / slowTicks : 0.0,
				heap.uordblks, heap.fordblks, sceneChanges, sceneName);
			for (size_t i = 0; i < renderCounters.size(); ++i)
			{
				const auto & counter = renderCounters[i];
				const double measuredMs = 1000.0 * counter.ticks / frequency;
				std::fprintf(log, "render_scope name=%s calls=%llu samples=%llu sampled_ms=%.3f estimated_ms_per_frame=%.3f sampling=%s max_sample_ms=%.3f\n",
					VitaRenderProfiling::names[i], static_cast<unsigned long long>(counter.calls),
					static_cast<unsigned long long>(counter.samples), measuredMs,
					counter.samples ? measuredMs * counter.calls / counter.samples / frameCount : 0.0,
					i <= static_cast<size_t>(VitaRenderProfiling::Kind::PaletteBind) ? "random1_16" : "exact",
					1000.0 * counter.maxTicks / frequency);
			}
			std::fprintf(log, "opaque_cache hits=%llu builds=%llu bypass=%llu evictions=%llu bytes=%u\n",
				static_cast<unsigned long long>(VitaOpaqueCacheStats::hits), static_cast<unsigned long long>(VitaOpaqueCacheStats::builds),
				static_cast<unsigned long long>(VitaOpaqueCacheStats::bypass), static_cast<unsigned long long>(VitaOpaqueCacheStats::evictions),
				static_cast<unsigned>(VitaOpaqueCacheStats::bytes));
			std::fprintf(log, "redraw_batch deferred_gui=%u requests=%u coalesced=%u\n",
				VitaBattleEvents::deferredGUI.load(), VitaBattleEvents::requests.load(), VitaBattleEvents::coalesced.load());
			std::fprintf(log, "sound_cache hits=%u misses=%u evictions=%u bytes=%u\n",
				VitaBattleEvents::soundHits.load(), VitaBattleEvents::soundMisses.load(), VitaBattleEvents::soundEvictions.load(), VitaBattleEvents::soundBytes.load());
			std::fprintf(log, "font_palette updates=%u unchanged=%u\n",
				VitaBattleEvents::fontPaletteUpdates.load(), VitaBattleEvents::fontPaletteUnchanged.load());
			std::fprintf(log, "pending_prune full=%u ancestor=%u inactive=%u\n",
				VitaBattleEvents::pendingFull.load(), VitaBattleEvents::pendingAncestor.load(), VitaBattleEvents::pendingInactive.load());
			std::fprintf(log, "panel_paint full=%u border=%u\n",
				VitaBattleEvents::panelFull.load(), VitaBattleEvents::panelBorder.load());
			std::fprintf(log, "scene_redraw map=%u town=%u\n",
				VitaBattleEvents::deferredMap.load(), VitaBattleEvents::deferredTown.load());
			std::fprintf(log, "town_cache hits=%llu builds=%llu bypass=%llu failures=%llu bytes=%u rebuild_ms=%.3f max_rebuild_ms=%.3f copy_ms=%.3f scope=gui_cumulative\n",
				static_cast<unsigned long long>(VitaTownCacheStats::hits), static_cast<unsigned long long>(VitaTownCacheStats::builds),
				static_cast<unsigned long long>(VitaTownCacheStats::bypass), static_cast<unsigned long long>(VitaTownCacheStats::failures),
				VitaTownCacheStats::bytes.load(std::memory_order_relaxed), VitaTownCacheStats::rebuildUs / 1000.0,
				VitaTownCacheStats::maxRebuildUs / 1000.0, VitaTownCacheStats::copyUs / 1000.0);
			for(size_t i = 0; i < VitaBattleEvents::counters.size(); ++i)
			{
				const auto & c = VitaBattleEvents::counters[i];
				std::fprintf(log, "battle_event name=%s calls=%u total_ms=%.3f max_ms=%.3f scope=all_threads_cumulative\n",
					VitaBattleEvents::names[i], c.calls.load(), c.totalUs.load() / 1000.0, c.maxUs.load() / 1000.0);
			}
			std::fflush(log);
		}
		phaseTicks.fill(0);
		renderCounters = {};
		sampleCount = droppedSamples = sceneChanges = 0;
		frameCount = overBudget = 0;
		maximumFrame = 0;
		reportStart = SDL_GetPerformanceCounter();
	}
};
VitaFrameProfiler vitaFrameProfiler;
}
#endif

ObjectConstruction::ObjectConstruction(CIntObject *obj)
{
	ENGINE->createdObj.push_front(obj);
	ENGINE->captureChildren = true;
}

ObjectConstruction::~ObjectConstruction()
{
	assert(!ENGINE->createdObj.empty());
	ENGINE->createdObj.pop_front();
	ENGINE->captureChildren = !ENGINE->createdObj.empty();
}

GameEngine::GameEngine()
	: captureChildren(false)
	, fakeStatusBar(std::make_shared<EmptyStatusBar>())
{
	inGuiThread = true;

	eventDispatcherInstance = std::make_unique<EventDispatcher>();
	windowHandlerInstance = std::make_unique<WindowHandler>();
	screenHandlerInstance = std::make_unique<ScreenHandler>();
	renderHandlerInstance = std::make_unique<RenderHandler>();
	shortcutsHandlerInstance = std::make_unique<ShortcutHandler>();
	inputHandlerInstance = std::make_unique<InputHandler>(); // Must be after windowHandlerInstance and shortcutsHandlerInstance
#ifdef VCMI_VITA
	framerateManagerInstance = std::make_unique<FramerateManager>(30);
#else
	framerateManagerInstance = std::make_unique<FramerateManager>(settings["video"]["targetfps"].Integer());
#endif

	discordInstance = std::make_unique<Discord>();

#ifndef ENABLE_VIDEO
	videoPlayerInstance = std::make_unique<CEmptyVideoPlayer>();
#else
	if (settings["session"]["disableVideo"].Bool())
		videoPlayerInstance = std::make_unique<CEmptyVideoPlayer>();
	else
		videoPlayerInstance = std::make_unique<CVideoPlayer>();
#endif

	soundPlayerInstance = std::make_unique<CSoundHandler>();
	musicPlayerInstance = std::make_unique<CMusicHandler>();
	sound().setVolume(settings["general"]["sound"].Integer());
	music().setVolume(settings["general"]["music"].Integer());
	cursorHandlerInstance = std::make_unique<CursorHandler>();

	asyncTasks = std::make_unique<AsyncRunner>();
}

void GameEngine::handleEvents()
{
	events().dispatchTimer(framerate().consumeElapsedMilliseconds());

	//player interface may want special event handling
	if(engineUser->capturedAllEvents())
		return;

	input().processEvents();
}

void GameEngine::fakeMouseMove()
{
	dispatchMainThread([](){
		ENGINE->events().dispatchMouseMoved(Point(0, 0), ENGINE->getCursorPosition());
	});
}

[[noreturn]] void GameEngine::mainLoop()
{
#ifdef VCMI_VITA
	static std::once_flag watchdogStarted;
	std::call_once(watchdogStarted, []()
	{
		std::thread([]()
		{
			FILE * log = std::fopen(VCMI_VITA_WATCHDOG_LOG, "w");
			if (log) std::setvbuf(log, nullptr, _IONBF, 0);
			Uint32 nextReport = 0;
			unsigned dumpHold = 0;
			for (;;)
			{
				SDL_Delay(250);
				if (!VitaRuntime::enabled.load(std::memory_order_acquire)) continue;
				const Uint32 now = SDL_GetTicks();
				const Uint32 last = VitaRuntime::lastProgress.load(std::memory_order_acquire);
				const auto phase = VitaRuntime::phase.load(std::memory_order_relaxed);
				if (log && now >= nextReport)
				{
					SceKernelFreeMemorySizeInfo memory{};
					memory.size = sizeof(memory);
					sceKernelGetFreeMemorySize(&memory);
					std::fprintf(log, "ticks=%u phase=%u idle_ms=%u frames=%u free_user=%d free_cdram=%d\n", now, phase, now-last, VitaRuntime::frames.load(), memory.size_user, memory.size_cdram);
					std::fprintf(log, "bonus_user_locks=%u\n", static_cast<unsigned>(VitaBonusLocks::live.load()));
					nextReport = now + 10000;
				}
				SceCtrlData pad{};
				const unsigned mask = SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER | SCE_CTRL_START;
				const bool held = sceCtrlPeekBufferPositive(0, &pad, 1) > 0 && (pad.buttons & mask) == mask;
				dumpHold = held ? dumpHold + 1 : 0;
				if (dumpHold >= 8)
				{
					if (log) std::fprintf(log, "MANUAL DUMP: phase=%u idle_ms=%u frames=%u\n", phase, now-last, VitaRuntime::frames.load());
					std::abort();
				}
			}
		}).detach();
	});
	VitaUidDiagnostics::report("main_loop");
	VitaRuntime::enabled.store(true, std::memory_order_release);
#endif
	for (;;)
	{
#ifdef VCMI_VITA
		vitaFrameProfiler.begin();
		vitaFrameProfiler.phase(VitaRuntime::FetchInput);
		VitaRuntime::mark(VitaRuntime::FetchInput);
#endif
		input().fetchEvents();

#ifndef VCMI_VITA
		// A frame with nothing new to show would still hold interfaceMutex, which is the lock the
		// network thread needs to produce the next one. The time bound is only a safety net.
		const auto now = std::chrono::steady_clock::now();

		bool wantsFrame;
		{
			std::scoped_lock interfaceLock(interfaceMutex);
			wantsFrame = engineUser->wantsFrameRendered();
		}
		if(!wantsFrame && now - lastFrameRendered < maxFrameSkipDuration)
		{
			// Nothing to show yet - sleep 1ms, just enough to stop the SDL_PollEvent busy-spin.
			// Not framerateDelay(): it pads to a full frame every call, delaying the netpack
			// this skip exists to keep fast.
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		lastFrameRendered = now;
#endif
		updateFrame();
#ifdef VCMI_VITA
		vitaFrameProfiler.phase(VitaRuntime::Present);
		VitaRuntime::mark(VitaRuntime::Present);
#endif
		screenHandlerInstance->presentScreenTexture();
#ifdef VCMI_VITA
		VitaRuntime::frames.fetch_add(1, std::memory_order_relaxed);
		vitaFrameProfiler.phase(VitaRuntime::FrameDelay);
		VitaRuntime::mark(VitaRuntime::FrameDelay);
#endif
		framerate().framerateDelay(); // holds a constant FPS
#ifdef VCMI_VITA
		vitaFrameProfiler.finish();
#endif
	}
}

void GameEngine::updateFrame()
{
#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::UpdateLock);
	VitaRuntime::mark(VitaRuntime::UpdateLock);
#endif
	std::scoped_lock interfaceLock(ENGINE->interfaceMutex);

	// under the lock: netpacks load and re-cache the same assets from the network thread
#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::ReclaimAssets);
	VitaRuntime::mark(VitaRuntime::ReclaimAssets);
#endif
	renderHandler().reclaimEvictedAssets();

#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::UpdateGame);
	VitaRuntime::mark(VitaRuntime::UpdateGame);
#endif
	engineUser->onUpdate();

#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::HandleInput);
	VitaRuntime::mark(VitaRuntime::HandleInput);
#endif
	handleEvents();

	// before the redraw, so that a window that was only covered can reclaim its layer
#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::ClearLayers);
	VitaRuntime::mark(VitaRuntime::ClearLayers);
#endif
	screenHandlerInstance->clearReleasedLayers();

#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::DrawWindows);
	VitaRuntime::mark(VitaRuntime::DrawWindows);
#endif
#ifdef VCMI_VITA
	if (const auto top = windows().topWindow<CIntObject>())
		vitaFrameProfiler.scene(typeid(*top).name());
#endif
	windows().simpleRedraw();

	if (settings["video"]["performanceOverlay"]["show"].Bool())
		drawPerformanceOverlay();

#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::UploadScreen);
	VitaRuntime::mark(VitaRuntime::UploadScreen);
#endif
	screenHandlerInstance->updateScreenTexture();

#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::FrameCallbacks);
	VitaRuntime::mark(VitaRuntime::FrameCallbacks);
#endif
	windows().onFrameRendered();
#ifdef VCMI_VITA
	vitaFrameProfiler.phase(VitaRuntime::UpdateCursor);
	VitaRuntime::mark(VitaRuntime::UpdateCursor);
#endif
	ENGINE->cursor().update();
}

GameEngine::~GameEngine()
{
#ifdef VCMI_VITA
	VitaRuntime::enabled.store(false, std::memory_order_release);
#endif
	// enforce deletion order on shutdown
	// all UI elements including adventure map must be destroyed before Gui Handler
	// proper solution would be removal of adventureInt global
	adventureInt.reset();
}

ShortcutHandler & GameEngine::shortcuts()
{
	assert(shortcutsHandlerInstance);
	return *shortcutsHandlerInstance;
}

FramerateManager & GameEngine::framerate()
{
	assert(framerateManagerInstance);
	return *framerateManagerInstance;
}

bool GameEngine::isKeyboardCtrlDown() const
{
	return inputHandlerInstance->isKeyboardCtrlDown();
}

bool GameEngine::isKeyboardCmdDown() const
{
	return inputHandlerInstance->isKeyboardCmdDown();
}

bool GameEngine::isKeyboardAltDown() const
{
	return inputHandlerInstance->isKeyboardAltDown();
}

bool GameEngine::isKeyboardShiftDown() const
{
	return inputHandlerInstance->isKeyboardShiftDown();
}

const Point & GameEngine::getCursorPosition() const
{
	return inputHandlerInstance->getCursorPosition();
}

Point GameEngine::screenDimensions() const
{
	return screenHandlerInstance->getLogicalResolution();
}

bool GameEngine::isRoeData() const
{
	return LIBRARY->isRoeData();
}

bool GameEngine::isDemoData() const
{
	return LIBRARY->isDemoData();
}

void GameEngine::drawPerformanceOverlay()
{
	auto font = EFonts::FONT_SMALL;
	const auto & fontPtr = ENGINE->renderHandler().loadFont(font);

	Canvas target = screenHandler().getScreenCanvas();

	auto powerState = ENGINE->input().getPowerState();
	std::string powerSymbol = ""; // add symbol if emoji are supported (e.g. VCMI extras)
	if(powerState.powerState == PowerStateMode::ON_BATTERY)
		powerSymbol = fontPtr->canRepresentCharacter("🔋") ? "🔋 " : (LIBRARY->generaltexth->translate("vcmi.overlay.battery") + " ");
	else if(powerState.powerState == PowerStateMode::CHARGING)
		powerSymbol = fontPtr->canRepresentCharacter("🔌") ? "🔌 " : (LIBRARY->generaltexth->translate("vcmi.overlay.charging") + " ");

	std::string fps = std::to_string(framerate().getFramerate())+" FPS";
	std::string time = TextOperations::getFormattedTimeLocal(std::time(nullptr));
	std::string power = powerState.powerState == PowerStateMode::UNKNOWN ? "" : powerSymbol + std::to_string(powerState.percent) + "%";

	std::string textToDisplay = time + (power.empty() ? "" : " | " + power) + " | " + fps;

	maxPerformanceOverlayTextWidth = std::max(maxPerformanceOverlayTextWidth, static_cast<int>(fontPtr->getStringWidth(textToDisplay))); // do not get smaller (can cause graphical glitches)

	Rect targetArea;
	std::string edge = settings["video"]["performanceOverlay"]["edge"].String();
	int marginTopBottom = settings["video"]["performanceOverlay"]["marginTopBottom"].Integer();
	int marginLeftRight = settings["video"]["performanceOverlay"]["marginLeftRight"].Integer();

	Point boxSize(maxPerformanceOverlayTextWidth + 6, fontPtr->getLineHeight() + 2);

	if (edge == "topleft")
		targetArea = Rect(marginLeftRight, marginTopBottom, boxSize.x, boxSize.y);
	else if (edge == "topright")
		targetArea = Rect(screenDimensions().x - marginLeftRight - boxSize.x, marginTopBottom, boxSize.x, boxSize.y);
	else if (edge == "bottomleft")
		targetArea = Rect(marginLeftRight, screenDimensions().y - marginTopBottom - boxSize.y, boxSize.x, boxSize.y);
	else if (edge == "bottomright")
		targetArea = Rect(screenDimensions().x - marginLeftRight - boxSize.x, screenDimensions().y - marginTopBottom - boxSize.y, boxSize.x, boxSize.y);

	target.drawColor(targetArea.resize(1), Colors::BRIGHT_YELLOW);
	target.drawColor(targetArea, ColorRGBA(0, 0, 0));
	target.drawText(targetArea.center(), font, Colors::WHITE, ETextAlignment::CENTER, textToDisplay);
}

bool GameEngine::amIGuiThread()
{
	return inGuiThread;
}

void GameEngine::dispatchMainThread(const std::function<void()> & functor)
{
	inputHandlerInstance->dispatchMainThread(functor);
}

IScreenHandler & GameEngine::screenHandler()
{
	return *screenHandlerInstance;
}

IRenderHandler & GameEngine::renderHandler()
{
	return *renderHandlerInstance;
}

EventDispatcher & GameEngine::events()
{
	return *eventDispatcherInstance;
}

InputHandler & GameEngine::input()
{
	return *inputHandlerInstance;
}

WindowHandler & GameEngine::windows()
{
	assert(windowHandlerInstance);
	return *windowHandlerInstance;
}

std::shared_ptr<IStatusBar> GameEngine::statusbar()
{
	auto locked = currentStatusBar.lock();

	if (!locked)
		return fakeStatusBar;

	return locked;
}

void GameEngine::setStatusbar(const std::shared_ptr<IStatusBar> & newStatusBar)
{
	currentStatusBar = newStatusBar;
}

void GameEngine::onScreenResize(bool resolutionChanged, bool windowResized)
{
	if(resolutionChanged)
		if(!screenHandler().onScreenResize(windowResized))
			return;

	windows().onScreenResize();
	ENGINE->cursor().onScreenResize();
}

void GameEngine::setEngineUser(IGameEngineUser * user)
{
	engineUser = user;
}
