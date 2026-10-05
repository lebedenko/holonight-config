#include "holonight/config/document.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HoloNight::Config {
namespace {
bool requireDirectory(const std::filesystem::path& path, std::error_code& error) {
  if (std::filesystem::is_directory(path, error)) {
    return true;
  }
  if (!error) {
    error = std::make_error_code(std::errc::not_a_directory);
  }
  return false;
}

bool prependLinkTarget(const std::filesystem::path& candidate, std::filesystem::path& resolved,
                       std::deque<std::filesystem::path>& pending, std::error_code& error) {
  auto target = std::filesystem::read_symlink(candidate, error);
  if (error) {
    return false;
  }
  if (target.is_absolute()) {
    resolved = target.root_path();
    target = target.relative_path();
  }
  for (auto part = target.end(); part != target.begin();) {
    --part;
    pending.push_front(*part);
  }
  return true;
}

// weakly_canonical cannot resolve every dangling link. Walk components so writes
// can create the target while retaining an existing link, including directory links.
std::filesystem::path resolveTarget(const std::filesystem::path& path, std::error_code& error) {
  const auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return {};
  }
  auto resolved = absolute.root_path();
  const auto relative = absolute.relative_path();
  std::deque<std::filesystem::path> pending(relative.begin(), relative.end());
  int links = 0;
  constexpr int kMaximumLinks = 40;
  while (!pending.empty()) {
    const auto component = pending.front();
    pending.pop_front();
    if (component.empty()) {
      continue;
    }
    if (component == "." || component == "..") {
      if (!requireDirectory(resolved, error)) {
        return {};
      }
      if (component == "..") {
        resolved = resolved.parent_path();
      }
      continue;
    }
    const auto candidate = resolved / component;
    const auto status = std::filesystem::symlink_status(candidate, error);
    if (error == std::errc::no_such_file_or_directory) {
      error.clear();
    }
    if (error) {
      return {};
    }
    if (!std::filesystem::is_symlink(status)) {
      resolved = candidate;
      continue;
    }
    if (++links > kMaximumLinks) {
      error = std::make_error_code(std::errc::too_many_symbolic_link_levels);
      return {};
    }
    if (!prependLinkTarget(candidate, resolved, pending, error)) {
      return {};
    }
  }
  return resolved;
}

class Descriptor {
 public:
  explicit Descriptor(int value) : value_(value) {}
  ~Descriptor() {
    if (value_ >= 0) {
      ::close(value_);
    }
  }
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;
  Descriptor(Descriptor&&) = delete;
  Descriptor& operator=(Descriptor&&) = delete;
  [[nodiscard]] int get() const { return value_; }

