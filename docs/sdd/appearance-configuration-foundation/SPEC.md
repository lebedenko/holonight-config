# SPEC: Appearance Configuration Foundation

**Initiative:** ACF-003
**Date:** 2026-08-07
**Status:** Proposed
**Upstream baseline:** `a4db5d806f3058683c40845efcfebed7446dbc5b`

## Goal

Provide the toolkit-neutral, independently buildable C++ authority for HoloNight's canonical global appearance
document. Consumers must share field types, defaults, validation, path resolution, parsing, deterministic
serialization, atomic writing, and diagnostics without depending on Qt or any HoloNight product repository.

The coordinating contract is the umbrella initiative
`docs/initiatives/appearance-configuration-foundation/README.md` at umbrella commit `aa9bcc5`.

## Public document contract

The canonical document is `$XDG_CONFIG_HOME/holonight/appearance.toml`, or
`$HOME/.config/holonight/appearance.toml` when `XDG_CONFIG_HOME` is unset or empty. The only override is
`HOLONIGHT_APPEARANCE_FILE`; a non-empty value replaces the complete resolved path.

```toml
version = 1

[theme]
scheme = "holonight-dark"
accent = "blue"

[typography]
ui_family = "Inter"
ui_size = 12
monospace_family = "JetBrains Mono"
monospace_size = 12
title_family = "Audiowide"
title_size = 10
display_family = "Rajdhani"
display_size = 24

[icons]
theme = "HoloNight"
fallback = "Papirus"
cursor = "default"

[layout]
scale = 1.0

[shape]
style = "inherit"
scale = 1.0
# base_radius and base_chamfer are optional
```

All fields shown above are required in a persisted v1 document except `shape.base_radius` and
`shape.base_chamfer`. Serialization always emits the complete required document and omits disengaged optional
values. It never emits legacy fields.

## Defaults and validation

| Field | Default | Valid values |
|---|---|---|
| `version` | `1` | Exactly integer `1` |
| `theme.scheme` | `holonight-dark` | Non-empty trimmed UTF-8 string, at most 128 bytes |
| `theme.accent` | `blue` | Non-empty trimmed UTF-8 string, at most 128 bytes |
| Font families | As shown above | Non-empty trimmed UTF-8 string, at most 256 bytes |
| Font sizes | `12`, `12`, `10`, `24` | Integer points in `[6, 48]` |
| Icon/cursor identifiers | As shown above | Non-empty trimmed UTF-8 string, at most 256 bytes |
| `layout.scale` | `1.0` | Finite number in `[0.5, 3.0]` |
| `shape.style` | `inherit` | `inherit`, `hybrid`, `rounded`, or `chamfered` |
| `shape.scale` | `1.0` | Finite number in `[0.25, 4.0]` |
| Shape base overrides | unset | When present, finite number in `[0, 128]` |

The neutral package validates identifier structure only. Scheme existence and valid scheme/accent combinations
remain owned by the HoloNight theme catalog in `holonight-qt`.

Whitespace surrounding string values is removed during validation. An empty result is invalid. Unknown keys and
tables are rejected so misspellings cannot silently become inert user preferences. Missing required fields, wrong
types, duplicate TOML keys, unsupported versions, non-finite or out-of-range numbers, and oversized input are
errors; a document is never partially accepted or field-by-field defaulted.

The maximum input size is 64 KiB. A missing file is distinct from an invalid file and resolves to `defaults()` at
startup without creating anything.

## Functional requirements

- Expose standard-C++ value types for the complete appearance document, with value equality.
- Expose `defaults()`, `validate()`, `parse()`, `load()`, `serialize()`, and `writeAtomically()` operations with no
  Qt, event-loop, compositor, Settings, Shell, or adapter dependency.
- Return structured results rather than logging, throwing for ordinary file/configuration errors, or terminating.
- Diagnostics identify a stable error code, human-readable message, source path where applicable, and TOML source
  location where available. Diagnostics never include unrelated file contents.
- `load()` distinguishes `Missing`, `IoError`, `TooLarge`, `SyntaxError`, `UnsupportedVersion`, and
  `ValidationError` outcomes. Successful missing-file resolution reports that defaults were used.
- `serialize()` is deterministic: schema order, UTF-8, LF endings, one trailing newline, locale-independent numeric
  formatting, and omission of disengaged shape overrides.
- `writeAtomically()` creates the parent directory when needed, writes a uniquely named same-directory temporary
  file, flushes it, closes it, atomically replaces the destination, and cleans up its temporary file on failure.
  A failed write leaves the previous destination content intact.
- The package does not watch files. Each consumer owns its native watcher and publishes only a fully parsed and
  validated replacement, retaining its last known-good value after a reload failure.
- Provide test helpers for temporary-path resolution and representative valid/invalid documents; helpers must not
  mutate process-global environment implicitly.

## Build and package requirements

- Use CMake and CTest and require C++20 or newer.
- Export one namespaced library target, `HoloNight::Config`, with conventional install rules and package config files.
- Use toml++ for TOML parsing. Keep it a private implementation dependency so consumers do not inherit toml++ types
  or compile definitions through the public API.
- Public headers use only the C++ standard library and the library's own declarations.
- Build and tests succeed independently from the umbrella and all other HoloNight repositories.

## Clean-break constraints

This repository does not parse, write, migrate, delete, or discover `theme.conf`, `appearance.json`, Shell
`config.toml`, KDE configuration, or field-specific `HOLONIGHT_*` variables. It never stores derived dark/light mode,
transparency, blur, shell behavior, credentials, private URLs, or resolved color palettes.

## Verification

- Unit tests cover every default and validation boundary, including NaN/infinity constructed through the API.
- Parse tests cover valid complete documents, both optional values, missing required fields, unknown fields, wrong
  types, duplicates, malformed TOML, unsupported versions, size limits, and source diagnostics.
- Path tests cover XDG, HOME fallback, explicit override, empty variables, and injected environments without changing
  the test runner's environment.
- Serialization tests use golden output and parse/serialize round trips.
- Atomic-write tests cover initial creation, replacement, parent creation, failure preservation, and temporary-file
  cleanup.
- Install/consumer smoke tests use only the exported `HoloNight::Config` package.

## Non-goals

- File watching, IPC, portals, theme-catalog lookup, palette generation, or applying appearance.
- A generic configuration framework for Shell or other product settings.
- Legacy compatibility or automatic migration.
- Secret storage.
