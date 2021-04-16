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

#include "GifImage.h"
#include "LzwDecoder.h"
#include "Logger.h"
#include "bitmagic.h"
#include "Gfx.h"
#include <stdint.h>
#include <fstream>
#include <cstring>

#define GIF_MAX_BUFFER_SIZE -1_u32 // 4 gigs
#define GIF_LOGICAL_SCREEN_WIDTH_OFFS                6
#define GIF_LOGICAL_SCREEN_HEIGHT_OFFS               8
#define GIF_LOGICAL_SCREEN_FLAGS_OFFS               10
#define GIF_LOGICAL_SCREEN_BG_COLOR_OFFS            11
#define GIF_LOGICAL_SCREEN_PIXEL_ASPECT_RATIO_OFFS  12
#define GIF_AFTER_LOGICAL_SCREEN_DESCRIPTOR_OFFS    13

static std::string gifVersionToStr(GifImage::GifVersion version)
{
    switch (version)
    {
    case GifImage::GifVersion::Invalid: return "Invalid";
    case GifImage::GifVersion::_87a:    return "87a";
    case GifImage::GifVersion::_89a:    return "89a";
    default: return "Unknown";
    }
}

static GifImage::GifVersion strToGifVersion(const char* version)
{
    if (!std::isdigit(version[0]) || !std::isdigit(version[1]) || !std::isalpha(version[2]))
        return GifImage::GifVersion::Invalid;
    if (std::strcmp(version, "87a") == 0) return GifImage::GifVersion::_87a;
    if (std::strcmp(version, "89a") == 0) return GifImage::GifVersion::_89a;
    return GifImage::GifVersion::Unknown;
}

int GifImage::open(const std::string &filepath)
{
    m_filePath.clear();
    std::cout << std::hex;

    auto fileObject{std::ifstream{filepath, std::ios::binary}};
    if (fileObject.fail()) // If failed to open
    {
        Logger::err << "Failed to open file: " << strerror(errno) << Logger::End;
        return 1;
    }
    Logger::log << "Opened file" << Logger::End;

    char magicBytes[4]{};
    fileObject.read(magicBytes, 3);
    Logger::log << "Magic bytes (ASCII): " << magicBytes << Logger::End;
    if (magicBytes[0] != 'G' ||
        magicBytes[1] != 'I' ||
        magicBytes[2] != 'F')
    {
        Logger::err << "Invalid magic bytes" << Logger::End;
        return 1;
    }
    Logger::log << "Magic bytes OK" << Logger::End;

    char gifVersionStr[4]{};
    fileObject.read(gifVersionStr, 3);
    Logger::log << "GIF version (ASCII): " << gifVersionStr << Logger::End;
    m_gifVersion = strToGifVersion(gifVersionStr);
    Logger::log << "GIF version (enum): " << gifVersionToStr(m_gifVersion) << Logger::End;
    if (m_gifVersion == GifVersion::Invalid)
    {
        Logger::err << "Invalid GIF version" << Logger::End;
        return 1;
    }
    Logger::log << "GIF version OK" << Logger::End;

    fileObject.seekg(0, std::ios::end);
    std::ios::pos_type fileSize{fileObject.tellg()};
    // If greater than the max 32-bit value (m_fileSize is 32-bit)
    // or greater than the max allowed buffer size
    if (fileSize > -1_u32 || fileSize > GIF_MAX_BUFFER_SIZE)
    {
        Logger::err << "File is too large" << Logger::End;
        return 1;
    }
    m_fileSize = fileSize;

    m_buffer = new uint8_t[m_fileSize];
    fileObject.seekg(0, std::ios::beg);
    fileObject.read((char*)m_buffer, m_fileSize);
    Logger::log << "Copied 0x" << m_fileSize << " bytes" << Logger::End;
    fileObject.close();

    // TODO: Should be checked before copying
    if (m_buffer[m_fileSize - 1] != ';')
    {
        Logger::err << "File does not end with a ';' character" << Logger::End;
        return 1;
    }
    Logger::log << "Trailer byte OK" << Logger::End;

    if (_fetchLogicalScreenDescriptor())
        return 1;

    for (uint32_t offset{
            GIF_AFTER_LOGICAL_SCREEN_DESCRIPTOR_OFFS +
            (m_hasGlobalColorTable ? m_globalColorTableSizeInBytes : 0_u32)};
        offset < m_fileSize;)
    {
        Logger::log << "Separator byte (ASCII): '" << m_buffer[offset] << "' at 0x" << offset << Logger::End;
        switch (m_buffer[offset])
        {
        case ',': // Image descriptor ahead
        {
            if (_fetchImageDescriptor(offset))
                return 1;

            // Go thru the image descriptor
            offset += 10;

            // Skip the local palette if there is one
            if (m_imageFrames.size() &&
                m_imageFrames[m_imageFrames.size() - 1]->imageDescriptor.hasLocalColorTable)
                offset += m_imageFrames[m_imageFrames.size() - 1]->imageDescriptor.localColorTableSizeInBytes;


            // Skip the LZW minimum code size byte
            ++offset;

            while (offset < m_fileSize)
            {
                if (m_buffer[offset] == 0) // Block terminator
                {
                    Logger::log << "End of a block" << Logger::End;
                    ++offset; // Skip the block terminator
                    break;
                }
                // Skip a sub-block and the size byte
                offset += m_buffer[offset] + 1;
            }
        } // End of case ','

        case ';': // End of data
        {
            goto after_loop;
        } // End of case ';'

        } // End of switch
    }
    Logger::log << "End of file" << Logger::End;

after_loop:

    Logger::log << "Found " << m_imageFrames.size() << " frame(s)" << Logger::End;
    m_bitmapWidthPx = m_logicalScreen.width;
    m_bitmapHeightPx = m_logicalScreen.height;

    m_filePath = filepath;
    Logger::log << "Image loaded" << Logger::End;
    m_isInitialized = true;
    return 0;
}

