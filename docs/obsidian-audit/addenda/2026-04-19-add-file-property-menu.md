# "Add file property" — hamburger entry point into the frontmatter editor

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. The Properties panel exposes an `+ Add property` button, but Obsidian also surfaces "Add file property" in the markdown hamburger menu's `action` section — that entry point is undocumented in `domains/editor-markdown.md` and `domains/editor.md`.
**Supersedes / extends:** Extends `domains/editor-markdown.md` §"metadataEditor", `domains/properties.md` (if present — if not, fold forward).
**Relevant cluster plans:** `superpowers/plans/2026-04-19-cluster-r-view-header-menus.md` (R P2 ships the menu entry + the routing into the frontmatter editor).

---

## 1. Menu entry

**Markdown hamburger, section `action`:**

```
Add file property
```

Icon: `lucide-list-plus`. Fires the `markdown:add-metadata-property` command (or equivalent internal plugin command — exact ID varies per release; locate via the string `"Add file property"`).

---

## 2. Behaviour

1. Ensures the active view has focus on its frontmatter editor (`metadataEditor`). If the view's current mode is Source and the file has no frontmatter, the command **inserts an empty frontmatter block** (`---\n\n---\n`) at the top of the file first.
2. Appends a new empty property row to the frontmatter editor with cursor placed in the **key** field.
3. If the view is currently in Reading mode, transparently switches to LivePreview (via `editor:toggle-live-preview`) so the newly inserted property is visible and editable.
4. The user types the key name; on `Enter` or `Tab`, focus moves to the value field.
5. Value type is inferred (text/number/date/list) on first save via the same inference rules the Properties panel uses (`Corbomite::inferPropertyType` in our impl, `Uk` or similar in Obsidian's).

---

## 3. Difference from the Properties panel's `+` button

Identical underlying mechanism — both call the same `metadataEditor.addProperty(null, null)` (key=null, value=null → blank row). The only difference is **where focus lands after**:
- Properties panel `+`: focus lands in the panel's own row (dock view).
- Hamburger "Add file property": focus lands in the inline frontmatter editor at the top of the active markdown view.

The former is useful when the user is already in the Properties panel flow; the latter is useful when the user is mid-write and wants to add a property without switching panes.

---

## 4. Availability

- Only enabled when the active view is a `MarkdownView` (any mode — Source / LivePreview / Reading). Disabled or hidden otherwise.
- Not available for Canvas, Graph, Image, PDF, or Audio views (those don't have frontmatter).
- Always available for `.md` files regardless of current frontmatter presence (the command creates one if missing).

---

## 5. Implementation hints for Corbomite

- New method on `Corbomite::MarkdownView`: `void insertFrontmatterProperty()`.
- Implementation pseudocode:
  1. If view is currently in Reading mode, call `setViewMode(ViewMode::LivePreview)`.
  2. Access the view's underlying `NoteDocument` / frontmatter editor widget.
  3. If document has no frontmatter: insert `---\n\n---\n` at offset 0 via the active editor's `insertText` API (not a vault-level write — goes through the editor so it's batched with normal save flow).
  4. Append empty property row: new `QLineEdit` pair (key + value) in the frontmatter editor, focus the key field.
  5. Commit the row on key-field blur with non-empty key (matches Properties panel writeback path via `FileManager::processFrontMatter`).
- Menu wiring: `MarkdownView::onMoreOptionsMenu` adds an `action`-section entry fires `insertFrontmatterProperty()`.
- Disabled/hidden on non-Markdown views: handled by the fact that `MarkdownView::onMoreOptionsMenu` is only called for MarkdownView; no explicit gate needed.
- Command palette registration: a parallel `markdown:add-metadata-property` command (Cluster C substrate) fires the same method, for keyboard users.
