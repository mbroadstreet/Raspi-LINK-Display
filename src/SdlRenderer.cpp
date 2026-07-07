#include "SdlRenderer.h"
#include "DisplayText.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

void SdlRenderer::render(const LinkDisplayState& state)
{
    int outputW = config_.width;
    int outputH = config_.height;
    SDL_GetRendererOutputSize(renderer_, &outputW, &outputH);

    const SDL_Rect topBand {0, 0, outputW, 58};
    const SDL_Rect centerBand {0, topBand.h, outputW, outputH - 116};
    const SDL_Rect bottomBand {0, outputH - 58, outputW, 58};

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 18, 18, 18, 255);
    SDL_RenderFillRect(renderer_, &topBand);
    SDL_RenderFillRect(renderer_, &bottomBand);

    const SDL_Color white {245, 245, 245, 255};
    const SDL_Color soft {210, 210, 210, 255};
    const SDL_Color blue {80, 140, 255, 255};

    renderCenteredText(DisplayText::formatStatusLine(state), statusFont_, topBand, soft);
    renderCenteredText(DisplayText::formatTempoLine(state), tempoFont_, centerBand, white);

    // v0.3 bottom band: integer beat (left) + 4-segment phase bar (right)
    renderBottomBeatAndPhaseBar(state, bottomBand, soft, blue);

    SDL_RenderPresent(renderer_);
}

void SdlRenderer::renderBottomBeatAndPhaseBar(const LinkDisplayState& state,
                                              const SDL_Rect& area,
                                              SDL_Color textColor,
                                              SDL_Color barColor)
{
    // Left side: integer beat
    std::string beatText = "Beat " + std::to_string(static_cast<int>(state.beat));
    renderCenteredText(beatText, bottomFont_, {area.x, area.y, area.w / 2, area.h}, textColor);

    // Right side: 4-segment phase bar
    const int barHeight = 18;
    const int barY = area.y + (area.h - barHeight) / 2;
    const int barWidth = area.w / 2 - 30;
    const int segmentWidth = barWidth / 4;
    const int barX = area.x + area.w / 2 + 15;

    // Draw 4 segments
    SDL_SetRenderDrawColor(renderer_, barColor.r, barColor.g, barColor.b, barColor.a);
    for (int i = 0; i < 4; ++i)
    {
        SDL_Rect seg {barX + i * segmentWidth + 2, barY, segmentWidth - 4, barHeight};
        SDL_RenderFillRect(renderer_, &seg);
    }

    // Phase position marker (white vertical line)
    const double phaseNorm = state.phase / state.quantum; // 0.0 - 1.0
    const int markerX = barX + static_cast<int>(phaseNorm * barWidth);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer_, markerX, barY - 4, markerX, barY + barHeight + 4);
}