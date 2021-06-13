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
#include "PnmImage.h"
#include "GifImage.h"
#include "SvgImage.h"
#include "Logger.h"
#include "misc.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <memory>
#include <cstring>
#include <algorithm>

#define INITIAL_WINDOW_WIDTH  10
#define INITIAL_WINDOW_HEIGHT 10
#define MAX_WINDOW_WIDTH  1900
#define MAX_WINDOW_HEIGHT 1000
#define ZOOM_STEP_PERC 5
#define MOVE_STEP_PX 10

int main(int argc, char** argv)
{
    const bool isTestingMode{argc > 2 && std::strcmp(argv[1], "--test")};

    std::string filePath{};
    if (argc < 2) // If no file given
    {
        // Open the logo image

        std::string parentDir = getExeParentDir();
        if (parentDir.length())
            filePath = parentDir + "/../img/icon.bmp";
        else
            return 1;
    }
    else // If there is file given
    {
        // Open it
        filePath = argv[1];
    }
    std::string fileExtension{filePath.substr(filePath.find_last_of('.')+1)};
    std::transform(
            fileExtension.begin(), fileExtension.end(),
            fileExtension.begin(),
            [](char c){ return std::tolower(c); });

    std::unique_ptr<Image> image{};
    if (fileExtension.compare("bmp") == 0)
    {
        image = std::make_unique<BmpImage>();
    }
    else if (fileExtension.compare("pnm") == 0 ||
             fileExtension.compare("pbm") == 0 ||
             fileExtension.compare("pgm") == 0 ||
             fileExtension.compare("ppm") == 0)
    {
      image = std::make_unique<PnmImage>();
    }
    else if (fileExtension.compare("gif") == 0)
    {
        image = std::make_unique<GifImage>();
    }
    else if (fileExtension.compare("svg") == 0)
    {
        image = std::make_unique<SvgImage>();
    }
    else
    {
        Logger::err << "Unknown file extension: " << fileExtension << Logger::End;
        return 1;
    }

    int openStatus{image->open(filePath)};
    if (openStatus)
    {
        Logger::err << "Failed to open image, exiting" << Logger::End;
        return openStatus;
    }

    SDL_Init(SDL_INIT_VIDEO);

    auto window{SDL_CreateWindow(
            "LIMG",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            std::min(image->getWidthPx(), (uint32_t)MAX_WINDOW_WIDTH), std::min(image->getHeightPx(), (uint32_t)MAX_WINDOW_HEIGHT),
            SDL_WINDOW_RESIZABLE
    )};
    if (!window)
    {
        Logger::err << "Failed to create window: " << SDL_GetError() << Logger::End;
        return 1;
    }

    auto renderer{SDL_CreateRenderer(window, -1, 0)};
    if (!renderer)
    {
        Logger::err << "Failed to create renderer: " << SDL_GetError() << Logger::End;
        return 1;
    }

    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_SetWindowMinimumSize(window, 10, 10);
    SDL_SetWindowMaximumSize(window, MAX_WINDOW_WIDTH, MAX_WINDOW_HEIGHT);

    SDL_Texture* texture{SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            image->getWidthPx(), image->getHeightPx()
    )};
    if (!texture)
    {
        Logger::err << "Failed to create texture: " << SDL_GetError() << Logger::End;
        return 1;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    if (isTestingMode)
    {
        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        int renderStatus{image->render(texture, windowWidth, windowHeight)};
        if (renderStatus)
        {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return renderStatus;
        }
        SDL_Rect srcRect{0, 0, windowWidth, windowHeight};
        SDL_RenderCopy(renderer, texture, &srcRect, nullptr);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return renderStatus;
    }

    bool isRunning{true};
    bool isRedrawNeeded{};
    bool isFullscreen{};
    bool useTransparency{true};
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    // Set the initial zoom so that the image fits in the window
    float zoom = std::min(1.0f, std::min((float)MAX_WINDOW_WIDTH / image->getWidthPx(), (float)MAX_WINDOW_HEIGHT / image->getHeightPx()));
    int viewportX{};
    int viewportY{};

    auto updateWindowTitle{[&window, &image, &zoom](){
        SDL_SetWindowTitle(window,
                ("LIMG - " + image->getFilepath() +
                 " (" + std::to_string(image->getWidthPx()) + 'x' + std::to_string(image->getHeightPx()) + ") [" +
                 std::to_string((int)std::round(zoom * 100 / ZOOM_STEP_PERC) * ZOOM_STEP_PERC) + "%]").c_str());
    }};
    updateWindowTitle();

    // Render the whole image
    int renderStatus{image->render(texture, image->getWidthPx(), image->getHeightPx())};
    if (renderStatus)
    {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return renderStatus;
    }

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
                case SDLK_ESCAPE: // Quit
                case SDLK_q:
                    isRunning = false;
                    break;

                case SDLK_f: // Toggle fullscreen
                    isFullscreen = !isFullscreen;
                    SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    // Let's not lose focus
                    SDL_RaiseWindow(window);
                    break;

                case SDLK_t: // Toggle transparency
                    useTransparency = !useTransparency;
                    SDL_SetTextureBlendMode(texture, useTransparency ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
                    isRedrawNeeded = true;
                    break;
                }
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_KP_PLUS: // Zoom in
                    zoom += 0.05;
                    if (zoom > 1000)
                        zoom = 1000;
                    updateWindowTitle();
                    isRedrawNeeded = true;
                    break;

                case SDLK_KP_MINUS: // Zoom out
                    zoom -= 0.05;
                    if (zoom < ZOOM_STEP_PERC / 100.0)
                        zoom = ZOOM_STEP_PERC / 100.0;
                    updateWindowTitle();
                    isRedrawNeeded = true;
                    break;

                case SDLK_h: // Go left
                    viewportX -= MOVE_STEP_PX;
                    isRedrawNeeded = true;
                    break;

                case SDLK_j: // Go down
                    viewportY += MOVE_STEP_PX;
                    isRedrawNeeded = true;
                    break;

                case SDLK_k: // Go up
                    viewportY -= MOVE_STEP_PX;
                    isRedrawNeeded = true;
                    break;

                case SDLK_l: // Go right
                    viewportX += MOVE_STEP_PX;
                    isRedrawNeeded = true;
                    break;
                }
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    windowWidth = event.window.data1;
                    windowHeight = event.window.data2;
                    //Logger::log << std::dec << "Window size changed to " << windowWidth << 'x' << windowHeight << Logger::End;
                    [[fallthrough]];
                case SDL_WINDOWEVENT_SHOWN:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_MINIMIZED:
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
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // XXX: Make the zoom center the window center, not the image center
            const int dstRectWidth{int(image->getWidthPx() * zoom)};
            const int dstRectHeight{int(image->getHeightPx() * zoom)};
            const SDL_Rect dstRect{
                windowWidth / 2 - dstRectWidth / 2 - viewportX,
                windowHeight / 2 - dstRectHeight / 2 - viewportY,
                dstRectWidth,
                dstRectHeight};
            if (SDL_RenderCopy(renderer, texture, nullptr, &dstRect))
                Logger::err << "Failed to copy texture: " << SDL_GetError() << Logger::End;
            isRedrawNeeded = false;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    Logger::log << "End" << Logger::End;

    return 0;
}

