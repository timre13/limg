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

#include <stdint.h>

//========================== Byte order conversion =============================

/*
 * Convert bytes to network byte order.
 */
inline uint16_t toNbo(uint16_t value)
{
    uint8_t *bytes{(uint8_t*)&value};
    return
        ((uint16_t)bytes[0]) |
        ((uint16_t)bytes[1] << 8);
}

inline uint32_t toNbo(uint32_t value)
{
    uint8_t *bytes{(uint8_t*)&value};
    return
        ((uint32_t)bytes[0])       |
        ((uint32_t)bytes[1] <<  8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

inline uint64_t toNbo(uint64_t value)
{
    uint8_t *bytes{(uint8_t*)&value};
    return
        ((uint64_t)bytes[0])       |
        ((uint64_t)bytes[1] <<  8) |
        ((uint64_t)bytes[2] << 16) |
        ((uint64_t)bytes[3] << 24) |
        ((uint64_t)bytes[4] << 32) |
        ((uint64_t)bytes[5] << 40) |
        ((uint64_t)bytes[6] << 48) |
        ((uint64_t)bytes[7] << 56);
}

//==============================================================================

//==================== Fixed-size integer literal suffixes =====================

constexpr uint8_t operator "" _u8(unsigned long long value)
{
    return static_cast<uint8_t>(value);
}

constexpr uint16_t operator "" _u16(unsigned long long value)
{
    return static_cast<uint16_t>(value);
}

constexpr uint32_t operator "" _u32(unsigned long long value)
{
    return static_cast<uint32_t>(value);
}

constexpr uint64_t operator "" _u64(unsigned long long value)
{
    return static_cast<uint64_t>(value);
}

//==============================================================================
