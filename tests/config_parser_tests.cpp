#include "Config.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <cstdio>

static int failures = 0;
static std::string selfPath;

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
    std::string exe = selfPath.empty() ? std::string("./config_parser_tests") : selfPath;
    std::string cmd = exe + " --test-child " + case_name;
    for (size_t i = 0; i < args.size(); ++i)
    {
        cmd += " " + args[i];
    }
    return std::system(cmd.c_str());
}

int main(int argc, char** argv)
{
    selfPath = (argc > 0 ? std::string(argv[0]) : std::string("./config_parser_tests"));

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

    // === Strict validation via temp config files (for config-file keys) ===
    {
        std::ofstream tmp("/tmp/test_bad_status_font.conf");
        tmp << "status_font_size=0\n";
        tmp.close();
        auto v = make_argv({"test", "--config", "/tmp/test_bad_status_font.conf"});
        int rc = run_child("bad_status_font_size_zero", {"--config", "/tmp/test_bad_status_font.conf"});
        expect("bad_status_font_size_zero", rc != 0);
        std::remove("/tmp/test_bad_status_font.conf");
    }

    {
        std::ofstream tmp("/tmp/test_bad_phase_gap.conf");
        tmp << "phase_bar_segment_gap=-7\n";
        tmp.close();
        int rc = run_child("bad_phase_gap_negative", {"--config", "/tmp/test_bad_phase_gap.conf"});
        expect("bad_phase_gap_negative", rc != 0);
        std::remove("/tmp/test_bad_phase_gap.conf");
    }

    {
        std::ofstream tmp("/tmp/test_bad_help_overlay.conf");
        tmp << "help_overlay_seconds=999\n";
        tmp.close();
        int rc = run_child("bad_help_overlay_seconds", {"--config", "/tmp/test_bad_help_overlay.conf"});
        expect("bad_help_overlay_seconds", rc != 0);
        std::remove("/tmp/test_bad_help_overlay.conf");
    }

    {
        std::ofstream tmp("/tmp/test_bad_color_value.conf");
        tmp << "status_inactive_color=999,44,48,255\n";
        tmp.close();
        int rc = run_child("bad_color_value", {"--config", "/tmp/test_bad_color_value.conf"});
        expect("bad_color_value", rc != 0);
        std::remove("/tmp/test_bad_color_value.conf");
    }

    {
        std::ofstream tmp("/tmp/test_bad_color_partial.conf");
        tmp << "status_inactive_color=40abc,44,48,255\n";
        tmp.close();
        int rc = run_child("bad_color_partial", {"--config", "/tmp/test_bad_color_partial.conf"});
        expect("bad_color_partial", rc != 0);
        std::remove("/tmp/test_bad_color_partial.conf");
    }

    // CLI-based negatives (real CLI options)
    expect("bad_width_negative", run_child("bad_width_negative", {"--width", "-480"}) != 0);
    expect("bad_height_zero", run_child("bad_height_zero", {"--height", "0"}) != 0);
    expect("bad_width_partial", run_child("bad_width_partial", {"--width", "480abc"}) != 0);
    expect("bad_tempo_partial", run_child("bad_tempo_partial", {"--tempo", "120abc"}) != 0);

    // === Alias behavior ===
    // background_color maps to center only if center absent
    {
        std::ofstream tmp("/tmp/test_alias_bg.conf");
        tmp << "background_color=10,20,30,255\n";
        tmp.close();
        auto v = make_argv({"test", "--config", "/tmp/test_alias_bg.conf"});
        Config c = parseConfig(v.size(), v.data());
        expect("alias_background_maps_to_center", c.centerBandColor.r == 10 && c.centerBandColor.g == 20 && c.centerBandColor.b == 30);
        std::remove("/tmp/test_alias_bg.conf");
    }

    // band_color maps to top/bottom only if explicit top/bottom absent
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

    // explicit top/center/bottom win over aliases
    {
        std::ofstream tmp("/tmp/test_explicit_wins.conf");
        tmp << "background_color=99,99,99,255\n";  // should be ignored
        tmp << "center_band_color=11,22,33,255\n"; // explicit wins
        tmp << "band_color=44,55,66,255\n";         // should be ignored for top/bottom
        tmp << "top_band_color=1,2,3,255\n";
        tmp << "bottom_band_color=7,8,9,255\n";
        tmp.close();
        auto v = make_argv({"test", "--config", "/tmp/test_explicit_wins.conf"});
        Config c = parseConfig(v.size(), v.data());
        expect("explicit_center_wins_over_alias", c.centerBandColor.r == 11 && c.centerBandColor.g == 22 && c.centerBandColor.b == 33);
        expect("explicit_top_wins_over_alias", c.topBandColor.r == 1 && c.topBandColor.g == 2 && c.topBandColor.b == 3);
        expect("explicit_bottom_wins_over_alias", c.bottomBandColor.r == 7 && c.bottomBandColor.g == 8 && c.bottomBandColor.b == 9);
        std::remove("/tmp/test_explicit_wins.conf");
    }

    // === CLI behavior failures (real CLI options) ===
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
