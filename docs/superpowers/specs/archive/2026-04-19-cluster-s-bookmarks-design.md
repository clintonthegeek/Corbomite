# Cluster S — Bookmarks core plugin (Design Spec)

**Date:** 2026-04-19
**Scope:** Ship the Bookmarks internal plugin as an Obsidian-compatible `.obsidian/bookmarks.json` round-trip + side panel + command set + "Bookmark…" modal. Single-phase "normal task" dispatch post Cluster R Phase 1 (which supplies the menu-slot that S populates).
**Audit references:**
- `docs/obsidian-audit/addenda/2026-04-19-bookmarks-core-plugin.md` — full feature spec (on-disk schema, panel, commands, modal).
- Existing cluster plans: `2026-04-17-cluster-n-plugin-ready-surfaces.md` (plugin metadata format, `corbomite_add_plugin()`, `X-Corbomite-ApiLevel`), `2026-04-16-cluster-q-internal-plugin-wrapping.md` (InternalPlugin pattern, `Plugin::createView`, `PluginContext::setContextConfigurator`).

---

## 1. Goal

Fill the bookmark-shaped hole in Corbomite: an internal plugin at `src/plugins/bookmarks/` providing:
- A right-dock panel listing bookmarked files/folders/headings/blocks/searches/graphs, organised into optional user-defined groups.
- `.obsidian/bookmarks.json` round-trip exactly matching Obsidian's format (per addendum §2) so vaults move freely between Corbomite and Obsidian.
- Seven commands under the `bookmarks:*` namespace (per addendum §4).
- A "Bookmark…" modal for naming + grouping new bookmarks (per addendum §5).

**Unblocks:** Cluster R's "Bookmark…" menu slot goes from disabled-placeholder to live once S ships; R's GraphView menu gets its "Bookmark" action (graph options snapshot).

---

## 2. Key design decisions

### 2.1 Plugin as `KPluginFactory` `.so`

Follow the canonical Cluster Q / Cluster N InternalPlugin pattern: `src/plugins/bookmarks/` with `CMakeLists.txt` using `corbomite_add_plugin()`, a `BookmarksPlugin` class deriving from `Corbomite::Plugin`, and `metadata.json.in`.

Plugin id: `bookmarks`. Enabled by default (registered in `core-plugins.json` defaults).

### 2.2 On-disk format matches Obsidian verbatim

`.obsidian/bookmarks.json` with the schema in addendum §2. Unknown fields / unknown item `type` values are preserved on round-trip via Cluster B's unknown-key idiom (`QJsonObject` merge-preserving). This means vaults round-trip cleanly between Corbomite and Obsidian without losing future-format bookmarks we don't understand.

### 2.3 Model-first panel implementation

New `BookmarksModel : public QAbstractItemModel` backs a `QTreeView`. Column 0 is the bookmark title; no other columns. Item data:
- `Qt::DisplayRole` → title (inferred or overridden).
- `Qt::DecorationRole` → per-type icon.
- `Qt::ToolTipRole` → resolved path + creation date.
- Custom role `BookmarksTypeRole` → the raw `type` string for styling hooks.

Drag-drop via a custom MIME type `application/x-corbomite-bookmarks-drag` for intra-panel reorder; `text/uri-list` for drag-out to other widgets (copies the vault-relative path).

### 2.4 Addition path: modal for user-initiated, silent for command-initiated

- "Bookmark…" menu item (R P2) → modal (name + group picker) → on save, adds entry.
- `bookmarks:bookmark-current-file` (keyboard) → **no modal**; appends at root with inferred title. Matches Obsidian's two-path UX.

### 2.5 Block-id insertion lives on MarkdownView

`bookmarks:bookmark-current-block` requires the block under the cursor to have a `^blockId` marker. If absent, the command inserts one. This needs a new `MarkdownView::ensureBlockIdAtCursor()` method; fallback is to just refuse with a Notice ("Move cursor into a block to bookmark it.") if we don't want to auto-insert.

**Default in spec:** ship the auto-insert version. The Notice-only fallback is a future settings toggle.

### 2.6 Settings surface

Add a `BookmarksSettingsTab` contributed via `Plugin::createSettingsTab` (Cluster N substrate). Fields:
- **Enabled by default for new vaults** (reflects `core-plugins.json` default).
- **Backup before first mutation:** checkbox — if true, copies `bookmarks.json` to `bookmarks.json.bak` before the first write each session.
- (future) **Snapshot on conflict:** if Obsidian modified `bookmarks.json` while we had it open (detected via mtime), keep both.

---

## 3. Architecture

### 3.1 File structure

