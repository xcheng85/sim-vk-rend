#pragma once
#include <iostream>
#include <format>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <camera.h>

using namespace std;

extern bool gRunning;
extern double gDt;
extern uint64_t gLastFrame;

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
    Window() = delete;
    Window(void *configuration, Camera &camera) : _camera{camera}
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
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    gRunning = false;
                    break;
                case SDLK_w:
                    _camera.handleKeyboardEvent(Camera::CameraActionType::FORWARD, gDt);
                    break;
                case SDLK_s:
                    _camera.handleKeyboardEvent(Camera::CameraActionType::BACKWARD, gDt);
                    break;
                case SDLK_a:
                    _camera.handleKeyboardEvent(Camera::CameraActionType::LEFT, gDt);
                    break;
                case SDLK_d:
                    _camera.handleKeyboardEvent(Camera::CameraActionType::RIGHT, gDt);
                    break;
                }
                break;

            // case SDL_MOUSEBUTTONDOWN:
            //     switch (event.button.button)
            //     {
            //     case SDL_BUTTON_LEFT:
            //         SDL_ShowSimpleMessageBox(0, "Mouse", "Left button was pressed!", _window);
            //         break;
            //     case SDL_BUTTON_RIGHT:
            //         SDL_ShowSimpleMessageBox(0, "Mouse", "Right button was pressed!", _window);
            //         break;
            //     default:
            //         SDL_ShowSimpleMessageBox(0, "Mouse", "Some other button was pressed!", _window);
            //         break;
            //     }
            //     break;
            case SDL_MOUSEMOTION:
                int mouseX = event.motion.x;
                int mouseY = event.motion.y;

                if (_firstMouseCursor)
                {
                    _lastMouseCursorX = mouseX;
                    _lastMouseCursorY = mouseY;
                    _firstMouseCursor = false;
                }

                float dx = mouseX - _lastMouseCursorX;
                float dy = _lastMouseCursorY - mouseY; // screen space is opposite to camera space

                _lastMouseCursorX = mouseX;
                _lastMouseCursorY = mouseY;

                _camera.handleMouseCursorEvent(dx, dy);
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
    Camera &_camera;
    bool _firstMouseCursor{true};
    int _lastMouseCursorX;
    int _lastMouseCursorY;
};