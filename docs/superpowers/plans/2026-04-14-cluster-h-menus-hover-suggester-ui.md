# Cluster H — Menus / hover / suggester UI

> **Living-status note:** This file is the *plan*. Live status (Not started / In progress / Done / Blocked) is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md) Roadmap. Update PROJECT-STATE per the rituals in [`docs/CONTRIBUTING-OPS.md`](../../CONTRIBUTING-OPS.md), not this file. Edit this file only when the plan itself changes (work breakdown, target classes, references).

**Plan written:** 2026-04-14. Derived from `docs/obsidian-audit/GAP-ANALYSIS.md` §Cluster H.

**Covers:** P2.7 (hover-link preview infrastructure), P2.24 (Menu section-ordering protocol), P2.25 (mid-construction menu signals), P2.31 (Ribbon plugin slot), P3.10 (EditorSuggest registry with insertion-order iteration), P3.11 (HoverLinkSource registry).

## Goal

Build the UI-primitive layer that makes Corbomite's chrome *feel* like Obsidian's while staying KDE-native underneath: three registries (HoverLinkSource, EditorSuggest, Ribbon slot), the Menu section-ordering coordination protocol, the mid-construction menu signal pattern, and a hover-link preview widget. Obsidian users type `[[`, hover a link for 300ms, and right-click a tab — all three interactions must feel familiar. The widget *chrome* is free to be KDE-idiomatic (`QMenu`, `QFrame` popover, `KToolBar` for ribbon); the *contracts* between these widgets and the rest of the app (which determine plugin authorability later) must match the audit specs exactly.

Cluster grouped because all six gaps terminate in the same UI layer and share the Component lifecycle + the mid-construction emit pattern. Splitting would produce widgets that visually work but fail the plugin-compat checkup.

## Audit references

- **Menu section-ordering protocol:** `domains/ui-bundle.md §5` — 30+ `addSections([...])` call sites surveyed; canonical section IDs are `title`, `open`, `action-primary`, `action`, `info`, `info.copy`, `view`, `system`, `""` (unset), `danger` per the synthesis finding. `Menu.sort()` algorithm documented.
- **Mid-construction emit pattern:** `domains/workspace.md §4` — `file-menu`/`url-menu`/`editor-menu`/`files-menu`/`leaf-menu`/`tab-group-menu`/`markdown-viewport-menu` all emit after core items are added and before the menu is shown, so plugins can push items into named sections.
- **`HoverLinkSource` registry:** `domains/workspace.md §7` (owner) + `domains/ui-bundle.md §7` (HoverPopover widget). **Pass 1 correction (applied):** hover delay is **300ms per-popover** (`waitTime`), not 500ms — 500ms is the **registry poll interval**.
- **`HoverPopover` three-wire architecture:** `domains/ui-bundle.md §7` — (a) `HoverLinkSource` registry entries (per-view-type, with `display` and `defaultMod` modifier key), (b) `hover-link` Workspace event carrying the source ID, (c) `HoverPopover` widget mounted at the cursor. All three are load-bearing; missing one breaks the UX.
- **`EditorSuggest` registry behaviour:** `domains/editor.md §3` — `EditorSuggestManager` uses **first-non-null-`onTrigger`-wins with no priority system**. Insertion order is the entire coordination mechanism. Built-ins registered first shadow plugin overrides of `[[`/`#`.
- **`EditorSuggest<T>` base class:** `domains/editor.md §3` — required overrides `onTrigger(cursor, editor, file)`, `getSuggestions(context)`, `renderSuggestion(value, el)`, `selectSuggestion(value, evt)`. `EditorSuggestTriggerInfo` shape `{start, end, query}`.
- **Ribbon slot:** `domains/ui-bundle.md` (implicit — `addRibbonIcon` returns a DOM element) + `02-extension-surfaces.md` entry "addRibbonIcon". `addRibbonIcon` **keys on `title`, not `icon`** — same-title collision is a preserved Obsidian bug (`domains/plugin.md §3`).
- **Corbomite already uses `KCommandBar`:** the command palette front-end is covered; this cluster adds the *suggester popup* (a different widget, for in-editor inline autocomplete) and the hover popover, not the palette.

