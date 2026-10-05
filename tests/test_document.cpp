#include "holonight/config/appearance_document.h"
#include "holonight/config/codec.h"
#include "holonight/config/document.h"
#include "holonight/config/store.h"
#include "holonight/config/test_support.h"
#include "test.h"

#include <array>
#include <fstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace HoloNight::Config;
namespace {
DocumentSnapshot snapshot(std::string_view bytes) {
  auto parsed = parseDocument(bytes);
  EXPECT_TRUE(parsed);
  return *parsed.value;
}
Edit set(const DocumentSnapshot& baseline, const KeyPath& key, Value value) {
  return {.key = key, .baseline = baseline.value(key), .pending = std::move(value)};
}
void write(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream file(path, std::ios::binary);
  file << bytes;
  EXPECT_TRUE(file.good());
}
}  // namespace

TEST_CASE(document_preserves_unrelated_bytes_and_value_comments) {
  const std::string original =
      "# heading\r\n\"quoted.key\" = 0x10 # keep\r\n[theme] # section\r\n"
      "accent  =  'blue' # accent\r\nunknown = { x = 1, y = '☾' }\r\n"
      "list = [1, # item\r\n 2]\r\ntext = '''first\r\n[not.a.header]\r\nlast'''\r\n"
      "[[items]]\r\nname = 'one'\r\n[[items]]\r\nname = 'two'\r\n";
  const auto baseline = snapshot(original);
  const auto saved = patchDocument(baseline, {
                                                 set(baseline, {"theme", "accent"}, std::string{"red"}),
                                                 set(baseline, {"quoted.key"}, std::int64_t{17}),
                                             });
  EXPECT_EQ(saved.status, SaveStatus::Success);
  auto expected = original;
  expected.replace(expected.find("0x10"), 4, "17");
  expected.replace(expected.find("'blue'"), 6, "'red'");
  // toml++ emits literal strings when no escaping is required.
  EXPECT_EQ(saved.snapshot->revision.bytes, expected);
}

TEST_CASE(document_unicode_source_columns_and_multiline_values) {
  const auto baseline = snapshot("\"☾\" = 'old' # Unicode key\nx = \"\"\"line\nline\"\"\" # text\ny=1\n");
  const auto saved =
      patchDocument(baseline, {set(baseline, {"☾"}, std::string{"new"}), set(baseline, {"x"}, std::string{"single"})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->revision.bytes, "\"☾\" = 'new' # Unicode key\nx = 'single' # text\ny=1\n");
}

TEST_CASE(document_reset_retains_comments_and_sections) {
  const auto baseline = snapshot("# above\n[theme] # section\n  accent = 'blue' # user note\nother=1\n");
  const auto saved = patchDocument(
      baseline,
      {{.key = {"theme", "accent"}, .baseline = baseline.value({"theme", "accent"}), .pending = std::nullopt}});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->revision.bytes, "# above\n[theme] # section\n # user note\nother=1\n");
}

TEST_CASE(document_insertions_decode_quoted_section_paths) {
  const auto baseline = snapshot("['a.b'] # heading\r\nx=1\r\n");
  auto saved = patchDocument(baseline, {set(baseline, {"a.b", "y.z"}, true)});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->revision.bytes, "['a.b'] # heading\r\n'y.z' = true\r\nx=1\r\n");
  saved = patchDocument(baseline, {set(baseline, {"new", "value"}, std::int64_t{7})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->value({"new", "value"}), std::optional<Value>{std::int64_t{7}});
}

