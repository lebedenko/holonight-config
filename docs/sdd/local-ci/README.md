# Local CI rehearsal

Baseline: b185c8404368cee6b2729ee17ffbcc671ba41cf3. Umbrella package CI-002.

The module has no provider repositories. Build/test and independent static jobs
use the same immutable Arch-based tools/TOML image, snapshot launcher and lane
commands locally and remotely; licensing uses REUSE 6.2.0. Preserve main push/PR
triggers, separate job results, contents-read permission and development tasks.
Build/test preserves the debug-tests preset and its no-tests=error behavior. Static
checks use the preset's actual build/test directory (the prior workflow configured
build/test but attempted static targets under build). Each lane has a fresh tree.

Copy the accepted pilot launcher/regressions into module-owned scripts/ci; no
umbrella product implementation or external helper checkout is needed. Preserve
edited/new inputs, deletions, executable mode and symlinks, while excluding ignored
builds. Mount the input read-only and collect host-owned logs/results in build/ci.
Failure logs print to local and remote consoles. Image layers can be cached.

Files: Taskfile.yml, README.md, .gitignore, push validation workflows, scripts/ci
and this SDD. No public C++ contracts change. Acceptance: launcher regressions,
full task ci, complete log review, host format/tidy/test, source/build isolation,
diff checks and licensing. Acceptance passed on 2026-10-02. No pushes or pin updates are included.

## Exposed baseline corrections

The corrected static job exposed baseline findings. Add braces and trailing commas;
move typography/shape parsing into private helpers to meet complexity limits while
preserving field order/diagnostics; use string_view::substr in the write loop instead
of pointer arithmetic; make vector moves explicit in Result without changing signatures.
Public enums retain the existing int ABI with documented, narrow performance-enum-size
exceptions: shrinking those types would change provider layouts. Test assertion macros
retain expression/file/line diagnostics, delegate to a single-evaluation helper, and
have a documented macro-usage exception. New assertion regression cases verify failures
still throw and operands are evaluated once.

Give the existing installed-package consumer probe a compile-only target/compile command
and include it in full tidy. The isolated package installation/run test remains required.
Remove unused QML preset inputs and pass QML paths only for QML modules in local tooling.
Host format/full tidy/test and all three container lanes pass, including the assertion
regressions. Final clean acceptance: `build/ci/20261002T191647Z-sdfcu3ed/`.
Snapshot isolation preserved hashes, modes and timestamps of 143 tracked source and
development-build files (`build/ci/20261002T191535Z-juy59w0d/`). Complete logs
were reviewed; no actionable diagnostics remain. Host evidence: build/ci/host-format.log,
build/ci/host-tidy.log and build/ci/host-test.log. No public contracts or provider pins change.
