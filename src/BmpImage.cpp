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

#include "Logger.h"
#include "Gfx.h"
#include "bitmagic.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <cerrno>
#include <ios>
#include <cstring>
#include <iomanip>
#include <stdint.h>
#include <bitset>
#include <cmath>
#include <cassert>
#include <bitset>

#define BMP_MAX_BUFFER_SIZE             -1_u32
#define BMP_MAGIC_BYTE_1                'B'
#define BMP_MAGIC_BYTE_2                'M'
#define BMP_SIZE_FIELD_OFFS             0x02
#define BMP_BITMAP_OFFS_FIELD_OFFS      0x0a
#define BMP_DIB_HEADER_OFFS             0x0e

#define BMP_BITMAPCOREHEADER_SIZE       12
    #define BMP_BITMAPCOREHEADER_WIDTH_FIELD_OFFS       0x12
    #define BMP_BITMAPCOREHEADER_HEIGHT_FIELD_OFFS      0x14
    #define BMP_BITMAPCOREHEADER_CPLANE_FIELD_OFFS      0x16
    #define BMP_BITMAPCOREHEADER_CDEPTH_FIELD_OFFS      0x18
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

int BmpImage::_readBitmapCoreHeader()
{
    std::memcpy(&m_bitmapWidthPx,  m_buffer+BMP_BITMAPCOREHEADER_WIDTH_FIELD_OFFS, 2);
    std::memcpy(&m_bitmapHeightPx, m_buffer+BMP_BITMAPCOREHEADER_HEIGHT_FIELD_OFFS, 2);
    m_bitmapHeightPx = std::abs((int32_t)m_bitmapHeightPx);
    Logger::log << std::dec;
    Logger::log << "Bitmap size: " << m_bitmapWidthPx << "x" << m_bitmapHeightPx << " px" << Logger::End;
    if (m_bitmapWidthPx == 0 || m_bitmapHeightPx == 0)
    {
        Logger::err << "Zero width/height" << Logger::End;
        return 1;
    }
    if ((int32_t)m_bitmapWidthPx < 0)
    {
        Logger::err << "Negative width" << Logger::End;
        return 1;
    }
    uint32_t widthTimesHeight{m_bitmapWidthPx * m_bitmapHeightPx};
    if (widthTimesHeight / m_bitmapWidthPx != m_bitmapHeightPx)
    {
        Logger::err << "Width times height overflows, this is not safe" << Logger::End;
        return 1;
    }

    uint16_t colorPlaneNum{};
    std::memcpy(&colorPlaneNum, m_buffer+BMP_BITMAPCOREHEADER_CPLANE_FIELD_OFFS, 2);
    if (colorPlaneNum != 1)
    {
        Logger::err << "Color plane number is invalid (not 1)" << Logger::End;
        return 1;
    }
    std::memcpy(&m_bitsPerPixel, m_buffer+BMP_BITMAPCOREHEADER_CDEPTH_FIELD_OFFS, 2);
    Logger::log << "Color depth: " << m_bitsPerPixel << " bits" << Logger::End;
    Logger::log << std::hex;
    switch (m_bitsPerPixel)
    {
    case  1:
    case  4:
    case  8:
    case 24:
        // OK
        break;
    default:
        Logger::err << "Invalid color depth, allowed values "
                "are 1, 4, 8 and 24" << Logger::End;
        return 1;
    }

    if (m_bitmapOffset < BMP_DIB_HEADER_OFFS+m_dibHeaderSize)
    {
        Logger::err << "Bitmap cannot be inside the headers" << Logger::End;
        return 1;
    }
    const uint64_t calcImageWNoPadding{(uint64_t)std::ceil(m_bitmapWidthPx*m_bitsPerPixel/8.0)};
    const uint64_t calcImageSize{(calcImageWNoPadding+calcImageWNoPadding%4)*m_bitmapHeightPx};
    if (m_fileSize < m_bitmapOffset+calcImageSize||
        m_fileSize < m_bitmapOffset+m_imageSize)
    {
        Logger::err << "Too small file, no room for pixel data" << Logger::End;
        return 1;
    }

    return 0;
}

