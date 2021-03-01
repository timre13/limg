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

#include "BmpImage.h"

#include "bitmagic.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <cerrno>
#include <ios>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <stdint.h>
#include <bitset>
#include <cmath>
#include <cassert>
#include <bitset>

#define BUFFER_SIZE                     1024*1024*100   // Max: 0xffffffff
#define BMP_MAGIC_BYTES                 0x424d          // BM in ASCII
#define BMP_SIZE_FIELD_OFFS             0x02
#define BMP_BITMAP_OFFS_FIELD_OFFS      0x0a
#define BMP_DIB_HEADER_OFFS             0x0e

#define BMP_BITMAPCOREHEADER_SIZE       12
    #define BMP_BITMAPCOREHEADER_WIDTH_FIELD_OFFS       0x12
    #define BMP_BITMAPCOREHEADER_HEIGHT_FIELD_OFFS      0x14
#define BMP_OS22XBITMAPHEADER_SIZE_VAR1 64
#define BMP_OS22XBITMAPHEADER_SIZE_VAR2 16
#define BMP_BITMAPINFOHEADER_SIZE       40
    #define BMP_BITMAPINFOHEADER_WIDTH_FIELD_OFFS       0x12
    #define BMP_BITMAPINFOHEADER_HEIGHT_FIELD_OFFS      0x16
    #define BMP_BITMAPINFOHEADER_CPLANE_FIELD_OFFS      0x1a
    #define BMP_BITMAPINFOHEADER_CDEPTH_FIELD_OFFS      0x1c
    #define BMP_BITMAPINFOHEADER_COMPMETH_FIELD_OFFS    0x1e
    #define BMP_BITMAPINFOHEADER_IMGSIZE_FIELD_OFFS     0x22
    #define BMP_BITMAPINFOHEADER_HRES_FIELD_OFFS        0x26
    #define BMP_BITMAPINFOHEADER_VRES_FIELD_OFFS        0x2a
    #define BMP_BITMAPINFOHEADER_CNUM_FIELD_OFFS        0x2e
#define BMP_BITMAPV2INFOHEADER_SIZE     52
#define BMP_BITMAPV3INFOHEADER_SIZE     56
#define BMP_BITMAPV4HEADER_SIZE         108
#define BMP_BITMAPV5HEADER_SIZE         124

static std::string compMethodToStr(BmpImage::CompressionMethod method)
{
    switch (method)
    {
    case BmpImage::CompressionMethod::BI_RGB:            return "RGB";
    case BmpImage::CompressionMethod::BI_RLE8:           return "RLE8";
    case BmpImage::CompressionMethod::BI_RLE4:           return "RLE4";
    case BmpImage::CompressionMethod::BI_BITFIELDS:      return "Bitfields";
    case BmpImage::CompressionMethod::BI_JPEG:           return "JPEG";
    case BmpImage::CompressionMethod::BI_PNG:            return "PNG";
    case BmpImage::CompressionMethod::BI_ALPHABITFIELDS: return "Alphabitfields";
    case BmpImage::CompressionMethod::BI_CMYK:           return "CMYK";
    case BmpImage::CompressionMethod::BI_CMYKRLE8:       return "CMYKRLE8";
    case BmpImage::CompressionMethod::BI_CMYKRLE4:       return "CMYKRLE4";
    default:                                             return "Unknown/Invalid";
    }
}

static std::string dibHeaderSizeToName(uint32_t size)
{
    switch (size)
    {
    case BMP_BITMAPCOREHEADER_SIZE:       return "BITMAPCOREHEADER";
    case BMP_OS22XBITMAPHEADER_SIZE_VAR1: return "OS22XBITMAPHEADER (variant 1)";
    case BMP_OS22XBITMAPHEADER_SIZE_VAR2: return "OS22XBITMAPHEADER (variant 2)";
    case BMP_BITMAPINFOHEADER_SIZE:       return "BITMAPINFOHEADER";
    case BMP_BITMAPV2INFOHEADER_SIZE:     return "BITMAPV2INFOHEADER";
    case BMP_BITMAPV3INFOHEADER_SIZE:     return "BITMAPV3INFOHEADER";
    case BMP_BITMAPV4HEADER_SIZE:         return "BITMAPV4HEADER";
    case BMP_BITMAPV5HEADER_SIZE:         return "BITMAPV5HEADER";
    default:                              return "Unknown/Invalid";
    }
}

