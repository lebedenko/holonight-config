# Configuration architecture: holonight-config

Work package: CA-001. Upstream baseline: `b4b0a2482979338079699198d4f4249b6e78e338`.

## Scope

Neutral documents, schemas, preserving edits, locked storage and appearance v2.

Follow the accepted [shared contract](../../../../docs/initiatives/configuration-architecture/README.md).
Keep this repository independently buildable; do not modify another repository in its implementation commit.

## Design and acceptance

Each application owns its configuration schema, file, settings UI and behavior. Global appearance is shared;
application preferences are separate. Files and Viewer must remain usable without Shell or Settings. AI and Packages
retain their own configuration. Infrastructure requires neither Shell, Settings nor a running daemon.

Snapshots retain original bytes, parsed typed values, override presence, source spans and content revisions.
Paths are vectors of key segments, including quoted keys containing dots. Schemas declare typed defaults,
constraints, descriptions and reload policy, with domain validators for related values and dynamic collections.
Edit batches carry set/remove operations and baseline values/presence. Save outcomes distinguish success,
per-key conflicts, invalid documents/edits, unsupported patches, pre-replacement storage failures and
post-replacement durability failures.

Use toml++ and a TOML-aware lexical editor; never serialize an existing document wholesale or substitute by regex.
Preserve unrelated bytes, comments, ordering, whitespace and unknown fields. Reset removes an assignment and retains
comments/sections. Arrays and arrays of tables are whole conflict values. Unsupported safe patches fail unchanged.
Reparse and validate every candidate. Establish preservation fixtures before consumer adoption.

Merge baseline, pending and current values: unrelated edits merge, identical edits converge, different changes to the
same value conflict. Lock a stable sibling file for cooperating writers; read/patch under the lock and recheck the
revision immediately before replacement. Arbitrary editors do not participate in the lock: a race remains between
the final check and rename. Follow existing symlinks, abort on retargeting, preserve existing permissions, create new
files as 0600, sync a same-directory temporary file, rename, and sync the directory.

Appearance v2 uses sparse defaults, rejects invalid known fields and warns about preserved unknown fields. Retain the
v1 decoder and explicit v1 serialization APIs. Document version is metadata, separate from the effective appearance
model. First successful Settings save upgrades valid v1 by changing only version and requested values. New editing
documents use v2; unsupported versions are read-only. Enable GUI v2 writes only after readers/adapters pass compatibility.

Shell owns defaults/validation in its exported configuration package. Preserve paths and meanings. Reads never create
files or write defaults. Missing overrides use defaults; reset removes the override. Reject invalid known values.

Settings retains Save/Discard, tracks baseline/pending edits, refreshes untouched controls on external changes and
retains pending edits. Expose baseline/disk/pending values and per-value keep-pending/accept-external resolution;
recheck on save. Show default/override status and diagnostics. Discard loads latest disk. Invalid external documents
block saves and running consumers retain last valid values; startup errors use defaults with diagnostics. Missing
files use defaults without writes. Watch files and nearest existing parents through replacement/deletion/recreation;
publish only differing effective values. Domain saves have independent outcomes. Rollback is conditional on the staged
revision still being current; concurrent changes survive and must not be reported successfully applied.

- [ ] Preservation fixtures cover comments, unknown fields, quoted/dotted keys, inline tables, multiline strings, Unicode, CRLF, arrays/AoT, insertion/reset and rejected patches.
- [ ] Merge, convergence, conflicts/reset, cooperating locks and revision-change aborts pass.
- [ ] Unreadable files, permissions, interrupted writes, replacement failures, symlink retargeting and durability outcomes pass.
- [ ] v1 behavior remains compatible; sparse v2/reset and surgical first-save upgrades pass; unsupported versions cannot be overwritten.
- [ ] Runtime invalid/startup/missing/delete/recreate and unchanged-signal scenarios pass.
- [ ] Settings Save/Discard, external updates, per-value resolution, partial saves and adapter/rollback concurrency pass.
- [ ] Each repository passes required clean acceptance and installed-package checks at accepted provider revisions.
- [ ] Files and Viewer pass standalone checks without Shell or Settings installed.
- [ ] Every participating submodule is clean and pinned to a canonical published implementation commit.
- [ ] Dependency-order integration and user-operated concurrent-edit/appearance checks are recorded with dates and revisions.

