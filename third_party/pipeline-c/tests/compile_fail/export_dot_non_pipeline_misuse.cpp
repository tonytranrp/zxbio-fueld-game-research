#include <pb/pipeline.hpp>

struct NotAPipeline {};

int main() {
  (void)pb::to_dot<NotAPipeline>("bad");
  return 0;
}
