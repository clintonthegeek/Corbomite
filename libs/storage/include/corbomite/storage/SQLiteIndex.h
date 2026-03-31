// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Corbomite {

struct SearchMatch {
    QString notePath;
    QString snippet;
    double score = 0.0;
};

class SQLiteIndex : public QObject {
    Q_OBJECT

public:
    explicit SQLiteIndex(QObject *parent = nullptr);
    ~SQLiteIndex() override;

    bool open(const QString &dbPath);
    void close();

    void rebuildIndex(const QString &vaultRoot);
    // TODO: Move to background thread for large vaults (>1000 notes)
    void indexNote(const QString &relativePath, const QString &title, const QString &content);
    void removeNote(const QString &relativePath);

    QVector<SearchMatch> search(const QString &query, int maxResults = 100) const;
    // TODO: Support Obsidian search operators (file:, path:, tag:, line:, regex)

Q_SIGNALS:
    void indexReady();

private:
    void createTables();
    QString m_connectionName;
    bool m_isOpen = false;
};

} // namespace Corbomite
