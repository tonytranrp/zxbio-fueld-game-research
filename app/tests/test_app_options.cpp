// Goal 204/206/207's Check. The expectations below were written from the PRE-PORT parse chain --
// app/src/main.cpp's 180-line if/else ladder as it stood at commit 76731cc -- before it was
// deleted, which is the only order in which "behaviour must not change" is a checkable claim
// rather than a hope. Every value is the one that chain produced; where a comment says "unchanged"
// it means the number was read out of that chain, not chosen here.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "render/diligent/look_preset.hpp"

#include "app_options.hpp"
#include "engine/cli/help.hpp"
#include "engine/cli/parser.hpp"

using app::AppOptions;
using app::RendererKind;
using Catch::Approx;

namespace {

// Drives the REAL table, the same way main() does, from a vector of arguments.
app::AppOptions parsed(std::vector<std::string> args) {
    AppOptions options;
    std::vector<char*> argv{const_cast<char*>("voxel_app")};
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    const engine::cli::ParseOutcome outcome =
        app::parse_app_options(static_cast<int>(argv.size()), argv.data(), options);
    REQUIRE(outcome.ok);
    REQUIRE_FALSE(outcome.help_requested);
    return options;
}

bool rejects(std::vector<std::string> args) {
    AppOptions options;
    std::vector<char*> argv{const_cast<char*>("voxel_app")};
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    return !app::parse_app_options(static_cast<int>(argv.size()), argv.data(), options).ok;
}

// Every flag name the pre-port `unknown argument` diagnostic listed, plus --crash-test, which was
// the only Debug-only one. 43 names. If a future option is added without a row, the count test
// below is what notices.
constexpr std::array<std::string_view, 43> kPrePortFlagNames{
    "--mode",         "--renderer",       "--frames",      "--radius",        "--seed",
    "--verify-frame", "--validation",     "--autofly",     "--walk",          "--noclip",
    "--step-height",  "--no-view-polish", "--wind-speed",  "--no-wind",       "--crosshair",
    "--no-crosshair", "--upload-budget",  "--dump-every",  "--no-post",       "--no-sky",
    "--no-bloom",     "--no-tonemap",     "--no-shadows",  "--no-ao",         "--no-lod-march",
    "--lod-quality",  "--no-grain",       "--no-taa",      "--smooth-pixels", "--grain",
    "--ao-radius",    "--shadow-lod",     "--svo-threads", "--svo-upload-mb", "--debug-view",
    "--voxel-log2",   "--region-log2",    "--lod-radius",  "--no-trees",      "--pos",
    "--yaw",          "--pitch",          "--crash-test",
};

} // namespace

