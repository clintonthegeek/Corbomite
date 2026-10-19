# Corbomite — manual test run-through

**One file, maintained across every pass.** Delete a line only when it has been
verified green by a human; never delete one because it "looks fine".

Each item records what was **actually verified by a human** and what was not, so
a later session can tell the difference between "tested" and "asserted in a unit
test". Items marked 🤖 were checked by an automated test; items marked 👤 need a
person. An item with neither is not done.

Last updated: **2026-10-18 (second pass)**, after a live pass driven through
the running dev build (xdotool under XWayland, `/tmp` copies of the starter
vault). That pass found and fixed four defects the offscreen suite could not
see — the editor theme was never saved (§10), a view header never followed its
own name (§13c's canvas read "No file"), the canvas placeholder's buttons were
unreachable under the chrome overlay (§13c), and
`Plugin::onExternalSettingsChange()` never fired (§14) — and turned up one
open data-loss item: canvas edits are buffered until the view unloads and an
external edit to an open `.canvas` is discarded (§13c, punch-list P1).
Sections marked *(verified 2026-10-18)* below were driven in the app.

The one clock-bound item is §13a (File Recovery): start it first, since the
snapshot interval is measured in minutes.

Two things to know before walking this:

- Run it against **copies** of a vault, not a production one. §13b's merge
  moves the source note to the trash on purpose, and §13a's restore overwrites
  a file. The 2026-10-17 L5 soak used `/tmp` copies of DevVault and jsoncanvas;
  that is the right shape for this pass too.
- The dev tree is current with `HEAD` as of 2026-10-18 15:25 — a
  `cmake --build --preset dev` at that point had nothing to compile. The app is
  `build-dev/bin/libCorbomiteApp.so` (the `bin/Corbomite` stub just loads it),
  and bundled plugins are `build-dev/lib/plugins/corbomite/*.so`. If a rebuild
  compiles nothing, what you are running is the tree.

---

## How to build and run

```bash
cmake --build --preset dev -j 3
./build-dev/bin/Corbomite
```

`-j 3`, not `-j 10`: this machine has 4 shared cores. `-j 10` was for the old
dev box.

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

> **2026-10-18 — verified live, and one defect fixed.** Separate **Tabs** and
> **Commands** groups exist (no single "Other" bucket), and disabled actions are
> gone: "Toggle Fold" and "Insert Table…" do not appear, while their enabled
> neighbours do. The verb rows carry their shortcuts (`Move Tab Left
> Ctrl+Shift+PgUp`). One real defect surfaced: `Move Tab to New Window`
> appeared **twice** — once under Tabs (its KAction) and again under Commands
> (the Obsidian-id mirror registered for `hotkeys.json`). Every mirrored id was
> affected; the palette now uses `CommandRegistry::listPaletteCommands()` and
> the mirrors are `paletteHidden`. Re-check by typing `Move Tab` — each row
> should appear once.

- [ ] 👤 Disabled actions are **gone** (no "Toggle Fold", no "Insert Table") *(verified 2026-10-18)*
- [ ] 👤 Separate **Tabs** and **Format** groups exist *(Tabs verified; Format appears with an editor focused)*
- [ ] 👤 No more single "Other" group with ~60 entries *(verified 2026-10-18)*
- [ ] 👤 No row appears twice for the same verb *(fixed 2026-10-18 — see above)*

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
- [ ] 👤 **Alt+1 / Alt+2 / Alt+Last Tab switches tabs** *(fixed 2026-10-18 — the
      tab-jump lookup was pairing two unrelated "group" notions and always came
      back empty, so the shortcut fired and nothing moved. Still needs a hand
      pass: with 3+ tabs open, Alt+2 must land on the second tab, not the
      leftmost one.)*

## 8. Graph provider registration (O5.T4) 👤

> **2026-10-18 — verified live, both directions.** With a **graph** tab focused
> the menubar reads `File Edit View Go Tab Graph Settings Help` — the Graph menu
> is there and the markdown-only menus (Format/Heading/Insert/Table) are gone.
> With a markdown note focused it reads `File Edit View Go Tab Format Heading
> Insert Table Settings Help` — no Graph menu at all, i.e. removed rather than
> greyed. (The first live pass the same day read `… Settings Help Graph`, i.e.
> the provider menu trailing Help; fixed by `moveStandardMenusLast()` — the
> user then confirmed the order on all four tab types.) **Zoom to Fit works**: the graph tab opened zoomed in (only edge lines
> on screen); Shift+1 fitted it, nodes and edges all visible. The in-pane panel
> offers Reset / Filters / Display / Forces. Side finding in the same shot: the
> status bar still showed the previous text view's `Words: 342 Ln 1, Col 1` —
> filed in the punch list.

- [ ] 👤 Open a graph tab → **Graph** menu with 5 verbs appears *(verified 2026-10-18)*
- [ ] 👤 Switch to markdown → it **disappears entirely** (not greyed) *(verified 2026-10-18)*
- [ ] 👤 Animate / Zoom to Fit / Copy screenshot / Reset forces / Open local
      graph each work *(Zoom to Fit verified 2026-10-18; the rest are one click each)*
- [ ] 👤 Zoom-to-fit is greyed until a graph with nodes exists *(not checked — this vault's graph always has nodes)*

## 9. Bugs found on the first pass — verify the fixes

| # | Symptom | Root cause | Status |
|---|---|---|---|
| 9a | `Ctrl+Shift+PgUp/Dn` did nothing; PgUp/PgDown did | Registered `SHIFT \| Key_PageUp` — **`Ctrl` was missing**, while my commit message claimed the Ctrl+Shift parity | fixed `0c4b224f`, 🤖 tested |
| 9b | No Tab menu | Actions went into **Go**, under a comment claiming a "Tab menu" that never existed | fixed `0c4b224f`, 🤖 tested |
| 9c | Editor theme applied before Apply; Cancel couldn't revert | `currentTextChanged` → `setActiveThemeByName` + immediate `save()` | fixed `0c4b224f` — **needs 👤 retest** |
| 9d | Editor theme only worked after touching app theme | `configChanged` → `refreshSystemTheme`, which early-returns unless the name is "Follow system" | **probably fixed, not verified — see §10** |

## 10. ⚠ Settings — the one that is NOT done

> **2026-10-18 — found and fixed; this section is now a confirmation pass.**
> Your report ("app themes work, editor themes don't save — Apply and OK make
> no difference") was a **third** factor, and not `0c4b224f`'s two: the dialog
> never wrote the key at all. `SettingsDialog` is a `KPageDialog` with a
> hand-written `applySettings()` that enumerates widgets by `objectName`, and
> the `markoffTheme` branch was missing — `MarkoffTheme` was never in
> `corbomite-devrc`, so `MainWindow` always read the `"Follow system"` default
> and the editor tracked the app palette forever. Fixed and pinned by
> `tests/dialogs/tst_settingsdialog` (Apply and OK; red without the branch).
> Verified live 2026-10-18 with xdotool: choosing Dark + Apply writes
> `MarkoffTheme=Dark` and the editor repaints while the chrome stays put.
> Walk the sequence below to confirm it the way you'd use it.

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

## 11. The 2026-10-17 L5 soak, and the four fixes it produced 👤

**The soak is discharged — do not re-run it.** It was closed 2026-10-17
(closeout: `decisions-archive.md` → "2026-10-17 — L5 soak closed"); it ran
L5.1–L5.6 on `/tmp` copies of DevVault and jsoncanvas plus one Obsidian round
trip, and found four defects. All four were **fixed and pinned offscreen that
same day, and none has been re-run live** — so the fixes, not the soak, are
what is outstanding. The test named on each line is what pins it; a line that
fails means that test passed for the wrong reason.

- [ ] 👤 Restore a workspace whose popout holds two notes side by side → both
      floating panes **render their views**, not just their tab titles, without
      dragging one onto the other
      (`tst_workspace_popout::restoreFloatingLeaf_isNotDeferred`)
- [ ] 👤 Close the **last tab in a pane** (the pane collapses) → undo close
      restores it as **its own pane on the same side**, not tabbed into the
      survivor (`tst_workspace_session::undoCloseLeafRestoresCollapsedSplitPane`)
- [ ] 👤 Open a vault with **no `workspace.json`** → exactly one empty leaf, no
      second pane, and no tab titled with a raw 16-hex id
      (`tst_workspace_session::missingWorkspaceJsonWithHostNeverZeroHandlerCreatesOneLeaf`)
- [ ] 👤 Arrange splits, quit, then read `workspace.json` → no empty `tabs`
      node (`children: []`) beside a real split. That node was Corbomite's, not
      in the file it had loaded, and Obsidian's own save omitted it.
- [ ] 👤 Regression over the same ground: drag a tab between groups; Split
      Right / Split Down; popout; switch vault with splits and a popout open →
      no crash, no lost tree

**Accepted on 2026-10-17 — do not re-file these two.** A between-group drag
works through the KDDW drop targets but **dropping on the destination tab bar**
does not; and a failed drag leaves the tab drawn askew until the next click.
Both were eyeballed and accepted as-is.

## 11b. Build hygiene — verified by inspection, worth one confirmation 👤

The test gate was unified 2026-10-02 and a real bug was fixed with it. Nothing
here should need you, but if anything below surprises you, say so.

> **2026-10-18 — both checks run by the assistant; one doc figure was wrong, one
> host-side gate fixed.** Dev: **278** tests, **277** with
> `-E 'benchmark|perf'` (the "few hundred" bound holds; the 277 recorded earlier
> the same day is now exact again). Release (`/tmp/rel-check`, fresh configure):
> **15** tests, **none of them Corbomite's** — the box's "~109 submodule tests"
> was stale and is corrected below.
>
> Two leaves were leaking into every configure, including release:
> `markoff-canvas`'s gate defaults **ON** when undefined and was absent from the
> FORCE-OFF list at `CMakeLists.txt:116-121`, so its 2 tests always registered
> (**fixed**: added, release 17 → 15, dev 280 → 278). `markoff-parser` does
> `add_subdirectory(tests)` with **no gate at all** (`markoff-parser/CMakeLists.
> txt:95`), so its 13 tests cannot be switched off from the host — filed upstream
> as ASK-M33. The remaining 2 are ECM's `appstreamtest`. Also inert:
> `MARKOFF_LIVE_BUILD_TESTS`/`MARKOFF_PARSER_BUILD_TESTS` in that list name no
> gate the leaves read; `MARKOFF_CORE/SOURCE/STYLED` are real.

- [ ] 👤 `rm -rf build-dev && cmake --preset dev` from scratch, then confirm
      `ctest -N -E 'benchmark|perf'` in `build-dev` reports a few hundred tests
      (**277** measured 2026-10-18; the number moves with every test added, so
      what matters is the order of magnitude, not the exact figure). It
      previously reported **109** — all from the markoff-family submodule, none
      of them ours — because no preset set the test gate and its default is
      OFF. It only appeared to work because this machine's `build-dev/` cache
      was already configured. If you ever get 109, the gate is off again.
- [ ] 👤 `cmake --preset release -B /tmp/rel-check` then `ctest -N` should show
      **no Corbomite tests** *(verified 2026-10-18 — 15 tests, all
      markoff-family plus `appstreamtest`; the "~109" this box used to predict
      was wrong, see the note above)*.

## 12. Regression sweeps — run before any release 👤

- [ ] Open/close/reopen a vault repeatedly (no stale dock widgets)
- [ ] Toggle Dev, dark and light repeatedly (no palette leak)
- [ ] Open 5+ tabs of mixed types and cycle focus (menus track the focused type)
- [ ] Disable a plugin at runtime, then re-enable
- [ ] `.obsidian/app.json`: rename link format + trash option survive a restart
- [ ] A note whose frontmatter contains `----` — "Add property" must not corrupt it

---

## 13. Cluster F-a + the canvas M6 exit — new 2026-10-17 👤

Four gates, all in the tree, all offscreen-green and none validatable
offscreen. The punch list carries each as its own box.

### 13a. File Recovery + Version history (F-a P1.8)

- [ ] 👤 Settings → **File Recovery** exists: interval in minutes (**0 = Off**,
      default 60), max snapshot age (default 7 days), per-file snapshot count
      (default 50)
- [ ] 👤 Drop the interval to 1–5 min, open a note, type, leave it dirty, wait
      it out
- [ ] 👤 Note hamburger (…) → **Version history** → the modal renders: a list
      newest-first, a read-only preview, **red markers** on save-failure
      entries
- [ ] 👤 **Restore this version** → the file is overwritten on disk **and** the
      leaf reloads from disk. The restore writes through the recovery store,
      not `Vault`, so a leaf that keeps showing the old text is the bug
- [ ] 👤 Close the vault (or quit) → the recovery directory is purged per the
      retention policy

### 13b. "Merge entire file with…" (F-a P3.7) — use a vault copy

- [ ] 👤 Note hamburger → **Merge entire file with…** → stage 1 is a fuzzy
      picker that **excludes the source**, stage 2 asks append or prepend and
      states the destructive consequence
- [ ] 👤 Append a second note into a target, then repeat with prepend → the
      body moves, the source's frontmatter is stripped, the source lands in the
      **configured trash** (`.trash` or system per `.obsidian/app.json`), and
      the target opens
- [ ] 👤 **Cancel** at each stage → both files byte-untouched
- [ ] 👤 Merge a note that a canvas or another note links → the link fix-up
      leaves no dangling reference

### 13c. Canvas M6 exit — rename rewriting (M6.2) + missing-file (M6.3)

> **2026-10-18 — M6.3's half passes live; M6.2 is still to check.** Create,
> Locate…, Remove and the vault-event auto-resolve were all driven in the app
> (screenshots in the session): Create wrote the file and the card became a
> real card in place; Locate… opened the picker and rewrote the node;
> Remove deleted the node; creating the target outside the app resolved the
> card with no reopen, and deleting it turned the card back into the
> placeholder. **One defect found and fixed in that pass:** the buttons
> painted but could not be **pressed** — the press was routed through
> `QGraphicsScene::itemAt()` and the chrome overlay sits above every node, so
> the topmost item was never the card. Now routed by walking the item stack,
> pinned by `tst_canvas_missing_file::pressOnButtonReachesItThroughAnItemAbove`.
>
> **New, and fixed the same day:** canvas edits used to be buffered in memory
> and written only when the view unloaded, and an external edit to an open
> `.canvas` was discarded. Both are gone: a mutation now autosaves after the
> `[Editor] Autosave delay` (2 s default, the same setting notes use), and an
> external write is adopted when you have nothing unsaved — with a status-bar
> message — or, when you do, your version is kept and the app says so in the
> status bar **and** as a desktop notification if the window is not in front of
> you ("saving will replace the file"). Both were driven live 2026-10-18
> (notification seen on the desktop). Pinned by `tst_canvas_persistence`
> (autosave, adopt-on-reload, and the policy cases).

- [ ] 👤 Rename a note that a canvas points at — a **file card**, a **group
      background**, a **text-card link** → the `.canvas` is rewritten, the
      canvas still renders, and the link shows as resolved in
      backlinks/outgoing
- [ ] 👤 Rename **the canvas itself** → same
- [ ] 👤 Open a canvas whose file card points at a file that does not exist →
      the card paints **"File not found: <path>"** with **Create / Locate… /
      Remove** *(verified 2026-10-18)*
- [ ] 👤 The three buttons hover and click (their paint state and hit-testing
      are exactly what the offscreen cases cannot see) *(click verified
      2026-10-18; hover/cursor feel still a human call)*
- [ ] 👤 **Create** writes the file and re-renders the card in place;
      **Locate…** re-picks and re-renders; **Remove** deletes the node
      *(all three verified 2026-10-18)*
- [ ] 👤 With the canvas still open, create or rename a file into that missing
      path from outside → the card resolves **without reopening the canvas**
      *(verified 2026-10-18, both directions: create resolves, delete dangles
      again)*

---

## 14. Kiln panel + Settings → Plugins — new 2026-10-18 👤

Landed in `c3365ee3` / `50cb9c9e` / `277c006b` and `190cf24e`. No trajectory
step names either, so these lines are the only gate they have.

> **2026-10-18 — §14a partly walked, §14b fully walked, both pass so far.** The
> panel opens in the right dock (cramped at its default width); with the engine
> at `~/dev/Kiln/build/kiln`, a projection over `PKM LM` and a site directory,
> **Check** produced a real dry run (30 notes selected, nothing written) and
> **Project** wrote the Hugo tree; a **wrong** engine path is reported in the
> panel instead of failing silently. Setting the panel up by hand-editing
> `.obsidian/plugins/corbomite-kiln/data.json` **now reaches the open panel** —
> it did not, because `Plugin::onExternalSettingsChange()` never fired in
> production (the watch install was a stub); fixed and pinned in
> `tst_plugin_external_settings`. §14b: the detail pane is populated on open,
> row switching leaves no stale painting, and there is no Configure… button
> anywhere — the correct result.

### 14a. The Kiln panel

- [ ] 👤 Command palette → **Kiln: open the panel** (and **Kiln: check the
      projection** / **Kiln: write the pages**) → the panel opens in the right
      dock area
- [ ] 👤 Set the engine with **Browse…** — `kiln` is **not** on `PATH`; the
      built binary is `~/dev/Kiln/build/kiln`
- [ ] 👤 Point the engine at a **wrong** path and run → the panel says the
      engine failed to start. A silent no-op is the failure mode here: the
      runner only learns about a bad path from `FailedToStart`, never from
      `finished()`
- [ ] 👤 Add and remove a projection; set its **Folders**, **Held back**,
      **Running order**, **Section**, **Shape** (bundle|leaf), **Front
      matter**, **Fields**, **Link base** and **Taxonomies** → the **section
      list matches what the engine itself derives**, including the **"A nested
      tag also carries its parents"** switch
- [ ] 👤 **Check** (tooltip: writes nothing) against a real vault → the report
      renders the engine's findings; **Project** and **Write the site shell**
      write to the configured **Site directory**; **Reveal** and **Copy
      config** work (the copied JSON is what `kiln check --config -` reads)
- [ ] 👤 Hand-edit the plugin's `data.json` on disk → the panel reloads. Its own
      save must not re-enter (it compares before acting)

### 14b. Settings → Plugins detail pane

- [ ] 👤 Settings → **Plugins** → the detail pane is **populated on open** for
      the initially selected plugin. It used to be empty until you selected
      another row and came back
- [ ] 👤 Click down the plugin list → nothing is left painting at the previous
      row's geometry
- [ ] 👤 **No Configure… button anywhere** — that is the correct result: no
      bundled plugin offers `configPages()` today, so the button is exercised
      only by `tst_plugins_page`'s fake plugin
- [ ] 👤 Disable and re-enable a plugin → list and detail pane stay consistent

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
| §11 | Cluster L5 soak, 2026-08-17; **rewritten 2026-10-18** — the soak closed 2026-10-17, and its four fixes are the open half |
| §11b | Build-hygiene pass, 2026-10-02 (test-gate unification) |
| §12 | Standing regressions; §12's last line is the frontmatter-fence P1 fix |
| §13 | The 2026-10-17 gates: Cluster F-a (P1.8, P3.7) and the canvas M6 exit (M6.2, M6.3) |
| §14 | Surfaces landed 2026-10-18: the Kiln panel and Settings → Plugins `190cf24e` |

Automated coverage lives in the suite, not here — if an item above says 🤖, the
assertion is in `tests/`. This file is for the things a test cannot see.