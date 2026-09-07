// Goal 209/210's Check: every checked-in .scn parses, re-emits and re-parses to an equal model;
// a malformed line reports file, line number and what was expected; `include` resolves relative to
// the including file and rejects cycles; and a scenario registered from a test's own TU appears in
// the registry with no central list touched.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "dev/scenario/parser.hpp"
#include "dev/scenario/registry.hpp"

using namespace dev::scenario;

namespace {

// Where the checked-in scenarios live, passed in by CMake so this does not depend on the working
// directory the test happens to be run from.
const std::filesystem::path kScenarioDir{DEV_SCENARIO_DIR};

std::filesystem::path temp_dir() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "dev_scenario_format_test";
    std::filesystem::create_directories(dir);
    return dir;
}

void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// A built-in registered from THIS translation unit: goal 210's "one new .cpp, no registry list".
// It is an anonymous-namespace static, so its constructor runs before main() and nothing central
// mentions it.
const AutoRegister kTestBuiltIn{Entry{
    .name = "test_builtin_probe",
    .description = "registered by the test's own TU, to prove no central list is needed",
    .origin = Origin::BuiltIn,
    .source = "test_scenario_format.cpp",
    .build =
        []() {
            Scenario s;
            s.name = "test_builtin_probe";
            s.segments.push_back(Segment{.kind = SegmentKind::Wait, .seconds = 1.0f});
            return s;
        },
}};

} // namespace

TEST_CASE("every checked-in scenario parses, re-emits and re-parses to an equal model", "[scenario]") {
    REQUIRE(std::filesystem::is_directory(kScenarioDir));
    int seen = 0;
    for (const std::filesystem::directory_entry& item : std::filesystem::directory_iterator(kScenarioDir)) {
        if (!item.is_regular_file() || item.path().extension() != ".scn") {
            continue;
        }
        CAPTURE(item.path().string());
        const ParseResult first = load_scenario(item.path().string());
        INFO(first.message);
        REQUIRE(first.ok);
        // Every checked-in scenario says what it is for. The prompt asks for a one-paragraph
        // header; this asserts the machine-readable half of that exists.
        CHECK_FALSE(first.scenario.description.empty());
        CHECK_FALSE(first.scenario.name.empty());

        const std::string emitted = emit_scenario(first.scenario);
        const ParseResult second = parse_scenario(emitted, item.path().string());
        INFO(second.message);
        INFO(emitted);
        REQUIRE(second.ok);
        CHECK(second.scenario == first.scenario);
        ++seen;
    }
    CHECK(seen >= 10); // the starting library of goal 213
}

TEST_CASE("a malformed line reports file, line number and what was expected", "[scenario]") {
    const std::string text = "name probe\n"
                             "describe a deliberately broken scenario\n"
                             "hold sideways 1.0\n";
    const ParseResult result = parse_scenario(text, "probe.scn");
    REQUIRE_FALSE(result.ok);
    CHECK(result.message.find("probe.scn:3:") != std::string::npos);
    CHECK(result.message.find("sideways") != std::string::npos);
    CHECK(result.message.find("forward|back") != std::string::npos);
}

TEST_CASE("each directive's diagnostic names the directive", "[scenario]") {
    struct Case {
        const char* line;
        const char* expectFragment;
    };
    const std::vector<Case> cases{
        {"pose 1,2 0 0", "pose expects"},
        {"look 90 0", "look expects"},
        {"goto 1,2,3", "goto expects"},
        {"wait", "wait expects"},
        {"assert nonsense_metric < 5", "unknown metric"},
        {"assert frame_ms_p95 ~ 5", "assert expects"},
        {"backend metal", "backend expects"},
        {"capture whenever shot", "capture expects"},
        {"capture event nonsense shot", "capture event expects"},
        {"option no-dashes", "option names start with"},
        {"nonsense_directive 1", "unknown directive"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.line);
        const ParseResult result = parse_scenario(std::string{"name probe\n"} + c.line + "\n", "p.scn");
        REQUIRE_FALSE(result.ok);
        CHECK(result.message.find("p.scn:2:") != std::string::npos);
        CHECK(result.message.find(c.expectFragment) != std::string::npos);
    }
}

