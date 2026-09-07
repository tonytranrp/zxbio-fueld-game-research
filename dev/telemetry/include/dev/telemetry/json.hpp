#pragma once

#include <string>
#include <string_view>

namespace dev::telemetry {

// A minimal JSON *writer*. Hand-rolled, deliberately, and here is the case (standing rule 8 of the
// prompt: no new dependency without a written one).
//
// What is actually needed: emit one object per run, with nested objects, arrays, numbers, strings
// and booleans, to a file nothing in this project ever reads back in C++ (the consumers are
// `python -m json.tool`, a diff, and a human). That is the whole requirement. It is ~90 lines.
//
// What a dependency would cost: nlohmann/json is a 25k-line single header whose inclusion is a
// measurable compile-time item in every TU that touches it (compile-time-performance.md's whole
// subject), and RapidJSON/simdjson are parsers -- their value is in reading, which is the half of
// the problem this project does not have. Neither earns a pin in cmake/Dependencies.cmake for an
// output-only format.
//
// What would change the answer: the moment the harness needs to READ a report back (comparing a
// run against a committed baseline in C++ rather than in Python), hand-rolling a parser is a
// genuinely bad idea and this note should be replaced with a pinned dependency.
class JsonWriter {
public:
    JsonWriter() = default;

    void begin_object();
    void end_object();
    void begin_array();
    void end_array();

    // Inside an object: a key, then exactly one value or one begin_*.
    void key(std::string_view name);

    void value(std::string_view text);
    void value(const char* text) { value(std::string_view{text}); }
    void value(const std::string& text) { value(std::string_view{text}); }
    void value(double number);
    void value(long long number);
    void value(unsigned long long number);
    // No std::size_t overload: it IS unsigned long long on this target (and unsigned long on the
    // Linux CI legs), so declaring one is either a redefinition or a portability trap.
    void value(int number) { value(static_cast<long long>(number)); }
    void value(unsigned number) { value(static_cast<unsigned long long>(number)); }
    void value(long number) { value(static_cast<long long>(number)); }
    void value(unsigned long number) { value(static_cast<unsigned long long>(number)); }
    void value(bool flag);
    void null_value();

    // key + value in one call, which is what almost every call site wants.
    template <class T>
    void field(std::string_view name, const T& v) {
        key(name);
        value(v);
    }

    [[nodiscard]] const std::string& str() const noexcept { return out_; }
    [[nodiscard]] bool write_to(const std::string& path) const;

private:
    void separate();
    void indent();

    std::string out_;
    int depth_ = 0;
    bool needComma_ = false;
    bool afterKey_ = false;
};

} // namespace dev::telemetry
