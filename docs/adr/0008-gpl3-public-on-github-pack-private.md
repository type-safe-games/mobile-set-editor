---
status: accepted
---

# GPL-3.0-or-later, public on GitHub, template pack and goldens private

The fork declares **GPL-3.0-or-later** and publishes its source as a standalone `mobile-set-editor` repository on **GitHub**, push-mirrored from the private Gitea primary. The inherited MSE code is "GPL 2 or later", but the Android app links Apache-2.0 code (Kotlin stdlib, AndroidX/Compose), which the FSF treats as incompatible with GPL-2.0-only and compatible with GPLv3. So the app can only ship under v3, and the repo says so instead of leaving a v2 `COPYING` that disagrees with the binary. The Full Magic Pack and every rendering of it stay private: they carry WotC-derived frames and "All rights reserved" fonts with no license (see the F-Droid research, Gitea #5).

## Considered Options

- **Keep GPL-2.0-or-later on paper**: legal, since the APK would ship under the v3 option, but the declared license wouldn't match what's distributed. Rejected.
- **Codeberg / GitLab**: F-Droid accepts any public git. GitHub won because the MSE lineage (twanvl, haganbmj, GenevensiS) and the pack's users are there.
- **A GitHub fork of GenevensiS**: a hard fork never PRs back (ADRs 0001, 0007), and fork networks hide us from search and steer contributors to the parent. Lineage lives in git history and the credits instead.

## Consequences

- `COPYING` becomes the GPLv3 text. Inherited headers keep "v2 or later". New Android, Kotlin and facade files are GPL-3.0-or-later. A NOTICE explains that the combined work is distributed under v3+. The README and an in-app About/licenses screen credit twanvl, haganbmj and GenevensiS.
- **Name: "Mobile Set Editor".** The README describes it as an Android port of Magic Set Editor and carries a "not affiliated with Wizards of the Coast" disclaimer. The app itself never says "Magic".
- **The `templates/Full-Magic-Pack` submodule is removed.** Build and tests find templates through `MSE_TEMPLATES_DIR`, which defaults to a gitignored `templates/`. History isn't rewritten.
- **Golden images** (ADR 0007) live privately next to the pack and run only where it exists. Public CI renders a small self-authored free **Test fixture** instead. The fixture is test-only and never ships.
- **What's public**: code, `docs/adr/` and `CONTEXT.md`. Agent and tracker tooling (`docs/agents/`, `.claude/`, `CLAUDE.md`, `.serena/`, and `prototypes/`) stays off public `master`: it is gitignored and tracked in a separate private Gitea repo laid over the same working tree (#51), so a plain push-mirror of `master` publishes only the public tree. Earlier history still holds those files; it is not rewritten. Planning stays on Gitea issues. GitHub Issues takes bug reports only, triaged onto Gitea by hand.
- **Timing**: the mirror goes public at the first buildable milestone (2.6 merge + shim, submodule removed, GPLv3 `COPYING`), so the first public tree already matches this ADR.
