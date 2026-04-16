// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/VaultWriter.h"

#include "corbomite/core/Vault.h"

namespace Corbomite {

bool VaultWriter::create(const QString &relativePath, const QByteArray &body)
{
    return m_vault && m_vault->create(relativePath, body);
}

bool VaultWriter::write(const QString &relativePath, const QByteArray &body)
{
    return m_vault && m_vault->write(relativePath, body);
}

bool VaultWriter::rename(const QString &oldPath, const QString &newPath)
{
    return m_vault && m_vault->rename(oldPath, newPath);
}

bool VaultWriter::remove(const QString &relativePath)
{
    return m_vault && m_vault->remove(relativePath);
}

} // namespace Corbomite
