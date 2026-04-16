// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Corbomite {

class Vault;

/// Read-only Vault facade for plugins with the "vault.read" permission.
/// Stub — wire-up to the underlying Vault lands in Cluster Q Task 7.
class VaultReader
{
public:
    explicit VaultReader(Vault *vault) : m_vault(vault) {}

    QByteArray read(const QString &relativePath) const;
    bool exists(const QString &relativePath) const;
    QStringList list(const QString &subdir = {}) const;

private:
    Vault *m_vault;
};

} // namespace Corbomite
