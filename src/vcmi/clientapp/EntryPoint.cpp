/*
 * EntryPoint.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

// EntryPoint.cpp : Defines the entry point for the console application.

#include "StdInc.h"
#include "../Global.h"
#include <vstd/DateUtils.h>

#include "../client/ClientCommandManager.h"
#include "../client/CMT.h"
#include "../client/CPlayerInterface.h"
#include "../client/CServerHandler.h"
#include "../client/GameEngine.h"
#include "../client/GameInstance.h"
#include "../client/gui/CursorHandler.h"
#include "../client/gui/WindowHandler.h"
#include "../client/mainmenu/CMainMenu.h"
#include "render/Graphics.h"
#include "render/IRenderHandler.h"
#include "../client/windows/CMessage.h"
#include "../client/windows/InfoWindows.h"

#include "../lib/AsyncRunner.h"
#include "../lib/CConsoleHandler.h"
#include "../lib/CConfigHandler.h"
#include "../lib/CThreadHelper.h"
#include "../lib/ExceptionsCommon.h"
#include "../lib/filesystem/Filesystem.h"
#include "../lib/logging/CBasicLogConfigurator.h"
#include "../lib/texts/TextOperations.h"
#include "../lib/modding/CModHandler.h"
#include "../lib/modding/ModDescription.h"
#include "../lib/texts/CGeneralTextHandler.h"
#include "../lib/texts/MetaString.h"
#include "../lib/GameLibrary.h"
#include "../lib/ScopeGuard.h"
#include "../lib/VCMIDirs.h"

#include <boost/program_options.hpp>
#include <vstd/StringUtils.h>

#ifdef VCMI_VITA
#define SDL_MAIN_HANDLED
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>
#include <atomic>
#include <malloc.h>
#include <iconv.h>
#include <deque>
#include <SDL_ttf.h>
#include "../lib/vita/VitaLoading.h"
#include "../lib/vita/VitaBootScreen.h"
#include "../lib/vita/VitaLegacyLineStats.h"
#include "../lib/vita/VitaUidDiagnostics.h"
#include "../lib/vita/VitaZipIO.h"
#include "../lib/vita/VitaSurfaceCopyTest.h"
extern "C" unsigned int _get_vita_heap_size();
extern "C" int _vita_net_init();
#endif

#ifdef VCMI_SDL3
// SDL_main.h is header-only in SDL3 and emits the platform entry point here.
// When VCMI supplies its own wmain() below, SDL must not generate one.
#if defined(VCMI_WINDOWS) && !defined(__GNUC__) && defined(VCMI_WITH_DEBUG_CONSOLE)
#define SDL_MAIN_HANDLED
#endif
// iOS enters through client_main() in ios/main.m instead, so only the declaration of
// SDL_main() is wanted here - the generated entry point would collide with it
#ifdef VCMI_IOS
#define SDL_MAIN_NOIMPL
#endif
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#else
#include <SDL_main.h>
#include <SDL.h>
#endif

#ifdef VCMI_ANDROID
#include "../lib/CAndroidVMHelper.h"
#ifdef VCMI_SDL3
#include <SDL3/SDL_system.h>
#else
#include <SDL_system.h>
#endif
#endif

#if __MINGW32__
#undef main
#endif

namespace po = boost::program_options;
namespace po_style = boost::program_options::command_line_style;

static std::optional<std::string> criticalInitializationError;

static void init()
{
	try
	{
		CStopWatch tmh;
		LIBRARY->initializeLibrary();
#ifdef VCMI_VITA
		VitaUidDiagnostics::report("library_ready");
#endif

		logGlobal->info("Initializing VCMI_Lib: %d ms", tmh.getDiff());
	}
	catch (const DataLoadingException & e)
	{
		criticalInitializationError = e.what();
		return;
	}

	// Debug code to load all maps on start
	//ClientCommandManager commandController;
	//commandController.processCommand("translate maps", false);
}

static void checkForModLoadingFailure()
{
	const auto & brokenMods = LIBRARY->modh->getModsWithFailedRequests();
	if (!brokenMods.empty())
	{
		MetaString messageText;
		messageText.appendTextID("vcmi.client.errors.modLoadingFailure");

		for (const auto & modID : brokenMods)
		{
			messageText.appendRawString(LIBRARY->modh->getModInfo(modID).getName());
			messageText.appendEOL();
		}
		CInfoWindow::showInfoDialog(messageText.toString(&GAME->translator()), {});
	}
}

static void prog_version()
{
	printf("%s\n", GameConstants::VCMI_VERSION);
	std::cout << VCMIDirs::get().genHelpString();
}

static void prog_help(const po::options_description &opts)
{
	auto time = std::time(nullptr);
	std::tm tm = vstd::safeLocalTime(time);
	printf("%s - A Heroes of Might and Magic 3 clone\n", GameConstants::VCMI_PROJECT_NAME_VERSIONED);
	printf("Copyright (C) 2007-%d VCMI dev team - see AUTHORS file\n", tm.tm_year + 1900);
	printf("This is free software; see the source for copying conditions. There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
	std::cout << opts;
}

#if defined(VCMI_WINDOWS) && !defined(__GNUC__) && defined(VCMI_WITH_DEBUG_CONSOLE)
int wmain(int argc, wchar_t* argv[])
#elif defined(VCMI_MOBILE) && !defined(VCMI_VITA)
int SDL_main(int argc, char *argv[])
#else
int main(int argc, char * argv[])
#endif
{
#ifdef VCMI_VITA
	const auto vitaBootTime = std::chrono::steady_clock::now();
	if (FILE * initialLog = std::fopen("ux0:data/vcmi-startup-rc1.log", "w"))
		std::fclose(initialLog);
	if (!std::freopen("ux0:data/vcmi-startup-rc1.log", "a", stdout))
		return 1;
	if (!std::freopen("ux0:data/vcmi-startup-rc1.log", "a", stderr))
		return 1;
	static char vitaStdoutBuffer[4096], vitaStderrBuffer[4096];
	std::setvbuf(stdout, vitaStdoutBuffer, _IOLBF, sizeof(vitaStdoutBuffer));
	std::setvbuf(stderr, vitaStderrBuffer, _IOLBF, sizeof(vitaStderrBuffer));
	std::fprintf(stderr, "VCMI VITA BUILD 01.00 (1.0.0-rc.1): main entered\n");
	std::fprintf(stderr, "Vita log policy: startup_level=info stderr=line file=buffered flush_on=warn_or_1s_on_write\n");
	std::fprintf(stderr, "Vita clocks before: cpu=%d bus=%d gpu=%d xbar=%d MHz\n",
		scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(), scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency());
	const int cpuClockResult = scePowerSetArmClockFrequency(444);
	const int busClockResult = scePowerSetBusClockFrequency(166);
	const int gpuClockResult = scePowerSetGpuClockFrequency(222);
	const int xbarClockResult = scePowerSetGpuXbarClockFrequency(166);
	std::fprintf(stderr, "Vita clocks active: cpu=%d bus=%d gpu=%d xbar=%d MHz rc=%d,%d,%d,%d\n",
		scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(), scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency(),
		cpuClockResult, busClockResult, gpuClockResult, xbarClockResult);
	iconv_t conversion = iconv_open("UTF-8", "CP1252");
	char sourceBytes[] = {static_cast<char>(0x80), static_cast<char>(0xe9)};
	char converted[8]{};
	char * source = sourceBytes;
	char * destination = converted;
	size_t sourceLeft = sizeof(sourceBytes);
	size_t destinationLeft = sizeof(converted);
	bool encodingOK = conversion != reinterpret_cast<iconv_t>(-1);
	if (encodingOK)
	{
		encodingOK = iconv(conversion, &source, &sourceLeft, &destination, &destinationLeft) != static_cast<size_t>(-1)
			&& sourceLeft == 0 && sizeof(converted) - destinationLeft == 5
			&& std::memcmp(converted, "\xe2\x82\xac\xc3\xa9", 5) == 0;
		iconv_close(conversion);
	}
	std::fprintf(stderr, "Vita encoding: CP1252 to UTF-8 self-test %s\n", encodingOK ? "PASS" : "FAIL");
	if (!encodingOK)
		return 1;
	SceKernelFreeMemorySizeInfo memory{};
	memory.size = sizeof(memory);
	const int memoryResult = sceKernelGetFreeMemorySize(&memory);
	std::fprintf(stderr, "VCMI Vita start: heap=%u free_user=%d free_cdram=%d memory_rc=%d\n",
		_get_vita_heap_size(), memory.size_user, memory.size_cdram, memoryResult);
	// newlib poll() uses sceNetEpollCreate without the lazy init done by socket().
	// Initialize before Boost.Asio starts its event-loop thread.
	const int networkResult = _vita_net_init();
	std::fprintf(stderr, "VCMI Vita network runtime init: %d\n", networkResult);
	if (networkResult != 0)
		return 1;
#endif
#ifdef SDL_MAIN_HANDLED
	// entry point is provided by VCMI, so SDL has to be told that it already ran
	SDL_SetMainReady();
#endif
#ifdef VCMI_VITA
    bool quietBoot = false;
    for(int i = 1; i < argc; ++i)
        if(std::string_view(argv[i]) == "--headless" || std::string_view(argv[i]) == "--help" || std::string_view(argv[i]) == "--version") quietBoot = true;
    if(!quietBoot)
    {
        VitaBootScreen::begin();
        std::fprintf(stderr, "Vita boot splash: visible_from_main_ms=%lld active=%d\n", static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-vitaBootTime).count()), VitaBootScreen::active);
    }
#endif


#ifdef VCMI_ANDROID
#ifdef VCMI_SDL3
	CAndroidVMHelper::initClassloader(SDL_GetAndroidJNIEnv());
#else
	CAndroidVMHelper::initClassloader(SDL_AndroidGetJNIEnv());
#endif
	// boost will crash without this
	setenv("LANG", "C", 1);
#endif

#if !defined(VCMI_MOBILE)
	// Correct working dir executable folder (not bundle folder) so we can use executable relative paths
	boost::filesystem::current_path(boost::filesystem::system_complete(argv[0]).parent_path());
#endif
	std::cout << "Starting... " << std::endl;
#ifdef VCMI_VITA
	VitaUidDiagnostics::report("main_ready");
#endif
	po::options_description opts("Allowed options");
	po::variables_map vm;

	opts.add_options()
		("help,h", "display help and exit")
		("version,v", "display version information and exit")
		("testmap", po::value<std::string>(), "")
		("testsave", po::value<std::string>(), "")
		("logLocation", po::value<std::string>(), "new location for log files")
		("spectate,s", "enable spectator interface for AI-only games")
		("spectate-ignore-hero", "wont follow heroes on adventure map")
		("spectate-hero-speed", po::value<int>(), "hero movement speed on adventure map")
		("spectate-battle-speed", po::value<int>(), "battle animation speed for spectator")
		("spectate-skip-battle", "skip battles in spectator view")
		("spectate-skip-battle-result", "skip battle result window")
		("onlyAI", "allow one to run without human player, all players will be default AI")
		("headless", "runs without GUI, implies --onlyAI")
		("ai", po::value<std::vector<std::string>>(), "AI to be used for the player, can be specified several times for the consecutive players")
		("oneGoodAI", "puts one default AI and the rest will be EmptyAI")
		("autoSkip", "automatically skip turns in GUI")
		("disable-video", "disable video player")
		("nointro,i", "skips intro movies")
		("donotstartserver,d","do not attempt to start server and just connect to it instead server")
		("serverport", po::value<si64>(), "override port specified in config file")
		("savefrequency", po::value<si64>(), "limit auto save creation to each N days");

	if(argc > 1)
	{
		try
		{
			po::store(po::parse_command_line(argc, argv, opts, po_style::unix_style|po_style::case_insensitive), vm);
		}
		catch(boost::program_options::error &e)
		{
			std::cerr << "Failure during parsing command-line options:\n" << e.what() << std::endl;
		}
	}

	po::notify(vm);
	if(vm.count("help"))
	{
		prog_help(opts);
#ifdef VCMI_IOS
		exit(0);
#else
		return 0;
#endif
	}
	if(vm.count("version"))
	{
		prog_version();
#ifdef VCMI_IOS
		exit(0);
#else
		return 0;
#endif
	}

	// Init old logging system and new (temporary) logging system
	CStopWatch total;
	CStopWatch pomtime;
	std::cout.flags(std::ios::unitbuf);

	setThreadNameLoggingOnly("MainGUI");
	boost::filesystem::path logPath = VCMIDirs::get().userLogsPath() / "VCMI_Client_log.txt";
	if(vm.count("logLocation"))
		logPath = vm["logLocation"].as<std::string>() + "/VCMI_Client_log.txt";

#ifndef VCMI_IOS

	auto callbackFunction = [](std::string buffer, bool calledFromIngameConsole)
	{
		ClientCommandManager commandController;
		commandController.processCommand(buffer, calledFromIngameConsole);
	};

	CConsoleHandler console(callbackFunction);
	console.start();

	CBasicLogConfigurator logConfigurator(logPath, &console);
#else
	CBasicLogConfigurator logConfigurator(logPath, nullptr);
#endif

	logConfigurator.configureDefault();
	logGlobal->info("Starting client of '%s'", GameConstants::VCMI_PROJECT_NAME_VERSIONED);
	logGlobal->info("Creating console and configuring logger: %d ms", pomtime.getDiff());
	logGlobal->info("The log file will be saved to %s", TextOperations::filesystemPathToUtf8(logPath));
#ifdef VCMI_SDL3
	logGlobal->info("Media backend: SDL3");
#else
	logGlobal->warn("Media backend: SDL2 - deprecated, slower and does not support all features");
#endif

	// Init filesystem and settings
	try
	{
		LIBRARY = new GameLibrary;
		LIBRARY->initializeFilesystem(false);
	}
	catch (const DataLoadingException & e)
	{
		handleFatalError(e.what(), true);
	}

	Settings session = settings.write["session"];
	auto setSettingBool = [&](const std::string & key, const std::string & arg) {
		Settings s = settings.write(vstd::split(key, "/"));
		if(vm.count(arg))
			s->Bool() = true;
		else if(s->isNull())
			s->Bool() = false;
	};
	auto setSettingInteger = [&](const std::string & key, const std::string & arg, si64 defaultValue) {
		Settings s = settings.write(vstd::split(key, "/"));
		if(vm.count(arg))
			s->Integer() = vm[arg].as<si64>();
		else if(s->isNull())
			s->Integer() = defaultValue;
	};

	setSettingBool("session/onlyai", "onlyAI");
	setSettingBool("session/disableVideo", "disable-video");
	if(vm.count("headless"))
	{
		session["headless"].Bool() = true;
		session["onlyai"].Bool() = true;
	}
	else if(vm.count("spectate"))
	{
		session["spectate"].Bool() = true;
		session["spectate-ignore-hero"].Bool() = vm.count("spectate-ignore-hero");
		session["spectate-skip-battle"].Bool() = vm.count("spectate-skip-battle");
		session["spectate-skip-battle-result"].Bool() = vm.count("spectate-skip-battle-result");
		if(vm.count("spectate-hero-speed"))
			session["spectate-hero-speed"].Integer() = vm["spectate-hero-speed"].as<int>();
		if(vm.count("spectate-battle-speed"))
			session["spectate-battle-speed"].Float() = vm["spectate-battle-speed"].as<int>();
	}
	// Server settings
	setSettingBool("session/donotstartserver", "donotstartserver");

	// Init special testing settings
	setSettingInteger("session/serverport", "serverport", 0);
	setSettingInteger("general/saveFrequency", "savefrequency", 1);

	// Initialize logging based on settings
	logConfigurator.configure();
	logGlobal->debug("settings = %s", settings.toJsonNode().toString());

	try
	{
		// Some basic data validation to produce better error messages in cases of incorrect install
		auto testFile = [](const std::string & filename, const std::string & message)
		{
			if (!CResourceHandler::get()->existsResource(ResourcePath(filename)))
				handleFatalError(message, false);
		};

		testFile("DATA/HELP.TXT", "VCMI requires Heroes III: Shadow of Death or Heroes III: Complete data files to run!");
		testFile("MODS/VCMI/MOD.JSON", "VCMI installation is corrupted!\nBuilt-in mod was not found!");
		testFile("DATA/NOTOSERIF-MEDIUM.TTF", "VCMI installation is corrupted!\nBuilt-in font was not found!\nManually deleting '" + TextOperations::filesystemPathToUtf8(VCMIDirs::get().userDataPath()) + "/Mods/VCMI' directory (if it exists)\nor clearing app data and reimporting Heroes III files may fix this problem.");
		testFile("DATA/PLAYERS.PAL", "Heroes III data files (Data/H3Bitmap.lod) are incomplete or corruped!\n Please reinstall them.");
		testFile("SPRITES/DEFAULT.DEF", "Heroes III data files (Data/H3Sprite.lod) are incomplete or corruped!\n Please reinstall them.");

		if(!settings["session"]["headless"].Bool())
		{
			if(LIBRARY->getGameDataMode() == GameLibrary::GameDataMode::ROE)
				handleFatalError("Heroes III: Restoration of Erathia (including HD Edition) data files are not supported!", false);

			ENGINE = std::make_unique<GameEngine>();
#ifdef VCMI_VITA
			runVitaSurfaceCopyTest();
#endif
		}

		GAME = std::make_unique<GameInstance>();

		if (ENGINE)
			ENGINE->setEngineUser(GAME.get());
	
#ifndef VCMI_NO_THREADED_LOAD
		//we can properly play intro only in the main thread, so we have to move loading to the separate thread
#ifdef VCMI_VITA
		const JsonNode loadingLabels(JsonPath::builtin("config/vitaLoading.json"));
		const bool ownTtf = !TTF_WasInit();
		TTF_Font * loadingFont = nullptr;
		SDL_Texture * finalLoadingLabel = nullptr;
		if (!ownTtf || TTF_Init() == 0)
			loadingFont = TTF_OpenFont("app0:Mods/vcmi/Content/Data/NotoSans-Medium.ttf", 22);
		if (!loadingFont)
			std::fprintf(stderr, "Vita loading font error: %s\n", TTF_GetError());
		std::deque<std::pair<SDL_Texture *, SDL_Rect>> loadingLines;
		size_t displayedStage = std::numeric_limits<size_t>::max();
		std::atomic<bool> loadingComplete{false};
		std::thread loading([&loadingComplete]()
#else
		std::thread loading([]()
#endif
		{
			setThreadName("initialize");
			init();
#ifdef VCMI_VITA
			loadingComplete.store(true, std::memory_order_release);
#endif
		});
#else
		init();
#endif

#ifndef VCMI_NO_THREADED_LOAD
	#ifdef VCMI_ANDROID // android loads the data quite slowly so we display native progressbar to prevent having only black screen for few seconds
		{
			CAndroidVMHelper vmHelper;
			vmHelper.callStaticVoidMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "showProgress");
	#endif // ANDROID
#ifdef VCMI_VITA
			const Uint32 loadingStart = SDL_GetTicks();
			Uint32 nextReport = 0;
			extern SDL_Renderer * mainRenderer;
			while (!loadingComplete.load(std::memory_order_acquire))
			{
				const Uint32 elapsed = SDL_GetTicks() - loadingStart;
				SDL_PumpEvents();
                VitaBootScreen::draw(mainRenderer);
                Point size;
                SDL_RenderGetLogicalSize(mainRenderer, &size.x, &size.y);
                SDL_Rect track{size.x * 10 / 100, size.y * 84 / 100, size.x * 80 / 100, 4};
                SDL_SetRenderDrawColor(mainRenderer, 39, 62, 76, 255);
                SDL_RenderFillRect(mainRenderer, &track);
                const int travel = track.w - track.w / 5;
                const int cycle = static_cast<int>((elapsed / 6) % (2 * travel));
                SDL_Rect pulse{track.x + (cycle <= travel ? cycle : 2 * travel - cycle), track.y, track.w / 5, track.h};
                SDL_SetRenderDrawColor(mainRenderer, 205, 177, 111, 255);
                SDL_RenderFillRect(mainRenderer, &pulse);
				const size_t stage = VitaLoading::stage.load(std::memory_order_relaxed);
				if (loadingFont && stage != displayedStage)
				{
					if (loadingLines.size() == 3)
					{
						SDL_DestroyTexture(loadingLines.front().first);
						loadingLines.pop_front();
					}
					SDL_Texture * loadingLabel = nullptr;
					SDL_Rect labelSize{};
					const std::string key = std::string(stage < VitaLoading::names.size() ? "vcmi.vita.loading." : "vcmi.vita.finalizing.") + VitaLoading::names.at(stage % VitaLoading::names.size());
					const std::string label = loadingLabels[key].String();
					SDL_Surface * text = TTF_RenderUTF8_Blended(loadingFont, label.c_str(), SDL_Color{232, 228, 210, 255});
					if (text)
					{
						labelSize = {0, 0, text->w, text->h};
						loadingLabel = SDL_CreateTextureFromSurface(mainRenderer, text);
						SDL_FreeSurface(text);
					}
					if (!loadingLabel)
						std::fprintf(stderr, "Vita loading label error: %s\n", SDL_GetError());
					if (loadingLabel) loadingLines.emplace_back(loadingLabel, labelSize);
					displayedStage = stage;
				}
				int lineY = size.y * 56 / 100;
				for (auto & line : loadingLines)
				{
					line.second.x = track.x;
					line.second.y = lineY;
					SDL_RenderCopy(mainRenderer, line.first, nullptr, &line.second);
					lineY += 42;
				}
				SDL_RenderPresent(mainRenderer);
				if (elapsed >= nextReport)
				{
					const auto heapInfo = mallinfo();
					std::fprintf(stderr, "Vita loading: elapsed_ms=%u heap_used=%d heap_free_arena=%d\n", elapsed, heapInfo.uordblks, heapInfo.fordblks);
					nextReport = elapsed + 10000;
				}
				SDL_Delay(100);
			}
			std::fprintf(stderr, "Vita loading: library complete after %u ms\n", SDL_GetTicks() - loadingStart);
			VitaBootScreen::draw(mainRenderer);
			if(loadingFont)
			{
				const auto label = loadingLabels["vcmi.vita.finalizing.finishing"].String();
				auto * text = TTF_RenderUTF8_Blended(loadingFont, label.c_str(), SDL_Color{232, 228, 210, 255});
				if(text)
				{
					auto * texture = SDL_CreateTextureFromSurface(mainRenderer, text);
					Point size;
					SDL_RenderGetLogicalSize(mainRenderer, &size.x, &size.y);
					SDL_Rect target{size.x / 10, size.y * 64 / 100, text->w, text->h};
					if(texture) SDL_RenderCopy(mainRenderer, texture, nullptr, &target);
					SDL_RenderPresent(mainRenderer);
					finalLoadingLabel = texture;
					SDL_FreeSurface(text);
				}
			}
			for (const auto & line : loadingLines) SDL_DestroyTexture(line.first);
			if (loadingFont) TTF_CloseFont(loadingFont);
			if (ownTtf && TTF_WasInit()) TTF_Quit();
#endif
			loading.join();
#ifdef VCMI_VITA
			std::fprintf(stderr, "Vita legacy lines: calls=%u skipped_bytes=%u\n",
				VitaLegacyLineStats::calls.load(), VitaLegacyLineStats::bytes.load());
#endif
	#ifdef VCMI_ANDROID
			vmHelper.callStaticVoidMethod(CAndroidVMHelper::NATIVE_METHODS_DEFAULT_CLASS, "hideProgress");
		}
	#endif // ANDROID
#endif // THREADED

		if (criticalInitializationError.has_value())
		{
			handleFatalError(criticalInitializationError.value(), false);
		}

		if (ENGINE)
		{
			pomtime.getDiff();
#ifdef VCMI_VITA
			const auto vitaGuiStart = std::chrono::steady_clock::now();
#endif
			graphics = new Graphics(); // should be before curh
#ifdef VCMI_VITA
			const auto vitaGraphicsEnd = std::chrono::steady_clock::now();
#endif
			ENGINE->renderHandler().onLibraryLoadingFinished(LIBRARY);
#ifdef VCMI_VITA
			const auto vitaRenderEnd = std::chrono::steady_clock::now();
#endif
			CMessage::init();
#ifdef VCMI_VITA
			const auto vitaMessagesEnd = std::chrono::steady_clock::now();
			std::fprintf(stderr, "Vita gui init: graphics_ms=%lld render_ms=%lld messages_ms=%lld\n",
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(vitaGraphicsEnd-vitaGuiStart).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(vitaRenderEnd-vitaGraphicsEnd).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(vitaMessagesEnd-vitaRenderEnd).count()));
#endif
			logGlobal->info("Message handler: %d ms", pomtime.getDiff());

			ENGINE->cursor().init();
			ENGINE->cursor().show();
		}

		logGlobal->info("Initialization of VCMI (together): %d ms", total.getDiff());

		session["autoSkip"].Bool()  = vm.count("autoSkip");
		session["oneGoodAI"].Bool() = vm.count("oneGoodAI");
		session["aiSolo"].Bool() = false;
	
		if(vm.count("testmap"))
		{
			session["testmap"].String() = vm["testmap"].as<std::string>();
			session["onlyai"].Bool() = true;
			GAME->server().debugStartTest(session["testmap"].String(), false);
		}
		else if(vm.count("testsave"))
		{
			session["testsave"].String() = vm["testsave"].as<std::string>();
			session["onlyai"].Bool() = true;
			GAME->server().debugStartTest(session["testsave"].String(), true);
		}
		else if (!settings["session"]["headless"].Bool())
		{
			GAME->mainmenu()->makeActiveInterface();

			bool playIntroVideo = !vm.count("battle") && !vm.count("nointro") && settings["video"]["showIntro"].Bool();
			if(playIntroVideo)
				GAME->mainmenu()->playIntroVideos();
			else
				GAME->mainmenu()->playMusic();
		}
	
#ifndef VCMI_UNIX
		// on Linux, name of main thread is also name of our process. Which we don't want to change
		setThreadName("MainGUI");
#endif

#ifdef VCMI_VITA
#ifndef VCMI_NO_THREADED_LOAD
		if(finalLoadingLabel) SDL_DestroyTexture(finalLoadingLabel);
#endif
		VitaBootScreen::finish();
		const auto vitaHeapAtMenu = mallinfo();
		std::fprintf(stderr, "Vita ZIP reuse: memory_opens=%u memory_hits=%u\n",
			VitaZipIO::openCalls.load(std::memory_order_relaxed), VitaZipIO::cacheHits.load(std::memory_order_relaxed));
		std::fprintf(stderr, "Vita boot: wall_total_ms=%lld heap_used=%d heap_free_arena=%d\n",
			static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-vitaBootTime).count()),
			vitaHeapAtMenu.uordblks, vitaHeapAtMenu.fordblks);
#endif
		const auto & runMainLoop = []()
		{
			try
			{
				if (ENGINE)
				{
					checkForModLoadingFailure();
					ENGINE->mainLoop();
				}
				else
				{
					GAME->server().waitForNetworkThread();
				}
			}
			catch (const GameShutdownException & )
			{
				// no-op - just break out of main loop
				logGlobal->info("Main loop termination requested");
			}
		};

		const auto & cleanupEngine = [&logConfigurator]()
		{
			if(settings["session"]["headless"].Bool() && GAME->server().client)
				GAME->server().endGameplay();

			if(ENGINE)
			{
				//aquire interfaceMutex to prevent undefined behavior on vstd::makeUnlockGuard(ENGINE->interfaceMutex)
				std::scoped_lock interfaceLock(ENGINE->interfaceMutex);
				GAME->server().endNetwork();
			}
			else
			{
				GAME->server().endNetwork();
			}

			if(!settings["session"]["headless"].Bool())
			{
				if(GAME->server().client)
					GAME->server().endGameplay();

				if (ENGINE)
					ENGINE->windows().clear();
			}

			GAME.reset();

			if(!settings["session"]["headless"].Bool())
			{
				CMessage::dispose();
				delete graphics;
				graphics = nullptr;
			}

			if(ENGINE)
			{
				// must be executed before reset - since unique_ptr resets pointer to null before calling destructor
				ENGINE->async().wait();

				ENGINE.reset();
			}

			delete LIBRARY;
			LIBRARY = nullptr;
			logConfigurator.deconfigure();

			std::cout << "Ending...\n";
		};

		auto onExit = vstd::makeScopeGuard(cleanupEngine);
		runMainLoop();
	}
	catch (const GameShutdownException &)
	{
		logGlobal->info("Shutdown requested during initialization");
	}
	return 0;
}

/// Notify user about encountered fatal error and terminate the game
/// TODO: decide on better location for this method
void handleFatalError(const std::string & message, bool terminate)
{
	logGlobal->error("FATAL ERROR ENCOUNTERED, VCMI WILL NOW TERMINATE");
	logGlobal->error("Reason: %s", message);

	std::string messageToShow = "Fatal error! " + message;

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error!", messageToShow.c_str(), nullptr);

	if (terminate)
		throw std::runtime_error(message);
	else
		throw GameShutdownException();
}
