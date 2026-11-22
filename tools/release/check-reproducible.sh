#!/usr/bin/env bash
# Mobile Set Editor - is the release APK reproducible? (#77)
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the unsigned release APKs twice, from two clean clones of this repository's HEAD at
# different paths, and compares them byte for byte (diffoscope shows the differences, if there are
# any and it is installed). The submodules come from this checkout, so nothing is downloaded but
# Gradle's own dependencies (verified against gradle/verification-metadata.xml).
#
#   tools/release/check-reproducible.sh [work-dir]     (default: a new temporary directory)
#
# Needs what a release build needs: the Android SDK (android/local.properties or ANDROID_HOME)
# with the pinned NDK, CMake and ninja on PATH, a JDK 17+.
set -euo pipefail

SRC=$(cd "$(dirname "$0")/../.." && pwd)
WORK=${1:-$(mktemp -d)}
mkdir -p "$WORK"
echo "check-reproducible: building HEAD ($(git -C "$SRC" rev-parse --short HEAD)) twice under $WORK"

clone() {
  local dir=$1
  rm -rf "$dir"
  git clone --quiet --no-hardlinks "$SRC" "$dir"
  # the submodules from this checkout, at the commits the clone records
  git -C "$SRC" config --file .gitmodules --get-regexp '^submodule\..*\.path$' | while read -r key path; do
    name=${key#submodule.}; name=${name%.path}
    git -C "$dir" config "submodule.$name.url" "$SRC/$path"
  done
  git -C "$dir" -c protocol.file.allow=always submodule --quiet update --init
  [[ -f $SRC/android/local.properties ]] && cp "$SRC/android/local.properties" "$dir/android/"
  return 0
}

build() {
  local dir=$1
  (cd "$dir/android" && ./gradlew --no-daemon --quiet --console=plain assembleRelease) >"$dir.log" 2>&1 ||
    { tail -40 "$dir.log" >&2; echo "check-reproducible: the build in $dir failed (log: $dir.log)" >&2; exit 2; }
}

# two different paths (of different lengths), so paths that leak into the build show up
A=$WORK/a
B=$WORK/second-checkout
for d in "$A" "$B"; do
  echo "check-reproducible: cloning into $d"
  clone "$d"
  echo "check-reproducible: building in $d"
  build "$d"
done

status=0
for apk in "$A"/android/app/build/outputs/apk/release/*-unsigned.apk; do
  name=$(basename "$apk")
  other=$B/android/app/build/outputs/apk/release/$name
  if [[ ! -f $other ]]; then
    echo "MISSING  $name in the second build"; status=1
  elif cmp -s "$apk" "$other"; then
    echo "SAME     $name ($(sha256sum < "$apk" | cut -c1-16))"
  else
    echo "DIFFERS  $name"; status=1
    if command -v diffoscope >/dev/null; then
      diffoscope --text "$WORK/$name.diff.txt" "$apk" "$other" || true
      echo "         differences: $WORK/$name.diff.txt"
    fi
  fi
done
[[ $status == 0 ]] && echo "check-reproducible: the unsigned release APKs are identical"
exit $status
