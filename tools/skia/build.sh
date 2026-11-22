#!/usr/bin/env bash
# Build Skia, CPU-only, from the pinned submodules (ADR 0004, ADR 0011).
#
#   tools/skia/build.sh <target> [<target>...]
#
# Targets: linux-x64, android-arm64-v8a, android-x86_64. Output goes to
# out/skia/<target>/ (libskia.a plus the vendored deps' static libraries, HarfBuzz's among them).
# gn is built from third_party/gn into out/skia/gn/ on first use.
#
# Nothing is fetched: tools/git-sync-deps is never run. The dependencies Skia's
# DEPS names live in third_party/skia-deps/ and are symlinked into the
# third_party/externals/ paths that Skia's BUILD files expect.
#
# Environment:
#   ANDROID_NDK_HOME  NDK root, needed for the android-* targets.
#   SKIA_NDK_API      Android API level to compile against (default 26, the app's minSdk).
#   CC, CXX, AR       Host compilers for gn and linux-x64 (default clang, clang++, ar).
#   JOBS              ninja -j (default: ninja's own).
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
SKIA=$ROOT/third_party/skia
DEPS_DIR=$ROOT/third_party/skia-deps
OUT=$ROOT/out/skia
DEPS_NAMES=(zlib libpng libjpeg-turbo freetype harfbuzz)

: "${CC:=clang}" "${CXX:=clang++}" "${AR:=ar}" "${SKIA_NDK_API:=26}"

die() { echo "skia/build.sh: $*" >&2; exit 1; }

[[ $# -ge 1 ]] || die "usage: $0 <linux-x64|android-arm64-v8a|android-x86_64>..."
[[ -f $SKIA/BUILD.gn ]] || die "third_party/skia is empty; run: git submodule update --init third_party/skia third_party/gn third_party/skia-deps"
[[ -f $ROOT/third_party/gn/build/gen.py ]] || die "third_party/gn is empty; run git submodule update --init third_party/gn"

# Link each vendored dep where Skia's BUILD files look for it, and check it is
# at the commit Skia's DEPS pins.
link_deps() {
  mkdir -p "$SKIA/third_party/externals"
  local name want have link
  for name in "${DEPS_NAMES[@]}"; do
    [[ -e $DEPS_DIR/$name/.git ]] || die "third_party/skia-deps/$name is empty; run git submodule update --init third_party/skia-deps"
    want=$(sed -nE "s|.*\"third_party/externals/$name\" *: *\"[^@]*@([0-9a-f]{40})\".*|\1|p" "$SKIA/DEPS")
    have=$(git -C "$DEPS_DIR/$name" rev-parse HEAD)
    [[ -n $want ]] || die "Skia's DEPS no longer names third_party/externals/$name"
    [[ $want == "$have" ]] || die "third_party/skia-deps/$name is at $have but Skia's DEPS pins $want (see tools/skia/README.md)"
    link=$SKIA/third_party/externals/$name
    if [[ -L $link || ! -e $link ]]; then
      ln -sfn "../../../skia-deps/$name" "$link"
    else
      die "$link exists and is not a symlink; remove it (left over from git-sync-deps?)"
    fi
  done
}

build_gn() {
  local gn=$OUT/gn/gn
  if [[ ! -x $gn ]]; then
    # Setting CXX stops gen.py from downloading a pinned clang through cipd.
    # (--allow-warnings: gn builds with -Werror, and GCC warns where clang does not)
    CXX=$CXX AR=$AR python3 "$ROOT/third_party/gn/build/gen.py" --allow-warnings --out-path="$OUT/gn" >/dev/null
    ninja -C "$OUT/gn" ${JOBS:+-j "$JOBS"} gn
  fi
  GN=$gn
}

# Arguments shared by every target. Everything not needed to rasterise paths,
# images and glyphs into a CPU surface is off.
common_args() {
  cat <<'EOF'
is_official_build = true
is_debug = false
is_component_build = false

# No GPU, PDF, SVG or XPS backends (ADR 0004).
skia_enable_ganesh = false
skia_enable_graphite = false
skia_use_gl = false
skia_use_vulkan = false
skia_use_dawn = false
skia_use_metal = false
skia_enable_pdf = false
skia_enable_svg = false
skia_use_xps = false

# No modules or tools. HarfBuzz is built (libharfbuzz.a) but Skia doesn't use it: the Skia
# Canvas shapes text with it directly, as Pango does on the desktop, so that glyph advances match
# the Golden images (#42). MSE lays out lines itself, so no SkShaper or SkParagraph.
skia_enable_skottie = false
skia_enable_skshaper = false
skia_enable_skparagraph = false
skia_enable_skunicode = false
skia_enable_tools = false
skia_enable_android_utils = false
skia_use_harfbuzz = true
skia_use_icu = false
skia_use_expat = false

# Fonts: FreeType, and a font manager that knows only the files we hand it
# (the font registry, ADR 0004). No fontconfig, no Android system fonts.
skia_use_freetype = true
skia_use_fontconfig = false
skia_enable_fontmgr_empty = false
skia_enable_fontmgr_custom_empty = true
skia_enable_fontmgr_custom_directory = false
skia_enable_fontmgr_custom_embedded = false
skia_enable_fontmgr_android = false
skia_use_fixed_gamma_text = false

# Codecs: PNG and JPEG through SkCodec, nothing else.
skia_use_libpng_decode = true
skia_use_libpng_encode = true
skia_use_libjpeg_turbo_decode = true
skia_use_libjpeg_turbo_encode = true
skia_use_libwebp_decode = false
skia_use_libwebp_encode = false
skia_use_wuffs = false
skia_use_piex = false
skia_use_dng_sdk = false
skia_use_ndk_images = false
skia_use_zlib = true

# Everything is built from the vendored sources, never the system's.
skia_use_system_zlib = false
skia_use_system_libpng = false
skia_use_system_libjpeg_turbo = false
skia_use_system_freetype2 = false

skia_use_x11 = false
skia_use_perfetto = false
skia_use_partition_alloc = false
EOF
}

target_args() {
  case $1 in
    linux-x64)
      printf 'target_os = "linux"\ntarget_cpu = "x64"\ncc = "%s"\ncxx = "%s"\nar = "%s"\n' "$CC" "$CXX" "$AR" ;;
    android-arm64-v8a|android-x86_64)
      [[ -n ${ANDROID_NDK_HOME:-} && -d $ANDROID_NDK_HOME ]] || die "ANDROID_NDK_HOME is not set to an NDK"
      local cpu=arm64; [[ $1 == android-x86_64 ]] && cpu=x64
      printf 'target_os = "android"\ntarget_cpu = "%s"\nndk = "%s"\nndk_api = %s\n' "$cpu" "$ANDROID_NDK_HOME" "$SKIA_NDK_API" ;;
    *) die "unknown target '$1' (linux-x64, android-arm64-v8a, android-x86_64)" ;;
  esac
}

