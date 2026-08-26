#include <pb/pipeline.hpp>

struct NotAPipeline {};

int main() {
  (void)pb::make_descriptor<NotAPipeline>();
  return 0;
}
