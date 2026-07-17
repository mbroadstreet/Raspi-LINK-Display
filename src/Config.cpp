#include "Config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <cctype>
#include <iomanip>

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

// --- Strict whole-string numeric conversion helpers ---
static int strictStoi(const std::string& s, const std::string& key, const std::string& path, int lineNumber) {
    std::string t = trim(s);
    if (t.empty()) {
        std::cerr << "Error: " << key << " must be a valid integer: " << s << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    size_t pos = 0;
    int v;
    try {
        v = std::stoi(t, &pos);
        if (pos != t.size()) {
            std::cerr << "Error: " << key << " must be a valid integer (no trailing characters): " << s << " in " << path << ":" << lineNumber << "\n";
            std::exit(1);
        }
    } catch (...) {
        std::cerr << "Error: " << key << " must be a valid integer: " << s << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    return v;
}

static double strictStod(const std::string& s, const std::string& key, const std::string& path, int lineNumber) {
    std::string t = trim(s);
    if (t.empty()) {
        std::cerr << "Error: " << key << " must be a valid number: " << s << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    size_t pos = 0;
    double v;
    try {
        v = std::stod(t, &pos);
        if (pos != t.size()) {
            std::cerr << "Error: " << key << " must be a valid number (no trailing characters): " << s << " in " << path << ":" << lineNumber << "\n";
            std::exit(1);
        }
    } catch (...) {
        std::cerr << "Error: " << key << " must be a valid number: " << s << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    return v;
}

// --- Strict RGBA parser with validation and error reporting ---
static RgbaColor parseRgba(const std::string& value, const std::string& key, const std::string& path, int lineNumber)
{
    std::istringstream iss(value);
    std::string token;
    std::vector<int> components;

    while (std::getline(iss, token, ','))
    {
        std::string t = trim(token);
        if (t.empty()) continue;

        try
        {
            size_t pos = 0;
            int v = std::stoi(t, &pos);
            if (pos != t.size()) {
                std::cerr << "Error: Invalid color component in " << key << " (must be valid integer, no trailing chars): " << value
                          << " in " << path << ":" << lineNumber << "\n";
                std::exit(1);
            }
            if (v < 0 || v > 255)
            {
                std::cerr << "Error: Invalid color component in " << key << " (must be 0-255): " << value
                          << " in " << path << ":" << lineNumber << "\n";
                std::exit(1);
            }
            components.push_back(v);
        }
        catch (...)
        {
            std::cerr << "Error: Invalid color value for " << key << ": " << value
                      << " in " << path << ":" << lineNumber << "\n";
            std::exit(1);
        }
    }

    if (components.size() != 3 && components.size() != 4)
    {
        std::cerr << "Error: " << key << " must have 3 or 4 components (r,g,b[,a]): " << value
                  << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }

    RgbaColor c;
    c.r = components[0];
    c.g = components[1];
    c.b = components[2];
    c.a = (components.size() == 4) ? components[3] : 255;
    return c;
}

// --- Boolean parser (strict) ---
static bool parseBool(const std::string& value, const std::string& key, const std::string& path, int lineNumber)
{
    std::string v = trim(value);

    if (v == "true" || v == "yes" || v == "1" || v == "on") return true;
    if (v == "false" || v == "no" || v == "0" || v == "off") return false;

    std::cerr << "Error: Invalid boolean value for " << key << ": " << value
              << " (expected true/false/yes/no/1/0/on/off) in " << path << ":" << lineNumber << "\n";
    std::exit(1);
    return false;
}

// --- Numeric helpers for config file (with file/line in errors) ---
static int parsePositiveInt(const std::string& value, const std::string& key, const std::string& path, int lineNumber)
{
    int v = strictStoi(value, key, path, lineNumber);
    if (v <= 0) {
        std::cerr << "Error: " << key << " must be > 0: " << value << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    return v;
}

static int parseNonNegativeInt(const std::string& value, const std::string& key, const std::string& path, int lineNumber)
{
    int v = strictStoi(value, key, path, lineNumber);
    if (v < 0) {
        std::cerr << "Error: " << key << " must be >= 0: " << value << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    return v;
}

static int parseHelpOverlaySeconds(const std::string& value, const std::string& key, const std::string& path, int lineNumber)
{
    int v = strictStoi(value, key, path, lineNumber);
    if (v < 1 || v > 60) {
        std::cerr << "Error: " << key << " must be between 1 and 60: " << value << " in " << path << ":" << lineNumber << "\n";
        std::exit(1);
    }
    return v;
}

// --- Numeric helpers for CLI (no file/line) ---
static int parsePositiveIntCli(const std::string& value, const std::string& option)
{
    std::string t = trim(value);
    if (t.empty()) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    size_t pos = 0;
    int v;
    try {
        v = std::stoi(t, &pos);
        if (pos != t.size()) {
            std::cerr << "Error: " << option << " requires a positive numeric value\n";
            std::exit(1);
        }
    } catch (...) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    if (v <= 0) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    return v;
}

static double parsePositiveDoubleCli(const std::string& value, const std::string& option)
{
    std::string t = trim(value);
    if (t.empty()) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    size_t pos = 0;
    double v;
    try {
        v = std::stod(t, &pos);
        if (pos != t.size()) {
            std::cerr << "Error: " << option << " requires a positive numeric value\n";
            std::exit(1);
        }
    } catch (...) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    if (v <= 0.0) {
        std::cerr << "Error: " << option << " requires a positive numeric value\n";
        std::exit(1);
    }
    return v;
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
        << "  --print-config            Print effective configuration and exit.\n"
        << "  --module-info             Print module metadata and exit.\n"
        << "  --help                    Show this help.\n";
}

// --- Load config file with strict validation ---

// --- v0.6 Color Preset helper implementations (narrow, per ticket) ---

static bool isValidPresetId(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) return false;
    }
    return true;
}

static std::string colorKeyToField(const std::string& subkey) {
    if (subkey == "status_inactive_color" ||
        subkey == "status_no_peers_color" ||
        subkey == "status_connected_color" ||
        subkey == "tempo_color" ||
        subkey == "phase_bar_color" ||
        subkey == "phase_marker_color" ||
        subkey == "top_band_color" ||
        subkey == "center_band_color" ||
        subkey == "bottom_band_color" ||
        subkey == "help_overlay_background_color" ||
        subkey == "help_overlay_text_color") {
        return subkey;
    }
    return "";
}

static void registerPresetColor(Config& config, const std::string& presetId, const std::string& subkey, const RgbaColor& col, const std::string& /*path*/, int /*lineNumber*/) {
    config.colorPresetOverrides[presetId][subkey] = col;
}

void applyColorPreset(Config& config, const std::string& presetName) {
    restoreBaseColors(config);
    auto it = config.colorPresetOverrides.find(presetName);
    if (it == config.colorPresetOverrides.end()) return;
    const auto& overrides = it->second;
    for (const auto& kv : overrides) {
        const std::string& key = kv.first;
        const RgbaColor& col = kv.second;
        if (key == "status_inactive_color") config.statusInactiveColor = col;
        else if (key == "status_no_peers_color") config.statusNoPeersColor = col;
        else if (key == "status_connected_color") config.statusConnectedColor = col;
        else if (key == "tempo_color") config.tempoColor = col;
        else if (key == "phase_bar_color") config.phaseBarColor = col;
        else if (key == "phase_marker_color") config.phaseMarkerColor = col;
        else if (key == "top_band_color") config.topBandColor = col;
        else if (key == "center_band_color") config.centerBandColor = col;
        else if (key == "bottom_band_color") config.bottomBandColor = col;
        else if (key == "help_overlay_background_color") config.helpOverlayBackgroundColor = col;
        else if (key == "help_overlay_text_color") config.helpOverlayTextColor = col;
    }
}

void cycleColorPreset(Config& config) {
    if (config.colorPresetNames.size() < 2) return;
    config.activeColorPresetIndex = (config.activeColorPresetIndex + 1) % static_cast<int>(config.colorPresetNames.size());
    std::string next = config.colorPresetNames[config.activeColorPresetIndex];
    applyColorPreset(config, next);
}

std::string getActiveColorPresetName(const Config& config) {
    if (config.activeColorPresetIndex >= 0 && config.activeColorPresetIndex < static_cast<int>(config.colorPresetNames.size())) {
        return config.colorPresetNames[config.activeColorPresetIndex];
    }
    return "";
}

void captureBaseColors(Config& config) {
    config.baseStatusInactiveColor = config.statusInactiveColor;
    config.baseStatusNoPeersColor = config.statusNoPeersColor;
    config.baseStatusConnectedColor = config.statusConnectedColor;
    config.baseTempoColor = config.tempoColor;
    config.basePhaseBarColor = config.phaseBarColor;
    config.basePhaseMarkerColor = config.phaseMarkerColor;
    config.baseTopBandColor = config.topBandColor;
    config.baseCenterBandColor = config.centerBandColor;
    config.baseBottomBandColor = config.bottomBandColor;
    config.baseHelpOverlayBackgroundColor = config.helpOverlayBackgroundColor;
    config.baseHelpOverlayTextColor = config.helpOverlayTextColor;
}

void restoreBaseColors(Config& config) {
    config.statusInactiveColor = config.baseStatusInactiveColor;
    config.statusNoPeersColor = config.baseStatusNoPeersColor;
    config.statusConnectedColor = config.baseStatusConnectedColor;
    config.tempoColor = config.baseTempoColor;
    config.phaseBarColor = config.basePhaseBarColor;
    config.phaseMarkerColor = config.basePhaseMarkerColor;
    config.topBandColor = config.baseTopBandColor;
    config.centerBandColor = config.baseCenterBandColor;
    config.bottomBandColor = config.baseBottomBandColor;
    config.helpOverlayBackgroundColor = config.baseHelpOverlayBackgroundColor;
    config.helpOverlayTextColor = config.baseHelpOverlayTextColor;
}

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

        try
        {
            if      (key == "width")                     config.width = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "height")                    config.height = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "fullscreen")                config.fullscreen = parseBool(value, key, path, lineNumber);
            else if (key == "font_path")                 config.fontPath = value;
            else if (key == "status_font_size")          config.statusFontSize = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "tempo_font_size")           config.tempoFontSize = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "bottom_font_size")          config.bottomFontSize = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "help_font_size")            config.helpFontSize = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "top_band_color")
            {
                config.topBandColor = parseRgba(value, key, path, lineNumber);
                config.topBandColorExplicit = true;
            }
            else if (key == "center_band_color")
            {
                config.centerBandColor = parseRgba(value, key, path, lineNumber);
                config.centerBandColorExplicit = true;
            }
            else if (key == "bottom_band_color")
            {
                config.bottomBandColor = parseRgba(value, key, path, lineNumber);
                config.bottomBandColorExplicit = true;
            }
            else if (key == "status_inactive_color")     config.statusInactiveColor = parseRgba(value, key, path, lineNumber);
            else if (key == "status_no_peers_color")     config.statusNoPeersColor = parseRgba(value, key, path, lineNumber);
            else if (key == "status_connected_color")    config.statusConnectedColor = parseRgba(value, key, path, lineNumber);
            else if (key == "tempo_color")               config.tempoColor = parseRgba(value, key, path, lineNumber);
            else if (key == "phase_bar_color")           config.phaseBarColor = parseRgba(value, key, path, lineNumber);
            else if (key == "phase_marker_color")        config.phaseMarkerColor = parseRgba(value, key, path, lineNumber);
            else if (key == "phase_bar_height")          config.phaseBarHeight = parsePositiveInt(value, key, path, lineNumber);
            else if (key == "phase_bar_segment_gap")     config.phaseBarSegmentGap = parseNonNegativeInt(value, key, path, lineNumber);
            else if (key == "phase_bar_margin")
            {
                int m = parseNonNegativeInt(value, key, path, lineNumber);
                // Practical usable width check
                if (config.width > 0 && (m * 2 >= config.width))
                {
                    std::cerr << "Error: phase_bar_margin too large for width (leaves no usable bar): " << value
                              << " in " << path << ":" << lineNumber << "\n";
                    std::exit(1);
                }
                config.phaseBarMargin = m;
            }
            else if (key == "help_overlay_background_color") config.helpOverlayBackgroundColor = parseRgba(value, key, path, lineNumber);
            else if (key == "help_overlay_text_color")   config.helpOverlayTextColor = parseRgba(value, key, path, lineNumber);
            else if (key == "help_overlay_seconds")      config.helpOverlaySeconds = parseHelpOverlaySeconds(value, key, path, lineNumber);
            else if (key == "hide_mouse_cursor")         config.hideMouseCursor = parseBool(value, key, path, lineNumber);
            else if (key == "background_color")
            {
                config.backgroundColor = parseRgba(value, key, path, lineNumber);
                config.backgroundColorExplicit = true;
            }
            else if (key == "band_color")
            {
                config.bandColor = parseRgba(value, key, path, lineNumber);
                config.bandColorExplicit = true;
            }
            // v0.6 Color Presets (dot-prefixed only)
            else if (key == "color_presets")
            {
                // File provides color_presets= : replace built-ins (or disable if empty value)
                config.colorPresetNames.clear();
                config.colorPresetLabels.clear();
                config.colorPresetOverrides.clear();
                config.initialColorPreset.clear();

                std::vector<std::string> parsedPresets;
                std::istringstream ss(value);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    std::string t = trim(token);
                    if (!t.empty())
                    {
                        if (!isValidPresetId(t))
                        {
                            std::cerr << "Error: Invalid color preset ID '" << t << "' in " << path << ":" << lineNumber << std::endl;
                            std::exit(1);
                        }
                        if (std::find(parsedPresets.begin(), parsedPresets.end(), t) != parsedPresets.end())
                        {
                            std::cerr << "Error: Duplicate color preset ID '" << t << "' in " << path << ":" << lineNumber << std::endl;
                            std::exit(1);
                        }
                        parsedPresets.push_back(t);
                    }
                }
                config.colorPresetNames = parsedPresets;
            }
            else if (key == "color_preset")
            {
                std::string id = trim(value);
                if (!isValidPresetId(id))
                {
                    std::cerr << "Error: Invalid color preset ID '" << id << "' in " << path << ":" << lineNumber << std::endl;
                    std::exit(1);
                }
                config.initialColorPreset = id;
            }
            else if (key.rfind("color_preset.", 0) == 0)
            {
                std::string rest = key.substr(13);
                size_t dot = rest.find('.');
                if (dot == std::string::npos)
                {
                    std::cerr << "Error: Invalid color preset key format '" << key << "' in " << path << ":" << lineNumber << std::endl;
                    std::exit(1);
                }
                std::string presetId = rest.substr(0, dot);
                std::string subkey = rest.substr(dot + 1);

                if (!isValidPresetId(presetId))
                {
                    std::cerr << "Error: Invalid color preset ID '" << presetId << "' in " << path << ":" << lineNumber << std::endl;
                    std::exit(1);
                }

                if (subkey == "name")
                {
                    // .name is metadata only, not a color override
                    std::string label = trim(value);
                    if (label.empty())
                    {
                        std::cerr << "Error: color_preset." << presetId << ".name requires a non-empty value in " << path << ":" << lineNumber << std::endl;
                        std::exit(1);
                    }
                    config.colorPresetLabels[presetId] = label;
                    // do not store in overrides, do not parse as color
                    continue;
                }

                std::string field = colorKeyToField(subkey);
                if (field.empty())
                {
                    std::cerr << "Error: Unknown color key '" << subkey << "' in preset '" << presetId << "' in " << path << ":" << lineNumber << std::endl;
                    std::exit(1);
                }

                RgbaColor col = parseRgba(value, key, path, lineNumber);
                registerPresetColor(config, presetId, subkey, col, path, lineNumber);
            }
            else
            {
                std::cerr << "Warning: Unknown config key '" << key << "' in " << path << ":" << lineNumber << "\n";
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error parsing " << key << " = " << value << " in " << path << ":" << lineNumber << ": " << e.what() << "\n";
            errorCount++;
        }
    }
}

