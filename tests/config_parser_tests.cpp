#include "Config.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <cstdio>

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

// Helper to build argv from vector (argc = size, no nullptr)
static std::vector<char*> make_argv(const std::vector<std::string>& args)
{
    std::vector<char*> argv;
    for (auto& s : args)
    {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    return argv;
}

// Run a child test case by re-executing self with special flag
static int run_child(const std::string& case_name, const std::vector<std::string>& args)
{
    std::string exe = (argc > 0 ? std::string(argv[0]) : std::string("./config_parser_tests"));
    std::string cmd = exe + " --test-child " + case_name;
    for (size_t i = 0; i < args.size(); ++i)
    {
        cmd += " " + args[i];
    }
    return std::system(cmd.c_str());
}

int main(int argc, char** argv)
{
    // Support child mode for negative tests
    if (argc > 2 && std::string(argv[1]) == "--test-child")
    {
        std::string case_name = argv[2];
        // Rebuild argv for the child case (skip the --test-child and name)
        std::vector<std::string> child_args;
        child_args.push_back(argv[0]);
        for (int i = 3; i < argc; ++i)
        {
            child_args.push_back(argv[i]);
        }
        auto vargv = make_argv(child_args);
        // Call parseConfig - it will exit on error, which is what we want for negative
        // The parent will see the exit code
        Config c = parseConfig(vargv.size(), vargv.data());
        // If we reach here for a negative case, it's a fail, but parent will see exit 0
        return 0;
    }

    // === Good paths ===
    {
        auto v = make_argv({"test"});
        Config c = parseConfig(v.size(), v.data());
        expect("default_config_width", c.width == 480);
        expect("default_config_fullscreen", c.fullscreen == true);
        expect("default_config_tempo", c.initialTempo == 120.0);
        expect("default_config_center_band", c.centerBandColor.r == 18 && c.centerBandColor.g == 18 && c.centerBandColor.b == 18);
    }

    {
        auto v = make_argv({"test", "--config", "config/link-pi-display.example.conf"});
        Config c = parseConfig(v.size(), v.data());
        expect("example_config_width", c.width == 480);
        expect("example_config_hide_mouse", c.hideMouseCursor == true);
        expect("example_config_top_band", c.topBandColor.r == 0 && c.topBandColor.g == 0 && c.topBandColor.b == 0);
    }

    {
        auto v = make_argv({"test", "--windowed", "--width", "640", "--tempo", "125"});
        Config c = parseConfig(v.size(), v.data());
        expect("cli_windowed", c.fullscreen == false);
        expect("cli_width", c.width == 640);
        expect("cli_tempo", c.initialTempo == 125.0);
    }

    // === Strict validation failures (via child) ===
    expect("bad_width_negative", run_child("bad_width_negative", {"--width", "-480"}) != 0);
    expect("bad_height_zero", run_child("bad_height_zero", {"--height", "0"}) != 0);
    expect("bad_status_font_size_zero", run_child("bad_status_font_size_zero", {"--status_font_size", "0"}) != 0);  // note: may need -- for CLI, but parser will catch in validation
    expect("bad_phase_gap_negative", run_child("bad_phase_gap_negative", {"--phase_bar_segment_gap", "-7"}) != 0);
    expect("bad_help_overlay_seconds", run_child("bad_help_overlay_seconds", {"--help_overlay_seconds", "999"}) != 0);
    expect("bad_color_value", run_child("bad_color_value", {"--status_inactive_color", "999,44,48,255"}) != 0);
    expect("bad_width_partial", run_child("bad_width_partial", {"--width", "480abc"}) != 0);
    expect("bad_tempo_partial", run_child("bad_tempo_partial", {"--tempo", "120abc"}) != 0);
    expect("bad_color_partial", run_child("bad_color_partial", {"--status_inactive_color", "40abc,44,48,255"}) != 0);

    // === Alias behavior ===
    // For alias, we can test by creating temp config with background_color and check center
    {
        std::ofstream tmp(" /tmp/test_alias_bg.conf");
        tmp << "background_color=10,20,30,255\n";
        tmp.close();
        auto v = make_argv({"test", "--config", "/tmp/test_alias_bg.conf"});
        Config c = parseConfig(v.size(), v.data());
        expect("alias_background_maps_to_center", c.centerBandColor.r == 10 && c.centerBandColor.g == 20 && c.centerBandColor.b == 30);
        std::remove("/tmp/test_alias_bg.conf");
    }

    {
        std::ofstream tmp("/tmp/test_alias_band.conf");
        tmp << "band_color=5,6,7,255\n";
        tmp.close();
        auto v = make_argv({"test", "--config", "/tmp/test_alias_band.conf"});
        Config c = parseConfig(v.size(), v.data());
        expect("alias_band_maps_to_top", c.topBandColor.r == 5 && c.topBandColor.g == 6 && c.topBandColor.b == 7);
        expect("alias_band_maps_to_bottom", c.bottomBandColor.r == 5 && c.bottomBandColor.g == 6 && c.bottomBandColor.b == 7);
        std::remove("/tmp/test_alias_band.conf");
    }

    // === CLI behavior failures (via child) ===
    expect("unknown_option", run_child("unknown_option", {"--foo-bar"}) != 0);
    expect("missing_config_value", run_child("missing_config_value", {"--config"}) != 0);
    expect("missing_width_value", run_child("missing_width_value", {"--width"}) != 0);
    expect("bad_width_negative_cli", run_child("bad_width_negative_cli", {"--width", "-480"}) != 0);
    expect("bad_width_partial_cli", run_child("bad_width_partial_cli", {"--width", "480abc"}) != 0);
    expect("bad_tempo_negative_cli", run_child("bad_tempo_negative_cli", {"--tempo", "-1"}) != 0);
    expect("bad_tempo_partial_cli", run_child("bad_tempo_partial_cli", {"--tempo", "120abc"}) != 0);

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