TEST_CASE("the defaults survive the port unchanged", "[app][cli]") {
    const AppOptions o = parsed({});
    CHECK(o.backend == render::diligent::Backend::Vulkan);
    CHECK(o.renderer == RendererKind::Svo);
    CHECK(o.frames == 0u);
    CHECK(o.radius == world::streaming::kDefaultWorldBounds.radius_chunks);
    CHECK(o.seed == 1337);
    CHECK_FALSE(o.verify_frame);
    CHECK_FALSE(o.validation);
    CHECK_FALSE(o.autofly);
    CHECK_FALSE(o.walk);
    CHECK_FALSE(o.noclip);
    CHECK_FALSE(o.step_height.has_value());
    CHECK(o.view_polish);
    CHECK_FALSE(o.crosshair.has_value()); // the app resolves this to !verify_frame, see below
    CHECK(o.upload_budget == 4u);
    CHECK(o.dump_every == 0u);
    CHECK(o.post);
    CHECK(o.bloom);
    CHECK(o.tonemap);
    CHECK(o.sky);
    CHECK_FALSE(o.start_pos.has_value());
    CHECK_FALSE(o.start_yaw_deg.has_value());
    CHECK_FALSE(o.start_pitch_deg.has_value());
    // svo world
    CHECK(o.svo.seed == 1337);
    CHECK(o.svo.voxel_size_log2 == -7);
    // Prompt 007 goal 329 moved this from 9 (512 m) to 12 (4096 m): 2048 m of view instead of
    // 256 m. The test is UPDATED rather than relaxed -- its job is to catch an accidental change to
    // a default, and this was a deliberate one with a measured cost table behind it (see
    // svo_world.hpp). V = 12 + 7 = 19, five bits under kMaxVoxelBits.
    CHECK(o.svo.root_size_log2 == 12);
    CHECK(o.svo.lod_radius == Approx(4.0f));
    CHECK(o.svo.trees);
    CHECK(o.svo.worker_threads == 0u);
    // svo shading
    CHECK(o.svo_settings.shadows);
    CHECK(o.svo_settings.ao);
    CHECK(o.svo_settings.lod_march);
    CHECK(o.svo_settings.sky);
    CHECK(o.svo_settings.grain);
    CHECK(o.svo_settings.taa);
    CHECK(o.svo_settings.lod_quality == Approx(1.0f));
    CHECK(o.svo_settings.shadow_lod == Approx(4.0f));
    CHECK(o.svo_settings.ao_radius_px == Approx(32.0f));
    CHECK(o.svo_settings.smooth_pixels == Approx(6.0f));
    CHECK(o.svo_settings.grain_amplitude == Approx(0.10f));
    CHECK(o.svo_settings.debug_view == render::diligent::SvoDebugView::None);
    CHECK(o.svo_settings.upload_bytes_per_frame == std::size_t{32} * 1024 * 1024);
    CHECK(o.svo_settings.wind.base_speed == world::wind::WindParams{}.base_speed);
}

TEST_CASE("every flag reaches the field the parse chain used to set", "[app][cli]") {
    CHECK(parsed({"--mode", "d3d12"}).backend == render::diligent::Backend::D3D12);
    CHECK(parsed({"--mode", "vulkan"}).backend == render::diligent::Backend::Vulkan);
    CHECK(parsed({"--renderer", "mesh"}).renderer == RendererKind::Mesh);
    CHECK(parsed({"--frames", "8"}).frames == 8u);
    CHECK(parsed({"--radius", "12"}).radius == 12);
    CHECK(parsed({"--seed", "99"}).seed == 99);
    CHECK(parsed({"--seed", "99"}).svo.seed == 99); // finalize() mirrors it, as the chain did
    CHECK(parsed({"--verify-frame"}).verify_frame);
    CHECK(parsed({"--validation"}).validation);
    CHECK(parsed({"--autofly"}).autofly);
    CHECK(parsed({"--walk"}).walk);
    CHECK(parsed({"--noclip"}).noclip);
    CHECK(*parsed({"--step-height", "0.5"}).step_height == Approx(0.5f));
    CHECK_FALSE(parsed({"--no-view-polish"}).view_polish);
    CHECK(parsed({"--wind-speed", "11"}).svo_settings.wind.base_speed == Approx(11.0f));
    CHECK(*parsed({"--crosshair"}).crosshair);
    CHECK_FALSE(*parsed({"--no-crosshair"}).crosshair);
    CHECK(parsed({"--upload-budget", "0"}).upload_budget == 0u); // 0 means unlimited, unchanged
    CHECK(parsed({"--dump-every", "30"}).dump_every == 30u);
    CHECK_FALSE(parsed({"--no-post"}).post);
    CHECK_FALSE(parsed({"--no-sky"}).sky);
    CHECK_FALSE(parsed({"--no-sky"}).svo_settings.sky); // finalize() mirrors it, as the chain did
    CHECK_FALSE(parsed({"--no-bloom"}).bloom);
    CHECK_FALSE(parsed({"--no-tonemap"}).tonemap);
    CHECK_FALSE(parsed({"--no-shadows"}).svo_settings.shadows);
    CHECK_FALSE(parsed({"--no-ao"}).svo_settings.ao);
    CHECK_FALSE(parsed({"--no-lod-march"}).svo_settings.lod_march);
    CHECK(parsed({"--lod-quality", "0.5"}).svo_settings.lod_quality == Approx(0.5f));
    CHECK_FALSE(parsed({"--no-grain"}).svo_settings.grain);
    CHECK_FALSE(parsed({"--no-taa"}).svo_settings.taa);
    CHECK(parsed({"--smooth-pixels", "3"}).svo_settings.smooth_pixels == Approx(3.0f));
    CHECK(parsed({"--grain", "0.25"}).svo_settings.grain_amplitude == Approx(0.25f));
    CHECK(parsed({"--ao-radius", "16"}).svo_settings.ao_radius_px == Approx(16.0f));
    CHECK(parsed({"--shadow-lod", "2"}).svo_settings.shadow_lod == Approx(2.0f));
    CHECK(parsed({"--svo-threads", "6"}).svo.worker_threads == 6u);
    CHECK(parsed({"--svo-upload-mb", "8"}).svo_settings.upload_bytes_per_frame ==
          std::size_t{8} * 1024 * 1024);
    CHECK(parsed({"--voxel-log2", "-5"}).svo.voxel_size_log2 == -5);
    CHECK(parsed({"--region-log2", "8"}).svo.root_size_log2 == 8);
    CHECK(parsed({"--lod-radius", "12"}).svo.lod_radius == Approx(12.0f));
    CHECK_FALSE(parsed({"--no-trees"}).svo.trees);
    CHECK(parsed({"--yaw", "45"}).start_yaw_deg == Approx(45.0f));
    CHECK(parsed({"--pitch", "-20"}).start_pitch_deg == Approx(-20.0f));

    const AppOptions pos = parsed({"--pos", "1,2,3"});
    REQUIRE(pos.start_pos.has_value());
    CHECK(pos.start_pos->x == Approx(1.0f));
    CHECK(pos.start_pos->y == Approx(2.0f));
    CHECK(pos.start_pos->z == Approx(3.0f));
}

