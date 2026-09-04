# HoloNight Config

Toolkit-neutral C++ configuration contracts shared by HoloNight components. The library owns the canonical global
appearance document without depending on Qt or another HoloNight repository.

## Build and install

HoloNight Config requires CMake 3.25, a C++20 compiler, pkg-config, and toml++ 3.x development files. Dependency
discovery is local and never downloads packages.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
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
