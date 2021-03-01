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
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <memory>
#include <cstring>

#define INITIAL_WINDOW_WIDTH  10
#define INITIAL_WINDOW_HEIGHT 10

int main(int argc, char** argv)
{
    if (argc < 2)
        return 1;

    const bool isTestingMode{argc > 2 && std::strcmp(argv[1], "--test")};

    auto image{std::make_unique<BmpImage>()};
    int openStatus{image->open(argv[1])};
    if (openStatus)
    {
        std::cerr << "Failed to open image, exiting" << '\n';
        return openStatus;
    }

    SDL_Init(SDL_INIT_VIDEO);

    auto window{SDL_CreateWindow(
            "LIMG",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            image->getWidthPx(), image->getHeightPx(),
            SDL_WINDOW_RESIZABLE
    )};
    auto renderer{SDL_CreateRenderer(window, -1, 0)};
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_SetWindowMinimumSize(window, 10, 10);
    SDL_SetWindowMaximumSize(window, 2000, 2000);

    if (isTestingMode)
    {
        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        int renderStatus{image->render(renderer, windowWidth, windowHeight)};
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return renderStatus;
    }

    bool isRunning{true};
    bool isRedrawNeeded{};
    bool isFullscreen{};

    while (isRunning)
    {
        SDL_Event event;
        while (isRunning && SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                isRunning = false;
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    isRunning = false;
                    break;

                case SDLK_f:
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    break;
                }
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_SHOWN:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_MINIMIZED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_EXPOSED:
                case SDL_WINDOWEVENT_RESTORED:
                    isRedrawNeeded = true;
                    break;
                }
                break;
            }
        }
        if (!isRunning)
            break;
        
        if (isRedrawNeeded)
        {
            int windowWidth, windowHeight;
            SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            int renderStatus{image->render(renderer, windowWidth, windowHeight)};
            if (renderStatus)
            {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return renderStatus;
            }
            isRedrawNeeded = false;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "End" << '\n';

    return 0;
}

