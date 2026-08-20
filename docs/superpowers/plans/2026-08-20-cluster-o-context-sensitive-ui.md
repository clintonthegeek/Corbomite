# Cluster O — Context-sensitive menu bar, toolbar & sidebar

**Opened:** 2026-08-20 (stub, user-directed, during Cluster M Phase M4 live-testing).
**Planned:** 2026-08-20 from [`../../audit-2026-08-20-context-sensitive-ui.md`](../../audit-2026-08-20-context-sensitive-ui.md).
**Type:** Full plan. **Track:** strategic cluster.
**Read the audit report first** — it carries the ~79-action inventory, the evidence
behind every doctrine call below, and the seven bugs O1 fixes. This plan does not
repeat it.

---

## Decisions of record (user, 2026-08-20)

Answers to the report's §11. These are **normative** — an executor may not
re-litigate them, only report if one proves impossible.

| # | Question | Decision |
|---|---|---|
| Q1 | Hide vs disable | **Hide** other document types' menus; **disable** this type's currently-unavailable actions. Reading mode *disables* editing verbs, does not hide them. |
| Q2 | Top-level menus appearing/disappearing | **Accepted**, conditional on doing it cleanly in Qt/KXMLGUI. Universal menus must never shift position — see §D5. |
| Q3 | User override | **Required.** Contextual hiding is overridable by the user from the toolbar context menu. A force-shown out-of-context toolbar keeps its buttons **disabled** (except genuinely universal ones). |
| Q4 | Ribbon | **Keep purely for plugins.** Not repurposed as the document-type toolbar. |
| Q5 | Canvas settings scope | **App-wide** (`corbomite.kcfg`). |
| Q6 | Sidebar | No contextual hide/show. **Raising** a relevant tool view is acceptable where it makes sense; user accepts losing contextual hide/show. |
| Q7 | Graph controls | **Relocate** into the graph pane. |
| Q8 | Bases toolbar | **Promote** the 7 `QToolButton`s to KActions. |
| Q9 | 17 stubs + 2 placebos | **Implement the cheap ones; keep soon-to-be-built ones as stubs.** See §D7 — verification narrowed "cheap" to 3 actions. |
| Q10 | Shortcut scoping | **Scope shortcuts to context** (or overload on context — whichever is saner per action). |
| Q11 | Cluster I | **Fold relevant remnants in and close Cluster I.** |
| Q12 | Sequencing | **Planner's call.** |

---

## Goal

Menu bar, toolbar, and per-pane chrome become functions of the focused tab's
document type and current capability, instead of one static markdown-shaped surface.
A canvas tab gets a Canvas menu and a canvas toolbar with working snap/grid/zoom
actions; a bases tab gets its seven existing toolbar functions as real, bindable,
palette-visible actions; markdown's Format/Heading/Insert/Table menus stop occupying
the menubar when no markdown tab is focused; and nothing in the chrome is ever a
silent no-op.

**Success condition, stated as a test:** for every enabled action in the collection,
triggering it on any focused document type either performs a visible effect or the
action is disabled. Zero silent no-ops. (`tst_action_context_no_silent_noop`.)

## Explicit non-goals

- **Not** a rewrite of `CorbomiteMDI` / KateMDI sidebars.
- **Not** implementing the block-structural editor verbs (tables, fold, callouts,
  checkbox toggle) — those need upstream Markoff work; O7 writes the steer.
- **Not** Cluster I Phase 6 (search regex/match-case toggles, `Notice::post` at 5
  failure sites) — that is search-panel UI, not action context. O0 moves it to the
  punch list.
- **Not** touching `.obsidian/` schema. Nothing in this cluster changes vault format.
  (Canvas settings go to `corbomite.kcfg` per Q5.)
- **Not** the plugin ribbon (Q4) beyond documenting that it stays plugin-only.

---

## Doctrine — the contract this cluster establishes

Normative. Every phase is measured against it.

### D1 — Three tiers

| Tier | Question | Keyed on | Changes when | Mechanism |
|---|---|---|---|---|
| **A — Presence** | Does this action exist in the chrome at all? | **view type** (`View::getViewType()`) | active leaf's *type* changes | `KXMLGUIClient` swap on the factory |
| **B — Enablement** | Is it usable right now? | **capability predicates the view answers about itself** | any context change | `ActionContextController::refresh()` |
| **C — Check state** | Does it reflect live state? | view state | any context change | same refresh pass |

Vault-open is **not** a fourth tier — it is a Tier-B capability of the window.
`updateVaultActions`' hand-maintained list is retired into the same refresh pass.

### D2 — Hide vs disable

> **Hide** what belongs to a *different* document kind.
> **Disable** what belongs to *this* kind but is unavailable right now.

A permanently greyed **Table** menu on a canvas tab teaches nothing and costs a
menubar slot. A greyed **Bold** in Reading mode is correct and informative.

### D3 — Surface ownership

Five surfaces exist (report §2). After this cluster each owns exactly one job:

| Surface | Owns | Never |
|---|---|---|
| `KActionCollection` + `.rc` (universal, `MainWindow`) | actions meaningful for **any or no** focused tab | type-specific verbs |
| `ViewActions` provider (per view type) | that type's verbs, its menu, its toolbar | universal verbs |
| Per-pane hamburger (`onMoreOptionsMenu`) | per-**instance** operations (rename, export, copy path, split) | anything that wants a shortcut |
| `CommandRegistry` | plugin-supplied commands | host verbs |
| `RibbonToolBar` | plugin ribbon icons only (**Q4**) | anything host-owned |

**Rule:** if an action wants a keyboard shortcut, a palette entry, or a Hotkeys-page
row, it must be a `QAction` in either the universal collection or a provider
collection. The hamburger is for things that legitimately have none of those.

### D4 — Toolbar visibility is tri-state and user-overridable (**Q3**)

Each provider gets one persistent `KToolBar`, created eagerly at startup, owned by
`ActionContextController`, added via `addToolBar()` (the `RibbonToolBar` pattern, now
`MainWindow::setupRibbonToolBar()` at `MainWindow.cpp:2064-2072` — **corrected
2026-08-20 post-O1**; the plan's original `:2167-2180` citation was written before O1
moved ~289 lines out of `MainWindow.cpp` into `ActionContextController`. **Must be
created after `setupGUI`**).

```
enum class ToolBarPolicy { Auto, AlwaysShow, AlwaysHide };   // persisted in kcfg
```

- `Auto` (default): visible iff the active leaf's type matches.
- `AlwaysShow`: pinned visible; **out of context its actions are disabled** (Tier B
  returns not-applicable), except actions that are genuinely universal.
- `AlwaysHide`: pinned hidden.

