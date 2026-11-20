# Mobile Set Editor

A fork of Magic Set Editor 2 (C++17, wxWidgets, CMake; upstream `twanvl/MagicSetEditor2`)
being ported to mobile, Android first. Game and style templates come from the
`templates/Full-Magic-Pack` submodule (~1 GB; run `git submodule update --init` if empty).
minizip-ng is vendored as a pinned submodule in `third_party/minizip-ng` (CMake stops if it's missing).

- `doc/` is MSE's own user/template documentation. `docs/` is this project's agent and
  design docs (`docs/agents/`, `docs/adr/`). Don't mix them.
- wx is being removed from the core in ADR 0003's order. `String` is now `std::wstring`
  (free functions in `src/util/string.hpp`), but `Image`/`Bitmap`/`DC` are still wx types
  (`src/util/prec.hpp`). The core reaches the GUI only through host hooks, which the
  desktop app installs in `src/gui/host_hooks.cpp`.
- Headless testing goes through the CLI: `magicseteditor --cli <script.mse-script>`
  (see `test/tests.cmake`).

## Agent skills

### Issue tracker

Gitea issues at `gitea.home/admin/mobile-set-editor`, driven by curl against the REST API. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five-role vocabulary (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `CONTEXT.md` and `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