// Helper to safely get next CLI value or error
static std::string getRequiredCliValue(int& i, int argc, char** argv, const std::string& option)
{
    if (i + 1 >= argc)
    {
        std::cerr << "Error: " << option << " requires a value\n";
        std::exit(1);
    }
    std::string val = argv[++i];
    if (val.rfind("--", 0) == 0)
    {
        std::cerr << "Error: " << option << " requires a value\n";
        std::exit(1);
    }
    return val;
}

// --- Main config parser with correct precedence ---
Config parseConfig(int argc, char** argv)
{
    Config config;
    config.fontPath = findDefaultFontPath();

    // v0.6 Built-in color presets (active when no config file or file omits color_presets=)
    config.colorPresetNames = {"default", "high_contrast"};
    config.colorPresetLabels["default"] = "Default";
    config.colorPresetLabels["high_contrast"] = "High Contrast";
    config.initialColorPreset = "default";

    // Built-in high_contrast provides overrides (default is label-only, uses base colors)
    config.colorPresetOverrides["high_contrast"]["status_inactive_color"] = {220, 220, 220, 255};
    config.colorPresetOverrides["high_contrast"]["status_no_peers_color"] = {220, 220, 220, 255};
    config.colorPresetOverrides["high_contrast"]["status_connected_color"] = {255, 255, 255, 255};
    config.colorPresetOverrides["high_contrast"]["tempo_color"] = {255, 255, 255, 255};
    config.colorPresetOverrides["high_contrast"]["phase_bar_color"] = {255, 255, 255, 255};
    config.colorPresetOverrides["high_contrast"]["phase_marker_color"] = {0, 0, 0, 255};
    config.colorPresetOverrides["high_contrast"]["top_band_color"] = {0, 0, 0, 255};
    config.colorPresetOverrides["high_contrast"]["center_band_color"] = {0, 0, 0, 255};
    config.colorPresetOverrides["high_contrast"]["bottom_band_color"] = {0, 0, 0, 255};
    config.colorPresetOverrides["high_contrast"]["help_overlay_background_color"] = {0, 0, 0, 230};
    config.colorPresetOverrides["high_contrast"]["help_overlay_text_color"] = {255, 255, 255, 255};

    std::string configFilePath;
    bool configFileExplicitlyRequested = false;

    // === Pass 1: Look for --config only ===
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--config")
        {
            if (i + 1 >= argc || std::string(argv[i+1]).rfind("--", 0) == 0)
            {
                std::cerr << "Error: --config requires a PATH argument\n";
                std::exit(1);
            }
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

    // === Apply legacy alias fallback logic ONLY if the deprecated alias was explicitly present ===
    if (config.backgroundColorExplicit && !config.centerBandColorExplicit)
    {
        config.centerBandColor = config.backgroundColor;
    }
    if (config.bandColorExplicit)
    {
        if (!config.topBandColorExplicit)
            config.topBandColor = config.bandColor;
        if (!config.bottomBandColorExplicit)
            config.bottomBandColor = config.bandColor;
    }

    // === Pass 2: Apply all CLI overrides (strict) ===
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); std::exit(0); }
        else if (arg == "--print-config")     config.printConfig = true;
        else if (arg == "--module-info")      config.moduleInfo = true;
        else if (arg == "--no-gui")               config.noGui = true;
        else if (arg == "--windowed")             config.fullscreen = false;
        else if (arg == "--config")
        {
            if (i + 1 >= argc || std::string(argv[i+1]).rfind("--", 0) == 0)
            {
                std::cerr << "Error: --config requires a PATH argument\n";
                std::exit(1);
            }
            ++i;  // skip the path value
            continue;
        }
        else if (arg == "--width")
        {
            std::string val = getRequiredCliValue(i, argc, argv, "--width");
            config.width = parsePositiveIntCli(val, "--width");
        }
        else if (arg == "--height")
        {
            std::string val = getRequiredCliValue(i, argc, argv, "--height");
            config.height = parsePositiveIntCli(val, "--height");
        }
        else if (arg == "--font")
        {
            config.fontPath = getRequiredCliValue(i, argc, argv, "--font");
        }
        else if (arg == "--tempo")
        {
            std::string val = getRequiredCliValue(i, argc, argv, "--tempo");
            config.initialTempo = parsePositiveDoubleCli(val, "--tempo");
        }
        else if (arg == "--quantum")
        {
            std::string val = getRequiredCliValue(i, argc, argv, "--quantum");
            config.quantum = parsePositiveDoubleCli(val, "--quantum");
        }
        else if (arg.rfind("--", 0) == 0)
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            std::exit(1);
        }
    }

    // v0.6: Validate every listed preset has at least one definition (.name or color key)
    for (const auto& id : config.colorPresetNames)
    {
        bool hasDef = (config.colorPresetLabels.find(id) != config.colorPresetLabels.end()) ||
                      (config.colorPresetOverrides.find(id) != config.colorPresetOverrides.end());
        if (!hasDef)
        {
            std::cerr << "Error: color preset '" << id << "' is listed in color_presets but has no definition (no color_preset." << id << ".* entries) in " << config.configPath << std::endl;
            std::exit(1);
        }
    }

    // Capture base colors AFTER all defaults/file/CLI/alias/validation, BEFORE initial preset apply
    captureBaseColors(config);

    // v0.6: Apply initial color preset if defined
    if (!config.colorPresetNames.empty())
    {
        std::string startPreset = config.initialColorPreset;
        if (startPreset.empty())
        {
            startPreset = config.colorPresetNames[0];
        }

        // Validate that the initial preset exists in the list
        auto it = std::find(config.colorPresetNames.begin(), config.colorPresetNames.end(), startPreset);
        if (it == config.colorPresetNames.end())
        {
            std::cerr << "Error: color_preset '" << startPreset << "' not listed in color_presets" << std::endl;
            std::exit(1);
        }

        config.activeColorPresetIndex = static_cast<int>(std::distance(config.colorPresetNames.begin(), it));
        // active name derived from activeColorPresetIndex via getActiveColorPresetName

        // Apply overrides on top of base colors
        applyColorPreset(config, startPreset);
    }

    return config;
}

