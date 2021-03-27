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

#include "PnmImage.h"
#include "Logger.h"
#include "Gfx.h"
#include "bitmagic.h"
#include <SDL2/SDL_render.h>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#define PNM_MAX_BUFFER_SIZE -1_u32 // 4 gigs

static std::string pnmTypeToStr(PnmImage::PnmType type)
{
    switch (type)
    {
        case PnmImage::PnmType::PBM_Ascii: return "PBM ASCII";
        case PnmImage::PnmType::PGM_Ascii: return "PGM ASCII";
        case PnmImage::PnmType::PPM_Ascii: return "PPM ASCII";
        case PnmImage::PnmType::PBM_Bin:   return "PBM Binary";
        case PnmImage::PnmType::PGM_Bin:   return "PGM Binary";
        case PnmImage::PnmType::PPM_Bin:   return "PPM Binary";
        default: return "Invalid";
    }
}

int PnmImage::fetchImageSize()
{
    m_headerEndOffset = 2; // Skip the magic number

    // Skip whitespace
    while (m_headerEndOffset < m_fileSize && std::isspace(m_buffer[m_headerEndOffset]))
        ++m_headerEndOffset;

    // Skip comments
    while (m_buffer[m_headerEndOffset] == '#')
    {
        while (m_headerEndOffset < m_fileSize && m_buffer[m_headerEndOffset] != '\n')
            ++m_headerEndOffset;
        ++m_headerEndOffset;
    }

    std::stringstream ss{};

    // Get width
    while (m_headerEndOffset < m_fileSize && !std::isspace(m_buffer[m_headerEndOffset]))
    {
        ss << m_buffer[m_headerEndOffset];
        ++m_headerEndOffset;
    }
    ss >> m_bitmapWidthPx;
    ss.clear(); // Clear EOF flag

    // Skip whitespace
    while (m_headerEndOffset < m_fileSize && std::isspace(m_buffer[m_headerEndOffset]))
        ++m_headerEndOffset;

    // Skip comments
    while (m_buffer[m_headerEndOffset] == '#')
    {
        while (m_headerEndOffset < m_fileSize && m_buffer[m_headerEndOffset] != '\n')
            ++m_headerEndOffset;
        ++m_headerEndOffset;
    }

    // Get height
    while (m_headerEndOffset < m_fileSize && !std::isspace(m_buffer[m_headerEndOffset]))
    {
        ss << m_buffer[m_headerEndOffset];
        ++m_headerEndOffset;
    }
    ss >> m_bitmapHeightPx;

    Logger::log << "Bitmap size: " << m_bitmapWidthPx << "x" << m_bitmapHeightPx << Logger::End;

    if (m_type == PnmType::PGM_Ascii ||
        m_type == PnmType::PGM_Bin ||
        m_type == PnmType::PPM_Ascii ||
        m_type == PnmType::PPM_Bin)
    {
        ss.clear(); // Clear EOF flag

        // Skip comments
        while (m_buffer[m_headerEndOffset] == '#')
        {
            while (m_headerEndOffset < m_fileSize && m_buffer[m_headerEndOffset] != '\n')
                ++m_headerEndOffset;
            ++m_headerEndOffset;
        }
        ++m_headerEndOffset;

        // Get the max grayscale/color value
        while (m_headerEndOffset < m_fileSize && !std::isspace(m_buffer[m_headerEndOffset]))
        {
            ss << m_buffer[m_headerEndOffset];
            ++m_headerEndOffset;
        }
        ++m_headerEndOffset;
        ss >> m_maxPixelVal;

        Logger::log << "Max grayscale/color value: " << m_maxPixelVal << Logger::End;
        if (m_maxPixelVal == 0)
        {
            Logger::err << "Max grayscale/color value is set to zero" << Logger::End;
            return 1;
        }
    }

    return 0;
}

