# File Recovery core plugin — backup store + Version History modal

**Date:** 2026-04-19
**Discovered during:** Cluster R (View-header menus) scoping. `domains/views.md §187-188` briefly name-drops the `file-recovery` internal plugin as a backup-store destination for save-failure snapshots (Cluster G's TextFileView save-failure-backup cites the same path). The plugin's user-facing surface — the Version History modal, snapshot policy, and "Open version history" menu entry — is undocumented.
**Supersedes / extends:** Extends `domains/views.md §187-188`.
**Relevant cluster plans:** No immediate cluster. Deferred to a post-parity "Cluster T" slot; Cluster R ships the menu item as a disabled placeholder.

---

## 1. Overview

**Plugin id:** `file-recovery`. Enabled by default. Non-removable (core plugin). Responsible for:
1. **Passive periodic snapshotting.** Every N minutes (default 60, configurable in Settings → File Recovery), every markdown file modified since the previous interval has a timestamped snapshot written to the backup store.
2. **Save-failure backup.** When `TextFileView::save()` throws, the plugin receives a `fileManager.storeTextFileBackup(path, content)` call with the in-memory content; this is a **separate** write path from the periodic snapshot, meant as a data-loss safety net.
3. **Version History modal UI.** A modal showing the snapshot log per file, with a preview pane and "Restore this version" button.

---

## 2. Backup store on-disk format

**Root:** `<vault>/.obsidian/file-recovery.json` (single file; the snapshots themselves are embedded inline, not stored as separate files).

**Schema:**

```json
{
  "version": 1,
  "maxSnapshotAge": 604800,
  "maxSnapshotsPerFile": 50,
  "entries": {
    "notes/foo.md": [
      { "ts": 1713544200000, "data": "<full file contents as string>" },
      { "ts": 1713544800000, "data": "<...>" },
      { "ts": 1713545400000, "data": "<...>" }
    ],
    "notes/bar.md": [ /* ... */ ]
  }
}
```

Keyed by vault-relative path. `data` is the full file body at snapshot time — plain string, no compression. `ts` is ms-since-epoch.

**Size management:**
- `maxSnapshotAge` (seconds): snapshots older than this are pruned on the next write. Default 7 days.
- `maxSnapshotsPerFile`: oldest snapshots beyond this count are pruned. Default 50.
- A file deleted from the vault has its entry removed immediately (not retained for undelete).

**Save-failure backups** are distinguished by an extra `{"failure": true}` field on the entry and are **not pruned** by the age/count policy.

**Size ceiling:** the full `file-recovery.json` is typically small-kilobytes-to-low-megabytes; Obsidian does not compress. Large-vault users sometimes hit performance issues reading/writing this file — a future optimization would be per-file snapshot files in `.obsidian/file-recovery/<path-hash>/` with an index.

---

## 3. "Open version history" menu entry

**Markdown hamburger, section `view.linked`:**

```
Open version history
```

Fires the `file-recovery:open` command which opens the Version History modal scoped to the active file.

Also accessible via the command palette.

---

## 4. Version History modal

```
┌──────────────────────────────────────────────────────┐
│  Version history: foo.md                       [X]   │
├─────────────────────────┬────────────────────────────┤
│  2026-04-19 14:32       │                            │
│  2026-04-19 13:28       │   Preview of selected      │
│  2026-04-19 11:47       │   snapshot                 │
│  2026-04-18 15:06       │                            │
│  2026-04-18 09:12       │                            │
│  ...                    │                            │
│                         │                            │
│                         │                            │
│                         │  [ Restore this version ]  │
└─────────────────────────┴────────────────────────────┘
```

- **Left pane:** list of snapshot timestamps (newest top), save-failure snapshots marked with a red dot.
- **Right pane:** preview of the selected snapshot. Full content rendered as read-only Source mode (no LivePreview, no ReadingView — just plain text).
- **Restore button:** writes the snapshot content to the current file via `vault.modify` (atomic RMW). The restore **itself creates a new snapshot** of the pre-restore content, so restores are undoable.
- **No diff view** in the current Obsidian build (as of 1.12.7). Per-snapshot diffs are a user-requested feature that has not shipped.

---

## 5. Settings

`Settings → File Recovery`:
- **Snapshot interval** (minutes): 5, 15, 30, **60** (default), 120, "Off".
- **Maximum snapshot age** (days): 1, 3, **7** (default), 14, 30.
- **Save-failure backups:** always on (non-configurable).

---

## 6. Why Cluster R defers this

Cluster G's `TextFileView` already writes save-failure backups to `.obsidian/file-recovery/<filename>-<timestamp>.md` (per Cluster G Part 1 spec §2.4) — a simpler format than Obsidian's. That lives as a data-loss safety net without the Version History UI.

The full file-recovery feature (periodic snapshots + modal + restore) is a separate cluster's worth of work:
- New plugin at `src/plugins/file-recovery/`.
- Periodic snapshot scheduler (QTimer on vault open, walks `Vault::getMarkdownFiles()` every interval).
- JSON round-trip with size management.
- Version History modal (`QDialog` with `QListWidget` + read-only `QPlainTextEdit` preview).

Cluster R P2 ships the "Open version history" menu entry as a **disabled placeholder** (greyed out, tooltip "File recovery plugin not enabled"); a future Cluster T activates it.

---

## 7. Compatibility note

Our Cluster G save-failure format (`<filename>-<timestamp>.md` files in `.obsidian/file-recovery/`) does **not** match Obsidian's `file-recovery.json` format. When we ship the full Cluster T:

- Option A: migrate existing per-file backups into the JSON store on first run.
- Option B: keep both paths (legacy files + new JSON) and have the modal read from both.
- Option C: drop the per-file format entirely at Cluster T ship; accept that pre-T save-failure backups are lost.

Recommendation: A. Migration is a one-time read-all-files-merge-into-json at Cluster T load; after one successful migration, the legacy directory is removed.
