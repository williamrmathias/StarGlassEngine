#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdlib>

namespace util 
{

constexpr float kMaxFP16 = 65504.f;

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

// FP16 <-> FP32 conversion
// "Accuracy and performance of the lattice Boltzmann method with 64-bit, 32-bit, and customized 16-bit number formats"
uint32_t as_uint(const float x)
{
    return *(uint32_t*)&x;
}
float as_float(const uint32_t x)
{
    return *(float*)&x;
}

float half_to_float(const uint16_t x)
{ // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t e = (x & 0x7C00) >> 10; // exponent
    const uint32_t m = (x & 0x03FF) << 13; // mantissa
    const uint32_t v = as_uint((float)m) >> 23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
}
uint16_t float_to_half(const float x)
{ // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint32_t b = as_uint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint32_t e = (b & 0x7F800000) >> 23; // exponent
    const uint32_t m = b & 0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) | ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) | (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
}

} // namespace util
