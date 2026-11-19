---
status: accepted
---

# Skia is built with its own gn build, from source, pinned to milestone branches

F-Droid builds everything from source, and Skia's official build uses `gn` plus `git-sync-deps`, which fetches during the build. We keep the official build. The F-Droid recipe builds `gn` from source. Skia is a submodule pinned to a Skia milestone branch, and each third-party dependency the CPU-only configuration needs is its own pinned submodule, so nothing is fetched at build time. Gradle runs `gn`/`ninja` for each ABI, and the app's CMake links the result. Skia is upgraded only at milestone bumps, so Golden images (ADR 0010) don't change because of Skia updates in between.

## Considered Options

- **Our own CMake build of Skia (OsmAnd's approach)**: one build system and no `gn`, but Skia's source lists would have to be kept in sync by hand on every upgrade, forever. Building `gn` costs one ninja invocation.
