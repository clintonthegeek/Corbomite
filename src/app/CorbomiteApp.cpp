// SPDX-License-Identifier: GPL-3.0-or-later
#include "CorbomiteApp.h"

#include <QFileInfo>

namespace Corbomite {

CorbomiteApp::CorbomiteApp(QObject *parent)
    : QObject(parent)
{
}

CorbomiteApp::~CorbomiteApp() = default;

bool CorbomiteApp::openVault(const QString &path)
{
    if (!QFileInfo::exists(path)) return false;

    closeVault();
    m_currentPath = path;
    m_recentVaults.add(path);
    m_recentVaults.save();
    Q_EMIT vaultOpened(path);
    return true;
}

void CorbomiteApp::closeVault()
{
    if (m_currentPath.isEmpty()) return;
    m_currentPath.clear();
    Q_EMIT vaultClosed();
}

bool CorbomiteApp::isOpen() const
{
    return !m_currentPath.isEmpty();
}

QString CorbomiteApp::currentVaultPath() const
{
    return m_currentPath;
}

QStringList CorbomiteApp::recentVaults()
{
    m_recentVaults.load();
    return m_recentVaults.list();
}

} // namespace Corbomite
