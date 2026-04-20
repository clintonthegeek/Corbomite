// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/SQLiteIndex.h"

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/MetadataCache.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

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
    m_vaultRoot.clear();
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
}

void SQLiteIndex::setVaultRoot(const QString &vaultRoot)
{
    m_vaultRoot = vaultRoot;
}

void SQLiteIndex::setMetadataCache(MetadataCache *cache)
{
    if (m_cache) {
        disconnect(m_cache, nullptr, this, nullptr);
    }
    m_cache = cache;
    if (m_cache) {
        connect(m_cache, &MetadataCache::cacheChanged,
                this, &SQLiteIndex::onMetadataCacheChanged);
        connect(m_cache, &MetadataCache::cacheDeleted,
                this, &SQLiteIndex::onMetadataCacheDeleted);

        // Reconcile now: MetadataCache may already hold persisted entries
        // loaded silently via installPersistedState (no cacheChanged fires
        // for those). Without this, any index row lost to a schema
        // migration stays lost until the file is edited.
        reconcileWithCache();
    }
}

void SQLiteIndex::reconcileWithCache()
{
    if (!m_isOpen || !m_cache) return;

    const QStringList paths = m_cache->allPaths();
    for (const QString &path : paths) {
        // Empty hash => tracked-unsupported (non-.md). No FTS/link rows to
        // write for those; MetadataCache never emits cacheChanged for them
        // either, so skipping here matches the event-driven path.
        if (m_cache->getFileHash(path).isEmpty()) continue;
        const std::optional<CachedMetadata> cache = m_cache->getFileCache(path);
        if (!cache) continue;
        m_resolver.addVaultPath(path);
        writeRowsFromCache(path, *cache);
    }
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

// --- MetadataCache subscription slots ---

void SQLiteIndex::onMetadataCacheChanged(const QString &path,
                                         const QString &prevHash,
                                         const CachedMetadata &cache)
{
    Q_UNUSED(prevHash);
    if (!m_isOpen) return;

    // Ensure the resolver sees this path (no-op if already present). Only
    // needed if legacy consumers call the read API that depends on it,
    // but harmless.
    m_resolver.addVaultPath(path);

    writeRowsFromCache(path, cache);
}

void SQLiteIndex::onMetadataCacheDeleted(const QString &path,
                                         const CachedMetadata &prevCache)
{
    Q_UNUSED(prevCache);
    if (!m_isOpen) return;

    m_resolver.removeVaultPath(path);
    deleteRowsForPath(path);
}

void SQLiteIndex::deleteRowsForPath(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(path);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM links WHERE source_path = ?"));
    query.addBindValue(path);
    query.exec();

    query.prepare(QStringLiteral("DELETE FROM note_tags WHERE note_path = ?"));
    query.addBindValue(path);
    query.exec();
}

void SQLiteIndex::writeRowsFromCache(const QString &path, const CachedMetadata &cache)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    // Read body from disk. MetadataCache's `cacheChanged` doesn't carry
    // the raw content to keep the signal lean; re-reading is cheap and
    // keeps disk + FTS as the single source of truth.
    QString content;
    if (!m_vaultRoot.isEmpty()) {
        const QString fullPath = QDir(m_vaultRoot).absoluteFilePath(path);
        QFile f(fullPath);
        if (f.open(QIODevice::ReadOnly)) {
            content = QString::fromUtf8(f.readAll());
        }
    }

    // Title: first heading if present; else basename.
    QString title;
    if (cache.headings && !cache.headings->isEmpty()) {
        title = cache.headings->first().heading;
    } else {
        title = QFileInfo(path).completeBaseName();
    }

    db.transaction();

    deleteRowsForPath(path);

    QSqlQuery q(db);

    // FTS row.
    q.prepare(QStringLiteral(
        "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
    q.addBindValue(path);
    q.addBindValue(title);
    q.addBindValue(content);
    q.exec();

    // Link rows. `LinkCache.link` is stored as "path#subpath" for resolved
    // targets (or just the raw unresolved target). Split on the first '#'.
    auto splitTarget = [](const QString &storedLink,
                          QString &targetPath,
                          QString &subpath) {
        targetPath = storedLink;
        // Use a non-null empty string so the bound value is "" not NULL
        // (the `subpath` column is NOT NULL DEFAULT '').
        subpath = QStringLiteral("");
        const int hashIdx = targetPath.indexOf(QLatin1Char('#'));
        if (hashIdx >= 0) {
            subpath = targetPath.mid(hashIdx);
            targetPath = targetPath.left(hashIdx);
        }
    };

    if (cache.links) {
        for (const LinkCache &link : *cache.links) {
            QString targetPath;
            QString subpath;
            splitTarget(link.link, targetPath, subpath);
            // CachedMetadata doesn't discriminate wiki vs standard markdown
            // links — MetadataParser::(f) lumps both into `cache.links`.
            // Default to "wiki" since that's the majority case and the
            // current UI/search layer doesn't discriminate. Phase-8 follow-up
            // may reinstate fine-grained typing if needed.
            q.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text, subpath) "
                "VALUES(?, ?, 'wiki', ?, ?)"));
            q.addBindValue(path);
            q.addBindValue(targetPath);
            q.addBindValue(link.displayText.has_value()
                               ? link.displayText.value()
                               : QVariant(QMetaType(QMetaType::QString)));
            q.addBindValue(subpath);
            q.exec();
        }
    }

    if (cache.embeds) {
        for (const LinkCache &link : *cache.embeds) {
            QString targetPath;
            QString subpath;
            splitTarget(link.link, targetPath, subpath);
            q.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO links(source_path, target_path, link_type, display_text, subpath) "
                "VALUES(?, ?, 'embed', ?, ?)"));
            q.addBindValue(path);
            q.addBindValue(targetPath);
            q.addBindValue(link.displayText.has_value()
                               ? link.displayText.value()
                               : QVariant(QMetaType(QMetaType::QString)));
            q.addBindValue(subpath);
            q.exec();
        }
    }

    // Tag rows. CachedMetadata's TagCache.tag includes the leading `#`
    // (mirrors Obsidian's shape). We store it verbatim.
    if (cache.tags) {
        for (const TagCache &tag : *cache.tags) {
            q.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO note_tags(note_path, tag) VALUES(?, ?)"));
            q.addBindValue(path);
            q.addBindValue(tag.tag);
            q.exec();
        }
    }

    db.commit();
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
    return searchCompiled(fts5Query, requiredTags, excludedTags,
                          QStringList{}, QStringList{}, maxResults);
}

