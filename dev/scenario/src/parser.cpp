#include "dev/scenario/parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace dev::scenario {

namespace {

[[nodiscard]] std::string_view trim(std::string_view text) noexcept {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::vector<std::string_view> split_words(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        const std::size_t begin = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        if (i > begin) {
            words.push_back(line.substr(begin, i - begin));
        }
    }
    return words;
}

[[nodiscard]] bool to_float(std::string_view text, float& out) noexcept {
    if (text.empty()) {
        return false;
    }
    const std::string_view body = text.front() == '+' ? text.substr(1) : text;
    const auto* last = body.data() + body.size();
    const std::from_chars_result r = std::from_chars(body.data(), last, out);
    return r.ec == std::errc{} && r.ptr == last;
}

[[nodiscard]] bool to_uint(std::string_view text, std::uint32_t& out) noexcept {
    if (text.empty()) {
        return false;
    }
    const auto* last = text.data() + text.size();
    const std::from_chars_result r = std::from_chars(text.data(), last, out);
    return r.ec == std::errc{} && r.ptr == last;
}

[[nodiscard]] bool to_vec3(std::string_view text, glm::vec3& out) noexcept {
    float parts[3]{};
    std::size_t begin = 0;
    for (int i = 0; i < 3; ++i) {
        const std::size_t comma = text.find(',', begin);
        const bool lastPart = i == 2;
        if (lastPart != (comma == std::string_view::npos)) {
            return false;
        }
        const std::string_view part = lastPart ? text.substr(begin) : text.substr(begin, comma - begin);
        if (!to_float(part, parts[i])) {
            return false;
        }
        begin = comma + 1;
    }
    out = glm::vec3{parts[0], parts[1], parts[2]};
    return true;
}

[[nodiscard]] std::string format_vec3(const glm::vec3& v) {
    std::ostringstream out;
    out << v.x << ',' << v.y << ',' << v.z;
    return out.str();
}

[[nodiscard]] std::string format_number(float value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

[[nodiscard]] std::string format_number(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

struct Context {
    std::string message;
    int depth = 0;
    std::vector<std::string> openFiles; // for the cycle diagnostic
};

[[nodiscard]] std::string where(std::string_view path, int line) {
    return std::string{path} + ":" + std::to_string(line) + ": ";
}

bool parse_into(std::string_view text, std::string_view path, Scenario& scenario, Context& ctx);

bool handle_include(std::string_view argument, std::string_view path, int lineNumber, Scenario& scenario,
                    Context& ctx) {
    const std::filesystem::path base = std::filesystem::path{std::string{path}}.parent_path();
    const std::filesystem::path resolved =
        base.empty() ? std::filesystem::path{std::string{argument}} : base / std::string{argument};
    // Cycles are rejected by identity of the resolved path, not by a depth counter: a depth limit
    // turns "a includes b includes a" into "too deep", which points at the wrong file.
    const std::string canonical = resolved.lexically_normal().string();
    if (std::find(ctx.openFiles.begin(), ctx.openFiles.end(), canonical) != ctx.openFiles.end()) {
        ctx.message = where(path, lineNumber) + "include cycle: \"" + canonical + "\" is already being read";
        return false;
    }
    std::ifstream file(resolved);
    if (!file) {
        ctx.message = where(path, lineNumber) + "include \"" + canonical + "\" could not be opened";
        return false;
    }
    std::ostringstream body;
    body << file.rdbuf();
    ctx.openFiles.push_back(canonical);
    const bool ok = parse_into(body.str(), canonical, scenario, ctx);
    ctx.openFiles.pop_back();
    return ok;
}

bool parse_into(std::string_view text, std::string_view path, Scenario& scenario, Context& ctx) {
    int lineNumber = 0;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t nl = text.find('\n', begin);
        std::string_view raw =
            nl == std::string_view::npos ? text.substr(begin) : text.substr(begin, nl - begin);
        begin = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
        ++lineNumber;

        if (const std::size_t hash = raw.find('#'); hash != std::string_view::npos) {
            raw = raw.substr(0, hash);
        }
        const std::string_view line = trim(raw);
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string_view> words = split_words(line);
        const std::string_view directive = words[0];
        const auto need = [&](std::size_t count, std::string_view expected) {
            if (words.size() < count) {
                ctx.message =
                    where(path, lineNumber) + std::string{directive} + " expects " + std::string{expected};
                return false;
            }
            return true;
        };

        if (directive == "name") {
            if (!need(2, "an identifier")) {
                return false;
            }
            scenario.name = std::string{words[1]};
        } else if (directive == "describe") {
            // Everything after the directive, verbatim -- a description is prose, not tokens.
            scenario.description = std::string{trim(line.substr(directive.size()))};
        } else if (directive == "option") {
            if (!need(2, "an option name, e.g. --no-taa")) {
                return false;
            }
            if (!words[1].starts_with("--")) {
                ctx.message = where(path, lineNumber) + "option names start with -- (got \"" +
                              std::string{words[1]} + "\")";
                return false;
            }
            for (std::size_t i = 1; i < words.size(); ++i) {
                scenario.options.emplace_back(words[i]);
            }
        } else if (directive == "pose") {
            if (!need(4, "<x,y,z> <yaw> <pitch>")) {
                return false;
            }
            Pose pose;
            if (!to_vec3(words[1], pose.position) || !to_float(words[2], pose.yaw_deg) ||
                !to_float(words[3], pose.pitch_deg)) {
                ctx.message = where(path, lineNumber) + "pose expects <x,y,z> <yaw> <pitch>";
                return false;
            }
            scenario.pose = pose;
        } else if (directive == "pose_ground") {
            if (!need(5, "<x,z> <yaw> <pitch> <metres above ground>")) {
                return false;
            }
            GroundPose pose;
            glm::vec3 xz{};
            const std::string flat = std::string{words[1]} + ",0";
            if (!to_vec3(flat, xz) || !to_float(words[2], pose.yaw_deg) ||
                !to_float(words[3], pose.pitch_deg) || !to_float(words[4], pose.height_above_ground)) {
                ctx.message =
                    where(path, lineNumber) + "pose_ground expects <x,z> <yaw> <pitch> <metres above ground>";
                return false;
            }
            pose.xz = glm::vec2{xz.x, xz.y};
            scenario.ground_pose = pose;
        } else if (directive == "hold") {
            if (!need(3, "<keys|none> <seconds>")) {
                return false;
            }
            Segment segment;
            segment.kind = SegmentKind::Hold;
            if (!parse_keys(words[1], segment.keys)) {
                ctx.message = where(path, lineNumber) +
                              "hold expects '+'-joined keys from "
                              "forward|back|left|right|up|down|boost|jump, or none (got \"" +
                              std::string{words[1]} + "\")";
                return false;
            }
            if (!to_float(words[2], segment.seconds)) {
                ctx.message = where(path, lineNumber) + "hold expects a duration in seconds";
                return false;
            }
            scenario.segments.push_back(segment);
        } else if (directive == "look") {
            if (!need(4, "<yaw> <pitch> <seconds>")) {
                return false;
            }
            Segment segment;
            segment.kind = SegmentKind::Look;
            if (!to_float(words[1], segment.yaw_deg) || !to_float(words[2], segment.pitch_deg) ||
                !to_float(words[3], segment.seconds)) {
                ctx.message = where(path, lineNumber) + "look expects <yaw> <pitch> <seconds>";
                return false;
            }
            scenario.segments.push_back(segment);
        } else if (directive == "goto") {
            if (!need(3, "<x,y,z> <radius> [timeout]")) {
                return false;
            }
            Segment segment;
            segment.kind = SegmentKind::Goto;
            segment.seconds = 30.0f; // default timeout
            if (!to_vec3(words[1], segment.target) || !to_float(words[2], segment.radius)) {
                ctx.message = where(path, lineNumber) + "goto expects <x,y,z> <radius> [timeout]";
                return false;
            }
            if (words.size() >= 4 && !to_float(words[3], segment.seconds)) {
                ctx.message = where(path, lineNumber) + "goto's timeout must be a number of seconds";
                return false;
            }
            scenario.segments.push_back(segment);
        } else if (directive == "wait") {
            if (!need(2, "<seconds>")) {
                return false;
            }
            Segment segment;
            segment.kind = SegmentKind::Wait;
            if (!to_float(words[1], segment.seconds)) {
                ctx.message = where(path, lineNumber) + "wait expects a duration in seconds";
                return false;
            }
            scenario.segments.push_back(segment);
        } else if (directive == "capture") {
            if (!need(2, "<frame N|time T|event NAME|end> <name>")) {
                return false;
            }
            CapturePoint point;
            std::size_t nameIndex = 2;
            if (words[1] == "end") {
                point.when = CaptureWhen::End;
            } else if (words[1] == "frame") {
                if (!need(4, "frame <N> <name>") || !to_uint(words[2], point.frame)) {
                    if (ctx.message.empty()) {
                        ctx.message = where(path, lineNumber) + "capture frame expects a frame number";
                    }
                    return false;
                }
                point.when = CaptureWhen::Frame;
                nameIndex = 3;
            } else if (words[1] == "time") {
                if (!need(4, "time <T> <name>") || !to_float(words[2], point.seconds)) {
                    if (ctx.message.empty()) {
                        ctx.message = where(path, lineNumber) + "capture time expects seconds";
                    }
                    return false;
                }
                point.when = CaptureWhen::Seconds;
                nameIndex = 3;
            } else if (words[1] == "event") {
                if (!need(4, "event <name> <capture-name>")) {
                    return false;
                }
                CapturePoint probe;
                if (!parse_capture_when(words[2], probe)) {
                    ctx.message = where(path, lineNumber) +
                                  "capture event expects "
                                  "tree-swapped|grounded|slow-frame|world-ready (got \"" +
                                  std::string{words[2]} + "\")";
                    return false;
                }
                point.when = CaptureWhen::Event;
                point.event = probe.event;
                nameIndex = 3;
            } else {
                ctx.message = where(path, lineNumber) + "capture expects frame|time|event|end (got \"" +
                              std::string{words[1]} + "\")";
                return false;
            }
            if (words.size() <= nameIndex) {
                ctx.message = where(path, lineNumber) + "capture expects a name for the image";
                return false;
            }
            point.name = std::string{words[nameIndex]};
            if (words.size() > nameIndex + 1) {
                if (words[nameIndex + 1] != "no-golden") {
                    ctx.message = where(path, lineNumber) +
                                  "capture takes only \"no-golden\" after the "
                                  "name (got \"" +
                                  std::string{words[nameIndex + 1]} + "\")";
                    return false;
                }
                point.golden = false;
            }
            scenario.captures.push_back(point);
        } else if (directive == "assert") {
            if (!need(4, "<metric> <op> <value>")) {
                return false;
            }
            Assertion assertion;
            if (!parse_metric(words[1], assertion.metric)) {
                ctx.message = where(path, lineNumber) + "unknown metric \"" + std::string{words[1]} + "\"";
                return false;
            }
            if (!parse_op(words[2], assertion.op)) {
                ctx.message = where(path, lineNumber) + "assert expects <|<=|>|>=|== (got \"" +
                              std::string{words[2]} + "\")";
                return false;
            }
            float value = 0.0f;
            if (!to_float(words[3], value)) {
                ctx.message = where(path, lineNumber) + "assert expects a numeric threshold";
                return false;
            }
            assertion.value = static_cast<double>(value);
            scenario.assertions.push_back(assertion);
        } else if (directive == "backend") {
            if (!need(2, "<vk|d3d12|both>")) {
                return false;
            }
            if (!parse_backend(words[1], scenario.backend)) {
                ctx.message = where(path, lineNumber) + "backend expects vk|d3d12|both (got \"" +
                              std::string{words[1]} + "\")";
                return false;
            }
        } else if (directive == "golden") {
            if (!need(2, "<directory>")) {
                return false;
            }
            scenario.golden_dir = std::string{words[1]};
        } else if (directive == "include") {
            if (!need(2, "<other.scn>")) {
                return false;
            }
            if (!handle_include(words[1], path, lineNumber, scenario, ctx)) {
                return false;
            }
        } else {
            ctx.message = where(path, lineNumber) + "unknown directive \"" + std::string{directive} +
                          "\" (known: name describe option pose hold look goto wait capture assert "
                          "backend golden include)";
            return false;
        }
    }
    return true;
}

} // namespace

ParseResult parse_scenario(std::string_view text, std::string_view path) {
    ParseResult result;
    Context ctx;
    ctx.openFiles.emplace_back(std::filesystem::path{std::string{path}}.lexically_normal().string());
    if (!parse_into(text, path, result.scenario, ctx)) {
        return {.ok = false, .message = std::move(ctx.message), .scenario = {}};
    }
    result.scenario.source = std::string{path};
    if (result.scenario.name.empty()) {
        // Fall back to the file's stem, so a scenario file that forgot `name` still has one --
        // and so a checked-in scenario's name and file name cannot silently differ.
        result.scenario.name = std::filesystem::path{std::string{path}}.stem().string();
    }
    return result;
}

ParseResult load_scenario(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {.ok = false, .message = "scenario \"" + path + "\" could not be opened", .scenario = {}};
    }
    std::ostringstream body;
    body << file.rdbuf();
    return parse_scenario(body.str(), path);
}

