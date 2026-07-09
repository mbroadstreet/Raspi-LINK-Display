#include "Config.h"
#include "DisplayText.h"
#include "LinkEngine.h"
#include "SdlRenderer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <thread>

namespace
{
    std::atomic<bool> gShouldQuit {false};

    void handleSignal(int)
    {
        gShouldQuit.store(true);
    }

    void runConsole(LinkEngine& engine)
    {
        while (!gShouldQuit.load())
        {
            const auto state = engine.snapshot();
            std::cout
                << DisplayText::formatStatusLine(state) << " | "
                << DisplayText::formatTempoLine(state) << " | "
                << DisplayText::formatBeatPhaseLine(state) << "\n";

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void runGui(const Config& config, LinkEngine& engine)
    {
        SdlRenderer renderer(config);

        const auto frameTime = std::chrono::milliseconds(33);

        while (!gShouldQuit.load())
        {
            const auto frameStart = std::chrono::steady_clock::now();

            if (renderer.pollQuit())
            {
                break;
            }

            renderer.render(engine.snapshot());

            const auto elapsed = std::chrono::steady_clock::now() - frameStart;
            if (elapsed < frameTime)
            {
                std::this_thread::sleep_for(frameTime - elapsed);
            }
        }
    }
}

int main(int argc, char** argv)
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    try
    {
        const Config config = parseConfig(argc, argv);
        if (config.printConfig) {
            printEffectiveConfig(config);
            return 0;
        }
        if (config.moduleInfo) {
            printModuleInfo();
            return 0;
        }
        LinkEngine engine(config.initialTempo, config.quantum);

        if (config.noGui)
        {
            runConsole(engine);
        }
        else
        {
            runGui(config, engine);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "Run with --help for usage.\n";
        return 1;
    }

    return 0;
}
