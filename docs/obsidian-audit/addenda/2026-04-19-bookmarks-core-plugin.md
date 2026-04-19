# Bookmarks core plugin — on-disk format, panel, commands, modal

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. The "Bookmark…" hamburger entry for markdown notes (and the bookmarks side panel) surfaced a Bookmarks internal-plugin gap in the audit: `domains/core.md` lists `file-explorer`, `global-search`, `outline`, `graph`, `canvas`, etc., but not `bookmarks`, even though Bookmarks ships enabled-by-default in Obsidian 1.x.
**Supersedes / extends:** No prior coverage in audit. Touches `domains/core.md` §"Internal plugins" (to be cross-referenced), `domains/workspace.md` §6 (menu emission — the "Bookmark…" menu entry is injected here).
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-s-bookmarks.md` (to be written), `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (to be written — R P2 ships the "Bookmark…" menu slot, S P1 ships the plugin that populates it).

> **Source provenance.** Obsidian 1.12.7. Primary reference: `renamed/_internal.js` and `formatted/app.js` for the bookmarks internal-plugin class (identifier varies per build — recent releases use `E1`/`A1`/similar; locate via the string `"bookmarks.json"`). Panel view-type is `"bookmarks"`. Built-in command prefix is `bookmarks:`. Help docs at `testvaults/obsidian-help/en/Plugins/Bookmarks.md` cross-reference the user-facing surface.

---

## 1. Overview

Bookmarks is an **internal plugin** (enabled by default) that lets users save references to files, folders, headings, blocks, full-text searches, and graph configurations into a tree of groups. Persistence is a single `.obsidian/bookmarks.json` file. A right-dock `ItemView` with `viewType === "bookmarks"` renders the tree; commands register under the `bookmarks:*` namespace; a modal provides titled + grouped bookmark creation.

**Corbomite equivalent status at 2026-04-19:** missing. No plugin at `src/plugins/bookmarks/`, no on-disk reader, no commands registered.

---

## 2. On-disk schema

File: `<vault>/.obsidian/bookmarks.json`. JSON with a single top-level `items` array. Items are a discriminated union on `type`:

```json
{
  "items": [
    { "type": "file",   "ctime": 1713544200000, "path": "notes/foo.md", "title": "Optional custom title" },
    { "type": "file",   "ctime": 1713544300000, "path": "notes/foo.md#Heading", "subpath": "#Heading" },
    { "type": "file",   "ctime": 1713544400000, "path": "notes/foo.md#^abc123", "subpath": "#^abc123" },
    { "type": "folder", "ctime": 1713544500000, "path": "notes/daily" },
    { "type": "search", "ctime": 1713544600000, "query": "tag:#todo path:notes/" },
    { "type": "graph",  "ctime": 1713544700000, "options": { "search": "", "tags": true, ... } },
    { "type": "group",  "ctime": 1713544800000, "title": "Reading queue", "items": [ /* recursive */ ] }
  ]
}
```

**Type-by-type field contract:**

| `type` | Required | Optional | Semantics |
|---|---|---|---|
| `file` | `path` (vault-relative + optional `#heading` or `#^blockId` suffix), `ctime` (ms) | `title` (override display), `subpath` (the `#...` portion split out — redundant when present in `path`; Obsidian writes both) | Opens via `workspace.openLinkText(path, "")`. Heading/block bookmarks are just `file` items with subpath. |
| `folder` | `path`, `ctime` | `title` | Reveals folder in File Explorer (`file-explorer:reveal-active-file` against that path). |
| `search` | `query`, `ctime` | `title` | Opens Search panel and runs query via `global-search:open`. |
| `graph` | `options` (the full `GraphOptions` blob), `ctime` | `title` | Opens Graph view and applies options. |
| `group` | `items` (nested array), `ctime` | `title` | Pure container; no open action. Ordering within an array matters (drag-reorders are persisted). |

Unknown `type` values are retained verbatim on round-trip — Cluster B's unknown-key preservation idiom applies.

`ctime` is the bookmark's creation timestamp in ms-since-epoch, not the underlying file's; used as a stable sort key fallback.

---

## 3. Panel view

**Container:** `ItemView` with `viewType === "bookmarks"`, icon `"lucide-bookmark"`, docked right sidebar by default. Header title "Bookmarks".

**Content:** tree widget. Each row renders an icon by type (file/folder/heading/block/search/graph/group), a title (user-overridden `title` or inferred from `path`/`query`), and context-menu support. Root-level `items` are at depth 0; groups expand inline.

