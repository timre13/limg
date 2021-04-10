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

#include "LzwDecoder.h"
#include "bitmagic.h"
#include "Logger.h"
#include <iostream>
#include <ctime>
#include <string>
#include <algorithm>
#include <cstring>

std::vector<uint8_t> LzwDecoder::getDecompressedData()
{
    if (m_initialCodeSize < 2 || m_initialCodeSize > 8)
    {
        Logger::err << "Invalid initial LZW code size: " << +m_initialCodeSize << Logger::End;
        return {};
    }

    uint8_t codeSize = m_initialCodeSize;

    unsigned long long currentBitOffset{};

    /*
     * Extracts `m_codeSize` number of bits from the input buffer.
     */
    auto extractDataFromBuffer{[this, codeSize, currentBitOffset](){ // -> uint16_t
        uint16_t output{};
        std::memcpy(&output, &m_inputBuffer + currentBitOffset / 8, 2);
        return output >> (7 - codeSize);
    }};
    
    auto isClearCode{[codeSize](uint16_t value){
        return value == (1 << codeSize);
    }};

    auto isEndOfInformationCode{[codeSize](uint16_t value){
        return value == (1 << codeSize) + 1;
    }};

    using byteString_t = std::vector<uint8_t>;

    std::vector<byteString_t> dictionary;
    byteString_t output;

    for (unsigned long long i{}; i < (1 << codeSize); ++i)
        dictionary.push_back({(uint8_t)i});

    uint16_t prevCode{};
    uint16_t currCode{};

    byteString_t string1;
    byteString_t string2;

    /*
     * XXX:
     * https://web.archive.org/web/20160304075538/http://qalle.net/gif89a.php#lzw
     *  - Implement End of Information code
     *  - "The first available compression code value is <Clear code> + 2."
     *  - "The output codes are of variable length, starting at <code size> + 1 bits per code, up to 12 bits per code."
     */

    prevCode = extractDataFromBuffer();
    currentBitOffset += codeSize;
    string1 = dictionary[prevCode];
    string2.push_back(string1[0]);

    output.insert(output.end(), string1.begin(), string1.end());
    while (currentBitOffset / 8 < m_inputBuffer.size())
    {
        currCode = extractDataFromBuffer();
        currentBitOffset += codeSize;

        if (isClearCode(currCode))
        {
            Logger::log << "Decompressor: Clear code found" << Logger::End;

            // Reset stuff
            codeSize = m_initialCodeSize;
            dictionary.clear();
            for (unsigned long long i{}; i < (1 << codeSize); ++i)
                dictionary.push_back({(uint8_t)i});

            continue;
        }
        if (isEndOfInformationCode(currCode))
        {
            Logger::log << "Decompressor: End of information code found" << Logger::End;
            break;
        }

        if (currCode >= dictionary.size()) // If the code is NOT in the dictionary
        {
            string1 = dictionary[prevCode];
            string1.insert(string1.end(), string2.begin(), string2.end()); // string1 += string2
        }
        else // If the code is in the dictionary
        {
            string1 = dictionary[currCode];
        }

        if (dictionary.size() - 1 > (1ul << codeSize))
            ++codeSize;

        //output.write((const char*)string1.data(), string1.size());
        output.insert(output.end(), string1.begin(), string1.end());

        string2 = {string1[0]};

        auto stringToInsert = dictionary[prevCode];
        stringToInsert.insert(stringToInsert.end(), string2.begin(), string2.end());
        dictionary.push_back(stringToInsert);

        prevCode = currCode;
    }

    Logger::log << std::dec << "Decompressor: Decompressed " << m_inputBuffer.size() << " bytes to " <<
        output.size() << std::hex << Logger::End;
    return output;
}

