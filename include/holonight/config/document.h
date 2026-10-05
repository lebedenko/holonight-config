#pragma once

#include "holonight/config/result.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <variant>

namespace HoloNight::Config {

using KeyPath = std::vector<std::string>;
// Aggregate literals are parsed by toml++; arrays/AoT are one conflict value.
struct AggregateValue {
  std::string toml;
  bool operator==(const AggregateValue&) const = default;
};
using Value = std::variant<bool, std::int64_t, double, std::string, AggregateValue>;

struct SourceSpan {
  std::size_t begin{};
  std::size_t end{};
  bool operator==(const SourceSpan&) const = default;
};
struct DocumentEntry {
  Value value;
  SourceSpan source;
  std::optional<SourceSpan> assignment;
};
struct DocumentRevision {
  bool exists{false};
  // Exact content comparison avoids hash collisions and distinguishes empty/missing.
  std::string bytes;
  bool operator==(const DocumentRevision&) const = default;
};
struct DocumentSnapshot {
  std::filesystem::path path;
  DocumentRevision revision;
  std::map<KeyPath, DocumentEntry> overrides;
  [[nodiscard]] std::optional<Value> value(const KeyPath& key) const;
};

enum class ReloadPolicy : std::uint8_t { Live, Restart };
struct SchemaField {
  KeyPath key;
  std::optional<Value> default_value;
  std::string description;
  ReloadPolicy reload{ReloadPolicy::Live};
  std::function<std::vector<Diagnostic>(const Value&)> validate;
};
struct DocumentSchema {
  std::vector<SchemaField> fields;
  // Also validates dynamic collections, related values and unknown-field policy.
  std::function<std::vector<Diagnostic>(const DocumentSnapshot&)> validate_domain;
};
struct Edit {
  KeyPath key;
  std::optional<Value> baseline;
  // nullopt removes an override; absence differs from an explicit default.
  std::optional<Value> pending;
};
using EditBatch = std::vector<Edit>;
struct Conflict {
  KeyPath key;
  std::optional<Value> baseline;
  std::optional<Value> disk;
  std::optional<Value> pending;
};
enum class SaveStatus : std::uint8_t {
  Success,
  Conflict,
  Invalid,
  UnsupportedPatch,
  StorageFailure,
  DurabilityFailure,
  RevisionChanged,
};
struct SaveResult {
  SaveStatus status{SaveStatus::StorageFailure};
  // DurabilityFailure includes the snapshot that was already replaced on disk.
  std::optional<DocumentSnapshot> snapshot;
  std::vector<Conflict> conflicts;
  std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] Result<DocumentSnapshot> parseDocument(std::string_view bytes, const std::filesystem::path& path = {});
// Missing succeeds with exists=false; unreadable/invalid fails and never creates a file.
[[nodiscard]] Result<DocumentSnapshot> readDocument(const std::filesystem::path& path);
[[nodiscard]] std::vector<Diagnostic> validateDocument(const DocumentSnapshot& snapshot, const DocumentSchema& schema);
[[nodiscard]] SaveResult patchDocument(const DocumentSnapshot& current, const EditBatch& edits,
                                       const DocumentSchema& schema = {});
// Cooperating writers lock the canonical target's stable sibling .lock file.
// Arbitrary editors can still race between the final revision check and rename.
[[nodiscard]] SaveResult saveDocument(const std::filesystem::path& path, const EditBatch& edits,
                                      const DocumentSchema& schema = {});

// Conditional rollback for staged application: restores exact prior bytes/existence
// only while the staged revision is current, under the same writer lock.
[[nodiscard]] SaveResult restoreDocument(const std::filesystem::path& path, const DocumentRevision& staged,
                                         const DocumentSnapshot& previous, const DocumentSchema& schema = {});

}  // namespace HoloNight::Config
