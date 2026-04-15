// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultScanner.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/core/NoteMeta.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QThread>
#include <QCoreApplication>
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

    // Enable WAL mode for concurrent reads during background indexing
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));

    createTables();
    m_dbPath = dbPath;
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

    // Schema migration: bumping user_version drops + recreates link/tag
    // tables so subpath-aware schema takes effect on existing vaults.
    // FTS data survives (it's cheap to rebuild from scan but also schema-stable).
    {
        QSqlQuery vq(QSqlDatabase::database(m_connectionName));
        int schemaVersion = 0;
        if (vq.exec(QStringLiteral("PRAGMA user_version")) && vq.next()) {
            schemaVersion = vq.value(0).toInt();
        }
        if (schemaVersion < 1) {
            vq.exec(QStringLiteral("DROP TABLE IF EXISTS links"));
            vq.exec(QStringLiteral("DROP TABLE IF EXISTS note_tags"));
            vq.exec(QStringLiteral("PRAGMA user_version = 1"));
        }
    }

    // Link relationships — schema v1 adds subpath ("#heading" or "#^block"),
    // making it part of the primary key so [[Note#A]] and [[Note#B]] from the
    // same source are distinct rows.
    query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS links ("
        "source_path TEXT NOT NULL, "
        "target_path TEXT NOT NULL, "
        "link_type TEXT NOT NULL, "
        "display_text TEXT, "
        "subpath TEXT NOT NULL DEFAULT '', "
        "PRIMARY KEY (source_path, target_path, link_type, subpath)"
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

    // Use a transaction for much faster bulk insertion
    query.exec(QStringLiteral("BEGIN TRANSACTION"));

    VaultScanner scanner;
    FileSystemAdapter fs;
    auto notes = scanner.scan(vaultRoot);

    // Seed the 6-step resolver with the full vault.
    QStringList allPaths;
    allPaths.reserve(notes.size());
    for (const auto &meta : notes) {
        allPaths.append(meta.relativePath);
    }
    m_resolver.setVaultPaths(allPaths);

    int count = 0;
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

        // Keep UI responsive during large vault indexing
        if (++count % 100 == 0) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    query.exec(QStringLiteral("COMMIT"));

    Q_EMIT indexReady();
}

// Background indexing creates a temporary SQLiteIndex in a QThread.
// It opens its own connection, does the full rebuild, then signals completion.
// WAL mode (set in open()) allows the main thread to read concurrently.

void SQLiteIndex::rebuildIndexAsync(const QString &vaultRoot)
{
    if (m_workerThread) {
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    const QString dbPath = m_dbPath;

    m_workerThread = QThread::create([dbPath, vaultRoot]() {
        // Create a standalone SQLiteIndex in the worker thread — it manages
        // its own connection and uses the proven synchronous rebuildIndex().
        SQLiteIndex worker;
        if (worker.open(dbPath)) {
            worker.rebuildIndex(vaultRoot);
            worker.close();
        }
    });

    connect(m_workerThread, &QThread::finished, this, [this]() {
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
        Q_EMIT indexReady();
    });

    m_workerThread->start();
}

bool SQLiteIndex::isRebuilding() const
{
    return m_workerThread != nullptr && m_workerThread->isRunning();
}

void SQLiteIndex::indexNote(const QString &relativePath, const QString &title, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Ensure the resolver knows about this path (no-op if already present).
    m_resolver.addVaultPath(relativePath);

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

    m_resolver.removeVaultPath(relativePath);

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

namespace {

// FTS5's snippet() function wraps hits in caller-supplied delimiters
// (we use `<b>` / `</b>`). Strip them out and translate their positions
// into a merge-sorted, non-overlapping QVector<QPair<int,int>> over the
// cleaned text so SearchMatch.matches lines up with what the panel renders.
struct StrippedSnippet {
    QString text;
    QVector<QPair<int, int>> matches;
};

StrippedSnippet stripBoldMarkup(const QString &raw)
{
    static const QString openTag = QStringLiteral("<b>");
    static const QString closeTag = QStringLiteral("</b>");
    StrippedSnippet out;
    out.text.reserve(raw.size());
    int i = 0;
    while (i < raw.size()) {
        if (raw.mid(i, openTag.size()) == openTag) {
            const int rangeStart = out.text.size();
            i += openTag.size();
            while (i < raw.size() && raw.mid(i, closeTag.size()) != closeTag) {
                out.text.append(raw.at(i));
                ++i;
            }
            if (i < raw.size()) i += closeTag.size();
            const int rangeEnd = out.text.size();
            if (rangeEnd > rangeStart) out.matches.append({rangeStart, rangeEnd});
        } else {
            out.text.append(raw.at(i));
            ++i;
        }
    }
    return out;
}

} // namespace

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
        const auto snippet = stripBoldMarkup(q.value(1).toString());
        match.snippet = snippet.text;
        match.matches = snippet.matches;
        match.score = q.value(2).toDouble();
        results.append(match);
    }

    return results;
}

