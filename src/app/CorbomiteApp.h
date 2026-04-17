// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "RecentVaults.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class VaultModel;
class NoteService;

/// Top-level application object. Owns the legacy `VaultModel` +
/// `NoteService` pair and drives the vault open/close lifecycle.
///
/// During Q.0 Phase 8 this absorbed what used to live in `VaultService`
/// (which is deleted in Task 8.3). `NoteService` is folded into
/// `FileManager` in Task 8.4; the canonical `Corbomite::Vault` +
/// `FileManager` aggregate itself is still constructed downstream in
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
    NoteService *noteService() const;
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
    NoteService *m_noteService;
    RecentVaults m_recentVaults;
};

} // namespace Corbomite
