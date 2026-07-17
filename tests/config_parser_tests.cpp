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
        // simulate change
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
        // reload preserves CLI overrides
        Config c = parse_for_test({"test", "--tempo", "140"});
        expect("cli_tempo", c.initialTempo == 140.0);
        c.initialTempo = 120.0;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_cli_tempo", ok && c.initialTempo == 140.0);
    }

    {
        // reload preserves --width
        Config c = parse_for_test({"test", "--width", "640"});
        expect("cli_width", c.width == 640);
        c.width = 480;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_width", ok && c.width == 640);
    }

    {
        // reload preserves --height
        Config c = parse_for_test({"test", "--height", "400"});
        expect("cli_height", c.height == 400);
        c.height = 320;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_height", ok && c.height == 400);
    }

    {
        // reload preserves --font
        Config c = parse_for_test({"test", "--font", "/fake/font.ttf"});
        expect("cli_font", c.fontPath == "/fake/font.ttf");
        c.fontPath = "default";
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_font", ok && c.fontPath == "/fake/font.ttf");
    }

    {
        // reload preserves --windowed
        Config c = parse_for_test({"test", "--windowed"});
        expect("cli_windowed", c.fullscreen == false);
        c.fullscreen = true;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_windowed", ok && c.fullscreen == false);
    }

    {
        // reload preserves --no-gui
        Config c = parse_for_test({"test", "--no-gui"});
        expect("cli_no_gui", c.noGui == true);
        c.noGui = false;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_no_gui", ok && c.noGui == true);
    }

    {
        // reload preserves --quantum
        Config c = parse_for_test({"test", "--quantum", "8"});
        expect("cli_quantum", c.quantum == 8.0);
        c.quantum = 4.0;
        bool ok = tryReloadConfig(c);
        expect("reload_preserves_quantum", ok && c.quantum == 8.0);
    }

    {
        // invalid reload keeps current (real explicit-config failure test)
        std::ofstream tmp("/tmp/test_valid_for_corrupt.conf");
        tmp << R"CFG(width=480
height=320
)CFG";
        tmp.close();
        Config c = parse_for_test({"test", "--config", "/tmp/test_valid_for_corrupt.conf"});
        int orig_width = c.width;
        std::string orig_path = c.startupConfigPath;
        std::remove("/tmp/test_valid_for_corrupt.conf");
        bool ok = tryReloadConfig(c);
        expect("invalid_reload_keeps_current", !ok && c.width == orig_width && c.startupConfigPath == orig_path);
        std::remove("/tmp/test_valid_for_corrupt.conf");  // ensure gone
    }



    {
        // config-defined presets still override after reload
        std::ofstream tmp("/tmp/test_reload_preset.conf");
        tmp << R"CFG(color_presets=default,high_contrast
color_preset=high_contrast
color_preset.default.name=Default
color_preset.high_contrast.name=HC
color_preset.high_contrast.tempo_color=1,2,3,255
)CFG";
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
        // P still cycles after reload
        Config c = parse_for_test({"test"});
        cycleColorPreset(c);
        std::string after_p = getActiveColorPresetName(c);
        bool ok = tryReloadConfig(c);
        cycleColorPreset(c);
        expect("p_after_reload", ok);
    }

    {
        // module info includes R
        // tested via print but in parser we can call
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
