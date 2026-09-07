// Goal 202/203/206's Check, on a fake table rather than the app's real one: this suite is about
// the MECHANISM (every value kind round-trips, --no-x and --x reach the same target, the three
// diagnostics name the option, a duplicate long name is a compile-time property, @file expands).
// The app's own table is checked field-by-field against pre-port behaviour in app/tests.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "engine/cli/help.hpp"
#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"
#include "engine/cli/response_file.hpp"

using Catch::Approx;
using namespace engine::cli;

namespace {

enum class Flavour { Sweet, Sour, Salt };

struct Fake {
    bool flag = false;
    bool toggle = true;
    int count = 7;
    float scale = 1.5f;
    std::string text = "unset";
    glm::vec3 where{0.0f};
    Size size{0, 0};
    Flavour flavour = Flavour::Sweet;
    std::optional<float> maybe;
    std::size_t big = 4;
};

constexpr std::array kFlavours{
    EnumEntry{"sweet", static_cast<int>(Flavour::Sweet)},
    EnumEntry{"sour", static_cast<int>(Flavour::Sour)},
    EnumEntry{"salt", static_cast<int>(Flavour::Salt)},
};

constexpr std::array kTable{
    Option{.name = "flag", .set = bind<&Fake::flag>(), .kind = ValueKind::Flag, .help = "a presence flag"},
    Option{.name = "toggle",
           .set = bind<&Fake::toggle>(),
           .kind = ValueKind::Toggle,
           .help = "on by default",
           .default_text = "on"},
    Option{.name = "count", .set = bind<&Fake::count>(), .kind = ValueKind::Int, .help = "how many"},
    Option{.name = "scale", .set = bind<&Fake::scale>(), .kind = ValueKind::Float, .help = "how much"},
    Option{.name = "text", .set = bind<&Fake::text>(), .kind = ValueKind::String, .help = "what to say"},
    Option{.name = "where", .set = bind<&Fake::where>(), .kind = ValueKind::Vec3, .help = "a point"},
    Option{.name = "size", .set = bind<&Fake::size>(), .kind = ValueKind::Size, .help = "an extent"},
    Option{.name = "flavour",
           .set = bind<&Fake::flavour>(),
           .kind = ValueKind::Enum,
           .help = "pick one",
           .group = "Taste",
           .enum_values = kFlavours},
    Option{.name = "maybe",
           .set = bind<&Fake::maybe>(),
           .kind = ValueKind::Float,
           .help = "unset unless given",
           .alias = "perhaps"},
    Option{.name = "big", .set = bind<&Fake::big>(), .kind = ValueKind::Int, .help = "a size_t"},
};

// The compile-time property goal 202's Check asks for, asserted where a real table asserts it.
static_assert(has_unique_names(kTable));

ParseOutcome run(Fake& fake, std::vector<std::string> args) {
    return parse(kTable, &fake, args);
}

} // namespace

TEST_CASE("every value kind round-trips through the table", "[cli]") {
    Fake fake;
    const ParseOutcome outcome =
        run(fake, {"--flag", "--count", "42", "--scale", "0.25", "--text", "hello", "--where", "1,-2.5,3",
                   "--size", "1280x720", "--flavour", "sour", "--maybe", "9", "--big", "64"});
    REQUIRE(outcome.ok);
    CHECK(fake.flag);
    CHECK(fake.count == 42);
    CHECK(fake.scale == Approx(0.25f));
    CHECK(fake.text == "hello");
    CHECK(fake.where.x == Approx(1.0f));
    CHECK(fake.where.y == Approx(-2.5f));
    CHECK(fake.where.z == Approx(3.0f));
    CHECK(fake.size == Size{1280, 720});
    CHECK(fake.flavour == Flavour::Sour);
    REQUIRE(fake.maybe.has_value());
    CHECK(*fake.maybe == Approx(9.0f));
    CHECK(fake.big == 64u);
}

