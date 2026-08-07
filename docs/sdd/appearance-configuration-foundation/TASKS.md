# SDD Tasks — appearance-configuration-foundation

Implementation started after umbrella ACF-003 became `Ready`, on published baseline
`c0e4aa2dd38906684848fde0ba9eef325eb47b8b`.

- [x] ACF3-01: Establish C++20 CMake project, test target, install/export rules, and external consumer smoke test.
- [x] ACF3-02: Implement schema value types, exact defaults, equality, structured diagnostics, and whole-document
  validation.
- [x] ACF3-03: Implement injected-environment path resolution with the single supported override.
- [x] ACF3-04: Add bounded TOML v1 parsing with strict required/unknown-field handling and source diagnostics.
- [x] ACF3-05: Add deterministic TOML serialization and golden/round-trip tests.
- [x] ACF3-06: Add loading semantics that distinguish missing defaults from present invalid documents.
- [x] ACF3-07: Add same-directory atomic writing, failure preservation, cleanup, and filesystem tests.
- [x] ACF3-08: Add isolated consumer test helpers without implicit environment mutation.
- [x] ACF3-09: Document public API, schema evolution, platform guarantees, dependency provisioning, and downstream
  integration.
- [ ] ACF3-10: Run formatting, warnings-as-errors build, unit tests, install test, and package-consumer test; publish
  the verified commit for the umbrella handoff.

## Completion evidence

Record exact commands, toolchain versions, test results, and the published commit here before asking the umbrella
coordinator to mark ACF-003 `Done`. A local or unpublished commit is not a handoff.

Local verification on 2026-08-07:

- CMake `4.4.2`, GCC `16.1.1 20260728`, toml++ `3.4.0`.
- `cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug` — passed.
- `cmake --build build -j2` — passed with `-Wall -Wextra -Wpedantic -Werror` on production and test-support code.
- `./build/tests/holonight_config_tests` — 25/25 focused schema, codec, path, store, and helper tests passed.
- `ctest --test-dir build --output-on-failure` — 2/2 CTest entries passed, including install-tree external consumer.
- `cmake --build build --target format-check` and `git diff --check` — passed.
- Publication and the exact published implementation commit remain pending; ACF3-10 is intentionally unchecked.
