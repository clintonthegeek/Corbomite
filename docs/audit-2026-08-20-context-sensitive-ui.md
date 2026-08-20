# Context-sensitive UI — action, menu, toolbar & sidebar audit

**Date:** 2026-08-20
**Status:** Report / pre-plan investigation. No code written. Feeds
[`superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md`](superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md).
**Deliverable requested:** full inventory of KActions and chrome tagged by document
type; a decision on whether document type is the right discriminator; a decision on
whether sidebars should be contextual; ambiguities for user redress.

---

## 0. Executive summary

Corbomite has **five disjoint action surfaces** that do not know about each other.
The static menubar is one of them, and it is the only one wired to shortcuts, the
command palette, and the Hotkeys settings page — and it is shaped exclusively for
markdown.

The headline number:

> **Of the ~79 `QAction`s in `MainWindow`'s `KActionCollection`, exactly zero belong
> to canvas, bases, or graph.** 39 are markdown-only. 17 are permanently disabled
> stubs. 2 more are placebo dialogs that discard their result. The entire top-level
> **Table** menu (6/6) and the entire **View ▸ fold** group (3/3) are greyed out on
> every launch and always will be until Markoff grows those verbs.

Three findings reframe the cluster:

1. **The polymorphic seams already exist and have rotted.** `View::zoomIn/zoomOut/
   zoomReset` are base virtuals added by legacy Cluster V precisely so app-shell
   actions could route to whichever view is active. `MainWindow`'s zoom actions
   **bypass them** and go straight to `activeEditor()->activeLeaf()->setFontScale()`
   (`MainWindow.cpp:707-729`), and `MarkdownView`'s three overrides are stale
   `TODO` no-ops (`MarkdownView.cpp:87-103`). Canvas and graph — both of which have
   real zoom — get nothing. Cluster O is less "invent a mechanism" than "re-light
   the mechanism that was designed for this and abandoned during the port."

2. **Document type is the right *primary* key but is not sufficient.** Reading mode
   is the same `viewType` as Live/Source with a different capability set; canvas
   read-only lock, bases per-view config, and selection-gated verbs (canvas
   "Convert to file", markdown table ops) are all same-type/different-availability.
   The report proposes a **three-tier model** (presence / enablement / check-state)
   with type keying only the first tier. §5.

3. **Sidebars should *not* be contextual — the graph controls panel is not a model
   to copy, it is a misplacement to fix.** In Obsidian the graph's Filters/Display/
   Forces panel is view-local chrome inside the graph pane; Corbomite docked it as a
   shared right-sidebar tool view. The consequence is a live bug: one singleton
   `GraphControlsPanel` is wired by every `GraphViewTab` with no disconnect, so two
   graph tabs both react to one slider. The thing worth duplicating for other types
   is the *pane-local inspector*, not the sidebar leaf. §7.

Recommended mechanism: **per-view-type `KXMLGUIClient` providers, swapped on the
factory** — the KParts/Kate pattern, already present in-tree as `CorbomiteMDI::
GUIClient`. §6.

---

## 1. Method

Read in full: `src/app/MainWindow.{h,cpp}` (3126 + 327 lines), `src/app/corbomiteui.rc.in`,
`src/mdi/CorbomiteMDI.{h,cpp}` action/sidebar layers, `libs/core` view hierarchy
(`View`, `ItemView`, `FileView`, `EditableFileView`, `TextFileView`, `EmptyView`,
`ViewRegistry`, `Workspace`, `WorkspaceLeaf`, `MenuSectionHelper`, `Command.h`),
`src/canvas/*`, `libs/canvas/CanvasAlignmentStrategy`, `libs/bases/BasesView`,
`src/plugins/graph-view/*`, `src/app/Ribbon*`, all nine core-plugin `metadata.json.in`.

Audit corpus read: `obsidian-audit/domains/{core,workspace,views,canvas,editor-markdown,
ui-bundle}.md` §6/§9/§10, `addenda/2026-04-19-graph-screenshot.md`,
`PARITY-MATRIX.md` §4–5, `punch-list.md`, the Cluster I / Cluster R plans, and
`specs/2026-06-10-mainwindow-decomposition-design.md`.

Everything asserted below with a `file:line` was read, not inferred. Claims about
Obsidian behaviour not covered by the audit corpus are flagged as such.

**Doc-drift note:** `CLAUDE.md` says local KDE sources are checked out at
`~/src/kde/src/<repo>`. That directory does not exist on this machine (`~/src`
contains `codemirror`, `OrgModeParser`, `qtbase`). KXMLGUI API claims below were
verified against the installed headers at `/usr/include/KF6/KXmlGui/`, not against
Kate's source.

---

## 2. The five action surfaces

