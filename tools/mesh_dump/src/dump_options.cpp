#include "dump_options.hpp"

#include <array>
#include <vector>

#include "engine/cli/help.hpp"

namespace tools::mesh_dump {

namespace {

using engine::cli::bind;
using engine::cli::Option;
using engine::cli::ValueKind;

constexpr std::array kTable{
    Option{.name = "cx", .set = bind<&Options::cx>(), .kind = ValueKind::Int, .help = "chunk X"},
    Option{.name = "cy", .set = bind<&Options::cy>(), .kind = ValueKind::Int, .help = "chunk Y"},
    Option{.name = "cz", .set = bind<&Options::cz>(), .kind = ValueKind::Int, .help = "chunk Z"},
    Option{.name = "seed",
           .set = bind<&Options::seed>(),
           .kind = ValueKind::Int,
           .help = "world seed",
           .default_text = "1337"},
    Option{.name = "out",
           .set = bind<&Options::out>(),
           .kind = ValueKind::String,
           .help = "output .obj path",
           .default_text = "chunk_<cx>_<cy>_<cz>.obj"},
};

static_assert(engine::cli::has_unique_names(kTable));

// The positional contract, in order. A positional is turned into the option it stands for and
// re-parsed through the same row -- so `mesh_dump 1 2 3 abc` produces the table's own "option
// \"--seed\" expects an integer, got \"abc\"" rather than a second, differently-worded diagnostic.
constexpr std::array<std::string_view, 5> kPositionalOrder{"cx", "cy", "cz", "seed", "out"};

} // namespace

engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out) {
    std::vector<std::string> positionals;
    engine::cli::ParseOutcome outcome =
        engine::cli::parse_command_line(kTable, &out, argc, argv, &positionals);
    if (!outcome.ok || outcome.help_requested) {
        return outcome;
    }
    if (positionals.size() > kPositionalOrder.size()) {
        return {.ok = false,
                .help_requested = false,
                .message = "too many positional arguments (expected at most " +
                           std::to_string(kPositionalOrder.size()) + ": cx cy cz seed out.obj)"};
    }
    std::vector<std::string> asOptions;
    asOptions.reserve(positionals.size() * 2);
    for (std::size_t i = 0; i < positionals.size(); ++i) {
        asOptions.emplace_back("--" + std::string{kPositionalOrder[i]});
        asOptions.push_back(positionals[i]);
    }
    // Named options were applied first, so a positional wins on a conflict -- which is the order
    // the pre-port tool had, since positionals were all it read.
    return engine::cli::parse(kTable, &out, asOptions);
}

std::string help_text() {
    return engine::cli::render_help(
        "mesh_dump",
        "Extracts one chunk's mesh (terrain + deterministic tree decoration) from the real\n"
        "generator and writes a Wavefront .obj.\n"
        "\n"
        "Positional form, unchanged: mesh_dump [cx cy cz] [seed] [out.obj]",
        kTable);
}

} // namespace tools::mesh_dump