```
src/plugins/bookmarks/
  BookmarksPlugin.h/.cpp            (KPluginFactory entry; onLoad/onUnload; command reg)
  BookmarksView.h/.cpp              (ItemView subclass, viewType "bookmarks")
  BookmarksModel.h/.cpp             (QAbstractItemModel)
  BookmarksStore.h/.cpp             (JSON round-trip, mutation API)
  BookmarkItem.h                    (struct: type, path, subpath, title, query, options, children[])
  BookmarkModal.h/.cpp              (QDialog with name + group picker)
  CMakeLists.txt                    (uses corbomite_add_plugin())
  metadata.json.in                  (id=bookmarks, enabled-by-default=true, X-Corbomite-DockArea=right)
  BookmarksSettingsTab.h/.cpp       (settings page)
  tests/
    tst_bookmarks_store.cpp         (JSON round-trip, unknown-key preservation)
    tst_bookmarks_model.cpp         (tree operations, drag-drop)
    tst_bookmarks_commands.cpp      (each command produces expected json delta)
    tst_bookmarks_modal.cpp         (name pre-fill + group picker + save)
```

### 3.2 Data types

```cpp
struct BookmarkItem {
    QString type;                       // "file", "folder", "heading", "block", "search", "graph", "group"
    QString path;                       // file/folder/heading/block (with #... suffix)
    QString subpath;                    // redundant heading/block suffix, written alongside
    QString title;                      // user override; empty = infer from path/query
    QString query;                      // search only
    QJsonObject options;                // graph only
    qint64 ctime = 0;
    QList<BookmarkItem> children;       // group only
    QJsonObject unknownKeys;            // Cluster B idiom — preserved on round-trip
};

class BookmarksStore : public QObject {
    Q_OBJECT
public:
    explicit BookmarksStore(Corbomite::Vault *vault, QObject *parent = nullptr);
    bool load();                        // reads <vault>/.obsidian/bookmarks.json
    bool save();                        // atomic write
    void addBookmark(BookmarkItem item, const QStringList &groupPath = {});
    void removeBookmark(const QStringList &itemPath);
    void moveBookmark(const QStringList &fromPath, const QStringList &toParentPath, int index);
    BookmarkItem *find(const QStringList &path);
    const QList<BookmarkItem> &rootItems() const;
signals:
    void changed();
};
```

### 3.3 Command surface

Registered in `BookmarksPlugin::onLoad` via `CommandRegistrar`:

| Command | Args | Effect |
|---|---|---|
| `bookmarks:bookmark-current-file` | — | Adds `{type:"file", path: activeFile.path, ctime: now}` at root. Notice: "Bookmarked: {basename}". |
| `bookmarks:bookmark-current-heading` | — | Requires cursor in markdown view + in a heading range. Adds `{type:"file", path: file.path + "#" + heading}`. |
| `bookmarks:bookmark-current-block` | — | Requires cursor in markdown view. Ensures block id at cursor (`^id`); adds `{type:"file", path: file.path + "#^" + id}`. |
| `bookmarks:bookmark-current-search` | — | Requires active view is search. Adds `{type:"search", query: currentQuery}`. |
| `bookmarks:bookmark-current-graph` | — | Requires active view is graph. Snapshots options. |
| `bookmarks:bookmark-all-tabs` | — | Adds one `file` bookmark per workspace tab. |
| `bookmarks:open` | — | Reveals + activates the Bookmarks panel. |

### 3.4 Panel

`BookmarksView : public Corbomite::ItemView`. `viewType == "bookmarks"`. Icon theme: `bookmark-new`.

Header title: i18n("Bookmarks"). Right-side header button: `+` ("New bookmark from current").

Content: `QTreeView` with `BookmarksModel`. Interactions per addendum §3:
- Single-click: `workspace.openLinkText(item.path, "")` or equivalent via WorkspaceController.
- Middle-click / Ctrl+click: open in new tab (dispatches a split + leaf-open).
- Shift+click: open in split right.
- Drag: reorder via internal DnD.
- Right-click: `QMenu` with Rename / Move to group / Delete / (per-type extras).

Per-plugin UI state (expand/collapse state of groups) persists through `SessionManager::pluginSessionState("bookmarks")` — mirrors FileExplorer's Cluster Q follow-up idiom.

### 3.5 "Bookmark…" modal

`BookmarkModal` is a `QDialog`:

```
QVBoxLayout
  QLabel "Name"
  QLineEdit nameEdit          (pre-filled)
  QLabel "Group"
  QComboBox groupCombo        (items from recursive walk of groups; "/" = root)
  QDialogButtonBox {Cancel, Save}
```

Pre-fill logic:
- `type=file` → basename (no extension).
- `type=heading` → heading text.
- `type=block` → first 40 chars of block content.
- `type=search` → query string.
- `type=graph` → "Graph view".

Save calls `store->addBookmark(item, groupPath)` + closes with `Accepted`. Cancel closes with `Rejected`.

### 3.6 Plugin lifecycle

- `onLoad(context)`:
  1. Read `context->vault()->readConfigJson(".obsidian/bookmarks.json")`.
  2. Parse into store.
  3. Register commands via `context->commandRegistrar()`.
  4. Register view type `"bookmarks"` via `context->viewRegistrar()`.
  5. Subscribe to `store->changed()` → schedule debounced save (500 ms trailing).
  6. `createView` returns a docked `BookmarksView` instance mounted via the plugin dock host.
- `onUnload`:
  1. Final save if dirty.
  2. Unregister view + commands (automatic via `CommandRegistrar` / `ViewRegistrar` scope).
  3. Tear down store.