 private:
  int value_;
};
class Temporary {
 public:
  Temporary() = default;
  Temporary(const Temporary&) = delete;
  Temporary& operator=(const Temporary&) = delete;
  Temporary(Temporary&&) = delete;
  Temporary& operator=(Temporary&&) = delete;
  std::filesystem::path path;
  ~Temporary() {
    if (!path.empty()) {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  }
};
SaveResult failure(SaveStatus status, std::string message, const std::filesystem::path& path) {
  SaveResult result;
  result.status = status;
  result.diagnostics.push_back({
      .code = ErrorCode::AtomicWriteError,
      .severity = Severity::Error,
      .message = std::move(message),
      .path = path,
      .position = std::nullopt,
  });
  return result;
}
SaveResult storageError(std::string_view operation, const std::filesystem::path& path) {
  const int error_number = errno;
  return failure(SaveStatus::StorageFailure, std::string{operation} + ": " + std::strerror(error_number), path);
}
SaveResult replaceDocument(const std::filesystem::path& path, const std::filesystem::path& target,
                           const DocumentSnapshot& current, SaveResult patched) {
  struct stat metadata{};
  mode_t mode = 0600;
  if (current.revision.exists) {
    if (::stat(target.c_str(), &metadata) != 0) {
      return storageError("inspect configuration permissions", path);
    }
    mode = metadata.st_mode & 07777;
  }
  Temporary temporary;
  std::error_code error;
  std::string pattern = (target.parent_path() / ("." + target.filename().string() + ".tmp-XXXXXX")).string();
  Descriptor output(::mkstemp(pattern.data()));
  if (output.get() < 0) {
    return storageError("create configuration temporary file", path);
  }
  temporary.path = pattern;
  const auto& bytes = patched.snapshot->revision.bytes;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto written = ::write(output.get(), std::string_view{bytes}.substr(offset).data(), bytes.size() - offset);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      return storageError("write configuration temporary file", path);
    }
    offset += static_cast<std::size_t>(written);
  }
  if (::fchmod(output.get(), mode) != 0) {
    return storageError("preserve configuration permissions", path);
  }
  if (::fsync(output.get()) != 0) {
    return storageError("sync configuration temporary file", path);
  }
  // POSIX open() is variadic even without O_CREAT.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  Descriptor directory(::open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (directory.get() < 0) {
    return storageError("open configuration directory", path);
  }
  // Check both exact bytes/existence and the symlink target immediately before replacement.
  const auto latest = readDocument(target);
  if (!latest || latest.value->revision != current.revision || resolveTarget(path, error) != target || error) {
    return failure(SaveStatus::RevisionChanged, "configuration changed before replacement", path);
  }
  struct stat latest_metadata{};
  if (current.revision.exists &&
      (::stat(target.c_str(), &latest_metadata) != 0 || latest_metadata.st_mode != metadata.st_mode ||
       latest_metadata.st_ino != metadata.st_ino || latest_metadata.st_dev != metadata.st_dev)) {
    return failure(SaveStatus::RevisionChanged, "configuration metadata changed before replacement", path);
  }
  if (::rename(temporary.path.c_str(), target.c_str()) != 0) {
    return storageError("replace configuration", path);
  }
  temporary.path.clear();
  patched.snapshot->path = path;
  if (::fsync(directory.get()) != 0) {
    auto result = storageError("sync configuration directory after replacement", path);
    result.status = SaveStatus::DurabilityFailure;
    result.snapshot = std::move(patched.snapshot);
    return result;
  }
  return patched;
}
}  // namespace

namespace {
SaveResult prepareRestore(const DocumentSnapshot& previous, const DocumentSchema& schema) {
  auto parsed = previous.revision.exists ? parseDocument(previous.revision.bytes, previous.path)
                                         : Result<DocumentSnapshot>::success(DocumentSnapshot{});
  SaveResult result;
  if (!parsed) {
    result.status = SaveStatus::Invalid;
    result.diagnostics = std::move(parsed.diagnostics);
    return result;
  }
  result.diagnostics = validateDocument(*parsed.value, schema);
  const bool invalid =
      std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.severity == Severity::Error; });
  result.status = invalid ? SaveStatus::Invalid : SaveStatus::Success;
  if (!invalid) {
    result.snapshot = std::move(parsed.value);
  }
  return result;
}

