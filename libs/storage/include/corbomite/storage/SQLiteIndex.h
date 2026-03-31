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
