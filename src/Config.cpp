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
                config.colorPresetNames.clear();
                std::istringstream ss(value);
                std::string token;
                while (std::getline(ss, token, ','))
                {
                    std::string t = trim(token);
                    if (!t.empty())
                    {
                        if (!isValidPresetId(t))
                        {
                            std::cerr << "Error: Invalid color preset ID '" << t << "' in " << path << ":" << lineNumber << "
";
                            std::exit(1);
                        }
                        config.colorPresetNames.push_back(t);
                    }
                }
            }
            else if (key == "color_preset")
            {
                std::string id = trim(value);
                if (!isValidPresetId(id))
                {
                    std::cerr << "Error: Invalid color preset ID '" << id << "' in " << path << ":" << lineNumber << "
";
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
                    std::cerr << "Error: Invalid color preset key format '" << key << "' in " << path << ":" << lineNumber << "
";
                    std::exit(1);
                }
                std::string presetId = rest.substr(0, dot);
                std::string subkey = rest.substr(dot + 1);

                if (!isValidPresetId(presetId))
                {
                    std::cerr << "Error: Invalid color preset ID '" << presetId << "' in " << path << ":" << lineNumber << "
";
                    std::exit(1);
                }

                std::string field = colorKeyToField(subkey);
                if (field.empty())
                {
                    std::cerr << "Error: Unknown color key '" << subkey << "' in preset '" << presetId << "' in " << path << ":" << lineNumber << "
";
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
            std::cerr << "Error: color_preset '" << startPreset << "' not listed in color_presets
";
            std::exit(1);
        }

        config.activeColorPresetIndex = static_cast<int>(std::distance(config.colorPresetNames.begin(), it));
        config.activeColorPresetName = startPreset;  // for print

        // Apply overrides on top of base colors
        applyColorPreset(config, startPreset);
    }

    return config;
}

// --- v0.5 inspection functions ---

void printEffectiveConfig(const Config& config)
{
    std::cout
        << "width=" << config.width << "\n"
        << "height=" << config.height << "\n"
        << "fullscreen=" << (config.fullscreen ? "true" : "false") << "\n"
        << "no_gui=" << (config.noGui ? "true" : "false") << "\n"
        << "tempo=" << std::fixed << std::setprecision(2) << config.initialTempo << "\n"
        << "quantum=" << std::fixed << std::setprecision(2) << config.quantum << "\n"
        << "font_path=" << config.fontPath << "\n"
        << "status_font_size=" << config.statusFontSize << "\n"
        << "tempo_font_size=" << config.tempoFontSize << "\n"
        << "bottom_font_size=" << config.bottomFontSize << "\n"
        << "help_font_size=" << config.helpFontSize << "\n"
        << "status_inactive_color=" << config.statusInactiveColor.r << "," << config.statusInactiveColor.g << "," << config.statusInactiveColor.b << "," << config.statusInactiveColor.a << "\n"
        << "status_no_peers_color=" << config.statusNoPeersColor.r << "," << config.statusNoPeersColor.g << "," << config.statusNoPeersColor.b << "," << config.statusNoPeersColor.a << "\n"
        << "status_connected_color=" << config.statusConnectedColor.r << "," << config.statusConnectedColor.g << "," << config.statusConnectedColor.b << "," << config.statusConnectedColor.a << "\n"
        << "tempo_color=" << config.tempoColor.r << "," << config.tempoColor.g << "," << config.tempoColor.b << "," << config.tempoColor.a << "\n"
        << "phase_bar_color=" << config.phaseBarColor.r << "," << config.phaseBarColor.g << "," << config.phaseBarColor.b << "," << config.phaseBarColor.a << "\n"
        << "phase_marker_color=" << config.phaseMarkerColor.r << "," << config.phaseMarkerColor.g << "," << config.phaseMarkerColor.b << "," << config.phaseMarkerColor.a << "\n"
        << "top_band_color=" << config.topBandColor.r << "," << config.topBandColor.g << "," << config.topBandColor.b << "," << config.topBandColor.a << "\n"
        << "center_band_color=" << config.centerBandColor.r << "," << config.centerBandColor.g << "," << config.centerBandColor.b << "," << config.centerBandColor.a << "\n"
        << "bottom_band_color=" << config.bottomBandColor.r << "," << config.bottomBandColor.g << "," << config.bottomBandColor.b << "," << config.bottomBandColor.a << "\n"
        << "help_overlay_background_color=" << config.helpOverlayBackgroundColor.r << "," << config.helpOverlayBackgroundColor.g << "," << config.helpOverlayBackgroundColor.b << "," << config.helpOverlayBackgroundColor.a << "\n"
        << "help_overlay_text_color=" << config.helpOverlayTextColor.r << "," << config.helpOverlayTextColor.g << "," << config.helpOverlayTextColor.b << "," << config.helpOverlayTextColor.a << "\n"
        << "phase_bar_height=" << config.phaseBarHeight << "\n"
        << "phase_bar_segment_gap=" << config.phaseBarSegmentGap << "\n"
        << "phase_bar_margin=" << config.phaseBarMargin << "\n"
        << "help_overlay_seconds=" << config.helpOverlaySeconds << "\n"
        << "hide_mouse_cursor=" << (config.hideMouseCursor ? "true" : "false") << "\n";
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
        << "controls=F1,F,Q,Esc\n"
        << "config_file=config/link-pi-display.example.conf\n"
        << "external_control=not implemented\n"
        << "container_integration=future\n"
        << "manual_start=true\n"
        << "systemd_enabled=false\n";
}
