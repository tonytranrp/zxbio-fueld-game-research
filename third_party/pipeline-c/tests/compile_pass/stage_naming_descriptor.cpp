#include <pb/pipeline.hpp>

#include <cstddef>
#include <string_view>
#include <type_traits>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };

struct StringViewName {
  constexpr operator std::string_view() const noexcept { return std::string_view{"parse.name"}; }
};

struct ViewName {
  [[nodiscard]] constexpr std::string_view view() const noexcept { return std::string_view{"finish.name"}; }
};

struct CStrKey {
  [[nodiscard]] constexpr const char* c_str() const noexcept { return "parse.key"; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return std::string_view{"parse.key"}.size(); }
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr StringViewName name{};
  static constexpr CStrKey key{};
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  static constexpr ViewName name{};
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;
using ParseDescriptor = pb::pipeline_stage_descriptor_t<Pipeline, 0>;
using FinishDescriptor = pb::pipeline_stage_descriptor_t<Pipeline, 1>;
using EdgeDescriptor = pb::pipeline_edge_descriptor_t<Pipeline, 0>;

static_assert(pb::valid<Pipeline>);
static_assert(ParseDescriptor::key() == std::string_view{"parse.key"});
static_assert(ParseDescriptor::name() == std::string_view{"parse.name"});
static_assert(FinishDescriptor::key() == std::string_view{"finish.name"});
static_assert(FinishDescriptor::name() == std::string_view{"finish.name"});
static_assert(EdgeDescriptor::from_key() == std::string_view{"parse.key"});
static_assert(EdgeDescriptor::from_name() == std::string_view{"parse.name"});
static_assert(EdgeDescriptor::to_key() == std::string_view{"finish.name"});
static_assert(EdgeDescriptor::to_name() == std::string_view{"finish.name"});
static_assert(pb::edge_from_key<Pipeline, 0>() == std::string_view{"parse.key"});
static_assert(pb::edge_from_name<Pipeline, 0>() == std::string_view{"parse.name"});
static_assert(pb::edge_to_key<Pipeline, 0>() == std::string_view{"finish.name"});
static_assert(pb::edge_to_name<Pipeline, 0>() == std::string_view{"finish.name"});
static_assert(pb::describe<Pipeline>().stage_key<0>() == std::string_view{"parse.key"});
static_assert(pb::describe<Pipeline>().stage_name<0>() == std::string_view{"parse.name"});
static_assert(pb::describe<Pipeline>().stage_key<1>() == std::string_view{"finish.name"});
static_assert(pb::describe<Pipeline>().stage_name<1>() == std::string_view{"finish.name"});
static_assert(pb::descriptor_view<Pipeline>().stage_records()[0].key == std::string_view{"parse.key"});
static_assert(pb::descriptor_view<Pipeline>().stage_records()[0].name == std::string_view{"parse.name"});
static_assert(pb::descriptor_view<Pipeline>().stage_records()[1].key == std::string_view{"finish.name"});
static_assert(pb::descriptor_view<Pipeline>().stage_records()[1].name == std::string_view{"finish.name"});
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].from_key == std::string_view{"parse.key"});
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].from_name == std::string_view{"parse.name"});
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].to_key == std::string_view{"finish.name"});
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].to_name == std::string_view{"finish.name"});
static_assert(std::same_as<ParseDescriptor::stage_type, Parse>);
static_assert(std::same_as<FinishDescriptor::stage_type, Finish>);

int main() {
  constexpr auto descriptor = pb::descriptor_view<Pipeline>();
  return descriptor.stage_records()[0].key == std::string_view{"parse.key"} ? 0 : 1;
}