The user flips the policy by toggling the toolbar from the toolbar context menu.
Toggling out of `Auto` is sticky. A "Reset toolbar visibility" entry restores `Auto`
for all.

**Trap:** `setupGUI(Default, …)` includes `Save`, so `KMainWindow` persists and
re-applies toolbar visibility itself in `applyMainWindowSettings`. The controller
must re-apply policy **after** that, or KMainWindow's restored visibility silently
wins. Named in O3.T4.

### D5 — Menubar stability (**Q2**)

Universal menus must never move. The base `corbomiteui.rc` gains a merge marker:

```xml
<MenuBar>
  <Menu name="file"/> <Menu name="edit"/> <Menu name="go"/> <Menu name="view"/>
  <Merge name="viewtype_merge"/>
  <!-- Settings + Help appended by KXMLGUI -->
</MenuBar>
```

Every provider declares its menu with `append="viewtype_merge"`. Result: File / Edit /
Go / View never shift, Settings / Help stay rightmost, and the type menu always
appears in exactly one slot between them. Verified in O3.V2.

### D6 — Shortcut scoping (**Q10**)

A provider's shortcuts are live only while its client is installed. `Ctrl+B` does
nothing on a canvas tab. This is intended. Two consequences:

- The **universal** collection must not bind any key a provider wants.
- Overloading the same key across providers is *allowed* and is the preferred
  answer where the semantic matches (e.g. `Ctrl+G` = "jump to group" on canvas would
  collide with the universal `graph_view` — so it does **not** get overloaded; but
  per-type `Ctrl+1..6` heading vs. nothing on canvas is fine because only one client
  is installed at a time).

### D7 — Which stubs are actually cheap (**Q9**, verified)

`MarkoffDocument::applyFlatEdit` creates **every** new block as
`BlockKind::Paragraph` and performs **no kind re-parse**
(`libs/markoff-family/libs/markoff-core/src/MarkoffDocument.cpp:1589, 1639, 1659`).
Markoff's contract-v2 verb surface is exactly six verbs
(`markoff/core/MarkdownView.h:76-81`). Therefore:

**Cheap — implement in O7 (Corbomite-side only, no re-pin):**

| Action | How |
|---|---|
| `heading_increase` / `heading_decrease` | read `EditorContext.headingLevel` (already consumed at `MainWindow.cpp:602-607`), call `setHeadingLevel(level ± 1)`, clamp 0..6 |
| `insert_wiki_link` | `insertAtCursor("[[]]", caretMarker)` — inline, stays inside the paragraph; the shipped `CompletionController` then fires on `[[` |
| `insert_image` | vault-file picker → `insertAtCursor("![[path]]")` — inline embed, same reasoning |

**Not cheap — stay stubs, O7 writes a Markoff steer:** `toggle_checkbox` (the task
box lives in the ListItem *marker*, which `listItemDisplayMarker`'s doc explicitly
says is "NOT part of the edited flat-view byte space"), `insert_block_quote`,
`insert_code_block`, `insert_horizontal_rule`, `insert_table`, `insert_callout`, all
six `table_*`, all three fold actions. Every one needs a block-kind change that
`applyFlatEdit` cannot express.

> **Bonus finding to punch-list (O0.T3):** the same limitation means **template
> insertion** (`insertTemplate` → `insertAtCursor`, shipped 2026-06-11) inserts a
> template's headings/lists/code fences as flat Paragraphs, and `splitOnNewlineRuns`
> collapses blank-line runs. Not corruption (`serializeForSave` re-emits the literal
> text and it re-parses on next load) but a real display-until-reload defect.
> **Verify before asserting** — write the test first.

---

## Audit references

