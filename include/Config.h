#pragma once

#include <string>

struct Config
{
    int width = 480;
    int height = 320;
    bool fullscreen = true;
    bool noGui = false;
    double initialTempo = 120.0;
    double quantum = 4.0;
    std::string fontPath;
};

Config parseConfig(int argc, char** argv);
std::string findDefaultFontPath();
void printUsage(const char* argv0);
