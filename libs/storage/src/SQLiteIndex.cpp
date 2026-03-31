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
    query.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5("
        "path, title, content, tokenize = 'porter unicode61'"
        ")"));
}

void SQLiteIndex::rebuildIndex(const QString &vaultRoot)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral("DELETE FROM notes_fts"));

    VaultScanner scanner;
    FileSystemAdapter fs;
    auto notes = scanner.scan(vaultRoot);

    for (const auto &meta : notes) {
        auto content = fs.readFile(meta.absolutePath(vaultRoot));
        if (!content.has_value()) continue;

        QString title = meta.nameFromPath();

        query.prepare(QStringLiteral(
            "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
        query.addBindValue(meta.relativePath);
        query.addBindValue(title);
        query.addBindValue(content.value());
        query.exec();
    }

    Q_EMIT indexReady();
}

void SQLiteIndex::indexNote(const QString &relativePath, const QString &title, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Remove existing entry first (upsert)
    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    // Insert new entry
    query.prepare(QStringLiteral(
        "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
    query.addBindValue(relativePath);
    query.addBindValue(title);
    query.addBindValue(content);
    query.exec();
}

void SQLiteIndex::removeNote(const QString &relativePath)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
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

} // namespace Corbomite