| # | Surface | Owner | Reaches menubar? | Toolbar? | Shortcuts? | Palette? | Hotkeys page? | Context-aware? |
|---|---|---|---|---|---|---|---|---|
| 1 | `KActionCollection` + `corbomiteui.rc` | `MainWindow::setupActions` (`:1279-1815`) | ✅ | ✅ main | ✅ | ✅ | ✅ | ❌ static |
| 2 | Per-pane hamburger (`onMoreOptionsMenu` + `MenuSectionHelper`) | each `View` subclass | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ fully |
| 3 | In-view widget toolbars | `BasesView` (7 `QToolButton`s, `BasesView.cpp:74-128`), canvas context menus | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ inherently |
| 4 | `CommandRegistry` (Obsidian `Command` + `checkCallback`) | `libs/core/Command.h` | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ via `checkCallback` |
| 5 | `RibbonToolBar` (top-docked, plugin ribbon icons) | `src/app/RibbonToolBar` | ❌ | ✅ own | ❌ | ❌ | ❌ | ❌ |
| — | `CorbomiteMDI::GUIClient` (a 2nd XMLGUI client, tool-view actions) | `src/mdi/CorbomiteMDI.cpp:78-131` | ✅ View menu | ❌ | ✅ | ❌ (separate collection) | ✅ | ✅ per tool view |

Observations:

- **Surface 2 is already exactly what Cluster O wants** — contextual, per-view-type,
  Obsidian-section-ordered, implemented by all four view types (`MarkdownView.h:82`,
  `CanvasFileView.h:39`, `GraphView.h:45`, `EditableFileView.h:54`). It is invisible
  to shortcuts, the palette, and hotkey configuration. Cluster O's job is largely to
  give surfaces 1 and 2 a shared vocabulary, not to build a third thing.
- **Surface 4 already implements Obsidian's availability doctrine** —
  `Command::checkCallback(bool checking)`, `listAvailable()`, `isAvailable(id)`
  (`Command.h:38-100`). It is palette-only. Nothing in the menubar consults it.
- **Surface 5 is empty.** `grep addRibbonIcon` finds only the plumbing
  (`RibbonRegistrar.cpp`, `Plugin.cpp`) — **no core plugin registers a ribbon icon**,
  so the ribbon toolbar renders as a permanently blank strip to the right of the main
  toolbar in every session.
- **`ItemView::addAction(icon, title, callback)`** — Obsidian's `view.addAction`
  (per-view header icon strip, `editor-markdown.md` §6.1) — exists
  (`ItemView.h:27-28`) and is used by exactly one class: `EmptyView`
  (`EmptyView.cpp:44-49`). Cluster R built the hamburger and deliberately left the
  header strip unused. It is a live, tested seam sitting idle.

---

## 3. Complete KAction inventory, classified

Classification key:

- **U** — universal (meaningful for any focused tab, or for no tab)
- **W** — workspace-scoped (tab/leaf/vault plumbing; universal in practice)
- **M** — markdown-only
- **∅** — registered but permanently disabled (stub, no implementation)
- **☠** — enabled but silently does nothing on non-markdown tabs (**bug class**)

### 3.1 File

| Action | Class | Notes |
|---|---|---|
| `file_open_vault` | U | |
| `file_open_recent` | U | `KRecentFilesAction` |
| `file_close_vault` | W | vault-gated (`updateVaultActions:2974`) |
| `file_new_note` | W | vault-gated |
| `file_new_canvas` | W | vault-gated |
| `file_save` | ☠ | **Ctrl+S is a no-op on a canvas tab.** `saveCurrentNote` (`:2268-2286`) handles `activeEditor()` then `TextFileView`; `CanvasFileView` is a `FileView`, not a `TextFileView`. Canvas only ever saves in `onUnloadFile` (`CanvasFileView.cpp:71-73`). |
| `file_quit` | U | `KStandardAction` |
| `options_configure` | U | `KStandardAction`, Ctrl+, |

### 3.2 Edit

| Action | Class | Notes |
|---|---|---|
| `edit_undo` / `edit_redo` | U | Correctly type-dispatched already (bases → canvas → editor ladder, `:1331-1361`) — the one place `MainWindow` does the right thing. Never enable-gated, though: always enabled regardless of stack depth. |
| `edit_find` | ☠ | `onFind` returns silently if `activeEditor()` is null (`:676-681`). Ctrl+F on a **bases** tab does nothing while `BasesView`'s own search box sits in its toolbar (`BasesView.cpp:83`). Nothing on canvas or graph either. |
| `edit_replace`, `edit_find_next`, `edit_find_prev` | ☠ | same |

### 3.3 Go

| Action | Class | Notes |
|---|---|---|
| `quick_switcher`, `command_palette`, `search_vault`, `graph_view` | W | |
| `open_daily_note` | W | |
| `insert_template` | ☠ | `insertTemplate` (`:2921-2948`) opens the picker, expands the template, then returns silently if there is no `activeEditor()`. On a canvas tab you pick a template and nothing happens. |
| `tab_next`, `tab_prev`, `tab_close` | W | |

### 3.4 View

