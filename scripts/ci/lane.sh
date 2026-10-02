#!/bin/sh
set -eu
lane=$1
mkdir /work/source
cp -a /input/. /work/source/
cd /work/source
export HOME=/work/build/home LC_ALL=C TZ=UTC
mkdir -p "$HOME"
case "$lane" in
  licensing)
    reuse --version
    reuse lint
    ;;
  build-test|static-checks)
    python3 --version
    git --version
    cmake --version
    ninja --version
    c++ --version
    clang-format --version
    clang-tidy --version
    pkg-config --modversion tomlplusplus
    cmake --preset debug-tests
    if [ "$lane" = build-test ]; then
      python3 scripts/ci/test_launcher.py
      cmake --build --preset debug-tests --parallel
      ctest --preset debug-tests
    else
      cmake --build build/test --target format-check
      cmake --build build/test --target tidy
    fi
    ;;
  *) echo "Unknown lane: $lane" >&2; exit 2 ;;
esac
