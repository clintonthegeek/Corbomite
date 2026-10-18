# Debounce: Corbomite keeps trailing QTimers; the audit's own §20 row contradicts its Correction 11

**Date:** 2026-10-17
**Discovered during:** reconciling the `editor` debounce P1.
**Supersedes / extends:** the 2026-10-03 editor addendum
[`2026-10-03-editor-wrapper-commands-and-keymaps.md`](2026-10-03-editor-wrapper-commands-and-keymaps.md)
§3.4 (Correction 11, correct) versus its §20 CONFIRMED table (wrong) — the two disagree
about `Ml`, and the P1 asked for exactly this re-read.

## Finding

Obsidian's `debounce` (`Ml`, `app/own/0355-Ml.js`) is a **non-resetting leading-arm
throttle**: the first call arms a timer for `timeout`; calls arriving inside the window
update the captured arguments but **do not** push the fire time out; the third argument
switches to a max-wait mode. The 2026-10-03 addendum's **Correction 11** (`:285-315`) reads
this correctly and explicitly says it is "not lodash-style trailing debounce and not
`QTimer::singleShot`-restart semantics".

But the **same addendum's §20 CONFIRMED table** asserts the opposite:

> `TextFileView` 2000 ms **trailing-edge** debounce; `Ml` is lodash `debounce` and no
> `leading` arg is passed — confirmed (`Ml(n.save.bind(n), 2e3)`, `n = !1` default)

`Ml` is not lodash, and the semantics are leading-arm, not trailing-edge. The timing
constant (2000 ms) is right; the mechanism row is wrong and contradicts Correction 11.
Correction 11 is the source-read, executed one and wins.

## The Corbomite choice

Corbomite's debounced sites are standard **restarting single-shot `QTimer`s** — i.e.
trailing-edge, the semantics §20 (wrongly) attributed to Obsidian:

| Site | Interval | Location |
|---|---|---|
| `TextFileView` autosave | 2000 ms | `libs/core/src/TextFileView.cpp:31` (`m_debounceTimer->start()` on every edit) |
| `SearchView` query | 300 ms | `src/plugins/search/SearchView.cpp` |
| `PropertiesView` write-back | 500 ms | `src/plugins/properties/PropertiesView.cpp` |
| `OutlineView` refresh | 500 ms | `src/plugins/outline/OutlineView.cpp` |
| `GraphControlsPanel` search | 300 ms | `src/plugins/graph-view/GraphControlsPanel.cpp` |

This is a **deliberate, low-impact divergence**: the difference is only *when* a burst's
work fires (Obsidian: `firstCall + timeout`, even mid-typing; Corbomite: `lastCall +
timeout`, i.e. after you pause). Nothing reads the intermediate state, so there is no
correctness consequence, and the trailing form is the safer one for autosave. Reproducing a
non-resetting leading-arm throttle would only make a save land in the middle of typing.

The editor-scoped `Ml` sites the audit names (`expandText` 10 ms, `editorSuggest.trigger`
50 ms, `updateLinkPopup` 300 ms) are **Markoff's editor internals**, not Corbomite code, so
there is nothing to port. The 2026-06-10 `expandText` note's mechanism is already corrected
by Correction 11 (magnitude 10 ms right; "fires ≥10 ms after the change" was the wrong
description).

## Action taken

No production code changed. The P1 ("every debounce/timing claim needs re-reading") is
reconciled: the audit's §20 mislabel is recorded here, Corbomite's trailing form is recorded
as a deliberate divergence, and the remaining timing claims are Markoff-owned. The item is
closed.