int GifImage::_fetchLogicalScreenDescriptor()
{
    if (m_fileSize < 13)
    {
        Logger::err << "Not enough room for logical screen descriptor" << Logger::End;
        return 1;
    }

    std::cout << std::dec;

    std::memcpy(&m_logicalScreen.width, m_buffer + GIF_LOGICAL_SCREEN_WIDTH_OFFS, 2);
    Logger::log << "Logical screen width: " << m_logicalScreen.width << Logger::End;

    std::memcpy(&m_logicalScreen.height, m_buffer + GIF_LOGICAL_SCREEN_HEIGHT_OFFS, 2);
    Logger::log << "Logical screen height: " << m_logicalScreen.height << Logger::End;

    uint8_t flags{m_buffer[GIF_LOGICAL_SCREEN_FLAGS_OFFS]};
    m_hasGlobalColorTable = !!(flags & 0b10000000);
    Logger::log << "Has global color table? " << (m_hasGlobalColorTable ? "yes" : "no") << Logger::End;

    m_colorResolution = int((flags & 0b01110000) >> 4) + 1;
    Logger::log << "Color resolution: " << +m_colorResolution << Logger::End;

    // Sort flag is ignored

    if (m_hasGlobalColorTable)
    {
        //                             = 2 ^ ("this value" + 1)
        m_globalColorTableSizeInColors = 1 << ((flags & 0b00000111) + 1);
        Logger::log << "Global color table contains " << m_globalColorTableSizeInColors << " colors" << Logger::End;
        m_globalColorTableSizeInBytes = m_globalColorTableSizeInColors * 3;
        Logger::log << "Global color table size: " << m_globalColorTableSizeInBytes << " bytes" << Logger::End;
    }
    else
    {
        m_globalColorTableSizeInColors = 0;
        m_globalColorTableSizeInBytes = 0;
    }

    if (m_hasGlobalColorTable)
    {
        m_logicalScreen.bgColorIndex = m_buffer[GIF_LOGICAL_SCREEN_BG_COLOR_OFFS];
        Logger::log << "Background color index: " << +m_logicalScreen.bgColorIndex << Logger::End;
    }
    else
    {
        m_logicalScreen.bgColorIndex = 0;
    }

    if (m_buffer[GIF_LOGICAL_SCREEN_PIXEL_ASPECT_RATIO_OFFS])
    {
        m_logicalScreen.pixelAspectRatio = (m_buffer[GIF_LOGICAL_SCREEN_PIXEL_ASPECT_RATIO_OFFS] + 15) / 64.0;
        Logger::log << "Pixel aspect ratio: " << m_logicalScreen.pixelAspectRatio << Logger::End;
    }
    else
    {
        m_logicalScreen.pixelAspectRatio = 0;
    }

    std::cout << std::hex;

    return 0;
}

