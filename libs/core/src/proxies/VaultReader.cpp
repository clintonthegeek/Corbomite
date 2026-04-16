// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/VaultReader.h"

#include "corbomite/core/Vault.h"

namespace Corbomite {

QByteArray VaultReader::read(const QString &relativePath) const
{
    return m_vault ? m_vault->read(relativePath) : QByteArray{};
}

bool VaultReader::exists(const QString &relativePath) const
{
    return m_vault && m_vault->exists(relativePath);
}

QStringList VaultReader::list(const QString &subdir) const
{
    return m_vault ? m_vault->list(subdir) : QStringList{};
}

} // namespace Corbomite
