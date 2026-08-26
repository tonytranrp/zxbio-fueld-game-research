#include <pb/pipeline.hpp>
#include <string>

int main() {
  pb::runtime::detail::convert_error<int>(pb::runtime::error{.message = "mismatch"});
  return 0;
}
