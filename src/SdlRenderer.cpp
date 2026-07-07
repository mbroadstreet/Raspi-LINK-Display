#include "SdlRenderer.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>

SdlRenderer::SdlRenderer(int width, int height, const std::string& fontPath)
    : window(nullptr), renderer(nullptr), font(nullptr), quit(false) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        quit = true;
        return;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed" << std::endl;
        quit = true;
        return;
    }
    Uint32 flags = SDL_WINDOW_SHOWN;
    // Fullscreen means SDL_WINDOW_FULLSCREEN_DESKTOP for 480x320 design
    // (windowed mode uses --windowed flag)
    window = SDL_CreateWindow("Raspi-LINK-Display", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window) {
        std::cerr << "Window creation failed" << std::endl;
        quit = true;
        return;
    }
    renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(window), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer creation failed" << std::endl;
        quit = true;
        return;
    }
    font = TTF_OpenFont(fontPath.c_str(), 24);
    if (!font) {
        std::cerr << "Font load failed: " << TTF_GetError() << std::endl;
        // continue without font for skeleton
    }
}

SdlRenderer::~SdlRenderer() {
    if (font) TTF_CloseFont(static_cast<TTF_Font*>(font));
    if (renderer) SDL_DestroyRenderer(static_cast<SDL_Renderer*>(renderer));
    if (window) SDL_DestroyWindow(static_cast<SDL_Window*>(window));
    TTF_Quit();
    SDL_Quit();
}

void SdlRenderer::render(const std::string& status, const std::string& tempo, const std::string& beatPhase, bool windowed) {
    if (quit || !renderer) return;
    SDL_SetRenderDrawColor(static_cast<SDL_Renderer*>(renderer), 0, 0, 0, 255);
    SDL_RenderClear(static_cast<SDL_Renderer*>(renderer));

    // Simple text rendering (placeholder - full implementation would use TTF_RenderText)
    // For skeleton we just clear; real text would be added here
    SDL_RenderPresent(static_cast<SDL_Renderer*>(renderer));
}

void SdlRenderer::present() {
    if (renderer) SDL_RenderPresent(static_cast<SDL_Renderer*>(renderer));
}

bool SdlRenderer::shouldQuit() const {
    return quit;
}