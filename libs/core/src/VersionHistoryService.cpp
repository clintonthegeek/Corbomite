// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/VersionHistoryService.h"

#include "corbomite/core/DataAdapter.h"

namespace Corbomite {

VersionHistoryService::VersionHistoryService(DataAdapter *adapter, QString vaultRoot)
    : m_adapter(adapter)
    , m_store(adapter, vaultRoot)
    , m_vaultRoot(vaultRoot)
{
}

QList<RecoverySnapshot> VersionHistoryService::snapshots(const QString &relativePath) const
{
    return m_store.list(relativePath);
}

std::optional<QString> VersionHistoryService::preview(const RecoverySnapshot &snapshot) const
{
    return m_store.read(snapshot.path);
}

bool VersionHistoryService::restore(const QString &relativePath,
                                    const RecoverySnapshot &snapshot)
{
    if (!m_adapter || m_vaultRoot.isEmpty())
        return false;

    auto content = m_store.read(snapshot.path);
    if (!content)
        return false;

    const QString absPath = m_vaultRoot + QLatin1Char('/') + relativePath;

    // Pre-restore safety copy: the content we are about to overwrite becomes a
    // periodic snapshot, so the restore itself is recoverable (Q4).
    auto current = m_adapter->read(absPath);
    if (current && *current != *content)
        m_store.write(relativePath, *current, /*failure=*/false);

    return m_adapter->write(absPath, *content);
}

void VersionHistoryService::purge(const QString &relativePath)
{
    m_store.removeAll(relativePath);
}

}  // namespace Corbomite
