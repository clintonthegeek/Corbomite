# Cluster R — View-header menus (Design Spec)

**Date:** 2026-04-19
**Scope:** Per-leaf hamburger menu (the "…" overflow in the view header) for all view types, aligned with Obsidian's `onMoreOptionsMenu` contract and canonical section ordering. Wires up the broadly-easy/medium items uncovered by the 2026-04-19 hamburger-menu audit: file-menu universals on `EditableFileView`, per-view specialisations on `MarkdownView` / `CanvasFileView` / `GraphView`, and inline "Backlinks in document" rendering.
**Audit references:**
- `domains/views.md` §30-41 (`ItemView` header + `onMoreOptions` contract), §67-74 (`EditableFileView` + title handlers + Rename/Delete menu items), §73 ("Delete" as canonical `danger`-section entry).
- `domains/workspace.md` §316, §340, §342, §420 (`workspace:copy-path`, `-full-path`, `-url`; `obsidian://` protocol; `workspace:export-pdf`; injected menu items per view type).
- `domains/editor-markdown.md` §229 (Reading/Source toggle commands; `markdown:add-metadata-property`).
- `domains/ui-bundle.md` §113-235 (`Menu`/`MenuItem`/`MenuSeparator`), §330-389 (section registry + mid-construction emit pattern).
- 10 addenda at `docs/obsidian-audit/addenda/2026-04-19-*.md` (bookmarks plugin, rename/move/delete modals, open-with-default-app, show-in-folder, merge-file modal, canvas export-as-image, graph copy-screenshot, add-file-property menu, file-recovery plugin).

---

## 1. Goal

Replace the current stub hamburger (single unfunctional "Rename…" entry on `EditableFileView` at `libs/core/src/EditableFileView.cpp:33-34`; the `moreBtn` in `libs/core/src/ItemView.cpp:55-66`) with a full Obsidian-style per-leaf "…" menu that mirrors Obsidian's `onMoreOptions` contract, respects the canonical section order, and wires up every item that has an existing or trivially-addable primitive.