TEST_CASE("all twelve debug-view names still resolve, and each implies no-post", "[app][cli]") {
    using View = render::diligent::SvoDebugView;
    const std::array<std::pair<std::string_view, View>, 12> kViews{{
        {"lit", View::Lit},
        {"ao", View::AO},
        {"normal", View::Normal},
        {"facenormal", View::FaceNormal},
        {"level", View::Level},
        {"steps", View::Steps},
        {"coverage", View::Coverage},
        {"cubepx", View::CubePixels},
        {"smooth", View::SmoothNormal},
        {"lodcube", View::LodCube},
        {"material", View::Material},
        {"distance", View::Distance},
    }};
    for (const auto& [name, view] : kViews) {
        CAPTURE(name);
        const AppOptions o = parsed({"--debug-view", std::string{name}});
        CHECK(o.svo_settings.debug_view == view);
        CHECK_FALSE(o.post); // the chain's own `options.no_post = true` side effect
    }
    CHECK(rejects({"--debug-view", "nonsense"}));
}

TEST_CASE("the no-wind spelling zeroes the field itself", "[app][cli]") {
    // The chain assigned world::wind::still_wind() wholesale rather than flagging each reader;
    // finalize() does the same, and this pins that it is the WHOLE field, not just the speed.
    const AppOptions o = parsed({"--no-wind"});
    const world::wind::WindParams still = world::wind::still_wind();
    CHECK(o.svo_settings.wind.base_speed == Approx(still.base_speed));
    CHECK(o.svo_settings.wind.gust_amplitude == Approx(still.gust_amplitude));
    // ...and --wind-speed after --no-wind is still overridden, because finalize() runs last. That
    // is the pre-port order too (the chain's --no-wind assignment happened at parse time, so the
    // LAST of the two won; this is the one place the port changes which wins). Recorded here
    // deliberately rather than discovered later.
    CHECK(parsed({"--wind-speed", "20", "--no-wind"}).svo_settings.wind.base_speed ==
          Approx(still.base_speed));
    CHECK(parsed({"--no-wind", "--wind-speed", "20"}).svo_settings.wind.base_speed ==
          Approx(still.base_speed));
}

