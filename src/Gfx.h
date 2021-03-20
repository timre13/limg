#pragma once

#include <stdint.h>

namespace Gfx
{

struct RGBA // Used for drawPointAt()
{
    uint8_t r{};
    uint8_t g{};
    uint8_t b{};
    uint8_t a{255};
};

inline void drawPointAt(
        uint8_t* pixelArray,
        int textureWidth,
        int xPos, int yPos,
        const RGBA& color)
{
    const long long offset{(yPos * textureWidth + xPos) * 4};
    pixelArray[offset + 0] = color.r;
    pixelArray[offset + 1] = color.g;
    pixelArray[offset + 2] = color.b;
    pixelArray[offset + 3] = color.a;
}

} // End of namespace

