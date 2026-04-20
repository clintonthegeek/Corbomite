# Link Index — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend SQLiteIndex with link relationship tracking, tag indexing, backlink/outlink queries, and link repair on rename.

**Architecture:** Add `links` and `note_tags` tables to the existing `.corbomite/index.sqlite`. Extend `indexNote()` to parse content for wikilinks, markdown links, embeds, and tags. Add query methods for backlinks, outlinks, orphan links, and tags. Add `repairLinks()` for updating references when notes are renamed.

**Tech Stack:** C++20, Qt6::Sql (SQLite), QRegularExpression

**Spec:** `docs/superpowers/specs/2026-03-31-link-index-design.md`

**Current state:** SQLiteIndex has FTS5 `notes_fts` table with `open/close/rebuildIndex/indexNote/removeNote/search`. 17 tests passing (16 unit + 1 E2E suite).

**Files modified:**
- `libs/storage/include/corbomite/storage/SQLiteIndex.h` — add LinkInfo struct, new methods
- `libs/storage/src/SQLiteIndex.cpp` — implement tables, link extraction, queries, repair
- `tests/storage/tst_sqliteindex.cpp` — add ~15 new test cases

---

### Task 1: Schema + LinkInfo + Link Extraction

**Files:**
- Modify: `libs/storage/include/corbomite/storage/SQLiteIndex.h`
- Modify: `libs/storage/src/SQLiteIndex.cpp`
- Modify: `tests/storage/tst_sqliteindex.cpp`

- [ ] **Step 1: Write link extraction and query tests**

Add to `tests/storage/tst_sqliteindex.cpp`:

```cpp
    void testWikiLinkExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target Note]] for details."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target Note.md"));
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("wiki"));
    }

    void testWikiLinkWithAlias()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target|displayed text]] here."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target.md"));
        QCOMPARE(outlinks.at(0).displayText, QStringLiteral("displayed text"));
    }

    void testEmbedExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Embed: ![[image.png]]"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("embed"));
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("image.png"));
    }

    void testMarkdownLinkExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [click here](other.md) for more."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("other.md"));
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("markdown"));
    }

    void testHeadingLinkStripsFragment()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target#Section One]] for info."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target.md"));
    }

    void testBacklinksFor()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("A"),
                        QStringLiteral("Links to [[Target]]"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("B"),
                        QStringLiteral("Also links to [[Target]]"));
        index.indexNote(QStringLiteral("c.md"), QStringLiteral("C"),
                        QStringLiteral("No links here"));

        auto backlinks = index.backlinksFor(QStringLiteral("Target.md"));
        QCOMPARE(backlinks.size(), 2);

        QStringList sources;
        for (const auto &link : backlinks) sources << link.sourcePath;
        QVERIFY(sources.contains(QStringLiteral("a.md")));
        QVERIFY(sources.contains(QStringLiteral("b.md")));
    }

    void testOutlinksFor()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[A]] and [[B]] and ![[C.png]]"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 3);
    }

    void testRemoveNoteRemovesLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[Target]]"));
        QCOMPARE(index.backlinksFor(QStringLiteral("Target.md")).size(), 1);

        index.removeNote(QStringLiteral("source.md"));
        QCOMPARE(index.backlinksFor(QStringLiteral("Target.md")).size(), 0);
    }

    void testOrphanLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        // source links to Target, but Target doesn't exist as an indexed note
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[Nonexistent Note]]"));

        auto orphans = index.orphanLinks();
        QVERIFY(orphans.contains(QStringLiteral("Nonexistent Note.md")));

        // Now index the target — it should no longer be orphan
        index.indexNote(QStringLiteral("Nonexistent Note.md"), QStringLiteral("Nonexistent Note"),
                        QStringLiteral("I exist now"));
        orphans = index.orphanLinks();
        QVERIFY(!orphans.contains(QStringLiteral("Nonexistent Note.md")));
    }

    void testLinksInCodeBlockExcluded()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Real [[Link]]\n\n```\n[[Not A Link]]\n```\n"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Link.md"));
    }

    void testTagExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Hello #project and #status/active tag"));

        auto tags = index.allTags();
        QVERIFY(tags.contains(QStringLiteral("project")));
        QVERIFY(tags.contains(QStringLiteral("status/active")));
    }

    void testNotesWithTag()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("A"),
                        QStringLiteral("Has #shared and #onlyA"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("B"),
                        QStringLiteral("Has #shared and #onlyB"));

        auto shared = index.notesWithTag(QStringLiteral("shared"));
        QCOMPARE(shared.size(), 2);

        auto onlyA = index.notesWithTag(QStringLiteral("onlyA"));
        QCOMPARE(onlyA.size(), 1);
        QCOMPARE(onlyA.at(0), QStringLiteral("a.md"));
    }

    void testTagsInCodeBlockExcluded()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Real #tag\n\n```\n#not-a-tag\n```\n"));

        auto tags = index.allTags();
        QVERIFY(tags.contains(QStringLiteral("tag")));
        QVERIFY(!tags.contains(QStringLiteral("not-a-tag")));
    }

    void testReindexUpdatesLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        // First index with link
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("[[OldTarget]]"));
        QCOMPARE(index.outlinksFor(QStringLiteral("source.md")).size(), 1);
        QCOMPARE(index.outlinksFor(QStringLiteral("source.md")).at(0).targetPath,
                 QStringLiteral("OldTarget.md"));

        // Re-index with different content
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("[[NewTarget]]"));
        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("NewTarget.md"));

        // Old target should have no backlinks
        QCOMPARE(index.backlinksFor(QStringLiteral("OldTarget.md")).size(), 0);
    }