## Target classes

| Class | File | Notes |
|---|---|---|
| `Corbomite::MenuSectionHelper` | `libs/core/src/MenuSectionHelper.{h,cpp}` | Wraps `QMenu` with section-ordering protocol. `addToSection(QAction*, sectionId)`, `insertSeparatorsBetweenSections()`. Canonical ordering from audit |
| `Corbomite::MenuEventEmitter` | `libs/core/src/MenuEventEmitter.{h,cpp}` | Implements the mid-construction emit pattern: core items added → `Q_EMIT fileMenu(menu, file)` → plugins (when they exist) push items → show. Signal names mirror Obsidian events |
| `Corbomite::HoverLinkSource` | `libs/core/include/corbomite/core/HoverLinkSource.h` | `{id: QString, display: QString, defaultMod: Qt::KeyboardModifier}` struct |
| `Corbomite::HoverLinkSourceRegistry` | `libs/core/src/HoverLinkSourceRegistry.{h,cpp}` | `register(HoverLinkSource) → Handle` / `unregister(Handle)`. Per-view-type opt-in. Emits via Qt signal, per the three-wire architecture |
| `Corbomite::HoverPopover` | `src/editor/HoverPopover.{h,cpp}` | `QFrame` widget with embedded `Markoff::ReadingView`. 300ms hover timer. Positions at cursor; dismissed on mouse-leave + N ms |
| `Corbomite::EditorSuggestTriggerInfo` | `libs/core/include/corbomite/core/EditorSuggestTriggerInfo.h` | `{start: CursorPos, end: CursorPos, query: QString}` |
| `Corbomite::EditorSuggest` | `libs/core/src/EditorSuggest.{h,cpp}` | Abstract base. Pure virtuals: `onTrigger`, `getSuggestions`, `renderSuggestion`, `selectSuggestion`. Inherits `Component` (from Cluster C) |
| `Corbomite::EditorSuggestManager` | `libs/core/src/EditorSuggestManager.{h,cpp}` | Insertion-order iteration; first-non-null-`onTrigger` wins. Dispatches cursor events to the winner's `getSuggestions`. No priority system (compat invariant) |
| `Corbomite::SuggestPopup` | `src/editor/SuggestPopup.{h,cpp}` | The `QListView`-inside-`QFrame` popup widget that renders `EditorSuggest.getSuggestions` output and hosts the `renderSuggestion` delegate |
| `Corbomite::RibbonSlot` | `src/app/RibbonSlot.{h,cpp}` | Wraps Corbomite's `KToolBar` left-dock. `addRibbonIcon(iconName, title, onActivated) → Handle`. **Keys on `title`** (Obsidian-compat bug preserved) |
| `Corbomite::Notice` | `src/dialogs/Notice.{h,cpp}` | Transient toast widget. Auto-dismiss timing matching Obsidian (needs confirmation — see open questions). Action button support |

Existing `CompletionPopup` (`src/editor/CompletionPopup.{h,cpp}`) subsumed into `SuggestPopup` or refactored to consume `EditorSuggestManager`. `KCommandBar` is untouched (command palette is separate from in-editor suggester).

## KDE / GPL3-compatible prior art

**Local KDE source convention:** the KDE source tree is checked out locally at `~/src/kde/src/<repo>`. **Always grep there first; never clone from `invent.kde.org` unless a repo is genuinely missing locally.** Verified-present locally: `kate`, `kdevelop`, `kio`, `kconfig`, `kconfigwidgets`, `kparts`, `kxmlgui`, `kwidgetsaddons`, `ktexteditor`, `krunner`, `baloo`, `okular`, `poppler`, `qtkeychain`, `sonnet`.

