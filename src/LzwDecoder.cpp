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
#include <unordered_map>
#include <iostream>
#include <ctime>
#include <string>
#include <algorithm>
#include <cstring>
#include <bitset>

std::vector<uint8_t> LzwDecoder::getDecompressedData()
{
    Logger::log << "Decompressor: Starting decompression of 0x" << m_inputBuffer.size() << " bytes" << Logger::End;
    Logger::log << "Decompressor: Code size: " << +m_initialCodeSize << Logger::End;

    /*
    if (m_initialCodeSize < 2 || m_initialCodeSize > 8)
    {
        Logger::err << "Invalid initial LZW code size: " << +m_initialCodeSize << Logger::End;
        return {};
    }
    */

    uint8_t codeSize = m_initialCodeSize;

    // A string that can contain null-bytes
    using byteString_t = std::vector<uint8_t>;

    uint32_t currentBitOffset{};

    /*
     * Extracts `m_codeSize` number of bits from the input buffer.
     */
    auto extractDataFromBuffer{[](
            uint32_t currentBitOffset,
            uint8_t codeSize,
            const byteString_t &inputBuffer
            ){ // -> uint16_t

        uint32_t output{};
        const uint32_t startByteOffset{currentBitOffset / 8};
        const uint8_t bitsToShift{uint8_t(24 - currentBitOffset % 8 - codeSize)};

        // Data extraction:
        {
            uint8_t outputBuff[3]{};
            std::memcpy(&outputBuff, inputBuffer.data() + startByteOffset, 3);
            output =
                (uint32_t)outputBuff[0] << 16 |
                (uint32_t)outputBuff[1] <<  8 |
                (uint32_t)outputBuff[2] <<  0;
        }

        output >>= bitsToShift;

        uint16_t bitmask{};
        for (int i{}; i < codeSize; ++i)
            bitmask |= 1 << i;

        output &= bitmask;

        //Logger::log << std::bitset<32>(output) << Logger::End;

        return uint16_t(output);
    }};
    
    auto isClearCode{[](uint16_t value, uint8_t codeSize){
        return value == (1 << codeSize);
    }};

    auto isEndOfInformationCode{[](uint16_t value, uint8_t codeSize){
        return value == ((1 << codeSize) + 1);
    }};

    byteString_t output;

    std::unordered_map<uint16_t, byteString_t> dictionary;
    for (uint16_t i{}; i < (1 << codeSize); ++i)
        dictionary[i] = {(uint8_t)i};

    uint16_t nextPossibleCode = (1 << codeSize) + 2;
    uint16_t prevCode{};
    uint16_t currCode{};
    byteString_t string1;
    byteString_t string2;

    prevCode = extractDataFromBuffer(currentBitOffset, codeSize, m_inputBuffer);
    currentBitOffset += codeSize;
    string1 = dictionary[prevCode];
    string2.push_back(string1[0]);

    output.insert(output.end(), string1.begin(), string1.end());
    while (currentBitOffset < m_inputBuffer.size() * 8)
    {
        currCode = extractDataFromBuffer(currentBitOffset, codeSize, m_inputBuffer);
        currentBitOffset += codeSize;

        if (isClearCode(prevCode, codeSize))
        {
            Logger::log << "Decompressor: Clear code found" << Logger::End;

            // Reset stuff
            codeSize = m_initialCodeSize;
            dictionary.clear();
            for (uint16_t i{}; i < (1 << codeSize); ++i)
                dictionary[i] = {(uint8_t)i};
            nextPossibleCode = (1 << codeSize) + 2;

            prevCode = currCode;
            continue;
        }
        if (isEndOfInformationCode(prevCode, codeSize))
        {
            Logger::log << "Decompressor: End of information code found" << Logger::End;

            prevCode = currCode;
            break;
        }

        if (dictionary.find(currCode) == dictionary.end()) // If the code is NOT in the dictionary
        {
            string1 = dictionary[prevCode];
            string1.insert(string1.end(), string2.begin(), string2.end()); // string1 += string2
        }
        else // If the code is in the dictionary
        {
            string1 = dictionary[currCode];
        }

        //output.write((const char*)string1.data(), string1.size());
        output.insert(output.end(), string1.begin(), string1.end());

        string2 = {string1[0]};

        byteString_t stringToInsert = dictionary.at(prevCode);
        stringToInsert.insert(stringToInsert.end(), string2.begin(), string2.end());
        dictionary[nextPossibleCode++] = stringToInsert;

        if (dictionary.size() == (1ul << codeSize))
        {
            ++codeSize;
            Logger::log << "Incremented code size to " << +codeSize << Logger::End;
        }

        prevCode = currCode;
    }

    Logger::log << std::dec << "Decompressor: Decompressed " << m_inputBuffer.size() << " bytes to " <<
        output.size() << std::hex << Logger::End;
    return output;
}

