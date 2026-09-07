#include "dev/telemetry/json.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>

namespace dev::telemetry {

void JsonWriter::separate() {
    if (afterKey_) {
        afterKey_ = false;
        return;
    }
    if (needComma_) {
        out_ += ',';
    }
    if (!out_.empty()) {
        out_ += '\n';
        indent();
    }
    needComma_ = true;
}

void JsonWriter::indent() {
    out_.append(static_cast<std::size_t>(depth_) * 2, ' ');
}

void JsonWriter::begin_object() {
    separate();
    out_ += '{';
    ++depth_;
    needComma_ = false;
}

void JsonWriter::end_object() {
    --depth_;
    out_ += '\n';
    indent();
    out_ += '}';
    needComma_ = true;
}

void JsonWriter::begin_array() {
    separate();
    out_ += '[';
    ++depth_;
    needComma_ = false;
}

void JsonWriter::end_array() {
    --depth_;
    out_ += '\n';
    indent();
    out_ += ']';
    needComma_ = true;
}

void JsonWriter::key(std::string_view name) {
    // NOT separate() then value(): value() separates for itself, and calling both emitted the
    // separator TWICE -- a leading ",\n" before every key, which balanced parentheses perfectly
    // and parsed nowhere. The unit test that missed it checked brace balance and substrings; it
    // now runs a real grammar validator over the output (see test_frame_report.cpp).
    value(name);
    // The value that follows belongs to this key, on this line: suppress the newline/comma that
    // its own separate() would otherwise emit.
    out_ += ": ";
    afterKey_ = true;
}

void JsonWriter::value(std::string_view text) {
    separate();
    out_ += '"';
    for (const char c : text) {
        switch (c) {
        case '"':
            out_ += "\\\"";
            break;
        case '\\':
            out_ += "\\\\";
            break;
        case '\n':
            out_ += "\\n";
            break;
        case '\r':
            out_ += "\\r";
            break;
        case '\t':
            out_ += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char escape[8];
                std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(c));
                out_ += escape;
            } else {
                out_ += c;
            }
            break;
        }
    }
    out_ += '"';
}

void JsonWriter::value(double number) {
    separate();
    if (!std::isfinite(number)) {
        // JSON has no NaN or Infinity, and a report that silently emits one is a report no tool
        // will open. null says "this was not measured", which is what a NaN here always means.
        out_ += "null";
        return;
    }
    char buffer[64];
    // %.6g: enough to distinguish two frame times, short enough that a 900-frame report stays
    // diffable by eye.
    std::snprintf(buffer, sizeof(buffer), "%.6g", number);
    out_ += buffer;
}

void JsonWriter::value(long long number) {
    separate();
    out_ += std::to_string(number);
}

void JsonWriter::value(unsigned long long number) {
    separate();
    out_ += std::to_string(number);
}

void JsonWriter::value(bool flag) {
    separate();
    out_ += flag ? "true" : "false";
}

void JsonWriter::null_value() {
    separate();
    out_ += "null";
}

bool JsonWriter::write_to(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file << out_ << '\n';
    return file.good();
}

} // namespace dev::telemetry