```

- [ ] **Step 2: Add LinkInfo struct and new method declarations to header**

Update `libs/storage/include/corbomite/storage/SQLiteIndex.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Corbomite {

struct SearchMatch {
    QString notePath;
    QString snippet;
    double score = 0.0;
};

struct LinkInfo {
    QString sourcePath;
    QString targetPath;
    QString linkType;       // "wiki", "markdown", "embed"
    QString displayText;    // alias, if any
};

class SQLiteIndex : public QObject {
    Q_OBJECT

public:
    explicit SQLiteIndex(QObject *parent = nullptr);
    ~SQLiteIndex() override;

    bool open(const QString &dbPath);
    void close();

    void rebuildIndex(const QString &vaultRoot);
    void indexNote(const QString &relativePath, const QString &title, const QString &content);
    void removeNote(const QString &relativePath);

    // Full-text search
    QVector<SearchMatch> search(const QString &query, int maxResults = 100) const;

    // Link queries
    QVector<LinkInfo> backlinksFor(const QString &targetPath) const;
    QVector<LinkInfo> outlinksFor(const QString &sourcePath) const;
    QVector<QString> orphanLinks() const;

    // Tag queries
    QStringList allTags() const;
    QStringList notesWithTag(const QString &tag) const;

    // Link repair
    int repairLinks(const QString &oldTargetPath, const QString &newTargetPath,
                    const QString &vaultRoot);

Q_SIGNALS:
    void indexReady();

private:
    void createTables();
    void extractAndInsertLinks(const QString &sourcePath, const QString &content);
    void extractAndInsertTags(const QString &notePath, const QString &content);
    static QString resolveTarget(const QString &rawTarget);

    QString m_connectionName;
    bool m_isOpen = false;
};

} // namespace Corbomite
```

- [ ] **Step 3: Implement schema, extraction, and query methods**

Replace `libs/storage/src/SQLiteIndex.cpp` entirely:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultScanner.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/core/NoteMeta.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QFileInfo>
#include <QRegularExpression>

namespace Corbomite {

SQLiteIndex::SQLiteIndex(QObject *parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString())
{
}

SQLiteIndex::~SQLiteIndex()
{
    close();
}

bool SQLiteIndex::open(const QString &dbPath)
{
    close();

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        return false;
    }

    createTables();
    m_isOpen = true;
    return true;
}

void SQLiteIndex::close()
{
    if (m_isOpen) {
        QSqlDatabase::database(m_connectionName).close();
        m_isOpen = false;
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SQLiteIndex::createTables()
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // FTS5 content search (existing)
    query.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5("
        "path, title, content, tokenize = 'porter unicode61'"
        ")"));

    // Link relationships
    query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS links ("
        "source_path TEXT NOT NULL, "
        "target_path TEXT NOT NULL, "
        "link_type TEXT NOT NULL, "
        "display_text TEXT, "
        "PRIMARY KEY (source_path, target_path, link_type)"
        ")"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_links_target ON links(target_path)"));

    // Tag index
    query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS note_tags ("
        "note_path TEXT NOT NULL, "
        "tag TEXT NOT NULL, "
        "PRIMARY KEY (note_path, tag)"
        ")"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_tags_tag ON note_tags(tag)"));
}

void SQLiteIndex::rebuildIndex(const QString &vaultRoot)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral("DELETE FROM notes_fts"));
    query.exec(QStringLiteral("DELETE FROM links"));
    query.exec(QStringLiteral("DELETE FROM note_tags"));

    VaultScanner scanner;
    FileSystemAdapter fs;
    auto notes = scanner.scan(vaultRoot);

    for (const auto &meta : notes) {
        auto content = fs.readFile(meta.absolutePath(vaultRoot));
        if (!content.has_value()) continue;

        QString title = meta.nameFromPath();
        const QString &text = content.value();

        // FTS5
        query.prepare(QStringLiteral(
            "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
        query.addBindValue(meta.relativePath);
        query.addBindValue(title);
        query.addBindValue(text);
        query.exec();

        // Links and tags
        extractAndInsertLinks(meta.relativePath, text);
        extractAndInsertTags(meta.relativePath, text);
    }

    Q_EMIT indexReady();
}

void SQLiteIndex::indexNote(const QString &relativePath, const QString &title, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Remove existing entries (upsert)
    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM links WHERE source_path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM note_tags WHERE note_path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    // Insert FTS5
    query.prepare(QStringLiteral(
        "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
    query.addBindValue(relativePath);
    query.addBindValue(title);
    query.addBindValue(content);
    query.exec();

    // Extract and insert links + tags
    extractAndInsertLinks(relativePath, content);
    extractAndInsertTags(relativePath, content);
}

void SQLiteIndex::removeNote(const QString &relativePath)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM links WHERE source_path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM note_tags WHERE note_path = ?"));
    query.addBindValue(relativePath);
    query.exec();
}

QVector<SearchMatch> SQLiteIndex::search(const QString &query, int maxResults) const
{
    QVector<SearchMatch> results;
    if (query.trimmed().isEmpty() || !m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT path, snippet(notes_fts, 2, '<b>', '</b>', '...', 32), rank "
        "FROM notes_fts WHERE notes_fts MATCH ? "
        "ORDER BY rank LIMIT ?"));
    q.addBindValue(query);
    q.addBindValue(maxResults);

    if (!q.exec()) return results;

    while (q.next()) {
        SearchMatch match;
        match.notePath = q.value(0).toString();
        match.snippet = q.value(1).toString();
        match.score = q.value(2).toDouble();
        results.append(match);
    }

    return results;
}

// --- Link queries ---

QVector<LinkInfo> SQLiteIndex::backlinksFor(const QString &targetPath) const
{
    QVector<LinkInfo> results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT source_path, target_path, link_type, display_text "
        "FROM links WHERE target_path = ?"));
    q.addBindValue(targetPath);
    if (!q.exec()) return results;

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        results.append(info);
    }
    return results;
}

QVector<LinkInfo> SQLiteIndex::outlinksFor(const QString &sourcePath) const
{
    QVector<LinkInfo> results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT source_path, target_path, link_type, display_text "
        "FROM links WHERE source_path = ?"));
    q.addBindValue(sourcePath);
    if (!q.exec()) return results;

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        results.append(info);
    }
    return results;
}

QVector<QString> SQLiteIndex::orphanLinks() const
{
    QVector<QString> results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    // Find targets that are linked to but don't exist as indexed notes
    q.exec(QStringLiteral(
        "SELECT DISTINCT l.target_path FROM links l "
        "LEFT JOIN notes_fts n ON l.target_path = n.path "
        "WHERE n.path IS NULL"));

    while (q.next()) {
        results.append(q.value(0).toString());
    }
    return results;
}

// --- Tag queries ---

QStringList SQLiteIndex::allTags() const
{
    QStringList results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec(QStringLiteral("SELECT DISTINCT tag FROM note_tags ORDER BY tag"));

    while (q.next()) {
        results.append(q.value(0).toString());
    }
    return results;
}

QStringList SQLiteIndex::notesWithTag(const QString &tag) const
{
    QStringList results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT note_path FROM note_tags WHERE tag = ?"));
    q.addBindValue(tag);
    if (!q.exec()) return results;

    while (q.next()) {
        results.append(q.value(0).toString());
    }
    return results;
}

// --- Link repair ---

int SQLiteIndex::repairLinks(const QString &oldTargetPath, const QString &newTargetPath,
                              const QString &vaultRoot)
{
    if (!m_isOpen) return 0;

    // Find all notes that link to the old target
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT DISTINCT source_path FROM links WHERE target_path = ?"));
    q.addBindValue(oldTargetPath);
    if (!q.exec()) return 0;

    QStringList sourcePaths;
    while (q.next()) {
        sourcePaths.append(q.value(0).toString());
    }

    if (sourcePaths.isEmpty()) return 0;

    // Derive old/new note names for replacement
    auto nameFromPath = [](const QString &path) -> QString {
        QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) {
            name.chop(3);
        }
        return name;
    };

    QString oldName = nameFromPath(oldTargetPath);
    QString newName = nameFromPath(newTargetPath);

    FileSystemAdapter fs;
    int modifiedCount = 0;

    for (const auto &sourcePath : sourcePaths) {
        QString absPath = vaultRoot + QLatin1Char('/') + sourcePath;
        auto content = fs.readFile(absPath);
        if (!content.has_value()) continue;

        QString text = content.value();
        QString original = text;

        // Replace wikilinks: [[OldName]] → [[NewName]]
        // Also handles [[OldName|display]] and [[OldName#heading]]
        QRegularExpression wikiPattern(
            QStringLiteral(R"(\[\[)") + QRegularExpression::escape(oldName) +
            QStringLiteral(R"(([\]|#]))"));
        text.replace(wikiPattern, QStringLiteral("[[") + newName + QStringLiteral("\\1"));

        // Replace markdown links: [text](old/path.md) → [text](new/path.md)
        text.replace(
            QStringLiteral("](") + oldTargetPath + QStringLiteral(")"),
            QStringLiteral("](") + newTargetPath + QStringLiteral(")"));

        if (text != original) {
            fs.writeFile(absPath, text);

            // Re-index the modified source note
            QString title = nameFromPath(sourcePath);
            indexNote(sourcePath, title, text);
            ++modifiedCount;
        }
    }

    // Update links table directly for the target change
    q.prepare(QStringLiteral("UPDATE links SET target_path = ? WHERE target_path = ?"));
    q.addBindValue(newTargetPath);
    q.addBindValue(oldTargetPath);
    q.exec();

    return modifiedCount;
}

// --- Private extraction methods ---

void SQLiteIndex::extractAndInsertLinks(const QString &sourcePath, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Track code blocks to exclude links inside them
    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^```)"));
    bool inCodeBlock = false;

    // Patterns
    static const QRegularExpression embedPattern(QStringLiteral(R"(!\[\[([^\]|]+)\]\])"));
    static const QRegularExpression wikiAliasPattern(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])"));
    static const QRegularExpression wikiPattern(QStringLiteral(R"(\[\[([^\]|]+)\]\])"));
    static const QRegularExpression mdLinkPattern(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+\.md)\))"));

    const auto lines = content.split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        if (codeFencePattern.match(line).hasMatch()) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock) continue;

        // Embeds: ![[target]]
        auto it = embedPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            QString target = match.captured(1);
            // Don't append .md for embeds — they might be images/media
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text) "
                "VALUES(?, ?, 'embed', NULL)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.exec();
        }

        // Wikilinks with alias: [[target|display]]
        it = wikiAliasPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            QString target = resolveTarget(match.captured(1));
            QString display = match.captured(2);
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text) "
                "VALUES(?, ?, 'wiki', ?)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.addBindValue(display);
            query.exec();
        }

        // Plain wikilinks: [[target]] (must exclude already-matched alias patterns)
        // Use a copy of the line with alias patterns removed to avoid double-matching
        QString lineWithoutAliases = line;
        lineWithoutAliases.replace(wikiAliasPattern, QString());
        // Also remove embeds to avoid double-matching
        lineWithoutAliases.replace(embedPattern, QString());

        it = wikiPattern.globalMatch(lineWithoutAliases);
        while (it.hasNext()) {
            auto match = it.next();
            QString target = resolveTarget(match.captured(1));
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text) "
                "VALUES(?, ?, 'wiki', NULL)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.exec();
        }

        // Markdown links: [text](path.md)
        it = mdLinkPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            QString target = match.captured(2);
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text) "
                "VALUES(?, ?, 'markdown', NULL)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.exec();
        }
    }
}

