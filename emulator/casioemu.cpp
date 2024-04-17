#include "Config.hpp"
#include "Config/Config.hpp"
#include "Gui/imgui_impl_sdl2.h"
#include "Gui/Ui.hpp"
#include "SDL_timer.h"
#include "utils.h"

#include <SDL.h>
#include <SDL_image.h>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <ostream>
#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "Emulator.hpp"
#include "Logger.hpp"
#include "Data/EventCode.hpp"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_video.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <csignal>

static bool abort_flag = false;

void StartMemSpansConfigWatcherThread(const std::string &path);

using namespace casioemu;
// #define DEBUG
int main(int argc, char *argv[])
{
	std::map<std::string, std::string> argv_map;
	for (int ix = 1; ix != argc; ++ix)
	{
		std::string key, value;
		char *eq_pos = strchr(argv[ix], '=');
		if (eq_pos)
		{
			key = std::string(argv[ix], eq_pos);
			value = eq_pos + 1;
		}
		else
		{
			key = "model";
			value = argv[ix];
		}

		if (argv_map.find(key) == argv_map.end())
			argv_map[key] = value;
		else
			logger::Info("[argv] #%i: key '%s' already set\n", ix, key.c_str());
	}

	if (argv_map.find("model") == argv_map.end())
	{

#ifdef DEBUG
		argv_map["model"]="E:/projects/CasioEmuX/models/fx991cncw";
#else
		argv_map["model"]=EmuGloConfig.GetModulePath();
#endif
		// printf("No model path supplied\n");
		// exit(2);
	}

	int sdlFlags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
	if (SDL_Init(sdlFlags) != 0)
		PANIC("SDL_Init failed: %s\n", SDL_GetError());

	int imgFlags = IMG_INIT_PNG;
	if (IMG_Init(imgFlags) != imgFlags)
		PANIC("IMG_Init failed: %s\n", IMG_GetError());

	std::string history_filename;
	auto history_filename_iter = argv_map.find("history");
	if (history_filename_iter != argv_map.end())
		history_filename = history_filename_iter->second;

	if (!history_filename.empty())
	{
		
	}

    for (auto s: {SIGTERM, SIGINT}) {
        signal(s, [](int) {
            abort_flag = true;
        });
    }


	// while(1)
	// 	;
	{
		Emulator emulator(argv_map);
		
		// Note: argv_map must be destructed after emulator.

        // start colored spans file watcher thread
    	auto colored_spans_file = emulator.GetModelFilePath("mem-spans.txt");
        StartMemSpansConfigWatcherThread(colored_spans_file);

		// Used to signal to the console input thread when to stop.
		static std::atomic<bool> running(true);

		// std::thread console_input_thread([&] {
		// 	struct terminate_thread {};

		// 	while (1)
		// 	{
		// 		char *console_input_c_str;

		// 		if (console_input_c_str == NULL)
		// 		{
		// 			if(argv_map.find("exit_on_console_shutdown") != argv_map.end())
		// 			{
		// 				SDL_Event event;
		// 				SDL_zero(event);
		// 				event.type = SDL_WINDOWEVENT;
		// 				event.window.event = SDL_WINDOWEVENT_CLOSE;
		// 				SDL_PushEvent(&event);
		// 			}
		// 			else
		// 			{
		// 				logger::Info("Console thread shutting down\n");
		// 			}

		// 			break;
		// 		}

		// 		// Ignore empty lines.
		// 		if (console_input_c_str[0] == 0)
		// 			continue;


		// 		std::lock_guard<decltype(emulator.access_mx)> access_lock(emulator.access_mx);
		// 		if (!emulator.Running())
		// 			break;
		// 		emulator.ExecuteCommand(console_input_c_str);
		// 		free(console_input_c_str);

		// 		if (!emulator.Running())
		// 		{
		// 			SDL_Event event;
		// 			SDL_zero(event);
		// 			event.type = SDL_USEREVENT;
		// 			event.user.code = CE_EMU_STOPPED;
		// 			SDL_PushEvent(&event);
		// 			return;
		// 		}
		// 	}
		// });
		DebugUi ui;
		float m_DeltaTime;
		Uint64 m_StartTime;
		Uint64 m_PreFrameTime;
		m_StartTime = SDL_GetPerformanceCounter();
		m_PreFrameTime = m_StartTime;
		while (emulator.Running())
		{
			
			//std::cout<<SDL_GetMouseFocus()<<","<<emulator.window<<std::endl;
			SDL_Event event;
			if (!SDL_PollEvent(&event))
				continue;
			
            if (abort_flag) {
                abort_flag = false;
                SDL_Event ev_exit;
                SDL_zero(ev_exit);
                ev_exit.type = SDL_WINDOWEVENT;
                ev_exit.window.event = SDL_WINDOWEVENT_CLOSE;
                SDL_PushEvent(&ev_exit);
            }

			switch (event.type)
			{
			case SDL_USEREVENT:
				switch (event.user.code)
				{
				case CE_FRAME_REQUEST:
					emulator.Frame();
					
					break;
				case CE_EMU_STOPPED:
					if (emulator.Running())
						PANIC("CE_EMU_STOPPED event received while emulator is still running\n");
					break;
				}
				break;

			case SDL_WINDOWEVENT:
				
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_CLOSE:
					emulator.Shutdown();
					break;
				case SDL_WINDOWEVENT_RESIZED:
					 if (!emulator.IsResizable())
					 {
					 	// Normally, in this case, the window manager should not
					 	// send resized event, but some still does (such as xmonad)
					 	break;
					 }
					ImGui_ImplSDL2_ProcessEvent(&event);
					if(event.window.windowID == SDL_GetWindowID(emulator.window)){
					emulator.WindowResize(event.window.data1, event.window.data2);
					}
					break;
				case SDL_WINDOWEVENT_EXPOSED:
					emulator.Repaint();
					break;
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			case SDL_TEXTINPUT:
			case SDL_MOUSEMOTION:
			case SDL_MOUSEWHEEL:
				if(SDL_GetKeyboardFocus()!=emulator.window && SDL_GetMouseFocus()!=emulator.window)
				{
					ImGui_ImplSDL2_ProcessEvent(&event);
					break;
				}
				emulator.UIEvent(event);
				
				break;
			}
			ui.PaintUi();
			Uint64 currTime = SDL_GetPerformanceCounter();
			m_DeltaTime = (currTime - m_PreFrameTime) / (float)SDL_GetPerformanceFrequency();
			m_PreFrameTime = currTime;
			float totalTime = (currTime - m_StartTime) / (float)SDL_GetPerformanceFrequency();
			float FPS = 1.0f / m_DeltaTime;
			//SDL_LogInfo(0, "totalTime: %f\tdt: %f\tFPS: %f\n", totalTime, m_DeltaTime, FPS);

			// 限制帧率
			int MaxFPS = 120;
			if (m_DeltaTime < 1.0f / MaxFPS * 1000.0f)
				SDL_Delay(Uint32(floor(1.0 / MaxFPS * 1000.0 - m_DeltaTime)));
		
		}
		
		running = false;

		
		//console_input_thread.join();
	}

	std::cout << '\n';
	
	IMG_Quit();
	SDL_Quit();

	if (!history_filename.empty())
	{
		
	}

	return 0;
}

#define MEM_SPANS_CONFIG_POLLING_INTERVAL 1 /* seconds */
void StartMemSpansConfigWatcherThread(const std::string &path) {
    std::thread([&]() {
        uint64_t last_mtime{};
        // Currently we just use this naive file modification watcher (periodically polling method).
        // There are platform-dependent methods (like inotify on *nix)
        // but they're more efficient for folders/bunch of files; if just for a single file,
        // the performance impact can be considered *negligible*.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
        // loop during the whole program lifetime
        while (true) {
            if (FileSystem::exists(path)) {
                auto mtime = FileSystem::mtime_ms(path);
                if (mtime != last_mtime) {
                    DebugUi::UpdateMarkedSpans(casioemu::ParseColoredSpansConfig(path));
                    last_mtime = mtime;
                }
            } else {
                DebugUi::UpdateMarkedSpans({});
                last_mtime = 0L;
            }
            sleep(MEM_SPANS_CONFIG_POLLING_INTERVAL);
        }
#pragma clang diagnostic pop
    }).detach();
}
