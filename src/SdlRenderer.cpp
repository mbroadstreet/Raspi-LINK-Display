#include "SdlRenderer.h"

#include "DisplayText.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void require(bool ok, const std::string& message)
    {
        if (!ok)
        {
            throw std::runtime_error(message);
        }
    }
}

SdlRenderer::SdlRenderer(const Config& config)
    : config_(config)
{
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0,
            std::string("SDL_Init failed: ") + SDL_GetError());

    require(TTF_Init() == 0,
            std::string("TTF_Init failed: ") + TTF_GetError());

    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (config_.fullscreen)
    {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow(
        "Ableton Link Pi Display",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config_.width,
        config_.height,
        windowFlags
    );

    require(window_ != nullptr, std::string("SDL_CreateWindow failed: ") + SDL_GetError());

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer_)
    {
        std::cerr << "Accelerated renderer unavailable; falling back to software renderer: "
                  << SDL_GetError() << "\n";
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }

    require(renderer_ != nullptr, std::string("SDL_CreateRenderer failed: ") + SDL_GetError());

    if (config_.fontPath.empty())
    {
        throw std::runtime_error(
            "No usable TTF font found. Use --font PATH or install DejaVu/Liberation fonts."
        );
    }

    statusFont_ = TTF_OpenFont(config_.fontPath.c_str(), 30);   // v0.3.5: adjusted
    tempoFont_ = TTF_OpenFont(config_.fontPath.c_str(), 110);
    bottomFont_ = TTF_OpenFont(config_.fontPath.c_str(), 25);  // v0.3.5: adjusted

    require(statusFont_ != nullptr, std::string("TTF_OpenFont status failed: ") + TTF_GetError());
    require(tempoFont_ != nullptr, std::string("TTF_OpenFont tempo failed: ") + TTF_GetError());
    require(bottomFont_ != nullptr, std::string("TTF_OpenFont bottom failed: ") + TTF_GetError());
}

SdlRenderer::~SdlRenderer()
{
    if (bottomFont_) TTF_CloseFont(bottomFont_);
    if (tempoFont_) TTF_CloseFont(tempoFont_);
    if (statusFont_) TTF_CloseFont(statusFont_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);

    TTF_Quit();
    SDL_Quit();
}

bool SdlRenderer::pollQuit()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            return true;
        }

        if (event.type == SDL_KEYDOWN)
        {
            const SDL_Keycode key = event.key.keysym.sym;
            if (key == SDLK_ESCAPE || key == SDLK_q)
            {
                return true;
            }
        }
    }

    return false;
}

void SdlRenderer::renderCenteredText(const std::string& text, TTF_Font* font, SDL_Rect area, SDL_Color color)
{
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface)
    {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << "\n";
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    if (!texture)
    {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << "\n";
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst {0, 0, surface->w, surface->h};

    if (dst.w > area.w)
    {
        const double scale = static_cast<double>(area.w) / static_cast<double>(dst.w);
        dst.w = area.w;
        dst.h = static_cast<int>(dst.h * scale);
    }

    if (dst.h > area.h)
    {
        const double scale = static_cast<double>(area.h) / static_cast<double>(dst.h);
        dst.h = area.h;
        dst.w = static_cast<int>(dst.w * scale);
    }

    dst.x = area.x + (area.w - dst.w) / 2;
    dst.y = area.y + (area.h - dst.h) / 2;

    SDL_RenderCopy(renderer_, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void SdlRenderer::render(const LinkDisplayState& state)
{
    int outputW = config_.width;
    int outputH = config_.height;
    SDL_GetRendererOutputSize(renderer_, &outputW, &outputH);

    // v0.3.4: slightly lower top line + modest extra padding
    const int topPadding = 18;
    const int bottomPadding = 14;
    const int topBandHeight = 46;
    const int bottomBandHeight = 50;

    const SDL_Rect topBand {0, topPadding, outputW, topBandHeight};
    const SDL_Rect centerBand {0, topBand.y + topBand.h + 8, outputW, outputH - topBandHeight - bottomBandHeight - topPadding - bottomPadding - 16};
    const SDL_Rect bottomBand {0, outputH - bottomBandHeight - bottomPadding, outputW, bottomBandHeight};

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 18, 18, 18, 255);
    SDL_RenderFillRect(renderer_, &topBand);
    SDL_RenderFillRect(renderer_, &bottomBand);

    const SDL_Color statusNoPeers {40, 44, 48, 255};
    const SDL_Color statusConnected {75, 85, 95, 255};
    const SDL_Color tempoColor {64, 79, 96, 255};
    const SDL_Color phaseBarColor {83, 114, 151, 255};
    const SDL_Color phaseTextColor {75, 85, 95, 255};

    const SDL_Color statusColor = (state.linkEnabled && state.remotePeers > 0)
        ? statusConnected
        : statusNoPeers;

    renderCenteredText(DisplayText::formatStatusLine(state), statusFont_, topBand, statusColor);
    renderCenteredText(DisplayText::formatTempoLine(state), tempoFont_, centerBand, tempoColor);
    renderBottomBeatAndPhaseBar(state, bottomBand, phaseTextColor, phaseBarColor);

    SDL_RenderPresent(renderer_);
}

void SdlRenderer::renderBottomBeatAndPhaseBar(const LinkDisplayState& state,
                                              const SDL_Rect& area,
                                              SDL_Color textColor,
                                              SDL_Color barColor)
{
    // v0.3.4: slightly narrower phase bar than v0.3.3
    const int barHeight = 22;
    const int barY = area.y + (area.h - barHeight) / 2;
    const int margin = 24;   // increased for narrower bar
    const int barWidth = area.w - (margin * 2);
    const int barX = area.x + margin;
    const int segmentGap = 6;
    const int segmentWidth = std::max(5, (barWidth - (segmentGap * 3)) / 4);
    const int actualBarWidth = (segmentWidth * 4) + (segmentGap * 3);

    SDL_SetRenderDrawColor(renderer_, barColor.r, barColor.g, barColor.b, barColor.a);
    for (int i = 0; i < 4; ++i)
    {
        SDL_Rect segment {
            barX + i * (segmentWidth + segmentGap),
            barY,
            segmentWidth,
            barHeight
        };
        SDL_RenderFillRect(renderer_, &segment);
    }

    double phaseNorm = 0.0;
    if (state.quantum > 0.0)
    {
        phaseNorm = state.phase / state.quantum;
    }
    phaseNorm = std::clamp(phaseNorm, 0.0, 1.0);

    const int markerTravel = std::max(0, actualBarWidth - 1);
    const int markerOffset = static_cast<int>(std::lround(phaseNorm * markerTravel));
    const int markerX = std::clamp(barX + markerOffset, barX, barX + markerTravel);

    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer_, markerX, barY - 4, markerX, barY + barHeight + 4);
}
