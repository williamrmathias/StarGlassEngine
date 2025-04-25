#pragma once

// sge
#include "Resource.h"
#include "RenderEngine.h"

// stl
#include <optional>
#include <vector>
#include <string_view>

class AssetManager
{
};

struct ImageGltf 
{
    std::vector
};

/*
* A GLTF file loaded into CPU memory
*/
class LoadedGltf
{
public:
    std::vector<gfx::AllocatedImage> images;
};

std::optional<LoadedGltf> loadGLTF(std::string_view gltfPath);