## Implemented provider design

- `document.h`, `document.cpp`: key-segment paths, typed scalar/aggregate values, exact content revisions, source/assignment spans, schema callbacks, baseline/pending edit batches and conflict triples. TOML-aware value/comment scanning and toml++ header parsing preserve unrelated bytes. Planning, insertion and candidate validation are private helpers. Inline tables/arrays are whole values; unsafe inline-child and AoT modifications fail unchanged.
- `document_store.cpp`: canonical-target sibling flock, under-lock re-read, exact-byte/target recheck, temporary file cleanup, permission preservation, fsync/rename/directory-fsync, separate durability outcomes, and revision-guarded restoration of exact prior bytes/existence.
- `appearance_document.h`, `appearance_document.cpp`: sparse v2 decode projects only known values into an in-memory v1 default model and reuses existing validation. Unknown fields warn; version metadata stays separate. First-save upgrade edits only version/requested values. Existing explicit serializer/parser stay v1. `load()` reads both formats.
- `test_document.cpp`: preservation, Unicode/CRLF, multiline strings, insertion/reset, unsupported patches, merge/convergence/reset conflicts, permissions, two-process writes, symlink/revision guards, v1/v2 behavior and guarded rollback.
- `test_document_storage_faults.cpp`: executable-owned syscall interposition checks write/interruption, file-sync, rename and post-rename directory-sync outcomes without adding production fault hooks.
- Installed consumer probe exercises public v2 decoding/schema/reset APIs.

Remaining arbitrary-editor race: writers outside the sibling advisory lock can modify a file after the final revision check and before rename/unlink. Exact content equality detects observed changes; it is not a universal transaction or a history counter. Legacy explicit full-document writing retains its previous semantics and is outside the interactive editing protocol.

## CA-001a consumer acceptance correction

Baseline: published `7e83cacde8911452741420a29abbfe4465b7d52b`. Settings rollback review exposed a contract gap:
a pre-lock snapshot can miss unrelated changes subsequently merged by the save. Restoring that snapshot would erase
those changes even when the staged revision is still current. `stageDocument()` and `stageAppearanceDocument()`
return a separate StagedSaveResult containing the unchanged SaveResult API and the exact pre-write snapshot captured
under the advisory lock. Existing SaveResult layout and save/restore symbols remain compatible. Failed pre-replacement
saves have no rollback snapshot; a durability failure includes it because replacement already happened.

A regression also reproduced appearance saves classifying an unreadable source directory as Invalid instead of
StorageFailure (before correction: `/tmp/holonight-config-staging-regression.log`). Appearance saves now use the same
I/O classification as generic document saves. New regression coverage exercises unrelated edits between the client
baseline read and staging, exact rollback preservation, original absence, conflicts and post-replacement faults.

CA-001a verification, 2026-10-05: local CTest passed both behavioral/storage and installed-consumer suites. Full
`task tidy` and `task format-check` passed. Clean `task ci` passed build-test, static-checks (full source/test tidy
and formatting) and licensing (70/70 files); complete logs reviewed at `build/ci/20261005T205743Z-0h066bn9/`.
Mock launcher failures in the build log are deliberate cases of the four passing launcher tests. The installed
consumer resolves the additive staging API and checks its invalid-destination outcome.

## CA-001b dangling target correction

Baseline: published `b705ccb2bc7ab87c3bf9706ec42bb5c97dd4a5eb`. Missing file and directory symlink targets
reproduced two failed saves (`/tmp/holonight-config-dangling-regression.log`). Storage now walks components and follows
links before processing parent traversal; it retains existing symlinks while creating their missing targets. The
same resolver is used for target rechecks, preserving retarget detection. Loops and traversal through non-directories
fail without writes. Missing intermediate directories followed by parent traversal are rejected rather than
pretending that an inaccessible path resolves to a different file.

The additive, read-only `resolveDocumentTarget()` exposes this resolution for Qt watching: watchers must retain the
target-parent watch after a symlink target is deleted, when QFileInfo canonicalFilePath becomes empty. It creates
nothing and returns diagnostics for unavailable/invalid paths. Existing read and save API layouts are unchanged.

Inline-table members now appear in snapshots with typed values and source locations. Their unsafe child edits remain rejected; arrays and arrays of tables remain whole conflict values. This corrects override presence before Settings adoption without changing snapshot layout.