QVector<SearchMatch> SQLiteIndex::searchCompiled(const QString &fts5Query,
                                                  const QStringList &requiredTags,
                                                  const QStringList &excludedTags,
                                                  int maxResults) const
{
    QVector<SearchMatch> results;
    if (!m_isOpen) return results;
    if (fts5Query.isEmpty() && requiredTags.isEmpty() && excludedTags.isEmpty()) {
        return results;
    }

    QString sql;
    QVariantList binds;

    if (!fts5Query.isEmpty()) {
        sql = QStringLiteral(
            "SELECT path, snippet(notes_fts, 2, '<b>', '</b>', '...', 32), rank "
            "FROM notes_fts WHERE notes_fts MATCH ?");
        binds.append(fts5Query);
    } else {
        // Tag-only query — pull every note that survives the tag predicates,
        // synthesizing a default rank/snippet so callers can still rank them.
        sql = QStringLiteral(
            "SELECT path, '' AS snippet, 0 AS rank FROM notes_fts WHERE 1=1");
    }

    for (const QString &tag : requiredTags) {
        sql += QStringLiteral(
            " AND path IN (SELECT note_path FROM note_tags WHERE tag = ?)");
        binds.append(tag);
    }
    for (const QString &tag : excludedTags) {
        sql += QStringLiteral(
            " AND path NOT IN (SELECT note_path FROM note_tags WHERE tag = ?)");
        binds.append(tag);
    }

    sql += fts5Query.isEmpty()
        ? QStringLiteral(" ORDER BY path LIMIT ?")
        : QStringLiteral(" ORDER BY rank LIMIT ?");
    binds.append(maxResults);

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(sql);
    for (const QVariant &v : binds) q.addBindValue(v);
    if (!q.exec()) return results;

    while (q.next()) {
        SearchMatch match;
        match.notePath = q.value(0).toString();
        const auto snippet = stripBoldMarkup(q.value(1).toString());
        match.snippet = snippet.text;
        match.matches = snippet.matches;
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
        "SELECT source_path, target_path, link_type, display_text, subpath "
        "FROM links WHERE target_path = ?"));
    q.addBindValue(targetPath);
    if (!q.exec()) return results;

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        info.subpath = q.value(4).toString();
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
        "SELECT source_path, target_path, link_type, display_text, subpath "
        "FROM links WHERE source_path = ?"));
    q.addBindValue(sourcePath);
    if (!q.exec()) return results;

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        info.subpath = q.value(4).toString();
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
        "SELECT source_path, target_path, link_type, display_text, subpath FROM links"));

    while (q.next()) {
        LinkInfo info;
        info.sourcePath = q.value(0).toString();
        info.targetPath = q.value(1).toString();
        info.linkType = q.value(2).toString();
        info.displayText = q.value(3).toString();
        info.subpath = q.value(4).toString();
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

// Normalise an unresolved wikilink target: strip subpath, append .md if no
// extension. Preserves the legacy behaviour where unresolved links appeared
// in the graph as "Name.md" nodes.
static QString unresolvedTargetNormalized(const QString &raw)
{
    QString s = raw;
    const int hashPos = s.indexOf(QLatin1Char('#'));
    if (hashPos >= 0) s = s.left(hashPos);
    s = s.trimmed();
    if (!s.contains(QLatin1Char('.'))) s += QStringLiteral(".md");
    return s;
}

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
    static const QRegularExpression inlineCodePattern(QStringLiteral(R"(`[^`]+`)"));

    const auto lines = content.split(QLatin1Char('\n'));
    for (const auto &rawLine : lines) {
        if (codeFencePattern.match(rawLine).hasMatch()) {
            inCodeBlock = !inCodeBlock;
            continue;
        }
        if (inCodeBlock) continue;

        // Strip inline code spans so wikilinks inside `backticks` are ignored
        QString line = rawLine;
        line.replace(inlineCodePattern, QString());

        // Embeds: ![[target]] — media filenames (images, PDFs) pass through
        // verbatim; note-embeds (e.g. ![[Note#Section]]) route through the
        // resolver so the subpath is captured.
        auto it = embedPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            const QString raw = match.captured(1);
            QString target = raw;
            QString subpath;
            // If the raw target contains '#' or has no extension other than .md,
            // treat as a note-embed and resolve.
            const bool looksLikeNote = raw.contains(QLatin1Char('#'))
                || !raw.contains(QLatin1Char('.'))
                || raw.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive);
            if (looksLikeNote) {
                const auto resolved = m_resolver.resolve(sourcePath, raw);
                if (resolved.resolved) target = resolved.path;
                subpath = resolved.subpath;
            }
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text, subpath) "
                "VALUES(?, ?, 'embed', NULL, ?)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.addBindValue(subpath.isNull() ? QStringLiteral("") : subpath);
            query.exec();
        }

        // Wikilinks with alias: [[target|display]]
        it = wikiAliasPattern.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            const auto resolved = m_resolver.resolve(sourcePath, match.captured(1));
            QString target = resolved.resolved
                ? resolved.path
                : unresolvedTargetNormalized(match.captured(1));
            QString display = match.captured(2);
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text, subpath) "
                "VALUES(?, ?, 'wiki', ?, ?)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.addBindValue(display);
            // QString() binds as NULL; ensure empty literal so NOT NULL holds.
            query.addBindValue(resolved.subpath.isNull() ? QStringLiteral("") : resolved.subpath);
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
            const auto resolved = m_resolver.resolve(sourcePath, match.captured(1));
            QString target = resolved.resolved
                ? resolved.path
                : unresolvedTargetNormalized(match.captured(1));
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text, subpath) "
                "VALUES(?, ?, 'wiki', NULL, ?)"));
            query.addBindValue(sourcePath);
            query.addBindValue(target);
            query.addBindValue(resolved.subpath.isNull() ? QStringLiteral("") : resolved.subpath);
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

} // namespace Corbomite
