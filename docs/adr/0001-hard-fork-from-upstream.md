---
status: parent superseded by ADR 0007 (fork from GenevensiS MSE 2.6.x); the hard-fork stance stands
---

# Hard fork from upstream MagicSetEditor2

The mobile port is a hard fork of `twanvl/MagicSetEditor2`: we refactor the C++ core freely (including removing wxWidgets from it) with no obligation to stay mergeable with upstream or to keep the desktop wx build working. Upstream is low-activity and its toolchain (wx 3.0, boost ≤1.76) already conflicts with current versions, so staying mergeable would constrain the port's biggest decisions for little gain.

## Consequences

- Upstream fixes are cherry-picked by hand, if at all.
- The one compatibility promise kept is the `.mse-set` file format, which must still round-trip with desktop MSE 2.
