#include <pb/pipeline.hpp>

struct NotAPipeline {};

int main() {
  (void)pb::to_text<NotAPipeline>();
  return 0;
}
