#pragma once

#include "Config.h"
#include "LinkDisplayState.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <string>

class SdlRenderer
{
public:
    explicit SdlRenderer(const Config& config);
    ~SdlRenderer();

    SdlRenderer(const SdlRenderer&) = delete;
    SdlRenderer& operator=(const SdlRenderer&) = delete;

    bool pollQuit();
    void render(const LinkDisplayState& state);

private:
    void renderCenteredText(const std::string& text, TTF_Font* font, SDL_Rect area, SDL_Color color);

    Config config_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* statusFont_ = nullptr;
    TTF_Font* tempoFont_ = nullptr;
    TTF_Font* bottomFont_ = nullptr;
};