| Action | Class | Notes |
|---|---|---|
| `editor_toggle_mode` (Ctrl+E) | M | Not vault-gated, not type-gated. Cycles Source→Live→Reading; no-op with no markdown leaf. |
| `view_source_mode` | M | **Never enabled or disabled by anything.** `updateVaultActions` gates `view_editing_mode` and `view_reading_mode` but omits `view_source_mode` (`:2981-2982`) — an outright omission. |
| `view_editing_mode`, `view_reading_mode` | M | vault-gated only, not type-gated |
| `fold_all`, `unfold_all`, `toggle_fold` | ∅ | 3/3 permanently disabled |
| `split_right`, `split_down` | W | |
| `view_toggle_left_sidebar` | U | |
| `view_zoom_in/out/reset` | ☠ | Bypasses the `View::zoom*` virtuals; markdown-only in practice. See §4.1. |

### 3.5 Format / Heading / Insert / Table — 27 entries, all markdown-only

| Action | Class |
|---|---|
| `format_bold`, `format_italic`, `format_strikethrough`, `format_inline_code` | M (live) |
| `insert_link` | M (live) |
| `insert_wiki_link`, `insert_image`, `insert_code_block`, `insert_block_quote`, `insert_horizontal_rule` | ∅ |
| `heading_1` … `heading_6` | M (live) |
| `heading_increase`, `heading_decrease` | ∅ |
| `insert_table`, `insert_callout` | M — **placebo**: dialog opens, result discarded (`:511-535`; punch-list P3) |
| `toggle_checkbox` | ∅ |
| `table_row_above/below`, `table_col_left/right`, `table_delete_row/col` | ∅ — **the whole Table menu, 6/6** |

### 3.6 Tab / navigation (Cluster L Phase L4)

`tab_undo_close`, `go_back`, `go_forward`, `tab_jump_1`…`tab_jump_8`,
`tab_jump_last`, `tab_pin_toggle`, `tab_move_to_new_window`, `tab_toggle_stacked` —
all **W**. These are the healthiest group in the collection: `go_back`/`go_forward`
have real enablement tracked per active leaf (`updateBackForwardActions:2424`), and
`tab_pin_toggle`/`tab_toggle_stacked` re-bind check-state on leaf change
(`updateTabStateActions:2433`). **This is the pattern Cluster O should generalise.**

### 3.7 Totals

| Bucket | Count |
|---|---|
| Universal / workspace | ~30 |
| Markdown-only | 39 |
| Canvas-only | **0** |
| Bases-only | **0** |
| Graph-only | **0** |
| Permanently disabled stubs | 17 |
| Placebo (runs, discards) | 2 |
| Silently no-op off-markdown (☠) | 7 (`file_save`, `edit_find`×4, `insert_template`, zoom×3 — 9 actions across 7 behaviours) |
| **Total in `MainWindow`'s collection** | **~79** |

(`PARITY-MATRIX.md:102` says "~70 KActions (many disabled stubs)" — close enough;
the precise figure is 79 plus the MDI client's 5 + 3×N-tool-views in a *separate*
collection that `showCommandPalette` never walks.)

---

## 4. Bugs and inconsistencies found during the audit

These are real, reproducible, and mostly *are* Cluster O's subject matter rather than
side findings. Ordered by severity.

### 4.1 `View::zoom*` polymorphic dispatch is built, overridden, and bypassed

`View::zoomIn/zoomOut/zoomReset` are declared virtual (`View.h:60-62`) with no-op
bodies (`View.cpp:123-125`). `MarkdownView` overrides all three
(`MarkdownView.h:41-43`) — with stale `TODO(port-foundation-exploration)` empty
bodies (`MarkdownView.cpp:87-103`). `MainWindow::onZoomIn/Out/Reset` ignore the
virtuals entirely and call `leaf->setFontScale()` on the Markoff leaf
(`:707-729`). Net: the abstraction exists, nobody uses it, and canvas/graph — both
of which own a real zoom transform — have no zoom action at all.

### 4.2 In-place view-type change never refreshes the action state

`Workspace::setActiveLeaf` early-returns when the leaf is unchanged
(`Workspace.cpp:284-286`), so `activeLeafChanged` does **not** fire. But
`navigateActiveLeafTo` (`:2392-2422`) resolves the target's view type by extension
and calls `leaf->navigate(viewState)` — which swaps the leaf's `View` in place and
emits only `WorkspaceLeaf::viewChanged`. `MainWindow` connects `viewChanged` to
`updateBackForwardActions` **and nothing else** (`:2039-2044`).

**Reproduction:** open a markdown note, click a `[[Some Canvas.canvas]]` or
`[[Some Base.base]]` wikilink. The active leaf becomes a canvas/bases view; the
Format/Heading menus stay enabled, the editor-mode radio stays checked, and the
heading radio keeps whatever it was. Back/forward across view types (explicitly
supported — see the comment at `WorkspaceLeaf.h:123-127`) has the same effect.

