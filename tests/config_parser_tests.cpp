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

static bool rgbaEquals(const RgbaColor& color, int r, int g, int b, int a)
{
    return color.r == r && color.g == g && color.b == b && color.a == a;
}

static void testScreenPreset(const std::string& testName,
                             const std::string& path,
                             int width,
                             int height,
                             int statusFontSize,
                             int tempoFontSize,
                             int bottomFontSize,
                             int helpFontSize,
                             int phaseBarHeight,
                             int phaseBarGap,
                             int phaseBarMargin,
                             const std::string& initialPreset)
{
    Config c = parse_for_test({"test", "--config", path});

    const auto fileBaseSnapshotIsExact = [&]() {
        return rgbaEquals(c.baseStatusInactiveColor, 100, 44, 48, 255)
            && rgbaEquals(c.baseStatusNoPeersColor, 40, 54, 88, 255)
            && rgbaEquals(c.baseStatusConnectedColor, 105, 105, 95, 255)
            && rgbaEquals(c.baseTempoColor, 84, 89, 166, 255)
            && rgbaEquals(c.basePhaseBarColor, 73, 164, 121, 255)
            && rgbaEquals(c.basePhaseMarkerColor, 125, 205, 25, 255)
            && rgbaEquals(c.baseTopBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.baseCenterBandColor, 18, 18, 18, 255)
            && rgbaEquals(c.baseBottomBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.baseHelpOverlayBackgroundColor, 0, 0, 0, 220)
            && rgbaEquals(c.baseHelpOverlayTextColor, 210, 210, 210, 255);
    };
    const auto fileDefaultColorsAreExact = [&]() {
        return rgbaEquals(c.statusInactiveColor, 100, 44, 48, 255)
            && rgbaEquals(c.statusNoPeersColor, 40, 54, 88, 255)
            && rgbaEquals(c.statusConnectedColor, 105, 105, 95, 255)
            && rgbaEquals(c.tempoColor, 84, 89, 166, 255)
            && rgbaEquals(c.phaseBarColor, 73, 164, 121, 255)
            && rgbaEquals(c.phaseMarkerColor, 125, 205, 25, 255)
            && rgbaEquals(c.topBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.centerBandColor, 18, 18, 18, 255)
            && rgbaEquals(c.bottomBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.helpOverlayBackgroundColor, 0, 0, 0, 220)
            && rgbaEquals(c.helpOverlayTextColor, 210, 210, 210, 255);
    };
    const auto highContrastColorsAreExact = [&]() {
        return rgbaEquals(c.statusInactiveColor, 220, 220, 220, 255)
            && rgbaEquals(c.statusNoPeersColor, 220, 220, 220, 255)
            && rgbaEquals(c.statusConnectedColor, 255, 255, 255, 255)
            && rgbaEquals(c.tempoColor, 255, 255, 255, 255)
            && rgbaEquals(c.phaseBarColor, 255, 255, 255, 255)
            && rgbaEquals(c.phaseMarkerColor, 0, 0, 0, 255)
            && rgbaEquals(c.topBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.centerBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.bottomBandColor, 0, 0, 0, 255)
            && rgbaEquals(c.helpOverlayBackgroundColor, 0, 0, 0, 230)
            && rgbaEquals(c.helpOverlayTextColor, 255, 255, 255, 255);
    };
    const auto expectCompleteProfileState = [&](const std::string& phase) {
        expect(testName + "_" + phase + "_layout",
               c.width == width && c.height == height && c.fullscreen);
        expect(testName + "_" + phase + "_font_sizes",
               c.statusFontSize == statusFontSize
               && c.tempoFontSize == tempoFontSize
               && c.bottomFontSize == bottomFontSize
               && c.helpFontSize == helpFontSize);
        expect(testName + "_" + phase + "_phase_layout",
               c.phaseBarHeight == phaseBarHeight
               && c.phaseBarSegmentGap == phaseBarGap
               && c.phaseBarMargin == phaseBarMargin);
        expect(testName + "_" + phase + "_inherited_runtime_defaults",
               c.helpOverlaySeconds == 8 && c.hideMouseCursor
               && c.initialTempo == 120.0 && c.quantum == 4.0);
        expect(testName + "_" + phase + "_builtin_preset_order",
               c.colorPresetNames.size() == 2
               && c.colorPresetNames[0] == "default"
               && c.colorPresetNames[1] == "high_contrast");
        expect(testName + "_" + phase + "_builtin_labels_and_label_only_default",
               c.colorPresetLabels.count("default") == 1
               && c.colorPresetLabels.at("default") == "Default"
               && c.colorPresetLabels.count("high_contrast") == 1
               && c.colorPresetLabels.at("high_contrast") == "High Contrast"
               && c.colorPresetOverrides.count("default") == 0);
        expect(testName + "_" + phase + "_startup_and_config_paths",
               c.startupConfigPath == path && c.configPath == path);
        expect(testName + "_" + phase + "_active_preset",
               getActiveColorPresetName(c) == initialPreset);
        expect(testName + "_" + phase + "_file_base_snapshot",
               fileBaseSnapshotIsExact());
        if (initialPreset == "default")
            expect(testName + "_" + phase + "_file_default_palette",
                   fileDefaultColorsAreExact());
        else
            expect(testName + "_" + phase + "_high_contrast_palette",
                   highContrastColorsAreExact());
    };

    expectCompleteProfileState("initial");

    const bool reloadOk = tryReloadConfig(c);
    expect(testName + "_unchanged_reload_succeeds", reloadOk);
    expectCompleteProfileState("after_reload");

    cycleColorPreset(c);
    if (initialPreset == "default")
    {
        expect(testName + "_p_high_contrast_palette",
               getActiveColorPresetName(c) == "high_contrast" && highContrastColorsAreExact());
        cycleColorPreset(c);
        expect(testName + "_p_wrap_restored_file_default_palette",
               getActiveColorPresetName(c) == "default" && fileDefaultColorsAreExact());
    }
    else
    {
        expect(testName + "_p_restored_file_default_palette",
               getActiveColorPresetName(c) == "default" && fileDefaultColorsAreExact());
        cycleColorPreset(c);
        expect(testName + "_p_wrap_high_contrast_palette",
               getActiveColorPresetName(c) == "high_contrast" && highContrastColorsAreExact());
    }
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
        expect("no_config_compiled_dim_status_inactive",
               rgbaEquals(c.statusInactiveColor, 40, 44, 48, 255));
        expect("no_config_compiled_dim_status_no_peers",
               rgbaEquals(c.statusNoPeersColor, 40, 44, 48, 255));
        expect("no_config_compiled_dim_status_connected",
               rgbaEquals(c.statusConnectedColor, 75, 85, 95, 255));
        expect("no_config_compiled_dim_tempo",
               rgbaEquals(c.tempoColor, 64, 79, 96, 255));
        expect("no_config_compiled_dim_phase_bar",
               rgbaEquals(c.phaseBarColor, 83, 114, 151, 255));
        expect("no_config_compiled_dim_phase_marker",
               rgbaEquals(c.phaseMarkerColor, 255, 255, 255, 255));
    }

    {
        Config c = parse_for_test({"test", "--config", "config/link-pi-display.example.conf"});
        expect("example_config_width", c.width == 480);
        expect("example_config_hide_mouse", c.hideMouseCursor == true);
        expect("example_config_top_band", c.topBandColor.r == 0 && c.topBandColor.g == 0 && c.topBandColor.b == 0);

        // Sparse example inherits built-in presets and applies only its six-color file palette.
        expect("example_config_parses_presets_order",
               c.colorPresetNames.size() == 2
               && c.colorPresetNames[0] == "default"
               && c.colorPresetNames[1] == "high_contrast");
        expect("example_config_default_active", getActiveColorPresetName(c) == "default");
        expect("example_config_default_label_only",
               c.colorPresetLabels.count("default") == 1
               && c.colorPresetLabels["default"] == "Default"
               && c.colorPresetOverrides.count("default") == 0);
        expect("example_config_file_default_tempo_effective",
               rgbaEquals(c.tempoColor, 84, 89, 166, 255));
        expect("example_config_file_default_status_inactive",
               rgbaEquals(c.statusInactiveColor, 100, 44, 48, 255));
        expect("example_config_file_default_status_no_peers",
               rgbaEquals(c.statusNoPeersColor, 40, 54, 88, 255));
        expect("example_config_file_default_status_connected",
               rgbaEquals(c.statusConnectedColor, 105, 105, 95, 255));
        expect("example_config_file_default_phase_bar",
               rgbaEquals(c.phaseBarColor, 73, 164, 121, 255));
        expect("example_config_file_default_phase_marker",
               rgbaEquals(c.phaseMarkerColor, 125, 205, 25, 255));
        expect("example_config_base_center_band",
               c.centerBandColor.r == 18 && c.centerBandColor.g == 18 && c.centerBandColor.b == 18);

        const int base_tempo_r = c.tempoColor.r;
        const int base_tempo_g = c.tempoColor.g;
        const int base_tempo_b = c.tempoColor.b;
        const int base_center_r = c.centerBandColor.r;
        const int base_marker_r = c.phaseMarkerColor.r;

        cycleColorPreset(c);  // default -> high_contrast
        expect("example_config_high_contrast_active", getActiveColorPresetName(c) == "high_contrast");
        expect("example_config_high_contrast_tempo",
               c.tempoColor.r == 255 && c.tempoColor.g == 255 && c.tempoColor.b == 255);
        expect("example_config_high_contrast_marker",
               c.phaseMarkerColor.r == 0 && c.phaseMarkerColor.g == 0 && c.phaseMarkerColor.b == 0);
        expect("example_config_high_contrast_center",
               c.centerBandColor.r == 0 && c.centerBandColor.g == 0 && c.centerBandColor.b == 0);

        cycleColorPreset(c);  // high_contrast -> default restores base
        expect("example_config_cycle_back_default", getActiveColorPresetName(c) == "default");
        expect("example_config_cycle_restores_base_tempo",
               c.tempoColor.r == base_tempo_r && c.tempoColor.g == base_tempo_g && c.tempoColor.b == base_tempo_b);
        expect("example_config_cycle_restores_base_center",
               c.centerBandColor.r == base_center_r);
        expect("example_config_cycle_restores_base_marker",
               c.phaseMarkerColor.r == base_marker_r && c.phaseMarkerColor.r == 125);
    }

    testScreenPreset("screen_480x320_landscape",
                     "config/presets/480x320-landscape.conf",
                     480, 320, 30, 110, 25, 20, 22, 6, 24, "default");
    testScreenPreset("screen_480x320_high_contrast",
                     "config/presets/480x320-high-contrast.conf",
                     480, 320, 30, 110, 25, 20, 22, 6, 24, "high_contrast");
    testScreenPreset("screen_320x240_landscape",
                     "config/presets/320x240-landscape.conf",
                     320, 240, 22, 80, 18, 16, 16, 4, 16, "default");

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


    // === v0.6 Built-in color presets tests (no config file) ===
    {
        Config c = parse_for_test({"test"});
        expect("builtin_presets_count", c.colorPresetNames.size() == 2);
        expect("builtin_presets_default", c.colorPresetNames[0] == "default");
        expect("builtin_presets_high_contrast", c.colorPresetNames[1] == "high_contrast");
        expect("builtin_default_active", getActiveColorPresetName(c) == "default");
        expect("builtin_default_label", c.colorPresetLabels["default"] == "Default");
        expect("builtin_high_label", c.colorPresetLabels["high_contrast"] == "High Contrast");
        // default is label-only: preserves base colors
        expect("builtin_default_preserves_base_tempo", c.tempoColor.r == 64 && c.tempoColor.g == 79 && c.tempoColor.b == 96);
        // high_contrast has overrides
        expect("builtin_high_has_overrides", c.colorPresetOverrides.count("high_contrast") > 0 && c.colorPresetOverrides["high_contrast"].size() > 0);
    }

    // Test apply high contrast
    {
        Config c = parse_for_test({"test"});
        applyColorPreset(c, "high_contrast");
        expect("builtin_high_overrides_tempo", c.tempoColor.r == 255 && c.tempoColor.g == 255 && c.tempoColor.b == 255);
        expect("builtin_high_overrides_phase_marker", c.phaseMarkerColor.r == 0);
    }

    // Test cycle high_contrast -> default restores base colors (built-in, no config)
    {
        Config c = parse_for_test({"test"});
        int base_tempo_r = c.tempoColor.r;
        int base_phase_r = c.phaseMarkerColor.r;
        cycleColorPreset(c);  // default -> high_contrast
        expect("cycle_high_overrides_tempo", c.tempoColor.r == 255 && c.tempoColor.g == 255 && c.tempoColor.b == 255);
        cycleColorPreset(c);  // high_contrast -> default
        expect("cycle_default_restores_base_tempo", c.tempoColor.r == base_tempo_r && c.tempoColor.r == 64);
        expect("cycle_default_restores_base_phase_marker", c.phaseMarkerColor.r == base_phase_r && c.phaseMarkerColor.r == 255);
    }

    // Test with example config: high -> default restores example base
    {
        Config c = parse_for_test({"test", "--config", "config/link-pi-display.example.conf"});
        int base_tempo_r = c.tempoColor.r;
        cycleColorPreset(c);  // to high_contrast
        cycleColorPreset(c);  // back to default
        expect("example_cycle_default_restores_tempo", c.tempoColor.r == base_tempo_r);
    }

    // Test partial preset inherits from base after a previous preset was active
    {
        std::ofstream tmp("/tmp/test_partial.conf");
        tmp << R"CFG(color_presets=default,high_contrast,partial
color_preset=default
color_preset.default.name=Default
color_preset.high_contrast.name=High Contrast
color_preset.high_contrast.tempo_color=255,255,255,255
color_preset.high_contrast.phase_marker_color=0,0,0,255
color_preset.partial.name=Partial
color_preset.partial.tempo_color=50,50,50,255
)CFG";
        tmp.close();

        Config c = parse_for_test({"test", "--config", "/tmp/test_partial.conf"});

        cycleColorPreset(c);  // default -> high_contrast
        expect("partial_test_high_phase_marker", c.phaseMarkerColor.r == 0);

        cycleColorPreset(c);  // high_contrast -> partial
        expect("partial_after_cycle_tempo_override", c.tempoColor.r == 50);

        // other colors should be base, not previous high_contrast
        expect("partial_inherits_base_phase", c.phaseMarkerColor.r == 255);

        std::remove("/tmp/test_partial.conf");
    }

    // config file with color_presets=custom replaces built-ins
    {
        std::ofstream tmp("/tmp/test_custom_presets.conf");
        tmp << R"CFG(color_presets=custom
color_preset=custom
color_preset.custom.name=Custom
color_preset.custom.tempo_color=100,100,100,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_custom_presets.conf"});
        expect("file_replaces_presets_count", c.colorPresetNames.size() == 1);
        expect("file_replaces_name", c.colorPresetNames[0] == "custom");
        expect("file_replaces_label", c.colorPresetLabels["custom"] == "Custom");
        expect("file_replaces_override", c.colorPresetOverrides.count("custom") > 0);
        std::remove("/tmp/test_custom_presets.conf");
    }

    // config file with no color_presets= keeps built-ins
    {
        std::ofstream tmp("/tmp/test_no_presets_key.conf");
        tmp << "width=640\n";  // some other key, no color_presets
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_no_presets_key.conf"});
        expect("file_omits_presets_keeps_builtin", c.colorPresetNames.size() == 2 && c.colorPresetNames[0] == "default");
        std::remove("/tmp/test_no_presets_key.conf");
    }

    // config file with color_presets= (empty) disables
    {
        std::ofstream tmp("/tmp/test_empty_presets.conf");
        tmp << "color_presets=\n";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_empty_presets.conf"});
        expect("file_empty_presets_disables", c.colorPresetNames.empty());
        std::remove("/tmp/test_empty_presets.conf");
    }


    // v0.6 Ticket 3 reload tests
    {
        // no-config reload keeps built-in presets
        Config c = parse_for_test({"test"});
        expect("reload_no_config_initial", c.colorPresetNames.size() == 2 && c.colorPresetNames[0] == "default");
        c.colorPresetNames.clear();
        bool ok = tryReloadConfig(c);
        expect("reload_no_config_keeps_builtins", ok && c.colorPresetNames.size() == 2);
    }

    {
        // explicit config reload uses same path
        std::ofstream tmp("/tmp/test_explicit_reload.conf");
        tmp << R"CFG(color_presets=default,high_contrast
color_preset=default
color_preset.default.name=Default
color_preset.high_contrast.name=High Contrast
color_preset.high_contrast.tempo_color=255,255,255,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_explicit_reload.conf"});
        expect("explicit_path_set", c.startupConfigPath == "/tmp/test_explicit_reload.conf");
        c.colorPresetNames.clear();
        bool ok = tryReloadConfig(c);
        expect("reload_explicit_uses_same_path", ok && c.configPath == "/tmp/test_explicit_reload.conf");
        std::remove("/tmp/test_explicit_reload.conf");
    }

    {
        // reload preserves each required CLI override
        Config c = parse_for_test({"test", "--tempo", "140"});
        expect("cli_tempo", c.initialTempo == 140.0);
        c.initialTempo = 120.0;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_cli_tempo", ok && c.initialTempo == 140.0);
    }

    {
        // CLI --width wins over config file; reload with conflicting file width keeps live 640
        // (CLI precedence + live window deferral), without fabricating unreachable live state.
        {
            std::ofstream tmp("/tmp/test_reload_width.conf");
            tmp << "width=480\n";
            tmp << "height=320\n";
            tmp.close();
        }
        Config c = parse_for_test({"test", "--config", "/tmp/test_reload_width.conf", "--width", "640"});
        expect("cli_width", c.width == 640);
        {
            std::ofstream next("/tmp/test_reload_width.conf");
            next << "width=800\n";
            next << "height=320\n";
            next.close();
        }
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_width", ok && c.width == 640);
        std::remove("/tmp/test_reload_width.conf");
    }

    {
        // CLI --height wins; reload with conflicting file height keeps live 400.
        {
            std::ofstream tmp("/tmp/test_reload_height.conf");
            tmp << "width=480\n";
            tmp << "height=320\n";
            tmp.close();
        }
        Config c = parse_for_test({"test", "--config", "/tmp/test_reload_height.conf", "--height", "400"});
        expect("cli_height", c.height == 400);
        {
            std::ofstream next("/tmp/test_reload_height.conf");
            next << "width=480\n";
            next << "height=600\n";
            next.close();
        }
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_height", ok && c.height == 400);
        std::remove("/tmp/test_reload_height.conf");
    }

    {
        Config c = parse_for_test({"test", "--font", "/fake/font.ttf"});
        expect("cli_font", c.fontPath == "/fake/font.ttf");
        c.fontPath = "default";
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_font", ok && c.fontPath == "/fake/font.ttf");
    }

    {
        // CLI --windowed wins over file fullscreen=true; reload keeps live windowed/false.
        {
            std::ofstream tmp("/tmp/test_reload_windowed.conf");
            tmp << "fullscreen=true\n";
            tmp << "width=480\n";
            tmp << "height=320\n";
            tmp.close();
        }
        Config c = parse_for_test({"test", "--config", "/tmp/test_reload_windowed.conf", "--windowed"});
        expect("cli_windowed_reload", c.fullscreen == false);
        {
            std::ofstream next("/tmp/test_reload_windowed.conf");
            next << "fullscreen=true\n";
            next << "width=480\n";
            next << "height=320\n";
            next.close();
        }
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_windowed", ok && c.fullscreen == false);
        std::remove("/tmp/test_reload_windowed.conf");
    }

    {
        Config c = parse_for_test({"test", "--no-gui"});
        expect("cli_no_gui_reload", c.noGui == true);
        c.noGui = false;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_no_gui", ok && c.noGui == true);
    }

    {
        Config c = parse_for_test({"test", "--quantum", "8"});
        expect("cli_quantum", c.quantum == 8.0);
        c.quantum = 4.0;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_quantum", ok && c.quantum == 8.0);
    }

    {
        // repeated successful reloads preserve every required original CLI override
        Config c = parse_for_test({
            "test",
            "--width", "640",
            "--height", "400",
            "--windowed",
            "--no-gui",
            "--font", "/fake/font.ttf",
            "--tempo", "133",
            "--quantum", "8"
        });
        expect("cli_all_set",
               c.width == 640 && c.height == 400 && c.fullscreen == false && c.noGui == true
               && c.fontPath == "/fake/font.ttf" && c.initialTempo == 133.0 && c.quantum == 8.0);
        bool ok1 = tryReloadConfig(c);
        bool ok2 = tryReloadConfig(c);
        expect("repeated_reload_preserves_all_cli",
               ok1 && ok2
               && c.width == 640 && c.height == 400 && c.fullscreen == false && c.noGui == true
               && c.fontPath == "/fake/font.ttf" && c.initialTempo == 133.0 && c.quantum == 8.0
               && c.originalCliArgs.size() >= 2);
    }

    {
        // missing explicit reload source: nonfatal, complete previous state remains
        std::ofstream tmp("/tmp/test_valid_for_corrupt.conf");
        tmp << R"CFG(width=480
height=320
tempo_color=10,20,30,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_valid_for_corrupt.conf"});
        const int orig_width = c.width;
        const int orig_tempo_r = c.tempoColor.r;
        const std::string orig_path = c.startupConfigPath;
        const auto orig_cli = c.originalCliArgs;
        const int orig_preset_index = c.activeColorPresetIndex;
        std::remove("/tmp/test_valid_for_corrupt.conf");
        bool ok = tryReloadConfig(c);
        expect("missing_reload_returns_false", !ok);
        expect("missing_reload_keeps_width", c.width == orig_width);
        expect("missing_reload_keeps_tempo_color", c.tempoColor.r == orig_tempo_r);
        expect("missing_reload_keeps_startup_path", c.startupConfigPath == orig_path);
        expect("missing_reload_keeps_cli_args", c.originalCliArgs == orig_cli);
        expect("missing_reload_keeps_preset_index", c.activeColorPresetIndex == orig_preset_index);
        expect("missing_reload_process_continues", true);  // reached after tryReloadConfig
        std::remove("/tmp/test_valid_for_corrupt.conf");
    }

    {
        // existing-but-invalid reload: invalid RGBA must return false in-process (no std::exit)
        std::ofstream tmp("/tmp/test_invalid_existing_reload.conf");
        tmp << R"CFG(width=480
height=320
tempo_color=11,22,33,255
status_font_size=30
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_invalid_existing_reload.conf", "--windowed"});
        const int orig_width = c.width;
        const int orig_tempo_r = c.tempoColor.r;
        const int orig_tempo_g = c.tempoColor.g;
        const int orig_tempo_b = c.tempoColor.b;
        const std::string orig_path = c.startupConfigPath;
        const auto orig_cli = c.originalCliArgs;
        const bool orig_fullscreen = c.fullscreen;
        const int orig_status_font = c.statusFontSize;
        const auto orig_base_tempo = c.baseTempoColor;
        const int orig_active = c.activeColorPresetIndex;

        {
            std::ofstream bad("/tmp/test_invalid_existing_reload.conf");
            bad << "tempo_color=999,0,0,255\n";
            bad << "width=480\n";
            bad.close();
        }

        bool ok = tryReloadConfig(c);
        // Next assertions must execute: proves no process termination.
        expect("invalid_existing_reload_returns_false", !ok);
        expect("invalid_existing_reload_keeps_width", c.width == orig_width);
        expect("invalid_existing_reload_keeps_tempo",
               c.tempoColor.r == orig_tempo_r && c.tempoColor.g == orig_tempo_g && c.tempoColor.b == orig_tempo_b);
        expect("invalid_existing_reload_keeps_path", c.startupConfigPath == orig_path);
        expect("invalid_existing_reload_keeps_cli", c.originalCliArgs == orig_cli);
        expect("invalid_existing_reload_keeps_fullscreen", c.fullscreen == orig_fullscreen);
        expect("invalid_existing_reload_keeps_status_font", c.statusFontSize == orig_status_font);
        expect("invalid_existing_reload_keeps_base_tempo",
               c.baseTempoColor.r == orig_base_tempo.r && c.baseTempoColor.g == orig_base_tempo.g);
        expect("invalid_existing_reload_keeps_active_index", c.activeColorPresetIndex == orig_active);
        expect("invalid_existing_reload_process_continues", true);
        std::remove("/tmp/test_invalid_existing_reload.conf");
    }

    {
        // existing-but-invalid numeric value also nonfatal
        std::ofstream tmp("/tmp/test_invalid_numeric_reload.conf");
        tmp << "status_font_size=30\n";
        tmp << "tempo_color=1,2,3,255\n";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_invalid_numeric_reload.conf"});
        const int orig_font = c.statusFontSize;
        const int orig_r = c.tempoColor.r;
        {
            std::ofstream bad("/tmp/test_invalid_numeric_reload.conf");
            bad << "status_font_size=0\n";
            bad.close();
        }
        bool ok = tryReloadConfig(c);
        expect("invalid_numeric_reload_returns_false", !ok);
        expect("invalid_numeric_reload_keeps_font", c.statusFontSize == orig_font);
        expect("invalid_numeric_reload_keeps_tempo", c.tempoColor.r == orig_r);
        expect("invalid_numeric_reload_process_continues", true);
        std::remove("/tmp/test_invalid_numeric_reload.conf");
    }

    {
        // invalid preset content on reload: duplicate IDs -> nonfatal rollback
        std::ofstream tmp("/tmp/test_invalid_preset_reload.conf");
        tmp << R"CFG(color_presets=default,high_contrast
color_preset=default
color_preset.default.name=Default
color_preset.high_contrast.name=HC
color_preset.high_contrast.tempo_color=9,8,7,255
tempo_color=50,50,50,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_invalid_preset_reload.conf"});
        const int orig_tempo_r = c.tempoColor.r;
        const auto orig_names = c.colorPresetNames;
        const std::string orig_path = c.startupConfigPath;
        const auto orig_cli = c.originalCliArgs;
        {
            std::ofstream bad("/tmp/test_invalid_preset_reload.conf");
            bad << "color_presets=default,default\n";
            bad << "color_preset.default.name=Default\n";
            bad.close();
        }
        bool ok = tryReloadConfig(c);
        expect("invalid_preset_reload_returns_false", !ok);
        expect("invalid_preset_reload_keeps_tempo", c.tempoColor.r == orig_tempo_r);
        expect("invalid_preset_reload_keeps_names", c.colorPresetNames == orig_names);
        expect("invalid_preset_reload_keeps_path", c.startupConfigPath == orig_path);
        expect("invalid_preset_reload_keeps_cli", c.originalCliArgs == orig_cli);
        expect("invalid_preset_reload_process_continues", true);
        std::remove("/tmp/test_invalid_preset_reload.conf");
    }

    {
        // invalid undefined listed preset on reload
        std::ofstream tmp("/tmp/test_undefined_preset_reload.conf");
        tmp << R"CFG(color_presets=default
color_preset=default
color_preset.default.name=Default
tempo_color=12,13,14,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_undefined_preset_reload.conf"});
        const int orig_r = c.tempoColor.r;
        {
            std::ofstream bad("/tmp/test_undefined_preset_reload.conf");
            bad << "color_presets=ghost\n";
            bad.close();
        }
        bool ok = tryReloadConfig(c);
        expect("undefined_preset_reload_returns_false", !ok);
        expect("undefined_preset_reload_keeps_tempo", c.tempoColor.r == orig_r);
        expect("undefined_preset_reload_process_continues", true);
        std::remove("/tmp/test_undefined_preset_reload.conf");
    }

    {
        // live layout: deferred larger candidate width must not allow unusable margin
        std::ofstream tmp("/tmp/test_margin_vs_live_width.conf");
        tmp << R"CFG(width=480
height=320
phase_bar_margin=24
tempo_color=1,2,3,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_margin_vs_live_width.conf"});
        expect("margin_live_initial", c.width == 480 && c.phaseBarMargin == 24 && c.tempoColor.r == 1);
        {
            // Candidate width would make margin valid; live width remains 480 after deferral.
            std::ofstream bad("/tmp/test_margin_vs_live_width.conf");
            bad << "width=2000\n";
            bad << "phase_bar_margin=300\n";
            bad << "tempo_color=99,99,99,255\n";
            bad.close();
        }
        bool ok = tryReloadConfig(c);
        expect("margin_vs_live_width_rejects", !ok);
        expect("margin_vs_live_width_keeps_width", c.width == 480);
        expect("margin_vs_live_width_keeps_margin", c.phaseBarMargin == 24);
        expect("margin_vs_live_width_keeps_tempo", c.tempoColor.r == 1);
        expect("margin_vs_live_width_process_continues", true);
        std::remove("/tmp/test_margin_vs_live_width.conf");
    }

    {
        // window settings deferred: successful reload keeps live width/height/fullscreen
        // while applying safe color change; candidate window values are not stored as applied.
        std::ofstream tmp("/tmp/test_window_defer.conf");
        tmp << R"CFG(width=480
height=320
fullscreen=true
tempo_color=10,20,30,255
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_window_defer.conf", "--windowed"});
        expect("window_defer_cli_windowed", c.fullscreen == false && c.width == 480);
        {
            std::ofstream next("/tmp/test_window_defer.conf");
            next << "width=800\n";
            next << "height=600\n";
            next << "fullscreen=true\n";
            next << "tempo_color=40,50,60,255\n";
            next.close();
        }
        bool ok = tryReloadConfig(c);
        expect("window_defer_reload_ok", ok);
        expect("window_defer_keeps_live_width", c.width == 480);
        expect("window_defer_keeps_live_height", c.height == 320);
        expect("window_defer_keeps_live_fullscreen", c.fullscreen == false);
        expect("window_defer_applies_color", c.tempoColor.r == 40 && c.tempoColor.g == 50 && c.tempoColor.b == 60);
        std::remove("/tmp/test_window_defer.conf");
    }

    {
        // config-defined presets still override after reload
        std::ofstream tmp("/tmp/test_reload_preset.conf");
        tmp << "color_presets=high_contrast\n";
        tmp << "color_preset=high_contrast\n";
        tmp << "color_preset.high_contrast.name=HC\n";
        tmp << "color_preset.high_contrast.tempo_color=1,2,3,255\n";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_reload_preset.conf"});
        expect("preset_override_before", c.tempoColor.r == 1);
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_preset_override", ok && c.tempoColor.r == 1);
        std::remove("/tmp/test_reload_preset.conf");
    }

    {
        // empty color_presets= still disables after reload
        std::ofstream tmp("/tmp/test_reload_empty.conf");
        tmp << R"CFG(color_presets=
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_reload_empty.conf"});
        expect("empty_before", c.colorPresetNames.empty());
        bool ok = tryReloadConfig(c);
        expect("reload_empty_disables", ok && c.colorPresetNames.empty());
        std::remove("/tmp/test_reload_empty.conf");
    }

    {
        // P after R: active name changes to next preset and wraps to first
        Config c = parse_for_test({"test"});
        expect("p_after_r_initial_default", getActiveColorPresetName(c) == "default");
        bool ok = tryReloadConfig(c);
        expect("p_after_r_reload_ok", ok);
        expect("p_after_r_still_default", getActiveColorPresetName(c) == "default");
        const std::string before = getActiveColorPresetName(c);
        cycleColorPreset(c);
        const std::string after_first = getActiveColorPresetName(c);
        expect("p_after_r_changes_to_next", after_first == "high_contrast" && after_first != before);
        cycleColorPreset(c);
        const std::string after_wrap = getActiveColorPresetName(c);
        expect("p_after_r_wraps_to_first", after_wrap == "default");
    }

    {
        // Ticket 2 regression: built-in default is label-only base restore via cycle
        Config c = parse_for_test({"test"});
        const int base_r = c.tempoColor.r;
        cycleColorPreset(c); // high_contrast
        expect("t2_high_contrast_applied", c.tempoColor.r == 255);
        cycleColorPreset(c); // default base restore
        expect("t2_default_restores_base", c.tempoColor.r == base_r);
        bool ok = tryReloadConfig(c);
        expect("t2_reload_ok", ok);
        cycleColorPreset(c);
        expect("t2_p_after_reload_high_contrast", getActiveColorPresetName(c) == "high_contrast" && c.tempoColor.r == 255);
        cycleColorPreset(c);
        expect("t2_p_after_reload_wrap_default", getActiveColorPresetName(c) == "default");
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
