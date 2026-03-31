// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

namespace Corbomite {

class VaultModel;
class NoteService;

class VaultService : public QObject {
    Q_OBJECT

public:
    explicit VaultService(QObject *parent = nullptr);
    ~VaultService() override;

    bool openVault(const QString &path);
    void closeVault();

    VaultModel *vault() const;
    NoteService *noteService() const;
    bool isOpen() const;

    QStringList recentVaults() const;
    void addRecentVault(const QString &path);

Q_SIGNALS:
    void vaultOpened();
    void vaultClosed();

private:
    VaultModel *m_vault;
    NoteService *m_noteService;
};

} // namespace Corbomite
