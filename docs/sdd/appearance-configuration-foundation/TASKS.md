# SDD Tasks — appearance-configuration-foundation

All implementation tasks remain unchecked until umbrella ACF-003 is `Ready`. Complete them in small commits on top
of baseline `a4db5d806f3058683c40845efcfebed7446dbc5b`.

- [ ] ACF3-01: Establish C++20 CMake project, test target, install/export rules, and external consumer smoke test.
- [ ] ACF3-02: Implement schema value types, exact defaults, equality, structured diagnostics, and whole-document
  validation.
- [ ] ACF3-03: Implement injected-environment path resolution with the single supported override.
- [ ] ACF3-04: Add bounded TOML v1 parsing with strict required/unknown-field handling and source diagnostics.
- [ ] ACF3-05: Add deterministic TOML serialization and golden/round-trip tests.
- [ ] ACF3-06: Add loading semantics that distinguish missing defaults from present invalid documents.
- [ ] ACF3-07: Add same-directory atomic writing, failure preservation, cleanup, and filesystem tests.
- [ ] ACF3-08: Add isolated consumer test helpers without implicit environment mutation.
- [ ] ACF3-09: Document public API, schema evolution, platform guarantees, dependency provisioning, and downstream
  integration.
- [ ] ACF3-10: Run formatting, warnings-as-errors build, unit tests, install test, and package-consumer test; publish
  the verified commit for the umbrella handoff.

## Completion evidence

Record exact commands, toolchain versions, test results, and the published commit here before asking the umbrella
coordinator to mark ACF-003 `Done`. A local or unpublished commit is not a handoff.