TEST_CASE("include resolves relative to the including file", "[scenario]") {
    const std::filesystem::path dir = temp_dir();
    const std::filesystem::path sub = dir / "shared";
    std::filesystem::create_directories(sub);
    write(sub / "common.scn", "option --no-taa\noption --frames 30\n");
    write(dir / "main.scn", "name including\ndescribe includes a shared fragment\n"
                            "include shared/common.scn\nwait 1\n");

    const ParseResult result = load_scenario((dir / "main.scn").string());
    INFO(result.message);
    REQUIRE(result.ok);
    CHECK(result.scenario.name == "including");
    REQUIRE(result.scenario.options.size() == 3);
    CHECK(result.scenario.options[0] == "--no-taa");
    CHECK(result.scenario.options[1] == "--frames");
    CHECK(result.scenario.options[2] == "30");
    REQUIRE(result.scenario.segments.size() == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("an include cycle is rejected by name, not by a depth limit", "[scenario]") {
    const std::filesystem::path dir = temp_dir();
    write(dir / "a.scn", "name a\ninclude b.scn\n");
    write(dir / "b.scn", "include a.scn\n");
    const ParseResult result = load_scenario((dir / "a.scn").string());
    REQUIRE_FALSE(result.ok);
    CHECK(result.message.find("include cycle") != std::string::npos);
    CHECK(result.message.find("a.scn") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("a missing include names the path it could not open", "[scenario]") {
    const std::filesystem::path dir = temp_dir();
    write(dir / "a.scn", "name a\ninclude nowhere.scn\n");
    const ParseResult result = load_scenario((dir / "a.scn").string());
    REQUIRE_FALSE(result.ok);
    CHECK(result.message.find("nowhere.scn") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("a scenario added from a translation unit appears in the registry", "[scenario]") {
    // No central list was edited to make this pass -- kTestBuiltIn above is the whole registration.
    const Entry* entry = Registry::instance().find("test_builtin_probe");
    REQUIRE(entry != nullptr);
    CHECK(entry->origin == Origin::BuiltIn);
    CHECK(entry->source == "test_scenario_format.cpp");
    const Scenario built = entry->build();
    CHECK(built.name == "test_builtin_probe");
    REQUIRE(built.segments.size() == 1);
}

TEST_CASE("the registry reads the checked-in scenario directory", "[scenario]") {
    Registry registry;
    std::string error;
    const int added = registry.add_directory(kScenarioDir.string(), error);
    INFO(error);
    REQUIRE(added >= 10);
    CHECK(registry.find("spawn_stand") != nullptr);
    CHECK(registry.find("fly_transect") != nullptr);
    CHECK(registry.find("throughput_ramp") != nullptr);
    for (const Entry& entry : registry.entries()) {
        CAPTURE(entry.name);
        CHECK(entry.origin == Origin::File);
        CHECK_FALSE(entry.source.empty());
    }
}

TEST_CASE("a file scenario shadows a built-in of the same name", "[scenario]") {
    // The intended way to iterate on a built-in without rebuilding, and the reason
    // --list-scenarios prints each entry's source.
    Registry registry;
    registry.add(Entry{.name = "shadowed",
                       .description = "the built-in",
                       .origin = Origin::BuiltIn,
                       .source = "<built-in>",
                       .build = []() { return Scenario{}; }});
    registry.add(Entry{.name = "shadowed",
                       .description = "the file",
                       .origin = Origin::File,
                       .source = "shadowed.scn",
                       .build = []() { return Scenario{}; }});
    REQUIRE(registry.entries().size() == 1);
    CHECK(registry.find("shadowed")->origin == Origin::File);
}
