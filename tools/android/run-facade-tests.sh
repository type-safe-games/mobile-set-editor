#!/usr/bin/env bash
# Run the facade tests on an Android device or emulator (#43).
#
#   tools/android/run-facade-tests.sh [<build dir>]
#
# The build dir is a CMake build of this repository with the NDK toolchain (default:
# build-android-x86_64, for the x86_64 emulator), e.g.
#
#   tools/skia/build.sh android-x86_64
#   cmake -S . -B build-android-x86_64 -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
#         -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-android-x86_64 --target facade_tests_skia
#
# It pushes facade_tests_skia (stripped) and the Test fixture to /data/local/tmp/mse-facade-tests
# on the device adb sees (start the emulator first: emulator -avd <name> -no-window), plus the
# fixture's Golden images if templates/goldens has them, runs the tests there with the fixture's
# fonts as the only system fonts, and exits with their status.
#
# Environment: ANDROID_HOME (for adb, if it isn't on PATH) and ANDROID_NDK_HOME (for llvm-strip).
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
BUILD=${1:-$ROOT/build-android-x86_64}
DEVICE_DIR=/data/local/tmp/mse-facade-tests
GOLDENS=$ROOT/templates/goldens/images/fixture/fixture-cards

die() { echo "run-facade-tests.sh: $*" >&2; exit 1; }

ADB=$(command -v adb || true)
[[ -n $ADB ]] || ADB=${ANDROID_HOME:-$HOME/Android/Sdk}/platform-tools/adb
[[ -x $ADB ]] || die "adb not found (set ANDROID_HOME)"
[[ -f $BUILD/facade_tests_skia ]] || die "no $BUILD/facade_tests_skia: build it first (see the top of this script)"

STRIP=$(ls "${ANDROID_NDK_HOME:-/nonexistent}"/toolchains/llvm/prebuilt/*/bin/llvm-strip 2>/dev/null | head -1 || true)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp "$BUILD/facade_tests_skia" "$TMP/facade_tests_skia"
[[ -n $STRIP ]] && "$STRIP" "$TMP/facade_tests_skia"

"$ADB" wait-for-device
until [[ $("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r') == 1 ]]; do sleep 2; done

"$ADB" shell rm -rf "$DEVICE_DIR"
"$ADB" shell mkdir -p "$DEVICE_DIR"
"$ADB" push -q "$TMP/facade_tests_skia" "$DEVICE_DIR/"
"$ADB" push -q "$ROOT/test/fixture" "$DEVICE_DIR/"
goldens_arg=
if [[ -d $GOLDENS ]]; then
  "$ADB" push -q "$GOLDENS" "$DEVICE_DIR/goldens"
  goldens_arg=$DEVICE_DIR/goldens
fi

"$ADB" shell "cd $DEVICE_DIR && chmod 755 facade_tests_skia && \
  MSE_SYSTEM_FONT_DIRS=$DEVICE_DIR/fixture/fonts ./facade_tests_skia $DEVICE_DIR/fixture $DEVICE_DIR/work $goldens_arg; \
  echo \"exit status: \$?\"" | tee "$TMP/out.txt"
grep -q '^exit status: 0' "$TMP/out.txt"
