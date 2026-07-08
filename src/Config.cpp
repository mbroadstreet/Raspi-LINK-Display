#include "Config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// --- Helper: Parse RGBA string ---
static RgbaColor parseRgba(const std::string& value, const RgbaColor& fallback)
{
    std::istringstream iss(value);
    std::string token;
    RgbaColor c = fallback;
    int components[4] = {fallback.r, fallback.g, fallback.b, fallback.a};
    int i = 0;

    while (std::getline(iss, token, ',') && i < 4)
    {
        try { components[i] = std::stoi(token); }
        catch (...) { return fallback; }
        ++i;
    }

    if (i >= 3)
    {
        c.r = components[0];
        c.g = components[1];
        c.b = components[2];
        c.a = (i == 4) ? components[3] : 255;
    }
    return c;
}

// --- Font discovery ---
std::string findDefaultFontPath()
{
    const std::vector<std::string> candidates = {
        "./assets/fonts/display.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"
    };

    for (const auto& path : candidates)
        if (fs::exists(path)) return path;

    return {};
}

void printUsage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --no-gui                  Print Link state to console once per second.\n"
        << "  --windowed                Run in a window instead of fullscreen.\n"
        << "  --width N                 Window/layout width.\n"
        << "  --height N                Window/layout height.\n"
        << "  --font PATH               TTF font path.\n"
        << "  --tempo BPM               Initial tempo before joining a session.\n"
        << "  --quantum BEATS           Phase quantum.\n"
        << "  --config PATH             Path to config file.\n"
        << "  --help                    Show this help.\n";
}

// --- Load config file ---
static void loadConfigFile(Config& config, const std::string& path, int& errorCount)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Error: Config file not found: " << path << "\n";
        errorCount++;
        return;
    }

    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line))
    {
        ++lineNumber;
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            std::cerr << "Warning: Invalid line in " << path << ":" << lineNumber << "\n";
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        if      (key == "width")                     config.width = std::stoi(value);
        else if (key == "height")                    config.height = std::stoi(value);
        else if (key == "fullscreen")                config.fullscreen = (value == "true" || value == "1" || value == "yes");
        else if (key == "font_path")                 config.fontPath = value;
        else if (key == "status_font_size")          config.statusFontSize = std::stoi(value);
        else if (key == "tempo_font_size")           config.tempoFontSize = std::stoi(value);
        else if (key == "bottom_font_size")          config.bottomFontSize = std::stoi(value);
        else if (key == "help_font_size")            config.helpFontSize = std::stoi(value);
        else if (key == "top_band_color")            config.topBandColor = parseRgba(value, config.topBandColor);
        else if (key == "center_band_color")         config.centerBandColor = parseRgba(value, config.centerBandColor);
        else if (key == "bottom_band_color")         config.bottomBandColor = parseRgba(value, config.bottomBandColor);
        else if (key == "status_inactive_color")     config.statusInactiveColor = parseRgba(value, config.statusInactiveColor);
        else if (key == "status_no_peers_color")     config.statusNoPeersColor = parseRgba(value, config.statusNoPeersColor);
        else if (key == "status_connected_color")    config.statusConnectedColor = parseRgba(value, config.statusConnectedColor);
        else if (key == "tempo_color")               config.tempoColor = parseRgba(value, config.tempoColor);
        else if (key == "phase_bar_color")           config.phaseBarColor = parseRgba(value, config.phaseBarColor);
        else if (key == "phase_marker_color")        config.phaseMarkerColor = parseRgba(value, config.phaseMarkerColor);
        else if (key == "help_overlay_seconds")      config.helpOverlaySeconds = std::stoi(value);
        else if (key == "hide_mouse_cursor")         config.hideMouseCursor = (value == "true" || value == "1" || value == "yes");
        else if (key == "background_color")          config.backgroundColor = parseRgba(value, config.backgroundColor);
        else if (key == "band_color")                config.bandColor = parseRgba(value, config.bandColor);
        else
        {
            std::cerr << "Warning: Unknown config key '" << key << "' in " << path << ":" << lineNumber << "\n";
        }
    }
}

// --- Main config parser with correct precedence ---
Config parseConfig(int argc, char** argv)
{
    Config config;
    config.fontPath = findDefaultFontPath();

    std::string configFilePath;
    bool configFileExplicitlyRequested = false;

    // === Pass 1: Look for --config only ===
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--config" && i + 1 < argc)
        {
            configFilePath = argv[++i];
            configFileExplicitlyRequested = true;
        }
    }

    // === Load config file ===
    if (!configFilePath.empty())
    {
        int errorCount = 0;
        loadConfigFile(config, configFilePath, errorCount);
        config.configPath = configFilePath;

        if (errorCount > 0 && configFileExplicitlyRequested)
        {
            std::cerr << "Error: Specified config file could not be loaded: " << configFilePath << "\n";
            std::exit(1);
        }
    }
    else
    {
        // Try default location silently
        std::string defaultPath = "config/link-pi-display.conf";
        if (fs::exists(defaultPath))
        {
            int errorCount = 0;
            loadConfigFile(config, defaultPath, errorCount);
            config.configPath = defaultPath;
        }
    }

    // === Apply legacy alias fallback logic ===
    if (!config.topBandColor.r && !config.topBandColor.g && !config.topBandColor.b)
        config.topBandColor = config.bandColor;
    if (!config.centerBandColor.r && !config.centerBandColor.g && !config.centerBandColor.b)
        config.centerBandColor = config.backgroundColor;
    if (!config.bottomBandColor.r && !config.bottomBandColor.g && !config.bottomBandColor.b)
        config.bottomBandColor = config.bandColor;

    // === Pass 2: Apply all CLI overrides ===
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); std::exit(0); }
        else if (arg == "--no-gui")               config.noGui = true;
        else if (arg == "--windowed")             config.fullscreen = false;
        else if (arg == "--width"   && i+1<argc)  config.width = std::stoi(argv[++i]);
        else if (arg == "--height"  && i+1<argc)  config.height = std::stoi(argv[++i]);
        else if (arg == "--font"    && i+1<argc)  config.fontPath = argv[++i];
        else if (arg == "--tempo"   && i+1<argc)  config.initialTempo = std::stod(argv[++i]);
        else if (arg == "--quantum" && i+1<argc)  config.quantum = std::stod(argv[++i]);
    }

    return config;
}