int BmpImage::_readBitmapInfoHeader()
{
    std::memcpy(&m_bitmapWidthPx,  m_buffer+BMP_BITMAPINFOHEADER_WIDTH_FIELD_OFFS, 4);
    std::memcpy(&m_bitmapHeightPx, m_buffer+BMP_BITMAPINFOHEADER_HEIGHT_FIELD_OFFS, 4);
    m_bitmapHeightPx = std::abs((int32_t)m_bitmapHeightPx);
    Logger::log << std::dec;
    Logger::log << "Bitmap size: " << m_bitmapWidthPx << "x" << m_bitmapHeightPx << " px" << Logger::End;
    if (m_bitmapWidthPx == 0 || m_bitmapHeightPx == 0)
    {
        Logger::err << "Zero width/height" << Logger::End;
        return 1;
    }
    if ((int32_t)m_bitmapWidthPx < 0)
    {
        Logger::err << "Negative width" << Logger::End;
        return 1;
    }
    uint32_t widthTimesHeight{m_bitmapWidthPx * m_bitmapHeightPx};
    if (widthTimesHeight / m_bitmapWidthPx != m_bitmapHeightPx)
    {
        Logger::err << "Width times height overflows, this is not safe" << Logger::End;
        return 1;
    }

    uint16_t colorPlaneNum{};
    std::memcpy(&colorPlaneNum, m_buffer+BMP_BITMAPINFOHEADER_CPLANE_FIELD_OFFS, 2);
    if (colorPlaneNum != 1)
    {
        Logger::err << "Color plane number is invalid (not 1)" << Logger::End;
        return 1;
    }
    std::memcpy(&m_bitsPerPixel, m_buffer+BMP_BITMAPINFOHEADER_CDEPTH_FIELD_OFFS, 2);
    Logger::log << "Color depth: " << m_bitsPerPixel << " bits" << Logger::End;
    Logger::log << std::hex;
    switch (m_bitsPerPixel)
    {
    case  1:
    case  4:
    case  8:
    case 16:
    case 24:
    case 32:
        // OK
        break;
    default:
        Logger::err << "Invalid color depth, allowed values "
                "are 1, 4, 8, 16, 24 and 32" << Logger::End;
        return 1;
    }

    uint32_t compMethodUint{};
    std::memcpy(&compMethodUint, m_buffer+BMP_BITMAPINFOHEADER_COMPMETH_FIELD_OFFS, 4);
    Logger::log << "Compression method: 0x" << compMethodUint <<
            " / " << compMethodToStr((CompressionMethod)compMethodUint) << Logger::End;
    m_compMethod = static_cast<CompressionMethod>(compMethodUint);
    // TODO: Implement compression
    switch (m_compMethod)
    {
    case CompressionMethod::BI_RGB:
    case CompressionMethod::BI_CMYK:
    case CompressionMethod::BI_BITFIELDS:
        // Image is uncompressed
        Logger::log << "Image is not compressed" << Logger::End;
        break;
    case CompressionMethod::BI_RLE8:
    case CompressionMethod::BI_RLE4:
    case CompressionMethod::BI_JPEG:
    case CompressionMethod::BI_PNG:
    case CompressionMethod::BI_CMYKRLE8:
    case CompressionMethod::BI_CMYKRLE4:
        Logger::err << "Image is compressed, unimplemented" << Logger::End;
        return 1;
    default:
        Logger::err << "Invalid compression method" << Logger::End;
        return 1;
    }

    // Only 16 and 32-bit images can have bitmasks
    if (m_compMethod == CompressionMethod::BI_BITFIELDS && (m_bitsPerPixel != 16 && m_bitsPerPixel != 32))
    {
        Logger::err << "Bitmasks can only be used with 16 or 32-bit images" << Logger::End;
        return 1;
    }
    if (m_compMethod == CompressionMethod::BI_RLE4 && m_bitsPerPixel != 4)
    {
        Logger::err << "RLE4 compression is only possible with 4-bit images" << Logger::End;
        return 1;
    }
    if (m_compMethod == CompressionMethod::BI_RLE8 && m_bitsPerPixel != 8)
    {
        Logger::err << "RLE8 compression is only possible with 8-bit images" << Logger::End;
        return 1;
    }

    std::memcpy(&m_imageSize, m_buffer+BMP_BITMAPINFOHEADER_IMGSIZE_FIELD_OFFS, 4);
    // Only BI_RGB images can have the size field set to 0
    if (m_compMethod != CompressionMethod::BI_RGB && m_imageSize == 0)
    {
        Logger::err << "Image is compressed, but size is set to 0" << Logger::End;
        return 1;
    }
    Logger::log << "Size of the image data: 0x" << m_imageSize << Logger::End;

    if (m_bitmapOffset < BMP_DIB_HEADER_OFFS+m_dibHeaderSize)
    {
        Logger::err << "Bitmap cannot be inside the headers" << Logger::End;
        return 1;
    }
    const uint64_t calcImageWNoPadding{(uint64_t)std::ceil(m_bitmapWidthPx*m_bitsPerPixel/8.0)};
    const uint64_t calcImageSize{(calcImageWNoPadding+calcImageWNoPadding%4)*m_bitmapHeightPx};
    if (m_fileSize < m_bitmapOffset+calcImageSize||
        m_fileSize < m_bitmapOffset+m_imageSize)
    {
        Logger::err << "Too small file, no room for pixel data" << Logger::End;
        return 1;
    }

    std::memcpy(&m_imageHResPpm, m_buffer+BMP_BITMAPINFOHEADER_HRES_FIELD_OFFS, 4);
    std::memcpy(&m_imageVResPpm, m_buffer+BMP_BITMAPINFOHEADER_VRES_FIELD_OFFS, 4);
    Logger::log << std::dec << "Resolution (Pixel/Metre): " << m_imageHResPpm << "x"
            << m_imageVResPpm << Logger::End << std::hex;

    std::memcpy(&m_numOfPaletteColors, m_buffer+BMP_BITMAPINFOHEADER_CNUM_FIELD_OFFS, 4);
    Logger::log << std::dec << "Number of colors in palette: " <<
            m_numOfPaletteColors << std::hex << Logger::End;

    if ((m_bitsPerPixel == 1 && (m_numOfPaletteColors > 2 || m_numOfPaletteColors == 0)) ||
        (m_bitsPerPixel == 4 && m_numOfPaletteColors > 16) ||
        (m_bitsPerPixel == 8 && m_numOfPaletteColors > 256) ||
        (m_bitsPerPixel == 16 && m_numOfPaletteColors > 65536) ||
        // If the comp. method is BI_RGB, the palette must be empty
        (m_bitsPerPixel == 16 && m_compMethod == CompressionMethod::BI_RGB && m_numOfPaletteColors))
    {
        Logger::err << "Invalid palette" << Logger::End;
        return 1;
    }

    // If we have bitmasks
    if (m_compMethod == CompressionMethod::BI_BITFIELDS)
    {
        switch (m_dibHeaderSize)
        {
        // BITMAPV3INFOHEADER added support for alpha bitmask,
        // the later versions also support it
        case BMP_BITMAPV3INFOHEADER_SIZE:
        case BMP_BITMAPV4HEADER_SIZE:
        case BMP_BITMAPV5HEADER_SIZE:
            m_hasAlphaBitmask = true;
            break;
        default:
            // Older header
            m_hasAlphaBitmask = false;
            break;
        }

        std::memcpy(&m_rBitmask, m_buffer+m_bitmapOffset-(m_hasAlphaBitmask ? 16 : 12)+0, 4);
        std::memcpy(&m_gBitmask, m_buffer+m_bitmapOffset-(m_hasAlphaBitmask ? 16 : 12)+4, 4);
        std::memcpy(&m_bBitmask, m_buffer+m_bitmapOffset-(m_hasAlphaBitmask ? 16 : 12)+8, 4);
        if (m_hasAlphaBitmask)
            std::memcpy(&m_aBitmask, m_buffer+m_bitmapOffset-16+12, 4);

        Logger::log << "Bitmasks: " << '\n' <<
            "\tR: " << std::bitset<sizeof(m_rBitmask)*8>(m_rBitmask) << '\n' <<
            "\tG: " << std::bitset<sizeof(m_gBitmask)*8>(m_gBitmask) << '\n' <<
            "\tB: " << std::bitset<sizeof(m_bBitmask)*8>(m_bBitmask);
        if (m_hasAlphaBitmask)
            Logger::log << "\n\tA: " << std::bitset<sizeof(m_aBitmask)*8>(m_aBitmask);
        Logger::log << Logger::End;
    }

    return 0;
}

