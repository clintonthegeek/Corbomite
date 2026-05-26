# Cluster D.4a — Bases Cell Interactivity — Design

**Date:** 2026-05-26
**Status:** Approved (design phase)
**Cluster:** D (Bases UI completion) — sub-project D.4a (first slice of D.4 "interactivity/export")
**Predecessors:** D.1 (backend correctness), D.2 (read-side rendering), D.3 (toolbar menus + drawer + inline edit) — all shipped 2026-05-25/26
**Library:** `libs/bases` (`Corbomite::Bases`)

## Goal

Make the interactive *content within* Bases cells live, matching Obsidian: clicking a wikilink navigates the note, clicking a checkbox toggles it, clicking a tag searches for it, clicking an external URL opens the browser — while clicking cell whitespace still selects (and double-click still edits as text). Add a right-click file context menu and drag-out-as-wikilink. This closes the "the widgets are inactive" gap a user hits the moment they open a populated `.base`.

## Scope decomposition note

D.4 ("interactivity/export") is too large for one spec. It is sliced into:
- **D.4a (this spec):** live cell interactivity — click-activation of link/checkbox/tag/URL content, right-click context menu, drag-out-as-wikilink.
- **D.4b (later):** export (CSV/TSV/Markdown/clipboard via `QMimeData`), "+ New" button.
- **D.4c (later):** per-view undo/redo (`QUndoStack`).
- Formula editor and structured filter builder remain separate, larger sub-projects (their own specs).

## Scope (in scope for D.4a)

1. **Widget-level hit-testing.** The activation target is the painted interactive element, not the whole cell. Clicking whitespace selects/edits; clicking the element activates it.
2. **Wikilink/file click → navigate.** Plain click navigates the base's *own* leaf to the target note (`WorkspaceLeaf::navigate`, history-aware — the leaf's Back button returns to the base). Ctrl/Cmd-click or middle-click opens the note in a new tab.
3. **Checkbox click → toggle** the boolean frontmatter value (rides the existing `setData` → `processFrontMatter` path). Replaces the boolean double-click editor.
4. **Tag click → vault search** for that tag (a tag cell may hold several tags; the clicked tag is searched).
5. **External URL click → open in browser** (`QDesktopServices::openUrl`).
6. **Right-click context menu.** On a wikilink cell or a row that resolves to a note: Open, Open in new tab, Copy as wikilink, Rename…, Delete. On a plain cell: Copy value.
7. **Drag-out-as-wikilink.** Dragging an entry row whose note resolves produces `[[wikilink]]` text (`text/plain`).

## Out of scope (deferred)

- **Hover-preview popover.** Blocked: `HoverPopover` is disabled app-wide pending its rewire against read-only Live (`MainWindow.cpp:380`, Markoff dependency). Ships when that unfreezes.
- Export, "+ New", undo/redo, formula editor, filter builder (other D.4 slices).
- Tag-click semantics beyond "search for the tag" (e.g. tag autocomplete, tag pages).
- Creating a missing note on link click (link resolves → navigate/open; unresolved → no-op for now).

## Architecture

The defining constraint (from the design discussion): **the delegate owns both painting and hit-testing the interactive sub-regions**, so the click targets cannot drift from what is drawn. A pure, widget-free geometry helper computes the sub-rectangles; `paint()` draws into them and `editorEvent()` hit-tests against them. All behavior that needs vault/workspace access is reached through small host callbacks set by `MainWindow` (the established `setServices` / drawer-`FileManager` pattern), so `libs/bases` stays free of host specifics. The boolean toggle is the exception — it rides the model's existing `setData` write path rather than a callback.

### Components

#### 1. `CellHitTest` — pure geometry helper (new)

`libs/bases/include/corbomite/bases/CellHitTest.h` + `src/CellHitTest.cpp`. No Qt widgets (only `QRect`/`QString`/`ValuePtr`). The single entry point:

```cpp
namespace Corbomite::Bases {

struct CellHit {
    enum Kind { Whitespace, Checkbox, Link, Tag, Url } kind = Whitespace;
    int tagIndex = -1;        // valid when kind == Tag
    QString payload;          // Link: link target; Url: url; Tag: the tag text
};

/// Hit-test a point within a cell rect against the interactive element the
/// delegate paints for `valueType`/`value`. `point` and `cellRect` are in the
/// same coordinate space (viewport-relative). Returns Whitespace when the
/// point misses every interactive element (caller falls back to selection).
CellHit hitTestCell(const QString &valueType, const ValuePtr &value,
                    const QRect &cellRect, const QPoint &point);

/// The element rects the delegate must paint to (kept here so paint() and
/// hitTestCell agree). E.g. checkboxGlyphRect(cellRect), and per-tag chip rects.
QRect checkboxGlyphRect(const QRect &cellRect);
QVector<QRect> tagChipRects(const ValuePtr &listValue, const QRect &cellRect);
QRect linkTextRect(const QString &text, const QRect &cellRect);   // text-width bounded
QRect urlTextRect(const QString &text, const QRect &cellRect);

}  // namespace Corbomite::Bases
```

