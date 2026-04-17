// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "RecentVaults.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class PluginManager;

/// Top-level application object. Owns process-wide state (the
/// RecentVaults helper, the PluginManager) and drives the vault
/// open/close lifecycle as a pair of signals — the actual canonical
/// `Corbomite::Vault` + related data-layer objects are owned by
/// `MainWindow`, which constructs them from the `path` argument to
/// `vaultOpened`.
///
/// During Q.0 Phase 8 this absorbed `VaultService`; Phase 10 retired
/// `VaultModel` entirely so `CorbomiteApp` no longer owns any vault-
/// side state beyond the "is a vault currently open" flag. Cluster Q
/// Phase 2 added `PluginManager` ownership so plugin discovery +
/// enabled-state restore happens once at app construction.
class CorbomiteApp : public QObject {
    Q_OBJECT
public:
    explicit CorbomiteApp(QObject *parent = nullptr);
    ~CorbomiteApp() override;

    bool openVault(const QString &path);
    void closeVault();
    bool isOpen() const;
    QString currentVaultPath() const;

    /// Read-through accessor — also refreshes stale entries from the
    /// backing store so the welcome-screen list stays live across
    /// in-process opens (the KRecentFilesAction wired in MainWindow
    /// writes the same file).
    QStringList recentVaults();

    /// Process-wide plugin manager. Always non-null after construction.
    PluginManager *pluginManager() const { return m_pluginManager; }

Q_SIGNALS:
    void vaultOpened(const QString &path);
    void vaultClosed();

private:
    QString m_currentPath;
    RecentVaults m_recentVaults;
    PluginManager *m_pluginManager = nullptr;
};

} // namespace Corbomite
