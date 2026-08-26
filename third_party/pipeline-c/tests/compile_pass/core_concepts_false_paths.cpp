#include <pb/pipeline.hpp>

#include <string_view>

struct Raw {};
struct Parsed {};
struct Done {};
struct NotAStage {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  Parsed operator()(Raw) const { return {}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  Done operator()(Parsed) const { return {}; }
};

struct MetadataOnlyStage {
  using input_type = Raw;
  using output_type = Parsed;
};

struct TraitSpecializedStage {
  Parsed operator()(Raw) const { return {}; }
};

namespace pb::core {
template <>
struct stage_traits<::TraitSpecializedStage> {
  using input_type = ::Raw;
  using output_type = ::Parsed;
  using error_type = pb::no_error;

  static constexpr bool is_valid_stage = true;

  static constexpr std::string_view name() noexcept { return "trait_specialized_stage"; }
  static constexpr std::string_view key() noexcept { return "trait.specialized"; }
};
} // namespace pb::core

static_assert(pb::Stage<Parse>);
static_assert(pb::Stage<TraitSpecializedStage>);
static_assert(!pb::Stage<NotAStage>);

static_assert(pb::Connectable<Raw, Parse>);
static_assert(pb::Connectable<Raw, TraitSpecializedStage>);
static_assert(pb::connectable_v<Raw, Parse>);
static_assert(pb::connectable_v<Raw, TraitSpecializedStage>);
static_assert(!pb::Connectable<Parsed, Parse>);
static_assert(!pb::connectable_v<Parsed, Parse>);
static_assert(!pb::Connectable<Raw, NotAStage>);
static_assert(!pb::connectable_v<Raw, NotAStage>);

static_assert(pb::AdjacentStages<Parse, Finish>);
static_assert(pb::adjacent_stages_v<Parse, Finish>);
static_assert(!pb::AdjacentStages<Finish, Parse>);
static_assert(!pb::adjacent_stages_v<Finish, Parse>);
static_assert(!pb::AdjacentStages<NotAStage, Finish>);
static_assert(!pb::adjacent_stages_v<NotAStage, Finish>);
static_assert(!pb::AdjacentStages<Parse, NotAStage>);
static_assert(!pb::adjacent_stages_v<Parse, NotAStage>);

static_assert(pb::RunnableStage<Parse, Raw>);
static_assert(pb::runnable_stage_v<Parse, Raw>);
static_assert(!pb::RunnableStage<Parse, Parsed>);
static_assert(!pb::runnable_stage_v<Parse, Parsed>);
static_assert(!pb::RunnableStage<MetadataOnlyStage, Raw>);
static_assert(!pb::runnable_stage_v<MetadataOnlyStage, Raw>);
static_assert(!pb::RunnableStage<NotAStage, Raw>);
static_assert(!pb::runnable_stage_v<NotAStage, Raw>);

using TraitPipeline = pb::from<Raw>::then<TraitSpecializedStage>::to<Parsed>;
static_assert(pb::valid<TraitPipeline>);

int main() { return 0; }