int PnmImage::open(const std::string& filepath)
{
    m_filePath.clear();

    auto fileObject{std::ifstream{filepath, std::ios::binary}};
    if (fileObject.fail()) // If failed to open
    {
        Logger::err << "Failed to open file: " << strerror(errno) << Logger::End;
        return 1;
    }
    Logger::log << "Opened file" << Logger::End;

    char pnmTypeChars[3]{};
    fileObject.read(pnmTypeChars, 2);
    Logger::log << "Magic bytes (ASCII): " << pnmTypeChars << Logger::End;
    if (pnmTypeChars[0] != 'P' || pnmTypeChars[1] < '1' || pnmTypeChars[1] > '6')
    {
        Logger::err << "Invalid magic bytes" << Logger::End;
        return 1;
    }
    m_type = PnmType(pnmTypeChars[1] - '1');
    Logger::log << "PNM type: " << pnmTypeToStr(m_type) << Logger::End;

    fileObject.seekg(0, std::ios::end);
    std::ios::pos_type fileSize{fileObject.tellg()};
    // If greater than the max 32-bit value (m_fileSize is 32-bit)
    // or greater than the max allowed buffer size
    if (fileSize > -1_u32 || fileSize > PNM_MAX_BUFFER_SIZE)
    {
        Logger::err << "File is too large" << Logger::End;
        return 1;
    }
    m_fileSize = fileSize;

    m_buffer = new uint8_t[m_fileSize];
    fileObject.seekg(0, std::ios::beg);
    fileObject.read((char*)m_buffer, m_fileSize);
    fileObject.close();

    // Fill m_bitmapWidthPx, m_bitmapHeightPx and m_headerEndOffset
    if (fetchImageSize())
        return 1;

    m_filePath = filepath;
    Logger::log << "Image loaded" << Logger::End;
    m_isInitialized = true;
    return 0;
}

