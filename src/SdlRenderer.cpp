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