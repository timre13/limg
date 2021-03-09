/*
BSD 2-Clause License

Copyright (c) 2021, timre13
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Image.h"
#include <stdint.h>
#include <vector>

class BmpImage : public Image
{
public:
    enum class CompressionMethod
    {
        BI_RGB,             // None
        BI_RLE8,
        BI_RLE4,
        BI_BITFIELDS,       // Bitmasks
        BI_JPEG,
        BI_PNG,
        BI_CMYK = 11,       // None
        BI_CMYKRLE8,
        BI_CMYKRLE4,
    };

private:
    bool m_isInitialized{};
    uint8_t* m_buffer{};
    uint32_t m_fileSize{}; // Size of the whole file in bytes
    uint32_t m_bitmapOffset{};
    uint32_t m_dibHeaderSize{};
    uint32_t m_bitmapWidthPx{};
    uint32_t m_bitmapHeightPx{};
    uint16_t m_bitsPerPixel{};
    CompressionMethod m_compMethod{};
    uint32_t m_imageSize{}; // Size of the image in bytes, BI_RGB images can have it 0-ed
    int32_t m_imageHResPpm{};
    int32_t m_imageVResPpm{};
    uint32_t m_numOfPaletteColors{};
    uint32_t m_rBitmask{};
    uint32_t m_gBitmask{};
    uint32_t m_bBitmask{};
    bool m_hasAlphaBitmask{};
    uint32_t m_aBitmask{};

    int _readBitmapCoreHeader();
    int _readBitmapInfoHeader();

    int  _render1BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;
    int  _render4BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;
    int  _render8BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;
    int _render16BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;
    int _render24BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;
    int _render32BitImage(uint8_t* pixelArray, int windowWidth, int windowHeight, int textureWidth) const;

public:
    virtual int open(const std::string &filepath) override;
    virtual int render(SDL_Texture* texture, int windowWidth, int windowHeight, int textureWidth) const override;

    inline uint32_t getWidthPx() const { return m_bitmapWidthPx; }
    inline uint32_t getHeightPx() const { return m_bitmapHeightPx; }

    virtual ~BmpImage() override;
};
