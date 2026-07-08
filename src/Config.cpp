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

    return config;
}
