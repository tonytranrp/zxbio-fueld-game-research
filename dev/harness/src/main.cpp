// voxel_harness -- the scenario runner (Prompt 002 Group AI-B/AI-C).
//
//   voxel_harness --scenario walk_shoreline --backend vk,d3d12
//
// Resolves a scenario by name, builds the engine from its options through voxel_app's OWN option
// table, drives the fixed-step controller from the scenario's motion script instead of from GLFW,
// and runs app::run_svo / app::run_mesh -- the same object file voxel_app links, not a copy.
// Captures at the scenario's capture points, evaluates its assertions, and exits non-zero on any
// failure.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <cstdint>
#include <string>
#include <vector>

#include "crash_handler.hpp"
#include "dev/scenario/registry.hpp"
#include "engine/cli/parser.hpp"
#include "engine/core/log.hpp"
#include "harness_options.hpp"
#include "harness_run.hpp"
#include "image_compare.hpp"
#include "moire_metric.hpp"
#include "render/diligent/render_context.hpp"

namespace {

using engine::core::log;
using engine::core::LogLevel;
using namespace dev;

} // namespace

int main(int argc, char** argv) {
    try {
        app::install_crash_handler();
        std::setvbuf(stdout, nullptr, _IONBF, 0);

        harness::Options options;
        const engine::cli::ParseOutcome parsed = harness::parse_options(argc, argv, options);
        if (!parsed.ok) {
            log(LogLevel::Error, "{}", parsed.message);
            std::fputs("\n", stderr);
            std::fputs(harness::help_text().c_str(), stderr);
            return EXIT_FAILURE;
        }
        // --moire: a pure image measurement. Before the scenario registry, because it needs
        // neither a scenario nor a GPU -- it is the metric's own validation path.
        if (!options.moire_files.empty()) {
            int worst = EXIT_SUCCESS;
            for (const std::string& file : options.moire_files) {
                const harness::Image image = harness::load_png(file);
                if (image.empty()) {
                    log(LogLevel::Error, "moire: cannot read {}", file);
                    worst = EXIT_FAILURE;
                    continue;
                }
                const harness::MoireResult m =
                    harness::moire_ratio(image, static_cast<std::uint32_t>(options.moire_crop_top));
                if (m.valid) {
                    std::printf("%-56s ratio %8.3f  carrier %.5f  textured %5.1f%%\n", file.c_str(),
                                m.ratio, m.carrier_rms, 100.0 * m.textured_fraction);
                } else {
                    std::printf("%-56s n/a  (%s)\n", file.c_str(), m.note.c_str());
                }
            }
            return worst;
        }

        if (parsed.help_requested) {
            std::fputs(harness::help_text().c_str(), stdout);
            return EXIT_SUCCESS;
        }

        std::string registryError;
        const int loaded = scenario::Registry::instance().add_directory(options.scenario_dir, registryError);
        if (loaded < 0) {
            log(LogLevel::Error, "{}", registryError);
            return EXIT_FAILURE;
        }

        if (options.list) {
            // Both kinds, each with its source: "why is this scenario not the one I edited" is
            // otherwise a five-minute question.
            std::printf("%-20s %-9s %s\n", "NAME", "SOURCE", "DESCRIPTION");
            for (const scenario::Entry& entry : scenario::Registry::instance().entries()) {
                std::printf("%-20s %-9s %.90s\n", entry.name.c_str(),
                            entry.origin == scenario::Origin::BuiltIn ? "built-in" : "file",
                            entry.description.c_str());
            }
            return EXIT_SUCCESS;
        }

        if (options.scenario.empty()) {
            log(LogLevel::Error, "--scenario is required (try --list-scenarios)");
            return EXIT_FAILURE;
        }
        const scenario::Entry* entry = scenario::Registry::instance().find(options.scenario);
        if (entry == nullptr) {
            log(LogLevel::Error, "no scenario named \"{}\" (try --list-scenarios)", options.scenario);
            return EXIT_FAILURE;
        }
        const scenario::Scenario sc = entry->build();

        // Which backends: the scenario declares, the command line may narrow.
        std::vector<render::diligent::Backend> backends;
        const bool wantVk =
            options.backend_filter.empty() || options.backend_filter.find("vk") != std::string::npos;
        const bool wantD3d =
            options.backend_filter.empty() || options.backend_filter.find("d3d12") != std::string::npos;
        if (sc.backend != scenario::BackendSelection::D3D12 && wantVk) {
            backends.push_back(render::diligent::Backend::Vulkan);
        }
        if (sc.backend != scenario::BackendSelection::Vulkan && wantD3d) {
            backends.push_back(render::diligent::Backend::D3D12);
        }
        if (backends.empty()) {
            log(LogLevel::Error, "--backend \"{}\" selects nothing this scenario declares ({})",
                options.backend_filter, scenario::backend_name(sc.backend));
            return EXIT_FAILURE;
        }

        if (!options.ramp.empty()) {
            // Goal 219's yardstick. One backend at a time: a ramp is a measurement, and mixing two
            // backends' numbers into one table would invite exactly the cross-backend comparison
            // the golden calibration says not to make.
            int rampCode = EXIT_SUCCESS;
            for (const render::diligent::Backend backend : backends) {
                std::vector<harness::RampRung> rungs;
                if (harness::run_ramp(sc, options, backend, rungs) != EXIT_SUCCESS) {
                    rampCode = EXIT_FAILURE;
                }
            }
            return rampCode;
        }

        int exitCode = EXIT_SUCCESS;
        std::vector<harness::RunResult> runs;
        for (const render::diligent::Backend backend : backends) {
            harness::RunResult run;
            const int code = harness::run_one(sc, options, backend, run);
            if (code != EXIT_SUCCESS) {
                exitCode = code;
            }
            runs.push_back(std::move(run));
        }

        if (!options.report.empty()) {
            if (!harness::write_report(options.report, sc, options, runs)) {
                log(LogLevel::Error, "could not write report to {}", options.report);
                exitCode = EXIT_FAILURE;
            } else {
                log(LogLevel::Info, "report: {}", options.report);
            }
        }
        return exitCode;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "fatal: unknown exception\n");
        return EXIT_FAILURE;
    }
}
