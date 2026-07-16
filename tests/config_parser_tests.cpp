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

// Safe helper: owns the string storage so c_str() pointers remain valid during parseConfig()
static Config parse_for_test(const std::vector<std::string>& args)
{
    std::vector<std::string> storage = args;
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& s : storage)
    {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    return parseConfig(static_cast<int>(argv.size()), argv.data());
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
        std::vector<std::string> child_args;
        child_args.push_back(argv[0]);
        for (int i = 3; i < argc; ++i)
        {
            child_args.push_back(argv[i]);
        }
        Config c = parse_for_test(child_args);
        return 0;
    }

    // === Good paths ===
    {
        Config c = parse_for_test({"test"});
        expect("default_config_width", c.width == 480);
        expect("default_config_fullscreen", c.fullscreen == true);
        expect("default_config_tempo", c.initialTempo == 120.0);
        expect("default_config_center_band", c.centerBandColor.r == 18 && c.centerBandColor.g == 18 && c.centerBandColor.b == 18);
    }

    {
        Config c = parse_for_test({"test", "--config", "config/link-pi-display.example.conf"});
        expect("example_config_width", c.width == 480);
        expect("example_config_hide_mouse", c.hideMouseCursor == true);
        expect("example_config_top_band", c.topBandColor.r == 0 && c.topBandColor.g == 0 && c.topBandColor.b == 0);
    }

    {
        Config c = parse_for_test({"test", "--windowed", "--width", "640", "--tempo", "125"});
        expect("cli_windowed", c.fullscreen == false);
        expect("cli_width", c.width == 640);
        expect("cli_tempo", c.initialTempo == 125.0);
    }

    // === Strict validation via temp config files (for config-file keys) ===
    {
        std::ofstream tmp("/tmp/test_bad_status_font.conf");
        tmp << "status_font_size=0\n";
        tmp.close();
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
        Config c = parse_for_test({"test", "--config", "/tmp/test_alias_bg.conf"});
        expect("alias_background_maps_to_center", c.centerBandColor.r == 10 && c.centerBandColor.g == 20 && c.centerBandColor.b == 30);
        std::remove("/tmp/test_alias_bg.conf");
    }

    // band_color maps to top/bottom only if explicit top/bottom absent
    {
        std::ofstream tmp("/tmp/test_alias_band.conf");
        tmp << "band_color=5,6,7,255\n";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_alias_band.conf"});
        expect("alias_band_maps_to_top", c.topBandColor.r == 5 && c.topBandColor.g == 6 && c.topBandColor.b == 7);
        expect("alias_band_maps_to_bottom", c.bottomBandColor.r == 5 && c.bottomBandColor.g == 6 && c.bottomBandColor.b == 7);
        std::remove("/tmp/test_alias_band.conf");
    }

    // explicit top/center/bottom win over aliases
    {
        std::ofstream tmp("/tmp/test_explicit_wins.conf");
        tmp << "background_color=99,99,99,255\n";
        tmp << "center_band_color=11,22,33,255\n";
        tmp << "band_color=44,55,66,255\n";
        tmp << "top_band_color=1,2,3,255\n";
        tmp << "bottom_band_color=7,8,9,255\n";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_explicit_wins.conf"});
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
    expect("bad_tempo_partial_cli", run_child("bad_tempo_partial_cli", {"--tempo", "120abc"}) != 0);


    // === v0.6 Color Preset tests ===
    {
        // Good: basic presets via temp file
        std::ofstream tmp("/tmp/test_presets_good.conf");
        tmp << R"CFG(color_presets=default,high_contrast
color_preset=default
color_preset.default.name=Default
color_preset.default.tempo_color=64,79,96,255
color_preset.high_contrast.name=High Contrast
color_preset.high_contrast.tempo_color=255,255,255,255
)CFG";
        tmp.close();

        Config c = parse_for_test({"test", "--config", "/tmp/test_presets_good.conf"});
        expect("presets_list_size", c.colorPresetNames.size() == 2);
        expect("presets_initial", c.initialColorPreset == "default");
        expect("presets_active_name", getActiveColorPresetName(c) == "default");
        expect("presets_tempo_default", c.tempoColor.r == 64 && c.tempoColor.g == 79 && c.tempoColor.b == 96);

        std::remove("/tmp/test_presets_good.conf");
    }

    {
        // Negative: unknown initial preset
        std::ofstream tmp("/tmp/test_bad_initial_preset.conf");
        tmp << R"CFG(color_presets=default
color_preset=nonexistent
)CFG";
        tmp.close();

        int rc = run_child("bad_initial_preset", {"--config", "/tmp/test_bad_initial_preset.conf"});
        expect("bad_initial_preset_fails", rc != 0);
        std::remove("/tmp/test_bad_initial_preset.conf");
    }

    {
        // Negative: non-color key inside preset (e.g. width)
        std::ofstream tmp("/tmp/test_bad_preset_key.conf");
        tmp << R"CFG(color_presets=bad
color_preset.bad.width=999
)CFG";
        tmp.close();

        int rc = run_child("bad_preset_noncolor_key", {"--config", "/tmp/test_bad_preset_key.conf"});
        expect("bad_preset_noncolor_key_fails", rc != 0);
        std::remove("/tmp/test_bad_preset_key.conf");
    }

    {
        // Good: inheritance (only override some colors)
        std::ofstream tmp("/tmp/test_preset_inherit.conf");
        tmp << R"CFG(tempo_color=10,20,30,255
color_presets=dim
color_preset=dim
color_preset.dim.name=Dim
color_preset.dim.tempo_color=120,120,120,255
)CFG";
        tmp.close();

        Config c = parse_for_test({"test", "--config", "/tmp/test_preset_inherit.conf"});
        expect("preset_inherit_tempo", c.tempoColor.r == 120 && c.tempoColor.g == 120 && c.tempoColor.b == 120);

        std::remove("/tmp/test_preset_inherit.conf");
    }

    {
        // Negative: duplicate IDs in color_presets
        std::ofstream tmp("/tmp/test_dup_ids.conf");
        tmp << R"CFG(color_presets=default,high_contrast,default
color_preset=default
)CFG";
        tmp.close();
        int rc = run_child("dup_preset_ids", {"--config", "/tmp/test_dup_ids.conf"});
        expect("dup_preset_ids_fails", rc != 0);
        std::remove("/tmp/test_dup_ids.conf");
    }

    {
        // Negative: listed-but-undefined preset (listed in color_presets but no .name and no color override)
        std::ofstream tmp("/tmp/test_listed_undefined.conf");
        tmp << R"CFG(color_presets=default,high_contrast
color_preset=default
color_preset.default.name=Default
)CFG";
        tmp.close();
        int rc = run_child("listed_but_undefined", {"--config", "/tmp/test_listed_undefined.conf"});
        expect("listed_but_undefined_fails", rc != 0);
        std::remove("/tmp/test_listed_undefined.conf");
    }

    {
        // Negative: invalid preset ID with dot
        std::ofstream tmp("/tmp/test_bad_id_dot.conf");
        tmp << R"CFG(color_presets=bad.id
color_preset=bad.id
)CFG";
        tmp.close();
        int rc = run_child("bad_id_with_dot", {"--config", "/tmp/test_bad_id_dot.conf"});
        expect("bad_id_with_dot_fails", rc != 0);
        std::remove("/tmp/test_bad_id_dot.conf");
    }

    {
        // Negative: unknown subkey in preset
        std::ofstream tmp("/tmp/test_bad_subkey.conf");
        tmp << R"CFG(color_presets=foo
color_preset.foo.name=foo
color_preset.foo.width=123
)CFG";
        tmp.close();
        int rc = run_child("bad_subkey", {"--config", "/tmp/test_bad_subkey.conf"});
        expect("bad_subkey_fails", rc != 0);
        std::remove("/tmp/test_bad_subkey.conf");
    }

    {
        // Negative: invalid color value in preset
        std::ofstream tmp("/tmp/test_bad_color.conf");
        tmp << R"CFG(color_presets=foo
color_preset.foo.tempo_color=999,0,0,255
)CFG";
        tmp.close();
        int rc = run_child("bad_color_value", {"--config", "/tmp/test_bad_color.conf"});
        expect("bad_color_value_fails", rc != 0);
        std::remove("/tmp/test_bad_color.conf");
    }

    if (failures == 0)
    {
        std::cout << "All config parser tests passed." << std::endl;
        return 0;
    }
    else
    {
        std::cout << failures << " tests failed." << std::endl;
        return 1;
    }
}
