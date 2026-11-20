# Templates are user-imported from a file, never bundled or fetched

The app ships with no Templates and never names, links to or downloads a Template pack. The user imports a zip they obtained themselves (the Full Magic Pack's GitHub zip, a single shared style, or an `.mse-installer`); the app scans it at any depth for `*.mse-<type>` package directories and loose `.ttf`/`.otf` files, and ignores everything else. We chose this because the Full Magic Pack has no license, recreates WotC frame designs and carries "All rights reserved" fonts: F-Droid tolerates non-free *licensed* assets but not unlicensed or infringing ones, and an app that fetches the pack by default invites `NonFreeAdd`/`NonFreeNet` flags or rejection. File import keeps the app itself cleanly distributable while still reaching the real templates, at the cost of an empty first run.

## Considered Options

- **Bundle the pack (compressed)**: impossible on F-Droid for licensing, and ~620 MiB even as xz.
- **In-app download from a built-in URL**: smoothest onboarding, but makes the app the distributor of an unlicensed pack. A user-entered URL (no default) stays open as a post-MVP addition.
- **Ship a self-authored free starter Game + Style** so the app works out of the box: real template design work outside the `magic`-only scope; revisit only if F-Droid review demands a usable first run.
- **Read Templates in place from the zip via SAF**: saves ~800 MiB but the zip can move or lose its grant, and random access into a 35k-entry zip is slow; the pack is 89% PNG so keeping it zipped saves almost nothing anyway.

## Consequences

- Installed Templates are extracted as directories into private internal storage, excluded from Android backup; the size is shown before import is confirmed.
- The core Magic fonts (Beleren, MPlantin, Matrix, Relay) live in no package — the pack ships them loose for desktop users to install into the OS — so import collects loose font files into a shared font store that the font registry reads as a second source (amends ADR 0004).
- Import reuses MSE's installer data model: a picker grouped by `installer group:` with dependencies auto-selected; per-package version comparison (newer replaces, same/older skipped, no downgrade, one version per package); packages with missing dependencies or a newer `mse version:` than the app are refused.
- Each package extracts to a temp directory and is renamed into place, so a crash or full disk never leaves a half-installed Template.

## Amendment (2026-11-20): import also collects spelling dictionaries

Magic Styles spell-check card text with the pack's hunspell dictionaries, and a missing dictionary queues errors that change the Layout dump. So import also copies `.dic`/`.aff` files from any `dictionaries/` directory in the zip into `<template store>/dictionaries/`, where the core looks for them. A different file with the same name replaces the old one, and the host can switch this off per import (#46).