TEST_CASE("an optional target stays unset when its option is absent", "[cli]") {
    Fake fake;
    REQUIRE(run(fake, {"--count", "1"}).ok);
    CHECK_FALSE(fake.maybe.has_value());
}

TEST_CASE("a toggle answers to both the plain and the no- spelling", "[cli]") {
    // The whole point of ValueKind::Toggle: one row, one member, both spellings. Two rows pointing
    // at one bool is the shape that silently rots when the default changes.
    Fake off;
    REQUIRE(run(off, {"--no-toggle"}).ok);
    CHECK_FALSE(off.toggle);

    Fake on;
    on.toggle = false;
    REQUIRE(run(on, {"--toggle"}).ok);
    CHECK(on.toggle);

    // Last one wins, in argument order -- so a response file's setting can be overridden on the
    // command line after it.
    Fake both;
    REQUIRE(run(both, {"--no-toggle", "--toggle"}).ok);
    CHECK(both.toggle);
}

TEST_CASE("an alias is a row property, not a second row", "[cli]") {
    Fake fake;
    REQUIRE(run(fake, {"--perhaps", "2.5"}).ok);
    REQUIRE(fake.maybe.has_value());
    CHECK(*fake.maybe == Approx(2.5f));
}

TEST_CASE("an inline =value equals a separate value token", "[cli]") {
    Fake split;
    Fake joined;
    REQUIRE(run(split, {"--count", "5"}).ok);
    REQUIRE(run(joined, {"--count=5"}).ok);
    CHECK(split.count == joined.count);
}

TEST_CASE("the three diagnostics each name the option", "[cli]") {
    SECTION("unknown option") {
        Fake fake;
        const ParseOutcome outcome = run(fake, {"--nonsense"});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.message.find("--nonsense") != std::string::npos);
        CHECK(outcome.message.find("unknown option") != std::string::npos);
    }
    SECTION("missing value") {
        Fake fake;
        const ParseOutcome outcome = run(fake, {"--count"});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.message.find("--count") != std::string::npos);
        CHECK(outcome.message.find("no value followed it") != std::string::npos);
    }
    SECTION("unparseable value") {
        Fake fake;
        const ParseOutcome outcome = run(fake, {"--count", "twelve"});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.message.find("--count") != std::string::npos);
        CHECK(outcome.message.find("twelve") != std::string::npos);
    }
    SECTION("an unparseable enum lists what it would have accepted") {
        Fake fake;
        const ParseOutcome outcome = run(fake, {"--flavour", "umami"});
        CHECK_FALSE(outcome.ok);
        CHECK(outcome.message.find("--flavour") != std::string::npos);
        CHECK(outcome.message.find("sweet|sour|salt") != std::string::npos);
    }
}

TEST_CASE("trailing garbage in a number is a diagnostic, not a silent prefix", "[cli]") {
    // strtol/atoi -- what all three hand-rolled parsers this module replaces used -- accept "12abc"
    // as 12 and "abc" as 0. std::from_chars plus an end-of-input check does not.
    Fake fake;
    CHECK_FALSE(run(fake, {"--count", "12abc"}).ok);
    CHECK_FALSE(run(fake, {"--scale", "1.5x"}).ok);
    CHECK_FALSE(run(fake, {"--where", "1,2"}).ok);
    CHECK_FALSE(run(fake, {"--where", "1,2,3,4"}).ok);
    CHECK_FALSE(run(fake, {"--size", "1280"}).ok);
}

