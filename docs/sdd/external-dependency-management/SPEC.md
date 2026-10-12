# External dependency management — holonight-config

Work package: EDM-004
Audit baseline: `d6a392b41991f70a004d58f7694c7b6115cb7280`
Umbrella audit: `38146d8f474317f3f8d72699f60e6d16a71dd165`
Implementation baseline: `854784a32f16c231ada49db9b6188e79c5903900`
Status: Locally verified; publication and umbrella pin approval pending.

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall explicitly prepare supplied local providers and check installed external prerequisites.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG and any GMock targets they use.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/holonight-config.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Preserve pkg-config toml++ discovery and the custom test framework; Config uses neither Qt nor GTest.
The full Arch image is the published EDM-024 digest; licensing retains its separate image.
CI explicitly pulls images before running clean build/test, release and static acceptance with
`--pull=never --network=none`. Config has no local source providers to prepare.
QMarkdown source revision `b71740c7e0f26b0709f5e929a79e8d01dba56071` is an ecosystem prerequisite,
not a dependency of this library. Debian qualification remains deferred.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, installed-package consumers and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and manual checks.
- [x] Prepare the verified repository-local implementation commit.
- [ ] Publish and update the umbrella pin only when authorized.

## Verification — 2026-10-12

- Canonical `origin/main` was checked with `git ls-remote` and matched the implementation baseline;
  the repository was clean before edits. No internal provider revisions or Qt features apply.
- `python3 scripts/ci/test_launcher.py`: 4 tests pass (isolation, offline settings, failure propagation).
- `python3 scripts/ci/test_dependencies.py`: 3 tests pass, including direct CMake missing-package failure,
  configure/build/test failure without acquisition/preparation, external prefix paths with spaces,
  and preservation of stale consumer trees after a selected pkg-config package version changes.
- `python3 -m unittest discover -s tooling -p 'test_*.py'` at the umbrella: 32 fixtures pass,
  including internal/global conflicts, private Qt mismatch and provider compatibility behavior.
- `python3 tooling/sync.py --check --module holonight-config`: no drift. Standalone declarations
  match the canonical inputs; Config explicitly overrides toml discovery to pkg-config.
- `python3 scripts/ci/run.py`: all three lanes pass using clean snapshots and `--network=none --pull=never`.
  Build/test lane runs explicit `deps`, test configure/build/test and release configure/build.
  Both CTest entries pass: custom library suite and installed-package consumer. Release uses
  `BUILD_TESTING=OFF`; test uses `BUILD_TESTING=ON`. Production sources and APIs are unchanged.
- Toolchain: GCC 16.2.1 (20260810), CMake 4.4.3, Ninja 1.13.2, Python 3.14.7,
  pkgconf 3.0.7, toml++ 3.4.0, clang-format/clang-tidy 22.1.8, REUSE 6.2.0.
  Config has no Qt requirement; private-module/version and feature-switch tests are not applicable.
  The original discovery interface has no toml++ minimum version; no new restriction is introduced.
- Full image: `ghcr.io/lebedenko/holonight-deps@sha256:37ca051c39d1688d6413c47d94b8c0f221152dac6981ec67d155f2c43ea17b40`.
  Licensing image remains `docker.io/fsfe/reuse@sha256:85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`.
- Complete final logs reviewed: `build/ci/20261012T163132Z-ilsrcy6x/{build-test,static-checks,licensing}.log`;
  all exit codes zero. Earlier acceptance was superseded after the prerequisite discovery correction.
  Builds emit no actionable warnings. Tidy reports only suppressed non-user diagnostics and existing
  NOLINT suppressions (137163/45 for tests and 113112/23 for production). Formatting and REUSE pass.
- No manual UI acceptance applies to this toolkit-neutral library. Ecosystem integration remains EDM-022.

Publication and the umbrella gitlink remain pending authorization. Hosted CI has not been queried for
this unpublished implementation; no hosted acceptance is claimed.
