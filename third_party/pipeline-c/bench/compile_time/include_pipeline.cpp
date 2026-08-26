#include <pb/pipeline.hpp>

int main() { return pb::meta::size_v<pb::meta::type_list<int, double>> == 2 ? 0 : 1; }
