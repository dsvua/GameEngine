#include "Engine.hpp"

#include <chrono>

#include <SDL3/SDL.h>

#include <tracy/Tracy.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef TRACY_ENABLE
void* operator new(std ::size_t count)
{
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}
void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}
void operator delete(void* ptr, std::size_t) noexcept
{
    TracyFree(ptr);
    free(ptr);
}
#endif

Engine::Engine(const Params &ps)
{
    params = ps;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        printf("SDL could not initialize. SDL Error: %s\n", SDL_GetError());
        std::exit(1);
    }

    // TODO:
    // Add version to window title.
    window = SDL_CreateWindow(
        params.appName.c_str(),
        // size
        params.windowSize.x,
        params.windowSize.y,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        printf("Failed to create window. SDL Error: %s\n", SDL_GetError());
        std::exit(1);
    }

    // renderer = new Renderer(window, params.windowSize, vSync);
    gfxDevice.init(window, params.appName.c_str(), params.version, vSync);
    gameRenderer.init(gfxDevice, params.windowSize);
}