### 4.3 `GraphControlsPanel` is a shared singleton with no disconnect

`GraphViewPlugin::createView` lazily builds **one** `GraphControlsPanel`
(`GraphViewPlugin.cpp:97-103`) and hands the same pointer to every `GraphView`
created by the factory (`:44-47`). `GraphViewTab::wireControlsPanel()`
(`GraphViewTab.cpp:144-195`) makes ~13 `connect()` calls and never disconnects a
previous tab. **Two graph tabs open ⇒ moving one slider drives both engines.** This
is the concrete cost of treating view-local chrome as a global dock panel.

### 4.4 `refreshEditorActions` and `updateEditorActionStates` overlap and disagree

Both write the enabled state of `format_*` and `heading_*`.
`refreshEditorActions` (`:537-589`) sets them from `isMarkdown`;
`updateEditorActionStates` (`:611-627`) sets them from `hasEditing()`. Today the
ordering at `activeLeafChanged` (`:2081` then `:2091`) makes the stricter one win,
but `refreshEditorActions` is also called standalone at `:1814` and any future
call site would silently re-enable Bold in Reading mode. Meanwhile `insert_table` /
`insert_callout` are gated on `isMarkdown` only — **so Insert ▸ Table is enabled in
read-only Reading mode.**

### 4.5 `view_source_mode` is never enable-gated

`updateVaultActions` (`:2965-2990`) lists `view_editing_mode` and
`view_reading_mode` but not `view_source_mode`. It is enabled on the welcome screen
with no vault open.

### 4.6 Bases' toolbar is invisible to every global surface

`BasesView` builds 7 `QToolButton`s (Properties, Sort+group, Views, Filters,
Drawer, +New, Results) as raw widgets (`BasesView.cpp:74-128`). None is a `QAction`.
They cannot be given a shortcut, cannot appear in the command palette, cannot be
found in the Hotkeys settings page, and cannot be surfaced in a menu. Everything
Cluster D shipped is unreachable from the keyboard.

### 4.7 Command palette groups actions by objectName prefix

`showCommandPalette` (`:2288-2332`) buckets actions into File/View/Other by
`objectName().startsWith("file_"/"view_")`. Everything else — 60-odd actions
including all 17 disabled stubs — lands in "Other". Disabled actions are still
listed. `CommandRegistry::listAvailable()` is consulted for registry commands but
`KActionCollection` availability is not.

---

## 5. Is document type the right discriminator?

**Necessary, not sufficient.** Type answers "should this action be *present*"; it
cannot answer "should this action be *usable right now*". Counter-examples found in
code and in the audit:

| Case | Same viewType? | Availability differs? | Source |
|---|---|---|---|
| Reading vs Live vs Source | ✅ `"markdown"` | ✅ `hasEditing()` false in Reading | `MainWindow.cpp:615` |
| Canvas read-only lock | ✅ `"canvas"` | ✅ gates all editing/creation/delete/drag | `canvas.md` §9.8 (not yet built) |
| Canvas "Convert to file" | ✅ | ✅ only when exactly one text node is selected | `canvas.md` §6 |
| Markdown table ops | ✅ | ✅ only when the caret is in a table | `MainWindow.cpp:1640` comment |
| Bases active view config | ✅ `"bases"` | ✅ table vs future cards view | `bases.md` |
| Undo / Redo | any | ✅ stack depth | not currently checked |
| No vault open | n/a | ✅ | `updateVaultActions` |

### 5.1 Proposed model — three tiers

**Tier A — Presence.** Which actions exist in the menubar/toolbar/shortcut map at
all. Keyed on **view type**. Changes only when the active leaf's type changes
(including in-place, §4.2). Coarse and cheap: a menu either belongs to this document
kind or it does not.

**Tier B — Enablement.** Within a present set, which actions are enabled. Keyed on
**capability predicates the view answers about itself** — `canEdit()`, `canSave()`,
`canZoom()`, `canFind()`, `hasSelection()`, `canUndo()`. Refreshed on a new
`View::contextChanged()` signal (markdown already emits the equivalent via
`NoteEditorWidget::editorContextChanged`). This is where Reading mode, canvas lock,
selection gating, and undo-stack depth all land — **one mechanism, not four
bespoke ladders.**

**Tier C — Check state.** Checkable actions mirror live view state: editor-mode
radio, heading radio, snap toggles, pin, stacked. Already done correctly for pin/
stacked/back-forward (`:2424-2446`) — generalise that pattern.

Vault-open is not a fourth tier; it is a Tier-B capability of the *window*, and
`updateVaultActions` should be folded into the same refresh pass rather than being a
parallel hand-maintained list.

### 5.2 Hide vs disable — recommended rule

> **Hide** what belongs to a *different* document kind.
> **Disable** what belongs to *this* document kind but is unavailable right now.