QVector<SearchMatch> SQLiteIndex::searchCompiled(const QString &fts5Query,
                                                  const QStringList &requiredTags,
                                                  const QStringList &excludedTags,
                                                  const QStringList &regexPatterns,
                                                  const QStringList &caseSensitiveTerms,
                                                  int maxResults) const
{
    QVector<SearchMatch> results;
    if (!m_isOpen) return results;
    if (fts5Query.isEmpty() && requiredTags.isEmpty() && excludedTags.isEmpty()) {
        return results;
    }

    const bool postFilter = !regexPatterns.isEmpty() || !caseSensitiveTerms.isEmpty();

    // Pre-compile regexes once. Invalid patterns cause the whole query to
    // return no results — failing closed matches Obsidian's behaviour when
    // a `/…/` literal is malformed.
    QVector<QRegularExpression> regexes;
    regexes.reserve(regexPatterns.size());
    for (const QString &pat : regexPatterns) {
        QRegularExpression re(pat);
        if (!re.isValid()) return results;
        regexes.append(re);
    }

    QString sql;
    QVariantList binds;

    // When we post-filter we need the raw content column too. Pull an extra
    // column only in that path so the fast path's projection stays unchanged.
    const QString projection = postFilter
        ? QStringLiteral("path, snippet(notes_fts, 2, '<b>', '</b>', '...', 32), rank, content")
        : QStringLiteral("path, snippet(notes_fts, 2, '<b>', '</b>', '...', 32), rank");

    if (!fts5Query.isEmpty()) {
        sql = QStringLiteral("SELECT %1 FROM notes_fts WHERE notes_fts MATCH ?").arg(projection);
        binds.append(fts5Query);
    } else {
        sql = QStringLiteral("SELECT %1 FROM notes_fts WHERE 1=1").arg(
            postFilter
                ? QStringLiteral("path, '' AS snippet, 0 AS rank, content")
                : QStringLiteral("path, '' AS snippet, 0 AS rank"));
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
    // When post-filtering we may drop many rows; overfetch so small result
    // sets still hit maxResults after filtering.
    const int fetchLimit = postFilter ? qMax(maxResults * 4, 100) : maxResults;
    binds.append(fetchLimit);

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(sql);
    for (const QVariant &v : binds) q.addBindValue(v);
    if (!q.exec()) return results;

    while (q.next()) {
        if (postFilter) {
            const QString content = q.value(3).toString();
            bool keep = true;
            for (const QRegularExpression &re : regexes) {
                if (!re.match(content).hasMatch()) { keep = false; break; }
            }
            if (keep) {
                for (const QString &term : caseSensitiveTerms) {
                    if (!content.contains(term, Qt::CaseSensitive)) {
                        keep = false; break;
                    }
                }
            }
            if (!keep) continue;
        }

        SearchMatch match;
        match.notePath = q.value(0).toString();
        const auto snippet = stripBoldMarkup(q.value(1).toString());
        match.snippet = snippet.text;
        match.matches = snippet.matches;
        match.score = q.value(2).toDouble();
        results.append(match);
        if (results.size() >= maxResults) break;
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

// TODO(Cluster I follow-up): this method directly UPDATEs the links table
// AND rewrites the source notes on disk. Since Cluster I Phase 8, all
// writes to SQLiteIndex's tables go through MetadataCache::cacheChanged
// (SQLiteIndex derives from the cache). The direct UPDATE path here is a
// vestige from pre-Cluster-I. The disk-rewrite (update-wikilinks-in-source)
// is still valid; the UPDATE should be removed and the cache-driven
// re-parse on the source notes' mtime change should repopulate links
// automatically. See docs/cluster-retros/cluster-i.md "Deferred follow-ups".
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
            ++modifiedCount;
            // Note: the re-index of the source note now happens via
            // MetadataCache when the file-watcher picks up the write.
            // Phase 7 keeps this synchronous update of the `links` table
            // below as a best-effort until Phase 8 restructures the flow.
        }
    }

    // Update links table directly for the target change
    q.prepare(QStringLiteral("UPDATE links SET target_path = ? WHERE target_path = ?"));
    q.addBindValue(newTargetPath);
    q.addBindValue(oldTargetPath);
    q.exec();

    return modifiedCount;
}

} // namespace Corbomite
