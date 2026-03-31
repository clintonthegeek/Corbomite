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

QVector<LinkInfo> SQLiteIndex::allLinks() const
{
    QVector<LinkInfo> results;
    if (!m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.exec(QStringLiteral(
        "SELECT source_path, target_path, link_type, display_text FROM links"));

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

        // Replace wikilinks: [[OldName]] -> [[NewName]]
        // Also handles [[OldName|display]] and [[OldName#heading]]
        QRegularExpression wikiPattern(
            QStringLiteral(R"(\[\[)") + QRegularExpression::escape(oldName) +
            QStringLiteral(R"(([\]|#]))"));
        text.replace(wikiPattern, QStringLiteral("[[") + newName + QStringLiteral("\\1"));

        // Replace markdown links: [text](old/path.md) -> [text](new/path.md)
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

    // Strip heading/block reference: [[Note#heading]] -> Note
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