TEST_CASE(document_inline_tables_and_arrays_are_whole_values) {
  const auto baseline = snapshot("inline = { x=1, y=2 } # untouched\narray=[1,2] # list\n");
  auto saved = patchDocument(baseline, {set(baseline, {"array"}, AggregateValue{"[3, 4]"})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_TRUE(saved.snapshot->revision.bytes.starts_with("inline = { x=1, y=2 } # untouched\n"));
  saved = patchDocument(baseline, {set(baseline, {"inline", "x"}, std::int64_t{3})});
  EXPECT_EQ(saved.status, SaveStatus::UnsupportedPatch);
  EXPECT_FALSE(saved.snapshot);
}

TEST_CASE(document_unsupported_aot_patch_and_bad_literals_fail_unchanged) {
  TestSupport::TemporaryDirectory directory;
  auto path = directory.child("config.toml");
  const std::string original = "[[items]]\nx=1\n[[items]]\nx=2\n";
  write(path, original);
  const auto baseline = *readDocument(path).value;
  const auto saved = saveDocument(path, {set(baseline, {"items"}, AggregateValue{"[{x=3}]"})});
  EXPECT_EQ(saved.status, SaveStatus::UnsupportedPatch);
  EXPECT_EQ(readDocument(path).value->revision.bytes, original);
  const auto bad = patchDocument(
      snapshot("x=1\n"), {{.key = {"x"}, .baseline = Value{std::int64_t{1}}, .pending = Value{AggregateValue{"["}}}});
  EXPECT_EQ(bad.status, SaveStatus::UnsupportedPatch);
}

TEST_CASE(document_three_way_merge_converges_and_reports_conflict_values) {
  const auto baseline = snapshot("x=1\ny=2\n");
  const auto current = snapshot("x=1\ny=3 # external\n");
  auto saved = patchDocument(current, {set(baseline, {"x"}, std::int64_t{4})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->revision.bytes, "x=4\ny=3 # external\n");
  saved = patchDocument(current, {set(baseline, {"y"}, std::int64_t{3})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  saved = patchDocument(current, {set(baseline, {"y"}, std::int64_t{4})});
  EXPECT_EQ(saved.status, SaveStatus::Conflict);
  EXPECT_EQ(saved.conflicts.size(), 1U);
  EXPECT_EQ(saved.conflicts.front().baseline, baseline.value({"y"}));
  EXPECT_EQ(saved.conflicts.front().disk, current.value({"y"}));
  EXPECT_EQ(saved.conflicts.front().pending, std::optional<Value>{std::int64_t{4}});
  saved = patchDocument(current, {{.key = {"y"}, .baseline = baseline.value({"y"}), .pending = std::nullopt}});
  EXPECT_EQ(saved.status, SaveStatus::Conflict);
}

TEST_CASE(document_schema_rejects_invalid_current_and_candidate_values) {
  DocumentSchema schema{
      .fields =
          {
              {
                  .key = {"x"},
                  .default_value = Value{std::int64_t{1}},
                  .description = "positive",
                  .reload = ReloadPolicy::Live,
                  .validate =
                      [](const Value& value) {
                        if (std::get<std::int64_t>(value) > 0) {
                          return std::vector<Diagnostic>{};
                        }
                        return std::vector<Diagnostic>{{.message = "must be positive"}};
                      },
              },
          },
      .validate_domain = {},
  };
  const auto baseline = snapshot("x=1\n");
  EXPECT_EQ(patchDocument(baseline, {set(baseline, {"x"}, std::int64_t{-1})}, schema).status, SaveStatus::Invalid);
  EXPECT_EQ(patchDocument(snapshot("x=-1\n"), {}, schema).status, SaveStatus::Invalid);
}

TEST_CASE(document_missing_is_distinct_from_unreadable_and_does_not_create) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("missing/config.toml");
  const auto missing = readDocument(path);
  EXPECT_TRUE(missing);
  EXPECT_FALSE(missing.value->revision.exists);
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_FALSE(readDocument(directory.path()));
  const auto saved = saveDocument(path, {{.key = {"x"}, .baseline = std::nullopt, .pending = Value{std::int64_t{1}}}});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  struct stat metadata{};
  EXPECT_EQ(::stat(path.c_str(), &metadata), 0);
  EXPECT_EQ(metadata.st_mode & 0777, 0600);
}

TEST_CASE(document_saves_preserve_permissions_and_follow_symlinks) {
  TestSupport::TemporaryDirectory directory;
  const auto target = directory.child("target.toml");
  const auto link = directory.child("link.toml");
  write(target, "x=1\n");
  EXPECT_EQ(::chmod(target.c_str(), 0640), 0);
  std::filesystem::create_symlink(target, link);
  auto baseline = *readDocument(link).value;
  auto saved = saveDocument(link, {set(baseline, {"x"}, std::int64_t{2})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_TRUE(std::filesystem::is_symlink(link));
  EXPECT_EQ(readDocument(target).value->revision.bytes, "x=2\n");
  struct stat metadata{};
  EXPECT_EQ(::stat(target.c_str(), &metadata), 0);
  EXPECT_EQ(metadata.st_mode & 0777, 0640);
  std::filesystem::create_symlink(directory.child("absent.toml"), directory.child("dangling.toml"));
  EXPECT_FALSE(readDocument(directory.child("dangling.toml")));
}

TEST_CASE(document_cooperating_processes_merge_unrelated_edits) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("config.toml");
  write(path, "x=1\ny=1\n");
  const auto baseline = *readDocument(path).value;
  std::array<int, 2> gate{};
  EXPECT_EQ(::pipe(gate.data()), 0);
  const auto child = ::fork();
  EXPECT_TRUE(child >= 0);
  if (child == 0) {
    ::close(gate[1]);
    char signal = 0;
    if (::read(gate[0], &signal, 1) != 1) {
      ::_exit(2);
    }
    const auto saved = saveDocument(path, {set(baseline, {"x"}, std::int64_t{2})});
    ::_exit(saved.status == SaveStatus::Success ? 0 : 1);
  }
  ::close(gate[0]);
  EXPECT_EQ(::write(gate[1], "x", 1), 1);
  ::close(gate[1]);
  const auto saved = saveDocument(path, {set(baseline, {"y"}, std::int64_t{2})});
  int status = 0;
  EXPECT_EQ(::waitpid(child, &status, 0), child);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  EXPECT_EQ(saved.status, SaveStatus::Success);
  const auto current = *readDocument(path).value;
  EXPECT_EQ(current.value({"x"}), std::optional<Value>{std::int64_t{2}});
  EXPECT_EQ(current.value({"y"}), std::optional<Value>{std::int64_t{2}});
}

TEST_CASE(appearance_v2_sparse_defaults_unknown_warnings_and_strict_known_values) {
  const auto decoded = decodeAppearanceDocument(snapshot("version=2\n[theme]\naccent='red'\nunknown=7\n"));
  EXPECT_TRUE(decoded);
  EXPECT_EQ(decoded.value->document_version, 2);
  EXPECT_EQ(decoded.value->appearance.version, 1);
  EXPECT_EQ(decoded.value->appearance.theme.accent, "red");
  EXPECT_EQ(decoded.value->appearance.typography, defaults().typography);
  EXPECT_EQ(decoded.diagnostics.size(), 1U);
  EXPECT_EQ(decoded.diagnostics.front().severity, Severity::Warning);
  EXPECT_FALSE(decodeAppearanceDocument(snapshot("version=2\n[typography]\nui_size=2\n")));
  EXPECT_FALSE(decodeAppearanceDocument(snapshot("version=2\n[theme]\naccent=7\n")));
  EXPECT_FALSE(decodeAppearanceDocument(snapshot("version=3\n")));
  EXPECT_FALSE(parse("version=2\n"));  // Explicit v1 API remains strict.
}

TEST_CASE(appearance_first_save_upgrade_and_reset_preserve_explicit_values) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  const std::string original = "# custom appearance\n" + *serialize(defaults()).value;
  write(path, original);
  const auto baseline = *readDocument(path).value;
  const auto saved = saveAppearanceDocument(path, {set(baseline, {"theme", "accent"}, std::string{"red"})});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  auto expected = original;
  expected.replace(expected.find("version = 1"), 11, "version = 2");
  expected.replace(expected.find("accent = \"blue\""), 15, "accent = 'red'");
  EXPECT_EQ(saved.snapshot->revision.bytes, expected);
  const auto reset = saveAppearanceDocument(
      path,
      {{.key = {"theme", "accent"}, .baseline = saved.snapshot->value({"theme", "accent"}), .pending = std::nullopt}});
  EXPECT_EQ(reset.status, SaveStatus::Success);
  EXPECT_EQ(readAppearanceDocument(path).value->appearance.theme.accent, "blue");
  EXPECT_EQ(readAppearanceDocument(path).value->appearance.typography, defaults().typography);
}

TEST_CASE(appearance_new_document_is_sparse_v2_and_unsupported_version_is_read_only) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  const auto saved = saveAppearanceDocument(
      path, {{.key = {"theme", "accent"}, .baseline = std::nullopt, .pending = Value{std::string{"red"}}}});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->overrides.size(), 2U);
  EXPECT_EQ(readAppearanceDocument(path).value->document_version, 2);
  write(path, "version=99\n# preserve\n");
  EXPECT_EQ(saveAppearanceDocument(path, {}).status, SaveStatus::Invalid);
  EXPECT_EQ(readDocument(path).value->revision.bytes, "version=99\n# preserve\n");
}

TEST_CASE(document_detects_revision_changes_during_save) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("config.toml");
  write(path, "x=1\n");
  const auto baseline = *readDocument(path).value;
  int validations = 0;
  DocumentSchema schema;
  schema.validate_domain = [&](const DocumentSnapshot&) {
    if (++validations == 2) {
      write(path, "x=8 # external\n");
    }
    return std::vector<Diagnostic>{};
  };
  const auto saved = saveDocument(path, {set(baseline, {"x"}, std::int64_t{2})}, schema);
  EXPECT_EQ(saved.status, SaveStatus::RevisionChanged);
  EXPECT_EQ(readDocument(path).value->revision.bytes, "x=8 # external\n");
}

TEST_CASE(document_detects_symlink_retargeting_during_save) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("link.toml");
  const auto first = directory.child("first.toml");
  const auto second = directory.child("second.toml");
  write(first, "x=1\n");
  write(second, "x=9\n");
  std::filesystem::create_symlink(first, path);
  const auto baseline = *readDocument(path).value;
  int validations = 0;
  DocumentSchema schema;
  schema.validate_domain = [&](const DocumentSnapshot&) {
    if (++validations == 2) {
      std::filesystem::remove(path);
      std::filesystem::create_symlink(second, path);
    }
    return std::vector<Diagnostic>{};
  };
  const auto saved = saveDocument(path, {set(baseline, {"x"}, std::int64_t{2})}, schema);
  EXPECT_EQ(saved.status, SaveStatus::RevisionChanged);
  EXPECT_EQ(readDocument(first).value->revision.bytes, "x=1\n");
  EXPECT_EQ(readDocument(path).value->revision.bytes, "x=9\n");
}