| Target | Local path | What we're looking for |
|---|---|---|
| **Hover tooltip with rich content** | `~/src/kde/src/kdevelop/kdevplatform/language/duchain/navigation/` | Declaration tooltips are the canonical KDE rich-content hover — timing, mount/unmount lifecycle, content-mini-renderer integration. **Highest-value reference for `HoverPopover`** |
| **Inline message-bar widget** | `~/src/kde/src/ktexteditor/src/messages/` | Message bar pops into the editor in-place — similar geometry challenges to HoverPopover, different trigger semantics |
| **Completion popup with delegate rendering** | `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/` | Rich-content per-row rendering; categorisation; keyboard nav. Direct prior art for `SuggestPopup` |
| **Fuzzy ranking for command / runner** | `~/src/kde/src/krunner/` | Existing ranking infrastructure we may be able to lean on (though Cluster D is the primary search/fuzzy cluster) |
| **Context menu construction with sections** | `~/src/kde/src/ktexteditor/` (`katedocument.cpp` right-click paths), `~/src/kde/src/kate/` (tab-bar context menus) | Concrete examples of multi-section `QMenu` with separators; validate `MenuSectionHelper` design |
| **Ribbon / vertical toolbar** | `~/src/kde/src/kdevelop/kdevplatform/shell/` (activity-bar) | KDevelop has a left-dock activity bar conceptually identical to Obsidian's ribbon |
| **Toast / notice** | `~/src/kde/src/ktexteditor/src/messages/` + `~/src/kde/src/kwidgetsaddons/` (look for `KMessageWidget`) | `KMessageWidget` + auto-dismiss is the probable direct substitute |
| **Suggest popup with inline content** | `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/` again — look at `AbstractNavigationContext` tooltip integration | Popup that itself renders rich content below the list — pattern we need for wiki-link previews inside the suggester |

## Work breakdown

**Phase 1 — Menu section protocol + event emitter:**
1. Implement `Corbomite::MenuSectionHelper`. Wraps a `QMenu` with a map `sectionId → QVector<QAction*>`. `finalize()` flushes in canonical order with separators. Test: create a menu, add items in scrambled section order, assert final `QMenu::actions()` is in audit-documented order.
2. Implement `Corbomite::MenuEventEmitter`. Signals for `fileMenu(QMenu*, File)`, `urlMenu(QMenu*, Url)`, `editorMenu(QMenu*, Editor*)`, `filesMenu(QMenu*, Files)`, `leafMenu(QMenu*, Leaf)`, `tabGroupMenu(QMenu*, TabGroup)`, `markdownViewportMenu(QMenu*, ViewportCtx)`. Emitted mid-construction (after core items, before show).
3. Wire existing right-click paths (tab bar, editor, file explorer) through `MenuEventEmitter`. No plugins exist yet — these signals go unobserved — but the emission points are in place for when plugins do exist.

**Phase 2 — HoverLinkSource registry + HoverPopover:**
4. Define `HoverLinkSource` struct and `HoverLinkSourceRegistry`. Registry stores `QHash<QString /* view-type */, HoverLinkSource>`. Register built-ins: `"editor"` (modifier: `Qt::NoModifier` — plain hover), `"search"` (modifier: `Qt::CTRL`), `"backlinks"`, `"outlinks"`, `"graph"`. `"bases"` registered per the audit's hardcoded-source quirk (domains/rendering.md §11).
5. Implement `Corbomite::HoverPopover`. `QFrame` with `Qt::Popup | Qt::FramelessWindowHint`. Contains a `Markoff::ReadingView` instance. `showForLink(QPoint anchor, QString notePath, QString subpath = "")`. 300ms delay from hover-start to show; cancel on mouse-leave before threshold. Dismiss on click-elsewhere or Esc.
6. Wire the `hover-link` signal emission: whenever a view renders a link span, attach a hover handler that (a) checks the view's `HoverLinkSource` is registered, (b) validates the modifier matches `defaultMod`, (c) starts the 300ms timer, (d) emits `workspace::hoverLink(sourceId, target, anchor)` which the `HoverPopover` subscribes to.
7. Integration test: hover a wiki-link in the editor for 300ms → popover appears with the target note rendered; move cursor away → popover dismisses.

