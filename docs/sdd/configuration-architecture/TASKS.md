# CA-001 implementation

- [x] Inspect existing implementation and preserve explicit v1 contracts.
- [x] Implement provider snapshots, schemas, preserving edits, locked storage, guarded restore and appearance v2.
- [x] Add behavioral preservation/concurrency/storage/compatibility regression coverage and installed consumer probe.
- [x] Run focused tests, format/full tidy and clean container acceptance; review complete logs.
- [ ] Commit, publish and hand off exact revision.

Verification date: 2026-10-05.

- `cmake --preset test`; `cmake --build build/test -j2`; `ctest --test-dir build/test --output-on-failure`: passed both behavioral and installed-package tests, including guarded restore.
- `task tooling:refresh`; `task format-check`; `task tidy`: passed. Required static CMake targets cover all new translation units.
- Final `task ci`: build-test, static-checks and licensing passed in clean isolated trees. Logs: `build/ci/20261005T195949Z-8jzwsm9s/`. Complete logs reviewed; no actionable build/static diagnostics. The launcher regression deliberately prints mocked failures and its four tests pass.
- Earlier Docker attempt inside the sandbox failed on socket permission; the approved Docker run passed.
- `git diff --check`: passed.

This provider acceptance does not imply GUI, Qt watcher, adapter, Shell or ecosystem acceptance. Inline-child/AoT edits and unsupported table insertion layouts fail unchanged. Nonparticipating editors retain the documented final-check/rename race.