TEST_CASE(document_reset_preserves_embedded_comments_but_not_string_hashes) {
  const auto baseline = snapshot("x=[\n 1, # keep inner\n '# string value',\n 2] # keep trailing\ny=3\n");
  const auto saved =
      patchDocument(baseline, {{.key = {"x"}, .baseline = baseline.value({"x"}), .pending = std::nullopt}});
  EXPECT_EQ(saved.status, SaveStatus::Success);
  EXPECT_EQ(saved.snapshot->revision.bytes, "# keep inner\n # keep trailing\ny=3\n");
}

TEST_CASE(document_unreadable_files_and_permission_failures_do_not_replace) {
  using namespace HoloNight::Config;
  if (::geteuid() == 0) {
    return;  // Real permission checks require an unprivileged process.
  }
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("config.toml");
  write(path, "x=1\n");
  EXPECT_EQ(::chmod(path.c_str(), 0000), 0);
  EXPECT_FALSE(readDocument(path));
  EXPECT_EQ(saveDocument(path, {{{"x"}, Value{std::int64_t{1}}, Value{std::int64_t{2}}}}).status,
            SaveStatus::StorageFailure);
  EXPECT_EQ(::chmod(path.c_str(), 0600), 0);
  EXPECT_EQ(::chmod(directory.path().c_str(), 0500), 0);
  const auto saved =
      saveDocument(path, {{.key = {"x"}, .baseline = Value{std::int64_t{1}}, .pending = Value{std::int64_t{2}}}});
  EXPECT_EQ(::chmod(directory.path().c_str(), 0700), 0);
  EXPECT_EQ(saved.status, SaveStatus::StorageFailure);
  EXPECT_EQ(readDocument(path).value->revision.bytes, "x=1\n");
}

