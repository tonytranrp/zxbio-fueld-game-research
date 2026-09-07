#pragma once

// Which graphics backend a scenario -- or, since Prompt 004 goal 273, a single assertion -- applies
// to. Split out of scenario.hpp so assertion.hpp can name it without the two including each other.

#include <cstdint>
#include <string_view>

namespace dev::scenario {

enum class BackendSelection : std::uint8_t { Vulkan, D3D12, Both };

[[nodiscard]] bool parse_backend(std::string_view text, BackendSelection& out) noexcept;
[[nodiscard]] std::string_view backend_name(BackendSelection backend) noexcept;

} // namespace dev::scenario
