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

#pragma once

#include "Image.h"
#include "Logger.h"
#include <SDL2/SDL.h>

/*
 * This class can open PNM images.
 *
 * PNM (Portable aNyMap) is a family of file types:
 *      + PBM (Portable BitMap) is binary/ASCII 1-bit
 *      + PGM (Portable GrayMap) is binary/ASCII 8-bit
 *      + PPM (Portable PixMap) is binary/ASCII 24-bit
 */
class PnmImage final : public Image
{
public:
    enum class PnmType
    {
        PBM_Ascii,  // ASCII 1-bit
        PGM_Ascii,  // ASCII 8-bit
        PPM_Ascii,  // ASCII 24-bit
        PBM_Bin,    // binary 1-bit
        PGM_Bin,    // binary 8-bit
        PPM_Bin,    // binary 24-bit
    };

private:
    uint32_t m_fileSize{};
    bool m_isInitialized{};
    std::string m_filePath{};
    PnmType m_type{};
    uint8_t* m_buffer{};

public:
    virtual int open(const std::string &filepath) override;
    virtual int render(
            SDL_Texture* texture, int windowWidth, int windowHeight, int textureWidth) const override;

    virtual ~PnmImage() override;
};
