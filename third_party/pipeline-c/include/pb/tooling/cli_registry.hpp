#pragma once

// ─────────────────────────────────────────────────────────────
// pb::tooling::pipeline_registry — type-erased CLI pipeline table
//
// A compiled CLI cannot compile arbitrary user source at runtime, so the
// realistic closure for "accept user-defined pipelines (not just the built-in
// examples)" is a clean REGISTRY abstraction: any pipeline *type* known at the
// CLI's own compile time can be registered with a single line, after which the
// CLI can `list` and `describe` it (text / json / dot) without a hard-coded
// switch.
//
// EXTENSION POINT (for downstream forks)
// --------------------------------------
// To add your own pipeline to a fork of pb_cli, write a validated pipeline type
// and register it once:
//
//   using my_pipeline = pb::from<In>::then<...>::to<Out>;
//
//   pb::tooling::pipeline_registry registry;
//   registry.add<my_pipeline>("my-name", "my-topology",
//                             "One-line human description.",
//                             "my_graph");   // optional DOT graph id
//
// `add<Pipeline>` captures the pipeline's export closures (pb::core::to_dot /
// pb::core::to_json / pb::core::to_text) into type-erased std::function entries, so the
// rest of the CLI works against `pipeline_registry` without ever naming the
// pipeline type again.  Lookups are by registration name; iteration order is
// insertion order so `list` output stays deterministic.
//
// Standard-library-only and header-only by design — registering a pipeline pulls
// in no dependency beyond the export helpers the pipeline already uses.
// ─────────────────────────────────────────────────────────────

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pb/core/export_dot.hpp"
#include "pb/core/export_json.hpp"
#include "pb/core/export_text.hpp"

namespace pb::tooling {

/// One registered pipeline: human-facing metadata plus the type-erased export
/// closures that render it in each supported format.
struct pipeline_entry {
  /// Render format selector accepted by `render` / the CLI `--format` flag.
  std::string name;        ///< Lookup name, e.g. "order-linear".
  std::string topology;    ///< Short topology tag, e.g. "linear" / "branch".
  std::string description; ///< One-line human description shown by `list`.

  /// Type-erased exporters captured at registration time.  Each renders the
  /// originally registered pipeline type; the registry never re-names the type.
  std::function<std::string(std::string_view)> to_dot;  ///< DOT (takes graph id).
  std::function<std::string()> to_json;                 ///< JSON.
  std::function<std::string()> to_text;                 ///< compact text.

  /// Default DOT graph identifier used when the caller does not override it.
  std::string graph_name;
};

/// Ordered, name-keyed table of pipelines the CLI can describe.
///
/// The table owns no pipeline *types* — only the export closures captured by
/// `add<Pipeline>`.  This is what lets a single compiled binary expose an
/// open-ended set of pipelines while staying standard-library-only.
class pipeline_registry {
public:
  /// Register a validated pipeline under @p name.
  ///
  /// @tparam Pipeline a `pb::ValidPipeline` (e.g. `pb::from<...>::to<...>`).
  /// @param name        lookup name surfaced by `list` / accepted by `describe`.
  /// @param topology    short topology tag for `list` display.
  /// @param description one-line human description for `list`.
  /// @param graph_name  DOT graph identifier (defaults to @p name with '-'→'_').
  template <class Pipeline>
  void add(std::string name, std::string topology, std::string description,
           std::string graph_name = {}) {
    if (graph_name.empty()) {
      graph_name = default_graph_name(name);
    }
    pipeline_entry entry;
    entry.name = std::move(name);
    entry.topology = std::move(topology);
    entry.description = std::move(description);
    entry.graph_name = std::move(graph_name);
    entry.to_dot = [](std::string_view graph) { return pb::core::to_dot<Pipeline>(graph); };
    entry.to_json = [] { return pb::core::to_json<Pipeline>(); };
    entry.to_text = [] { return pb::core::to_text<Pipeline>(); };
    entries_.push_back(std::move(entry));
  }

  /// @return pointer to the entry registered under @p name, or nullptr.
  [[nodiscard]] const pipeline_entry* find(std::string_view name) const noexcept {
    for (const auto& entry : entries_) {
      if (entry.name == name) {
        return &entry;
      }
    }
    return nullptr;
  }

  /// @return true if a pipeline named @p name is registered.
  [[nodiscard]] bool contains(std::string_view name) const noexcept {
    return find(name) != nullptr;
  }

  /// Render @p name in @p format ("dot" | "json" | "text").
  ///
  /// @return the rendered graph, or an empty string if the name is unknown or
  ///         the format is unrecognised.  DOT uses the entry's stored graph id.
  [[nodiscard]] std::string render(std::string_view name, std::string_view format) const {
    const auto* entry = find(name);
    if (entry == nullptr) {
      return {};
    }
    if (format == "dot") {
      return entry->to_dot(entry->graph_name);
    }
    if (format == "json") {
      return entry->to_json();
    }
    if (format == "text") {
      return entry->to_text();
    }
    return {};
  }

  /// Insertion-ordered view of every registered pipeline (for `list`).
  [[nodiscard]] const std::vector<pipeline_entry>& entries() const noexcept {
    return entries_;
  }

  /// @return number of registered pipelines.
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
  /// Derive a DOT-safe graph id from a registration name ('-' → '_').
  [[nodiscard]] static std::string default_graph_name(std::string_view name) {
    std::string graph{name};
    for (auto& ch : graph) {
      if (ch == '-') {
        ch = '_';
      }
    }
    return graph;
  }

  std::vector<pipeline_entry> entries_;
};

} // namespace pb::tooling
