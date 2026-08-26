#include <pb/pipeline.hpp>

struct Input { int route{}; };
struct Parsed {};
struct Reviewed {};
struct Done {};

struct IsParsed {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};

struct IsReviewed {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return false; }
};

struct Parse {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input) const { return {}; }
};

struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  Reviewed operator()(Input) const { return {}; }
};

struct MissingReviewedHandlerJoin {
  using input_type = pb::meta::type_list<Parsed, Reviewed>;
  using output_type = Done;
  Done operator()(Parsed) const { return {}; }
};

using ParsedCase = pb::case_<IsParsed>::then<Parse>;
using ReviewedCase = pb::case_<IsReviewed>::then<Review>;
using BadPipeline = pb::from<Input>::branch<ParsedCase, ReviewedCase>::join<MissingReviewedHandlerJoin>::to<Done>;

static_assert(pb::valid<BadPipeline>);
