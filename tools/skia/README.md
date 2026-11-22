# Skia build

Skia is built CPU-only with its own gn build, from source, with nothing fetched at build time
([ADR 0004](../../docs/adr/0004-cpu-skia-canvas-unhinted-text-font-registry.md),
[ADR 0011](../../docs/adr/0011-skia-official-gn-build-from-source.md)).

```sh
git submodule update --init third_party/skia third_party/gn third_party/skia-deps
tools/skia/build.sh linux-x64                       # host
ANDROID_NDK_HOME=... tools/skia/build.sh android-arm64-v8a android-x86_64
tools/skia/smoke.sh                                  # draws text from a font file into out/skia/smoke.png
```

Each target ends up in `out/skia/<target>/`: `libskia.a` plus the static libraries of its
dependencies (`libharfbuzz.a`, `libpng.a`, `libjpeg.a`, `libfreetype2.a`, `libzlib.a`,
`libskcms.a`). Link all of them. Include Skia headers as `include/core/...` with `-Ithird_party/skia`. **Skia's public headers
need C++20**, so any file that includes them must be compiled as C++20.

`build.sh` builds `gn` from `third_party/gn` into `out/skia/gn/` on first use, writes `args.gn`
for the target and runs ninja. `args.gn` turns off every GPU, PDF, SVG and XPS backend, Skia's
text modules (SkShaper, SkParagraph, ICU), and every codec except PNG and JPEG.

HarfBuzz is built, but Skia doesn't use it: the Skia Canvas shapes text with it directly, the way
Pango does in the desktop reference, so that text measures exactly as in the Golden images (#42).
With SkShaper off nothing in Skia's gn graph depends on HarfBuzz, and gn only loads the build
files its root target reaches, so `build.sh` runs a second `gn gen` into
`out/skia/<target>/harfbuzz/` rooted at `//third_party/harfbuzz` (with the same arguments plus
`skia_use_system_harfbuzz = false`) and copies `libharfbuzz.a` next to `libskia.a`. The only font manager is the
custom-empty one, which loads only the font files it is given (`SkFontMgr_New_Custom_Empty()`,
then `makeFromFile`/`makeFromData`); fontconfig and Android's system fonts are not used. Android
targets compile against API 26, the app's minSdk (`SKIA_NDK_API` overrides it).

## Using it on the desktop

After `tools/skia/build.sh linux-x64`, two CMake options switch the desktop build to the Skia
backends in `src/gfx/skia/`:

- `-DMSE_SKIA_CODEC=ON`: images are decoded and encoded by Skia instead of wx. The
  `image-codec` test decodes `test/image_codec/images` with both codecs; give it a pack directory
  as a second argument to compare every image in the pack.
- `-DMSE_SKIA_CANVAS=ON`: cards render on the Skia Canvas instead of wx's. This is for headless
  rendering (the CLI, the facade tests, `tools/goldens`) only: the GUI still draws on wx canvases,
  which can't blit from a Skia one. The desktop then also adds the system's font directories to
  the font registry (`MSE_SYSTEM_FONT_DIRS` overrides them), because the Golden images were
  rendered with the system's fonts as well as the pack's.

`MSE_SKIA_LIB_DIR` points at another Skia build. Only `skia_codec.cpp` and `skia_surface.cpp`
include Skia (and HarfBuzz) headers, and they use no MSE headers: the core is C++17 with
`using namespace std`, which clashes with C++20's `std::format_string`. They are built with
`-fno-rtti`, like Skia.

## What is pinned

| Submodule | Pinned to | Licence |
| --- | --- | --- |
| `third_party/skia` | the tip of milestone branch `chrome/m159` | BSD-3-Clause (`LICENSE`) |
| `third_party/gn` | a `main` commit; build tool, not shipped | BSD-3-Clause (`LICENSE`) |
| `third_party/skia-deps/zlib` | the commit Skia's `DEPS` names | Zlib (`LICENSE`) |
| `third_party/skia-deps/libpng` | the commit Skia's `DEPS` names | libpng (`LICENSE`) |
| `third_party/skia-deps/libjpeg-turbo` | the commit Skia's `DEPS` names | IJG, BSD-3-Clause, Zlib (`LICENSE.md`) |
| `third_party/skia-deps/freetype` | the commit Skia's `DEPS` names | FreeType License or GPL-2.0-or-later (`LICENSE.TXT`); we use the FTL |
| `third_party/skia-deps/harfbuzz` | the commit Skia's `DEPS` names | MIT ("Old MIT", `COPYING`) |

That is the whole dependency set this configuration needs: with the `args.gn` above, the
generated ninja files reference no other `third_party/externals/` directory.

HarfBuzz shapes the same as the system HarfBuzz 15.0 that Pango used for the Golden images: on the
75,000 text measurements of the Golden corpus, both give the same widths.

Skia's BUILD files expect its dependencies in `third_party/skia/third_party/externals/<name>`,
inside the Skia checkout, where this repository cannot place submodules. So they live in
`third_party/skia-deps/<name>`, and `build.sh` symlinks each into the place Skia expects (Skia's
`.gitignore` ignores that directory, so the Skia submodule stays clean). Before building,
`build.sh` checks that each one is at the commit Skia's `DEPS` pins, and stops if not.
`tools/git-sync-deps` is never run.

`gen.py` in gn would download a pinned clang through `cipd` unless `CXX` is set; `build.sh`
always sets it (default `clang++`), so building gn fetches nothing either.

## Upgrading Skia

Only at a milestone bump, because every Skia change can move pixels in the Golden images
(ADR 0010, ADR 0011).

1. Move Skia to the new milestone branch:
   ```sh
   git config -f .gitmodules submodule.third_party/skia.branch chrome/mNNN
   git -C third_party/skia fetch --depth 1 origin chrome/mNNN
   git -C third_party/skia checkout FETCH_HEAD
   ```
2. Re-pin each dependency to the commit the new `third_party/skia/DEPS` names (the URL may change
   too; if so, update it in `.gitmodules`):
   ```sh
   for n in zlib libpng libjpeg-turbo freetype harfbuzz; do
     sha=$(sed -nE "s|.*\"third_party/externals/$n\" *: *\"[^@]*@([0-9a-f]{40})\".*|\1|p" third_party/skia/DEPS)
     git -C third_party/skia-deps/$n fetch --depth 1 origin "$sha"
     git -C third_party/skia-deps/$n checkout "$sha"
   done
   ```
3. Remove `out/skia/<target>` and build every target. If the build references a new
   `third_party/externals/<name>` (`grep -rho 'third_party/externals/[^/]*' out/skia/linux-x64 |
   sort -u`), either turn it off in `build.sh` or add it as a pinned submodule under
   `third_party/skia-deps/` and to `DEPS_NAMES` in `build.sh`. Renamed or removed gn arguments show
   up as `gn gen` warnings.
4. Optionally move `third_party/gn` to a newer `main` commit if the new Skia needs it.
5. Run `tools/skia/smoke.sh`, then the Golden image check, and re-capture or explain any change.
