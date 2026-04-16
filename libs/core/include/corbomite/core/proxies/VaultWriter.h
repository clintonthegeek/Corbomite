// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace Corbomite {

class Vault;

/// Mutating Vault facade for plugins with the "vault.write" permission.
/// Stub — wire-up to the underlying Vault lands in Cluster Q Task 7.
class VaultWriter
{
public:
    explicit VaultWriter(Vault *vault) : m_vault(vault) {}

    bool create(const QString &relativePath, const QByteArray &body);
    bool write(const QString &relativePath, const QByteArray &body);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &relativePath);

private:
    Vault *m_vault;
};

} // namespace Corbomite
