// SPDX-License-Identifier: GPL-3.0-or-later
#include "CorbomiteApp.h"

#include "corbomite/vault/PluginManager.h"

#include <QFileInfo>

namespace Corbomite {

CorbomiteApp::CorbomiteApp(QObject *parent)
    : QObject(parent)
    , m_pluginManager(new PluginManager(this))
{
    // Cluster Q Phase 2 (Task 12): discover + auto-enable plugins at
    // startup. Plugin views attach to MainWindow when each plugin
    // emits pluginLoaded — see MainWindow's pluginLoaded slot.
    m_pluginManager->discoverPlugins();
    m_pluginManager->loadEnabledStateFromConfig();
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