**Phase 3 — EditorSuggest registry + SuggestPopup:**
8. Define `Corbomite::EditorSuggestTriggerInfo` and `Corbomite::EditorSuggest` abstract base. Inherits `Component` (Cluster C forward-ref; plan lands after C's Phase 1).
9. Implement `EditorSuggestManager`. `registerSuggest(EditorSuggest*)` pushes to a `QVector` (insertion-order preserved). `dispatchCursorChange(Cursor, Editor*, File*)` iterates vector, returns the first `onTrigger` that returns non-null `EditorSuggestTriggerInfo`. **No priority sorting** (compat invariant).
10. Port built-in suggesters into the new framework: wiki-link (`[[`) and tag (`#`) suggesters currently hard-coded in `CompletionPopup`/`NoteEditorWidget`. Refactor into `EditorSuggest` subclasses registered at app start.
11. Implement `Corbomite::SuggestPopup`. `QListView` delegate that calls `EditorSuggest::renderSuggestion(value, QWidget*)` for each row. Keyboard nav, Enter → `selectSuggestion(value, evt)`. Positions at editor cursor via `Markoff::Editor::cursorScreenRect()`.
12. Delete or subsume `CompletionPopup` — the refactored built-in suggesters + `SuggestPopup` replace it functionally.

**Phase 4 — Ribbon slot:**
13. Implement `Corbomite::RibbonSlot`. Wraps `KToolBar` docked left, `Qt::Vertical` orientation, `Qt::ToolButtonIconOnly`. `addRibbonIcon(QIcon, QString title, std::function onActivated) → Handle`. **Keys on `title`** (same-title collision = Obsidian-compat quirk, preserve with documented rationale).
14. Initial ribbon population: "New note", "Open quick switcher", "Open graph view" (match Obsidian's default ribbon).

**Phase 5 — Notice:**
15. Implement `Corbomite::Notice`. Based on `KMessageWidget` if the API fits; else custom `QFrame` toast at bottom-right. Auto-dismiss after 4s (Obsidian default — confirm against source). `Notice::setAction(QString label, std::function callback)` for notices with undo/retry buttons.

**Phase 6 — End-to-end:**
16. Manual walkthrough: type `[[` → suggester opens and fuzzy-ranks notes via Cluster D's matcher → Enter inserts link. Hover a `[[link]]` → popover preview appears. Right-click a tab → `tabGroupMenu` signal fires (with no plugins, menu just shows built-in items). Click ribbon "New note" icon → new note created.

## Explore-agent dispatch prompts

**Prompt 1 — KDevelop hover-tooltip architecture:**
> Read KDevelop's declaration-navigation tooltip code at `~/src/kde/src/kdevelop/kdevplatform/language/duchain/navigation/`. Do NOT clone from upstream — local source is current. Identify: (a) the widget used for rich-content tooltips (is it a subclass of `QFrame`, `QToolTip`, or custom?), (b) the show/hide timing and mouse-tracking logic, (c) how rich content (syntax-highlighted code, links, rendered markup) is embedded into the tooltip, (d) the lifecycle boundary — when is the tooltip constructed/destroyed, what owns it. Report a translation plan for `Corbomite::HoverPopover` that uses `Markoff::ReadingView` as the content-renderer. Under 700 words.

**Prompt 2 — KDevelop completion popup delegate:**
> Read KDevelop's code-completion popup at `~/src/kde/src/kdevelop/kdevplatform/language/codecompletion/`. Identify: (a) the popup widget (likely `QListView` or custom), (b) how per-row rendering is delegated (categories, icons, rich HTML), (c) keyboard navigation and selection commit, (d) how the popup coordinates with the editor's cursor position to stay anchored. Report whether this is transferable to `Corbomite::SuggestPopup` or whether we should build atop plain `QListView` + `QAbstractItemDelegate` instead. Under 600 words.

**Prompt 3 — KMessageWidget as Notice backing:**
> Read `~/src/kde/src/kwidgetsaddons/src/kmessagewidget.{h,cpp}` and any existing usage in `~/src/kde/src/kate/` or `~/src/kde/src/kdevelop/`. Evaluate whether `KMessageWidget` can back `Corbomite::Notice` directly or whether we need a custom transient-toast widget. Report: (a) auto-dismiss support, (b) stacking behaviour if multiple notices fire in quick succession, (c) position/anchor behaviour (KMessageWidget is typically embedded in a layout; Obsidian's Notice is floating). Under 500 words. Recommend one approach.

**Prompt 4 — Menu section real-world validation:**
> Grep `~/src/kde/src/kate/` and `~/src/kde/src/ktexteditor/` for right-click context-menu construction. Identify: (a) the typical pattern (one `QMenu`, actions added directly? `KActionCollection`-backed?), (b) whether any existing KDE code uses a similar "named sections with separators between" protocol, (c) whether our `MenuSectionHelper` wrapper is a novel abstraction or whether we can lean on an existing KDE class. Under 500 words.

## Definition of done

- `MenuSectionHelper` produces sorted multi-section `QMenu`s with separators; `MenuEventEmitter` emits on every documented menu-construction surface.
- `HoverLinkSourceRegistry` has built-in entries for every view type Corbomite ships; `HoverPopover` shows after 300ms, dismisses correctly.
- `EditorSuggestManager` dispatches cursor events to insertion-order-first-win suggester; built-in wiki-link + tag suggesters ported into the new framework; `SuggestPopup` replaces `CompletionPopup`.
- `RibbonSlot` populated with initial built-in icons; `addRibbonIcon` keys on title per compat.
- `Notice` widget works with auto-dismiss + optional action button.
- Plugin-side surfaces (registration verbs) don't exist yet (Cluster N) but the registries are ready to accept them.

## Blocks / enables

- **Depends on:** Cluster C Phase 1 (`Component` lifecycle — every `EditorSuggest` is a Component; every popup is a Component; every menu-helper uses registerEvent for cleanup), Cluster E (section-pipeline consumed by `HoverPopover`'s embedded `Markoff::ReadingView` for preview-mini-render), Cluster I (MetadataCache consumed by suggesters for note/tag completion), Cluster D (fuzzy matcher consumed by suggester ranking).
- **Blocks:** Cluster N (plugin-ready surfaces) — the registries shipped here are what plugins will eventually populate.
- **Parallelisable with:** Cluster A, Cluster B, Cluster D — H is a UI-branch cluster; they're data-branch. Work from A/B/D is consumed by H but H's own implementation can proceed in parallel with their middle phases.
- **Estimated effort:** 3–4 weeks. Phase 2 (HoverPopover + HoverLinkSource integration) and Phase 3 (EditorSuggest refactor) are the biggest sub-projects; Phase 1 is mechanical; Phases 4–5 are small.

## Preserved Obsidian compat quirks

- `addRibbonIcon` keys on `title` — same-title collision is silent. Preserve.
- `EditorSuggestManager` uses **first-non-null-wins insertion order**; no priority system. Built-ins always shadow plugin overrides of `[[`/`#` unless a plugin registers before the built-in (impossible today, future plugin-load-order API may address it).
- HoverPopover hover delay is exactly 300ms (not 500ms — that's the registry poll). Preserve the constant.
- Menu section IDs are the canonical set (`title`, `open`, `action-primary`, `action`, `info`, `info.copy`, `view`, `system`, `""`, `danger`). Unknown section IDs go to the `""` (unset) bucket.
- `Notice` auto-dismiss duration: confirm against source during Phase 5 before coding. Likely 4s but not yet verified.

## Open questions

1. Does Obsidian use `hover-link` as a *signal fanned across all views* or per-view subscriptions? Workspace owns the registry but the audit doesn't clarify dispatch. Grep `on('hover-link'` in the source tree during Phase 2 planning.
2. `Notice` auto-dismiss duration — audit said "timing" but didn't name the number. Resolve during Phase 5 sub-task 15 prep.
3. Plugin-supplied HTML in `Notice` / `HoverPopover` content goes through DOMPurify in Obsidian. **Blocked on controller follow-up #1** (extract DOMPurify allowlist from `_internal.js`). H can ship with a conservative hardcoded allowlist for now and upgrade when the extraction lands; note in the Recent-decisions log.
