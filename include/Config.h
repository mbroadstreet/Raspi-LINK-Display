#pragma once

#include <string>

struct RgbaColor
{
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;
};

struct Config
{
    // Window / layout
    int width = 480;
    int height = 320;
    bool fullscreen = true;
    bool noGui = false;

    // Link
    double initialTempo = 120.0;
    double quantum = 4.0;

    // Font
    std::string fontPath;
    int statusFontSize = 30;
    int tempoFontSize = 110;
    int bottomFontSize = 25;
    int helpFontSize = 20;

    // Colors
    RgbaColor statusInactiveColor {40, 44, 48, 255};
    RgbaColor statusNoPeersColor {40, 44, 48, 255};
    RgbaColor statusConnectedColor {75, 85, 95, 255};
    RgbaColor tempoColor {64, 79, 96, 255};
    RgbaColor phaseBarColor {83, 114, 151, 255};
    RgbaColor phaseMarkerColor {255, 255, 255, 255};
    RgbaColor backgroundColor {0, 0, 0, 255};
    RgbaColor bandColor {18, 18, 18, 255};
    RgbaColor helpOverlayBackgroundColor {0, 0, 0, 220};
    RgbaColor helpOverlayTextColor {210, 210, 210, 255};

    // Phase meter
    int phaseBarHeight = 22;
    int phaseBarSegmentGap = 6;

    // Help overlay
    int helpOverlaySeconds = 8;

    // Config file path (runtime)
    std::string configPath;
};

Config parseConfig(int argc, char** argv);
std::string findDefaultFontPath();
void printUsage(const char* argv0);
RgbaColor parseRgbaColor(const std::string& value, const RgbaColor& fallback);
