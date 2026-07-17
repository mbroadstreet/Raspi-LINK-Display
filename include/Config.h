#pragma once

#include <string>
#include <vector>
#include <map>

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

    // Colors - v0.4 explicit band colors
    RgbaColor topBandColor {0, 0, 0, 255};
    RgbaColor centerBandColor {18, 18, 18, 255};
    RgbaColor bottomBandColor {0, 0, 0, 255};

    // Status colors
    RgbaColor statusInactiveColor {40, 44, 48, 255};
    RgbaColor statusNoPeersColor {40, 44, 48, 255};
    RgbaColor statusConnectedColor {75, 85, 95, 255};
    RgbaColor tempoColor {64, 79, 96, 255};

    // Phase meter
    RgbaColor phaseBarColor {83, 114, 151, 255};
    RgbaColor phaseMarkerColor {255, 255, 255, 255};
    int phaseBarHeight = 22;
    int phaseBarSegmentGap = 6;
    int phaseBarMargin = 24;

    // Help overlay
    RgbaColor helpOverlayBackgroundColor {0, 0, 0, 220};
    RgbaColor helpOverlayTextColor {210, 210, 210, 255};
    int helpOverlaySeconds = 8;

    // Base colors snapshot for v0.6 preset restoration (captured after final non-preset resolution)
    RgbaColor baseStatusInactiveColor;
    RgbaColor baseStatusNoPeersColor;
    RgbaColor baseStatusConnectedColor;
    RgbaColor baseTempoColor;
    RgbaColor basePhaseBarColor;
    RgbaColor basePhaseMarkerColor;
    RgbaColor baseTopBandColor;
    RgbaColor baseCenterBandColor;
    RgbaColor baseBottomBandColor;
    RgbaColor baseHelpOverlayBackgroundColor;
    RgbaColor baseHelpOverlayTextColor;

    // Fullscreen behavior
    bool hideMouseCursor = true;

    // Deprecated compatibility aliases (v0.4)
    // These will only be used if the explicit v0.4 keys are not set.
    RgbaColor backgroundColor {18, 18, 18, 255};   // maps to centerBandColor
    RgbaColor bandColor {18, 18, 18, 255};         // maps to topBandColor + bottomBandColor

    // Internal tracking for alias logic (not serialized)
    bool topBandColorExplicit = false;
    bool centerBandColorExplicit = false;
    bool bottomBandColorExplicit = false;
    bool backgroundColorExplicit = false;
    bool bandColorExplicit = false;

    // Runtime
    std::string configPath;
    std::string startupConfigPath;
    std::vector<std::string> originalCliArgs;

    // v0.5 inspection modes (early exit)
    bool printConfig = false;
    bool moduleInfo = false;

    // v0.6 Color Presets (dot-prefixed syntax only)
    std::vector<std::string> colorPresetNames;                    // from color_presets= list (order matters)
    std::string initialColorPreset;                               // from color_preset=
    std::map<std::string, std::map<std::string, RgbaColor>> colorPresetOverrides;
    std::map<std::string, std::string> colorPresetLabels;  // .name metadata only, per ticket

    // Runtime active preset state (mutable for P key cycling)
    int activeColorPresetIndex = -1;  // -1 means no presets / use base
};

Config parseConfig(int argc, char** argv);
std::string findDefaultFontPath();
void printUsage(const char* argv0);
RgbaColor parseRgbaColor(const std::string& value, const RgbaColor& fallback);

// v0.5 inspection helpers
void printEffectiveConfig(const Config& config);
void printModuleInfo();

// v0.6 Color preset helpers
void applyColorPreset(Config& config, const std::string& presetName);
void cycleColorPreset(Config& config);
std::string getActiveColorPresetName(const Config& config);

void captureBaseColors(Config& config);
void restoreBaseColors(Config& config);

// v0.6 Ticket 3: runtime reload
bool tryReloadConfig(Config& config);
