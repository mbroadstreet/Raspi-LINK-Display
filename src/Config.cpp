#include "Config.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string findDefaultFontPath()
{
    const std::vector<std::string> candidates = {
        "./assets/fonts/display.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"
    };

    for (const auto& path : candidates)
    {
        if (fs::exists(path))
        {
            return path;
        }
    }

    return {};
}

void printUsage(const char* argv0)
{
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  --no-gui              Print Link state to console once per second.\n"
        << "  --windowed            Run in a window instead of fullscreen.\n"
        << "  --width N             Window/layout width. Default: 480.\n"
        << "  --height N            Window/layout height. Default: 320.\n"
        << "  --font PATH           TTF font path.\n"
        << "  --tempo BPM           Initial tempo before joining a session. Default: 120.\n"
        << "  --quantum BEATS       Phase quantum. Default: 4.\n"
        << "  --help                Show this help.\n";
}

Config parseConfig(int argc, char** argv)
{
    Config config;
    config.fontPath = findDefaultFontPath();

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        auto needValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc)
            {
                throw std::runtime_error(std::string("Missing value for ") + name);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--no-gui")
        {
            config.noGui = true;
        }
        else if (arg == "--windowed")
        {
            config.fullscreen = false;
        }
        else if (arg == "--width")
        {
            config.width = std::stoi(needValue("--width"));
        }
        else if (arg == "--height")
        {
            config.height = std::stoi(needValue("--height"));
        }
        else if (arg == "--font")
        {
            config.fontPath = needValue("--font");
        }
        else if (arg == "--tempo")
        {
            config.initialTempo = std::stod(needValue("--tempo"));
        }
        else if (arg == "--quantum")
        {
            config.quantum = std::stod(needValue("--quantum"));
        }
        else
        {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (config.width <= 0 || config.height <= 0)
    {
        throw std::runtime_error("Width and height must be positive.");
    }

    if (config.quantum <= 0.0)
    {
        throw std::runtime_error("Quantum must be positive.");
    }

    return config;
}
