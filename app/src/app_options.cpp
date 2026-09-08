#include "app_options.hpp"

#include "render/diligent/look_preset.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/cli/help.hpp"
#include "engine/cli/response_file.hpp"
#include "world/wind/wind_field.hpp"

namespace app {

namespace {

using engine::cli::bind;
using engine::cli::EnumEntry;
using engine::cli::Option;
using engine::cli::ValueKind;

using Backend = render::diligent::Backend;
using SvoDebugView = render::diligent::SvoDebugView;
using Settings = render::diligent::SvoRenderer::Settings;

constexpr std::array kLooks{
    EnumEntry{"shipping", static_cast<int>(render::diligent::LookPreset::Shipping)},
    EnumEntry{"raw", static_cast<int>(render::diligent::LookPreset::Raw)},
    EnumEntry{"flat", static_cast<int>(render::diligent::LookPreset::Flat)},
    EnumEntry{"hatched", static_cast<int>(render::diligent::LookPreset::Hatched)},
};

constexpr std::array kBackends{
    EnumEntry{"vk", static_cast<int>(Backend::Vulkan)},
    EnumEntry{"vulkan", static_cast<int>(Backend::Vulkan)},
    EnumEntry{"d3d12", static_cast<int>(Backend::D3D12)},
};

constexpr std::array kRenderers{
    EnumEntry{"svo", static_cast<int>(RendererKind::Svo)},
    EnumEntry{"mesh", static_cast<int>(RendererKind::Mesh)},
};

// The 12 --debug-view names, verbatim from the parse chain this table replaced.
constexpr std::array kDebugViews{
    EnumEntry{"lit", static_cast<int>(SvoDebugView::Lit)},
    EnumEntry{"ao", static_cast<int>(SvoDebugView::AO)},
    EnumEntry{"normal", static_cast<int>(SvoDebugView::Normal)},
    EnumEntry{"facenormal", static_cast<int>(SvoDebugView::FaceNormal)},
    EnumEntry{"level", static_cast<int>(SvoDebugView::Level)},
    EnumEntry{"steps", static_cast<int>(SvoDebugView::Steps)},
    EnumEntry{"coverage", static_cast<int>(SvoDebugView::Coverage)},
    EnumEntry{"cubepx", static_cast<int>(SvoDebugView::CubePixels)},
    EnumEntry{"smooth", static_cast<int>(SvoDebugView::SmoothNormal)},
    EnumEntry{"lodcube", static_cast<int>(SvoDebugView::LodCube)},
    EnumEntry{"material", static_cast<int>(SvoDebugView::Material)},
    EnumEntry{"distance", static_cast<int>(SvoDebugView::Distance)},
};

constexpr std::array kTable{
    // ---- what to run -------------------------------------------------------------------------
    Option{.name = "mode",
           .set = bind<&AppOptions::backend>(),
           .kind = ValueKind::Enum,
           .help = "graphics backend",
           .default_text = "vk",
           .group = "Run",
           .enum_values = kBackends},
    Option{.name = "renderer",
           .set = bind<&AppOptions::renderer>(),
           .kind = ValueKind::Enum,
           .help = "world representation and renderer",
           .default_text = "svo",
           .group = "Run",
           .enum_values = kRenderers},
    Option{.name = "frames",
           .set = bind<&AppOptions::frames>(),
           .kind = ValueKind::Int,
           .help = "exit after N frames",
           .default_text = "0 = until the window closes",
           .group = "Run"},
    Option{.name = "seed",
           .set = bind<&AppOptions::seed>(),
           .kind = ValueKind::Int,
           .help = "world seed",
           .default_text = "1337",
           .group = "Run"},
    Option{.name = "radius",
           .set = bind<&AppOptions::radius>(),
           .kind = ValueKind::Int,
           .help = "mesh path only: the static world's horizontal half-size in chunks",
           .default_text = "48",
           .group = "Run"},
    Option{.name = "validation",
           .set = bind<&AppOptions::validation>(),
           .kind = ValueKind::Flag,
           .help = "enable the graphics API's validation layers",
           .group = "Run"},

    // ---- mechanical checks -------------------------------------------------------------------
    Option{.name = "verify-frame",
           .set = bind<&AppOptions::verify_frame>(),
           .kind = ValueKind::Flag,
           .help = "read the frame back and fail on one with no terrain-scale local contrast",
           .group = "Checks",
           .alias = "verify"},
    Option{.name = "autofly",
           .set = bind<&AppOptions::autofly>(),
           .kind = ValueKind::Flag,
           .help = "travel +X automatically once the world is up (bounded-memory / stutter check)",
           .group = "Checks"},
    Option{.name = "dump-every",
           .set = bind<&AppOptions::dump_every>(),
           .kind = ValueKind::Int,
           .help = "write a numbered PNG every N frames (VOXEL_DUMP_FRAME sets the stem)",
           .default_text = "0 = off",
           .group = "Checks"},

    // ---- the body ----------------------------------------------------------------------------
    Option{.name = "dev",
           .set = bind<&AppOptions::dev>(),
           .kind = ValueKind::Flag,
           .help = "unlock the developer tools as a group: --fly, --noclip, the G toggle",
           .group = "Player"},
    Option{.name = "walk",
           .set = bind<&AppOptions::walk>(),
           .kind = ValueKind::Flag,
           .help = "no-op: the body walks by default now (kept, it is in a dozen research logs)",
           .group = "Player"},
    Option{.name = "fly",
           .set = bind<&AppOptions::fly>(),
           .kind = ValueKind::Flag,
           .help = "dev only: the free camera (requires --dev)",
           .group = "Player"},
    Option{.name = "noclip",
           .set = bind<&AppOptions::noclip>(),
           .kind = ValueKind::Flag,
           .help = "dev only: skip body-vs-world collision entirely (requires --dev)",
           .group = "Player"},
    Option{.name = "speed-scale",
           .set = bind<&AppOptions::speed_scale>(),
           .kind = ValueKind::Float,
           .help = "multiply the base move speed (goal 229's clip_stress ramp; 1 = shipped)",
           .default_text = "1",
           .group = "developer"},
    Option{.name = "step-height",
           .set = bind<&AppOptions::step_height>(),
           .kind = ValueKind::Float,
           .help = "step allowance in metres: a smoothing budget on svo, a ledge climb on mesh",
           .default_text = "0.04 svo / 0.55 mesh",
           .group = "Player"},
    Option{.name = "max-walk-slope",
           .set = bind<&AppOptions::max_walk_slope_deg>(),
           .kind = ValueKind::Float,
           .help = "degrees of slope the body will still climb; steeper ground slides (goal 235)",
           .default_text = "40",
           .group = "developer"},
    Option{.name = "aim-range",
           .set = bind<&AppOptions::aim_range>(),
           .kind = ValueKind::Float,
           .help = "metres the crosshair readout will name a material within (goal 242)",
           .default_text = "34 (a 1 cm detail at 20/20)",
           .group = "developer"},
    Option{.name = "auto-exposure",
           .set = bind<&AppOptions::auto_exposure>(),
           .kind = ValueKind::Toggle,
           .help = "crosshair-metered auto-exposure (goal 237)",
           .default_text = "on",
           .group = "Rendering"},
    Option{.name = "exposure-crosshair",
           .set = bind<&AppOptions::exposure_metering_crosshair>(),
           .kind = ValueKind::Toggle,
           .help = "meter over ~6 degrees at the crosshair; off = a flat frame average",
           .default_text = "on",
           .group = "Rendering"},
    Option{.name = "exposure-key",
           .set = bind<&AppOptions::exposure_key>(),
           .kind = ValueKind::Float,
           .help = "middle grey the metered region is mapped to",
           .default_text = "0.36",
           .group = "Rendering"},
    Option{.name = "exposure-pool-deg",
           .set = bind<&AppOptions::exposure_pool_deg>(),
           .kind = ValueKind::Float,
           .help = "half-angle of the metering pool in degrees (research says 3 = a 6 degree pool)",
           .default_text = "3",
           .group = "Rendering"},
    Option{.name = "bloom-follows-exposure",
           .set = bind<&AppOptions::bloom_follows_exposure>(),
           .kind = ValueKind::Toggle,
           .help = "bloom threshold/intensity/radius track the adaptation level (goal 238)",
           .default_text = "on",
           .group = "Rendering"},
    Option{.name = "exposure-pin-ev",
           .set = bind<&AppOptions::exposure_pin_ev>(),
           .kind = ValueKind::Float,
           .help = "pin the adapted level to this EV instead of measuring (goal 238's captures)",
           .default_text = "unset = measure",
           .group = "developer"},
    Option{.name = "vsync",
           .set = bind<&AppOptions::vsync>(),
           .kind = ValueKind::Toggle,
           .help = "wait for the panel on present; OFF is the measuring configuration (goal 244)",
           .default_text = "on",
           .group = "Rendering"},
    Option{.name = "rebuild",
           .set = bind<&AppOptions::rebuild>(),
           .kind = ValueKind::Toggle,
           .help = "world rebuilds as the camera moves; OFF freezes the tree (goal 247's A/B)",
           .default_text = "on",
           .group = "developer"},
    Option{.name = "rebuild-trigger",
           .set = bind<&AppOptions::rebuild_trigger>(),
           .kind = ValueKind::Float,
           .help = "metres the camera may leave the build centre before a rebuild (goal 249)",
           .default_text = "24",
           .group = "developer"},
    Option{.name = "view-polish",
           .set = bind<&AppOptions::view_polish>(),
           .kind = ValueKind::Toggle,
           .help = "head-bob, landing dip and boost FOV kick",
           .default_text = "on",
           .group = "Player"},
    Option{.name = "crosshair",
           .set = bind<&AppOptions::crosshair>(),
           .kind = ValueKind::Toggle,
           .help = "the aim reticle",
           .default_text = "on, but off under --verify-frame",
           .group = "Player"},
    Option{.name = "pos",
           .set = bind<&AppOptions::start_pos>(),
           .kind = ValueKind::Vec3,
           .help = "start position",
           .group = "Player"},
    Option{.name = "yaw",
           .set = bind<&AppOptions::start_yaw_deg>(),
           .kind = ValueKind::Float,
           .help = "start yaw in degrees",
           .group = "Player"},
    Option{.name = "pitch",
           .set = bind<&AppOptions::start_pitch_deg>(),
           .kind = ValueKind::Float,
           .help = "start pitch in degrees",
           .group = "Player"},

    // ---- post chain --------------------------------------------------------------------------
    Option{.name = "post",
           .set = bind<&AppOptions::post>(),
           .kind = ValueKind::Toggle,
           .help = "the whole post chain (off renders straight to the swap chain)",
           .default_text = "on",
           .group = "Post"},
    Option{.name = "bloom",
           .set = bind<&AppOptions::bloom>(),
           .kind = ValueKind::Toggle,
           .help = "bloom (tonemap composite stays on)",
           .default_text = "on",
           .group = "Post"},
    Option{.name = "tonemap",
           .set = bind<&AppOptions::tonemap>(),
           .kind = ValueKind::Toggle,
           .help = "soft-knee tonemap (off = raw clamp)",
           .default_text = "on",
           .group = "Post"},
    Option{.name = "overlay",
           .set = bind<&AppOptions::overlay>(),
           .kind = ValueKind::Toggle,
           .help = "the ImGui debug panel (off for a golden: its digits change every run)",
           .default_text = "on",
           .group = "Post"},
    Option{.name = "gpu-timers",
           .set = bind<&AppOptions::gpu_timers>(),
           .kind = ValueKind::Toggle,
           .help = "per-pass GPU timestamp ranges (off is the A/B that measures their cost)",
           .default_text = "on",
           .group = "Post"},
    Option{.name = "sky",
           .set = bind<&AppOptions::sky>(),
           .kind = ValueKind::Toggle,
           .help = "the gradient sky pass (off = flat clear color)",
           .default_text = "on",
           .group = "Post"},

    // ---- svo world ---------------------------------------------------------------------------
    Option{.name = "voxel-log2",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::voxel_size_log2>(),
           .kind = ValueKind::Int,
           .help = "finest voxel edge = 2^N metres",
           .default_text = "-7 (7.8 mm)",
           .group = "SVO world"},
    Option{.name = "region-log2",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::root_size_log2>(),
           .kind = ValueKind::Int,
           .help = "root edge = 2^N metres",
           .default_text = "9 (512 m)",
           .group = "SVO world",
           .alias = "root-log2"},
    Option{.name = "lod-radius",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::lod_radius>(),
           .kind = ValueKind::Float,
           .help = "full resolution within this many metres",
           .default_text = "4",
           .group = "SVO world"},
    Option{.name = "trees",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::trees>(),
           .kind = ValueKind::Toggle,
           .help = "voxelize trees into the octree",
           .default_text = "on",
           .group = "SVO world"},
    Option{.name = "svo-threads",
           .set = bind<&AppOptions::svo_threads>(),
           .kind = ValueKind::Int,
           .help = "build pool size",
           .default_text = "0 = three quarters of the hardware threads",
           .group = "SVO world"},
    Option{.name = "svo-upload-mb",
           .set = bind<&AppOptions::svo_upload_mb>(),
           .kind = ValueKind::Int,
           .help = "staged GPU upload slice, in MiB per frame",
           .default_text = "32",
           .group = "SVO world"},

    // ---- svo shading -------------------------------------------------------------------------
    Option{.name = "shadows",
           .set = bind<&AppOptions::svo_settings, &Settings::shadows>(),
           .kind = ValueKind::Toggle,
           .help = "traced sun-shadow ray per hit",
           .default_text = "on",
           .group = "SVO shading"},
    Option{.name = "ao",
           .set = bind<&AppOptions::svo_settings, &Settings::ao>(),
           .kind = ValueKind::Toggle,
           .help = "4 short hemisphere rays per hit",
           .default_text = "on",
           .group = "SVO shading"},
    Option{.name = "lod-march",
           .set = bind<&AppOptions::svo_settings, &Settings::lod_march>(),
           .kind = ValueKind::Toggle,
           .help = "Laine-Karras early-out when a node projects under a pixel",
           .default_text = "on",
           .group = "SVO shading"},
    // Goal 289. Applies AT THE POSITION IT APPEARS -- anything after it overrides it, and it
    // overwrites anything before it. See look_preset.hpp for why that ordering rather than
    // per-option provenance tracking in the parser.
    Option{.name = "macro-field",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::macro_field>(),
           .kind = ValueKind::Toggle,
           .help = "generate the world from the baked macro terrain field (Prompt 006)",
           .group = "World"},
    Option{.name = "field-stages",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::field_stages>(),
           .kind = ValueKind::Int,
           .help = "stop the terrain pipeline after N stages (-1 = all)",
           .group = "World"},
    Option{.name = "look",
           .set = [](void* base, std::string_view token, const Option& self) -> engine::cli::Status {
               for (const EnumEntry& entry : self.enum_values) {
                   if (entry.name == token) {
                       render::diligent::apply_look(
                           static_cast<AppOptions*>(base)->svo_settings,
                           static_cast<render::diligent::LookPreset>(entry.value));
                       return engine::cli::Status::Ok;
                   }
               }
               return engine::cli::Status::BadValue;
           },
           .kind = ValueKind::Enum,
           .help = "an appearance preset, applied where it appears so later flags override it",
           .default_text = "shipping",
           .group = "Shading",
           .enum_values = kLooks},
    Option{.name = "stipple",
           .set = bind<&AppOptions::svo_settings, &render::diligent::SvoRenderer::Settings::stipple>(),
           .kind = ValueKind::Toggle,
           .help = "the deliberate directional stipple (goal 285)",
           .group = "Shading"},
    Option{.name = "stipple-amount",
           .set = bind<&AppOptions::svo_settings,
                       &render::diligent::SvoRenderer::Settings::stipple_amount>(),
           .kind = ValueKind::Float,
           .help = "multiplier on every material's own stipple amplitude",
           .group = "Shading"},
    Option{.name = "stipple-period",
           .set = bind<&AppOptions::svo_settings,
                       &render::diligent::SvoRenderer::Settings::stipple_period_px>(),
           .kind = ValueKind::Float,
           .help = "the stipple's SCREEN period in pixels, held constant with distance",
           .group = "Shading"},
    Option{.name = "filter-albedo",
           .set = bind<&AppOptions::svo_settings, &render::diligent::SvoRenderer::Settings::filter_albedo>(),
           .kind = ValueKind::Toggle,
           .help = "band-limit the albedo toward the hit node's average (goal 278)",
           .group = "Shading"},
    Option{.name = "grain",
           .set = bind<&AppOptions::svo_settings, &Settings::grain_amplitude>(),
           .kind = ValueKind::Float,
           .help = "per-cube brightness grain amplitude",
           .default_text = "0.10",
           .group = "SVO shading"},
    // The one genuinely awkward pair in the surface: --grain A sets an AMPLITUDE while --no-grain
    // removes the TERM, so they are two targets under names that look like one option. A Toggle
    // cannot express that (its --no- form has to reach the same member), so --no-grain stays a
    // plain flag and finalize() applies it. Named here rather than quietly renamed: goal 204's
    // rule is that behaviour does not change, and both spellings are documented.
    Option{.name = "no-grain",
           .set = bind<&AppOptions::no_grain>(),
           .kind = ValueKind::Flag,
           .help = "remove the per-cube grain term entirely (--grain 0 keeps it at zero amplitude)",
           .group = "SVO shading"},
    Option{.name = "taa",
           .set = bind<&AppOptions::svo_settings, &Settings::taa>(),
           .kind = ValueKind::Toggle,
           .help = "temporal anti-aliasing resolve",
           .default_text = "on",
           .group = "SVO shading"},
    Option{.name = "lod-quality",
           .set = bind<&AppOptions::svo_settings, &Settings::lod_quality>(),
           .kind = ValueKind::Float,
           .help = "1 = stop at one pixel; <1 finer, >1 coarser",
           .default_text = "1",
           .group = "SVO shading"},
    Option{.name = "mark-cell-usage",
           .set = bind<&AppOptions::svo_settings, &Settings::mark_cell_usage>(),
           .kind = ValueKind::Toggle,
           .help = "goal 261: the marcher stamps every cell it enters and flags absent ones",
           .default_text = "on",
           .group = "SVO shading"},
    Option{.name = "stream-cells",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::stream_cells>(),
           .kind = ValueKind::Toggle,
           .help = "goals 263-265: stream cells into fixed pools instead of uploading a whole tree",
           .default_text = "off",
           .group = "SVO world"},
    Option{.name = "cells-per-frame",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::cells_per_frame>(),
           .kind = ValueKind::Int,
           .help = "goal 264: finished cells handed to the renderer per frame",
           .default_text = "8",
           .group = "SVO world"},
    Option{.name = "brick-slots",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::brick_slots>(),
           .kind = ValueKind::Int,
           .help = "goal 260: fixed brick-pool capacity in slots; never grows",
           .default_text = "1200000",
           .group = "SVO world"},
    Option{.name = "cell-log2",
           .set = bind<&AppOptions::svo, &SvoWorldOptions::cell_size_log2>(),
           .kind = ValueKind::Int,
           .help = "goals 254-256: build the region as a GRID of 2^N m cells; 0 = one tree",
           .default_text = "0",
           .group = "SVO world"},
    Option{.name = "taa-blend",
           .set = bind<&AppOptions::svo_settings, &Settings::taa_blend>(),
           .kind = ValueKind::Float,
           .help = "weight of the new frame in the temporal resolve (1/N = an N-frame history)",
           .default_text = "0.125",
           .group = "SVO shading"},
    Option{.name = "beam-tile",
           .set = bind<&AppOptions::svo_settings, &Settings::beam_tile>(),
           .kind = ValueKind::Int,
           .help = "goal 266 start-t pre-pass: pixels per conservative cone bound; 0 = off (it is "
                   "correct but does not pay -- see research/frame-time-and-gpu-architecture-log.md section 13)",
           .default_text = "0",
           .group = "SVO shading"},
    Option{.name = "smooth-pixels",
           .set = bind<&AppOptions::svo_settings, &Settings::smooth_pixels>(),
           .kind = ValueKind::Float,
           .help = "ancestor span, in pixels, for the averaged normal",
           .default_text = "6",
           .group = "SVO shading"},
    Option{.name = "ao-radius",
           .set = bind<&AppOptions::svo_settings, &Settings::ao_radius_px>(),
           .kind = ValueKind::Float,
           .help = "AO ray length as a screen-space radius",
           .default_text = "32",
           .group = "SVO shading"},
    Option{.name = "shadow-lod",
           .set = bind<&AppOptions::svo_settings, &Settings::shadow_lod>(),
           .kind = ValueKind::Float,
           .help = "how much coarser a shadow ray may judge its LOD",
           .default_text = "4",
           .group = "SVO shading"},
    Option{.name = "debug-view",
           .set = bind<&AppOptions::svo_settings, &Settings::debug_view>(),
           .kind = ValueKind::Enum,
           .help = "render ONE shading term (implies --no-post: raw values, not tone-mapped ones)",
           .group = "SVO shading",
           .enum_values = kDebugViews},

    // ---- wind, upload ------------------------------------------------------------------------
    Option{.name = "wind",
           .set = bind<&AppOptions::wind>(),
           .kind = ValueKind::Toggle,
           .help = "the shared wind field (off zeroes the field itself, not each reader)",
           .default_text = "on",
           .group = "World motion"},
    Option{.name = "wind-speed",
           .set = bind<&AppOptions::svo_settings, &Settings::wind, &world::wind::WindParams::base_speed>(),
           .kind = ValueKind::Float,
           .help = "mean wind speed in m/s",
           .default_text = "6",
           .group = "World motion"},
    Option{.name = "upload-budget",
           .set = bind<&AppOptions::upload_budget>(),
           .kind = ValueKind::Int,
           .help = "mesh path: chunk mesh commits per frame",
           .default_text = "4; 0 = unlimited",
           .group = "World motion"},

#ifndef NDEBUG
    // Debug-only by construction, as it has been since Group J task 20 -- the flag does not exist
    // in a release build, and the #if lives inside the initializer list so there is still exactly
    // one table.
    Option{.name = "crash-test",
           .set = bind<&AppOptions::crash_test>(),
           .kind = ValueKind::String,
           .help = "deliberately exercise a crash-handler hook: av|abort|terminate",
           .group = "Debug"},
#endif
};

static_assert(engine::cli::has_unique_names(kTable));

} // namespace

void finalize(AppOptions& options) noexcept {
    options.svo.seed = options.seed;
    options.svo_settings.sky = options.sky;
    // A debug view renders raw values, so the post chain (which tone-maps them) has to be off.
    // This was `options.no_post = true` inside the --debug-view parse branch; it is a derived
    // rule, not a parse step, and the harness needs the same rule without re-parsing anything.
    if (options.svo_settings.debug_view != render::diligent::SvoDebugView::None) {
        options.post = false;
    }
    if (options.rebuild_trigger) {
        options.svo.rebuild_trigger_metres = *options.rebuild_trigger;
    }
    options.svo.worker_threads = static_cast<std::size_t>(std::max(0, options.svo_threads));
    options.svo_settings.upload_bytes_per_frame =
        static_cast<std::size_t>(std::max(1, options.svo_upload_mb)) * std::size_t{1024} * 1024;
    if (!options.wind) {
        options.svo_settings.wind = world::wind::still_wind();
    }
    if (options.no_grain) {
        options.svo_settings.grain = false;
    }
}

std::span<const engine::cli::Option> option_table() noexcept {
    return kTable;
}

engine::cli::ParseOutcome parse_app_options(int argc, char** argv, AppOptions& out) {
    engine::cli::ParseOutcome outcome = engine::cli::parse_command_line(kTable, &out, argc, argv);
    if (outcome.ok) {
        finalize(out);
    }
    return outcome;
}

std::optional<render::diligent::SvoRenderer::Settings> settings_from_response_file(const std::string& path) {
    const engine::cli::ExpandResult expanded =
        engine::cli::expand_response_files(std::vector<std::string>{"@" + path});
    if (!expanded.ok) {
        return std::nullopt;
    }
    AppOptions options;
    if (!engine::cli::parse(kTable, &options, expanded.args).ok) {
        return std::nullopt;
    }
    finalize(options);
    return options.svo_settings;
}

std::string app_help_text() {
    return engine::cli::render_help(
        "voxel_app",
        "The micro-voxel engine. Defaults to the sparse-brick octree path on Vulkan; the greedy-\n"
        "meshed 1 m chunk world is --renderer mesh.",
        kTable);
}

} // namespace app
