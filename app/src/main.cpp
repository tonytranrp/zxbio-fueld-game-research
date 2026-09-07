// voxel_app's entry point, and nothing else. The frame loop, the Session and both renderer paths
// live in app_run.cpp so voxel_harness links the SAME object file rather than a copy of the loop
// (Prompt 002 goal 211); the option table lives in app_options.cpp.

#include <cstdio>
#include <cstdlib>
#include <exception>

#include "app_options.hpp"
#include "app_run.hpp"
#include "crash_handler.hpp"
#include "engine/cli/parser.hpp"
#include "engine/core/log.hpp"

namespace {
using engine::core::log;
using engine::core::LogLevel;
using app::AppOptions;
} // namespace

int main(int argc, char** argv) {
    // Everything is inside the try, including argument parsing, and there is a catch-all: escaping
    // main is std::terminate, which loses the message. The crash handler below reports what it can
    // for the failures it hooks, but a thrown exception that never reaches a catch is not one of
    // them. (clang-tidy's bugprone-exception-escape; the same fix tools/svo_render got.)
    try {
        app::install_crash_handler();
        // Unbuffered stdout: when output is redirected to a file (smoke runs, CI), full buffering
        // would otherwise eat the final log lines -- including exception reports -- if the process
        // dies without flushing. Cost is irrelevant at this log volume.
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        AppOptions options;
        const engine::cli::ParseOutcome parsed = app::parse_app_options(argc, argv, options);
        if (!parsed.ok) {
            log(LogLevel::Error, "{}", parsed.message);
            std::fputs("\n", stderr);
            std::fputs(app::app_help_text().c_str(), stderr);
            return EXIT_FAILURE;
        }
        if (parsed.help_requested) {
            std::fputs(app::app_help_text().c_str(), stdout);
            return EXIT_SUCCESS;
        }
        // Goal 231: a flag that is silently ignored is how you lose an afternoon. --fly and
        // --noclip are dev tools; without --dev they REFUSE, naming the flag that unlocks them.
        if ((options.fly || options.noclip) && !options.dev) {
            log(LogLevel::Error,
                "{} is a developer tool -- pass --dev to unlock fly, noclip and the G toggle "
                "together. The body walks by default now (Prompt 003 goal 231).",
                options.fly ? "--fly" : "--noclip");
            return EXIT_FAILURE;
        }
#ifndef NDEBUG
        // Group J task 20's check: deliberately exercise each crash-handler hook. Debug-only by
        // construction -- the flag does not exist in a release build's table. It fires AFTER the
        // parse now rather than inside it, which is what lets the option be a row like any other.
        if (!options.crash_test.empty()) {
            log(LogLevel::Info, "crash-test: triggering \"{}\"", options.crash_test);
            if (options.crash_test == "av") {
                int* p = nullptr;
                *p = 42; // NOLINT(clang-analyzer-core.NullDereference) -- the point of the test
            } else if (options.crash_test == "abort") {
                std::abort();
            } else if (options.crash_test == "terminate") {
                std::terminate();
            }
            log(LogLevel::Error, "--crash-test expects av|abort|terminate, got \"{}\"", options.crash_test);
            return EXIT_FAILURE;
        }
#endif
        return app::run(options);
    } catch (const std::exception& e) {
        // fprintf, not log(): a handler in main must not itself be able to throw, and log()
        // formats. tools/svo_render's main reports the same way for the same reason.
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "fatal: unknown exception\n");
        return EXIT_FAILURE;
    }
}
