# Corbomite — manual test run-through

**One file, maintained across every pass.** Delete a line only when it has been
verified green by a human; never delete one because it "looks fine".

Each item records what was **actually verified by a human** and what was not, so
a later session can tell the difference between "tested" and "asserted in a unit
test". Items marked 🤖 were checked by an automated test; items marked 👤 need a
person. An item with neither is not done.

Last updated: **2026-10-02**, after the first inspector pass.

---

## How to build and run

```bash
cmake --build --preset dev -j 10
./build-dev/bin/Corbomite
```

The dev build is isolated: config `~/.config/corbomite-devrc`, data
`~/.local/share/corbomite-dev/`, window title `Corbomite [Dev]`. It will not
disturb an installed release build.

If a dev build's menus or toolbars look stale after a rebuild, delete
`~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc` and relaunch — the
KXMLGUI cache is shared by *every* dev build on this machine, including ones run
from other worktrees.

---

## 1. Canvas dark mode ✅ 👤

**Verified good by the user.** "The actual rendering of the canvas in dark mode
is great."

Still worth a repeat when other work lands, because a related defect was found
here (see §9):

- [ ] Dark theme: cards clearly distinct from the background (**this is the claim
      that mattered** — white-on-white cards were the original bug)
- [ ] Text, group labels, card borders, edges and grid all readable
- [ ] A group with a label; an edge with a label
- [ ] Export PNG **and** SVG → background dark, not white
- [ ] Switch to Light → fully reverts, no grey leftovers

## 2. Bases toolbar (O5.T1) — appearance must be unchanged 👤

The seven in-view buttons are now `setDefaultAction()` carriers. They must look
and behave **exactly** as they did before.

- [ ] Properties / Sort+group / Views panels open **under the same buttons**
- [ ] Properties pane toggles the drawer; check mark stays in sync
- [ ] **+New** creates an entry; greyed with no vault, live with one
- [ ] **Results** dropdown opens its menu (this had a Qt bug that left it
      opening nothing)
- [ ] The same operations via the new **Bases** menubar menu

## 3. Editor verbs (O7.T1) — 3 of 4 are new 🤖 + 👤

- [ ] 🤖 The three exist and carry no blocker property
- [ ] 👤 **Insert Wiki Link** (Ctrl+Shift+K) actually inserts. It was never
      blocked on Markoff — the verb existed and was simply never wired
- [ ] 👤 Cursor in an H2 → Increase ×3 → H5; Decrease back down
- [ ] 👤 In a plain paragraph **Decrease is greyed** (level 0 cannot go to −1)
- [ ] 👤 In an H6 **Increase is greyed**
- [ ] 👤 All three greyed with no editor focused
- [ ] 👤 **Editor theme dropdown is deferred to Apply** — and the *app* theme
      dropdown too. See §9; only the editor one had its handler removed so far.

## 4. Placebo dialogs (O7.T4) 🤖 + 👤

`Insert → Table…` and `Insert → Callout…` used to open a dialog, swallow the
input, and do nothing.

- [ ] 🤖 Both are disabled and state a blocker
- [ ] 👤 Hovering shows *why* ("blocked on: …")
- [ ] 👤 Confirm they no longer open a dialog at all

## 5. Graph controls panel (O6.T3) — the crash-class fix 🤖 + 👤

- [ ] 🤖 Two tabs get distinct panels; one tab's slider leaves the other alone
- [ ] 🤖 `createView()` returns nullptr
- [ ] 👤 **Open two graph tabs, move a slider in one, confirm the other does not
      move.** This is the real regression
- [ ] 👤 Each tab has its own controls; the right-hand dock panel is gone
- [ ] 👤 Panel looks correct in its new in-pane position

## 6. Command palette (O7.T5) 👤

- [ ] 👤 Disabled actions are **gone** (no "Toggle Fold", no "Insert Table")
- [ ] 👤 Separate **Tabs** and **Format** groups exist
- [ ] 👤 No more single "Other" group with ~60 entries

## 7. Tab reorder + linked panes (O5.T5) 🤖 + 👤

**Two bugs were found here on the first pass — see §9.** Re-run all of it.

- [ ] 🤖 Shortcuts are the *full* `Ctrl+Shift+PgUp` / `PgDn`
- [ ] 🤖 A `&Tab` menu exists and holds all three actions
- [ ] 👤 **Ctrl+Shift+PgUp / PgDn moves the tab**, greyed at the ends, no wrapping
- [ ] 👤 **Tab menu** visible in the menubar (after Go)
- [ ] 👤 **Link with Active Pane**: focus note A, then note B, trigger it → both
      linked; trigger again → unlinked
