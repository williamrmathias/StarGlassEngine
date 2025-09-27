// Enable the WSI extensions
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "RenderEngine.h"
#include "Camera.h"

// stl
#include <cstdio>
#include <iostream>
#include <chrono>

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

    Camera camera;

    // Poll for user input.
    bool stillRunning = true;

    auto inputTimeStart = std::chrono::high_resolution_clock::now();
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

            Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);

            ImGui_ImplSDL2_ProcessEvent(&event); // Forward SDL event to ImGui
            camera.processSDLEvent(event, mouseState); // forward input to camera
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (ImGui::Begin("StarGlass Engine - Editor"))
        {
            if (ImGui::CollapsingHeader("Global Scene Settings"))
            {
                // azimuth: 0 to 360 degrees - where 0 is the angle of north
                // altitude: -90 to 90 degrees - where 0 is the angle of the horizon
                static float azimuth = 0.f, altitude = 90.f;
                bool setSunDir = ImGui::SliderFloat("Sun: Azimuth", &azimuth, 0.f, 360.f);
                setSunDir |= ImGui::SliderFloat("Sun: Altitude", &altitude, -90.f, 90.f);

                if (setSunDir)
                    renderEngine.setSunDirection(azimuth, altitude);

                static float luminance = 5.f;
                if (ImGui::SliderFloat("Sun: Luminance", &luminance, 0.f, 10.f))
                    renderEngine.setSunLuminance(luminance);
            }

            if (ImGui::CollapsingHeader("Tone Mapping Settings"))
            {
                static float exposure = 1.f;
                if (ImGui::SliderFloat("Exposure", &exposure, 0.f, 10.f))
                    renderEngine.setExposure(exposure);

                using PipelineType = gfx::RenderEngine::PipelineType;
                static PipelineType pipeline = PipelineType::ToneMap;
                if (ImGui::Selectable("ToneMap", pipeline == PipelineType::ToneMap))
                    pipeline = PipelineType::ToneMap;
                if (ImGui::Selectable("PassThrough", pipeline == PipelineType::PassThrough))
                    pipeline = PipelineType::PassThrough;

                renderEngine.setActiveScreenSpacePipeline(pipeline);
            }

            if (ImGui::CollapsingHeader("Debug Shader Settings"))
            {
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
                if (ImGui::Selectable("NormalDebug", pipeline == PipelineType::NormalDebug))
                    pipeline = PipelineType::NormalDebug;
                if (ImGui::Selectable("VertexNormalDebug", pipeline == PipelineType::VertexNormalDebug))
                    pipeline = PipelineType::VertexNormalDebug;
                if (ImGui::Selectable("UvDebug", pipeline == PipelineType::UvDebug))
                    pipeline = PipelineType::UvDebug;

                renderEngine.setActiveMainPassPipeline(pipeline);
            }

            if (ImGui::CollapsingHeader("Camera Settings"))
            {
                float speed = camera.getSpeed();
                float sensitivity = camera.getSensitivity();
                if (ImGui::SliderFloat("Speed", &speed, 0.f, 10.f))
                    camera.setSpeed(speed);

                if (ImGui::SliderFloat("Sensitivity", &sensitivity, 0.f, 1.f))
                    camera.setSensitivity(sensitivity);
            }

        }
        ImGui::End();

        // update camera
        auto inputTimeEnd = std::chrono::high_resolution_clock::now();
        camera.updatePosition(std::chrono::duration<float>(inputTimeStart - inputTimeEnd).count());
        renderEngine.setViewMatrix(camera.getViewMatrix());
        renderEngine.setViewPosition(camera.getViewPosition());
        inputTimeStart = std::chrono::high_resolution_clock::now();

        // render scene
        renderEngine.render();
    }

    // Clean up.
    renderEngine.cleanup();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