std::string emit_scenario(const Scenario& scenario) {
    std::string out;
    out += "name " + scenario.name + "\n";
    if (!scenario.description.empty()) {
        out += "describe " + scenario.description + "\n";
    }
    out += "backend " + std::string{backend_name(scenario.backend)} + "\n";
    if (!scenario.golden_dir.empty()) {
        out += "golden " + scenario.golden_dir + "\n";
    }
    // Options go back out one per line in the order they were given: order matters (a later
    // --no-taa overrides an earlier --taa) and re-emitting them sorted would change the meaning.
    for (std::size_t i = 0; i < scenario.options.size();) {
        std::string line = "option " + scenario.options[i];
        ++i;
        while (i < scenario.options.size() && !scenario.options[i].starts_with("--")) {
            line += ' ' + scenario.options[i];
            ++i;
        }
        out += line + "\n";
    }
    if (scenario.ground_pose) {
        out += "pose_ground " + format_number(scenario.ground_pose->xz.x) + "," +
               format_number(scenario.ground_pose->xz.y) + " " +
               format_number(scenario.ground_pose->yaw_deg) + " " +
               format_number(scenario.ground_pose->pitch_deg) + " " +
               format_number(scenario.ground_pose->height_above_ground) + "\n";
    }
    if (scenario.pose) {
        out += "pose " + format_vec3(scenario.pose->position) + " " + format_number(scenario.pose->yaw_deg) +
               " " + format_number(scenario.pose->pitch_deg) + "\n";
    }
    for (const Segment& segment : scenario.segments) {
        switch (segment.kind) {
        case SegmentKind::Hold:
            out += "hold " + keys_to_string(segment.keys) + " " + format_number(segment.seconds) + "\n";
            break;
        case SegmentKind::Look:
            out += "look " + format_number(segment.yaw_deg) + " " + format_number(segment.pitch_deg) + " " +
                   format_number(segment.seconds) + "\n";
            break;
        case SegmentKind::Goto:
            out += "goto " + format_vec3(segment.target) + " " + format_number(segment.radius) + " " +
                   format_number(segment.seconds) + "\n";
            break;
        case SegmentKind::Wait:
            out += "wait " + format_number(segment.seconds) + "\n";
            break;
        }
    }
    for (const CapturePoint& point : scenario.captures) {
        out += "capture " + capture_when_to_string(point) + " " + point.name +
               (point.golden ? "" : " no-golden") + "\n";
    }
    for (const Assertion& assertion : scenario.assertions) {
        out += "assert " + std::string{metric_name(assertion.metric)} + " " +
               std::string{op_name(assertion.op)} + " " + format_number(assertion.value) + "\n";
    }
    return out;
}

} // namespace dev::scenario
