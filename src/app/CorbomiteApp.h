// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "RecentVaults.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class VaultModel;

/// Top-level application object. Owns the legacy `VaultModel` and
/// drives the vault open/close lifecycle.
///
/// During Q.0 Phase 8 this absorbed what used to live in `VaultService`
/// (deleted in Task 8.3). Task 8.4 folded `NoteService` into VaultModel,
/// so the NoteService getter is gone; call `vault()->createNote(...)`
/// etc. directly. The canonical `Corbomite::Vault` + `FileManager`
/// aggregate itself is still constructed downstream in
/// `MainWindow::onVaultOpened` and will migrate up here in a later
/// phase.
class CorbomiteApp : public QObject {
    Q_OBJECT
public:
    explicit CorbomiteApp(QObject *parent = nullptr);
    ~CorbomiteApp() override;

    bool openVault(const QString &path);
    void closeVault();

    VaultModel *vault() const;
    bool isOpen() const;

    /// Read-through accessor — also refreshes stale entries from the
    /// backing store so the welcome-screen list stays live across
    /// in-process opens (the KRecentFilesAction wired in MainWindow
    /// writes the same file).
    QStringList recentVaults();

Q_SIGNALS:
    void vaultOpened();
    void vaultClosed();

private:
    VaultModel *m_vault;
    RecentVaults m_recentVaults;
};

} // namespace Corbomite