void SQLiteIndex::extractAndInsertTags(const QString &notePath, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^```)"));
    static const QRegularExpression tagPattern(
        QStringLiteral(R"((?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*))"));

    bool inCodeBlock = false;
    const auto lines = content.split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        if (codeFencePattern.match(line).hasMatch()) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock) continue;

        auto it = tagPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO note_tags(note_path, tag) VALUES(?, ?)"));
            query.addBindValue(notePath);
            query.addBindValue(match.captured(1));
            query.exec();
        }
    }
}

QString SQLiteIndex::resolveTarget(const QString &rawTarget)
{
    QString target = rawTarget;

    // Strip heading/block reference: [[Note#heading]] → Note
    int hashPos = target.indexOf(QLatin1Char('#'));
    if (hashPos >= 0) {
        target = target.left(hashPos);
    }

    target = target.trimmed();

    // Append .md if no extension
    if (!target.contains(QLatin1Char('.'))) {
        target += QStringLiteral(".md");
    }

    return target;
}

} // namespace Corbomite
```

- [ ] **Step 4: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_sqliteindex --output-on-failure
```

Expected: All SQLiteIndex tests pass (8 existing + 14 new = 22 total).

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`

Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/storage/ tests/storage/tst_sqliteindex.cpp
git commit -m "feat: extend SQLiteIndex with link tracking, tag index, and backlink queries

New tables: links (source→target with type) and note_tags.
Link extraction from wikilinks [[]], embeds ![[]], markdown [](path.md).
Tag extraction with code block exclusion.
Queries: backlinksFor, outlinksFor, orphanLinks, allTags, notesWithTag.
14 new test cases covering extraction, queries, and edge cases."
```

---

### Task 2: Link Repair on Rename

**Files:**
- Modify: `tests/storage/tst_sqliteindex.cpp`

- [ ] **Step 1: Write link repair tests**

Add to `tests/storage/tst_sqliteindex.cpp`:

```cpp
    void testRepairLinksBasic()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        QDir().mkpath(vault);

        // Create source note that links to target
        QFile src(vault + "/source.md");
        src.open(QIODevice::WriteOnly);
        src.write("See [[OldNote]] for details.\n");
        src.close();

        // Create the target note
        QFile target(vault + "/OldNote.md");
        target.open(QIODevice::WriteOnly);
        target.write("I am the target.\n");
        target.close();

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        // Verify link exists
        QCOMPARE(index.backlinksFor(QStringLiteral("OldNote.md")).size(), 1);

        // Rename the target
        QFile::rename(vault + "/OldNote.md", vault + "/NewNote.md");

        // Repair links
        int modified = index.repairLinks(
            QStringLiteral("OldNote.md"),
            QStringLiteral("NewNote.md"),
            vault);

        QCOMPARE(modified, 1);

        // Verify source file content was updated
        QFile updated(vault + "/source.md");
        updated.open(QIODevice::ReadOnly);
        QString content = QString::fromUtf8(updated.readAll());
        QVERIFY(content.contains(QStringLiteral("[[NewNote]]")));
        QVERIFY(!content.contains(QStringLiteral("[[OldNote]]")));

        // Verify link index was updated
        QCOMPARE(index.backlinksFor(QStringLiteral("OldNote.md")).size(), 0);
        QCOMPARE(index.backlinksFor(QStringLiteral("NewNote.md")).size(), 1);
    }

    void testRepairLinksWithAlias()
    {
        QTemporaryDir tmp;
        QString vault = tmp.path() + "/vault";
        QDir().mkpath(vault);

        QFile src(vault + "/source.md");
        src.open(QIODevice::WriteOnly);
        src.write("See [[OldNote|click here]] for info.\n");
        src.close();

        QFile target(vault + "/OldNote.md");
        target.open(QIODevice::WriteOnly);
        target.write("Target.\n");
        target.close();

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(vault);

        QFile::rename(vault + "/OldNote.md", vault + "/NewNote.md");
        index.repairLinks(QStringLiteral("OldNote.md"), QStringLiteral("NewNote.md"), vault);

        QFile updated(vault + "/source.md");
        updated.open(QIODevice::ReadOnly);
        QString content = QString::fromUtf8(updated.readAll());
        QVERIFY(content.contains(QStringLiteral("[[NewNote|click here]]")));
    }

    void testRepairLinksNoMatchReturnsZero()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("No links here"));

        int modified = index.repairLinks(
            QStringLiteral("nonexistent.md"),
            QStringLiteral("other.md"),
            tmp.path());
        QCOMPARE(modified, 0);
    }