int BmpImage::open(const std::string &filepath)
{
    m_filePath.clear();

    FILE *fileObject{fopen(filepath.c_str(), "rb")};
    if (!fileObject) // If failed to open
    {
        Logger::err << "Failed to open file: " << std::strerror(errno) << Logger::End;
        return 1;
    }
    Logger::log << "Opened file" << Logger::End;

    Logger::log << std::hex;

    //========================== Bitmap file header ============================

    uint8_t magicBytes[2]{};
    std::fread(magicBytes, 1, 2, fileObject);
    if (magicBytes[0] != BMP_MAGIC_BYTE_1 || magicBytes[1] != BMP_MAGIC_BYTE_2)
    {
        Logger::err << "Invalid magic bytes" << Logger::End;
        return 1;
    }
    Logger::log << "Magic bytes OK" << Logger::End;

    std::fread(&m_fileSize, 1, 4, fileObject);
    Logger::log << "File size: 0x" << m_fileSize << Logger::End;
    if (m_fileSize > BMP_MAX_BUFFER_SIZE)
    {
        Logger::err << "File size is greater than `BMP_MAX_BUFFER_SIZE`" << Logger::End;
        return 1;
    }

    // Test if there is room for the file header plus the smallest type of DIB header
    if (m_fileSize < BMP_DIB_HEADER_OFFS + 12)
    {
        Logger::err << "File size is too small, no room for headers" << '\n';
        return 1;
    }

    // From now it is safe to use the whole file header plus 12 bytes from the DIB header

    m_buffer = new uint8_t[m_fileSize];

    std::rewind(fileObject); // Go to the beginning of the file
    auto bytesRead = std::fread(m_buffer, 1, m_fileSize, fileObject);
    Logger::log << "Read 0x" << bytesRead << " bytes" << Logger::End;
    std::fclose(fileObject);
    if (bytesRead != m_fileSize)
    {
        Logger::err << "Failed to read file" << Logger::End;
        return 1;
    }

    std::memcpy(&m_bitmapOffset, m_buffer+BMP_BITMAP_OFFS_FIELD_OFFS, 4);
    Logger::log << "Bitmap offset: 0x" << m_bitmapOffset << Logger::End;
    if (m_bitmapOffset >= m_fileSize)
    {
        Logger::err << "Invalid bitmap offset" << Logger::End;
        return 1;
    }

    //============================== DIB Header ================================

    std::memcpy(&m_dibHeaderSize, m_buffer+BMP_DIB_HEADER_OFFS, 4);
    Logger::log << "DIB header size: " << std::dec << m_dibHeaderSize << std::hex << Logger::End;
    Logger::log << "DIB header type: " << dibHeaderSizeToName(m_dibHeaderSize) << Logger::End;

    // Test if there is room for the DIB header
    if (m_fileSize < BMP_DIB_HEADER_OFFS + m_dibHeaderSize)
    {
        Logger::err << "File size is too small, no room for the DIB header" << '\n';
        return 1;
    }

    // We identify the type of the DIB header using its size
    switch (m_dibHeaderSize)
    {
    
    case BMP_BITMAPCOREHEADER_SIZE:
    {
        int status{_readBitmapCoreHeader()};
        if (status)
            return status;
        break;
    }
    
    case BMP_OS22XBITMAPHEADER_SIZE_VAR1: // Extended BITMAPCOREHEADER
    {
        int status{_readBitmapCoreHeader()};
        if (status)
            return status;
        break;
    }

    case BMP_BITMAPINFOHEADER_SIZE:
    {
        int status{_readBitmapInfoHeader()};
        if (status)
            return status;
        break;
    }

    case BMP_BITMAPV2INFOHEADER_SIZE: // Extended BITMAPINFOHEADER
    {
        int status{_readBitmapInfoHeader()};
        if (status)
            return status;
        break;
    }

    case BMP_BITMAPV3INFOHEADER_SIZE: // Extended BITMAPINFOHEADER
    {
        int status{_readBitmapInfoHeader()};
        if (status)
            return status;
        break;
    }

    case BMP_BITMAPV4HEADER_SIZE: // Extended BITMAPINFOHEADER
    {
        int status{_readBitmapInfoHeader()};
        if (status)
            return status;
        break;
    }

    case BMP_BITMAPV5HEADER_SIZE: // Extended BITMAPINFOHEADER
    {
        int status{_readBitmapInfoHeader()};
        if (status)
            return status;
        break;
    }

    default:
        Logger::err << "Unimplemented or invalid DIB header size" << Logger::End;
        return 1;
    }

    m_filePath = filepath;
    Logger::log << "Image loaded" << Logger::End;
    m_isInitialized = true;

    return 0;
}

