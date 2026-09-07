#include "dump_options.hpp"

#include <array>
#include <vector>

#include "engine/cli/help.hpp"

namespace tools::tree_dump {

namespace {

using engine::cli::bind;
using engine::cli::EnumEntry;
using engine::cli::Option;
using engine::cli::ValueKind;
using world::generation::TreeSpecies;

constexpr std::array kSpecies{
    EnumEntry{"round", static_cast<int>(TreeSpecies::RoundBroadleaf)},
    EnumEntry{"conifer", static_cast<int>(TreeSpecies::Conifer)},
    EnumEntry{"shrub", static_cast<int>(TreeSpecies::Shrub)},
    EnumEntry{"aspen", static_cast<int>(TreeSpecies::Aspen)},
};

constexpr std::array kTable{
    Option{.name = "species",
           .set = bind<&Options::species>(),
           .kind = ValueKind::Enum,
           .help = "which species to grow",
           .default_text = "round",
           .enum_values = kSpecies},
    Option{.name = "seed",
           .set = bind<&Options::seed>(),
           .kind = ValueKind::Int,
           .help = "growth seed",
           .default_text = "1337"},
    Option{.name = "out",
           .set = bind<&Options::out>(),
           .kind = ValueKind::String,
           .help = "output .obj path",
           .default_text = "tree_<species>_<seed>.obj"},
};

static_assert(engine::cli::has_unique_names(kTable));

constexpr std::array<std::string_view, 3> kPositionalOrder{"species", "seed", "out"};

} // namespace

std::string_view species_name(TreeSpecies species) noexcept {
    for (const EnumEntry& entry : kSpecies) {
        if (entry.value == static_cast<int>(species)) {
            return entry.name;
        }
    }
    return "unknown";
}

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
                           std::to_string(kPositionalOrder.size()) + ": species seed out.obj)"};
    }
    std::vector<std::string> asOptions;
    asOptions.reserve(positionals.size() * 2);
    for (std::size_t i = 0; i < positionals.size(); ++i) {
        asOptions.emplace_back("--" + std::string{kPositionalOrder[i]});
        asOptions.push_back(positionals[i]);
    }
    return engine::cli::parse(kTable, &out, asOptions);
}

std::string help_text() {
    return engine::cli::render_help(
        "tree_dump",
        "Writes a grown tree SKELETON (world/generation/tree_skeleton) as a Wavefront .obj of\n"
        "line segments, so the space-colonization result can be looked at before anything\n"
        "voxelizes it.\n"
        "\n"
        "Positional form, unchanged: tree_dump [species] [seed] [out.obj]",
        kTable);
}

} // namespace tools::tree_dump