# Write args.gn into a build directory, only when it changes (so ninja doesn't regenerate needlessly)
write_args() {
  local dir=$1; shift
  mkdir -p "$dir"
  printf '%s\n' "$@" > "$dir/args.gn.new"
  if cmp -s "$dir/args.gn.new" "$dir/args.gn"; then rm "$dir/args.gn.new"; else mv "$dir/args.gn.new" "$dir/args.gn"; fi
}

link_deps
build_gn
for target in "$@"; do
  dir=$OUT/$target
  args=$(common_args; echo; target_args "$target")
  write_args "$dir" "$args"
  (cd "$SKIA" && "$GN" gen "$dir")
  ninja -C "$dir" ${JOBS:+-j "$JOBS"} skia
  # Nothing in Skia's own graph depends on HarfBuzz once SkShaper is off, and gn only loads the
  # build files the root target reaches. So HarfBuzz gets its own build directory, rooted at it.
  write_args "$dir/harfbuzz" "$args" "skia_use_system_harfbuzz = false"
  # (quietly: most arguments have no effect in that smaller graph, and gn warns about each)
  if ! (cd "$SKIA" && "$GN" gen "$dir/harfbuzz" --root-target=//third_party/harfbuzz > "$dir/harfbuzz/gn.log" 2>&1); then
    cat "$dir/harfbuzz/gn.log" >&2; die "gn gen failed for HarfBuzz"
  fi
  ninja -C "$dir/harfbuzz" ${JOBS:+-j "$JOBS"} harfbuzz
  cp -p "$dir/harfbuzz/libharfbuzz.a" "$dir/libharfbuzz.a"
  echo "skia/build.sh: $target -> ${dir#"$ROOT"/}"
done
