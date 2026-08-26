#include <pb/pipeline.hpp>

struct NotAPipeline {};

int main() {
  (void)pb::to_json<NotAPipeline>();
  return 0;
}