int PnmImage::render(
        SDL_Texture* texture, int windowWidth, int windowHeight, int textureWidth) const
{
    if (!m_isInitialized)
    {
        Logger::err << "Cannot draw uninitialized image" << Logger::End;
        return 1;
    }

    SDL_Rect lockRect{0, 0, windowWidth, windowHeight};
    uint8_t* pixelArray{};
    int pitch{};
    if (SDL_LockTexture(texture, &lockRect, (void**)&pixelArray, &pitch))
    {
        Logger::err << "Failed to lock texture: " << SDL_GetError() << Logger::End;
        return 1;
    }

    uint32_t offset{m_headerEndOffset}; // Skip header
    uint32_t xPos{};
    uint32_t yPos{};
    std::stringstream ss{};
    while (offset < m_fileSize)
    {
        // Handle comments if this is an ASCII image
        if ((m_type == PnmType::PBM_Ascii ||
             m_type == PnmType::PGM_Ascii ||
             m_type == PnmType::PPM_Ascii) &&
            m_buffer[offset] == '#')
        {
            Logger::log << "Comment: \"";
            while (m_buffer[offset] != '\n')
            {
                Logger::log << m_buffer[offset];
                ++offset;
            }
            ++offset;
            Logger::log << "\"" << Logger::End;
            continue;
        }

        uint8_t currByte{m_buffer[offset]};

        switch (m_type)
        {
            case PnmType::PBM_Ascii:
            {
                if (xPos < windowWidth && yPos < windowHeight)
                {
                    if (!std::isspace(currByte))
                    {
                        if (currByte != '0' && currByte != '1')
                            Logger::warn << "Invalid value while rendering Plain PNM image:" <<
                                " as char: " << (char)currByte <<
                                " as hex: " << +currByte <<
                                ", treating it as nonzero" << Logger::End;

                        uint8_t colorVal{(currByte != '0') ? 0_u8 : 255_u8};
                        Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorVal, colorVal, colorVal});

                        ++xPos;
                        if (xPos >= m_bitmapWidthPx)
                        {
                            xPos = 0;
                            ++yPos;
                            if (yPos >= m_bitmapHeightPx)
                                goto end_of_func; // We are done
                        }
                    }
                }
                break;
            } // End of case

            case PnmType::PBM_Bin:
            {
                for (unsigned int i{}; i < 8; ++i)
                {
                    // XXX: Implement support for incomplete bytes at the end of lines

                    if (xPos < windowWidth && yPos < windowHeight)
                    {
                        uint8_t colorVal{(currByte & (1 << (7 - i))) ? 0_u8 : 255_u8};
                        Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorVal, colorVal, colorVal});
                    }

                    ++xPos;
                    if (xPos >= m_bitmapWidthPx)
                    {
                        xPos = 0;
                        ++yPos;
                        if (yPos >= m_bitmapHeightPx)
                            goto end_of_func; // We are done
                    }
                }
                break;
            } // End of case

            case PnmType::PGM_Ascii:
            {
                if (std::isspace(currByte))
                {
                    // This character (a whitespace) is the end of the current pixel value,
                    // so convert the whole value to int
                    uint16_t currValue{};
                    ss.clear();
                    ss >> currValue;

                    if (xPos < windowWidth && yPos < windowHeight)
                    {
                        uint8_t colorVal{uint8_t((float)currValue / m_maxPixelVal * 255)};
                        Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorVal, colorVal, colorVal});
                    }

                    ++xPos;
                    if (xPos >= m_bitmapWidthPx)
                    {
                        xPos = 0;
                        ++yPos;
                        if (yPos >= m_bitmapHeightPx)
                            goto end_of_func; // We are done
                    }
                }
                else
                {
                    ss.clear();
                    // Add a new digit to the stream
                    ss << m_buffer[offset];
                }
                break;
            } // End of case

            case PnmType::PPM_Ascii:
            {
                if (std::isspace(currByte))
                {
                    /*
                     * Specifies which value is the currently fetched.
                     * If 0, this is the red,
                     * if 1, this is the green,
                     * if 2, this is the blue component.
                     */
                    static short valInRgbI{};
                    static uint16_t rVal{};
                    static uint16_t gVal{};
                    static uint16_t bVal{};

                    ss.clear();
                    // This character (a whitespace) is the end of the current value,
                    // so convert the whole value to int
                    switch (valInRgbI)
                    {
                    case 0: ss >> rVal; break;
                    case 1: ss >> gVal; break;
                    case 2: ss >> bVal; break;
                    }

                    // If we have all the values for the color and
                    // the current pixel is visible
                    if (valInRgbI == 2 && xPos < windowWidth && yPos < windowHeight)
                    {
                        uint8_t colorR{uint8_t((float)rVal / m_maxPixelVal * 255)};
                        uint8_t colorG{uint8_t((float)gVal / m_maxPixelVal * 255)};
                        uint8_t colorB{uint8_t((float)bVal / m_maxPixelVal * 255)};
                        Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});
                    }

                    if (valInRgbI >= 2)
                    {
                        // Next time we start a new color, so we get red then
                        valInRgbI = 0;

                        ++xPos;
                        if (xPos >= m_bitmapWidthPx)
                        {
                            xPos = 0;
                            ++yPos;
                            if (yPos >= m_bitmapHeightPx)
                                goto end_of_func; // We are done
                        }
                    }
                    else
                    {
                        // Next time we get the next color component
                        ++valInRgbI;
                    }
                }
                else
                {
                    ss.clear();
                    // Add a new digit to the stream
                    ss << m_buffer[offset];
                }
                break;
            } // End of case

            default:
                Logger::err << "Failed to render image, unimplemented PNM type" << Logger::End;
                SDL_UnlockTexture(texture);
                return 1;
        }

        ++offset;
    }

end_of_func:
    SDL_UnlockTexture(texture);
    return 0;
}

PnmImage::~PnmImage()
{
    delete[] m_buffer;
}