int BmpImage::open(const std::string &filepath)
{
    FILE *fileObject{fopen(filepath.c_str(), "rb")};
    if (!fileObject) // If failed to open
    {
        std::cerr << "Failed to open file: " << std::strerror(errno) << '\n';
        return 1;
    }
    std::cout << "Opened file" << '\n';

    m_buffer = new uint8_t[BUFFER_SIZE];
    auto bytesRead = fread(m_buffer, 1, BUFFER_SIZE, fileObject);
    std::cout << "Read " << bytesRead << " bytes" << '\n';
    fclose(fileObject);

    std::cout << std::hex;

    //========================== Bitmap file header ============================

    if (m_buffer[0] != (BMP_MAGIC_BYTES >> 8) ||
        m_buffer[1] != (BMP_MAGIC_BYTES & 0xff))
    {
        std::cerr << "Invalid magic bytes" << '\n';
        return 1;
    }
    std::cout << "Magic bytes OK" << '\n';

    std::memcpy(&m_fileSize, m_buffer+BMP_SIZE_FIELD_OFFS, 4);
    std::cout << "File size: 0x" << m_fileSize << '\n';
    if (m_fileSize != bytesRead)
    {
        std::cerr << "File size in header is incorrect" << '\n';
        return 1;
    }

    std::memcpy(&m_bitmapOffset, m_buffer+BMP_BITMAP_OFFS_FIELD_OFFS, 4);
    std::cout << "Bitmap offset: 0x" << m_bitmapOffset << '\n';
    if (m_bitmapOffset <= 0x0a || m_bitmapOffset >= m_fileSize)
    {
        std::cout << toNbo(((uint32_t*)&m_buffer)[0x10]) << '\n';
        std::cerr << "Invalid bitmap offset" << '\n';
        return 1;
    }

    //============================== DIB Header ================================

    std::memcpy(&m_dibHeaderSize, m_buffer+BMP_DIB_HEADER_OFFS, 4);
    std::cout << "DIB header size: " << std::dec << m_dibHeaderSize << std::hex << '\n';
    std::cout << "DIB header type: " << dibHeaderSizeToName(m_dibHeaderSize) << '\n';
    // We decide the type of the DIB header using its size
    switch (m_dibHeaderSize)
    {
    case BMP_BITMAPCOREHEADER_SIZE: // XXX: Implement this
        std::memcpy(&m_bitmapWidthPx,  m_buffer+BMP_BITMAPCOREHEADER_WIDTH_FIELD_OFFS, 2);
        std::memcpy(&m_bitmapHeightPx, m_buffer+BMP_BITMAPCOREHEADER_HEIGHT_FIELD_OFFS, 2);
        break;

    case BMP_BITMAPINFOHEADER_SIZE:
    {
        std::memcpy(&m_bitmapWidthPx,  m_buffer+BMP_BITMAPINFOHEADER_WIDTH_FIELD_OFFS, 4);
        std::memcpy(&m_bitmapHeightPx, m_buffer+BMP_BITMAPINFOHEADER_HEIGHT_FIELD_OFFS, 4);
        m_bitmapWidthPx = std::abs((int32_t)m_bitmapWidthPx);
        m_bitmapHeightPx = std::abs((int32_t)m_bitmapHeightPx);
        std::cout << std::dec;
        std::cout << "Bitmap size: " << m_bitmapWidthPx << "x" << m_bitmapHeightPx << " px" << '\n';
        if (m_bitmapWidthPx == 0 || m_bitmapHeightPx == 0)
        {
            std::cerr << "Zero width/height" << '\n';
            return 1;
        }

        uint16_t colorPlaneNum{};
        std::memcpy(&colorPlaneNum, m_buffer+BMP_BITMAPINFOHEADER_CPLANE_FIELD_OFFS, 2);
        if (colorPlaneNum != 1)
        {
            std::cerr << "Color plane number is invalid (not 1)" << '\n';
            return 1;
        }
        std::memcpy(&m_bitsPerPixel, m_buffer+BMP_BITMAPINFOHEADER_CDEPTH_FIELD_OFFS, 2);
        std::cout << "Color depth: " << m_bitsPerPixel << " bits" << '\n';
        std::cout << std::hex;
        switch (m_bitsPerPixel)
        {
        case  1:
        case  2:
        case  4:
        case  8:
        case 16:
        case 24:
        case 32:
            // OK
            break;
        default:
            std::cerr << "Invalid color depth, allowed values "
                    "are 1, 2, 4, 8, 16, 24 and 32" << '\n';
            return 1;
        }

        uint32_t compMethodUint{};
        std::memcpy(&compMethodUint, m_buffer+BMP_BITMAPINFOHEADER_COMPMETH_FIELD_OFFS, 4);
        std::cout << "Compression method: 0x" << compMethodUint <<
                " / " << compMethodToStr((CompressionMethod)compMethodUint)<< '\n';
        m_compMethod = static_cast<CompressionMethod>(compMethodUint);
        // TODO: Implement compression
        switch (m_compMethod)
        {
        case CompressionMethod::BI_RGB:
        case CompressionMethod::BI_CMYK:
        case CompressionMethod::BI_BITFIELDS:
        case CompressionMethod::BI_ALPHABITFIELDS:
            // Image is uncompressed
            std::cout << "Image is not compressed" << '\n';
            break;
        case CompressionMethod::BI_RLE8:
        case CompressionMethod::BI_RLE4:
        case CompressionMethod::BI_JPEG:
        case CompressionMethod::BI_PNG:
        case CompressionMethod::BI_CMYKRLE8:
        case CompressionMethod::BI_CMYKRLE4:
            std::cerr << "Image is compressed, unimplemented" << '\n';
            return 1;
        default:
            std::cerr << "Invalid compression method" << '\n';
            return 1;
        }

        // Only 16 and 32-bit images can have bitmasks
        if (m_compMethod == CompressionMethod::BI_BITFIELDS && (m_bitsPerPixel != 16 && m_bitsPerPixel != 32))
        {
            std::cerr << "Bitmasks cannot be used with 16 or 32-bit images" << '\n';
            return 1;
        }
        if (m_compMethod == CompressionMethod::BI_RLE4 && m_bitsPerPixel != 4)
        {
            std::cerr << "RLE4 compression is only possible with 4-bit images" << '\n';
            return 1;
        }
        if (m_compMethod == CompressionMethod::BI_RLE8 && m_bitsPerPixel != 8)
        {
            std::cerr << "RLE8 compression is only possible with 8-bit images" << '\n';
            return 1;
        }

        std::memcpy(&m_imageSize, m_buffer+BMP_BITMAPINFOHEADER_IMGSIZE_FIELD_OFFS, 4);
        // Only BI_RGB images can have the size field set to 0
        if (m_compMethod != CompressionMethod::BI_RGB && m_imageSize == 0)
        {
            std::cerr << "Image is compressed, but size is set to 0" << '\n';
            return 1;
        }
        std::cout << "Size of the image data: " << m_imageSize << '\n';
        
        std::memcpy(&m_imageHResPpm, m_buffer+BMP_BITMAPINFOHEADER_HRES_FIELD_OFFS, 4);
        std::memcpy(&m_imageVResPpm, m_buffer+BMP_BITMAPINFOHEADER_VRES_FIELD_OFFS, 4);
        std::cout << std::dec << "Resolution (Pixel/Metre): " << m_imageHResPpm << "x"
                << m_imageVResPpm << '\n' << std::hex;

        std::memcpy(&m_numOfPaletteColors, m_buffer+BMP_BITMAPINFOHEADER_CNUM_FIELD_OFFS, 4);
        std::cout << std::dec << "Number of colors in palette: " <<
                m_numOfPaletteColors << std::hex << '\n';

        if ((m_bitsPerPixel == 1 && m_numOfPaletteColors > 2) ||
            (m_bitsPerPixel == 4 && m_numOfPaletteColors > 16) ||
            (m_bitsPerPixel == 8 && m_numOfPaletteColors > 256) ||
            (m_bitsPerPixel == 16 && m_numOfPaletteColors > 65536) ||
            // If the comp. method is BI_RGB, the palette must be empty
            (m_bitsPerPixel == 16 && m_compMethod == CompressionMethod::BI_RGB && m_numOfPaletteColors))
        {
            std::cerr << "Invalid palette" << '\n';
            return 1;
        }

        m_rowSize = std::ceil(m_bitsPerPixel * m_bitmapWidthPx / 32.0) * 4;

        break;
    } // End of `case BMP_BITMAPINFOHEADER_SIZE:`

    default:
        std::cerr << "Unimplemented or invalid DIB header size" << '\n';
        return 1;
    }

    std::cout << "Image loaded" << '\n';
    m_isInitialized = true;

    return 0;
}

