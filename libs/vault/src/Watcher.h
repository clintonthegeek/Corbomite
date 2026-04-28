// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace Corbomite {
class Vault;

namespace detail {

/// Private Vault helper. Observes the filesystem under the vault's base path
/// and emits `created` / `modified` / `deleted` / `renamed` signals at
/// relative paths. Vault owns one instance and translates these into tree
/// mutations + public signals. Namespace is `Corbomite::detail::` to keep
/// the class out of the public API surface (header is in `src/`, not
/// `include/`).
///
/// Task 2.2 ships only the skeleton + signal declarations (stub behaviour).
/// Task 2.4 fills in the full directory-traversal + diff-emission logic.
class Watcher : public QObject
{
    Q_OBJECT
public:
    explicit Watcher(Vault *vault, QObject *parent = nullptr);
    ~Watcher() override;

    void start(const QString &basePath);
    void stop();

signals:
    void created(const QString &relPath);
    void modified(const QString &relPath);
    void deleted(const QString &relPath);
    void renamed(const QString &oldRelPath, const QString &newRelPath);

    /// Cluster B Phase 3 — fired on every detected mutation regardless
    /// of path (vault content or `.obsidian/` plugin config). Vault
    /// converts this into the public `Vault::raw` signal; for
    /// `.obsidian/*.json` paths Vault additionally emits `configChanged`.
    void rawChange(const QString &relPath);

private Q_SLOTS:
    void onDirChanged(const QString &absDir);
    void onFileChanged(const QString &absPath);
    void drainPending();

private:
    Vault               *m_vault;
    QFileSystemWatcher   m_fsw;
    QString              m_basePath;
    QTimer               m_drainTimer;
    QHash<QString, qint64> m_knownFiles;
    QStringList          m_pending;

    void snapshotDirectory(const QString &absDir);
    QString toRel(const QString &abs) const;
};

} // namespace detail
} // namespace Corbomite
