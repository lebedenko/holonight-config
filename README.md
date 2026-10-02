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