void BmpImage::_render1BitImage(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != -1; ++i)
    {
        if (m_numOfPaletteColors) // Paletted
        {
            uint8_t paletteI{m_buffer[m_bitmapOffset + i / 8]};
            paletteI &= 1 << (7 - i % 8);
            paletteI >>= 7 - i % 8;

            if (paletteI >= m_numOfPaletteColors)
            {
                std::cerr << "Invalid color index while rendering 1-bit image: " << (int)paletteI << '\n';
                return;
            }

            SDL_SetRenderDrawColor(renderer,
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2],
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1],
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0],
                    255
            );
        }
        else // No palette, error (?)
        {
            std::cerr << "1-bit image without a palette" << '\n';
            return;
        }
        SDL_RenderDrawPoint(renderer, xPos, yPos);

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx % 4)
                // FIXME: This is wrong
                i += 4 - m_bitmapWidthPx % 4;

            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }
}

void BmpImage::_render4BitImage(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != -1; ++i)
    {
        if (m_numOfPaletteColors) // Paletted
        {
            // Use the more significant nibble if the number is even, use the another otherwise
            uint8_t paletteI{
                uint8_t((m_buffer[m_bitmapOffset + i / 2] & (i % 2 ? 0x0f : 0xf0)) >> (i % 2 ? 0 : 4))};

            if (paletteI >= m_numOfPaletteColors)
            {
                std::cerr << "Invalid color index while rendering 4-bit image: " << (int)paletteI << '\n';
                return;
            }

            SDL_SetRenderDrawColor(renderer,
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2],
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1],
                    m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0],
                    255
            );
        }
        else // RGB
        {
            uint8_t color{
                uint8_t((m_buffer[m_bitmapOffset + i / 2] & (i % 2 ? 0x0f : 0xf0)) >> (i % 2 ? 0 : 4))};

            // FIXME: Weird colors
            SDL_SetRenderDrawColor(renderer,
                    ((color & 0b00001000) >> 3) * 255,
                    ((color & 0b00000100) >> 2) * 255,
                    ((color & 0b00000010) >> 1) * 255,
                    //((color & 0b00000001) >> 0) * 255,
                    255
            );
        }
        SDL_RenderDrawPoint(renderer, xPos, yPos);

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx % 4)
                // FIXME: This is wrong
                i += 4 - m_bitmapWidthPx % 4;


            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }
}