int BmpImage::_render1BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; ++i)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            uint8_t colorR{};
            uint8_t colorG{};
            uint8_t colorB{};

            if (m_numOfPaletteColors) // Paletted
            {
                uint8_t paletteI{m_buffer[m_bitmapOffset + i / 8]};
                paletteI &= 1 << (7 - i % 8);
                paletteI >>= 7 - i % 8;

                if (paletteI >= m_numOfPaletteColors)
                {
                    Logger::err << "Invalid color index while rendering 1-bit image: " << (int)paletteI << Logger::End;
                    return 1;
                }

                colorR = m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2];
                colorG = m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1];
                colorB = m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0];
            }
            else // No palette, error (?)
            {
                Logger::err << "1-bit image without a palette" << Logger::End;
                return 1;
            }
            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});
        }

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx % 32)
                i += 32 - m_bitmapWidthPx % 32;

            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }

    return 0;
}

int BmpImage::_render4BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; ++i)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            // Use the more significant nibble if the number is even, use the another otherwise
            uint8_t paletteI{
                uint8_t((m_buffer[m_bitmapOffset + i / 2] & (i % 2 ? 0x0f : 0xf0)) >> (i % 2 ? 0 : 4))};

            // If the palette color number is specified, it is the max palette index,
            // if not specified, default to 2^4
            if ((m_numOfPaletteColors && paletteI >= m_numOfPaletteColors) || (paletteI >= 16))
            {
                Logger::err << "Invalid color index while rendering 4-bit image: " << (int)paletteI << Logger::End;
                return 1;
            }

            uint8_t colorR{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2]};
            uint8_t colorG{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1]};
            uint8_t colorB{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0]};
            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});
        }

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx % 8)
                i += 8 - m_bitmapWidthPx % 8;


            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }

    return 0;
}