TEST_CASE(appearance_legacy_load_accepts_v2_with_unchanged_effective_model_contract) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  write(path, "version=2\n[layout]\nscale=2\n");
  const auto loaded = load(path);
  EXPECT_TRUE(loaded);
  EXPECT_EQ(loaded.value->appearance.version, 1);
  EXPECT_EQ(loaded.value->appearance.layout.scale, 2.0);
  EXPECT_TRUE(saveAppearanceDocument(path, {{{"theme", "accent"}, std::nullopt, Value{std::string{"red"}}}}).status ==
              SaveStatus::Success);
}

TEST_CASE(document_guarded_rollback_restores_exact_bytes_and_refuses_external_changes) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  write(path, "version=2\n[theme] # heading\naccent = \"blue\" # note\n");
  const auto previous = *readDocument(path).value;
  const auto staged = saveAppearanceDocument(path, {set(previous, {"theme", "accent"}, std::string{"red"})});
  EXPECT_EQ(staged.status, SaveStatus::Success);
  EXPECT_EQ(restoreDocument(path, staged.snapshot->revision, previous, appearanceDocumentSchema()).status,
            SaveStatus::Success);
  EXPECT_EQ(readDocument(path).value->revision, previous.revision);
  const auto staged_again = saveAppearanceDocument(path, {set(previous, {"theme", "accent"}, std::string{"red"})});
  write(path, "version=2\n[theme]\naccent='green'\n");
  EXPECT_EQ(restoreDocument(path, staged_again.snapshot->revision, previous, appearanceDocumentSchema()).status,
            SaveStatus::RevisionChanged);
  EXPECT_EQ(readAppearanceDocument(path).value->appearance.theme.accent, "green");
}

TEST_CASE(document_guarded_rollback_restores_original_absence) {
  TestSupport::TemporaryDirectory directory;
  const auto path = directory.child("appearance.toml");
  const auto previous = *readDocument(path).value;
  const auto staged = saveAppearanceDocument(
      path, {{.key = {"theme", "accent"}, .baseline = std::nullopt, .pending = Value{std::string{"red"}}}});
  EXPECT_EQ(staged.status, SaveStatus::Success);
  EXPECT_EQ(restoreDocument(path, staged.snapshot->revision, previous, appearanceDocumentSchema()).status,
            SaveStatus::Success);
  EXPECT_FALSE(std::filesystem::exists(path));
}