void BmpImage::_render8BitImage(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
   uint_fast32_t xPos{};
   uint_fast32_t yPos{m_bitmapHeightPx - 1};

   for (uint_fast32_t i{}; yPos != -1; ++i)
   {
       if (xPos < windowWidth && yPos < windowHeight)
       {
           if (m_numOfPaletteColors) // Paletted image
           {
               uint8_t paletteI{m_buffer[m_bitmapOffset + i]};
               if (paletteI >= m_numOfPaletteColors)
               {
                   std::cerr <<
                       "Invalid color index while rendering 8-bit image: " << (int)paletteI <<
                       ", palette only has " << m_numOfPaletteColors << " entries" << '\n';
                   return;
               }

               SDL_SetRenderDrawColor(renderer,
                       m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2],
                       m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1],
                       m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0],
                       255
               );
           }
           else // RGB?
           {
               // FIXME: Weird colors
               SDL_SetRenderDrawColor(renderer,
                       ((m_buffer[m_bitmapOffset + i] & 0b00001100) >> 2) / 3.0 * 255,
                       ((m_buffer[m_bitmapOffset + i] & 0b00000011) >> 0) / 3.0 * 255,
                       ((m_buffer[m_bitmapOffset + i] & 0b00110000) >> 4) / 3.0 * 255,
                       //((m_buffer[m_bitmapOffset + i] & 0b11000000) >> 6) / 3.0 * 255,
                       255
               );
           }
           SDL_RenderDrawPoint(renderer, xPos, yPos);
       }

       // If the last pixel of the line
       if (xPos == m_bitmapWidthPx - 1)
       {
           /*
            * Add padding to the offset:
            * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
            */
           if (m_bitmapWidthPx % 4)
               i += 4 - m_bitmapWidthPx %  4;

           xPos = 0;
           --yPos;
       }
       else
       {
           ++xPos;
       }
   }
}

