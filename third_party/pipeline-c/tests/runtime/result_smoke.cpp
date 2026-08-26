#include "pb/runtime/result.hpp"

#include <cassert>
#include <concepts>
#include <memory>
#include <string>
#include <utility>

int main()
{
    using pb::runtime::error;
    using pb::runtime::error_category;
    using pb::runtime::make_result;
    using pb::runtime::result;
    using pb::runtime::to_result;

    auto ok = result<int>{7};
    assert(ok.has_value());
    assert(!ok.has_error());
    assert(ok.value() == 7);
    assert(ok.value_or(99) == 7);
    assert(ok.error_or(error{.message = "fallback"}).message == "fallback");

    auto failed = result<int>{error{.stage = {"stage-1", "parse"}, .category = error_category::stage_failure, .message = "boom"}};
    assert(!failed.has_value());
    assert(failed.has_error());
    assert(failed.value_or(99) == 99);
    assert(failed.error().message == "boom");
    assert(failed.error_or(error{.message = "fallback"}).message == "boom");

    auto void_ok = result<void>{};
    assert(void_ok.has_value());
    assert(!void_ok.has_error());
    assert(void_ok.error_or(error{.message = "fallback"}).message == "fallback");

    auto void_failed = result<void>{error{.message = "void boom"}};
    assert(!void_failed.has_value());
    assert(void_failed.has_error());
    assert(void_failed.error_or(error{.message = "fallback"}).message == "void boom");

    struct non_default_error {
        explicit non_default_error(std::string message_value) : message(std::move(message_value)) {}
        std::string message;
    };

    static_assert(std::default_initializable<result<void, non_default_error>>);

    auto custom_void_ok = result<void, non_default_error>{};
    assert(custom_void_ok.has_value());
    assert(!custom_void_ok.has_error());
    assert(custom_void_ok.error_or(non_default_error{"fallback"}).message == "fallback");

    auto custom_void_failed = result<void, non_default_error>{non_default_error{"custom void boom"}};
    assert(!custom_void_failed.has_value());
    assert(custom_void_failed.has_error());
    assert(custom_void_failed.error().message == "custom void boom");
    assert(custom_void_failed.error_or(non_default_error{"fallback"}).message == "custom void boom");
    assert(std::move(custom_void_failed).error_or(non_default_error{"fallback"}).message == "custom void boom");

    struct fake_expected {
        using value_type = int;
        using error_type = std::string;
        bool ok{};
        int value_{};
        std::string error_{};

        bool has_value() const { return ok; }
        int value() const { return value_; }
        std::string const& error() const { return error_; }
    };

    auto converted = to_result(fake_expected{.ok = false, .error_ = "bad"});
    assert(!converted.has_value());
    assert(converted.error().category == error_category::expected_error);

    struct move_only_expected {
        using value_type = std::unique_ptr<int>;
        using error_type = std::string;
        bool ok{};
        std::unique_ptr<int> value_{};
        std::string error_{};

        bool has_value() const { return ok; }
        std::unique_ptr<int>& value() & { return value_; }
        std::unique_ptr<int>&& value() && { return std::move(value_); }
        std::string& error() & { return error_; }
        std::string&& error() && { return std::move(error_); }
    };

    auto moved_value = to_result(move_only_expected{.ok = true, .value_ = std::make_unique<int>(21)});
    assert(moved_value.has_value());
    assert(*moved_value.value() == 21);

    auto moved_error = to_result(move_only_expected{.ok = false, .error_ = "move-only bad"});
    assert(!moved_error.has_value());
    assert(moved_error.error().message == "move-only bad");

    const auto const_ok = result<std::string>{std::string{"stable"}};
    assert(const_ok.value() == "stable");
    assert(const_ok.value_or("fallback") == "stable");

    auto move_only_ok = result<std::unique_ptr<int>>{std::make_unique<int>(34)};
    assert(move_only_ok.has_value());
    auto moved_payload = std::move(move_only_ok).value_or(std::make_unique<int>(0));
    assert(*moved_payload == 34);

    auto move_only_failed = result<std::unique_ptr<int>>{error{.message = "no pointer"}};
    auto fallback_payload = std::move(move_only_failed).value_or(std::make_unique<int>(55));
    assert(*fallback_payload == 55);

    struct move_only_error {
        explicit move_only_error(std::unique_ptr<int> payload_value) : payload(std::move(payload_value)) {}
        move_only_error(move_only_error&&) noexcept = default;
        move_only_error& operator=(move_only_error&&) noexcept = default;
        move_only_error(const move_only_error&) = delete;
        move_only_error& operator=(const move_only_error&) = delete;
        std::unique_ptr<int> payload;
    };

    auto move_only_error_ok = result<int, move_only_error>{9};
    auto fallback_error = std::move(move_only_error_ok).error_or(move_only_error{std::make_unique<int>(12)});
    assert(*fallback_error.payload == 12);

    auto move_only_error_failed = result<int, move_only_error>{move_only_error{std::make_unique<int>(77)}};
    auto propagated_error = std::move(move_only_error_failed).error_or(move_only_error{std::make_unique<int>(0)});
    assert(*propagated_error.payload == 77);

    struct void_expected {
        using value_type = void;
        using error_type = std::string;
        bool ok{};
        std::string error_{};

        bool has_value() const { return ok; }
        void value() const {}
        std::string const& error() const { return error_; }
    };

    auto converted_void_ok = to_result(void_expected{.ok = true});
    assert(converted_void_ok.has_value());
    assert(!converted_void_ok.has_error());

    auto converted_void_error = to_result(void_expected{.ok = false, .error_ = "void expected bad"});
    assert(!converted_void_error.has_value());
    assert(converted_void_error.error().category == error_category::expected_error);
    assert(converted_void_error.error().message == "void expected bad");

    auto built = make_result(13);
    assert(built.has_value());
    assert(built.value() == 13);

    auto built_void = make_result();
    assert(built_void.has_value());
    assert(!built_void.has_error());

    return 0;
}
