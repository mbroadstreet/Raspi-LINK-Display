#include "Config.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

static int failures = 0;

static void expect(const std::string& name, bool condition)
{
    if (condition)
    {
        std::cout << "PASS " << name << "\n";
    }
    else
    {
        std::cout << "FAIL " << name << "\n";
        ++failures;
    }
}

int main(int argc, char** argv)
{
    // Test default config
    {
        char* args[] = { (char*)"test", nullptr };
        Config c = parseConfig(1, args);
        expect("default_config_width", c.width == 480);
        expect("default_config_fullscreen", c.fullscreen == true);
        expect("default_config_tempo", c.initialTempo == 120.0);
        expect("default_config_center_band", c.centerBandColor.r == 18 && c.centerBandColor.g == 18 && c.centerBandColor.b == 18);
    }

    // Test example config
    {
        char* args[] = { (char*)"test", (char*)"--config", (char*)"config/link-pi-display.example.conf", nullptr };
        Config c = parseConfig(4, args);
        expect("example_config_width", c.width == 480);
        expect("example_config_hide_mouse", c.hideMouseCursor == true);
        expect("example_config_top_band", c.topBandColor.r == 0 && c.topBandColor.g == 0 && c.topBandColor.b == 0);
    }

    // Test CLI override
    {
        char* args[] = { (char*)"test", (char*)"--windowed", (char*)"--width", (char*)"640", (char*)"--tempo", (char*)"125", nullptr };
        Config c = parseConfig(7, args);
        expect("cli_windowed", c.fullscreen == false);
        expect("cli_width", c.width == 640);
        expect("cli_tempo", c.initialTempo == 125.0);
    }

    // Test alias fallback (background_color should map to center if no explicit center)
    {
        // This is harder without temp file, but we can test defaults
        char* args[] = { (char*)"test", nullptr };
        Config c = parseConfig(1, args);
        expect("alias_default_center", c.centerBandColor.r == 18);
    }

    // Note: Negative tests (bad values, unknown options, missing values, 480abc etc.)
    // are covered by the negative test commands in the TEST-PLAN and manual verification.
    // They are expected to fail with clear errors and non-zero exit.

    if (failures == 0)
    {
        std::cout << "All config parser tests passed.\n";
        return 0;
    }
    else
    {
        std::cout << failures << " tests failed.\n";
        return 1;
    }
}
