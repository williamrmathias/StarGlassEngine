#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdlib>

namespace util 
{

uint64_t fastHash(const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xCBF29CE484222325ull; // FNV-1a 64-bit offset

    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 0x100000001B3ull; // FNV prime
    }

    return hash;
}

inline void hashCombine(uint64_t& seed, uint64_t value)
{
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

void scratchDecodeURI(std::string_view uri, std::string& output)
{
    output.clear();

    // decode reserve characters
    for (size_t i = 0; i < uri.size(); ++i)
    {
        if (uri[i] == '%' && i + 2 < uri.size())
        {
            char hex[3] = { uri[i + 1], uri[i + 2], '\0' };
            if (std::isxdigit(hex[0]) && std::isxdigit(hex[1]))
            {
                output.push_back(static_cast<char>(std::strtol(hex, nullptr, 16)));
                i += 2;
            }
            else
            {
                SDL_LogError(0, "URI Decode error: uri contains invalid percent-encoding, uri: %s\n", uri.data());
                output.clear();
                return;
            }
        }
        else
        {
            output.push_back(uri[i]);
        }
    }
}

std::string decodeURI(std::string_view uri)
{
    std::string result;
    result.reserve(uri.size());
    scratchDecodeURI(uri, result);
    return result;
}


} // namespace util
