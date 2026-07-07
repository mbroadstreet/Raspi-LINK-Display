#include "LinkEngine.h"
#include "DisplayText.h"
#include "SdlRenderer.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <getopt.h>

int main(int argc, char* argv[]) {
    double initialTempo = 120.0;
    double quantum = 4.0;
    int width = 480;
    int height = 320;
    std::string fontPath = "font.ttf"; // not bundled
    bool noGui = false;
    bool windowed = false;

    // Simple option parsing (supports the required flags)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-gui") noGui = true;
        else if (arg == "--windowed") windowed = true;
        else if (arg == "--width" && i+1 < argc) width = std::stoi(argv[++i]);
        else if (arg == "--height" && i+1 < argc) height = std::stoi(argv[++i]);
        else if (arg == "--font" && i+1 < argc) fontPath = argv[++i];
        else if (arg == "--tempo" && i+1 < argc) initialTempo = std::stod(argv[++i]);
        else if (arg == "--quantum" && i+1 < argc) quantum = std::stod(argv[++i]);
    }

    LinkEngine linkEngine(initialTempo, quantum);
    linkEngine.enable(true);

    if (noGui) {
        std::cout << "Running in --no-gui console mode (SSH safe)" << std::endl;
        while (true) {
            auto state = linkEngine; // re-capture each poll
            std::string status = DisplayText::statusLine(linkEngine.isEnabled(), linkEngine.numPeers());
            std::string tempo = DisplayText::tempoLine(linkEngine.tempo());
            std::string beatPhase = DisplayText::beatPhaseLine(linkEngine.beat(), linkEngine.phase(), linkEngine.quantum());
            std::cout << status << " | " << tempo << " | " << beatPhase << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS pacing
        }
        return 0;
    }

    SdlRenderer renderer(width, height, fontPath);
    if (renderer.shouldQuit()) {
        std::cerr << "Renderer initialization failed" << std::endl;
        return 1;
    }

    // Simple explicit frame pacing (~30 FPS) without mixing vsync + unconditional delay
    auto last = std::chrono::steady_clock::now();
    const auto frameTime = std::chrono::milliseconds(33);

    while (!renderer.shouldQuit()) {
        // Re-capture SessionState every frame (no long-lived state)
        std::string status = DisplayText::statusLine(linkEngine.isEnabled(), linkEngine.numPeers());
        std::string tempoStr = DisplayText::tempoLine(linkEngine.tempo());
        std::string beatPhase = DisplayText::beatPhaseLine(linkEngine.beat(), linkEngine.phase(), linkEngine.quantum());

        renderer.render(status, tempoStr, beatPhase, windowed);
        renderer.present();

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
        if (elapsed < frameTime) {
            std::this_thread::sleep_for(frameTime - elapsed);
        }
        last = std::chrono::steady_clock::now();
    }

    return 0;
}