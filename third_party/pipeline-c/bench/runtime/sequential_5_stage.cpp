#include <pb/pipeline.hpp>

struct V { int value{}; };
struct S1 { using input_type = V; using output_type = V; V operator()(V v) const { return {v.value + 1}; } };
struct S2 { using input_type = V; using output_type = V; V operator()(V v) const { return {v.value + 1}; } };
struct S3 { using input_type = V; using output_type = V; V operator()(V v) const { return {v.value + 1}; } };
struct S4 { using input_type = V; using output_type = V; V operator()(V v) const { return {v.value + 1}; } };
struct S5 { using input_type = V; using output_type = V; V operator()(V v) const { return {v.value + 1}; } };
using P = pb::from<V>::then<S1>::then<S2>::then<S3>::then<S4>::then<S5>::to<V>;

int main() {
  auto out = pb::compile<P>(pb::runtime::sequential{}).run(V{0});
  return out.value == 5 ? 0 : 1;
}
