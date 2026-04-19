# "Merge entire file with…" modal

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. `domains/vault.md §89` documents `insertIntoFile(file, content, mode = "append"|"prepend")` as a primitive but the user-facing "Merge entire file with…" modal is undocumented. The menu item appears in the markdown hamburger's `action` section.
**Supersedes / extends:** Extends `domains/vault.md §89`.
**Relevant cluster plans:** Not targeted for immediate implementation. Cluster R plan notes this as deferred (low-value, adjacent to `insertIntoFile`).

---

## 1. Function

"Merge entire file with…" is a file-picker modal that takes the current file's entire contents and inserts them into another user-chosen file (appended by default; prepend option available). On confirm, the current file is **deleted** (trashed — not permanently removed) and the target file is opened in the active leaf.

UX summary:
1. User opens markdown note A's hamburger → "Merge entire file with…".
2. Fuzzy file-picker modal appears (SuggestModal shape).
3. User picks target note B.
4. Confirm dialog: "Merge 'A.md' into 'B.md'?" with append/prepend toggle + Cancel/Merge buttons.
5. Confirm: `insertIntoFile(B, A.contents, mode)` runs, then `vault.trash(A)` runs, then active leaf switches to B.

---

## 2. Modal pipeline

**Stage 1 — file picker** (SuggestModal):

```
┌────────────────────────────────────────────┐
│  Merge 'A.md' with...                 [X]  │
├────────────────────────────────────────────┤
│  [                                   ]     │
│  ─────────────────────────────────────     │
│  notes/project.md                          │
│  notes/research.md                         │
│  ...                                       │
└────────────────────────────────────────────┘
```

- Filter input, fuzzy-match against all `.md` files in vault except the source file itself.
- Selection advances to stage 2.

**Stage 2 — append/prepend confirm dialog:**

```
┌────────────────────────────────────────────┐
│  Merge files                          [X]  │
├────────────────────────────────────────────┤
│  This will copy all content from 'A.md'    │
│  into 'B.md' and delete 'A.md'.            │
│                                            │
│  (●) Append to end of B.md                 │
│  ( ) Prepend to start of B.md              │
│                                            │
│                  [ Cancel ]  [  Merge  ]   │
└────────────────────────────────────────────┘
```

---

## 3. Merge pipeline

1. Read source file's full contents via `vault.read(A)`.
2. **Frontmatter handling:** if source has frontmatter and target has frontmatter, frontmatter blocks **are not merged** — source frontmatter is stripped before insert. If source has frontmatter but target doesn't, the source frontmatter still gets stripped (Obsidian's current behaviour; arguably a bug but it's the shipped UX).
3. Call `vault.process(B, text => (mode === 'append' ? text + '\n\n' + srcBody : srcBody + '\n\n' + text))` — atomic RMW.
4. On success: `vault.trash(A, useSystemTrash)` following the trash-option policy.
5. Open target file in active leaf: `workspace.openLinkText(B.path, '')`.
6. Source note's MetadataCache entry is evicted by the delete event; target note's is re-derived by the modify event.

**Link rewriting:** No automatic link rewrite. `[[A]]` wikilinks elsewhere in the vault become broken after merge — Obsidian does not update them. (Users are expected to run "Update links on rename" manually; this is a known rough edge.)

**Failure modes:**
- Target file is open in another leaf with unsaved edits: the three-way-merge path (`TextFileView::onModify`) kicks in and merges on top of the user's pending edits.
- `insertIntoFile` fails (disk full, permission denied): source file is NOT trashed; Notice surfaces the error.
- Source and target are the same file: blocked at stage 1 (source is excluded from the picker).

---

## 4. Implementation hints for Corbomite

- Cluster R **does not ship this** in P1-P4; noted as a deferred follow-up.
- If/when shipped: lives on `MarkdownView::onMoreOptionsMenu` as an `action`-section item.
- Pipeline: reuse `FuzzyMatcher` from Cluster D for stage 1; `KMessageBox` for stage 2; `FileManager::insertIntoFile` (exists) + `FileManager::trashFile` (exists) for the merge.
- Frontmatter-strip: run source body through `MarkdownHeaderExtractor` to find frontmatter bounds and slice it out before the `insertIntoFile` call.

---

## 5. Why deferred

Low-value-to-effort ratio: the feature combines primitives that already exist (insertIntoFile + trashFile + fuzzy picker) but the UX is a three-stage flow with non-trivial frontmatter-handling edge cases. In Obsidian's own usage telemetry it's one of the least-exercised menu items. Cluster R defers it; revisit when a user explicitly asks.
