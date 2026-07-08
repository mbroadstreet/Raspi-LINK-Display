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

    SDL_Color toSDL(const RgbaColor& c)
    {
        return {static_cast<Uint8>(c.r), static_cast<Uint8>(c.g), static_cast<Uint8>(c.b), static_cast<Uint8>(c.a)};
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

    statusFont_ = TTF_OpenFont(config_.fontPath.c_str(), config_.statusFontSize);
    tempoFont_ = TTF_OpenFont(config_.fontPath.c_str(), config_.tempoFontSize);
    bottomFont_ = TTF_OpenFont(config_.fontPath.c_str(), config_.bottomFontSize);
    helpFont_ = TTF_OpenFont(config_.fontPath.c_str(), config_.helpFontSize);

    require(statusFont_ != nullptr, std::string("TTF_OpenFont status failed: ") + TTF_GetError());
    require(tempoFont_ != nullptr, std::string("TTF_OpenFont tempo failed: ") + TTF_GetError());
    require(bottomFont_ != nullptr, std::string("TTF_OpenFont bottom failed: ") + TTF_GetError());
    require(helpFont_ != nullptr, std::string("TTF_OpenFont help failed: ") + TTF_GetError());

    // Hide mouse cursor in fullscreen if requested
    if (config_.fullscreen && config_.hideMouseCursor)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }
}

SdlRenderer::~SdlRenderer()
{
    // Restore cursor on exit if it was hidden
    SDL_ShowCursor(SDL_ENABLE);

    if (helpFont_) TTF_CloseFont(helpFont_);
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

            if (key == SDLK_F1)
            {
                showHelpOverlay();
            }

            if (key == SDLK_f)
            {
                Uint32 flags = SDL_GetWindowFlags(window_);
                bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

                if (isFullscreen)
                {
                    SDL_SetWindowFullscreen(window_, 0);
                    if (config_.hideMouseCursor)
                        SDL_ShowCursor(SDL_ENABLE);
                }
                else
                {
                    SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    if (config_.hideMouseCursor)
                        SDL_ShowCursor(SDL_DISABLE);
                }
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
    int outputW = 0;
    int outputH = 0;
    SDL_GetRendererOutputSize(renderer_, &outputW, &outputH);

    const int topPadding = 18;
    const int bottomPadding = 14;
    const int topBandHeight = 46;
    const int bottomBandHeight = 50;

    const SDL_Rect topBand {0, topPadding, outputW, topBandHeight};
    const SDL_Rect centerBand {0, topBand.y + topBand.h + 8, outputW, outputH - topBandHeight - bottomBandHeight - topPadding - bottomPadding - 16};
    const SDL_Rect bottomBand {0, outputH - bottomBandHeight - bottomPadding, outputW, bottomBandHeight};

    // Background
    SDL_SetRenderDrawColor(renderer_,
        config_.backgroundColor.r,
        config_.backgroundColor.g,
        config_.backgroundColor.b,
        config_.backgroundColor.a);
    SDL_RenderClear(renderer_);

    // Bands
    SDL_SetRenderDrawColor(renderer_, toSDL(config_.topBandColor).r, toSDL(config_.topBandColor).g, toSDL(config_.topBandColor).b, toSDL(config_.topBandColor).a);
    SDL_RenderFillRect(renderer_, &topBand);

    SDL_SetRenderDrawColor(renderer_, toSDL(config_.bottomBandColor).r, toSDL(config_.bottomBandColor).g, toSDL(config_.bottomBandColor).b, toSDL(config_.bottomBandColor).a);
    SDL_RenderFillRect(renderer_, &bottomBand);

    // Status color selection
    SDL_Color statusColor;
    std::string statusText = DisplayText::formatStatusLine(state);

    if (statusText == "LINK Inactive")
        statusColor = toSDL(config_.statusInactiveColor);
    else if (statusText == "LINK Active · No Peers")
        statusColor = toSDL(config_.statusNoPeersColor);
    else
        statusColor = toSDL(config_.statusConnectedColor);

    renderCenteredText(statusText, statusFont_, topBand, statusColor);
    renderCenteredText(DisplayText::formatTempoLine(state), tempoFont_, centerBand, toSDL(config_.tempoColor));
    renderBottomPhaseBar(state, bottomBand, toSDL(config_.phaseBarColor), toSDL(config_.phaseMarkerColor));

    if (helpOverlayVisible_)
    {
        renderHelpOverlay();
    }

    SDL_RenderPresent(renderer_);
}

void SdlRenderer::renderBottomPhaseBar(const LinkDisplayState& state,
                                       const SDL_Rect& area,
                                       SDL_Color barColor,
                                       SDL_Color markerColor)
{
    const int barHeight = config_.phaseBarHeight;
    const int barY = area.y + (area.h - barHeight) / 2;
    const int margin = 24;
    const int barWidth = area.w - (margin * 2);
    const int barX = area.x + margin;
    const int segmentGap = config_.phaseBarSegmentGap;
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

    SDL_SetRenderDrawColor(renderer_, markerColor.r, markerColor.g, markerColor.b, markerColor.a);
    SDL_RenderDrawLine(renderer_, markerX, barY - 4, markerX, barY + barHeight + 4);
}

void SdlRenderer::showHelpOverlay()
{
    helpOverlayVisible_ = true;
    helpOverlayStartTime_ = SDL_GetTicks();
}

bool SdlRenderer::isHelpOverlayVisible() const
{
    return helpOverlayVisible_;
}

void SdlRenderer::renderHelpOverlay()
{
    Uint32 now = SDL_GetTicks();
    if (now - helpOverlayStartTime_ > static_cast<Uint32>(config_.helpOverlaySeconds * 1000))
    {
        helpOverlayVisible_ = false;
        return;
    }

    int outputW = 0, outputH = 0;
    SDL_GetRendererOutputSize(renderer_, &outputW, &outputH);

    const int w = 280;
    const int h = 70;
    const int x = (outputW - w) / 2;
    const int y = 20;

    SDL_Rect bg {x, y, w, h};

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_,
        config_.helpOverlayBackgroundColor.r,
        config_.helpOverlayBackgroundColor.g,
        config_.helpOverlayBackgroundColor.b,
        config_.helpOverlayBackgroundColor.a);
    SDL_RenderFillRect(renderer_, &bg);

    SDL_Color textColor = toSDL(config_.helpOverlayTextColor);

    renderCenteredText("F1 Help", helpFont_, {x, y + 5, w, 22}, textColor);
    renderCenteredText("Q / Esc  Quit", helpFont_, {x, y + 28, w, 20}, textColor);
    renderCenteredText("F        Toggle Fullscreen", helpFont_, {x, y + 48, w, 20}, textColor);
}
