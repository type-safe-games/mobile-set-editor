---
status: accepted (supersedes ADR 0001's choice of parent)
---

# Hard fork from GenevensiS MSE 2.6.x, not twanvl 2.1.2

The port forks from the community MSE 2.6.x line (`G-e-n-e-v-e-n-s-i-S/MagicSetEditor2`), merged into `master` with full history at HEAD `be65e962` (2.6.4), instead of staying on twanvl 2.1.2 (`f9d9356d`). The Full Magic Pack targets 2.6.x: 2.1.2 fails fatally on `m15-saga` (`slider` field), breaks `m15-mainframe-dfc` and misses ~30 script functions the M15 styles use, so staying on 2.1.2 would mean re-implementing the fork's data/script/render work by hand. 2.6.x descends from our old base under the same GPL-2+ license. ADR 0001's stance carries over unchanged with the new parent: no merge obligation, no desktop parity.

## Considered Options

- **Stay on 2.1.2 and port the needed features**: re-implements ~+10k lines of core work without its history or fixes.
- **Re-base on 2.6.x and keep re-merging it**: every merge would collide with the incremental wx removal (ADR 0003), which touches every file.

## Consequences

- **Round-trip target is MSE 2.6.x**: `.mse-set` file version 2.6.0 with `uid`, `linked_card_N`, `game_version`/`stylesheet_version`; older 2.x sets are read and upgraded on save. Writing 2.1.2-loadable files is not supported.
- **Following the parent**: core (data/script/render) commits are cherry-picked by hand when a pack update needs them, and the app's **supported MSE version** is bumped explicitly; ADR 0005's refusal of templates with a newer `mse version` keys off that number. GUI commits are ignored.
- **Desktop test host stays on system wx 3.2**: the first commit after the merge shims 2.6's wx ≥3.3.3 requirement (lower the version check; own PNG tEXt read/write in place of `wxIMAGE_OPTION_PNG_DESCRIPTION`, kept real because `<mse-card-data>` lives there; stub `wxWebRequestSync`, `wxWebResponse::GetContentType`, `wxApp::SetAppearance`). The shim dies when `Image` leaves wx.
- **Golden images** are frozen from the merge+shim commit; the pack's 2.6.3 `mse.exe` under Wine is a manual spot-check, not a gate.
- **wx removal (ADRs 0002/0003) grows, unchanged in shape**: ~6 new core→GUI call sites (recent-sets refresh in `mse2.cpp`, `BulkAction` refreshing a `CardListBase`, modal `WebRequestWindow` in `DownloadedImage`, `wxProgressDialog`/`wxMessageDialog` in core), plus the `wxWebRequest` installer and `src_updater/` to delete. `wxFont::AddPrivateFont` is replaced by the font registry (ADR 0004). Alpha-compose `Paste`/`GetSubImage` need own implementations when `Image` leaves wx. Boost.JSON (header-only, NDK-neutral) is a core dependency; its `src.hpp` include must move out of `gui/`.
- **MVP UI for 2.6-only features**: linked cards/faces and the `slider` editor are in; update-cards / `before version` scripts run automatically on load; JSON/CSV card import, dark-mode template files and image-to-symbol conversion are out.
