# External-modify merge: Corbomite uses a real three-way merge, not Obsidian's two-sided diff/patch

**Date:** 2026-10-17
**Discovered during:** reconciling the punch-list `views` P1 against its archived closure.
**Supersedes / extends:** records the decision the 2026-10-03 addendum
[`2026-10-03-views-leaf-viewstate-and-deferred-restore.md`](2026-10-03-views-leaf-viewstate-and-deferred-restore.md)
§1 asked for and never got. That addendum's facts stand; this one is the Corbomite choice.
**Relevant cluster plans:** none. Pin lives in `libs/core/src/ThreeWayMerge.cpp` +
`tests/core/tst_threewaymerge.cpp`.

## Finding

Obsidian's external-modify merge is `bZ` (`app/own/1810-bZ.js:2`, confirmed in the
restored 1.13.7 corpus):

```js
function bZ(e, t, n) {
  var i = new mL(), r = i.diff_main(e, t, !0, 0);
  r.length > 2 && (i.diff_cleanupSemantic(r), i.diff_cleanupEfficiency(r));
  var o = i.patch_make(e, r);
  return i.patch_apply(o, n)[0];
}
```

Two-sided diff `base→view`, patch onto disk, keep element `[0]` of `patch_apply` (the
per-patch success booleans are discarded). An overlapping same-line edit is dropped **and**
relocated onto the preceding line — the 2026-10-03 addendum executed `bZ` to prove it.

The 2026-10-03 addendum's Action said a faithful port must either reproduce this **or make
a deliberate, documented divergence**. The archived punch-list closed the item (`[x]`,
`archive/2026-10-17/punch-list-2026-10-17-full.md`) but no document recorded the choice.

## The decision

Corbomite **does not** use `bZ` and **does not** reproduce it. `TextFileView::onExternalModify`
(`libs/core/src/TextFileView.cpp:113-135`) calls
`ThreeWayMerge::threeWayMerge(m_lastSavedData, currentView, freshDisk)` — a line-level LCS
three-way merge (`libs/core/src/ThreeWayMerge.cpp`) — then advances `m_lastSavedData` to the
fresh disk text. There is no diff-match-patch, no two-sided patch, and no per-patch discard.

### What the merge does with an overlapping same-line edit

It keeps **both** edits rather than choosing a winner or dropping one. Executed on the
addendum's own example:

```
base  : "para one\npara two\npara three\n"
local : "para one\npara two USER EDIT\npara three\n"
remote: "para one\npara two DISK EDIT\npara three\n"
merge => "para one\npara two USER EDIT\npara two DISK EDIT\npara three\n"
```

Neither edit is lost and neither is relocated. This is strictly better than `bZ` for the
case `bZ` corrupts, so the divergence is deliberate and worth keeping.

## Why

Same reasoning as the sanitiser and frontmatter divergences: where fidelity would reproduce
*data loss* rather than *observable behaviour*, correctness wins, and the divergence is
recorded so it is a decision rather than an oversight. A vault round-trip is unaffected —
the merge only governs what a user sees after a concurrent external write.

## Residual, stated plainly

The merge is lossless but not a conflict resolver: an overlapping change is **concatenated
(local first, then remote)**, not flagged with a conflict marker. A future conflict UI would
replace the concatenation; until then the property that matters — no silent loss — holds.

The related Obsidian behaviour from the same addendum (§15, §13 Q6) does **not** apply: the
`isPlaintext = false` path that overwrites a dirty buffer with no notice (BasesView) has no
Corbomite counterpart, because `TextFileView` has no `isPlaintext` gate — it always merges.
Recorded here so it is not rediscovered as a gap.

## Action taken

- `ThreeWayMerge.cpp`'s "If both changed, remote wins" comment corrected to describe the
  actual concatenate-both behaviour.
- `tst_threewaymerge` gains a falsifiable same-line-conflict regression (the addendum's
  demanded case) and the misnamed `conflictRemoteWins` is replaced by one that asserts the
  exact merged output.
- `tst_textfileview_merge` gains an end-to-end same-line case through `onExternalModify`.
- The punch-list `views` P1 is reconciled and closed; the `app.embedRegistry` half of that
  item is tracked separately (the `EmbedRegistrar`-unwired P2 plugin item).
