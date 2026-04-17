// SPDX-License-Identifier: GPL-3.0-or-later
#include "CorbomiteApp.h"

#include "corbomite/vault/PluginManager.h"

#include <QFileInfo>

namespace Corbomite {

CorbomiteApp::CorbomiteApp(QObject *parent)
    : QObject(parent)
    , m_pluginManager(new PluginManager(this))
{
    // Cluster Q Phase 2 (Task 12): discover plugins at startup. Plugins
    // are vault-scoped — actual enable happens when MainWindow opens a
    // vault and is in a position to wire core services into each
    // PluginContext via PluginManager::setContextConfigurator + then
    // calls loadEnabledStateFromConfig.
#ifdef CORBOMITE_PLUGIN_DEV_DIR
    // Dev builds discover plugins from the build tree; release builds
    // fall through to PluginManager's default ${KDE_INSTALL_PLUGINDIR}
    // resolution.
    m_pluginManager->setSystemSearchPath(QStringLiteral(CORBOMITE_PLUGIN_DEV_DIR));
#endif
    m_pluginManager->discoverPlugins();
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
