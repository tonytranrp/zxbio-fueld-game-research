#pragma once

// A real JSON grammar validator, test-only.
//
// It exists because the first version of the writer's test checked brace balance and searched for
// substrings, and passed a file that began `{\n  ,\n  "schema": 1,` -- perfectly balanced,
// perfectly unparseable. Checking that output "looks right" is not the same as checking that a
// parser accepts it, and the difference cost a debugging session. ~90 lines, test-only, no
// dependency: exactly the tradeoff json.hpp's own note describes, applied to the other direction.

#include <cctype>
#include <string>
#include <string_view>

namespace dev::telemetry::test {

class JsonValidator {
public:
    explicit JsonValidator(std::string_view text) : text_(text) {}

    // Returns true if the whole input is exactly one well-formed JSON value.
    [[nodiscard]] bool validate() {
        skip_space();
        if (!parse_value()) {
            return false;
        }
        skip_space();
        if (at_ != text_.size()) {
            error_ = "trailing content at offset " + std::to_string(at_);
            return false;
        }
        return true;
    }

    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    void skip_space() {
        while (at_ < text_.size() &&
               (text_[at_] == ' ' || text_[at_] == '\n' || text_[at_] == '\r' || text_[at_] == '\t')) {
            ++at_;
        }
    }

    [[nodiscard]] bool fail(const char* what) {
        if (error_.empty()) {
            error_ = std::string{what} + " at offset " + std::to_string(at_) + " (near \"" +
                     std::string{text_.substr(at_ > 20 ? at_ - 20 : 0, 40)} + "\")";
        }
        return false;
    }

    [[nodiscard]] bool literal(std::string_view word) {
        if (text_.compare(at_, word.size(), word) != 0) {
            return fail("expected a literal");
        }
        at_ += word.size();
        return true;
    }

    [[nodiscard]] bool parse_string() {
        if (at_ >= text_.size() || text_[at_] != '"') {
            return fail("expected a string");
        }
        ++at_;
        while (at_ < text_.size()) {
            const char c = text_[at_];
            if (c == '"') {
                ++at_;
                return true;
            }
            if (c == '\\') {
                at_ += 2;
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("raw control character in a string");
            }
            ++at_;
        }
        return fail("unterminated string");
    }

    [[nodiscard]] bool parse_number() {
        const std::size_t begin = at_;
        if (at_ < text_.size() && (text_[at_] == '-' || text_[at_] == '+')) {
            ++at_;
        }
        while (at_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[at_])) != 0 || text_[at_] == '.' ||
                text_[at_] == 'e' || text_[at_] == 'E' || text_[at_] == '-' || text_[at_] == '+')) {
            ++at_;
        }
        return at_ > begin ? true : fail("expected a number");
    }

    [[nodiscard]] bool parse_object() {
        ++at_; // '{'
        skip_space();
        if (at_ < text_.size() && text_[at_] == '}') {
            ++at_;
            return true;
        }
        for (;;) {
            skip_space();
            if (!parse_string()) {
                return fail("expected a key");
            }
            skip_space();
            if (at_ >= text_.size() || text_[at_] != ':') {
                return fail("expected ':' after a key");
            }
            ++at_;
            skip_space();
            if (!parse_value()) {
                return false;
            }
            skip_space();
            if (at_ < text_.size() && text_[at_] == ',') {
                ++at_;
                continue;
            }
            if (at_ < text_.size() && text_[at_] == '}') {
                ++at_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    [[nodiscard]] bool parse_array() {
        ++at_; // '['
        skip_space();
        if (at_ < text_.size() && text_[at_] == ']') {
            ++at_;
            return true;
        }
        for (;;) {
            skip_space();
            if (!parse_value()) {
                return false;
            }
            skip_space();
            if (at_ < text_.size() && text_[at_] == ',') {
                ++at_;
                continue;
            }
            if (at_ < text_.size() && text_[at_] == ']') {
                ++at_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    [[nodiscard]] bool parse_value() {
        if (at_ >= text_.size()) {
            return fail("expected a value");
        }
        switch (text_[at_]) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return parse_string();
        case 't':
            return literal("true");
        case 'f':
            return literal("false");
        case 'n':
            return literal("null");
        default:
            return parse_number();
        }
    }

    std::string_view text_;
    std::size_t at_ = 0;
    std::string error_;
};

[[nodiscard]] inline bool is_valid_json(std::string_view text, std::string& error) {
    JsonValidator validator(text);
    if (validator.validate()) {
        return true;
    }
    error = validator.error();
    return false;
}

} // namespace dev::telemetry::test
