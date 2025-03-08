// Enable the WSI extensions
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "RenderEngine.h"

// stl
#include <cstdio>
#include <iostream>

int main()
{
#if defined(_DEBUG)
    AllocConsole();
    FILE* stdOut = freopen("CONOUT$", "w", stdout);
    FILE* stdErr = freopen("CONOUT$", "w", stderr);
#endif // DEBUG


    // Create an SDL window that supports Vulkan rendering.
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cout << "Could not initialize SDL." << std::endl;
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN);
    if (window == nullptr)
    {
        std::cout << "Could not create SDL window." << std::endl;
        return 1;
    }

    // program initialization
    RenderEngine renderEngine;
    renderEngine.init(window);

    // Poll for user input.
    bool stillRunning = true;
    while (stillRunning)
    {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {

            switch (event.type)
            {

            case SDL_QUIT:
                stillRunning = false;
                break;

            default:
                // Do nothing.
                break;
            }
        }

        SDL_Delay(10);
    }

    // Clean up.
    renderEngine.cleanup();

    SDL_DestroyWindow(window);
    SDL_Quit();

#if defined(_DEBUG)
    system("pause");
#endif

    return 0;
}