TEST_CASE("a duplicate long name is a compile-time property of the table", "[cli]") {
    // Documented as a property rather than exercised as a compile failure: a test that tries to
    // compile a bad table needs a second TU and a build-system hook, and the value here is that
    // static_assert(has_unique_names(...)) at every real table's definition site is what actually
    // fires. This pins the predicate those static_asserts depend on.
    static constexpr std::array kDuplicate{
        Option{.name = "count", .set = bind<&Fake::count>(), .kind = ValueKind::Int},
        Option{.name = "count", .set = bind<&Fake::big>(), .kind = ValueKind::Int},
    };
    static constexpr std::array kAliasCollision{
        Option{.name = "count", .set = bind<&Fake::count>(), .kind = ValueKind::Int},
        Option{.name = "big", .set = bind<&Fake::big>(), .kind = ValueKind::Int, .alias = "count"},
    };
    static constexpr std::array kMissingSetter{
        Option{.name = "count", .set = nullptr, .kind = ValueKind::Int},
    };
    STATIC_CHECK_FALSE(has_unique_names(kDuplicate));
    STATIC_CHECK_FALSE(has_unique_names(kAliasCollision));
    STATIC_CHECK_FALSE(has_unique_names(kMissingSetter));
    STATIC_CHECK(has_unique_names(kTable));
}

TEST_CASE("help text is generated from the table", "[cli]") {
    const std::string help = render_help("fake", "a fake program", kTable);
    for (const Option& row : kTable) {
        CAPTURE(row.name);
        CHECK(help.find("--" + std::string{row.name}) != std::string::npos);
    }
    // Groups appear, and a Toggle advertises both spellings so nobody has to guess.
    CHECK(help.find("Taste:") != std::string::npos);
    CHECK(help.find("--no-toggle") != std::string::npos);
    CHECK(help.find("--perhaps") != std::string::npos);
    CHECK(help.find("@FILE") != std::string::npos);
}

TEST_CASE("help is recognised and does not fail the parse", "[cli]") {
    Fake fake;
    const ParseOutcome outcome = run(fake, {"--help"});
    CHECK(outcome.ok);
    CHECK(outcome.help_requested);
}

TEST_CASE("a response file reproduces the same command line", "[cli]") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "engine_cli_response_test.txt";
    {
        std::ofstream out(path);
        out << "# a comment line\n";
        out << "\n";
        out << "--count 42\n";
        out << "--text hello   # trailing comment\n";
        out << "--flavour\n";
        out << "salt\n";
    }
    Fake fromFile;
    const ExpandResult expanded = expand_response_files(std::vector<std::string>{"@" + path.string()});
    REQUIRE(expanded.ok);
    REQUIRE(parse(kTable, &fromFile, expanded.args).ok);

    Fake fromArgs;
    REQUIRE(run(fromArgs, {"--count", "42", "--text", "hello", "--flavour", "salt"}).ok);

    CHECK(fromFile.count == fromArgs.count);
    CHECK(fromFile.text == fromArgs.text);
    CHECK(fromFile.flavour == fromArgs.flavour);
    std::filesystem::remove(path);
}

TEST_CASE("a missing response file names the path", "[cli]") {
    const ExpandResult expanded = expand_response_files(std::vector<std::string>{"@no/such/file.opts"});
    CHECK_FALSE(expanded.ok);
    CHECK(expanded.message.find("no/such/file.opts") != std::string::npos);
}

TEST_CASE("a nested response file is rejected with a clear message", "[cli]") {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "engine_cli_nested_test.txt";
    {
        std::ofstream out(path);
        out << "--count 1\n";
        out << "@other.opts\n";
    }
    const ExpandResult expanded = expand_response_files(std::vector<std::string>{"@" + path.string()});
    CHECK_FALSE(expanded.ok);
    CHECK(expanded.message.find("nested") != std::string::npos);
    CHECK(expanded.message.find(":2:") != std::string::npos); // file and line number
    std::filesystem::remove(path);
}

TEST_CASE("positionals are collected when the caller asks for them", "[cli]") {
    Fake fake;
    std::vector<std::string> positionals;
    const std::vector<std::string> args{"12", "--count", "3", "out.obj"};
    REQUIRE(parse(kTable, &fake, args, &positionals).ok);
    REQUIRE(positionals.size() == 2);
    CHECK(positionals[0] == "12");
    CHECK(positionals[1] == "out.obj");

    // ...and are an error when it does not: a program with no positional arguments should say so
    // rather than ignore a typo'd flag that lost its dashes.
    Fake strict;
    CHECK_FALSE(parse(kTable, &strict, args).ok);
}
