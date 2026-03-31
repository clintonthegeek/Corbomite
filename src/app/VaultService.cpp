// SPDX-License-Identifier: GPL-3.0-or-later
#include "VaultService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include <KSharedConfig>
#include <KConfigGroup>
#include <QFileInfo>

namespace Corbomite {

VaultService::VaultService(QObject *parent)
    : QObject(parent)
    , m_vault(new VaultModel(this))
    , m_noteService(new NoteService(m_vault, this))
{
}

VaultService::~VaultService() = default;

bool VaultService::openVault(const QString &path)
{
    if (!QFileInfo::exists(path)) return false;

    closeVault();
    m_vault->open(path);
    addRecentVault(path);
    Q_EMIT vaultOpened();
    return true;
}

void VaultService::closeVault()
{
    if (!m_vault->isOpen()) return;
    m_vault->close();
    Q_EMIT vaultClosed();
}

VaultModel *VaultService::vault() const
{
    return m_vault;
}

NoteService *VaultService::noteService() const
{
    return m_noteService;
}

bool VaultService::isOpen() const
{
    return m_vault->isOpen();
}

QStringList VaultService::recentVaults() const
{
    auto config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("RecentVaults"));
    return group.readEntry("Paths", QStringList());
}

void VaultService::addRecentVault(const QString &path)
{
    auto config = KSharedConfig::openConfig();
    KConfigGroup group = config->group(QStringLiteral("RecentVaults"));
    QStringList recent = group.readEntry("Paths", QStringList());
    recent.removeAll(path);
    recent.prepend(path);
    if (recent.size() > 10) recent = recent.mid(0, 10);
    group.writeEntry("Paths", recent);
    config->sync();
}

} // namespace Corbomite
