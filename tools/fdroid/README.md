# F-Droid build recipe

[`io.github.type_safe_games.mobileseteditor.yml`](io.github.type_safe_games.mobileseteditor.yml) is
the app's metadata for [fdroiddata](https://gitlab.com/fdroid/fdroiddata): what F-Droid builds and
how. It lives here so it can be tested against the source it builds. F-Droid uses the copy in
fdroiddata. Once that copy is merged, F-Droid updates it on its own (see below).

F-Droid builds each ABI on its own from the release tag: `gradle assembleRelease` in `android/app`
(release builds split per ABI), then it takes that ABI's APK (`output:`). The recipe changes nothing
in the build. Every native dependency is a pinned submodule (`third_party/README.md`), and
`tools/skia/build.sh` builds Skia and gn from source (ADR 0011). The build downloads only these:
the NDK, Gradle and its dependencies (checked against `android/gradle/verification-metadata.xml`),
and the Debian packages in `sudo:` (clang to build gn, CMake and ninja). No template pack or font is
fetched or shipped (ADR 0005).

`scandelete:` deletes what `fdroid scanner` flags in the submodules. None of it is built: test
data and fuzzing corpora, Skia's prebuilt `perfhost` and its Android sample apps, and HarfBuzz's and
vello's Rust/WebAssembly sources. Before you add a path there, make sure no build uses it. Check
the gn and CMake build graphs, e.g. `grep -r <path> out/skia/*/build.ninja.d`.

## Versions

`versionName` is the tag without its `v`. Each ABI's `versionCode` is `baseVersionCode * 10` plus
the ABI's digit (arm64-v8a 1, x86_64 2), both set in `android/app/build.gradle.kts`. F-Droid's
update check (`UpdateCheckMode: Tags`) reads `baseVersionCode` and `appVersion` from that file at
the newest tag (`UpdateCheckData`). It makes both versionCodes with `VercodeOperation`.

### Releasing a new version

1. In `android/app/build.gradle.kts`, set `appVersion` and `baseVersionCode`
   (`major * 10000 + minor * 100 + patch`). The build stops if the two don't match.
2. Commit, tag `vX.Y.Z` on that commit, and push both to GitHub.
3. That's all. With `AutoUpdateMode: Version`, F-Droid's checkupdates finds the tag and adds a
   copy of the last build entries with the new `versionName`, `versionCode` and `commit: vX.Y.Z`.
   It also updates `CurrentVersion`/`CurrentVersionCode`.

If a release changes how the app builds (a new ABI, another NDK, other build tools), change the
recipe here and run the steps below on that commit before you tag it. Then send the same change to
fdroiddata as a merge request. To change the NDK, set `ndk:` here and `ndkVersion` in
`android/app/build.gradle.kts`. A new ABI needs the next digit in `abiDigits`, a build entry with
its `output:`, and a line in `VercodeOperation`.

## Running F-Droid's checks and build locally

[`local.sh`](local.sh) runs the steps that fdroiddata's CI and F-Droid's buildserver run. It uses
F-Droid's buildserver image (`registry.gitlab.com/fdroid/fdroidserver:buildserver`, Debian trixie
with the SDK in `/opt/android-sdk`) and fdroidserver from git. You need docker and git. It works in
`out/fdroid`, which grows to about 3.5 GB (most of it the source and its submodules). Each build
compiles Skia and the app for both ABIs, because Gradle builds every split. On 12 cores a build
takes 10 to 15 minutes.

```sh
tools/fdroid/local.sh setup        # fdroidserver, fdroiddata's config, gradlew-fdroid, the image;
                                   # a clone of this repository with the test tag v<appVersion> on HEAD
tools/fdroid/local.sh lint         # fdroid lint + rewritemeta, which must change nothing
tools/fdroid/local.sh scan         # fdroid scanner on each build: must find 0 problems
tools/fdroid/local.sh build        # fdroid build of each versionCode (or: build 1002)
tools/fdroid/local.sh clean        # remove the proxy container and network
```

- **Source.** F-Droid builds the tag from the GitHub repository. To test a commit before it is
  tagged and pushed, `setup` clones this repository into `out/fdroid/source-repo` and tags the
  commit there (`setup <commit>` picks another commit). The tag never leaves that clone. The
  recipe used for `scan` and `build` points `Repo:` at that clone. Its submodules come from the
  URLs in `.gitmodules`, as on the buildserver.
- **lint** checks the committed recipe. `rewritemeta` is fdroiddata's canonical formatting (field
  order, line wrapping). If it reformats the recipe, the step fails and shows the diff: copy its
  version from `out/fdroid/lint/metadata/`.
- **scan** also fetches the source into `out/fdroid/data/build/` with every submodule.
- **build** works like the buildserver (`fdroid build --on-server`). Each versionCode gets a fresh
  container, which runs the recipe's `sudo:`, installs the NDK, and then removes sudo. The build
  doesn't fetch the source again (`--no-refresh`). Its only network goes through a proxy
  ([`proxy/`](proxy/)). The proxy allows only the hosts the build needs: apt, the SDK manager and
  its checksum log, Gradle and its checksum log, Maven repositories, and github.com for the Gradle
  distribution. It logs every connection, and `build` lists them when it is done. A submodule or
  script that tried to download anything else (Skia's `git-sync-deps`, `cipd`, googlesource)
  would be refused and show up there. The unsigned APKs end up in `out/fdroid/data/unsigned/`,
  and each build's log in `out/fdroid/build-<versionCode>.log`.

### Is F-Droid's APK the same as ours?

`tools/release/check-reproducible.sh` builds the unsigned release APKs of `HEAD` twice, from clean
clones. F-Droid's APKs of the same commit should be byte for byte the same as those:

```sh
tools/release/check-reproducible.sh out/repro
cmp out/repro/a/android/app/build/outputs/apk/release/app-arm64-v8a-release-unsigned.apk \
    out/fdroid/data/unsigned/io.github.type_safe_games.mobileseteditor_1001.apk
cmp out/repro/a/android/app/build/outputs/apk/release/app-x86_64-release-unsigned.apk \
    out/fdroid/data/unsigned/io.github.type_safe_games.mobileseteditor_1002.apk
```

They are the same, so F-Droid can publish APKs signed by the project instead of its own builds, if
we ever want that (`Binaries:` and `AllowedAPKSigningKeys:` in the recipe).

To install an unsigned APK on an emulator for testing, sign it with a throwaway key:

```sh
keytool -genkeypair -keystore /tmp/t.jks -storepass throwaway -alias t -keyalg RSA -dname CN=t
$ANDROID_HOME/build-tools/36.1.0/apksigner sign --ks /tmp/t.jks --ks-pass pass:throwaway \
    --out /tmp/mse.apk out/fdroid/data/unsigned/io.github.type_safe_games.mobileseteditor_1002.apk
adb -s emulator-5554 install /tmp/mse.apk
```
