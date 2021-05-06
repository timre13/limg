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

#include <string>

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(__NT__)
#define IS_WIN_OS 1
#endif

std::string getExeParentDir();

#ifdef IS_WIN_OS

// TODO
#error Windows support is not implemented

#else // Probably *NIX

#include <limits.h>
#include <unistd.h>

inline std::string getExeParentDir()
{
    uint16_t maxPathLength{};
#ifdef PATH_MAX
    maxPathLength = PATH_MAX;
#else
    maxPathLength = (uint16_t)-1;
#endif

    char* buffer = new char[maxPathLength];
    if (readlink("/proc/self/exe", buffer, maxPathLength) == -1)
        return "";

    buffer[maxPathLength - 1] = 0;
    std::string exePath{buffer};

    return exePath.substr(0, exePath.find_last_of("/"));
}

#endif