```

- [ ] **Step 2: Build and run tests**

Run:
```bash
cmake --build build && cd build && ctest -R tst_sqliteindex --output-on-failure
```

Expected: All tests pass (existing + new repair tests).

- [ ] **Step 3: Run all tests**

Run: `cd build && ctest --output-on-failure`

- [ ] **Step 4: Commit**

```bash
git add tests/storage/tst_sqliteindex.cpp
git commit -m "feat: add link repair tests — rename updates wikilinks in source notes"
```

---

### Task 3: NoteService Integration + VaultModel::allTags Refactor

**Files:**
- Modify: `libs/models/include/corbomite/models/NoteService.h`
- Modify: `libs/models/src/NoteService.cpp`
- Modify: `libs/models/include/corbomite/models/VaultModel.h`
- Modify: `libs/models/src/VaultModel.cpp`

- [ ] **Step 1: Add SQLiteIndex to NoteService for link repair**

In `libs/models/include/corbomite/models/NoteService.h`, add:

```cpp
class SQLiteIndex;  // forward declaration

// In the class, add:
    void setSearchIndex(SQLiteIndex *index);
// Add private member:
    SQLiteIndex *m_searchIndex = nullptr;
```

In `libs/models/src/NoteService.cpp`, add `#include "corbomite/storage/SQLiteIndex.h"` and implement:

```cpp
void NoteService::setSearchIndex(SQLiteIndex *index)
{
    m_searchIndex = index;
}
```

Update `NoteService::renameNote()` to call link repair:

```cpp
bool NoteService::renameNote(const QString &oldRelPath, const QString &newRelPath)
{
    FileSystemAdapter fs;
    QString oldAbs = m_vault->path() + QLatin1Char('/') + oldRelPath;
    QString newAbs = m_vault->path() + QLatin1Char('/') + newRelPath;

    if (!fs.rename(oldAbs, newAbs)) {
        return false;
    }

    // Repair links in other notes that reference the old path
    if (m_searchIndex) {
        m_searchIndex->repairLinks(oldRelPath, newRelPath, m_vault->path());
    }

    m_vault->renameNote(oldRelPath, newRelPath);
    return true;
}
```

- [ ] **Step 2: Refactor VaultModel::allTags() to use SQLiteIndex**

In `libs/models/include/corbomite/models/VaultModel.h`, add:

```cpp
class SQLiteIndex;  // forward declaration

// Add public method:
    void setSearchIndex(SQLiteIndex *index);
// Add private member:
    SQLiteIndex *m_searchIndex = nullptr;
```

In `libs/models/src/VaultModel.cpp`, add `#include "corbomite/storage/SQLiteIndex.h"` and update `allTags()`:

