#include "harness_options.hpp"

#include <array>

#include "engine/cli/help.hpp"

namespace dev::harness {

namespace {

using engine::cli::bind;
using engine::cli::Option;
using engine::cli::ValueKind;

constexpr std::array kTable{
    Option{.name = "scenario",
           .set = bind<&Options::scenario>(),
           .kind = ValueKind::String,
           .help = "which scenario to run",
           .group = "What to run"},
    Option{.name = "backend",
           .set = bind<&Options::backend_filter>(),
           .kind = ValueKind::String,
           .help = "narrow the scenario's own backend declaration: vk, d3d12, or vk,d3d12",
           .default_text = "whatever the scenario declares",
           .group = "What to run"},
    Option{.name = "list-scenarios",
           .set = bind<&Options::list>(),
           .kind = ValueKind::Flag,
           .help = "print every scenario with its source and exit",
           .group = "What to run"},
    Option{.name = "scenario-dir",
           .set = bind<&Options::scenario_dir>(),
           .kind = ValueKind::String,
           .help = "where the .scn files live",
           .default_text = "dev/scenarios",
           .group = "What to run"},
    Option{.name = "frames",
           .set = bind<&Options::frame_cap>(),
           .kind = ValueKind::Int,
           .help = "hard frame ceiling regardless of the script's length",
           .default_text = "0 = the script decides",
           .group = "What to run"},

    Option{.name = "ramp",
           .set = bind<&Options::ramp>(),
           .kind = ValueKind::String,
           .help = "re-run once per value: NAME:v1,v2,... (e.g. lod-radius:1,2,4,8,16)",
           .group = "What to run"},

    Option{.name = "headless",
           .set = bind<&Options::headless>(),
           .kind = ValueKind::Flag,
           .help = "run with the window hidden (same swap chain and readback, nothing on screen)",
           .group = "How to run"},

    Option{.name = "out-dir",
           .set = bind<&Options::out_dir>(),
           .kind = ValueKind::String,
           .help = "where captures, diff images and the report are written",
           .default_text = ".",
           .group = "Output"},
    Option{.name = "report",
           .set = bind<&Options::report>(),
           .kind = ValueKind::String,
           .help = "write the machine-readable run report to this path",
           .group = "Output"},
    Option{.name = "golden-root",
           .set = bind<&Options::golden_root>(),
           .kind = ValueKind::String,
           .help = "root of the per-scenario, per-backend golden directories",
           .default_text = "dev/goldens",
           .group = "Output"},
    Option{.name = "accept-golden",
           .set = bind<&Options::accept_golden>(),
           .kind = ValueKind::Flag,
           .help = "promote this run's captures to goldens (prints the old and new distances first)",
           .group = "Output"},
    Option{.name = "no-golden",
           .set = bind<&Options::no_golden>(),
           .kind = ValueKind::Flag,
           .help = "skip the golden comparison entirely (WARP in CI: its images are a software "
                   "rasteriser's, not this GPU's)",
           .group = "Output"},
};

static_assert(engine::cli::has_unique_names(kTable));

} // namespace

std::span<const engine::cli::Option> option_table() noexcept {
    return kTable;
}

engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out) {
    return engine::cli::parse_command_line(kTable, &out, argc, argv);
}

std::string help_text() {
    return engine::cli::render_help(
        "voxel_harness",
        "Runs a named scenario against the SAME frame loop voxel_app runs, driving the fixed-step\n"
        "controller from the scenario's motion script instead of from GLFW.\n"
        "\n"
        "A scenario's own `option` lines are voxel_app's options -- see `voxel_app --help` for those.",
        kTable);
}

} // namespace dev::harness
