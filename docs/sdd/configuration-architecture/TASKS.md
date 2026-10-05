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

## CA-001a

- [x] Reproduce published appearance storage-result misclassification and review rollback gap.
- [x] Add locked pre-write snapshots without changing the existing SaveResult layout.
- [x] Verify regression, preservation, storage faults and installed consumer; review full acceptance logs.
- [ ] Commit, publish and hand off the additive provider revision.

## CA-001b

- [x] Reproduce saves through dangling file and directory symlinks.
- [x] Resolve components safely and share read-only resolution with watchers.
- [x] Verify relative/absolute targets, parent traversal, loops, retargeting and full preservation/storage fixtures.
- [ ] Review clean acceptance, publish and hand off the correction.

2026-10-06 local: CA-001b focused CTest and installed-package consumer, full tidy and formatting passed. Clean `task ci` passed build-test, static-checks and licensing (`build/ci/20261005T213144Z-882fslrm/`); all 180 log lines reviewed. Inline-table override presence regression is covered while child edits remain unsupported.

## CA-001c

- [x] Reproduce identical-byte symlink retargeting before rollback.
- [x] Require the captured physical target as well as content revision.
- [x] Verify positive linked-target rollback and retargeting/refusal, full fixtures, analysis and clean acceptance.
- [ ] Publish and hand off.

2026-10-06 local: CA-001c host behavioral/installed-package tests, full analysis and formatting passed. Clean `task ci` passed all three lanes (`build/ci/20261005T224553Z-q2wn0vb6/`); complete logs reviewed. Tests cover existing and originally absent targets; retargeted identical bytes cannot authorize rollback.