**Interactions:**
- Single-click: open in active leaf.
- Middle-click (or Ctrl+click): open in new tab.
- Shift+click: open in split.
- Drag: reorder within siblings or across groups.
- Right-click: context menu with "Rename…", "Move to group…", "Delete", and type-specific extras.
- `+` button in header: "New bookmark from current" (prefers active leaf's file/search/graph depending on viewType).
- Group folder-icon toggle: expand/collapse persists in the plugin's `data.json` (not in `bookmarks.json` itself — UX state vs. data).

---

## 4. Commands

Registered at plugin load:

| Command ID | Default hotkey | Effect |
|---|---|---|
| `bookmarks:bookmark-current-file` | (none) | Adds `{type:"file", path: activeFile.path, ctime: Date.now()}` at root. No modal. |
| `bookmarks:bookmark-current-heading` | (none) | Active view must be markdown + cursor within a heading's range → adds `{type:"file", path: activeFile.path + "#" + heading.heading, subpath: "#..."}`. |
| `bookmarks:bookmark-current-block` | (none) | Ensures the block under cursor has a block id (inserts `^abc123` if missing via `fileManager.processFrontMatter`-like block-id path) then adds the `#^blockId` bookmark. |
| `bookmarks:bookmark-current-search` | (none) | Active view must be search panel → snapshots query string. |
| `bookmarks:bookmark-current-graph` | (none) | Active view must be graph → snapshots `GraphOptions`. |
| `bookmarks:bookmark-all-tabs` | (none) | Iterates workspace tabs, adds one file bookmark per tab. |
| `bookmarks:open` | (none) | Reveals/activates the Bookmarks panel. |

The "Bookmark…" entry in any `onPaneMenu` fires **not** a command but a *modal* (§5); the modal then calls the same under-the-hood `addBookmark` helper.

---

## 5. "Bookmark…" modal UX

Opened from the hamburger menu ("Bookmark…" with icon `"lucide-bookmark"`, section `action`). Shape:

```
┌────────────────────────────────────────────┐
│  Bookmark                             [X]  │
├────────────────────────────────────────────┤
│  Name    [Inferred from file/heading….]    │
│  Group   [▼ Bookmarks                  ]   │
│                                            │
│                    [ Cancel ]  [ Save ]    │
└────────────────────────────────────────────┘
```

- **Name** text field pre-filled from the bookmark's inferred title (e.g. file basename, heading text, query string).
- **Group** dropdown lists the root plus every `type:"group"` recursively, rendered with `/`-joined titles.
- Save commits to `bookmarks.json`; Cancel closes without write.

No validation beyond "Name must be non-empty". Duplicate titles are allowed.

---

## 6. Lifecycle + events

- Plugin `onload`: reads `bookmarks.json` if present (else starts with empty `items`), registers the view, registers commands, emits `bookmarks:changed` on mutation.
- Writes debounce (likely ~500 ms trailing) to avoid thrashing on drag-reorder.
- Plugin disable: unregisters view + commands; does not delete `bookmarks.json` (data persists across enable/disable cycles, matching every other core plugin).

No Obsidian-side API is exported beyond the commands and `bookmarks.json` format — third-party plugins programmatically add bookmarks by writing JSON, not via an exposed method.

---

## 7. Implementation hints for Corbomite

- Location: `src/plugins/bookmarks/` using `corbomite_add_plugin()` (Cluster N idiom).
- On-disk read/write via `Vault::readConfigJson`/`writeConfigJson` (Q.0 Phase 4).
- Panel implementation: `QTreeView` over a `BookmarksModel` (`QAbstractItemModel`); drag-reorder via internal drag-drop MIME type.
- "Bookmark…" menu modal: `QDialog` with `QLineEdit` name field + `QComboBox` group picker; populate combo from recursive walk of groups.
- Command registration via `CommandRegistrar` (Cluster Q).
- Bookmark-current-block: requires a new `MarkdownView::ensureBlockIdAtCursor()` helper since block-id insertion is not a FileManager primitive today.

---

## 8. Follow-ups surfaced by this addendum

- `domains/core.md` should gain a "Bookmarks" entry in its internal-plugin table the next time that doc is revised (or via a separate corrective addendum).
- The `domains/views.md` `view.linked` submenu inventory is silent about whether "Open linked view" ever reaches into Bookmarks-panel state — it doesn't; Bookmarks is neither a file-linked view nor bookmarkable itself.
- The `obsidian://` protocol does not have a bookmark-dedicated URL (verified against `workspace.md §420`) — bookmarks don't round-trip through deep links.