Geometry rules (must match `BasesCellDelegate::paint`):
- **Checkbox:** the ballot glyph is drawn `Qt::AlignCenter`; `checkboxGlyphRect` is a fixed-size (≈18×18) square centered in the cell. A click inside it is `Checkbox`.
- **Link/Url:** the text is drawn left-aligned; `linkTextRect`/`urlTextRect` is the bounding rect of the rendered text (via `QFontMetrics`), clipped to the cell. A click inside it is `Link`/`Url`.
- **Tag list:** tags render as chips left-to-right; `tagChipRects` returns each chip's rect in order. A click inside chip *i* is `Tag` with `tagIndex = i` and `payload` = that tag's text.
- Anything else → `Whitespace`.

#### 2. `BasesCellDelegate` (modify)

- Add `editorEvent(QEvent *, QAbstractItemModel *, const QStyleOptionViewItem &, const QModelIndex &) override`. On `QEvent::MouseButtonRelease` (left button):
  - Read `valueType` (`BasesTreeModel::ValueTypeRole`) + `value` (`ValuePtrRole`).
  - `const CellHit hit = hitTestCell(valueType, value, option.rect, mouseEvent->pos());`
  - `Checkbox` → `model->setData(index, !currentBool, Qt::EditRole)` (existing `processFrontMatter` path); return `true`.
  - `Link` → `Q_EMIT linkClicked(hit.payload, mouseEvent->modifiers());` return `true`.
  - `Tag` → `Q_EMIT tagClicked(hit.payload);` return `true`.
  - `Url` → `Q_EMIT urlClicked(hit.payload);` return `true`.
  - `Whitespace` → `return QStyledItemDelegate::editorEvent(...)` (default: selection; double-click still edits).
- Middle-click on a `Link` is also treated as "open in new tab": handle `QEvent::MouseButtonRelease` with `MiddleButton` → emit `linkClicked` with a synthetic Ctrl modifier (or a dedicated `bool newTab` arg — see signal shape below).
- Update `paint()` to draw checkbox/link/url/tag at the exact rects `CellHitTest` reports (refactor the existing boolean/tag/link painting to call the shared rect helpers). Tag chips get a subtle rounded-rect background so they read as clickable.
- Remove the boolean branch from `createEditor`/`setEditorData`/`setModelData` (single-click toggle replaces the `QCheckBox` editor). Number/Date/String editors stay.
- Signals:
  ```cpp
  Q_SIGNALS:
      void linkClicked(const QString &target, Qt::KeyboardModifiers mods);
      void tagClicked(const QString &tag);
      void urlClicked(const QString &url);
  ```

#### 3. `BasesView` (modify)

Wires the delegate signals + view-level interactions:
- Connect `m_delegate` signals (after creating the delegate):
  - `linkClicked(target, mods)`: resolve `target` → vault note path; if `mods` has Ctrl/Cmd → `m_openInNewTab(path)`; else `leaf()->navigate({type:"markdown", state:{file:path}})`. No-op if unresolved.
  - `tagClicked(tag)` → `m_searchTag(tag)`.
  - `urlClicked(url)` → `QDesktopServices::openUrl(QUrl(url))`.