Explicitly **out of scope** (shipped elsewhere or deferred):
- Bookmarks plugin itself (Cluster S).
- WorkspaceWindow popout / "Open in new window" (Cluster G follow-up #6). R ships a **disabled menu slot**.
- In-editor Find/Replace (Qutepart fork Phase 3). R ships a **disabled menu slot**.
- `workspace.openLinkText()` dispatcher (Cluster G follow-up #3). R ships the "Open linked view" submenu but wires it to existing plugin panel-focus paths; plugin-as-leaf opening follows when dispatcher lands.
- File-recovery plugin / Version History modal (deferred post-parity "Cluster T"). R ships a **disabled menu slot**.
- "Merge entire file with…" (deferred; addendum written; out of R).

---

## 2. Current state (one-paragraph snapshot)

`Corbomite::ItemView` (libs/core) builds the hamburger as a `QToolButton` with `overflow-menu` icon at `ItemView.cpp:55-66` and dispatches on click to `showMoreOptionsMenu()` at `:120-127`, which calls `onMoreOptionsMenu(&menu)` then `onPaneMenu(&menu)` then execs at cursor. `View::onMoreOptionsMenu` doesn't exist — `ItemView::onMoreOptionsMenu` is a no-op at `:118`. `View::onPaneMenu` at `View.cpp:67` is empty; `EditableFileView::onPaneMenu` at `EditableFileView.cpp:29-36` adds the unfunctional `Rename…` item whose `startRename()` at `:38-44` is a stub (flips `m_renaming` true-then-false, no dialog shown). `MenuSectionHelper` at `libs/core/src/MenuSectionHelper.cpp` exists but has zero production callers (Cluster H shipped it; the `FileExplorerPanel` example reference got absorbed into the plugin migration). Its section order (`title / open / action-primary / action / info / info.copy / view / system / "" / danger`) diverges from Obsidian's canonical order (`close / pane / open / action / find / info / info.copy / view / view.linked / system / "" / danger`) — notably missing `find` and `view.linked`.

---

## 3. Key design decisions

### 3.1 Two hook pattern: `onMoreOptionsMenu` is the primary subclass entry point

Match Obsidian's split verbatim (`views.md §38`). `View::onMoreOptionsMenu(MenuSectionHelper&)` becomes a new virtual hook, defaulting to no-op. `View::onPaneMenu(QMenu*, const QString &source)` retains its existing role for *tab-header* and *right-click* menus but grows a `source` parameter (defaulting to `"pane-menu"`) so subclasses can differentiate hamburger-triggered invocation from tab-header invocation where that matters (it does for the `file-menu` emit).

`ItemView::showMoreOptionsMenu()` is rewritten:

```cpp
void ItemView::showMoreOptionsMenu()
{
    QMenu menu(this);
    MenuSectionHelper helper(&menu);

    // Subclass contributions (primary)
    onMoreOptionsMenu(helper);

    // Backward-compat: pane-menu items also participate
    onPaneMenu(&menu, QStringLiteral("more-options"));

    // Plugin hook: leaf-menu emission (Cluster H's MenuEventEmitter)
    if (m_leaf && m_leaf->menuEventEmitter())
        m_leaf->menuEventEmitter()->emitLeafMenu(&menu, m_leaf);

    helper.finalize();
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}
```

Rationale: subclasses that want structured section placement use `onMoreOptionsMenu(helper)`. Subclasses that have existing `onPaneMenu` logic keep working unchanged. Plugins intercept via `MenuEventEmitter::fileMenu` / `leafMenu` as today.

### 3.2 `MenuSectionHelper` aligns to Obsidian's canonical order

Update `canonicalSectionOrder()` to:

```
close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger
```

Breaking renames: `title` → `close`, `action-primary` → `pane`. New sections: `find`, `view.linked`. The test fixture at `tests/core/tst_menusectionhelper.cpp` is updated to exercise the new keys. No other production callers exist.

Submenu support: new method

```cpp
MenuSectionHelper addSubmenu(const QString &sectionId,
                              const QString &title,
                              const QIcon &icon = {});
```

Returns a nested helper whose actions are grouped into a `QMenu` inserted as a submenu at the outer helper's finalize time. Necessary for `info.copy` (Copy path → 3 variants) and `view.linked` (Open linked view → 5 options).

### 3.3 `EditableFileView::onMoreOptionsMenu` owns all file-menu universals

Every item that's available for any `FileView`-derived view (Markdown, Canvas, future Image/PDF) goes here. Subclasses override to **prepend** view-specific items via their own `onMoreOptionsMenu` (which is called from `ItemView::showMoreOptionsMenu` — subclass chains to base via `EditableFileView::onMoreOptionsMenu(helper)`).

### 3.4 Disabled-placeholder items for unbuilt subsystems

Four menu entries have audit-grade UX specs but ship as disabled QActions in R so the menu structure visually matches Obsidian:

- "Open in new window" (Cluster G follow-up #6).
- "Find…" / "Replace…" (Qutepart fork Phase 3).
- "Open version history" (deferred Cluster T).
- "Bookmark…" (Cluster S — once S ships, R's menu item switches from disabled-placeholder to live dispatch via the `bookmarks:bookmark-current-file` command).

Each disabled action has a tooltip explaining why it's disabled and which cluster unblocks it.

### 3.5 Backlinks-in-document lives in the Backlinks plugin, not core

The inline "Backlinks in document" rendering (addendum-referenced; Obsidian appends a backlinks section to the rendered Markdown Reading view) is a `PostProcessorRegistry` contribution (Cluster J substrate). It lives in `src/plugins/backlinks/` as a new registration, gated on per-view state `backlinksInDocument: true` persisted in the leaf's viewState. `libs/core/` stays plugin-free for this feature.

### 3.6 "Open linked view" submenu dispatches to existing plugin panels for now

Pending Cluster G follow-up #3 (`workspace.openLinkText` dispatcher), the five submenu entries:

| Label | Dispatches to (interim) | Final form (post-follow-up) |
|---|---|---|
| Open local graph | `graph:open-local` command | Opens graph view as new leaf linked to file |
| Open backlinks | `backlinks:open` command (focuses panel, scrolls to active-file entry) | Opens Backlinks as new leaf linked to file |
| Open outgoing links | `outlinks:open` | Opens Outlinks as new leaf |
| Open file properties | `properties:open` | Opens Properties as new leaf |
| Open outline | `outline:open` | Opens Outline as new leaf |

Interim behaviour is correct-enough for UX; the dispatcher follow-up upgrades to true leaf-opening without menu changes.

### 3.7 Menu item icons use lucide-equivalent theme icons

Obsidian uses lucide icons (`lucide-edit-3`, `lucide-trash-2`, `lucide-folder-open`, etc.). Corbomite maps via `QIcon::fromTheme()` to the closest KDE theme icon (`edit-rename`, `edit-delete-shred`, `document-open-folder`, etc.). Mapping table in §6.2.

---

## 4. Architecture

### 4.1 Class contract delta

```
View (existing)
  + virtual void onMoreOptionsMenu(MenuSectionHelper &helper)   // new, default no-op
  - virtual void onPaneMenu(QMenu *menu)                         // existing
  + virtual void onPaneMenu(QMenu *menu, const QString &source) // grows source param

ItemView (existing)
  - void showMoreOptionsMenu()                                  // rewritten per §3.1

EditableFileView (existing)
  + void onMoreOptionsMenu(MenuSectionHelper &helper) override  // universal file-menu items

MarkdownView (existing)
  + void onMoreOptionsMenu(MenuSectionHelper &helper) override  // prepends md-specific,
                                                                 // then calls EditableFileView::onMoreOptionsMenu

CanvasFileView (existing)
  + void onMoreOptionsMenu(MenuSectionHelper &helper) override  // prepends canvas-specific

GraphView (existing)
  + void onMoreOptionsMenu(MenuSectionHelper &helper) override  // graph-specific only (not FileView)
```

### 4.2 Section assignment rulebook

| Section | Contents |
|---|---|
| `close` | (Reserved for `onTabMenu`, not hamburger — empty here.) |
| `pane` | Split right / Split down / Open in new window (disabled) |
| `open` | (Reserved for "Open as ..." actions — empty in MVP.) |
| `action` | Rename… / Move file to… / Bookmark… / Merge entire file (deferred; omit) / Add file property / Export to PDF / Canvas: Export as image / Graph: Copy screenshot |
| `find` | Find… / Replace… (both disabled) |
| `info` | (Reserved for "View source" etc. — empty in MVP.) |
| `info.copy` | Copy path → submenu(as Obsidian URL, from vault folder, from system root) |
| `view` | Reading View / Source mode (markdown); Backlinks in document toggle (markdown) |
| `view.linked` | Open linked view → submenu(local graph, backlinks, outgoing links, file properties, outline); Open version history (disabled) |
| `system` | Open in default app / Show in system explorer / Reveal file in Navigation |
| `""` (unset) | plugin-contributed items with unknown section |
| `danger` | Delete file |

### 4.3 File structure

```
libs/core/
  include/corbomite/core/
    View.h                          (mod: add onMoreOptionsMenu virtual + onPaneMenu source param)
    MenuSectionHelper.h             (mod: canonical order + addSubmenu)
  src/
    View.cpp                        (mod)
    ItemView.cpp                    (mod: rewrite showMoreOptionsMenu)
    MenuSectionHelper.cpp           (mod: canonical order + addSubmenu impl)
    EditableFileView.cpp            (mod: full onMoreOptionsMenu impl; delete stub startRename body)
  src/proxies/
    (no changes in R; plugin-facing proxies stay as-is)
  tests/
    tst_menusectionhelper.cpp       (mod: update section keys; add submenu coverage)
    tst_view_more_options.cpp       (new: unit tests for View::onMoreOptionsMenu dispatch)

libs/vault/
  include/corbomite/vault/FileManager.h
    (mod: add promptForFileRename, promptForMove, promptForDeletion)
  src/FileManager.cpp
    (mod)
  tests/
    tst_file_manager_prompts.cpp    (new)

libs/core/ (platform primitives)
  include/corbomite/core/Platform.h  (new header)
  src/Platform.cpp                   (new: openWithDefaultApp, showInFolder)
  tests/
    tst_platform.cpp                (new — uses QProcess mocks or DBus stubs)

src/editor/
  MarkdownView.h/.cpp               (mod: onMoreOptionsMenu impl; insertFrontmatterProperty;
                                     exportToPdf helper)

src/canvas/
  CanvasFileView.h/.cpp             (mod: onMoreOptionsMenu impl; calls CanvasScene::renderToImage)

libs/canvas/
  include/canvas/CanvasScene.h      (mod: add renderToImage/renderToSvg)
  src/CanvasScene.cpp               (mod)

src/plugins/graph-view/
  GraphView.h/.cpp                  (mod: onMoreOptionsMenu impl; copy-screenshot command)
  CommandRegistrar wiring           (mod: register graph:copy-screenshot)

src/plugins/backlinks/
  BacklinksPlugin.cpp               (mod: register BacklinksPostProcessor for in-document render)
  BacklinksPostProcessor.h/.cpp     (new)

src/plugins/<each panel>/
  (mod: register their :open command for view.linked submenu dispatch)
```

---

## 5. Phases

### Phase 1 — Menu substrate alignment

**Goal:** hook + helper + section order ready, tested, with zero new user-visible menu items.

Tasks:
1. Add `View::onMoreOptionsMenu(MenuSectionHelper &helper)` virtual, default empty.
2. Grow `View::onPaneMenu` second overload with `source` param; keep old overload as a shim calling the new one with `"pane-menu"`.
3. Update `MenuSectionHelper::canonicalSectionOrder()` to full Obsidian order: `close, pane, open, action, find, info, info.copy, view, view.linked, system, "", danger`.
4. Add `MenuSectionHelper::addSubmenu(sectionId, title, icon)` returning a nested helper.
5. Rewrite `ItemView::showMoreOptionsMenu()` per §3.1.
6. Update `tests/core/tst_menusectionhelper.cpp` to new section keys; add submenu test cases.
7. Add `tests/core/tst_view_more_options.cpp` — verifies dispatch order: `onMoreOptionsMenu` first, then `onPaneMenu(menu, "more-options")`, then `MenuEventEmitter::leafMenu` (if any), then finalize.

**Definition of done:** All existing tests pass; two new test files green; hamburger still shows "Rename…" (unchanged UX) but via the new hook path.

### Phase 2 — Universal file-menu items on `EditableFileView`

**Goal:** every item specced in addenda 1-10 for any `.md` or `.canvas` file wires through a real primitive. Unbuilt items ship as disabled placeholders.

Tasks:
1. `FileManager::promptForFileRename(TFile*, QWidget*)` — modal per addendum `2026-04-19-rename-move-modals.md §1`. Returns the new path or empty QString on cancel.
2. `FileManager::promptForMove(TFile*, QWidget*)` — folder-picker modal per same addendum §2. Returns new path or empty.
3. `FileManager::promptForDeletion(TAbstractFile*, QWidget*)` — confirm modal per addendum `2026-04-19-delete-confirm-modal.md`. Returns bool (confirmed + trashed). Skips modal if `vault->getConfig("promptDelete", true) == false`.
4. `Corbomite::Platform::openWithDefaultApp(const QString &absolutePath)` via `QDesktopServices::openUrl(QUrl::fromLocalFile(...))`. Returns bool. On failure shows Notice.
5. `Corbomite::Platform::showInFolder(const QString &absolutePath)` — DBus FileManager1 → xdg-open fallback on Linux; native paths on mac/Win. See addendum `2026-04-19-show-in-folder.md §4`.
6. `Corbomite::PathUtils::obsidianUrlFor(vaultName, relativePath, subpath=QString())` — emits `obsidian://open?vault=...&file=...` URI.
7. `EditableFileView::onMoreOptionsMenu(MenuSectionHelper &helper)` — items common to every file-view (markdown, canvas, future image/PDF):
   - `action`: Rename… → `FileManager::promptForFileRename`.
   - `action`: Move file to… → `promptForMove`.
   - `info.copy` submenu: "as Obsidian URL" / "from vault folder" / "from system root".
   - `system`: Open in default app → `Platform::openWithDefaultApp(fullPath)`.
   - `system`: Show in system explorer → `Platform::showInFolder(fullPath)`.
   - `system`: Reveal file in Navigation → dispatches `file-explorer:reveal-file` command.
   - `danger`: Delete → `FileManager::promptForDeletion`.
   - `pane`: Open in new window (disabled placeholder).
   - `view.linked`: Open version history (disabled placeholder).
8. Delete the stub `EditableFileView::startRename()` body; `onPaneMenu` still contributes "Rename…" to tab-header context menus — route it through `promptForFileRename`.
9. Add FileExplorer plugin command `file-explorer:reveal-file` that scrolls + selects the file in the panel's QTreeView.

**Items deliberately NOT on `EditableFileView`** (per Obsidian's per-view-type menus; these go on the specific subclass in P3):
- Bookmark… — markdown + canvas + graph only (not all FileViews; future PDF/image views don't bookmark in Obsidian).
- Export to PDF — markdown only (Reading-view-based pipeline).
- Find… / Replace… — markdown only (editor-based).
- Add file property — markdown only (requires frontmatter editor).
- Reading View / Source mode / Split right / Split down — markdown + canvas + graph, but the concrete dispatch differs per view; per-subclass in P3.
- "Open linked view" submenu — markdown gets 5 entries, canvas gets only Backlinks, graph gets none.

**Definition of done:** every menu entry above fires its wired primitive; every disabled placeholder renders greyed-out with tooltip; unit tests for each new primitive (promptForFileRename, promptForMove, promptForDeletion, openWithDefaultApp, showInFolder, obsidianUrlFor).

### Phase 3 — Per-view specialisations

**Goal:** Markdown, Canvas, Graph each prepend their view-specific items to the universal set from P2.

Tasks:
1. `MarkdownView::onMoreOptionsMenu(helper)`:
   - `pane`: Split right → dispatches existing `split_right` action.
   - `pane`: Split down → `split_down`.
   - `view`: Backlinks in document → checkable toggle, persists in viewState as `backlinksInDocument: true`. On toggle, emits `viewChanged` so the BacklinksPostProcessor re-runs.
   - `view`: Reading View → checkable; dispatches `markdown:toggle-preview`.
   - `view`: Source mode → checkable; dispatches `editor:toggle-source`.
   - `action`: Bookmark… → dispatches `bookmarks:bookmark-current-file` if plugin enabled, else disabled placeholder.
   - `action`: Add file property → `insertFrontmatterProperty()` (new method per addendum `2026-04-19-add-file-property-menu.md §5`).
   - `action`: Export to PDF → `ExportToPdf::exportFile(m_file)` (new helper).
   - `find`: Find… / Replace… (disabled placeholders; activate when Qutepart fork Phase 3 ships).
   - `view.linked` submenu: 5 entries dispatching to the 5 plugin commands per §3.6 (local graph, backlinks, outgoing links, file properties, outline).
   - Calls `EditableFileView::onMoreOptionsMenu(helper)` to pick up universal items (Rename/Move/CopyPath/OpenDefault/ShowInFolder/Reveal/Delete/OpenInNewWindow-placeholder/OpenVersionHistory-placeholder).
2. `CanvasFileView::onMoreOptionsMenu(helper)`:
   - `pane`: Split right / Split down.
   - `action`: Bookmark… → same Bookmark plugin gating as MarkdownView.
   - `action`: Export as image → `showExportAsImageModal()` (new method; invokes the 3-option dialog per addendum `2026-04-19-canvas-export-as-image.md §2`).
   - `view.linked` submenu: **single entry** "Open backlinks" → `backlinks:open`. (Canvas doesn't have outgoing links / outline / properties / local-graph in Obsidian's menu.)
   - Calls `EditableFileView::onMoreOptionsMenu(helper)` for universal items.
3. `GraphView::onMoreOptionsMenu(helper)`:
   - `pane`: Split right / Split down.
   - `action`: Copy screenshot → `QWidget::grab().toImage()` → clipboard + Notice. See addendum `2026-04-19-graph-screenshot.md §4`.
   - `action`: Bookmark… → same gating (`bookmarks:bookmark-current-graph` when plugin enabled).
   - **Does not** call `EditableFileView::onMoreOptionsMenu` — GraphView is an `ItemView`, not a `FileView`. Graph's hamburger is deliberately short.
4. New on `CanvasScene` (libs/canvas): `QImage renderToImage(const QRectF &bounds, bool transparentBg, bool showEdges, qreal scale)`; `void renderToSvg(const QRectF &bounds, QIODevice *out, bool transparentBg, bool showEdges)`.
5. New on `MarkdownView`: `void insertFrontmatterProperty()` per addendum §5.
6. New `ExportToPdf::exportFile(TFile*, QWidget*)` helper — `QPrinter` + `ReadingView::renderToPdf(printer)` pipeline. ReadingView already renders; new helper is ~50 LOC.
7. Graph plugin: register `graph:copy-screenshot` command.
8. Wire `markdown:add-metadata-property` command (Cluster C substrate) alongside the menu item for keyboard users.

**Definition of done:** all three view types show correct hamburger menus per the user's original message; copy-screenshot writes a valid PNG to clipboard; canvas export dialog writes valid PNG/SVG; backlinks-in-document toggle persists across leaf detach/reopen.

### Phase 4 — Inline Backlinks-in-document renderer

**Goal:** when `backlinksInDocument` is true on a MarkdownView leaf, the ReadingView (and LivePreview) renders an auto-appended backlinks section at the end of the note body.

Tasks:
1. New `BacklinksPostProcessor` in `src/plugins/backlinks/`. Implements the `PostProcessor` interface (Cluster J substrate at `libs/core/include/corbomite/core/PostProcessorRegistry.h`).
2. `process(MarkdownRenderChild *child, QWidget *container, MarkdownPostProcessorCtx *ctx)`: if `ctx->viewState["backlinksInDocument"].toBool()`, walk `MetadataCache::backlinksFor(ctx->sourcePath)`, render via the plugin's `Markoff::LinkRenderer` into a new `QWidget` appended to `container`.
3. Styling: a horizontal rule separator, an `<h3>Backlinks</h3>`, and a `<ul>` with one `<li>` per source file. Each list item is a wikilink-rendered clickable entry.
4. Re-run trigger: subscribe to `MetadataCache::cacheChanged` filtered to `ctx->sourcePath` backlinks — when changed, ask the render host to re-process this node.
5. Backlinks plugin `onLoad` registers the post-processor; `onUnload` unregisters.
6. Gate: disabled when plugin disabled. The menu toggle in P3 hides when plugin disabled.

**Definition of done:** toggling the menu item shows/hides the backlinks region; the region updates live when another note adds/removes a link to this file; toggle state persists across session reload.

---

## 6. Detailed contracts

### 6.1 Per-view menu inventory (verification table)

This is the ground-truth table R targets. Each row = one Obsidian menu entry the user listed + its Cluster R shipping state.

**Markdown notes:**

| Obsidian entry | Section | R ships as | Primitive | Source |
|---|---|---|---|---|
| Backlinks in document | `view` | P3 toggle + P4 renderer | viewState[backlinksInDocument] + BacklinksPostProcessor | addenda bookmarks §N/A + J substrate |
| Reading View | `view` | P3 checkable dispatch | `markdown:toggle-preview` | editor-markdown.md §229 |
| Source mode | `view` | P3 checkable dispatch | `editor:toggle-source` | editor-markdown.md §229 |
| Split right | `pane` | P3 | `Workspace::duplicateLeaf(Qt::Horizontal)` | existing MainWindow.cpp:789 |
| Split down | `pane` | P3 | `Workspace::duplicateLeaf(Qt::Vertical)` | existing MainWindow.cpp:798 |
| Open in new window | `pane` | disabled placeholder | (Cluster G follow-up #6) | workspace.md §99-102 |
| Rename… | `action` | P2 | `FileManager::promptForFileRename` | rename-move-modals addendum §1 |
| Move file to… | `action` | P2 | `FileManager::promptForMove` | rename-move-modals addendum §2 |
| Bookmark… | `action` | P3 (MarkdownView; wire to plugin if loaded) | `bookmarks:bookmark-current-file` | bookmarks-core-plugin addendum |
| Merge entire file with… | `action` | **omitted** (deferred) | — | merge-file-modal addendum |
| Add file property | `action` | P3 (MarkdownView only) | `MarkdownView::insertFrontmatterProperty` | add-file-property-menu addendum |
| Export to PDF… | `action` | P3 (MarkdownView only) | `ExportToPdf::exportFile` | workspace.md §342 |
| Find… | `find` | P3 disabled placeholder (MarkdownView only) | (Qutepart fork P3) | ui-bundle.md section `find` |
| Replace… | `find` | P3 disabled placeholder (MarkdownView only) | (Qutepart fork P3) | — |
| Copy path → as Obsidian URL | `info.copy` submenu | P2 | `PathUtils::obsidianUrlFor` + clipboard | workspace.md §340, §420 |
| Copy path → from vault folder | `info.copy` submenu | P2 | `m_file->path()` + clipboard | — |
| Copy path → from system root | `info.copy` submenu | P2 | `Vault::basePath() + m_file->path()` + clipboard | — |
| Open version history | `view.linked` | disabled placeholder | (deferred Cluster T) | file-recovery-plugin addendum |
| Open linked view → local graph | `view.linked` submenu | P3 interim | `graph:open-local` | §3.6 |
| Open linked view → backlinks | `view.linked` submenu | P3 interim | `backlinks:open` | §3.6 |
| Open linked view → outgoing links | `view.linked` submenu | P3 interim | `outlinks:open` | §3.6 |
| Open linked view → file properties | `view.linked` submenu | P3 interim | `properties:open` | §3.6 |
| Open linked view → outline | `view.linked` submenu | P3 interim | `outline:open` | §3.6 |
| Open in default app | `system` | P2 | `Platform::openWithDefaultApp` | open-with-default-app addendum |
| Show in system explorer | `system` | P2 | `Platform::showInFolder` | show-in-folder addendum |
| Reveal file in Navigation | `system` | P2 | `file-explorer:reveal-file` | views.md §53 |
| Delete file | `danger` | P2 | `FileManager::promptForDeletion` | delete-confirm-modal addendum |

**Canvas (.canvas):**

| Obsidian entry | Section | R ships as | Primitive |
|---|---|---|---|
| Split right / Split down | `pane` | P3 | `Workspace::duplicateLeaf` |
| Open in new window | `pane` | disabled placeholder | — |
| Rename… / Move file to… | `action` | P2 (from EditableFileView base) | `promptForFileRename` / `promptForMove` |
| Bookmark… | `action` | P3 (CanvasFileView; wire to S) | `bookmarks:bookmark-current-file` |
| Export as image | `action` | P3 (CanvasFileView) | `CanvasScene::renderToImage`/`renderToSvg` |
| Copy path → 3 variants | `info.copy` submenu | P2 (from EditableFileView base) | — |
| Open version history | `view.linked` | P2 disabled placeholder (from EditableFileView base) | — |
| Open linked view → backlinks (canvas gets only this, not all 5) | `view.linked` submenu | P3 (CanvasFileView) | `backlinks:open` |
| Open in default app / Show in system explorer / Reveal file in Navigation | `system` | P2 (from EditableFileView base) | — |
| Delete file | `danger` | P2 (from EditableFileView base) | `promptForDeletion` |

**Graph view:**

| Obsidian entry | Section | R ships as | Primitive |
|---|---|---|---|
| Split right / Split down | `pane` | P3 | `Workspace::duplicateLeaf` |
| Copy screenshot | `action` | P3 | `QWidget::grab().toImage()` + clipboard |
| Bookmark… | `action` | P3 placeholder (wire to S when present) | `bookmarks:bookmark-current-graph` |

### 6.2 Lucide → KDE theme icon mapping

| Lucide | KDE theme |
|---|---|
| `lucide-more-vertical` | `overflow-menu` (or `application-menu`; already used) |
| `lucide-edit-3` | `edit-rename` |
| `lucide-folder-tree` | `folder-open` (for Move to...) |
| `lucide-bookmark` | `bookmark-new` |
| `lucide-list-plus` | `list-add` |
| `lucide-file-output` | `document-export` (Export to PDF) |
| `lucide-search` | `edit-find` |
| `lucide-replace` | `edit-find-replace` |
| `lucide-copy` | `edit-copy` |
| `lucide-history` | `chronometer` |
| `lucide-network` | `kgraphviewer` (Local graph) |
| `lucide-arrow-right-circle` | `go-next` (Outgoing links) |
| `lucide-list-ordered` | `format-list-ordered` (Outline) |
| `lucide-external-link` | `document-open` (Open in default app) |
| `lucide-folder-open` | `folder-open-outline` (Show in system explorer) |
| `lucide-focus` | `mark-location` (Reveal in Navigation) |
| `lucide-trash-2` | `edit-delete-shred` or `user-trash` |
| `lucide-camera` | `camera-photo` (Copy screenshot) |
| `lucide-image-down` | `image-x-generic` (Export as image) |

### 6.3 viewState keys introduced

- `backlinksInDocument: true/false` — per-leaf, persisted in workspace.json under the leaf's viewState. Default `false`.

No other persistent state is added by R.

### 6.4 Commands introduced / registered

| Command ID | Owner | Purpose |
|---|---|---|
| `file-explorer:reveal-file` | file-explorer plugin | scrolls + selects `{path}` in the panel's tree |
| `graph:copy-screenshot` | graph-view plugin | emits copy-screenshot signal handled by active GraphView |
| `backlinks:open` | backlinks plugin | focuses the Backlinks panel + scrolls to active-file section |
| `outlinks:open` | outlinks plugin | focuses the Outlinks panel |
| `outline:open` | outline plugin | focuses the Outline panel + scrolls to cursor position |
| `properties:open` | properties plugin | focuses the Properties panel |

`bookmarks:bookmark-current-file` / `-heading` / `-block` / `-graph` are Cluster S's responsibility; R only references them.

---

## 7. Tests

### 7.1 Unit tests

- `tst_menusectionhelper.cpp` (existing, modified): new canonical keys, submenu finalize.
- `tst_view_more_options.cpp` (new): `View::onMoreOptionsMenu` dispatch order, `onPaneMenu(source)` receives `"more-options"` from the hamburger path, MenuEventEmitter fires.
- `tst_file_manager_prompts.cpp` (new): `promptForFileRename` validation rules (empty/bad-char/reserved/collision), `promptForMove` folder routing, `promptForDeletion` respects `promptDelete` config.
- `tst_platform.cpp` (new): `openWithDefaultApp` returns true for existing path, false for non-existent; `showInFolder` DBus-success path (mock) and fallback path.
- `tst_canvas_export.cpp` (new): `CanvasScene::renderToImage` produces non-null QImage of expected dimensions; `renderToSvg` produces valid XML.
- `tst_backlinks_postprocessor.cpp` (new): toggle on → appended region present in rendered HTML; MetadataCache change triggers re-render.

### 7.2 Integration tests

- `tst_e2e_menus.cpp` (new): open a markdown note, click hamburger, verify all expected QActions present with correct section ordering; click "Rename…" → modal appears with basename pre-selected; click Save → file is renamed; assert MetadataCache link-rewrite fired.

### 7.3 Flakes / known-failing expected

- None introduced by R. Existing flakies (`tst_markoff_inline_math`, `tst_renderengine`, `tst_completion_popup`, `tst_benchmark_layout`) stay as-is.

---

## 8. Risks + mitigations

| Risk | Mitigation |
|---|---|
| Grown `View::onPaneMenu` signature breaks existing overrides | Keep old overload as inline forwarder; deprecation is non-breaking. |
| `MenuSectionHelper` section renames break out-of-tree plugins | None exist yet (pre-1.0 ABI per N). Document in `docs/plugin-development/CHANGELOG.md`. |
| `QDesktopServices::openUrl` on Linux fallbacks silently | Show Notice on false return; note in the addendum. |
| Backlinks-in-document re-renders thrash on MetadataCache changes | Debounce `cacheChanged` in the post-processor with same 50ms window used by Cluster K's QueryController. |
| `CanvasScene::render` captures unexpected scene items (e.g. viewport grid) | Pass a specific `source` QRectF + filter child items by a display-capability flag; existing Cluster M Canvas-audit follow-up territory if edge cases surface. |
| Disabled-placeholder items look like bugs to users | Tooltip on each disabled QAction explains why ("Requires Bookmarks plugin" / "Requires Source mode find/replace — see roadmap"). |
| "Open linked view" interim dispatch focuses a dock panel instead of opening a leaf | Documented as interim per §3.6; upgrades when Cluster G follow-up #3 lands, no menu changes needed. |

---

## 9. Open questions

1. **Reading View / Source mode menu UX shape.** Obsidian shows them as *two separate menu items* (not a radio group). Cluster R matches that. When exactly-one-of-three is active (Source / LivePreview / Reading), the two menu entries are both unchecked only when the third mode (LivePreview) is active — this is a three-state encoding. Is that what we want, or do we expose LivePreview explicitly as a third entry?
   - **Default choice in spec:** match Obsidian exactly (two entries, LivePreview is implicit).
2. **"Copy path → as Obsidian URL" generates `obsidian://open?vault=...&file=...` — Corbomite vault name.** Our vaults don't have a stable "name" concept the way Obsidian does (Obsidian vaults register with the app via `vaults.json`). We'd need to either (a) generate `corbomite://` URLs, (b) reuse the vault folder name as the Obsidian vault name, or (c) generate real `obsidian://` URLs assuming the user also has Obsidian installed with a matching vault name.
   - **Default choice in spec:** (b) — vault folder basename. Also emit a matching `corbomite://` variant as a second submenu entry for Corbomite-to-Corbomite round-trip. Defer cluster Q addendum for `registerObsidianProtocolHandler` (cluster-q plan §468) — that's about **receiving** URLs, not generating them.
3. **"Backlinks in document" for LivePreview mode.** Obsidian only shows it in Reading view (it's a post-processor; LivePreview doesn't run post-processors on the live-editing portion). Do we match?
   - **Default choice in spec:** match Obsidian. Menu toggle works in LivePreview/Source too (persists to viewState), but the rendered region only appears in Reading mode.

Flag if any defaults are wrong; otherwise they land as-spec'd.

---

## 10. Blocks / enables

**Blocks:**
- Cluster S (Bookmarks) — R's "Bookmark…" menu slot calls S's command. S can implement independently, but the menu slot goes live only after S ships.
- Cluster T (file-recovery) — R's "Open version history" placeholder activates when T ships.
- Cluster G follow-up #3 (`openLinkText` dispatcher) — R's "Open linked view" submenu upgrades from interim to final when #3 lands.
- Cluster G follow-up #6 (WorkspaceWindow popout) — R's "Open in new window" placeholder activates when #6 lands.
- Qutepart fork Phase 3 (find/replace API) — R's "Find…"/"Replace…" placeholders activate when Phase 3 lands.

**Enables:**
- Consistent plugin-contributed menu items. Plugins emitting to `MenuEventEmitter::leafMenu` land in the canonical section order for the first time in production.
- Cluster H follow-up #2 (migrate 5 menu construction sites) — R P1's substrate alignment is the prereq; the remaining 4 sites (CanvasScene, Markoff Editor, TextControl, CorbomiteMDI Sidebar) can migrate incrementally afterwards.

---

## 11. Preserved-compat quirks

- Obsidian's default Delete button in the delete-confirm modal is **Cancel** (not Delete). Match exactly.
- Rename modal selects the **basename** not the full name. Match exactly.
- "Backlinks in document" region is rendered in Reading mode only (not LivePreview). Match exactly per §9 Q3.
- `canonicalSectionOrder()` has an empty-string bucket `""` between `system` and `danger` where unknown sections funnel — match exactly.
- Canvas Export modal defaults to "Only selected nodes" when a selection is non-empty, otherwise "Full canvas" — match exactly.
- Graph Copy Screenshot has no resolution modal — captures at current viewport size at current DPR. Match exactly.

---

## 12. Estimated effort

- P1 (substrate): ~1 day (helper + hook + tests).
- P2 (universal file-menu items + primitives): ~2-3 days (modals + Platform primitives + Export-to-PDF + 9 menu entries).
- P3 (per-view specialisations): ~2 days (3 view overrides + canvas render primitives + graph screenshot).
- P4 (backlinks-in-document): ~1 day (post-processor + toggle integration).

Total: **~6-7 days of focused work.** Comparable to Cluster L (Properties panel, single-phase normal task) in scope.

---

## 13. Audit addenda cited

All at `docs/obsidian-audit/addenda/`:

1. `2026-04-19-bookmarks-core-plugin.md`
2. `2026-04-19-rename-move-modals.md`
3. `2026-04-19-delete-confirm-modal.md`
4. `2026-04-19-open-with-default-app.md`
5. `2026-04-19-show-in-folder.md`
6. `2026-04-19-merge-file-modal.md` (referenced as deferred)
7. `2026-04-19-canvas-export-as-image.md`
8. `2026-04-19-graph-screenshot.md`
9. `2026-04-19-add-file-property-menu.md`
10. `2026-04-19-file-recovery-plugin.md` (referenced as deferred)

Plus existing domain references at `domains/views.md`, `domains/workspace.md`, `domains/ui-bundle.md`, `domains/editor-markdown.md`.
