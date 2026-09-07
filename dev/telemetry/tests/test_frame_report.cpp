// Goal 215/216's Check on the parts that do not need a GPU: the report's own numbers are
// internally consistent (the six phases sum to the wall time), the percentiles are nearest-rank,
// slow frames are attributed by the same priority run_svo used, and the JSON writer emits
// something a parser accepts.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "dev/telemetry/frame_report.hpp"
#include "dev/telemetry/json.hpp"
#include "json_validator.hpp"

using Catch::Approx;
using namespace dev::telemetry;

namespace {

FrameRecord frame(std::uint32_t index, double wallMs, FrameCauses causes = {}) {
    FrameRecord r;
    r.index = index;
    r.wall_ms = wallMs;
    r.causes = causes;
    // Split the wall time across the six phases so the coverage check has something real to
    // measure -- the proportions do not matter, the sum does.
    r.phases.frame_start = wallMs * 0.05;
    r.phases.upload = wallMs * 0.05;
    r.phases.camera = wallMs * 0.15;
    r.phases.render = wallMs * 0.25;
    r.phases.post = wallMs * 0.20;
    r.phases.overlay = wallMs * 0.10;
    r.phases.present = wallMs * 0.15;
    r.phases.capture = wallMs * 0.05;
    return r;
}

} // namespace

TEST_CASE("the six phases account for the frame", "[telemetry]") {
    FrameReport report;
    for (std::uint32_t i = 0; i < 100; ++i) {
        report.add(frame(i, 6.0 + static_cast<double>(i) * 0.01));
    }
    CHECK(report.phase_coverage() == Approx(1.0).margin(0.001));
    CHECK(report.frames_outside_phase_tolerance() == 0);
}

TEST_CASE("an unaccounted phase shows up as coverage under 1.0, not as a rounding error", "[telemetry]") {
    // The failure mode goal 215 asks about: if a phase is missing, the report must SAY so rather
    // than quietly attributing the gap to noise.
    FrameReport report;
    for (std::uint32_t i = 0; i < 50; ++i) {
        FrameRecord r = frame(i, 10.0);
        r.phases.present = 0.0; // 15% of the frame, unmeasured
        report.add(r);
    }
    CHECK(report.phase_coverage() == Approx(0.85).margin(0.001));
    CHECK(report.frames_outside_phase_tolerance() == 50);
}

TEST_CASE("percentiles are nearest-rank, so a p99 is a frame that happened", "[telemetry]") {
    FrameReport report;
    // 100 frames: 99 at 5 ms, one at 40 ms. The p99 must be 40, not an interpolated 5.35.
    for (std::uint32_t i = 0; i < 99; ++i) {
        report.add(frame(i, 5.0));
    }
    report.add(frame(99, 40.0));
    const Percentiles p = report.frame_ms();
    CHECK(p.median == Approx(5.0));
    CHECK(p.p95 == Approx(5.0));
    CHECK(p.p99 == Approx(5.0)); // the 99th of 100 sorted values is still 5
    CHECK(p.max == Approx(40.0));
    CHECK(p.mean == Approx(5.35));
}

TEST_CASE("slow frames are attributed by the same priority run_svo used", "[telemetry]") {
    FrameReport report;
    report.add(frame(0, 30.0, FrameCauses{.swapped = true, .uploading = true, .building = true}));
    report.add(frame(1, 25.0, FrameCauses{.uploading = true, .building = true}));
    report.add(frame(2, 22.0, FrameCauses{.building = true}));
    report.add(frame(3, 21.0, FrameCauses{}));
    report.add(frame(4, 5.0, FrameCauses{.swapped = true})); // fast: not slow at all
    const SlowFrameCounts slow = report.slow_frames();
    CHECK(slow.total == 4);
    CHECK(slow.on_swap == 1); // swap wins over upload and build
    CHECK(slow.while_uploading == 1);
    CHECK(slow.while_building == 1);
    CHECK(slow.other == 1);
}

TEST_CASE("gpu percentiles ignore the frames with no valid query", "[telemetry]") {
    // The Vulkan first-command fault means the first frames carry no GPU time; including them as
    // zeroes would make the instrumentation-cost measurement in goal 220 read low.
    FrameReport report;
    for (std::uint32_t i = 0; i < 10; ++i) {
        FrameRecord r = frame(i, 6.0);
        r.counters.gpu_ms = i < 2 ? 0.0 : 4.0;
        report.add(r);
    }
    CHECK(report.gpu_ms().mean == Approx(4.0));
    CHECK(report.gpu_ms().min == Approx(4.0));
}

