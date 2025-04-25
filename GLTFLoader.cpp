// sge
#include "GLTFLoader.h"
#include "Log.h"

// cgltf
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// stb
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// stl
#include <span>

struct ScopedGLTFData
{
    cgltf_data* data;

    cgltf_data* operator->() const { return data; }

    ~ScopedGLTFData()
    {
        cgltf_free(data);
    }
};

static std::vector<gfx::AllocatedImage> loadImages(
    gfx::RenderEngine* engine, std::span<cgltf_image> gltfImages
)
{
    for (const cgltf_image& image : gltfImages)
    {
        if (image.buffer_view == nullptr)
        {
            SDL_LogError(0, "GLTF load error: input image has now buffer view: %s", image.name);
            std::abort();
        }

        cgltf_buffer_view* bufferView = image.buffer_view;
        cgltf_buffer* buffer = bufferView->buffer;
        const uint8_t* data = static_cast<uint8_t*>(buffer->data) + bufferView->offset;
    }
}

std::optional<LoadedGltf> loadGLTF(gfx::RenderEngine* engine, std::string_view gltfPath)
{
    LoadedGltf loadedGltf;

    cgltf_options options{}; // default loading options
    ScopedGLTFData gltfData;

    cgltf_result result = cgltf_parse_file(&options, gltfPath.data(), &gltfData.data);
    if (result != cgltf_result_success || gltfData.data == nullptr)
    {
        SDL_LogError(0, "GLTF load error: Could not read file data: %s\n", gltfPath.data());
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        return std::nullopt;
    }

    result = cgltf_load_buffers(&options, gltfData.data, gltfPath.data());
    if (result != cgltf_result_success)
    {
        SDL_LogError(0, "GLTF load error: Could not read buffer data: %s\n", gltfPath.data());
        SDL_LogError(0, "GLTF load error code: %i\n", result);
        return std::nullopt;
    }

    // load images
    loadedGltf.images = loadImages(engine, { gltfData->images, gltfData->images_count });
}