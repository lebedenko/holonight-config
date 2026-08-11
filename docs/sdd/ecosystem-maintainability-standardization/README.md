# Ecosystem Maintainability Standardization

Status: Draft

Umbrella work package: see `docs/initiatives/ecosystem-maintainability-standardization/TASKS.md` in the pinned
umbrella checkout.

## Profile and review result

Profile: Shared library.

The exported HoloNightConfig package is well bounded and has an install-tree consumer test, but its C++20 declaration differs from the ecosystem C++23 baseline and `install` currently implies a per-user prefix.

The review covered target boundaries, CMake and presets, Task commands, CI/release workflows, tests, packaging,
documentation, installation behavior, and (where applicable) QML module/import/resource metadata. Product changes
remain backlog work; this document does not change runtime behavior.

## Accepted constraints

- Production installation must configure prefix `/usr` and support a temporary `DESTDIR` stage consumed by downstream builds.
- Task names are capability-based; inapplicable local or removal tasks are omitted, not implemented as no-ops.
- Package-manager changes, service enablement, active-session changes, and user/admin data mutation are out of scope.
- CI-image changes belong to the existing Shared CI Build Infrastructure initiative.

## Completion criteria

The local work package is complete only when the tasks in [TASKS.md](TASKS.md) pass repository-local verification,
the commit is published to the canonical remote, and the umbrella coordinator accepts the handoff.