TEST_CASE("the worst five frames come back worst first", "[telemetry]") {
    FrameReport report;
    const double times[]{5.0, 30.0, 7.0, 45.0, 6.0, 12.0, 9.0};
    std::uint32_t index = 0;
    for (const double t : times) {
        report.add(frame(index++, t));
    }
    const std::vector<FrameRecord> worst = report.worst(5);
    REQUIRE(worst.size() == 5);
    CHECK(worst[0].wall_ms == Approx(45.0));
    CHECK(worst[1].wall_ms == Approx(30.0));
    CHECK(worst[2].wall_ms == Approx(12.0));
    CHECK(worst[3].wall_ms == Approx(9.0));
    CHECK(worst[4].wall_ms == Approx(7.0));
}

TEST_CASE("warm-up frames are counted, not silently dropped", "[telemetry]") {
    FrameReport report;
    report.add_warmup(900.0);
    report.add_warmup(120.0);
    report.add(frame(0, 6.0));
    CHECK(report.frame_count() == 1);
    CHECK(report.warmup_count() == 2);
    CHECK(report.warmup_seconds() == Approx(1.02));
}

TEST_CASE("the JSON writer emits well-formed, escaped output", "[telemetry]") {
    JsonWriter json;
    json.begin_object();
    json.field("scenario", "spawn_stand");
    json.field("quoted", "a \"quoted\" \\ value\nwith a newline");
    json.field("frames", 900u);
    json.field("p95", 6.25);
    json.field("passed", true);
    json.key("nan_is_null");
    json.value(std::nan(""));
    json.key("phases");
    json.begin_array();
    for (int i = 0; i < 3; ++i) {
        json.begin_object();
        json.field("index", i);
        json.field("ms", 6.0 + i);
        json.end_object();
    }
    json.end_array();
    json.end_object();

    const std::string text = json.str();
    CAPTURE(text);
    // THE check: a real grammar validator, not a balance count. The first version of this test
    // passed output whose every key was preceded by a stray comma -- key() separated twice --
    // because balanced braces and a findable substring are not the same thing as parseable.
    std::string parseError;
    INFO(parseError);
    CHECK(dev::telemetry::test::is_valid_json(text, parseError));
    CHECK(parseError.empty());
    CHECK(text.front() == '{');
    CHECK(text.back() == '}');
    CHECK(text.find("\"scenario\": \"spawn_stand\"") != std::string::npos);
    CHECK(text.find("\\\"quoted\\\"") != std::string::npos);
    CHECK(text.find("\\\\ value\\nwith") != std::string::npos);
    CHECK(text.find("\"nan_is_null\": null") != std::string::npos);
    CHECK(text.find("\"passed\": true") != std::string::npos);

    // Braces and brackets balance, and no comma ever precedes a closing token -- the two ways a
    // hand-rolled emitter actually goes wrong.
    int braces = 0;
    int brackets = 0;
    char previousMeaningful = '\0';
    for (const char c : text) {
        if (c == '{') {
            ++braces;
        } else if (c == '}') {
            --braces;
            CHECK(previousMeaningful != ',');
        } else if (c == '[') {
            ++brackets;
        } else if (c == ']') {
            --brackets;
            CHECK(previousMeaningful != ',');
        }
        if (c != ' ' && c != '\n') {
            previousMeaningful = c;
        }
        CHECK(braces >= 0);
        CHECK(brackets >= 0);
    }
    CHECK(braces == 0);
    CHECK(brackets == 0);
}

TEST_CASE("the validator itself rejects what the writer's old bug produced", "[telemetry]") {
    // A guard on the guard: if is_valid_json accepted anything, the test above it would be
    // worthless. The first case is verbatim the shape the double-separating key() emitted.
    using dev::telemetry::test::is_valid_json;
    std::string error;
    CHECK_FALSE(is_valid_json("{\n  ,\n  \"schema\": 1\n}", error));
    CHECK_FALSE(is_valid_json("{\"a\": 1,}", error));
    CHECK_FALSE(is_valid_json("[1, 2", error));
    CHECK_FALSE(is_valid_json("{\"a\" 1}", error));
    CHECK_FALSE(is_valid_json("{} trailing", error));
    CHECK_FALSE(is_valid_json("{\"a\": }", error));
    CHECK(is_valid_json("{\"a\": [1, {\"b\": null}], \"c\": true}", error));
    CHECK(is_valid_json("{}", error));
    CHECK(is_valid_json("[]", error));
}