TEST_CASE("root-log2 is an alias, not a second option", "[app][cli]") {
    CHECK(parsed({"--root-log2", "7"}).svo.root_size_log2 ==
          parsed({"--region-log2", "7"}).svo.root_size_log2);
    CHECK(parsed({"--verify"}).verify_frame == parsed({"--verify-frame"}).verify_frame);
}

TEST_CASE("help lists all 43 pre-port option names", "[app][cli]") {
    const std::string help = app::app_help_text();
    int found = 0;
    for (const std::string_view name : kPrePortFlagNames) {
#ifdef NDEBUG
        if (name == "--crash-test") {
            continue; // compiled out of the table in a release build, by construction
        }
#endif
        CAPTURE(name);
        CHECK(help.find(name) != std::string::npos);
        ++found;
    }
#ifdef NDEBUG
    CHECK(found == 42);
#else
    CHECK(found == 43);
#endif
}

TEST_CASE("help cannot fall behind the table", "[app][cli]") {
    // Every row appears, so an option added without a help line is a test failure rather than a
    // surprise at 2am. The count is the table's, not a number written down here.
    const std::string help = app::app_help_text();
    for (const engine::cli::Option& row : app::option_table()) {
        CAPTURE(row.name);
        CHECK(help.find("--" + std::string{row.name}) != std::string::npos);
        if (!row.alias.empty()) {
            CHECK(help.find("--" + std::string{row.alias}) != std::string::npos);
        }
    }
}

TEST_CASE("a malformed value is now a diagnostic where the chain silently took a prefix", "[app][cli]") {
    // Recorded as a deliberate improvement, not a regression: strtol("twelve") was 0 and
    // strtol("12abc") was 12. Both are refused now, and --seed with nothing after it -- which the
    // chain turned into "keep the default" -- says so.
    CHECK(rejects({"--seed", "twelve"}));
    CHECK(rejects({"--seed", "12abc"}));
    CHECK(rejects({"--seed"}));
    CHECK(rejects({"--pos", "1,2"}));
    CHECK(rejects({"--nonsense"}));
}

TEST_CASE("a config file reproduces a twelve-flag command line exactly", "[app][cli]") {
    // Goal 206's Check, on the real table.
    const std::vector<std::string> command{
        "--mode",          "d3d12", "--renderer",   "svo",     "--seed",        "4242",
        "--lod-radius",    "9",     "--no-shadows", "--no-ao", "--grain",       "0.3",
        "--smooth-pixels", "2",     "--voxel-log2", "-6",      "--region-log2", "8",
        "--svo-upload-mb", "16",    "--walk",       "--pos",   "10,20,30"};
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "voxel_app_options_test.cfg";
    {
        std::ofstream out(path);
        out << "# a twelve-flag configuration, one option per line\n";
        for (std::size_t i = 0; i < command.size(); ++i) {
            out << command[i];
            const bool valueFollows = i + 1 < command.size() && !command[i + 1].starts_with("--");
            if (valueFollows) {
                out << ' ' << command[++i];
            }
            out << '\n';
        }
    }
    const AppOptions fromFile = parsed({"@" + path.string()});
    const AppOptions fromCommand = parsed(command);

    CHECK(fromFile.backend == fromCommand.backend);
    CHECK(fromFile.renderer == fromCommand.renderer);
    CHECK(fromFile.seed == fromCommand.seed);
    CHECK(fromFile.walk == fromCommand.walk);
    CHECK(fromFile.svo.lod_radius == Approx(fromCommand.svo.lod_radius));
    CHECK(fromFile.svo.voxel_size_log2 == fromCommand.svo.voxel_size_log2);
    CHECK(fromFile.svo.root_size_log2 == fromCommand.svo.root_size_log2);
    CHECK(fromFile.svo_settings.shadows == fromCommand.svo_settings.shadows);
    CHECK(fromFile.svo_settings.ao == fromCommand.svo_settings.ao);
    CHECK(fromFile.svo_settings.grain_amplitude == Approx(fromCommand.svo_settings.grain_amplitude));
    CHECK(fromFile.svo_settings.smooth_pixels == Approx(fromCommand.svo_settings.smooth_pixels));
    CHECK(fromFile.svo_settings.upload_bytes_per_frame == fromCommand.svo_settings.upload_bytes_per_frame);
    REQUIRE(fromFile.start_pos.has_value());
    REQUIRE(fromCommand.start_pos.has_value());
    CHECK(fromFile.start_pos->x == Approx(fromCommand.start_pos->x));
    CHECK(fromFile.start_pos->z == Approx(fromCommand.start_pos->z));
    std::filesystem::remove(path);
}