```cpp
void VaultModel::setSearchIndex(SQLiteIndex *index)
{
    m_searchIndex = index;
}

QStringList VaultModel::allTags() const
{
    // Prefer indexed tags (fast) over filesystem scan (slow)
    if (m_searchIndex) {
        return m_searchIndex->allTags();
    }

    // Fallback to filesystem scan (existing implementation)
    if (!m_tagCacheDirty) {
        return m_cachedTags;
    }
    // ... existing filesystem scan code ...
}
```

- [ ] **Step 3: Wire in MainWindow**

In `src/app/MainWindow.cpp`, in `onVaultOpened()`, after creating the search index, add:

```cpp
    m_vaultService->noteService()->setSearchIndex(m_searchIndex);
    m_vaultService->vault()->setSearchIndex(m_searchIndex);
```

In `onVaultClosed()`, before deleting:

```cpp
    m_vaultService->noteService()->setSearchIndex(nullptr);
    m_vaultService->vault()->setSearchIndex(nullptr);
```

- [ ] **Step 4: Build and run all tests**

Run:
```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add libs/models/ src/app/MainWindow.cpp
git commit -m "feat: integrate link repair into NoteService rename, refactor allTags to use index

NoteService::renameNote() now calls SQLiteIndex::repairLinks() to
update wikilinks in all notes referencing the renamed note.
VaultModel::allTags() delegates to SQLiteIndex when available
(fast SQL query vs slow filesystem scan)."
```

---

Self-review:

1. **Spec coverage:** Links table ✓. Tags table ✓. Wiki/alias/embed/markdown extraction ✓. Code block exclusion ✓. Heading fragment stripping ✓. backlinksFor ✓. outlinksFor ✓. orphanLinks ✓. allTags ✓. notesWithTag ✓. repairLinks ✓. indexNote extended ✓. removeNote extended ✓. rebuildIndex extended ✓. VaultModel::allTags refactor ✓. NoteService integration ✓. MainWindow wiring ✓.

2. **Placeholder scan:** All code complete. No TBDs.

3. **Type consistency:** `LinkInfo` struct fields match SQL columns and query results. `resolveTarget()` strips `#heading` and appends `.md` consistently. `repairLinks()` uses `FileSystemAdapter` (existing dependency). `extractAndInsertLinks/Tags` called from both `indexNote` and `rebuildIndex`.
