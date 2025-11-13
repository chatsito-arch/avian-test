#!/usr/bin/env bash
# Helper that cross-compiles Avian for the PlayStation Vita interpreter build.
set -euo pipefail
: "${VITASDK:?please export VITASDK=/path/to/vitasdk}"
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
AVIAN_ROOT=$(cd "$ROOT_DIR/.." && pwd)
export CC=arm-vita-eabi-gcc
export CXX=arm-vita-eabi-g++
export AR=arm-vita-eabi-ar
export RANLIB=arm-vita-eabi-ranlib
export STRIP=arm-vita-eabi-strip
VITA_FLAGS="-march=armv7-a -mfpu=neon -fPIC -O2 -DAVIAN_NO_JIT -DNO_POSIX_SIGNALS"
make -C "$AVIAN_ROOT" clean
make -C "$AVIAN_ROOT" arch=arm platform=linux process=interpret \
  extra-cflags="$VITA_FLAGS" extra-cxxflags="$VITA_FLAGS" \
  extra-ldflags="-L$VITASDK/arm-vita-eabi/lib" \
  extra-libraries="-lpthread"