// --- v0.5 inspection functions ---

void printEffectiveConfig(const Config& config)
{
    std::cout
        << "width=" << config.width << std::endl
        << "height=" << config.height << std::endl
        << "fullscreen=" << (config.fullscreen ? "true" : "false") << std::endl
        << "no_gui=" << (config.noGui ? "true" : "false") << std::endl
        << "tempo=" << std::fixed << std::setprecision(2) << config.initialTempo << std::endl
        << "quantum=" << std::fixed << std::setprecision(2) << config.quantum << std::endl
        << "font_path=" << config.fontPath << std::endl
        << "status_font_size=" << config.statusFontSize << std::endl
        << "tempo_font_size=" << config.tempoFontSize << std::endl
        << "bottom_font_size=" << config.bottomFontSize << std::endl
        << "help_font_size=" << config.helpFontSize << std::endl
        << "status_inactive_color=" << config.statusInactiveColor.r << "," << config.statusInactiveColor.g << "," << config.statusInactiveColor.b << "," << config.statusInactiveColor.a << std::endl
        << "status_no_peers_color=" << config.statusNoPeersColor.r << "," << config.statusNoPeersColor.g << "," << config.statusNoPeersColor.b << "," << config.statusNoPeersColor.a << std::endl
        << "status_connected_color=" << config.statusConnectedColor.r << "," << config.statusConnectedColor.g << "," << config.statusConnectedColor.b << "," << config.statusConnectedColor.a << std::endl
        << "tempo_color=" << config.tempoColor.r << "," << config.tempoColor.g << "," << config.tempoColor.b << "," << config.tempoColor.a << std::endl
        << "phase_bar_color=" << config.phaseBarColor.r << "," << config.phaseBarColor.g << "," << config.phaseBarColor.b << "," << config.phaseBarColor.a << std::endl
        << "phase_marker_color=" << config.phaseMarkerColor.r << "," << config.phaseMarkerColor.g << "," << config.phaseMarkerColor.b << "," << config.phaseMarkerColor.a << std::endl
        << "top_band_color=" << config.topBandColor.r << "," << config.topBandColor.g << "," << config.topBandColor.b << "," << config.topBandColor.a << std::endl
        << "center_band_color=" << config.centerBandColor.r << "," << config.centerBandColor.g << "," << config.centerBandColor.b << "," << config.centerBandColor.a << std::endl
        << "bottom_band_color=" << config.bottomBandColor.r << "," << config.bottomBandColor.g << "," << config.bottomBandColor.b << "," << config.bottomBandColor.a << std::endl
        << "help_overlay_background_color=" << config.helpOverlayBackgroundColor.r << "," << config.helpOverlayBackgroundColor.g << "," << config.helpOverlayBackgroundColor.b << "," << config.helpOverlayBackgroundColor.a << std::endl
        << "help_overlay_text_color=" << config.helpOverlayTextColor.r << "," << config.helpOverlayTextColor.g << "," << config.helpOverlayTextColor.b << "," << config.helpOverlayTextColor.a << std::endl
        << "phase_bar_height=" << config.phaseBarHeight << std::endl
        << "phase_bar_segment_gap=" << config.phaseBarSegmentGap << std::endl
        << "phase_bar_margin=" << config.phaseBarMargin << std::endl
        << "help_overlay_seconds=" << config.helpOverlaySeconds << std::endl
        << "hide_mouse_cursor=" << (config.hideMouseCursor ? "true" : "false") << std::endl;

    std::cout << "color_presets=";
    for (size_t i = 0; i < config.colorPresetNames.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.colorPresetNames[i];
    }
    std::cout << std::endl;
    std::cout << "active_color_preset=" << getActiveColorPresetName(config) << std::endl;
}

void printModuleInfo()
{
    std::cout
        << "module_name=raspi-link-display\n"
        << "display_name=Raspberry Pi Ableton Link Display\n"
        << "module_type=display\n"
        << "version=0.4.0\n"
        << "validated_tag=v0.4-pi-validated\n"
        << "validated_commit=a79db44\n"
        << "runtime=native-cpp-sdl2\n"
        << "language=c++17\n"
        << "display_target=480x320\n"
        << "primary_protocols=ableton-link\n"
        << "inputs=keyboard\n"
        << "outputs=sdl2-display,console-no-gui\n"
        << "controls=F1,F,P,Q,Esc\n"
        << "config_file=config/link-pi-display.example.conf\n"
        << "external_control=not implemented\n"
        << "container_integration=future\n"
        << "manual_start=true\n"
        << "systemd_enabled=false\n";
}