void BmpImage::_render16BitImage(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != -1; i += 2)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            if (m_compMethod == CompressionMethod::BI_RGB) // No palette, no bitmask, just simple RGB
            {
                // 5 bits/color component
                // XRRRRRGG GGGBBBBB
                uint8_t rVal{(uint8_t)((m_buffer[m_bitmapOffset + i + 1] & 0b01111100) >> 2)};
                uint8_t gVal{(uint8_t)(
                             (m_buffer[m_bitmapOffset + i + 1] & 0b00000011) << 3 |
                             (m_buffer[m_bitmapOffset + i + 0] & 0b11100000) >> 5)};
                uint8_t bVal{(uint8_t)(m_buffer[m_bitmapOffset + i + 0] & 0b00011111)};

                SDL_SetRenderDrawColor(renderer,
                //        rVal / 31.0f * 255, gVal / 31.0f * 255, bVal / 31.0f * 255, 255);
                        rVal << 3 | 7, gVal << 3 | 7, bVal << 3 | 7, 255);
                SDL_RenderDrawPoint(renderer, xPos, yPos);
            }
            else if (m_compMethod == CompressionMethod::BI_BITFIELDS) // RGB with bitmask, can have palette(?)
            {
                // TODO: Implement
            }
            else // No bitmask, paletted image (8 bit/color?)
            {
                // TODO: Implement
            }
        }

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx % 4)
                i += (4 - m_bitmapWidthPx % 4) * 2;

            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }
}

void BmpImage::_render24BitImage(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != -1; i += 3)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            // BGR format!
            SDL_SetRenderDrawColor(renderer,
                    m_buffer[m_bitmapOffset + i + 2],
                    m_buffer[m_bitmapOffset + i + 1],
                    m_buffer[m_bitmapOffset + i + 0],
                    255
            );

            SDL_RenderDrawPoint(renderer, xPos, yPos);
        }

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            i += m_bitmapWidthPx % 4;

            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }
}

void BmpImage::render(SDL_Renderer *renderer, int windowWidth, int windowHeight) const
{
    if (!m_isInitialized)
    {
        return;
    }

    switch (m_bitsPerPixel)
    {
    case 1:
        _render1BitImage(renderer, windowWidth, windowHeight);
        break;

    case 4:
        _render4BitImage(renderer, windowWidth, windowHeight);
        break;

    case 8:
        _render8BitImage(renderer, windowWidth, windowHeight);
        break;

    case 16:
        _render16BitImage(renderer, windowWidth, windowHeight);
        break;

    case 24:
        _render24BitImage(renderer, windowWidth, windowHeight);
        break;

    default:
        std::cerr << "Unimplemented color depth" << '\n';
        break;
    } // End of switch
}

BmpImage::~BmpImage()
{
    delete[] m_buffer;
}

