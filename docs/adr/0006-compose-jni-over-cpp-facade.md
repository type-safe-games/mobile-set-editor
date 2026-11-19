# Jetpack Compose UI over a hand-written JNI binding to a wx-free C++ facade

The Android UI is Kotlin + Jetpack Compose, minSdk 26 (Android 8.0). It talks to the C++ core through a thin, hand-written JNI layer over a small wx-free C++ **facade** that speaks only plain types (UTF-8 strings, integers, pixel buffers, region rectangles). We chose Compose because, with iOS out of scope, the cross-platform stacks' only advantage (shared UI) does not apply, while Compose wins on everything that does: the core can render straight into an Android `Bitmap` via `AndroidBitmap_lockPixels`, the F-Droid build is plain Gradle + NDK with no prebuilt exemptions, the APK baseline is smallest, and there is only one Skia in the process (ours, ADR 0004).

## Considered Options

- **Flutter + dart:ffi**: viable on F-Droid (Flutter SDK prebuilt exemption), but its value is a shared iOS UI we are not building; adds a C-only boundary, a pixel copy into `ui.Image`, a few MB of engine and a second Skia/Impeller.
- **Compose Multiplatform**: identical to Compose on Android; pays off only on iOS, and its artifacts' F-Droid repo trust is unverified.
- **Qt Quick**: no language boundary, but Qt must be built from source with no standard F-Droid recipe, it is the largest option, and LGPL-3 may clash with a GPL-2-only license declaration.
- **Generated bindings (SWIG, Djinni)** instead of hand-written JNI: saves little on a small, coarse boundary, and adds a generator the F-Droid build must run (SWIG's Java is not idiomatic Kotlin; Djinni is barely maintained).
- **JNI directly against core classes** (no facade): fewest layers, but couples JNI to internals being reshaped by the wx removal (ADR 0003) and leaves nothing testable off-device.

## Consequences

- The facade is testable headless on the desktop Linux build, the same green test host the wx removal uses.
- JNI rules apply at the seam: no C++ exceptions cross it (the facade catches and reports through the `ErrorSink`), and native threads must attach to the JVM before calling back.
- minSdk 26 is the floor; older devices are not supported.
- What the facade exposes (sessions, actions, undo, threading, render caching, field regions for tap-to-edit) is decided separately.
