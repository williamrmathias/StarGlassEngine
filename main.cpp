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
    // Create an SDL window that supports Vulkan rendering.
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_LogError(0, "Detected SDL Error: Could not initialize SDL\n");
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN);
    if (window == nullptr)
    {
        SDL_LogError(0, "Detected SDL Error: Could not create SDL window\n");
        return 1;
    }

    // program initialization
    gfx::RenderEngine renderEngine;
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

            ImGui_ImplSDL2_ProcessEvent(&event); // Forward SDL event to ImGui
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (ImGui::Begin("StarGlass Engine - Editor"))
        {
            // azimuth: 0 to 360 degrees - where 0 is the angle of north
            // altitude: -90 to 90 degrees - where 0 is the angle of the horizon
            static float azimuth = 0, altitude = 0;
            bool setSunDir = ImGui::SliderFloat("Sun: Azimuth", &azimuth, 0.f, 360.f);
            setSunDir |= ImGui::SliderFloat("Sun: Altitude", &altitude, -90.f, 90.f);
            
            if (setSunDir)
                renderEngine.setSunDirection(azimuth, altitude);

            using PipelineType = gfx::RenderEngine::PipelineType;
            static PipelineType pipeline = PipelineType::MainGraphics;
            if (ImGui::Selectable("MainGraphics", pipeline == PipelineType::MainGraphics))
                pipeline = PipelineType::MainGraphics;
            if (ImGui::Selectable("BaseColorDebug", pipeline == PipelineType::BaseColorDebug))
                pipeline = PipelineType::BaseColorDebug;
            if (ImGui::Selectable("MetalDebug", pipeline == PipelineType::MetalDebug))
                pipeline = PipelineType::MetalDebug;
            if (ImGui::Selectable("RoughDebug", pipeline == PipelineType::RoughDebug))
                pipeline = PipelineType::RoughDebug;

            renderEngine.setActiveDrawPipeline(pipeline);

        }
        ImGui::End();

        // render scene
        renderEngine.render();
    }

    // Clean up.
    renderEngine.cleanup();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