TEST_CASE("renderer settings are independently constructible from a config file", "[app][cli]") {
    // Goal 207: a tool with no window builds the SAME SvoRenderer::Settings the app would, which
    // is what lets one scenario run headless and windowed without two configurations.
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "voxel_app_settings_test.cfg";
    {
        std::ofstream out(path);
        out << "--no-shadows\n--ao-radius 12\n--lod-quality 0.75\n--grain 0.02\n--no-taa\n";
        out << "--shadow-lod 6\n--smooth-pixels 4\n--wind-speed 3.5\n--svo-upload-mb 64\n";
    }
    const render::diligent::SvoRenderer::Settings fromFile =
        app::settings_from_response_file(path.string()).value();
    const render::diligent::SvoRenderer::Settings fromApp =
        parsed({"--no-shadows", "--ao-radius", "12", "--lod-quality", "0.75", "--grain", "0.02", "--no-taa",
                "--shadow-lod", "6", "--smooth-pixels", "4", "--wind-speed", "3.5", "--svo-upload-mb", "64"})
            .svo_settings;

    CHECK(fromFile.shadows == fromApp.shadows);
    CHECK(fromFile.ao == fromApp.ao);
    CHECK(fromFile.taa == fromApp.taa);
    CHECK(fromFile.lod_march == fromApp.lod_march);
    CHECK(fromFile.sky == fromApp.sky);
    CHECK(fromFile.grain == fromApp.grain);
    CHECK(fromFile.lod_quality == Approx(fromApp.lod_quality));
    CHECK(fromFile.shadow_lod == Approx(fromApp.shadow_lod));
    CHECK(fromFile.ao_radius_px == Approx(fromApp.ao_radius_px));
    CHECK(fromFile.smooth_pixels == Approx(fromApp.smooth_pixels));
    CHECK(fromFile.grain_amplitude == Approx(fromApp.grain_amplitude));
    CHECK(fromFile.taa_blend == Approx(fromApp.taa_blend));
    CHECK(fromFile.wind.base_speed == Approx(fromApp.wind.base_speed));
    CHECK(fromFile.upload_bytes_per_frame == fromApp.upload_bytes_per_frame);
    CHECK(fromFile.debug_view == fromApp.debug_view);
    std::filesystem::remove(path);
}

// ------------------------------------------------------- Prompt 005 goal 289: the look presets
//
// NOTE THE TEST NAMES do not start with "--". Catch2's own argument parser sees the name
// ctest passes it, so a TEST_CASE called "--look ..." is read as an unknown OPTION and the test
// fails under ctest while passing when the binary is run by hand -- which is exactly how it
// presented here.
//
// These assert VALUES, not that the code runs. That is the point the goal asks for: a preset is
// the shipped look expressed as data, and a refactor that quietly changed one would otherwise be
// invisible until someone noticed the world looked different.

