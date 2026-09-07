#include "render_options.hpp"

#include <array>

#include "engine/cli/help.hpp"

namespace tools::svo_render {

namespace {

using engine::cli::bind;
using engine::cli::Option;
using engine::cli::ValueKind;

constexpr std::array kTable{
    Option{.name = "seed",
           .set = bind<&Options::seed>(),
           .kind = ValueKind::Int,
           .help = "world seed",
           .default_text = "1337",
           .group = "World"},
    Option{.name = "voxel-log2",
           .set = bind<&Options::voxel_log2>(),
           .kind = ValueKind::Int,
           .help = "finest voxel edge = 2^N metres",
           .default_text = "-7 (7.8 mm)",
           .group = "World"},
    // The drift ends here: the app's spelling is the name, the tool's old spelling is the alias.
    // Every checked-in command line using --root-log2 keeps working, and the two programs can now
    // be handed the same response file.
    Option{.name = "region-log2",
           .set = bind<&Options::root_log2>(),
           .kind = ValueKind::Int,
           .help = "root edge = 2^N metres",
           .default_text = "9 (512 m)",
           .group = "World",
           .alias = "root-log2"},
    Option{.name = "lod-radius",
           .set = bind<&Options::lod_radius>(),
           .kind = ValueKind::Float,
           .help = "full resolution within this many metres",
           .default_text = "4",
           .group = "World"},
    Option{.name = "trees",
           .set = bind<&Options::trees>(),
           .kind = ValueKind::Toggle,
           .help = "voxelize trees into the octree",
           .default_text = "on",
           .group = "World"},
    Option{.name = "lod-center",
           .set = bind<&Options::lod_center>(),
           .kind = ValueKind::Vec3,
           .help = "build the LOD around this point instead of the eye (goal 164's repro)",
           .group = "World"},

    Option{.name = "pos",
           .set = bind<&Options::pos>(),
           .kind = ValueKind::Vec3,
           .help = "camera position",
           .default_text = "12,82,24",
           .group = "Camera"},
    Option{.name = "xz",
           .set = bind<&Options::xz>(),
           .kind = ValueKind::Vec2,
           .help = "camera x and z; eye height derived from the terrain",
           .group = "Camera"},
    Option{.name = "yaw",
           .set = bind<&Options::yaw_deg>(),
           .kind = ValueKind::Float,
           .help = "yaw in degrees",
           .default_text = "0",
           .group = "Camera"},
    Option{.name = "pitch",
           .set = bind<&Options::pitch_deg>(),
           .kind = ValueKind::Float,
           .help = "pitch in degrees",
           .default_text = "-22",
           .group = "Camera"},
    Option{.name = "size",
           .set = bind<&Options::size>(),
           .kind = ValueKind::Size,
           .help = "output resolution",
           .default_text = "1280x720",
           .group = "Camera"},
    Option{.name = "fov",
           .set = bind<&Options::fov_deg>(),
           .kind = ValueKind::Float,
           .help = "vertical field of view in degrees",
           .default_text = "70",
           .group = "Camera"},

    Option{.name = "shadows",
           .set = bind<&Options::shadows>(),
           .kind = ValueKind::Toggle,
           .help = "traced sun-shadow ray per hit",
           .default_text = "on",
           .group = "Shading"},
    Option{.name = "ao",
           .set = bind<&Options::ao>(),
           .kind = ValueKind::Toggle,
           .help = "hemisphere ambient occlusion rays",
           .default_text = "on",
           .group = "Shading"},
    // Same awkward pair as voxel_app's (see app/src/app_options.cpp): --grain sets an AMPLITUDE,
    // --no-grain removes the TERM. A Toggle cannot express two targets, so --no-grain stays a
    // plain flag and parse_options() applies it -- identically in both programs.
    Option{.name = "no-grain",
           .set = bind<&Options::no_grain>(),
           .kind = ValueKind::Flag,
           .help = "remove the per-cube grain term entirely (--grain 0 keeps it at zero amplitude)",
           .group = "Shading"},
    Option{.name = "lod-march",
           .set = bind<&Options::lod_march>(),
           .kind = ValueKind::Toggle,
           .help = "Laine-Karras early-out when a node projects under a pixel",
           .default_text = "on",
           .group = "Shading"},
    Option{.name = "smooth-pixels",
           .set = bind<&Options::smooth_pixels>(),
           .kind = ValueKind::Float,
           .help = "ancestor span, in pixels, for the averaged normal",
           .default_text = "6",
           .group = "Shading"},
    Option{.name = "grain",
           .set = bind<&Options::grain_amplitude>(),
           .kind = ValueKind::Float,
           .help = "per-cube brightness grain amplitude",
           .default_text = "0.10",
           .group = "Shading"},
    Option{.name = "ao-radius",
           .set = bind<&Options::ao_radius_px>(),
           .kind = ValueKind::Float,
           .help = "AO ray length as a screen-space radius",
           .default_text = "32",
           .group = "Shading"},
    Option{.name = "beam-tile",
           .set = bind<&Options::beam_tile>(),
           .kind = ValueKind::Int,
           .help = "measure the oracle-beam step saving at this tile size (goal 266); 0 = off",
           .default_text = "0",
           .group = "Diagnostics"},
    Option{.name = "shadow-lod",
           .set = bind<&Options::shadow_lod>(),
           .kind = ValueKind::Float,
           .help = "how much coarser a shadow ray may judge its LOD",
           .default_text = "4",
           .group = "Shading"},
    Option{.name = "view",
           .set = bind<&Options::view>(),
           .kind = ValueKind::String,
           .help = "render ONE shading term: lit|ao|normal|facenormal|level|steps|coverage|cubepx|"
                   "smooth|lodcube|material|distance",
           .group = "Shading"},

    Option{.name = "verify-frame",
           .set = bind<&Options::verify>(),
           .kind = ValueKind::Flag,
           .help = "apply the app's local-contrast metric and carry the verdict in the exit code",
           .group = "Output",
           .alias = "verify"},
    Option{.name = "out",
           .set = bind<&Options::out>(),
           .kind = ValueKind::String,
           .help = "output PNG path",
           .default_text = "svo_render.png",
           .group = "Output"},
    Option{.name = "threads",
           .set = bind<&Options::threads>(),
           .kind = ValueKind::Int,
           .help = "render/build worker threads",
           .default_text = "0 = hardware concurrency",
           .group = "Output"},
};

static_assert(engine::cli::has_unique_names(kTable));

} // namespace

std::span<const engine::cli::Option> option_table() noexcept {
    return kTable;
}

engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out) {
    engine::cli::ParseOutcome outcome = engine::cli::parse_command_line(kTable, &out, argc, argv);
    if (outcome.ok && out.no_grain) {
        out.grain = false;
    }
    return outcome;
}

std::string help_text() {
    return engine::cli::render_help("svo_render",
                                    "The CPU REFERENCE renderer for the sparse-brick octree: the same\n"
                                    "TerrainSampler, build_tree and trace_ray the GPU marcher mirrors,\n"
                                    "written to a PNG. Option names match voxel_app's.",
                                    kTable);
}

} // namespace tools::svo_render
