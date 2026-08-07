# DESIGN: Appearance Configuration Foundation

## Dependency boundary

`holonight-config` is a leaf domain library. Its public API contains plain values such as `std::string`, arithmetic
types, `std::optional`, `std::filesystem::path`, and result/diagnostic structures. toml++ is confined to source files.
Qt conversion, live reload, catalog validation, UI state, and portal projection belong to consumers.

The implementation is split by responsibility without introducing a general configuration framework:

- `Appearance`: schema values, defaults, equality, and validation;
- `Path`: deterministic path resolution from an injected environment view;
- `Codec`: bounded TOML parsing and deterministic serialization;
- `Store`: file loading and same-directory atomic replacement;
- `Diagnostic`: stable error categories plus optional path/source position;
- `TestSupport`: opt-in fixtures and environment/path helpers for consumers.

## Result model

Ordinary configuration and filesystem failures are values, not exceptions. Public operations return a small
result type containing either a value or one or more diagnostics. Allocation and other exceptional failures retain
normal C++ exception behavior.

Loading a missing file is a successful resolution with `defaults()` and origin `Default`; loading a present invalid
file is a failure. This lets a consumer distinguish first-run behavior from a broken replacement. The store does not
hold last-known-good state: consumers already own process lifetime and decide whether startup defaults or the last
valid live value should remain active.

Validation is whole-document and runs after TOML decoding and before serialization. The parser tracks all consumed
keys and rejects unknown keys after decoding. This deliberately favors a clean pre-stable contract over forward
acceptance of misspelled or unsupported preferences. A future schema version must add an explicit version decoder.

## Path resolution

Path resolution accepts an environment lookup callback or immutable key/value view. Precedence is:

1. non-empty `HOLONIGHT_APPEARANCE_FILE` exactly as supplied;
2. non-empty `XDG_CONFIG_HOME` plus `holonight/appearance.toml`;
3. non-empty `HOME` plus `.config/holonight/appearance.toml`;
4. a `PathUnavailable` diagnostic.

The library does not expand `~`, consult Qt/KDE paths, canonicalize a path that may not exist, or modify environment
variables. Relative explicit overrides remain relative to the caller's working directory and are intended for tests
or controlled launches.

## Atomic storage

Serialization completes before filesystem mutation. The writer then ensures the parent directory exists, creates an
exclusive unique temporary file in that directory, writes all bytes, flushes and closes it, and renames it over the
destination. Same-directory placement preserves filesystem-level rename atomicity. Failure paths remove only the
temporary file created by that operation and never remove or truncate the destination.

The initial implementation guarantees process-level flush plus atomic rename. Directory `fsync` durability and
cross-platform replacement differences must be documented and tested per supported platform; they are not to be
misrepresented as stronger crash-consistency guarantees than the implementation provides.

## Packaging

The repository provides a normal installable CMake package. A private toml++ dependency may be discovered through a
system package or a narrowly pinned build dependency selected during implementation; it must not appear in public
headers. Dependency acquisition must not occur unexpectedly for downstream consumers.

`HoloNight::Config` is the only supported consumer target. Tests include a minimal external CMake consumer against
the install tree to catch missing exports and accidental transitive dependencies.

## Decisions and trade-offs

- Whole-document rejection prevents ambiguous partial state and makes last-known-good reload reliable, at the cost
  of requiring explicit schema evolution for new keys.
- Concrete defaults create a stable first-run experience. Font availability is not validated here because it is a
  platform concern; consumers may report or apply their normal font fallback.
- Title size `10` follows the Qt typography baseline and the accepted conceptual schema instead of retaining Shell's
  unusually small historical `8`; UI and monospace size `12` preserve the current Shell/Settings user-facing scale.
- The numeric ranges preserve already exercised Qt limits: fonts `[6, 48]`, layout `[0.5, 3.0]`, shape scale
  `[0.25, 4.0]`, and optional extents `[0, 128]`.
- Test helpers are separate from the production target so production consumers do not gain fixture APIs or test-only
  dependencies.
