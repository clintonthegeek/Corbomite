# Link Index — Design Specification

## Overview

Extend the existing `SQLiteIndex` in `libs/storage/` with link relationship tracking and tag indexing. This is the data infrastructure that backlinks panels, outline panels, graph view, and link repair on rename all depend on.

## Database Schema Extensions

Add two new tables to the existing `.corbomite/index.sqlite` database (alongside the existing `notes_fts` FTS5 table):

```sql
CREATE TABLE IF NOT EXISTS links (
    source_path TEXT NOT NULL,
    target_path TEXT NOT NULL,
    link_type TEXT NOT NULL,       -- 'wiki', 'markdown', 'embed'
    display_text TEXT,             -- alias from [[target|display]]
    PRIMARY KEY (source_path, target_path, link_type)
);

CREATE INDEX IF NOT EXISTS idx_links_target ON links(target_path);

CREATE TABLE IF NOT EXISTS note_tags (
    note_path TEXT NOT NULL,
    tag TEXT NOT NULL,
    PRIMARY KEY (note_path, tag)
);

CREATE INDEX IF NOT EXISTS idx_tags_tag ON note_tags(tag);
```

## New Data Types

```cpp
struct LinkInfo {
    QString sourcePath;      // Note containing the link
    QString targetPath;      // Note being linked to (resolved, with .md)
    QString linkType;        // "wiki", "markdown", "embed"
    QString displayText;     // Alias text, if any (from [[target|display]])
};
```

## Link Extraction

When `indexNote()` is called, parse the note content for links and tags using these regexes (same patterns proven in the highlighter):

| Pattern | Regex | Produces |
|---------|-------|----------|
| `[[Target]]` | `\[\[([^\]|]+)\]\]` | link_type="wiki", target=Target.md |
| `[[Target\|Display]]` | `\[\[([^\]|]+)\|([^\]]+)\]\]` | link_type="wiki", display_text=Display |
| `![[Embed]]` | `!\[\[([^\]|]+)\]\]` | link_type="embed" |
| `[Text](path.md)` | `\[([^\]]+)\]\(([^)]+\.md)\)` | link_type="markdown" |
| `#tag` | `(?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*)` | tag in note_tags |

**Target resolution:** Append `.md` if the target doesn't already have an extension. Handle `[[Target#heading]]` by stripping the `#heading` part (store just the note path).

**Code block exclusion:** Skip links and tags found inside ``` fenced code blocks (same line-by-line fence tracking used in `VaultModel::allTags()`).

## Extended SQLiteIndex API

### New Methods

```cpp
// Link queries
QVector<LinkInfo> backlinksFor(const QString &targetPath) const;
QVector<LinkInfo> outlinksFor(const QString &sourcePath) const;
QVector<QString> orphanLinks() const;  // targets that don't exist as notes

// Tag queries
QStringList allTags() const;
QStringList notesWithTag(const QString &tag) const;

// Link repair
int repairLinks(const QString &oldTargetPath, const QString &newTargetPath,
                const QString &vaultRoot);
```

### Modified Methods

```cpp
void indexNote(const QString &relativePath, const QString &title, const QString &content);
// Extended: after FTS5 insert, also extract and insert links + tags

void removeNote(const QString &relativePath);
// Extended: also DELETE FROM links WHERE source_path = ? AND DELETE FROM note_tags WHERE note_path = ?

void rebuildIndex(const QString &vaultRoot);
// Extended: also rebuilds links and note_tags tables
```

### createTables() Extended

```cpp
void createTables() {
    // Existing FTS5 table
    query.exec("CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(...)");
    // New tables
    query.exec("CREATE TABLE IF NOT EXISTS links (...)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_links_target ON links(target_path)");
    query.exec("CREATE TABLE IF NOT EXISTS note_tags (...)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_tags_tag ON note_tags(tag)");
}
```

## Link Repair on Rename

`repairLinks(oldTargetPath, newTargetPath, vaultRoot)`:

1. Query: `SELECT DISTINCT source_path FROM links WHERE target_path = ?` (oldTargetPath)
2. For each source note:
   a. Read file content from disk via `FileSystemAdapter`
   b. Derive old/new note names: strip `.md`, extract filename from path
   c. Replace in content:
      - `[[OldName]]` → `[[NewName]]`
      - `[[OldName|display]]` → `[[NewName|display]]`
      - `[[OldName#heading]]` → `[[NewName#heading]]`
      - `[text](old/path.md)` → `[text](new/path.md)`
   d. Write updated content back to disk
   e. Re-index the source note (updates FTS5 + links)
3. Update links table: `UPDATE links SET target_path = ? WHERE target_path = ?`
4. Return count of notes modified

Called from `NoteService::renameNote()` after the file rename succeeds.

## Integration with VaultModel

`VaultModel::allTags()` currently scans all files on disk with regex (slow for large vaults). Refactor to delegate to `SQLiteIndex::allTags()` which queries the indexed `note_tags` table (fast). The cache mechanism in VaultModel (`m_cachedTags`, `m_tagCacheDirty`) can be simplified since the index is always up-to-date.

## Testing

### New test cases in `tst_sqliteindex.cpp`:

**Link extraction:**
- Index note with `[[Target]]` → backlinksFor("Target.md") returns source
- Index note with `[[Target|Display]]` → link has displayText
- Index note with `![[image.png]]` → outlinksFor returns embed type
- Index note with `[text](other.md)` → outlinksFor returns markdown type
- Index note with `[[Target#heading]]` → target resolved to "Target.md" (heading stripped)

**Backlinks/outlinks:**
- backlinksFor returns all notes linking to target
- outlinksFor returns all links from source
- Multiple notes linking to same target all appear in backlinks
- Removing a note removes its links from both sides

**Orphan links:**
- Note links to non-existent target → orphanLinks() returns it
- After creating the target note → orphanLinks() no longer includes it

**Tags:**
- allTags() returns sorted unique tags
- notesWithTag() returns correct note paths
- Tags inside code blocks are excluded

**Link repair:**
- Rename target → all source notes' content updated
- Wikilinks `[[OldName]]` become `[[NewName]]`
- Aliased links `[[OldName|display]]` become `[[NewName|display]]`
- repairLinks returns correct count of modified notes
- FTS5 content also updated for modified notes

## What This Does NOT Include
- Backlinks/Outlinks/Outline UI panels (Sub-project 3b)
- Graph visualization (Sub-projects 3c/3d)
- Frontmatter `tags:` property extraction (future)
- Block reference `^id` tracking (future)
- Search operators using link data (future)
