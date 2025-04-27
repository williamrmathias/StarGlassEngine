#pragma once
#include <cstddef>
#include <cstdint>

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


} // namespace util