SaveResult removeDocument(const std::filesystem::path& path, const std::filesystem::path& target,
                          const DocumentSnapshot& current, SaveResult result) {
  if (!current.revision.exists) {
    return result;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  Descriptor directory(::open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (directory.get() < 0) {
    return storageError("open rollback directory", path);
  }
  std::error_code error;
  const auto latest = readDocument(target);
  if (!latest || latest.value->revision != current.revision || resolveTarget(path, error) != target || error) {
    return failure(SaveStatus::RevisionChanged, "configuration changed before rollback removal", path);
  }
  if (::unlink(target.c_str()) != 0) {
    return storageError("remove staged configuration", path);
  }
  result.snapshot->path = path;
  if (::fsync(directory.get()) != 0) {
    auto failed = storageError("sync directory after rollback removal", path);
    failed.status = SaveStatus::DurabilityFailure;
    failed.snapshot = std::move(result.snapshot);
    return failed;
  }
  return result;
}
}  // namespace

namespace {
bool rollbackTargetMatches(const std::filesystem::path& target, const DocumentSnapshot* previous) {
  if (previous == nullptr) {
    return true;
  }
  if (previous->path.empty()) {
    return false;
  }
  std::error_code error;
  return std::filesystem::absolute(previous->path, error).lexically_normal() == target && !error;
}
SaveResult updateDocument(const std::filesystem::path& path, const EditBatch& edits, const DocumentSchema& schema,
                          const DocumentRevision* staged, const DocumentSnapshot* previous,
                          std::optional<DocumentSnapshot>* before = nullptr) {
  if (path.empty() || path.filename().empty()) {
    return failure(SaveStatus::StorageFailure, "empty configuration destination", path);
  }
  std::error_code error;
  const auto target = resolveTarget(path, error);
  if (error) {
    return failure(SaveStatus::StorageFailure, error.message(), path);
  }
  if (!rollbackTargetMatches(target, previous)) {
    return failure(SaveStatus::RevisionChanged, "staged configuration target changed; rollback refused", path);
  }
  auto initial = readDocument(target);
  if (!initial) {
    SaveResult result;
    result.diagnostics = std::move(initial.diagnostics);
    result.status =
        std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == ErrorCode::IoError; })
            ? SaveStatus::StorageFailure
            : SaveStatus::Invalid;
    return result;
  }
  std::filesystem::create_directories(target.parent_path(), error);
  if (error) {
    return failure(SaveStatus::StorageFailure, error.message(), path);
  }
  const auto lock_path = target.string() + ".lock";
  // open() requires a mode argument with O_CREAT.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  Descriptor lock(::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600));
  if (lock.get() < 0) {
    return storageError("open configuration lock", path);
  }
  while (::flock(lock.get(), LOCK_EX) != 0) {
    if (errno != EINTR) {
      return storageError("lock configuration", path);
    }
  }
  if (resolveTarget(path, error) != target || error) {
    return failure(SaveStatus::RevisionChanged, "configuration target changed", path);
  }
  auto current = readDocument(target);
  if (!current) {
    SaveResult result;
    result.diagnostics = std::move(current.diagnostics);
    result.status =
        std::ranges::any_of(result.diagnostics, [](const auto& item) { return item.code == ErrorCode::IoError; })
            ? SaveStatus::StorageFailure
            : SaveStatus::Invalid;
    return result;
  }
  if (staged != nullptr && current.value->revision != *staged) {
    return failure(SaveStatus::RevisionChanged, "staged document changed; rollback refused", path);
  }
  auto patched = previous == nullptr ? patchDocument(*current.value, edits, schema) : prepareRestore(*previous, schema);
  if (previous != nullptr && !previous->revision.exists && patched.status == SaveStatus::Success) {
    return removeDocument(path, target, *current.value, std::move(patched));
  }
  if (patched.status != SaveStatus::Success) {
    return patched;
  }
  if (before != nullptr) {
    *before = *current.value;
  }
  if (patched.snapshot->revision.bytes == current.value->revision.bytes) {
    // An empty batch should neither create a missing file nor replace an unchanged one.
    patched.snapshot = std::move(current.value);
    return patched;
  }
  return replaceDocument(path, target, *current.value, std::move(patched));
}
}  // namespace

Result<std::filesystem::path> resolveDocumentTarget(const std::filesystem::path& path) {
  if (path.empty()) {
    return Result<std::filesystem::path>::failure({
        {
            .code = ErrorCode::PathUnavailable,
            .severity = Severity::Error,
            .message = "empty configuration path",
            .path = path,
            .position = std::nullopt,
        },
    });
  }
  std::error_code error;
  auto target = resolveTarget(path, error);
  if (error) {
    return Result<std::filesystem::path>::failure({
        {
            .code = ErrorCode::IoError,
            .severity = Severity::Error,
            .message = error.message(),
            .path = path,
            .position = std::nullopt,
        },
    });
  }
  return Result<std::filesystem::path>::success(std::move(target));
}

SaveResult saveDocument(const std::filesystem::path& path, const EditBatch& edits, const DocumentSchema& schema) {
  return updateDocument(path, edits, schema, nullptr, nullptr);
}
StagedSaveResult stageDocument(const std::filesystem::path& path, const EditBatch& edits,
                               const DocumentSchema& schema) {
  StagedSaveResult staged;
  staged.result = updateDocument(path, edits, schema, nullptr, nullptr, &staged.previous);
  if (staged.result.status != SaveStatus::Success && staged.result.status != SaveStatus::DurabilityFailure) {
    staged.previous.reset();
  }
  return staged;
}
SaveResult restoreDocument(const std::filesystem::path& path, const DocumentRevision& staged,
                           const DocumentSnapshot& previous, const DocumentSchema& schema) {
  return updateDocument(path, {}, schema, &staged, &previous);
}
}  // namespace HoloNight::Config
