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
#include <filesystem>
#include <string>
#include <vector>

/*
 * This class can open GIF image files.
 *
 * GIF specification: https://web.archive.org/web/20160304075538/http://qalle.net/gif89a.php
 */
class GifImage final : public Image
{
public:
    enum class GifVersion
    {
        Invalid,
        _87a,
        _89a,
        Unknown, // Unknown, but still valid (a newer version)
    };

private:
    uint8_t* m_buffer{};
    GifVersion m_gifVersion;
    struct LogicalScreen
    {
        uint16_t width{};
        uint16_t height{};
        uint8_t bgColorIndex{};
        float pixelAspectRatio{};
    } m_logicalScreen;
    // Whether there is a global color table after the logical screen descriptor
    bool m_hasGlobalColorTable{};
    // Number of bits per primary colors
    uint8_t m_colorResolution{};
    // If `m_hasGlobalColorTable` is true,
    // this is the size of the global color palette in number of colors
    // If the `globalColorTableFlag` flag is set to 0, this can be ignored
    int m_globalColorTableSizeInColors{};
    int m_globalColorTableSizeInBytes{};

    struct ImageFrame
    {
        uint32_t startOffset{};
        struct ImageDescriptor
        {
            uint16_t imageLeftPos{};
            uint16_t imageTopPos{};
            uint16_t imageWidth{};
            uint16_t imageHeight{};
            bool hasLocalColorTable{};
            bool isInterlaced{};
            int localColorTableSizeInColors{};
            int localColorTableSizeInBytes{};
        } imageDescriptor;
    };

    std::vector<ImageFrame*> m_imageFrames{};

    int _fetchLogicalScreenDescriptor();
    int _fetchImageDescriptor(uint32_t startOffset);

public:
    virtual int open(const std::string &filepath) override;
    virtual int render(
            SDL_Texture* texture,
            uint32_t viewportWidth, uint32_t viewportHeight) const override;

    virtual ~GifImage() override;
};

