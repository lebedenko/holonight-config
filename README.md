# HoloNight Config

Toolkit-neutral C++ configuration contracts shared by HoloNight components. The library owns the canonical global
appearance document without depending on Qt or another HoloNight repository.

## Build and install

HoloNight Config requires CMake 3.25, a C++20 compiler, pkg-config, and toml++ 3.x development files. Dependency
discovery is local and never downloads packages.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build/debug
ctest --test-dir build/test --output-on-failure
cmake --install build --prefix /desired/prefix
```

For local development, [Task](https://taskfile.dev/) provides the same entry points used during CI-oriented checks:
`task build`, `task test`, `task format-check`, and `task tidy`.

An installed consumer uses the single supported production target:

```cmake
find_package(HoloNightConfig CONFIG REQUIRED)
target_link_libraries(my_component PRIVATE HoloNight::Config)
```

Include `<holonight/config/config.h>` for the complete API. `defaults()` supplies the v1 value, `parse()` and
`serialize()` operate on TOML strings, `load()` distinguishes a missing file from an invalid file, and
`writeAtomically()` persists a validated value. Operations return `Result<T>` with structured diagnostics instead
of logging or throwing for ordinary configuration and filesystem errors.

## Document and path contract

The canonical file is `holonight/appearance.toml` below `$XDG_CONFIG_HOME`, falling back to
`$HOME/.config/holonight/appearance.toml`. A non-empty `HOLONIGHT_APPEARANCE_FILE` replaces that path completely.
Callers can inject an `Environment` for deterministic resolution without changing process-global environment.

Persisted v1 documents require `version` and the `theme`, `typography`, `icons`, `layout`, and `shape` tables.
`shape.base_radius` and `shape.base_chamfer` are the only optional fields. Parsing is strict and bounded to 64 KiB:
unknown, missing, duplicate, incorrectly typed, invalid, and unsupported-version fields reject the complete
document. A missing file succeeds with the shared defaults and `LoadOrigin::Default`; a present invalid file fails.

Schema changes require an explicit document-version decoder. Existing meanings and defaults must not be changed
silently, and serializers emit only the version they implement. Legacy HoloNight configuration files and
field-specific environment variables are intentionally outside this package.

## Storage guarantees

On supported Linux/POSIX filesystems, `writeAtomically()` serializes before mutation, creates its temporary file in
the destination directory, writes and `fsync`s it, closes it, then renames it over the destination. Failures remove
the operation's temporary file and preserve an existing destination. This provides atomic replacement to readers;
it does not promise directory-entry durability across sudden power loss because the parent directory is not
`fsync`ed. Cross-platform replacement semantics are not currently supported.

The detailed accepted contract and design rationale live in the
[Appearance Configuration Foundation SDD](docs/sdd/appearance-configuration-foundation/SPEC.md).

## License

HoloNight Config is licensed under `GPL-3.0-or-later`. See [LICENSE](LICENSE).

## Standalone developer tooling

See [tooling/README.md](tooling/README.md) for presets, local dependency overrides, editor refresh,
`task tooling:doctor`, and the independent Serena project.

## Local push validation

Run `task ci` with Git, Python 3, Task and an accessible Docker daemon; Podman is
used when Docker is absent. The immutable linux/amd64 build image supplies the
compiler, CMake, Ninja, clang tools and tomlplusplus. Other architectures require
configured amd64 emulation. REUSE licensing runs in its pinned 6.2.0 image.

Current tracked edits and non-ignored new files enter a read-only snapshot; deleted
and ignored files are omitted. New inputs are reported to add before pushing. Each
build/test, format/full tidy, and licensing lane gets its own disposable writable
copy. Development builds stay untouched and application artifacts are never reused.
Container-layer caches remain usable. GitHub CI uses these same scripts and images.

Complete logs, revision/dirty status, image identities, tool versions and lane exit
codes live under ignored `build/ci/<run>/`. Failed logs also print to the console.
Any required failure makes the task fail. For focused diagnosis use
`python3 scripts/ci/run.py --lane static-checks`. Run launcher regressions with
`python3 scripts/ci/test_launcher.py`. Releases/publication remain remote operations.

## Preserving document edits

`document.h` provides toolkit-neutral snapshots, schemas and edit batches. `readDocument()`
returns a missing snapshot without creating a file; unreadable and malformed files return
structured diagnostics. Snapshots retain exact original bytes, byte source spans and typed
values. A `KeyPath` is a vector of segments: `{"tray", "icon_overrides", "org.example.App"}`
contains three segments, so quoted keys containing dots remain unambiguous.

An `Edit` carries the baseline override (`nullopt` means absent) and the pending override
(`nullopt` means reset). `saveDocument()` reads current disk state under a stable sibling
`<canonical-target>.lock` advisory lock, merges unrelated edits, converges identical edits,
and returns baseline/disk/pending values for conflicting keys. Pending edits remain owned
by the caller. Re-resolve individual conflicts against the latest disk value and retry;
there is no whole-document overwrite option.

The TOML editor uses toml++ source spans and lexical inspection to replace only selected
values or remove their assignments. It retains surrounding comments and sections, including
comments within reset arrays. Arrays and inline tables are whole values. Arrays of tables
are whole conflict values; their replacement and edits inside inline tables currently return
`UnsupportedPatch`. Insertion into unsupported table arrangements also fails unchanged.
Every candidate is reparsed and schema-validated before writing. Unknown fields remain in
place unless a schema explicitly rejects them. Aggregate literals are parsed and normalized
before comparison; adding extra assignments through a literal is rejected.

Snapshots include individual inline-table members and their source locations for accurate
override status; editing those members remains unsupported. `resolveDocumentTarget()` resolves
physical targets without creating files, including dangling file and directory symlinks.
Saving follows existing symlinks to their physical target and rechecks both that target and
the exact content revision immediately before replacement. Existing permission bits survive;
new files have mode `0600`. A same-directory temporary file is fully written and synced,
renamed, then the parent directory is synced. `StorageFailure` leaves the destination
unreplaced; `DurabilityFailure` includes the snapshot already replaced on disk and means the
directory sync failed. Callers must account for that partial durability outcome.

The lock coordinates participating writers only. An arbitrary editor can still write between
the final revision check and rename. Do not delete the stable lock file after saving. An
explicit legacy `writeAtomically()` call retains its replacement/serialization contract;
new interactive editors should use the document APIs for preservation and coordination.

`restoreDocument()` supports staged application failures: it restores exact previous bytes
or previous absence under the same lock only if the staged content revision is still current.
Otherwise it returns `RevisionChanged` and preserves the external document. A successful
adapter operation must also compare its staged revision before reporting application success.

## Appearance document v2

`appearance_document.h` adds sparse v2 decoding and editing while preserving the explicit
v1 `parse()` and `serialize()` APIs. `load()` accepts both document formats. Document version
metadata lives in `AppearanceDocument::document_version`; the effective `Appearance` model
retains its existing v1 meaning for resolvers and adapters.

Absent v2 preferences use typed defaults. Invalid known types/ranges/enums reject the
document; unknown fields remain untouched and produce warnings. Unsupported versions
cannot be saved through the appearance editing API. `saveAppearanceDocument()` upgrades a
valid v1 document by changing only its version metadata and requested values. New documents
use v2. Reset removes an override. The provider does not enable GUI writes: consumer and
adapter compatibility must pass before Settings adopts v2 saves.

For asynchronous appearance application, use `stageAppearanceDocument()` (or generic `stageDocument()`). Its
`previous` snapshot is captured under the writer lock, so it includes unrelated changes merged after your client
baseline read. Pass that snapshot and the resulting staged revision to `restoreDocument()` on application failure.
A pre-lock client snapshot is not a safe rollback baseline. Pre-replacement failures have no `previous` snapshot;
post-replacement durability failures include it. These APIs preserve the existing SaveResult layout and save APIs.
