// SPDX-License-Identifier: GPL-3.0-or-later
#include "CorbomiteApp.h"

#include "corbomite/models/VaultModel.h"

#include <QFileInfo>

namespace Corbomite {

CorbomiteApp::CorbomiteApp(QObject *parent)
    : QObject(parent)
    , m_vault(new VaultModel(this))
{
}

CorbomiteApp::~CorbomiteApp() = default;

bool CorbomiteApp::openVault(const QString &path)
{
    if (!QFileInfo::exists(path)) return false;

    closeVault();
    m_vault->open(path);
    m_recentVaults.add(path);
    m_recentVaults.save();
    Q_EMIT vaultOpened();
    return true;
}

void CorbomiteApp::closeVault()
{
    if (!m_vault->isOpen()) return;
    m_vault->close();
    Q_EMIT vaultClosed();
}

VaultModel *CorbomiteApp::vault() const
{
    return m_vault;
}

bool CorbomiteApp::isOpen() const
{
    return m_vault->isOpen();
}

QStringList CorbomiteApp::recentVaults()
{
    m_recentVaults.load();
    return m_recentVaults.list();
}

} // namespace Corbomite