Rationale: KDE HIG treats greyed-out as "possible, but not in this state"; absent
means "not applicable here". A permanently greyed **Table** menu on a canvas tab
teaches the user nothing and costs a menubar slot. A greyed **Bold** in Reading mode
is correct and informative — the mode is one Ctrl+E away and the user should see
what they would get.

**Applied to the Cluster O plan's stated cases:**
- Format/Heading/Insert/Table on a canvas/bases/graph tab → **hide** (Tier A).
- Editor-mode radio on a non-markdown tab → **hide** (Tier A).
- Format verbs in Reading mode → **disable** (Tier B). The plan's phrasing was
  "hide/disable"; the recommendation is *disable*.
- The 17 stubs → neither. They should leave the menus entirely until implemented
  (see Q9).

---

## 6. Recommended mechanism

### 6.1 `ViewActions` — a per-view-type `KXMLGUIClient` provider

```
class ViewActions : public QObject, public KXMLGUIClient {
    virtual QString viewType() const = 0;        // "markdown" | "canvas" | "bases" | "graph"
    virtual void bind(View *view) = 0;           // Tier C: reflect state, connect signals
    virtual void unbind() = 0;
    virtual void refresh() = 0;                  // Tier B: capability → setEnabled
};
```

Each provider owns its own `KActionCollection` and its own `.rc` fragment declaring
the menus and the secondary toolbar it contributes:

```xml
<gui name="corbomite_canvas" version="1">
  <MenuBar>
    <Menu name="canvas" append="format_merge"><text>&amp;Canvas</text>
      <Action name="canvas_snap_grid"/> …
    </Menu>
  </MenuBar>
  <ToolBar name="canvasToolBar"><text>Canvas</text> … </ToolBar>
</gui>
```

A new `ActionContextController` (extracted from `MainWindow` — see §6.4) subscribes
to **both** `Workspace::activeLeafChanged` and the active leaf's
`WorkspaceLeaf::viewChanged`, resolves the target view type, and on a change:

```
guiFactory()->removeClient(m_current);   // menus + toolbar + shortcuts vanish
guiFactory()->addClient(next);           // and the new ones appear
next->bind(view);
```

Guarded by "only if the type actually changed", so ordinary same-type tab switches
cost one `refresh()` call, not an XMLGUI rebuild.

### 6.2 Why this and not the alternatives

