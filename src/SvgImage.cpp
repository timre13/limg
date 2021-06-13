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

#include "SvgImage.h"
#include "Logger.h"
#include <fstream>
#include <memory>
#include <sstream>

int SvgImage::open(const std::string &filepath)
{
    std::string content;
    {
        std::ifstream inputFile{filepath};

        if (inputFile.bad() | !inputFile.is_open())
        {
            Logger::log << "Failed to open file: " << strerror(errno) << Logger::End;
            return 1;
        }

        std::stringstream buffer;
        buffer << inputFile.rdbuf();
        content = buffer.str();
        inputFile.close();
    }

    m_parser = std::make_unique<XmlParser>(content);

    std::cout << "Found " << m_parser->size() << " elements\n";

    {
        auto svgElement = m_parser->findFirstElementWithName("svg");
        if (svgElement)
        {
            // XXX: Unit support

            std::stringstream widthSs;
            widthSs << svgElement->getAttribute("width");
            widthSs >> m_bitmapWidthPx;

            std::stringstream heightSs;
            heightSs << svgElement->getAttribute("height");
            heightSs >> m_bitmapHeightPx;
        }
        else
        {
            Logger::err << "SVG image without an \"svg\" XML element" << Logger::End;
            return 1;
        }
    }

    Logger::log << "Bitmap size: " << m_bitmapWidthPx << 'x' << m_bitmapHeightPx << " px" << Logger::End;

    return 0;
}

int SvgImage::render(
        SDL_Texture* texture,
        uint32_t viewportWidth, uint32_t viewportHeight) const
{
    (void)texture;
    (void)viewportWidth;
    (void)viewportHeight;

    return 0;
}