- **Link resolution helper** `resolveLink(target) → QString path`: resolve a wikilink target against the vault (reuse the D.1 resolution path — `BasesVaultResolver`/vault link lookup). Returns empty if unresolved.
- **Context menu:** `m_table->setContextMenuPolicy(Qt::CustomContextMenu)`; on `customContextMenuRequested(pos)`, find the index, build a `QMenu`:
  - link/row resolving to a note: *Open* (navigate leaf), *Open in new tab* (`m_openInNewTab`), *Copy as wikilink* (clipboard `[[name]]`), *Rename…* (`m_promptRename(path)`), *Delete* (`m_promptDelete(path)`).
  - plain cell: *Copy value* (clipboard, the cell's display text).
- **Drag:** `m_table->setDragEnabled(true); m_table->setDragDropMode(QAbstractItemView::DragOnly);` (mime supplied by the model).
- New host callbacks (set by `MainWindow`, all `std::function`, null-safe):
  ```cpp
  void setOpenInNewTabHandler(std::function<void(const QString &path)>);
  void setTagSearchHandler(std::function<void(const QString &tag)>);
  void setRenamePrompt(std::function<void(const QString &path)>);
  void setDeletePrompt(std::function<void(const QString &path)>);
  ```

#### 4. `BasesTreeModel` (modify)

- `QStringList mimeTypes() const override` → `{"text/plain"}`.
- `QMimeData *mimeData(const QModelIndexList &indexes) const override` → for each unique entry row whose `BasesEntry` resolves to a note, append `[[name]]`; join with newlines into `text/plain`. Group rows / unresolved rows contribute nothing.
- `flags()` adds `Qt::ItemIsDragEnabled` for entry (non-group) rows.

#### 5. `MainWindow` (modify)

In the existing BasesView branch of `propagateServicesToView` (where `setServices`/`setCurrentFile` are wired), set the four new callbacks:
- open-in-new-tab → `openFileInWorkspace(path)`.
- tag-search → trigger the search plugin/quick-search for the tag (reuse the existing search entry point).
- rename/delete → `FileManagerProxy::promptForFileRename` / `promptForDeletion` on the resolved `TAbstractFile`.

## Data flow

```
mouse release in cell
  -> BasesCellDelegate::editorEvent
       -> CellHitTest::hitTestCell(type, value, rect, pos)
          - Checkbox -> model->setData -> BasesTreeModel::setData -> processFrontMatter
          - Link/Tag/Url -> delegate signal -> BasesView
               Link -> resolveLink -> leaf()->navigate  | m_openInNewTab
               Tag  -> m_searchTag
               Url  -> QDesktopServices::openUrl
          - Whitespace -> default (select; double-click edits)

right-click -> customContextMenuRequested -> BasesView builds QMenu -> actions above / clipboard / m_promptRename|Delete
drag        -> BasesTreeModel::mimeData -> "[[wikilink]]" text/plain
```

## Testing

- **`tst_bases_cell_hittest`** (new, `QTEST_APPLESS_MAIN`): unit tests for `hitTestCell` + the rect helpers — checkbox-glyph hit vs whitespace; link-text hit vs trailing whitespace; tag chip index resolution (click chip 0 vs chip 2 vs gap); URL hit; empty/Null cells → Whitespace. No widgets.
- **`tst_bases_tree_model`** (extend): `mimeData` over entry rows yields `[[name]]` text; group rows / unresolved rows excluded; `flags()` has `ItemIsDragEnabled` for entries, not groups.
- **Checkbox toggle** rides the existing `setData` path already covered by `tst_bases_tree_model`.
- **Click routing, context menu, drag-drop, navigation, browser-open, tag-search** are GUI/host-wired — verified by build + launch against the films vault (click `director` links → base tab navigates; Back returns; Ctrl-click → new tab; right-click → menu; drag a row into a note → `[[wikilink]]`).

## Definition of done

- Clicking a wikilink cell's link text navigates the base's leaf to the note (Back returns); Ctrl/Cmd/middle-click opens a new tab. Clicking whitespace in the same cell still selects; double-click still edits.
- Clicking a checkbox glyph toggles the frontmatter; clicking whitespace selects.
- Clicking a tag searches for it; clicking an external URL opens the browser.
- Right-click yields the file context menu (link/row) or Copy value (plain cell).
- Dragging an entry row produces `[[wikilink]]` text on drop.
- `tst_bases_cell_hittest` + extended `tst_bases_tree_model` pass; full `libs/bases` suite green; clean build.
- Hover-preview explicitly deferred and documented.

## Risks / notes

- **paint/hit-test drift.** The whole design hinges on `paint()` and `hitTestCell` using the *same* rect helpers. Keep the rect math exclusively in `CellHitTest`; `paint()` must not compute element rects independently.
- **Link resolution.** Reuse D.1's vault link resolution rather than re-implementing; an unresolved link is a no-op (no note creation in this slice).
- **Same-tab navigation divergence.** The app's editor opens internal links via reuse-or-new-leaf (`openFileInWorkspace`); Bases deliberately navigates its *own* leaf (per user direction). Both are intentional; do not "unify" them.
- **`editorEvent` vs double-click.** Returning `false` for whitespace preserves Qt's default selection + double-click-to-edit. Only consume the event when an interactive element is hit, so text editing on whitespace is unaffected.
- **Middle-click** may not arrive as a normal release on all platforms; if `editorEvent` doesn't see it reliably, fall back to handling new-tab only via Ctrl/Cmd-click and document it.
- **Tag cells in the default vault.** `Films.base` shows `note.director` (link) but not `note.tags`; tag-click is still implemented generally and can be eyeballed by adding `note.tags` to a view.
