#include <pb/pipeline.hpp>

#include <cstddef>
#include <string_view>
#include <type_traits>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct ParseError {};
struct NotAStage {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  using error_type = ParseError;
  static constexpr auto stage_key() noexcept { return "order.parse"; }
  static constexpr auto stage_name() noexcept { return "parse"; }
  Parsed operator()(Raw raw) const { return {raw.value + 1}; }
};

struct StringViewName {
  constexpr operator std::string_view() const noexcept { return std::string_view{"converted.name"}; }
};

struct ViewName {
  [[nodiscard]] constexpr std::string_view view() const noexcept { return std::string_view{"view.name"}; }
};

struct CStrName {
  [[nodiscard]] constexpr const char* c_str() const noexcept { return "cstr.key"; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return std::string_view{"cstr.key"}.size(); }
};

struct ObjectNamed {
  using input_type = Parsed;
  using output_type = Parsed;
  static constexpr StringViewName name{};
  static constexpr CStrName key{};
};

struct ViewNamed {
  using input_type = Parsed;
  using output_type = Parsed;
  static constexpr ViewName name{};
};

struct Unnamed {
  using input_type = Parsed;
  using output_type = Parsed;
};

using Info = pb::stage_info<Parse>;
using ObjectNamedInfo = pb::stage_info<ObjectNamed>;
using ViewNamedInfo = pb::stage_info<ViewNamed>;
using UnnamedInfo = pb::stage_info<Unnamed>;

static_assert(Info::valid);
static_assert(UnnamedInfo::valid);
static_assert(!pb::stage_info<NotAStage>::valid);
static_assert(pb::stage_info<NotAStage>::name() == std::string_view{"<invalid>"});
static_assert(pb::stage_info<NotAStage>::key() == std::string_view{"<invalid>"});
static_assert(std::same_as<Info::stage_type, Parse>);
static_assert(std::same_as<Info::input_type, Raw>);
static_assert(std::same_as<Info::output_type, Parsed>);
static_assert(std::same_as<Info::error_type, ParseError>);
static_assert(Info::key() == std::string_view{"order.parse"});
static_assert(Info::name() == std::string_view{"parse"});
static_assert(ObjectNamedInfo::key() == std::string_view{"cstr.key"});
static_assert(ObjectNamedInfo::name() == std::string_view{"converted.name"});
static_assert(ViewNamedInfo::key() == std::string_view{"view.name"});
static_assert(ViewNamedInfo::name() == std::string_view{"view.name"});
static_assert(UnnamedInfo::key() == std::string_view{"<unnamed>"});
static_assert(UnnamedInfo::name() == std::string_view{"<unnamed>"});
static_assert(std::same_as<pb::stage_input_t<Parse>, Raw>);
static_assert(std::same_as<pb::stage_output_t<Parse>, Parsed>);
static_assert(std::same_as<pb::stage_error_t<Parse>, ParseError>);
static_assert(pb::stage_key<Parse>() == std::string_view{"order.parse"});
static_assert(pb::stage_name<Parse>() == std::string_view{"parse"});
static_assert(pb::stage_key<Unnamed>() == std::string_view{"<unnamed>"});
static_assert(pb::stage_name<Unnamed>() == std::string_view{"<unnamed>"});

int main() { return Info::name() == "parse" ? 0 : 1; }
