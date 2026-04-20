# Cluster G — Views hierarchy + TextFileView contract (SCOUTING)

> **Living-status note:** This is a *scouting document*, not a plan. It captures prior-art breadcrumbs + open architectural questions so a full plan can be written efficiently when Cluster G is unblocked. Live status is in [`docs/PROJECT-STATE.md`](../../PROJECT-STATE.md). When expanded to a full plan, rename to drop `-SCOUTING` suffix and update `INDEX.md`.

**Scouting written:** 2026-04-14.
**Expand to full plan when:** Cluster C Phase 1 lands (the `Component` base class needs concrete signature) and Cluster B Phase 3 lands (WorkspaceState integration shape for view serialisation).

**Covers (deferred to full plan):** P2.1 (ViewRegistry refactor), P2.2 (View/ItemView/FileView/TextFileView hierarchy), P2.3 (three-way merge on external modify), P2.4 (save-failure backup), P2.26 (leaf-close undo, cap 10), P2.27 (per-leaf history, cap 20), P2.28 (popout windows), P2.29 (stacked tabs mode), P2.30 (tab pin + linked-pane group).

## Why deferred

The full plan's target-class signatures depend on the finished `Corbomite::Component` base (from Cluster C). Writing it now means every "extends Component" assertion is a forward reference whose shape might shift. Writing *after* C Phase 1 lets us cite the actual API. Also, G is large — popout windows and stacked tabs alone are 1–2 weeks each — so premature planning risks underscoping.

## Audit input roster (when expanding)

- `domains/views.md` §1 — View / ItemView / FileView / TextFileView hierarchy, required overrides (`getViewType`, `getDisplayText`, `getIcon`, `onOpen`, `onClose`, `getViewData`, `setViewData`, `clear`).
- `domains/views.md` §7 — `ViewRegistry` full contract + built-in view registration table (md→MarkdownView, image exts, audio exts, video exts, pdf, canvas, base).
- `domains/views.md` §12 — Deferred-load `eD` stub pattern (biggest perf win; non-trivial).
- `domains/views.md` §1 — TextFileView debounced save (2000ms) + three-way-merge on external modify (`FX` diff-match-patch wrapper) + save-failure backup via `file-recovery`.
- `domains/workspace.md` §3 — workspace.json SplitNode tree (leaves within tabs within splits). Tab pin, linked-pane group flags.
- `domains/workspace.md` §1 — leaf-close undo (`undoHistory` cap 10) + per-leaf back/forward (`qD` cap 20).
- `domains/workspace.md` — popout windows (`WorkspaceWindow` variant), stacked tabs (`WorkspaceTabs` mode).
- `01-markoff-gaps.md` Pass 2 additions (views) — three-way-merge and backup-on-save-failure signals.

## Prior-art breadcrumbs (local paths — do NOT clone)

| Target | Local path | Note |
|---|---|---|
| **Multi-pane split layout with per-pane view types** | `~/src/kde/src/kdevelop/kdevplatform/sublime/` — specifically `mainwindow.cpp`, `areaindex.cpp`, `areaoperation.cpp` | KDevelop's `Sublime::Area` is the closest match for Obsidian's Workspace+Split+Leaf tree. Start here. |
| **Per-leaf document controller + tab history** | `~/src/kde/src/kdevelop/kdevplatform/shell/documentcontroller.cpp` | Per-pane open-document management + history stack |
| **Popout/detach window** | `~/src/kde/src/kate/apps/lib/` — search for `mainwindow` detach paths; `~/src/kde/src/kdevelop/kdevplatform/sublime/idealcontroller.cpp` | Both Kate and KDevelop support detaching tabs to new windows |
| **Stacked tabs** | `~/src/kde/src/kate/apps/` — tab-bar code in `lib/katesession.cpp` neighbourhood | Kate has tab-pin + middle-click-close; verify stacked-tabs support |
| **Three-way merge (diff-match-patch)** | External: Google `diff-match-patch` (Apache-2) C++ port, or `~/src/kde/src/` — search for `diff_match_patch` and `KDiff3` project | Obsidian uses diff-match-patch specifically; match behaviour |
| **Save-failure recovery / backup** | `~/src/kde/src/ktexteditor/src/document/katedocument.cpp` — `writeFile()` error-path + swap-file logic | Kate has the most mature Qt-side backup pattern |
| **`eD` deferred-load stub** | No direct KDE analogue — Qt's `QAbstractItemView` lazy-loading is conceptually similar but scope differs | Likely build fresh; document the {icon, title} cache contract |
| **ViewRegistry registry pattern** | `~/src/kde/src/kparts/` — `KParts::Part` factory registration | Conceptually similar; validate whether KParts' factory mechanism fits or if fresh build is cleaner |
| **KateSession pattern for named-workspace switch** | `~/src/kde/src/kate/apps/lib/session/` | Already referenced by Cluster C for vault-switch. Workspaces feature piggybacks on the same pattern |

## Key architectural questions to resolve during planning

1. **Does `Corbomite::View` subclass `Corbomite::Component`** (per Obsidian) or **extend `QWidget` directly** (KDE-native)? Cross-cutting — if Component, then C must land first. Likely answer: Component (for plugin-compat) but with `QWidget` composition via a `QWidget *viewWidget()` accessor.
2. **Does `WorkspaceSplit` use `QSplitter`?** Probably yes — but Obsidian's splits nest arbitrarily with type discriminants. Test `QSplitter` with 3+ levels of nesting + drag handles before committing.
3. **How does `eD` deferred-load translate?** Obsidian caches `{icon, title}` in workspace.json; the stub widget paints chrome from the cache without instantiating the real view. Corbomite needs the same cache mechanism — the `{icon, title}` serialisation is in Cluster B's WorkspaceState. Confirm tie-in.
4. **Popout windows: `QMdiArea` or independent `QMainWindow` instances?** Obsidian's popout is a fully-independent `BrowserWindow`. `QMainWindow` is closer; `QMdiArea` is parented-MDI. Almost certainly independent QMainWindows.
5. **Three-way-merge cadence:** merge on `vault::modify` externally, fallback to reload-with-prompt if merge fails. Need concrete diff library choice (port `diff-match-patch` vs `KDiff3`).

## Rough phasing (for planning, not prescriptive)

- Phase 1: `View` + `FileView` + `ItemView` + `TextFileView` hierarchy (subclass `Component`).
- Phase 2: `ViewRegistry` with built-in registrations; replace `EditorViewSpace` ad-hoc extension branching.
- Phase 3: `WorkspaceLeaf` + `WorkspaceSplit` + `WorkspaceTabs` container hierarchy.
- Phase 4: `TextFileView` debounced save + three-way merge + save-failure backup.
- Phase 5: deferred-load `eD` stub pattern.
- Phase 6: popout windows.
- Phase 7: stacked tabs.
- Phase 8: tab pin + linked-pane group.
- Phase 9: leaf-close undo + per-leaf back/forward history.

## When to expand

Trigger: **Cluster C's Phase 1 commit lands** (visible via PROJECT-STATE roadmap status change + a Recent-decisions entry). At that point, Ritual 3 Step 5 says "re-evaluate STUB plans." This scouting doc becomes the seed for a full plan.

Expansion effort estimate: ~90 minutes. Most content is already here; needs the full 15-section template applied + Explore-agent prompts for each of the 9 phases.
