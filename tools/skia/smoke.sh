#!/usr/bin/env bash
# Build and run the Skia smoke program against out/skia/linux-x64.
# Skia m159 public headers need C++20.
#   tools/skia/smoke.sh [font.ttf] [out.png]
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
LIB=$ROOT/out/skia/linux-x64
FONT=${1:-$ROOT/test/fixture/fonts/DejaVuSans.ttf}
PNG=${2:-$ROOT/out/skia/smoke.png}
[[ -f $LIB/libskia.a ]] || "$ROOT/tools/skia/build.sh" linux-x64
"${CXX:-clang++}" -std=c++20 -O1 -I"$ROOT/third_party/skia" "$ROOT/tools/skia/smoke.cpp" \
  -L"$LIB" -lskia -lpng -ljpeg -lfreetype2 -lzlib -lskcms -lpthread -ldl \
  -o "$ROOT/out/skia/smoke"
"$ROOT/out/skia/smoke" "$FONT" "$PNG"
echo "wrote ${PNG}"