| Option | Verdict |
|---|---|
| **A. `KXMLGUIClient` swap on the factory** (recommended) | KDE-native (`KParts::MainWindow::createGUI`, Kate's per-view clients). Already precedented in-tree by `CorbomiteMDI::GUIClient`, a second XMLGUI client merging into the View menu (`CorbomiteMDI.cpp:83-97`). API verified present: `KXMLGUIFactory::addClient/removeClient` (`kxmlguifactory.h:106,113`). Handles menu creation/teardown, toolbar creation/teardown, merge ordering, and shortcut scoping for free. Automatically fixes palette and Hotkeys-page pollution, because inactive types' actions are simply not in the window's client chain. |
| **B. One static `.rc`, toggle `setVisible()`** | Cheapest. But QMenuBar does not hide a top-level menu whose actions are all hidden — you would hand-manage each menu's own `menuAction()->setVisible()`. Does nothing for palette/hotkeys pollution. Keep as fallback if A proves janky. |
| **C. `plugActionList`/`unplugActionList` into `<ActionList>` placeholders** | Already used by the MDI client (`CorbomiteMDI.cpp:241,248`). Good for flat lists; **cannot build submenus**, so it can't express a Canvas menu with nested groups. Viable for a hot-swapped *toolbar* only. |
| **D. Hardcoded if/else ladder in `MainWindow`** | What exists today for undo/redo. Explicitly rejected by the Cluster O stub plan; agreed. |

### 6.3 Consequences to accept up front

- **Shortcuts scope with the client.** Ctrl+B does nothing on a canvas tab because
  `format_bold` is not in the window's client chain. This is the point, but it is a
  behaviour change worth stating (Q10).
- **Providers must be constructed eagerly at startup, not per-leaf.** The Hotkeys
  settings page (`SettingsDialog` embeds `KShortcutsEditor` over a single
  `KActionCollection`, punch-list entry at `docs/punch-list.md:265`) must be able to
  show *all* types' shortcuts, including types with no tab open. So build all
  providers at window construction; only *installation* is dynamic. `SettingsDialog`
  will need to take a list of collections rather than one.
- **Settings ▸ Configure Toolbars (`KEditToolBar`) only edits currently-active
  clients.** The canvas toolbar is configurable only while a canvas tab is focused.
  Acceptable; document it.
- **`corbomiteui.rc` version bump** (currently `version="10"`,
  `corbomiteui.rc.in:2`) is required on every structural change, or users keep a
  stale cached rc in `~/.local/share/kxmlgui5/corbomite/`.

### 6.4 Relationship to the MainWindow decomposition spec

[`specs/2026-06-10-mainwindow-decomposition-design.md`](superpowers/specs/2026-06-10-mainwindow-decomposition-design.md)
declares "No KXmlGuiWindow/action-framework redesign" a **non-goal** and notes
`setupActions` is a 455-line function. Cluster O is precisely that redesign, so the
two must be sequenced: **do the decomposition's action-controller extraction as
Cluster O phase O1**, not separately. `actionCollection()` ownership stays in
`MainWindow` (KF6 requires the `KXmlGuiWindow` to own its collection) — the providers
own *their own* collections, which is legal and is exactly what the MDI GUIClient
already does.

---

## 7. Should sidebars be contextual?

**Recommendation: no — not by document type.** Three reasons, then the exception.

1. **Obsidian doctrine.** Sidebar leaves are stable furniture. Backlinks, Outline,
   and Properties do not vanish for a canvas tab; they render an empty/not-applicable
   state. Users build spatial muscle memory on sidebar contents. (Note: canvas links
   *are* indexed into the global graph — `canvas.md` §9.9 — so Backlinks is genuinely
   meaningful on a canvas tab today.)

2. **Corbomite's tool views are user-arranged and drag-movable between left and
   right** (`CorbomiteMDI::Sidebar::dropEvent`, `moveToolView`). Auto-hiding fights
   the user's own arrangement. Worse, **sidebar layout is not persisted at all** —
   `Sidebar::saveSession/restoreSession` exist and are never called (punch-list P2,
   `docs/punch-list.md:215`). Adding auto-hide on top of amnesia would be actively
   hostile.

3. **The one apparent counter-example is a misplacement, not a model.** The graph
   controls panel feels like "a contextual sidebar" because it *is* view-local chrome
   that got docked globally (`X-Corbomite-DockArea: right`,
   `graph-view/metadata.json.in`). In Obsidian this panel lives inside the graph
   pane. Corbomite's version is a shared singleton with the cross-talk bug in §4.3.
   Gating its *visibility* on graph focus would leave the bug intact.

   *(Caveat: the audit corpus has no `domains/graph.md` — the graph plugin was never
   Pass-2'd; see `addenda/2026-04-19-graph-screenshot.md` §preamble. The claim about
   where Obsidian renders the panel is from product behaviour, not from the
   decompiled-source corpus, and should be verified before it drives a decision.)*

### 7.1 What to build instead — the pane-local inspector

The thing worth duplicating for other document types is a **collapsible inspector
inside the view's own pane**, toggled by a header action:

- Reuse the idle `ItemView::addAction(icon, title, callback)` seam
  (`ItemView.h:27`) for the cog button — this is Obsidian's `view.addAction`
  contract, already built, used by one class.
- The inspector is per-`View`, so the graph cross-talk bug (§4.3) disappears by
  construction: each `GraphViewTab` owns its own controls.
- It generalises exactly as the user speculated: a markdown per-note settings
  inspector (readable line width, mode, font scale), a bases config inspector with
  more room than the toolbar popups, a canvas inspector (snap, grid, lock,
  background).
- The existing `CollapsibleSection` widget (`src/plugins/graph-view/
  CollapsibleSection.{h,cpp}`) is already the right primitive and is currently
  graph-private — promote it to `libs/core`.

### 7.2 What sidebars *should* gain from this cluster

- **Reactive, not conditional**: panels stay put and render an honest empty state for
  types they can't serve. (Outline on a canvas should say so, not go blank.)
- **A "raise on first activation of a type" affordance**, optionally: when a graph
  tab is first focused in a session, *raise* an already-present tool view tab. A
  raise is reversible and non-destructive, unlike create/destroy. Offered as Q6.
- **Fix the persistence gap first** (punch-list P2) or any contextual behaviour will
  be indistinguishable from the existing amnesia.

---

## 8. Actions that will exist but don't yet

Sizing input for the classification, drawn from the audit corpus. This is what the
mechanism has to still fit in two years' time.

**Canvas** (`canvas.md` §6, §9.5, §9.6, §9.8): `canvas:new-file` (exists as
`file_new_canvas`), `canvas:export-as-image` (exists, hamburger-only),
`canvas:jump-to-group`, `canvas:convert-to-file` (selection-gated), snap-to-grid,
snap-to-objects, show-grid, zoom in/out/reset, zoom-to-fit (Shift+1),
zoom-to-selection (Shift+2), read-only lock, node colour, edge direction (exists in
context menu), group/ungroup, arrange/z-order. **~16, none currently a KAction.**

**Markdown** (`workspace.md` §6, `editor-markdown.md` §6): the 17 stubs, plus
`editor:follow-link` (Alt+Enter), `editor:open-link-in-new-leaf/-split/-window`,
`editor:rename-heading`, `editor:download-attachments`, `workspace:export-pdf`,
`workspace:edit-file-title` (F2), add-property, toggle-source-mode. **~26 more.**

**Bases**: the 7 in-view buttons plus add/duplicate/delete/set-default view, add
property, add formula, edit filters, group-by, sort, +New, copy table, export CSV,
toggle drawer. **~18, none currently a KAction.**

**Graph** (`addenda/2026-04-19-graph-screenshot.md` §1): `graph:open`,
`graph:open-local`, `graph:animate`, `graph:copy-screenshot` (exists in
`CommandRegistry` only), plus the panel's filter/display/force controls if promoted.
**~5–15.**

**Workspace/universal** (`workspace.md` §6): `workspace:copy-path/-full-path/-url`,
`workspace:close-others/-tab-group/-others-tab-group`, `workspace:new-tab/new-window`,
`workspace:close-window`, `workspace:show-trash`, `editor:focus-{top,bottom,left,right}`.
**~13.**

Projected end state: **~160 actions, of which under half are universal.** A single
flat static `.rc` is not a viable destination for that.

---

## 9. Proposed phasing (for the plan, not committed)

| Phase | Content | Gate |
|---|---|---|
| **O0** | Turn this report into a design spec: three-tier model, hide-vs-disable rule, `ViewActions` interface, action-ownership doctrine (which of the five surfaces owns what). | **User sign-off on §11 questions.** |
| **O1** | *Correctness, no new mechanism.* Extract `ActionContextController` from `MainWindow` (= decomposition spec's action step). Make it the single subscriber to `activeLeafChanged` **and** `viewChanged` (fixes §4.2). Re-light `View::zoom*` polymorphic dispatch (§4.1). Make `file_save`, `edit_find`/`replace`/`next`/`prev`, `insert_template` either work or disable themselves (§4.6 ☠ class). Merge `refreshEditorActions` into `updateEditorActionStates` (§4.4). Gate `view_source_mode` (§4.5). | Offscreen green + **no action silently no-ops** — testable by introspection. |
| **O2** | Tier-B capability contract: `View::capabilities()` / discrete virtuals + `View::contextChanged()`. Route Reading-mode gating, vault-open gating, and undo-stack gating through it. | Unit tests over the predicate table. |
| **O3** | `ViewActions` provider mechanism. Ship with **markdown as the only provider** — Format/Heading/Insert/Table/fold/editor-mode move out of `corbomiteui.rc` into `MarkdownViewActions`. Zero visible change except those menus vanish on non-markdown tabs. | Live eyeball (menubar reflow, no flicker on tab switch). |
| **O4** | **`CanvasViewActions`** — Canvas menu + `canvasToolBar`: snap-to-grid, snap-to-objects, show-grid, zoom in/out/reset/fit/selection, lock. Closes punch-list `[ui-bundle][canvas][P2][cluster-o]`. | Live eyeball — this is the phase the user actually feels. |
| **O5** | `BasesViewActions` + `GraphViewActions`. Bases' 7 `QToolButton`s become `setDefaultAction()` on provider actions — one definition, four surfaces (in-view toolbar, menubar, palette, hotkeys). | Live eyeball. |
| **O6** | Pane-local inspector: promote `CollapsibleSection` to `libs/core`, revive `ItemView::addAction` for the cog, move graph controls into the graph pane (fixes §4.3), add a canvas inspector. Optional / deferrable. | Live eyeball. |
| **O7** | Cleanup + soak: dispose of the 17 stubs and 2 placebos per Q9; palette grouping by owning surface instead of objectName prefix (§4.7); ribbon decision per Q4. | Soak. |

**Sequencing constraints:** O1 before everything (it is the bug-fix phase and the
extraction the decomposition spec wants). O3 before O4/O5. O6 independent of O3–O5.
Cluster M's remaining phases are unblocked by O4 specifically, not by the whole
cluster.

---

## 10. What I decided without asking

Recorded so they can be overturned cheaply:

1. **Document type is Tier A only.** A capability layer is mandatory; a pure
   type-switch cannot express Reading mode, canvas lock, or selection gating.
2. **Hide other types' menus; disable this type's unavailable actions.** §5.2.
3. **Reading mode disables editing verbs, it does not hide them** — contradicting
   the Cluster O stub plan's "hide/disable" phrasing, which left it open.
4. **`KXMLGUIClient` swap over `setVisible()` toggling or `plugActionList`.** §6.2.
5. **Sidebar tool views do not appear/disappear by document type.** §7.
6. **The graph controls panel is a misplacement to fix, not a pattern to copy** —
   with the caveat in §7.3 that the audit corpus doesn't cover the graph plugin.
7. **Providers are constructed eagerly, installed dynamically** — forced by the
   Hotkeys settings page. §6.3.
8. **`ItemView::addAction` is revived rather than a new header-action API invented.**
9. **The `☠` silent-no-op class is a Cluster O bug-fix phase (O1), not punch-list
   spillover** — those actions are precisely "static chrome that doesn't know what
   kind of tab is focused".

---

## 11. Ambiguities and questions for the user

**Q1 — Hide-vs-disable rule.** Confirm §5.2: hide other types' menus, disable this
type's currently-unavailable actions, and specifically *disable* (not hide) editing
verbs in Reading mode.

**Q2 — Menubar stability.** Is a menubar whose *top-level menus* appear and disappear
on tab switch acceptable? The alternative is a fixed set of top-level menus whose
contents change (e.g. one "Document" menu that re-titles itself to Canvas/Format/
Table). Menu-count churn is the most visible consequence of the recommendation and
the one most likely to feel wrong in practice.

**Q3 — Secondary toolbar shape.** One hot-swapped document-type toolbar to the right
of the main toolbar, or contributions merged into the main toolbar? And should it be
independently hideable via the usual KDE toolbar context menu?

**Q4 — The ribbon.** `RibbonToolBar` is a fifth action surface with **zero core
registrations** — an empty strip in every session. Keep it as the plugin ribbon
(Obsidian parity), retire it, or **repurpose it as the document-type toolbar** (it is
already the right shape: programmatic, non-XMLGUI, per-vault visibility state)?

**Q5 — Canvas settings scope.** Snap-to-grid/objects: app-wide (`corbomite.kcfg`,
new `Canvas` group), per-vault (`.obsidian/`), or per-file? `canvas.md` §9.5 says
Obsidian persists both flags via `saveOptions` (plugin-scoped) with both defaulting
ON, which maps most closely to app-wide. Corbomite has no precedent for
view-type-scoped settings.

**Q6 — Sidebar behaviour.** Confirm "no auto show/hide by document type". Do you want
the optional non-destructive "*raise* the relevant tool view on first activation of a
type" behaviour, or nothing at all?

**Q7 — Graph controls.** Relocate into the graph pane as a view-local inspector
(Obsidian-faithful, fixes the two-tab cross-talk bug §4.3), or leave it as a
right-dock tool view? If the latter, the cross-talk bug still needs its own fix and
should be punch-listed now.

**Q8 — Bases toolbar.** Promote the 7 `QToolButton`s to KActions (so they gain
shortcuts, palette entries, and a Hotkeys page presence) while keeping the in-view
toolbar via `setDefaultAction`? Or leave Bases' chrome widget-only?

**Q9 — The 17 stubs and 2 placebos.** Remove them from the menus entirely
(recommended — a permanently greyed Table menu is noise), keep them as
discoverability breadcrumbs, or implement a subset now? Note several are *cheap*
against the current Markoff contract (`insert_code_block`, `insert_block_quote`,
`insert_horizontal_rule`, `heading_increase/decrease` are all expressible via
existing verbs); the table ops genuinely need upstream Markoff work.

**Q10 — Shortcut scoping.** Accept that Ctrl+B does nothing on a canvas tab (because
`format_bold` is not in the active client chain) rather than being globally reserved?

**Q11 — Cluster boundary.** Cluster I ("Editor & Workspace UI Surfacing") has a live
remnant — Phase 5 tail (move-to-new-window, link-with-active-pane UI) and Phase 6
(search regex/match-case toggles, `Notice::post`) — in exactly this territory. Fold
into Cluster O, or keep separate? Recommendation: fold, and close Cluster I.

**Q12 — v1 scope.** Which providers ship in the first pass? Recommendation: markdown
+ canvas (O3–O4), with bases and graph deferred to O5 as a follow-on, so the phase
the user actually feels lands early.

---

## 12. Appendix — signals available to hook

| Signal | Fires when | Currently consumed by |
|---|---|---|
| `Workspace::activeLeafChanged(WorkspaceLeaf*)` | active leaf changes (early-returns if unchanged, `Workspace.cpp:284`) | `MainWindow` mega-lambda `:2029-2112`; `BasesView` refresh `:1247` |
| `Workspace::layoutReady()` | workspace.json load settles | gating |
| `WorkspaceLeaf::viewChanged(View*)` | leaf's view swapped in place (navigate / back / forward across types) | **only** `updateBackForwardActions` (§4.2) |
| `WorkspaceLeaf::pinnedChanged(bool)` | pin toggled | `updateTabStateActions` |
| `NoteEditorWidget::editorContextChanged(EditorContext)` | markdown caret/block context | `onEditorContextChanged` → heading radio + `updateEditorActionStates` |
| `NoteEditorWidget::viewModeChanged(ViewMode)` | Source/Live/Reading switch | editor-mode radio sync `:2109` |
| `ViewRegistry::viewRegistered/viewUnregistered(type)` | plugin registers/unregisters a view type | nothing — **the natural hook for provider install/uninstall** |
| `Vault`/`CorbomiteApp::vaultOpened/vaultClosed` | vault lifecycle | `updateVaultActions` |
| *(missing)* `View::contextChanged()` | selection / mode / lock changes inside any view | — proposed in O2 |

`ViewRegistry::viewRegistered` is worth calling out: it means a plugin-registered
view type (like `graph`) can ship its own `ViewActions` provider through the same
`ViewRegistrar` proxy it already uses for its factory — no new plugin-API surface.