int BmpImage::_render8BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
   uint_fast32_t xPos{};
   uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; ++i)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            uint8_t paletteI{m_buffer[m_bitmapOffset + i]};
            if (m_numOfPaletteColors && paletteI >= m_numOfPaletteColors)
            {
                Logger::err <<
                    "Invalid color index while rendering 8-bit image: " << (int)paletteI <<
                    ", palette only has " << m_numOfPaletteColors << " entries" << Logger::End;
                return 1;
            }

            uint8_t colorR{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 2]};
            uint8_t colorG{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 1]};
            uint8_t colorB{m_buffer[BMP_DIB_HEADER_OFFS + m_dibHeaderSize + paletteI * 4 + 0]};
            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});
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

    return 0;
}

int BmpImage::_render16BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; i += 2)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            uint8_t colorR{};
            uint8_t colorG{};
            uint8_t colorB{};
            uint8_t colorA{255};

            if (m_compMethod == CompressionMethod::BI_RGB) // No palette, no bitmask, just simple RGB
            {
                // 5 bits/color component
                // XRRRRRGG GGGBBBBB
                uint8_t rVal{(uint8_t)((m_buffer[m_bitmapOffset + i + 1] & 0b01111100) >> 2)};
                uint8_t gVal{(uint8_t)(
                             (m_buffer[m_bitmapOffset + i + 1] & 0b00000011) << 3 |
                             (m_buffer[m_bitmapOffset + i + 0] & 0b11100000) >> 5)};
                uint8_t bVal{(uint8_t)(m_buffer[m_bitmapOffset + i + 0] & 0b00011111)};

                //        rVal / 31.0f * 255, gVal / 31.0f * 255, bVal / 31.0f * 255, 255);
                colorR = rVal << 3 | 7;
                colorG = gVal << 3 | 7;
                colorB = bVal << 3 | 7;
            }
            else if (m_compMethod == CompressionMethod::BI_BITFIELDS) // RGB with bitmask
            {
                uint16_t bytes{
                    uint16_t(
                    uint16_t(m_buffer[m_bitmapOffset + i + 1]) << 8 |
                    uint16_t(m_buffer[m_bitmapOffset + i + 0]))};

                if (m_rBitmask)
                    colorR = float(bytes & m_rBitmask) / m_rBitmask * 255;
                if (m_gBitmask)
                    colorG = float(bytes & m_gBitmask) / m_gBitmask * 255;
                if (m_bBitmask)
                    colorB = float(bytes & m_bBitmask) / m_bBitmask * 255;
                if (m_aBitmask)
                    colorA = float(bytes & m_aBitmask) / m_aBitmask * 255;
            }

            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB, colorA});
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

    return 0;
}

