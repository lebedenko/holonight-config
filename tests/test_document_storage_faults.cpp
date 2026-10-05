#include "holonight/config/document.h"
#include "holonight/config/test_support.h"
#include "test.h"

#include <cerrno>
#include <fstream>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
enum class Fault : std::uint8_t { None, Write, Interrupt, FileSync, Rename, DirectorySync };
Fault fault = Fault::None;
}  // namespace
// Executable-owned syscall interposition injects storage failures without adding
// test hooks or environment-driven failure behavior to the production library.
extern "C" ssize_t write(int __fd, const void* __buf, size_t __n) {
  if (fault == Fault::Write) {
    errno = ENOSPC;
    return -1;
  }
  if (fault == Fault::Interrupt) {
    fault = Fault::None;
    errno = EINTR;
    return -1;
  }
  // Linux syscall is variadic; fixed arguments below match the kernel ABI.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return ::syscall(SYS_write, __fd, __buf, __n);
}
extern "C" int fsync(int __fd) {
  struct stat metadata{};
  if (::fstat(__fd, &metadata) != 0) {
    return -1;
  }
  if ((fault == Fault::DirectorySync && S_ISDIR(metadata.st_mode)) ||
      (fault == Fault::FileSync && S_ISREG(metadata.st_mode))) {
    errno = EIO;
    return -1;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return static_cast<int>(::syscall(SYS_fsync, __fd));
}
extern "C" int rename(const char* __old, const char* __new) {
  if (fault == Fault::Rename) {
    errno = EACCES;
    return -1;
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return static_cast<int>(::syscall(SYS_rename, __old, __new));
}

TEST_CASE(document_storage_failures_distinguish_before_and_after_replacement) {
  using namespace HoloNight::Config;
  for (const auto injected : {Fault::Write, Fault::FileSync, Fault::Rename, Fault::DirectorySync, Fault::Interrupt}) {
    TestSupport::TemporaryDirectory directory;
    const auto path = directory.child("config.toml");
    {
      std::ofstream file(path);
      file << "x=1\n";
    }
    fault = injected;
    const auto saved =
        saveDocument(path, {{.key = {"x"}, .baseline = Value{std::int64_t{1}}, .pending = Value{std::int64_t{2}}}});
    fault = Fault::None;
    const bool replaced = injected == Fault::DirectorySync || injected == Fault::Interrupt;
    EXPECT_EQ(saved.status, injected == Fault::DirectorySync ? SaveStatus::DurabilityFailure
                            : injected == Fault::Interrupt   ? SaveStatus::Success
                                                             : SaveStatus::StorageFailure);
    EXPECT_EQ(readDocument(path).value->revision.bytes, replaced ? "x=2\n" : "x=1\n");
    EXPECT_EQ(saved.snapshot.has_value(), replaced);
    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
      EXPECT_FALSE(entry.path().filename().string().find(".tmp-") != std::string::npos);
    }
  }
}
