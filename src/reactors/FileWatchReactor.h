// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>

namespace Corbomite {

class VaultModel;

class FileWatchReactor : public QObject {
    Q_OBJECT

public:
    explicit FileWatchReactor(VaultModel *vault, QObject *parent = nullptr);

    void startWatching(const QString &vaultRoot);
    void stopWatching();
    void suppressPath(const QString &absolutePath);

Q_SIGNALS:
    void fileModifiedExternally(const QString &relativePath);
    void fileCreatedExternally(const QString &relativePath);
    void fileDeletedExternally(const QString &relativePath);

private:
    void onDirectoryChanged(const QString &path);
    void processPendingChanges();
    bool shouldExclude(const QString &name) const;

    VaultModel *m_vault;
    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QSet<QString> m_pendingDirs;
    QSet<QString> m_suppressedPaths;
    QString m_vaultRoot;
    QSet<QString> m_knownFiles; // Track known files to detect adds/deletes
};

} // namespace Corbomite