int BmpImage::_render24BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; i += 3)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            // BGR format!
            uint8_t colorR{m_buffer[m_bitmapOffset + i + 2]};
            uint8_t colorG{m_buffer[m_bitmapOffset + i + 1]};
            uint8_t colorB{m_buffer[m_bitmapOffset + i + 0]};

            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB});
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

    return 0;
}

int BmpImage::_render32BitImage(
        uint8_t* pixelArray,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
{
    uint_fast32_t xPos{};
    uint_fast32_t yPos{m_bitmapHeightPx - 1};

    for (uint_fast32_t i{}; yPos != (uint_fast32_t)-1; i += 4)
    {
        if (xPos < windowWidth && yPos < windowHeight)
        {
            uint8_t colorA{255};
            uint8_t colorR{};
            uint8_t colorG{};
            uint8_t colorB{};

            if (m_compMethod == CompressionMethod::BI_RGB ||
                // If the compression method is BI_BITFIELDS and one of the bitmasks is 0,
                // the image is not bitmasked. The docs don't really say a lot about
                // alpha bitmasks, so this is just the result of my tests.
                // TODO: This is probably not the best way
                (m_compMethod == CompressionMethod::BI_BITFIELDS &&
                 (m_rBitmask == 0 || m_gBitmask == 0 || m_bBitmask == 0)))
                // RGB, no bitmask
            {
                colorA = m_buffer[m_bitmapOffset + i + 3];
                colorR = m_buffer[m_bitmapOffset + i + 2];
                colorG = m_buffer[m_bitmapOffset + i + 1];
                colorB = m_buffer[m_bitmapOffset + i + 0];
            }
            else if (m_compMethod == CompressionMethod::BI_BITFIELDS) // Bitmasks
            {
                uint32_t bytes{
                    uint32_t(
                    uint32_t(m_buffer[m_bitmapOffset + i + 3]) << 24 |
                    uint32_t(m_buffer[m_bitmapOffset + i + 2]) << 16 |
                    uint32_t(m_buffer[m_bitmapOffset + i + 1]) << 8 |
                    uint32_t(m_buffer[m_bitmapOffset + i + 0]))};

                if (m_rBitmask)
                    colorR = float(bytes & m_rBitmask) / m_rBitmask * 255;
                if (m_gBitmask)
                    colorG = float(bytes & m_gBitmask) / m_gBitmask * 255;
                if (m_bBitmask)
                    colorB = float(bytes & m_bBitmask) / m_bBitmask * 255;
                if (m_aBitmask)
                    colorA = float(bytes & m_aBitmask) / m_aBitmask * 255;
            }

            Gfx::drawPointAt(pixelArray, textureWidth, xPos, yPos, {colorR, colorG, colorB, colorA});
        }

        // If the last pixel of the line
        if (xPos == m_bitmapWidthPx - 1)
        {
            /*
             * Add padding to the offset:
             * Every line needs to be padded to a multiple of 4 with additional bytes at the end.
             */
            if (m_bitmapWidthPx / 4 % 4)
                i += 4 - m_bitmapWidthPx / 4 % 4 - 1; // But why?

            xPos = 0;
            --yPos;
        }
        else
        {
            ++xPos;
        }
    }

    return 0;
}
int BmpImage::render(
        SDL_Texture* texture,
        uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth) const
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

    int renderStatus{};
    switch (m_bitsPerPixel)
    {
    case  1: renderStatus =  _render1BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    case  4: renderStatus =  _render4BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    case  8: renderStatus =  _render8BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    case 16: renderStatus = _render16BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    case 24: renderStatus = _render24BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    case 32: renderStatus = _render32BitImage(pixelArray, windowWidth, windowHeight, textureWidth); break;
    default:
        Logger::err << "Unimplemented color depth" << Logger::End;
        SDL_UnlockTexture(texture);
        return 1;
    }

    SDL_UnlockTexture(texture);
    return renderStatus;
}

BmpImage::~BmpImage()
{
    delete[] m_buffer;
}