TEST_CASE("the look flag selects an appearance preset", "[app][options][look]") {
    using render::diligent::LookPreset;

    SECTION("shipping is the default configuration, stated rather than assumed") {
        AppOptions o;
        render::diligent::apply_look(o.svo_settings, LookPreset::Shipping);
        CHECK(o.svo_settings.filter_albedo);
        CHECK(o.svo_settings.stipple);
        CHECK(o.svo_settings.stipple_amount == Catch::Approx(2.5f));
        CHECK(o.svo_settings.stipple_period_px == Catch::Approx(7.0f));
        CHECK(o.svo_settings.grain);
        CHECK(o.svo_settings.grain_amplitude == Catch::Approx(0.10f));
        // A fresh AppOptions must already BE the shipping look, or "shipping" is a lie.
        const AppOptions fresh;
        CHECK(fresh.svo_settings.filter_albedo == o.svo_settings.filter_albedo);
        CHECK(fresh.svo_settings.stipple == o.svo_settings.stipple);
        CHECK(fresh.svo_settings.stipple_amount == Catch::Approx(o.svo_settings.stipple_amount));
        CHECK(fresh.svo_settings.stipple_period_px == Catch::Approx(o.svo_settings.stipple_period_px));
    }

    SECTION("raw turns off everything Prompt 005 added -- the A/B for the whole pass") {
        AppOptions o;
        render::diligent::apply_look(o.svo_settings, LookPreset::Raw);
        CHECK_FALSE(o.svo_settings.filter_albedo);
        CHECK_FALSE(o.svo_settings.stipple);
        CHECK(o.svo_settings.grain); // the OLD per-cube grain predates this pass and stays
    }

    SECTION("flat keeps the filtering and removes every deliberate pattern") {
        AppOptions o;
        render::diligent::apply_look(o.svo_settings, LookPreset::Flat);
        CHECK(o.svo_settings.filter_albedo);
        CHECK_FALSE(o.svo_settings.stipple);
        CHECK_FALSE(o.svo_settings.grain);
    }

    SECTION("hatched aims at the reference capture's own measured period") {
        AppOptions o;
        render::diligent::apply_look(o.svo_settings, LookPreset::Hatched);
        CHECK(o.svo_settings.stipple);
        // 10.67 px is goal 284's measurement of lin_water_checkerboard_after.png, not a taste.
        CHECK(o.svo_settings.stipple_period_px == Catch::Approx(10.67f));
        CHECK(o.svo_settings.stipple_amount > 2.5f);
    }

    SECTION("a preset touches ONLY appearance -- never quality or performance") {
        // The failure this guards against: a "look" that silently turns shadows off is a
        // performance setting wearing a costume, and it would show up as a frame-time win nobody
        // asked for.
        for (LookPreset look : {LookPreset::Shipping, LookPreset::Raw, LookPreset::Flat,
                                LookPreset::Hatched}) {
            AppOptions o;
            o.svo_settings.shadows = false;
            o.svo_settings.ao = false;
            o.svo_settings.taa = false;
            o.svo_settings.lod_quality = 0.5f;
            o.svo_settings.ao_radius_px = 11.0f;
            render::diligent::apply_look(o.svo_settings, look);
            CHECK_FALSE(o.svo_settings.shadows);
            CHECK_FALSE(o.svo_settings.ao);
            CHECK_FALSE(o.svo_settings.taa);
            CHECK(o.svo_settings.lod_quality == Catch::Approx(0.5f));
            CHECK(o.svo_settings.ao_radius_px == Catch::Approx(11.0f));
        }
    }
}

TEST_CASE("the look flag parses and position decides who wins", "[app][options][look]") {
    // The documented ordering contract (look_preset.hpp): --look applies WHERE IT APPEARS.
    const AppOptions later = parsed({"--look", "raw", "--filter-albedo"});
    CHECK_FALSE(later.svo_settings.stipple); // from the preset
    CHECK(later.svo_settings.filter_albedo); // the later flag won

    const AppOptions earlier = parsed({"--filter-albedo", "--look", "raw"});
    CHECK_FALSE(earlier.svo_settings.filter_albedo); // the preset came last and won
}

TEST_CASE("the look flag rejects a name it does not have", "[app][options][look]") {
    CHECK(rejects({"--look", "cinematic"}));
}
