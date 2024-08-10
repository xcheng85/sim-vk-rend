#pragma once
#include <iostream>
#include <format>
#include <SDL.h>
#include <SDL_vulkan.h>

using namespace std;

extern bool gRunning;

class SDL_Window;

struct WindowConfig
{
    uint32_t width{800};
    uint32_t height{600};
    std::string title{"default"};
};

// template <typename T>
// requires requires(T t) {
//     t.title;
//     t.width;
//     t.height;
// }

class Window
{
public:
    void init(void *configuration)
    {
        const auto &config = *(static_cast<WindowConfig *>(configuration));
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            cerr << format("SDL Init error: {}\n", SDL_GetError());
            return;
        }

        SDL_Vulkan_LoadLibrary(nullptr);

        SDL_DisplayMode current;
        SDL_GetCurrentDisplayMode(0, &current);

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        _window = SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.width, config.height, window_flags);
        int w, h;
        SDL_Vulkan_GetDrawableSize(_window, &w, &h);
    }
    void shutdown()
    {
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }
    void pollEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                gRunning = false;
                break;
            }
        }
    }
    inline SDL_Window *nativeHandle() const
    {
        return this->_window;
    };

private:
    SDL_Window *_window;
};