### 3.7 Permissions

Declares in metadata:
- `vault.read` (read `.obsidian/bookmarks.json` + read file metadata for title inference).
- `vault.write` (write `.obsidian/bookmarks.json`; ensure block ids in `bookmark-current-block`).
- `vault.events` (listen to rename/delete for stale-bookmark handling).
- `ui.views` (register the panel view type).
- `ui.commands` (register commands).

All auto-granted at trust level `X-Corbomite-Trusted=true` (plugin source under `src/plugins/` per Cluster N).

### 3.8 Rename/delete stale-bookmark handling

When a bookmarked file is renamed, walk bookmarks and update matching `path` values (preserving `#subpath`). When a bookmarked file is deleted, mark the bookmark with `unknownKeys["_orphaned"] = true` but keep it in the list (rendered greyed-out). Matches Obsidian's "don't silently lose bookmarks" invariant.

---

## 4. Phases (single phase, 3 task groups)

**Task group 1 — Store + model (2 days):**
1. `BookmarkItem` struct + JSON round-trip with unknown-key preservation.
2. `BookmarksStore` CRUD + atomic save.
3. `BookmarksModel` (QAbstractItemModel over store).
4. Tests: `tst_bookmarks_store`, `tst_bookmarks_model`.

**Task group 2 — Plugin shell + view + commands (2 days):**
5. `BookmarksPlugin` skeleton with `corbomite_add_plugin()` wiring.
6. `BookmarksView` with QTreeView + per-type icons + interactions.
7. Commands: 7 entries via `CommandRegistrar`.
8. Rename/delete stale-handling via Vault signals.
9. Session state round-trip.
10. Tests: `tst_bookmarks_commands`.

**Task group 3 — Modal + settings tab + R integration (1 day):**
11. `BookmarkModal` with name + group picker.
12. `BookmarksSettingsTab` (1 checkbox for backup-before-first-mutation).
13. Cluster R's "Bookmark…" menu slot: flip from disabled-placeholder to dispatching `bookmarks:bookmark-current-file` (with modal via sentinel `"with-modal": true` arg or a parallel `bookmarks:bookmark-current-file-with-modal` command).
14. Tests: `tst_bookmarks_modal`.

**Definition of done:**
- All 7 commands wired and tested.
- `.obsidian/bookmarks.json` round-trips an Obsidian-written file with unknown-key preservation (test fixture: a bookmarks.json exported from Obsidian 1.12.7).
- Panel shows, responds to drag-drop and context menus.
- Cluster R "Bookmark…" menu slot fires the modal.
- Full test suite green outside existing known-flakies.

Total estimate: **~5 days** as a single-phase normal task.

---

## 5. Risks + mitigations

| Risk | Mitigation |
|---|---|
| Obsidian's bookmarks.json schema evolves | Unknown-key preservation is the mitigation; tests lock in the current schema shape. |
| Block-id auto-insertion conflicts with a user's existing block-id style | Reuse `MetadataCache.blocks` to detect existing ids before generating a new one. |
| Drag-reorder persistence thrashes disk | 500 ms trailing debounce on save. |
| Graph bookmarks snapshot a `GraphOptions` blob that's Corbomite-specific | Keep options object as opaque JSON in the store; graph plugin re-applies on open. Round-trip to Obsidian may lose fidelity for graph bookmarks specifically; document in addenda follow-up. |
| Stale bookmarks (renamed outside Corbomite) accumulate | Mark as `_orphaned` + render greyed-out; user can bulk-clean via context menu "Remove all broken bookmarks". |

---

## 6. Open questions

1. **Auto-insert block ids on `bookmark-current-block` or refuse?** Default in spec: auto-insert. Alternative: refuse with Notice.
2. **Bookmarks panel docked default side — left or right?** Obsidian defaults to left in recent builds but it's user-configurable. Default in spec: right (matches our other dock-plugin defaults; user can move).
3. **`bookmarks:bookmark-current-file` via keybinding — what default?** Obsidian has no default; leaves it to user. Default in spec: match (no default).

Flag if any defaults are wrong; otherwise land as-spec'd.

---

## 7. Blocks / enables

**Blocked by:** Cluster R Phase 1 (menu substrate) — so the Bookmark… menu slot exists to wire into.

**Enables:**
- Full Obsidian-compat round-trip for vaults using bookmarks.
- Cluster R's "Bookmark…" menu slot goes live.
- Cluster R's GraphView "Bookmark" action goes live.
- Future: a plugin-facing `BookmarksProxy` (not in S scope; open to third-party bookmark providers).

---

## 8. Preserved-compat quirks

- Empty `bookmarks.json` (no `items` array) is valid; treat as equivalent to `{items: []}`.
- `subpath` field is redundant with the suffix in `path` but both are written; match exactly.
- Creation time is ms-since-epoch, not a date string.
- Search bookmarks snapshot the *query string*, not the search results.
- Opening a `type=folder` bookmark dispatches File Explorer reveal (not Obsidian-protocol `folder://`); match.