- [ ] 👤 **Pin a tab first, then link** → the other becomes pinned too
      (`workspace.md:392`: joining a pinned group forces the new member pinned)

## 8. Graph provider registration (O5.T4) 👤

- [ ] 👤 Open a graph tab → **Graph** menu with 5 verbs appears
- [ ] 👤 Switch to markdown → it **disappears entirely** (not greyed)
- [ ] 👤 Animate / Zoom to Fit / Copy screenshot / Reset forces / Open local
      graph each work
- [ ] 👤 Zoom-to-fit is greyed until a graph with nodes exists

## 9. Bugs found on the first pass — verify the fixes

| # | Symptom | Root cause | Status |
|---|---|---|---|
| 9a | `Ctrl+Shift+PgUp/Dn` did nothing; PgUp/PgDown did | Registered `SHIFT \| Key_PageUp` — **`Ctrl` was missing**, while my commit message claimed the Ctrl+Shift parity | fixed `0c4b224f`, 🤖 tested |
| 9b | No Tab menu | Actions went into **Go**, under a comment claiming a "Tab menu" that never existed | fixed `0c4b224f`, 🤖 tested |
| 9c | Editor theme applied before Apply; Cancel couldn't revert | `currentTextChanged` → `setActiveThemeByName` + immediate `save()` | fixed `0c4b224f` — **needs 👤 retest** |
| 9d | Editor theme only worked after touching app theme | `configChanged` → `refreshSystemTheme`, which early-returns unless the name is "Follow system" | **probably fixed, not verified — see §10** |

## 10. ⚠ Settings — the one that is NOT done

Two causes were fixed (9c, 9d). **They were diagnosed from source, not
reproduced by running the dialog**, and your description may involve a third
factor. Please walk the exact sequence:

- [ ] Set app theme **Light** and editor theme **Light**. Apply.
- [ ] Change **only** the editor theme to **Dark**. Apply.
- [ ] 👤 Did the editor go dark? (expected: yes)
- [ ] Change **only** the app theme to **Dark**. Apply.
- [ ] 👤 Did the editor follow? (expected: yes, via "Follow system" palette)
- [ ] Change **only** the editor theme to **Light**. Apply.
- [ ] 👤 Did the editor go light? (expected: yes — this is the case that
      previously did nothing)
- [ ] 👤 Press **Cancel** instead of Apply after changing a theme — nothing
      should change on screen or in `corbomite-devrc`

If step 3 or 6 fails, say so and I will dig into the Markoff leaf appliers
(`markoff-live`'s `binding->setTheme`, `markoff-styled`'s `m_styleApplier`) — I
verified both *push* the theme, but not that either has a live view attached.

## 11. Carried over from earlier passes 👤

- [ ] **Cluster L5 soak** — 6 scenarios, checkboxed in the plan
- [ ] **L5 human-eyeball backlog** from the 2026-08-17 soak
- [ ] Drag a tab between groups; split; popout; close-undo; vault switch

## 12. Regression sweeps — run before any release 👤

- [ ] Open/close/reopen a vault repeatedly (no stale dock widgets)
- [ ] Toggle Dev, dark and light repeatedly (no palette leak)
- [ ] Open 5+ tabs of mixed types and cycle focus (menus track the focused type)
- [ ] Disable a plugin at runtime, then re-enable
- [ ] `.obsidian/app.json`: rename link format + trash option survive a restart
- [ ] A note whose frontmatter contains `----` — "Add property" must not corrupt it

---

## Reporting back

For anything that fails, the three most useful things are:

1. **What you did**, step by step.
2. **What you expected** and **what happened**.
3. Whether it is **reproducible every time** or intermittent — and if
   intermittent, roughly how often.

That is enough for me to root-cause most things without guessing.

---

## Provenance

| Item | Origin |
|---|---|
| §1–§8 | First inspector pass, 2026-10-02 |
| §9, §10 | Bugs reported in that pass |
| §11 | Cluster L5, 2026-08-17 |
| §12 | Standing regressions; §12's last line is the frontmatter-fence P1 fix |

Automated coverage lives in the suite, not here — if an item above says 🤖, the
assertion is in `tests/`. This file is for the things a test cannot see.