# Mobile Set Editor

Mobile Set Editor is an Android port of [Magic Set Editor](https://github.com/twanvl/MagicSetEditor2)
(MSE), the program for designing your own trading cards. It renders cards with MSE's existing
game and style templates, which you import yourself.

The Android app is not released yet. Today the repository builds MSE's C++ core, a wx-free facade
over it, and a desktop test host that runs the tests headlessly.

Mobile Set Editor is not affiliated with, endorsed by or sponsored by Wizards of the Coast.
Magic: The Gathering is a trademark of Wizards of the Coast LLC.

## Credits

Mobile Set Editor is a hard fork of Magic Set Editor 2 and builds on the work of:

- **Twan van Laarhoven ([twanvl](https://github.com/twanvl/MagicSetEditor2))**, who wrote Magic Set
  Editor, and the other MSE developers;
- **[haganbmj](https://github.com/haganbmj/MagicSetEditor2)**, who maintained the 2.x fork;
- **[GenevensiS](https://github.com/G-e-n-e-v-e-n-s-i-S/MagicSetEditor2)**, whose 2.6 line this
  port is forked from.

The full history is kept in this repository's git log.

## License

GPL-3.0-or-later; see [COPYING](COPYING). Files inherited from Magic Set Editor are "GPL 2 or
later", and the combined work is distributed under version 3 or later; [NOTICE](NOTICE) explains
why and lists the third-party components.

## Templates

Nothing is bundled and nothing is downloaded. The app will import a zip of templates that you
obtained yourself (a template pack, a single shared style, or an `.mse-installer`), and installs
the `*.mse-*` packages and loose font files it finds inside. See
[ADR 0005](docs/adr/0005-templates-user-imported-never-bundled.md).

For development on the desktop, `templates/` is gitignored: put a pack there by hand, for example
copy or symlink the Full Magic Pack to `templates/Full-Magic-Pack` (its `data/` and
`Magic - Fonts/` directories). The desktop host reads templates from the directory in
`MSE_TEMPLATES_DIR`:

```sh
MSE_TEMPLATES_DIR=$PWD/templates/Full-Magic-Pack/data ./build/magicseteditor --cli my-set.mse-set
```

## Building the desktop test host

Linux is the tested platform. You need a C++17 compiler, CMake 3.13 or newer, pkg-config and the
development packages of wxWidgets 3.2 or newer (GTK 3), Boost (regex, json), hunspell, zlib, Pango
and fontconfig. Tests also need Perl, and `xvfb-run` when there is no display.

```sh
git clone <this repository> mobile-set-editor
cd mobile-set-editor
git submodule update --init third_party/minizip-ng
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
cd build && xvfb-run -a ctest --output-on-failure
```

The tests render the [Test fixture](test/fixture/README.md), a tiny self-authored Game and Style
with free fonts, so they need no template pack. The golden comparison in
[`tools/goldens`](tools/goldens/goldens.py) runs on the fixture alone with
`python tools/goldens/goldens.py check --fixture-only`; with a pack in `templates/` it also
checks the private Tier 1 and Tier 2 corpora
([ADR 0010](docs/adr/0010-two-tier-golden-corpus-layout-dump.md)).

## Building the Android app

Planned. The app will be Jetpack Compose over the same C++ facade through JNI
([ADR 0006](docs/adr/0006-compose-jni-over-cpp-facade.md)).

The core and facade already build for Android, from pinned submodules only (Skia, Boost,
Hunspell: [`third_party/README.md`](third_party/README.md)), into `libmse.so` with the Skia image
codec and Canvas. With an NDK (`ANDROID_NDK_HOME`):

```sh
git submodule update --init third_party
tools/skia/build.sh android-x86_64          # or android-arm64-v8a
cmake -S . -B build-android-x86_64 -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release
cmake --build build-android-x86_64
tools/android/run-facade-tests.sh build-android-x86_64   # the facade tests, on a running emulator
```

On the desktop, `cmake -DMSE_SKIA_CODEC=ON -DMSE_SKIA_CANVAS=ON` (after `tools/skia/build.sh
linux-x64`) renders with the same Skia backends, which `tools/goldens` checks against the Golden
images, and adds the `facade-skia` test: the facade tests on the wx-free host Android uses.
See [`tools/skia/README.md`](tools/skia/README.md).

## Documentation

- [`docs/adr/`](docs/adr/): the architecture decisions behind the port.
- [`CONTEXT.md`](CONTEXT.md): the project's vocabulary.
- [`doc/`](doc/): Magic Set Editor's own template and scripting documentation.
- [`CHANGES.md`](CHANGES.md): the changelog inherited from Magic Set Editor.