- **Primary:** [`docs/audit-2026-08-20-context-sensitive-ui.md`](../../audit-2026-08-20-context-sensitive-ui.md) — inventory, bug list, mechanism trade table, signal table (§12).
- `obsidian-audit/domains/core.md` §7 — `Command` + `checkCallback` availability doctrine (Tier B's ancestor).
- `obsidian-audit/domains/workspace.md` §6 — the ~30 workspace commands that must remain universal.
- `obsidian-audit/domains/canvas.md` §6, §9.5, §9.6, §9.8 — canvas action set; snap defaults ON; log2 zoom clamp; read-only lock.
- `obsidian-audit/domains/editor-markdown.md` §6 — Obsidian's per-view action bar + pane-menu doctrine (why the hamburger exists and what it should *not* hold).
- `obsidian-audit/addenda/2026-04-19-graph-screenshot.md` §1 — graph command set; **no `domains/graph.md` exists**, so graph-panel placement claims are product-behaviour, not corpus (O6.V1 verifies).
- `specs/2026-06-10-mainwindow-decomposition-design.md` — declares action-framework redesign a non-goal; **O1 supersedes that non-goal** and performs its action-controller extraction.

---

## Phases

### Phase O0 — Bookkeeping and boundary-setting (docs only, no code)

- [x] **O0.T1 — Fold Cluster I's live remnant into this plan.** Cluster I's remaining
      scope is Phase 5 tail (**link-with-active-pane** checkable toggle;
      **move-tab-left/right**) + Phase 6. The Phase 5 tail items are actions →
      they land here as O5.T5. `WorkspaceLeaf::setGroup(QString)`
      (`WorkspaceLeaf.h:62`) is the linked-pane primitive; "Open linked view" already
      ships in the hamburger from Cluster R.
- [x] **O0.T2 — Move Cluster I Phase 6 to the punch list** as two items: search
      regex/match-case toggles (`[search][P3]`), `Notice::post` at the 5 identified
      failure sites (`[ui-bundle][P3]`). Cite the Cluster I plan for the site list.
- [x] **O0.T3 — Punch-list the template-block-kind finding** from §D7, marked
      `[editor][P3][verify-first]`.
- [x] **O0.T4 — Punch-list `GraphControlsPanel` cross-talk** (report §4.3) as
      `[graph][P2][cluster-o]`, noting O6 fixes it structurally.
- [x] **O0.T5 — Close Cluster I.** Retro at `docs/cluster-retros/cluster-i.md`
      recording that Phases 1–4 shipped as legacy Cluster V, Phase 5 tail folded into
      O, Phase 6 punch-listed. `git mv` the plan to `plans/archive/`; update
      `plans/INDEX.md`.
- [x] **O0.T6 — Record the Cluster M boundary** (see §Cross-cluster below) in both
      plans.

**Gate:** docs consistent; `INDEX.md` shows I closed and O planned. **DONE 2026-08-20
(`524c5cdc`)** — checkboxes above were left unticked when the commit landed; fixed
here, no scope change.

---

### Phase O1 — Correctness first: kill the silent no-ops (no new mechanism)

The highest-value phase and the one a fresh session should start with. It fixes
seven real bugs and performs the extraction every later phase needs. **No behaviour
becomes context-sensitive yet** — actions either work everywhere they claim to, or
disable themselves.

- [x] **O1.T1 — Extract `ActionContextController`** (`src/app/ActionContextController.{h,cpp}`),
      a plain `QObject` child of `MainWindow`. Moves in: `refreshEditorActions`,
      `updateEditorActionStates`, `updateVaultActions`, `updateBackForwardActions`,
      `updateTabStateActions`, and the action-related half of the `activeLeafChanged`
      mega-lambda (`MainWindow.cpp:2029-2112`). `actionCollection()` ownership stays
      in `MainWindow` (KF6 requires the `KXmlGuiWindow` to own its collection); the
      controller takes it by pointer. **Retires:** the five scattered refresh
      functions and the action half of the mega-lambda. This is the
      decomposition spec's action step — do it here, not separately.
- [x] **O1.T2 — Subscribe to `viewChanged` as well as `activeLeafChanged`.**
      Fixes report §4.2: `Workspace::setActiveLeaf` early-returns when the leaf is
      unchanged (`Workspace.cpp:284-286`), so an in-place view-type swap
      (`navigateActiveLeafTo` → `leaf->navigate()`, `MainWindow.cpp:2392-2422`) fires
      only `WorkspaceLeaf::viewChanged`, which today drives *only*
      `updateBackForwardActions`. Rebind per active leaf, same pattern as the
      existing `m_activeLeafHistoryConnection`.
      **Test:** `tst_action_context::inPlaceViewTypeChange_refreshesActionState` —
      markdown leaf, `navigate()` to a `.canvas` viewState, assert `format_bold`
      disabled and the editor-mode radio cleared.
- [x] **O1.T3 — Re-light `View::zoomIn/zoomOut/zoomReset` polymorphic dispatch.**
      Report §4.1: the virtuals exist (`View.h:60-62`, no-op bodies `View.cpp:123-125`),
      `MarkdownView` overrides them with stale empty `TODO`s
      (`MarkdownView.cpp:87-103`), and `MainWindow::onZoomIn/Out/Reset` bypass them
      entirely (`:707-729`). Fix: `MarkdownView`'s overrides do what
      `MainWindow::onZoom*` does today (`leaf->setFontScale`); `MainWindow`'s actions
      call `activeLeaf()->view()->zoomIn()`; `CanvasFileView` and `GraphView`
      override onto their real viewport zoom. **Retires:** the `activeEditor()`
      special-case in the three zoom slots.
      **Test:** `tst_view_zoom_dispatch` — one slot per view type asserting the
      virtual is reached.
- [x] **O1.T4 — Make `file_save` type-complete.** Report §3.1: `saveCurrentNote`
      (`:2268-2286`) handles `activeEditor()` then `TextFileView`; `CanvasFileView`
      is a `FileView`, so **Ctrl+S is a no-op on canvas** — canvas only saves in
      `onUnloadFile` (`CanvasFileView.cpp:71-73`). Add a `View`-level `save()`
      capability (or route through `CanvasFileView`), and gate the action on
      `canSave()`. **Test:** `tst_action_context::saveAction_savesCanvas`.
- [x] **O1.T5 — Make find/replace and `insert_template` honest.** `onFind`/`onReplace`/
      `onFindNext`/`onFindPrev` (`:676-698`) and `insertTemplate` (`:2921-2948`)
      return silently with no `activeEditor()`. Either disable the action (Tier B
      `canFind()` / `canInsertText()`) or route it — **Bases owns a search box
      already** (`BasesView.cpp:83`), so `edit_find` on a bases tab should focus it.
      Decision: route where a target exists, disable otherwise.
- [x] **O1.T6 — Merge the two overlapping refresh functions.** Report §4.4:
      `refreshEditorActions` sets `format_*`/`heading_*` from `isMarkdown` while
      `updateEditorActionStates` sets them from `hasEditing()`; ordering makes the
      stricter win *today* but any new call site silently re-enables Bold in Reading.
      Collapse into one. Also fixes **Insert ▸ Table being enabled in read-only
      Reading mode**.
- [x] **O1.T7 — Gate `view_source_mode`.** Report §4.5: `updateVaultActions`
      (`:2965-2990`) lists `view_editing_mode` and `view_reading_mode` and omits
      `view_source_mode` outright.
- [x] **O1.T8 — Enablement for `edit_undo`/`edit_redo`** from the active view's real
      stack depth (bases `QUndoStack`, canvas `QUndoStack`, Markoff `undoD2`). They
      are currently always enabled.

**Gate:** offscreen suite green **plus** `tst_action_context_no_silent_noop` — an
introspection test that walks `actionCollection()`, and for each enabled action
asserts the controller reports a handler for the current view type. Live eyeball
**not** required (no visual change).

**DONE 2026-08-20** (`7c9faf91` zoom dispatch, `c3fbf00d` BasesView
focusSearch/undo-query, `bbe1c658` `ActionContextController` extraction +
tests). 317/317 offscreen (only the pre-existing documented
`tst_canvas_perf_500` `-j10` contention flake seen transiently, confirmed
passing standalone and in the final full-suite run).  One deviation from
plan-as-written: the acceptance-gate test and O1.T3's zoom test both showed
2 spurious failures under `ctest -j10` load on a first pass (`view_zoom_in`
reported enabled-with-no-handler in the `no-vault`/`bases` contexts, and the
editor-mode radio appeared not to clear on in-place view swap) — 3 standalone
reruns of `tst_action_context` and a full serial-safe rerun were clean, so
this is judged the same class of parallel-load flake as `tst_canvas_perf_500`,
not a logic bug; flagged here rather than silently dropped. Also fixed one
unrelated pre-existing-shaped bug surfaced by the new
`tst_graphview_plugin::zoomDispatchesToViewportTransform` test itself: a
stack-allocated `GraphView` reparented onto a stack-allocated `QWidget host`
segfaulted (double-destruction — locals destruct in reverse declaration
order, so `host` destructed first and deleted its Qt-owned child `view`,
which then destructed a second time at its own scope exit); fixed by
declaring `host` before `view`.

---

### Phase O2 — Tier-B capability contract

- [x] **O2.T1 — `View` capability surface.** Add to `libs/core` `View`:
      `virtual bool canEdit() const`, `canSave()`, `canZoom()`, `canFind()`,
      `hasSelection()`, `canUndo()`, `canRedo()`. Sensible base defaults (`false`
      except `canZoom`). Deliberately discrete virtuals rather than a bitmask —
      grep-able, and each has a natural per-type answer.
- [x] **O2.T2 — `View::contextChanged()` signal**, emitted whenever any capability
      answer could have changed. `MarkdownView` re-emits from the existing
      `NoteEditorWidget::editorContextChanged`; `CanvasFileView` from
      `CanvasScene::selectionChanged`; `BasesView` from its selection model;
      `GraphView` never (constant capabilities).
- [x] **O2.T3 — Controller consumes it.** `ActionContextController` connects
      `contextChanged` on the active view (rebound per leaf, same disconnect
      discipline as O1.T2) and runs the Tier-B/C refresh.
- [x] **O2.T4 — Route Reading mode through it.** `MarkdownView::canEdit()` returns
      `activeLeaf()->hasEditing()`. Retires the bespoke `hasEditing()` read in
      `updateEditorActionStates`.
- [x] **O2.T5 — Route vault-open through it** as a window-level capability. Retires
      `updateVaultActions`' hand-maintained 16-entry list.

**Tests:** `tst_view_capabilities` (one slot per type × per predicate);
`tst_action_context::readingMode_disablesFormatVerbs`;
`tst_action_context::noVault_disablesVaultActions`.

**Gate:** offscreen green. No visual change yet.

**DONE 2026-08-20** (`d8c414ac` capability surface, `ade51e03` controller
wiring). 318/318 offscreen (only the documented `tst_canvas_perf_500`
`-j10` flake, unrelated). O2.T5 needed no new code — `updateVaultActions`
was already folded into the controller's single `refresh()` pass in O1;
this task's "retirement" was already satisfied. Found and fixed one real
bug the new tests caught: `insert_table`/`insert_callout` were still
gated on `isMarkdown` alone (not `canEdit`), despite O1.T6's own comment
claiming this was fixed — Insert ▸ Table stayed enabled in read-only
Reading mode. Also found and fixed a genuine teardown-lifetime crash
while building `CanvasFileView`'s `contextChanged()` wiring: connecting
`QUndoStack::canUndoChanged`/`canRedoChanged` straight to
`View::contextChanged` (a signal on an intermediate base) crashes on
teardown, because `QUndoStack::~QUndoStack()` calls `clear()` — emitting
those signals — from *inside* `QWidget::~QWidget()`'s child teardown,
i.e. after `View::~View()` has already run and the object's vtable no
longer resolves to `Corbomite::View`. Fixed with an explicit
`CanvasFileView` destructor that disconnects early, before any
base-class teardown starts. Detail: `decisions-archive.md`.

---

### Phase O3 — The `ViewActions` provider mechanism (markdown only)

Proves the mechanism with **zero new features**. The only user-visible change: the
Format / Heading / Insert / Table menus and the editor-mode group vanish on
non-markdown tabs. (Today, post-O1/O2, they only *grey out* off-markdown — Tier B
disable, not Tier A hide. This phase is what makes them actually disappear.)

> **Executor note (added 2026-08-20, post-O1/O2):** O1 moved ~289 lines out of
> `MainWindow.cpp` into `ActionContextController.{h,cpp}` (see decisions-archive),
> so most of this phase's `MainWindow.cpp:<line>` citations below were written
> against the pre-O1 file and have likely drifted — two known-stale ones
> (D4's `RibbonToolBar` citation, O3.T2's punch-list citation) are already
> corrected inline. **Re-grep the rest (O3.T6's `setupActions()`/`corbomiteui.rc.in`
> locations for Format/Heading/Insert/Table/fold) before trusting them** — the
> action ids themselves (`format_bold`, `heading_1`, `insert_table`, `table_row_above`,
> `fold_all`, `editor_toggle_mode`, …) are all still `grep`-able by name in
> `MainWindow.cpp` and unaffected by the refactor, just at new line numbers.
> `ActionContextController` is now the source of truth for every action's Tier-B/C
> logic (`hasHandlerForCurrentContext()`'s table in particular is worth reading
> first — it already enumerates every action this phase needs to redistribute by
> view type). Current baseline: **318/318 offscreen**, `master` at `c10afea7`.

- [x] **O3.T1 — `Corbomite::ViewActions`** in `libs/core`:
      ```cpp
      class ViewActions : public QObject, public KXMLGUIClient {
          virtual QString viewType() const = 0;
          virtual void bind(View *view) = 0;    // Tier C: reflect state
          virtual void unbind() = 0;
          virtual void refresh() = 0;           // Tier B: capability → setEnabled
          virtual QList<QAction *> toolBarActions() const = 0;
      };
      ```
      Each provider owns its own `KActionCollection` — legal and already precedented
      by `CorbomiteMDI::GUIClient` (`CorbomiteMDI.cpp:78-131`), a second XMLGUI
      client merging into the View menu.
- [x] **O3.T2 — Provider registry + eager construction.** All providers are built at
      `MainWindow` construction and registered by view type; only *installation* is
      dynamic. **Forced by the Hotkeys page** — `SettingsDialog` embeds
      `KShortcutsEditor` over a single collection (`punch-list.md:273` — corrected
      2026-08-20, the punch list has grown since this plan was written; confirmed the
      `SettingsDialog` ctor still takes one `KActionCollection*`, exactly as this task
      assumes) and must show
      every type's shortcuts including types with no tab open. `SettingsDialog`'s
      ctor changes to take a list of collections.
- [x] **O3.T3 — Client swap in `ActionContextController`.**
      `guiFactory()->removeClient(old); addClient(next); next->bind(view);` guarded
      by "only when the resolved view type actually changed", so an ordinary
      same-type tab switch costs one `refresh()`, not an XMLGUI rebuild.
      API verified present: `kxmlguifactory.h:106,113`.
- [x] **O3.T4 — Persistent per-provider `KToolBar` + tri-state policy** per §D4.
      Created after `setupGUI` (`RibbonToolBar` pattern). Policy persisted in a new
      `corbomite.kcfg` `<group name="Toolbars">`. **Re-apply policy after
      `applyMainWindowSettings`** or KMainWindow's restored visibility wins.
- [x] **O3.T5 — `<Merge name="viewtype_merge"/>` in `corbomiteui.rc.in`** per §D5;
      **bump `version="10"` → `"11"`** or users keep a stale cached rc in
      `~/.local/share/kxmlgui5/corbomite/`.
- [x] **O3.T6 — `MarkdownViewActions`.** Move Format (5 live + 5 stubs), Heading
      (6 live + 2 stubs), Insert (3), Table (6 stubs), fold (3 stubs),
      `editor_toggle_mode`, the three editor-mode radios, and `insert_template` out
      of `MainWindow::setupActions` and `corbomiteui.rc.in` into the provider.
      `edit_find`/`edit_replace` stay universal (O1.T5 routes them).

**Verification tasks (do before writing provider #2):**
- [x] **O3.V1** — measure client-swap cost on a tab switch between types with a
      populated menubar. If it visibly flickers, fall back to the report §6.2
      Option B (`setVisible` toggling) for menus and keep the mechanism for toolbars.
- [x] **O3.V2** — confirm `append="viewtype_merge"` places the type menu in a stable
      slot and File/Edit/Go/View/Settings/Help never shift.

**Tests:** `tst_view_actions_provider` (install/uninstall leaves the collection
clean, no dangling shortcuts); `tst_action_context::typeSwap_installsCorrectClient`;
`tst_toolbar_policy` (Auto/AlwaysShow/AlwaysHide × in/out of context → visibility +
enabled state).

**Gate:** **live eyeball** — menubar reflow on tab switch, no flicker, toolbar
context-menu override works and sticks across restart.

**DONE 2026-08-20** (`e22ef7de` `ViewActions`/`ToolBarPolicy` base,
`a7e04fb0` `MarkdownViewActions` + controller client-swap + toolbar policy +
rc merge point + `SettingsDialog` multi-collection). 320/320 offscreen. One
Tier-B bug found and fixed during implementation: `MarkdownViewActions`
synced Tier-C check state on `editorContextChanged`/`viewModeChanged` but
never re-ran Tier-B `refresh()` on those signals, so entering Reading mode
left `format_bold` etc. enabled. **Live-eyeballed and confirmed**: Format/
Heading/Insert/Table + the editor-mode group vanish (not grey out) off
markdown, File/Edit/Go/View/Settings/Help never shift, and the toolbar's
Auto/Always Show/Always Hide context menu works. The first live pass looked
broken — the menus appeared but stayed permanently empty on every tab type —
which traced to a real KXMLGUI gotcha, **not a code defect**: dev builds
across different git worktrees on this machine share one KXMLGUI cache
(`~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc`), keyed only by
the dev app/component name, not by branch. A stale cache written earlier the
same day by the `feature/rich-clipboard` worktree's dev build (its own
`edit_copy_as` menu shape, independently also at `version="11"`, so the
version-bump self-heal never triggered) baked Format/Heading/Insert/Table in
as permanent base-document entries with no merge point — `MainWindow`'s own
client, not the provider, "owned" those containers, so
`KXMLGUIFactory`/`ContainerNode::destruct()`'s ownership check
(`client == state.guiClient`) could never match and delete them on
`removeClient()`. Deleting the stale cache file and relaunching fixed it
immediately; no production code changed. Noted as a standing caveat in
`CLAUDE.md`'s Dev Build Isolation section for future multi-worktree
sessions. **Next: Phase O4** (`CanvasViewActions` — the phase the user
feels; snap/grid/zoom toggles for canvas, closes punch-list
`[ui-bundle][canvas][P2][cluster-o]`).

---

### Phase O4 — `CanvasViewActions` (the phase the user feels)

Closes punch-list `[ui-bundle][canvas][P2][cluster-o]`.

- [ ] **O4.T1 — Expose the primitives.** `CanvasScene::alignmentStrategy()` accessor
      (currently a private `m_alignmentStrategy`, `CanvasScene.h:285`);
      `CanvasView::setGridVisible(bool)` + honour it in `drawBackground`
      (`CanvasView.cpp:232`); a `CanvasViewTab::canvasView()` accessor (only
      `canvasScene()` exists today).
- [ ] **O4.T2 — kcfg group** `<group name="Canvas">`: `SnapToGrid` (default `true`),
      `SnapToObjects` (default `true`), `ShowGrid` (default `true`). App-wide per
      **Q5**. Obsidian's defaults are both-ON (`canvas.md` §9.5) — match.
- [ ] **O4.T3 — Canvas menu + `canvasToolBar`:** `canvas_snap_grid` (checkable),
      `canvas_snap_objects` (checkable), `canvas_show_grid` (checkable),
      `canvas_zoom_in`/`_out`/`_reset` (via O1.T3's virtuals),
      `canvas_zoom_to_fit` (Shift+1), `canvas_zoom_to_selection` (Shift+2).
- [ ] **O4.T4 — Apply settings to every open canvas**, not just the focused one:
      the three toggles are app-wide, so a change must fan out to every live
      `CanvasScene`/`CanvasView`. Hook `CorbomiteSettings::configChanged`, same
      shape as `applyReadableLineWidth` (`MainWindow.cpp:3106`).
- [ ] **O4.T5 — Tier C:** checkable actions reflect kcfg on install; Tier B disables
      zoom-to-selection when nothing is selected (`hasSelection()`).
- [ ] **O4.T6 — Wire-when-M5-lands placeholders:** `canvas_lock` (read-only),
      `canvas_jump_to_group`, `canvas_convert_to_file`. Register them in the
      provider **disabled with a named TODO**, so landing M5 is a five-line change
      per action rather than a menu redesign. (These are the only new stubs this
      cluster is permitted to add, and only because M5 is already planned.)

**Tests:** `tst_canvas_view_actions` (each toggle reaches
`CanvasAlignmentStrategy`/`CanvasView`); `tst_canvas_settings_fanout` (two open
canvases both follow a kcfg change).

**Gate:** **live eyeball** — snap actually toggles off, grid hides, zoom-to-fit
frames the content, settings survive restart.

---

### Phase O5 — `BasesViewActions`, `GraphViewActions`, folded Cluster I remnant

- [ ] **O5.T1 — `BasesViewActions`** (**Q8**): promote Properties, Sort+group, Views,
      Filters, Drawer, +New, Results to KActions. `BasesView`'s existing
      `QToolButton`s become `setDefaultAction()` on them — **one definition, four
      surfaces** (in-view toolbar, menubar, palette, Hotkeys page). The in-view
      toolbar stays exactly where it is; this is additive.
      **Retires:** the seven bare `QToolButton` click connections
      (`BasesView.cpp:74-128, 165+`).
- [ ] **O5.T2 — Bases Tier B/C:** Filters/Drawer reflect state; +New disabled when
      the query has no writable target.
- [ ] **O5.T3 — `GraphViewActions`:** `graph_open_local`, `graph_animate`,
      `graph_copy_screenshot` (already a `CommandRegistry` command — the provider
      action delegates to it rather than duplicating, per §D3),
      `graph_zoom_to_fit`, `graph_reset_forces`.
- [ ] **O5.T4 — Provider registration via `ViewRegistrar`.** Graph's view type is
      plugin-registered (`GraphViewPlugin.cpp:41`), so its provider must register
      through the same proxy — proving a **plugin can ship a `ViewActions` provider
      with no new plugin-API surface**. `ViewRegistry::viewRegistered/
      viewUnregistered` is the install/uninstall hook (report §12).
- [ ] **O5.T5 — Folded Cluster I Phase 5 tail** (**Q11**): `leaf_link_with_active_pane`
      (checkable, over `WorkspaceLeaf::setGroup`, universal collection) and
      `tab_move_left`/`tab_move_right` (Ctrl+Shift+PgUp/PgDn, universal).

**Tests:** `tst_bases_view_actions` (each action drives the same path the button
did); `tst_graph_view_actions`; `tst_leaf_link_group`.

**Gate:** live eyeball on bases (buttons still work, shortcuts now work) and graph.

---

### Phase O6 — Pane-local inspector; relocate graph controls (**Q7**)

- [ ] **O6.V1 — Verify the Obsidian claim first.** There is **no `domains/graph.md`**
      in the audit corpus (`addenda/2026-04-19-graph-screenshot.md` §preamble says
      so). The claim that Obsidian renders graph settings inside the graph pane is
      product-behaviour, not corpus. Confirm against Obsidian before building; if
      wrong, the relocation still fixes the cross-talk bug and stands on that alone.
- [ ] **O6.T1 — Promote `CollapsibleSection`** from `src/plugins/graph-view/` to
      `libs/core` (it is the right primitive and is currently graph-private).
- [ ] **O6.T2 — `View`-level inspector slot:** a collapsible panel inside the view's
      own pane, toggled by a header action. **Revive `ItemView::addAction(icon,
      title, callback)`** (`ItemView.h:27`) for the cog — it is Obsidian's
      `view.addAction` contract, already built and tested, used today by exactly one
      class (`EmptyView.cpp:44-49`).
- [ ] **O6.T3 — Move `GraphControlsPanel` into the graph pane.** Each `GraphViewTab`
      owns its own instance. **This fixes report §4.3 by construction**: today one
      singleton (`GraphViewPlugin.cpp:97-103`) is wired by every tab via
      `wireControlsPanel()`'s ~13 `connect()` calls with **no disconnect**
      (`GraphViewTab.cpp:144-195`), so two graph tabs both react to one slider.
      Remove the `X-Corbomite-DockArea: right` tool-view hosting for graph.
      **Retires:** `GraphViewPlugin::createView`'s singleton panel.
- [ ] **O6.T4 — Canvas inspector** (snap / grid / background) as the second
      consumer, proving the slot generalises.

**Tests:** `tst_graph_controls_per_instance` — two `GraphViewTab`s, move one
slider, assert only that tab's engine changed (fails before O6.T3).

**Gate:** live eyeball.

---

### Phase O7 — Stub disposition, palette, soak, closeout (**Q9**)

- [ ] **O7.T1 — Implement the three cheap actions** per §D7: `heading_increase`,
      `heading_decrease`, `insert_wiki_link`, `insert_image`.
- [ ] **O7.T2 — Keep the rest as stubs**, but move every stub behind a single
      `registerPlannedAction()` helper carrying a one-line "blocked on: <what>"
      string, so the Hotkeys page and palette can filter them and a future session
      can grep the blockers.
- [ ] **O7.T3 — Markoff steer.** Handoff brief at
      `docs/handoff/2026-XX-XX-to-markoff-block-structural-verbs.md` requesting the
      verbs the remaining stubs need: block-kind change (blockquote / code block /
      thematic break), task-state toggle (the ListItem marker is outside the edited
      byte space), table row/column ops, fold. Cite `applyFlatEdit`'s
      Paragraph-only block creation as the reason these cannot be done consumer-side.
- [ ] **O7.T4 — Placebo dialogs:** `insert_table` / `insert_callout` currently exec
      and discard (`MainWindow.cpp:511-535`, punch-list P3). Disable them with the
      same "blocked on" annotation until O7.T3's verbs land. **Do not** ship an
      `applyFlatEdit`-based fake — it would insert flat Paragraphs.
- [ ] **O7.T5 — Palette grouping** (report §4.7): `showCommandPalette` buckets by
      `objectName().startsWith("file_"/"view_")` and dumps ~60 actions incl. all
      stubs into "Other", listing disabled ones. Regroup by owning surface
      (Universal / <Type> / Commands) and drop disabled entries.
- [ ] **O7.T6 — Sidebar raise-on-type** (**Q6**), optional: on first activation of a
      view type in a session, *raise* (never create/destroy) a configured tool view.
      **Blocked on** punch-list `[workspace][P2]` sidebar-layout persistence — build
      it only if that has landed, otherwise defer and say so.
- [ ] **O7.T7 — Soak + closeout.** Retro at `docs/cluster-retros/cluster-o.md`;
      archive plan + spec; `INDEX.md` → Done; PROJECT-STATE per the rituals.

---

## Cross-cluster boundaries

**Cluster M (canvas authoring).** M5.2 plans an **on-canvas floating control cluster**
(gear / zoom / undo-redo, Obsidian-faithful) while O4 builds a **canvas toolbar**.
These overlap. Obsidian has the on-canvas cluster *because it has no menubar*;
Corbomite does. **Decision to record in both plans (O0.T6): O4 owns the KAction /
menu / toolbar surface; M5 owns the underlying capabilities** (zoom clamp, read-only
lock flag, jump-to-group, convert-to-file). M5.2's floating widget should be slimmed
to canvas-intrinsic affordances or dropped — reassess when M5 is dispatched. O4.T6
pre-registers the M5-dependent actions so the join is trivial.

**Cluster I.** Closed by O0. Phase 5 tail → O5.T5; Phase 6 → punch list.

**Cluster N (rich clipboard).** Unrelated, isolated on `feature/rich-clipboard`. If N
adds copy-as/paste-as **actions**, they are universal-collection actions gated by
Tier B `hasSelection()`. No coordination needed beyond that.

**MainWindow decomposition spec.** O1.T1 performs its action-controller extraction
and supersedes its "no action-framework redesign" non-goal. The spec's other three
steps (VaultSessionController, plugin-host dedupe, theme mapping) are untouched.

---

## Standing rules and traps

1. **Never add a new always-disabled global action.** The only exception is O4.T6,
   and only because M5 is already planned.
2. **`corbomiteui.rc` version bump on every structural change**, or stale cached rc
   files in `~/.local/share/kxmlgui5/corbomite/` silently win.
3. **Providers eagerly constructed, dynamically installed.** Lazy construction breaks
   the Hotkeys page (O3.T2).
4. **Toolbar policy re-applied after `applyMainWindowSettings`** (§D4 trap).
5. **Disconnect discipline:** every per-leaf/per-view connection must be stored and
   disconnected on rebind. The `GraphControlsPanel` bug (report §4.3) is exactly this
   mistake; the existing `m_activeLeafHistoryConnection` pattern is the model.
6. **`applyFlatEdit` creates Paragraphs only.** Never ship a "cheap" insert that
   needs a different block kind (§D7).
7. **i18n every user-visible string** (`i18n()`), `QIcon::fromTheme` for every icon —
   canvas menus already violate this (Cluster M plan §9); do not extend the violation.
8. **Live-eyeball gate is standing** for O3, O4, O5, O6. Project memory: an
   offscreen-green keyboard/focus fix was previously found broken live. Offscreen
   green is necessary, never sufficient, for chrome changes.
9. **Do not touch `.obsidian/`.** Nothing here is vault format.

---

## Appendix A — Normative action assignment

Every action in the collection, and where it ends up. `MainWindow` = universal
collection + `corbomiteui.rc`.

| Actions | Destination |
|---|---|
| `file_open_vault`, `file_open_recent`, `file_close_vault`, `file_new_note`, `file_new_canvas`, `file_save`, `file_quit`, `options_configure` | MainWindow |
| `edit_undo`, `edit_redo`, `edit_find`, `edit_replace`, `edit_find_next`, `edit_find_prev` | MainWindow (Tier B routed, O1.T5/T8) |
| `quick_switcher`, `command_palette`, `search_vault`, `graph_view`, `open_daily_note` | MainWindow |
| `tab_*` (close/next/prev/undo_close/jump_1-8/jump_last/pin_toggle/move_to_new_window/toggle_stacked), `split_right`, `split_down`, `go_back`, `go_forward` | MainWindow |
| `tab_move_left`, `tab_move_right`, `leaf_link_with_active_pane` | MainWindow (new, O5.T5) |
| `view_toggle_left_sidebar`, `view_zoom_in`, `view_zoom_out`, `view_zoom_reset` | MainWindow (polymorphic, O1.T3) |
| `editor_toggle_mode`, `view_source_mode`, `view_editing_mode`, `view_reading_mode` | **MarkdownViewActions** |
| `format_bold`, `format_italic`, `format_strikethrough`, `format_inline_code`, `insert_link` | **MarkdownViewActions** |
| `heading_1`…`heading_6`, `heading_increase`, `heading_decrease` | **MarkdownViewActions** |
| `insert_wiki_link`, `insert_image`, `insert_code_block`, `insert_block_quote`, `insert_horizontal_rule`, `toggle_checkbox`, `insert_table`, `insert_callout`, `insert_template` | **MarkdownViewActions** |
| `table_row_above/below`, `table_col_left/right`, `table_delete_row/col` | **MarkdownViewActions** (stubs) |
| `fold_all`, `unfold_all`, `toggle_fold` | **MarkdownViewActions** (stubs) |
| `canvas_snap_grid`, `canvas_snap_objects`, `canvas_show_grid`, `canvas_zoom_*`, `canvas_lock`, `canvas_jump_to_group`, `canvas_convert_to_file` | **CanvasViewActions** (new) |
| bases: properties, sort+group, views, filters, drawer, new-item, results | **BasesViewActions** (new, promoted from `QToolButton`) |
| `graph_open_local`, `graph_animate`, `graph_copy_screenshot`, `graph_zoom_to_fit`, `graph_reset_forces` | **GraphViewActions** (new) |
| `corbomite_mdi_*` | unchanged (`CorbomiteMDI::GUIClient`) |

## Appendix B — Files touched (projected)

**New:** `src/app/ActionContextController.{h,cpp}`,
`libs/core/{include/corbomite/core,src}/ViewActions.{h,cpp}`,
`src/editor/MarkdownViewActions.{h,cpp}`, `src/canvas/CanvasViewActions.{h,cpp}`,
`libs/bases/…/BasesViewActions.{h,cpp}`,
`src/plugins/graph-view/GraphViewActions.{h,cpp}`,
`libs/core/…/CollapsibleSection.{h,cpp}` (moved),
`docs/cluster-retros/cluster-{i,o}.md`, 8 new test files.

**Modified:** `src/app/{MainWindow.{h,cpp},corbomiteui.rc.in,corbomite.kcfg}`,
`src/dialogs/SettingsDialog.{h,cpp}`, `libs/core/…/View.{h,cpp}`,
`libs/core/…/ItemView.cpp`, `src/editor/{MarkdownView,NoteEditorWidget}.{h,cpp}`,
`src/canvas/{CanvasFileView,CanvasViewTab}.{h,cpp}`,
`libs/canvas/…/{CanvasScene,CanvasView}.h`, `libs/bases/…/BasesView.{h,cpp}`,
`src/plugins/graph-view/{GraphViewPlugin,GraphView,GraphViewTab}.{h,cpp}`,
`src/plugins/graph-view/metadata.json.in`, `docs/punch-list.md`,
`docs/PROJECT-STATE.md`, `docs/superpowers/plans/INDEX.md`.

---

## Appendix C — Implementer's decision guide (added 2026-08-20, post-O3)

Written for whoever dispatches O4 (`CanvasViewActions`), O5
(`BasesViewActions`/`GraphViewActions`), or any later provider. Doctrine
D1-D7 above is normative; this appendix is the practical **checklist** for
turning a new action or setting into code without re-deriving the doctrine
from scratch, and folds in two things O1-O3 only learned by hitting them.

### C1 — Where does a new *action* live?

Ask in this order; stop at the first "yes":

1. **Is it meaningful for any-or-no focused tab** (vault, tab/window, global
   navigation — nothing about it presupposes a document *kind*)? →
   **`MainWindow`'s universal `KActionCollection` + `corbomiteui.rc.in`**,
   outside the `viewtype_merge` point. Never let a provider redeclare one of
   these by name (see C4).
2. **Does it belong to exactly one view type's own authoring/viewing
   semantics** (Bold only means something on markdown; snap-to-grid only
   means something on canvas)? → **that type's `ViewActions` provider.**
   Two sub-cases:
   - It needs a **new top-level menu slot** (Format, Heading, Canvas, …) →
     declare a `<Menu name="…" append="viewtype_merge"/>` in the provider's
     own XML (per D5). The name must be unique to this provider — see C4.
   - It's a natural extension of an **existing universal menu** (e.g.
     markdown's Editor Mode submenu living inside `view`) → merge into that
     container by name without `append=`; multiple clients sharing a
     genuinely-universal container (like `view`) is fine, unlike case C4.
3. **Is it per-*instance*, not per-type** (rename this tab, export this
   pane, copy this pane's path)? → the pane's hamburger
   (`onMoreOptionsMenu`), **not** a `QAction` at all — *unless* it also
   wants a shortcut, a command-palette entry, or a Hotkeys-page row, in
   which case D3's rule forces it into a real collection (universal or
   provider) instead; the hamburger is only for things that legitimately
   have none of those three.
4. **Is it plugin-supplied?** → `CommandRegistry` (commands) or
   `RibbonToolBar` (ribbon icons, Q4) — never host-owned.

### C2 — Hide or disable? (the D2 litmus test)

Ask: *would a user who has never seen this app be confused about why this
menu item exists at all on the current tab?*

- **Yes** → the action doesn't belong to this document kind. **Hide it**
  (Tier A — don't install this provider's client when the kind doesn't
  match; D2/D1).
- **No, they'd understand why it's here, it's just not usable right now**
  (nothing selected, read-only, no vault open, feature not wired yet) →
  **disable it** (Tier B — a capability predicate in `refresh()`).

If you're re-using one of `View`'s existing seven capabilities
(`canEdit`/`canSave`/`canZoom`/`canFind`/`hasSelection`/`canUndo`/`canRedo`),
call it directly. If the predicate is *specific to your type* and doesn't
generalise (e.g. "is a template selected"), don't force it onto the shared
`View` base — compute it locally inside your provider's own `refresh()`
against your bound view's concrete type, exactly as `MarkdownViewActions`
does with `canEdit` today. Only promote a predicate to `View` when a second
type would also need it.

### C3 — Where do *options/settings/toggles* turn up? (the second standing question)

There is one governing rule: **define the checkable `QAction` exactly once,
in the provider (or `MainWindow`) that owns the underlying capability, then
attach it to every surface that wants it** — menu entry, `toolBarActions()`
entry, and (once O6 lands) the pane-local inspector — via `addAction()` /
`setDefaultAction()`. This is the same "one definition, N surfaces" shape
O5.T1 already applies to the bases toolbar buttons; treat it as the general
rule, not a bases-only pattern. Never define the "same" toggle twice as two
separate `QAction`s with independently-maintained checked state — they will
drift.

Then decide *where the value itself lives* (its source of truth, independent
of how many places display it):

| Setting shape | Source of truth | Tier C sync |
|---|---|---|
| Per-document/per-instance, changes what's rendered right now (selection-driven, ephemeral) | in-memory state on the bound view/document | `refresh()`/`bind()` reads it straight off the view each time |
| App-wide, should survive restart, not tied to one document (canvas snap/grid — O4.T2's pattern) | a `corbomite.kcfg` `<group>` owned by that type | checkable action's checked-state set from kcfg on install **and** on `CorbomiteSettings::configChanged` (O4.T4's fan-out pattern — one settings change must reach every open instance of that type, not just the focused one) |
| Genuinely global, unrelated to any document type (theme, autosave interval, shortcuts scheme) | `SettingsDialog` page | not a toolbar/menu action at all |
| A toolbar's own meta-behaviour (visible/hidden, icon/text style) | `ActionContextController`'s `ToolBarPolicy` kcfg group (visibility) + `KToolBar`'s own built-in per-toolbar state (icon style/size) | don't reinvent either — see C5 for the trap in overriding KToolBar's context menu |

### C4 — The container-ownership trap (learned the hard way in O3)

**A `<Menu name="X">` (or any named container) may be declared by exactly
one client for its whole lifetime, or KXMLGUI can never delete it.**
`KXMLGUIFactory`'s `ContainerNode::destruct()` only removes an empty
top-level container when the client calling `removeClient()` is the
container's sole recorded owner (`clients.isEmpty() && client ==
state.guiClient`); if a second client — including `MainWindow`'s own base
document, even via a leftover stub — ever contributes to a container with
the same `name`, that container can no longer be fully removed when your
provider unbinds. Concretely:

- **Never declare a `<Menu>` in both `corbomiteui.rc.in` and a provider's
  own XML with the same `name`.** If you're adding a brand-new provider
  menu, it exists *only* in the provider's XML, `append="viewtype_merge"`,
  full stop — don't also stub it in the base rc "for clarity."
- **Reusing an existing universal container name on purpose** (e.g.
  merging into `view`) is fine — that container is *meant* to have multiple
  contributing clients and is never expected to disappear.
- **Bump the `<gui version="…">` number on `corbomiteui.rc.in` any time you
  touch its container structure**, and know that the bump can still fail to
  self-invalidate a stale cache if a *different build* (e.g. another git
  worktree's dev build, sharing the same KXMLGUI component name) happened
  to reach the same version number first — see the `CLAUDE.md` Dev Build
  Isolation caveat. If a fresh menu/toolbar change doesn't appear to take
  effect after a clean rebuild, check
  `~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc` for stale
  content before suspecting your own code.

### C5 — Toolbars: icon-only by default, and the context-menu trap

Every toolbar defaults to icon-only (`registerToolBar()` sets
`Qt::ToolButtonIconOnly` centrally for provider toolbars; `mainToolBar`
gets it declaratively via `corbomiteui.rc.in`'s `iconText="icononly"`).
**If your provider's toolbar is created programmatically** (no `<ToolBar>`
XML element — the pattern every provider toolbar has followed since O3,
since KXMLGUI's `Default` flag on `setupGUI()` would otherwise delete a
toolbar built before it), you get this for free by calling
`registerToolBar()`; don't set the style yourself.

Separately: `installToolBarContextMenu()` installs `Qt::CustomContextMenu`
on every registered provider toolbar, which **fully replaces** KToolBar's
own built-in right-click menu — including its icon/text style picker. O3
re-offers that picker inside the custom menu (Icon Only / Text Under Icon /
Text Alongside Icon) precisely so this doesn't regress Q3's user-override
requirement. **If you add a second context-menu-worthy per-toolbar setting
later, extend this same menu — don't stack a third custom context menu on
top of it.**

### C6 — Per-provider pre-flight checklist

Before writing a new `ViewActions` subclass:

- [ ] Own `KActionCollection`, own `componentName` (distinct from every
      other provider's).
- [ ] Constructed eagerly at `MainWindow` construction (C3.T2 requirement —
      the Hotkeys page needs your shortcuts even with no tab of your type
      open); only `addClient`/`removeClient` is dynamic.
- [ ] Every new top-level `<Menu>` uses `append="viewtype_merge"` and a
      `name` no other client declares (C4).
- [ ] `bind(View*)`/`unbind()` follow the disconnect discipline: store every
      per-view `QMetaObject::Connection` and disconnect it at the top of
      `bind()` before making new ones, and in `unbind()`. This is the exact
      shape of the `GraphControlsPanel` cross-talk bug (report §4.3) that O6
      exists to fix structurally — don't reintroduce a narrower instance of
      it in a new provider.
- [ ] `refresh()` re-runs on every signal that can change either Tier B or
      Tier C state for your type (O3's own bug: Tier C sync forgot to also
      call `refresh()`, leaving Reading-mode format verbs wrongly enabled —
      don't split the two without a specific reason).
- [ ] `i18n()` every string, `QIcon::fromTheme()` every icon (standing rule
      7 — canvas menus already violate this per the Cluster M plan; don't
      extend the violation into O4).
- [ ] Tests: an install/uninstall-cleanliness test (`tst_view_actions_provider`
      pattern), a Tier-B/C refresh test, and a toolbar-population test —
      the three the O3 "Tests:" line names, generalised to your type.