int GifImage::_fetchImageDescriptor(uint32_t startOffset)
{
    // Check if there is enough space for the descriptor
    if (m_fileSize < startOffset + 10)
    {
        Logger::err << "Image descriptor out of bounds" << Logger::End;
        return 1;
    }

    auto imageFrame = new ImageFrame;
    imageFrame->startOffset = startOffset;
    std::memcpy(&imageFrame->imageDescriptor.imageLeftPos, m_buffer + startOffset + 0, 2);
    std::memcpy(&imageFrame->imageDescriptor.imageTopPos,  m_buffer + startOffset + 2, 2);
    std::memcpy(&imageFrame->imageDescriptor.imageWidth,   m_buffer + startOffset + 4, 2);
    std::memcpy(&imageFrame->imageDescriptor.imageHeight,  m_buffer + startOffset + 6, 2);

    uint8_t flags{m_buffer[startOffset + 8]};
    imageFrame->imageDescriptor.hasLocalColorTable = !!(flags & 0b10000000);
    imageFrame->imageDescriptor.isInterlaced       = !!(flags & 0b01000000);

    // Sort flag is ignored

    // Reserved word is ignored
    
    if (imageFrame->imageDescriptor.hasLocalColorTable)
    {
        //                                                      = 2 ^ ("value" + 1)
        imageFrame->imageDescriptor.localColorTableSizeInColors = 1 << ((flags & 0b00000111) + 1);
        imageFrame->imageDescriptor.localColorTableSizeInBytes =
            imageFrame->imageDescriptor.localColorTableSizeInColors * 3;
    }

    std::cout << std::dec;

    Logger::log << "Image frame: " << '\n' <<
        '\t' << "Left position: "         << imageFrame->imageDescriptor.imageLeftPos << '\n' <<
        '\t' << "Right position: "        << imageFrame->imageDescriptor.imageTopPos << '\n' <<
        '\t' << "Width: "                 << imageFrame->imageDescriptor.imageWidth << '\n' <<
        '\t' << "Height: "                << imageFrame->imageDescriptor.imageHeight << '\n' <<
        '\t' << "Has local color table? " << (imageFrame->imageDescriptor.hasLocalColorTable?"yes":"no") << '\n'<<
        '\t' << "Interlaced? "            << (imageFrame->imageDescriptor.isInterlaced ? "yes" : "no");
    if (imageFrame->imageDescriptor.hasLocalColorTable)
    {
        Logger::log << "\n\t" << "Local color table contains " <<
            imageFrame->imageDescriptor.localColorTableSizeInColors << " colors" << '\n' <<
            '\t' << "Local color table size: " <<
            imageFrame->imageDescriptor.localColorTableSizeInBytes << " bytes";
    }
    Logger::log << Logger::End;

    std::cout << std::hex;

    m_imageFrames.push_back(imageFrame);

    return 0;
}

int GifImage::render(
        SDL_Texture* texture, uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    if (!m_isInitialized)
    {
        Logger::err << "Cannot draw uninitialized image" << Logger::End;
        return 1;
    }

    SDL_Rect lockRect{0, 0, (int)windowWidth, (int)windowHeight};
    uint8_t* pixelArray{};
    int pitch{};
    if (SDL_LockTexture(texture, &lockRect, (void**)&pixelArray, &pitch))
    {
        Logger::err << "Failed to lock texture: " << SDL_GetError() << Logger::End;
        return 1;
    }

    // Skip image descriptor
    uint32_t offset{m_imageFrames[0]->startOffset + 10};

    // Skip the local palette if there is one
    if (m_imageFrames.size() && m_imageFrames[m_imageFrames.size() - 1]->imageDescriptor.hasLocalColorTable)
        offset += m_imageFrames[m_imageFrames.size() - 1]->imageDescriptor.localColorTableSizeInBytes;

    LzwDecoder decoder{};
    decoder.setCodeSize(m_buffer[offset++]);

    while (offset < m_fileSize)
    {
        uint32_t subBlockSize{m_buffer[offset]};
        ++offset;

        Logger::log << "Buffering a sub-block of 0x" << subBlockSize << " bytes" << Logger::End;

        for (int i{}; i < subBlockSize; ++i)
            decoder << m_buffer[offset + i];

        offset += subBlockSize;
        if (m_buffer[offset] == 0) // Block terminator
        {
            Logger::log << "End of a block" << Logger::End;
            ++offset; // Skip the block terminator
            goto end_of_block;
        }
    }
end_of_block:
        ;

    auto decompressedData = decoder.getDecompressedData();
    unsigned int xPos{};
    unsigned int yPos{};
    for (int i{}; i < decompressedData.size(); ++i)
    {
        const uint32_t colorOffset{GIF_AFTER_LOGICAL_SCREEN_DESCRIPTOR_OFFS + decompressedData[i] * 3};

        uint8_t colorR{m_buffer[colorOffset + 0]};
        uint8_t colorG{m_buffer[colorOffset + 1]};
        uint8_t colorB{m_buffer[colorOffset + 2]};
        Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});

        ++xPos;
        if (xPos >= m_imageFrames[0]->imageDescriptor.imageWidth)
        {
            xPos = 0;
            ++yPos;
            if (yPos >= m_bitmapHeightPx)
                return 0; // We are done
        }
    }

    SDL_UnlockTexture(texture);
    return 0;
}

GifImage::~GifImage()
{
}
