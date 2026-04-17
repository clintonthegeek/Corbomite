// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginContext.h"

#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/MenuInjector.h"
#include "corbomite/core/proxies/MetadataCacheReader.h"
#include "corbomite/core/proxies/ProcessSpawner.h"
#include "corbomite/core/proxies/SecretStorage.h"
#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"

#include <KSharedConfig>

namespace Corbomite {

namespace {
constexpr auto kMetadataRead = "metadata.read";
constexpr auto kWorkspace    = "workspace";
constexpr auto kUiCommands   = "ui.commands";
constexpr auto kUiViews      = "ui.views";
constexpr auto kUiMenus      = "ui.menus";
constexpr auto kNetwork      = "network";
constexpr auto kSecrets      = "secrets";
constexpr auto kProcess      = "process";
constexpr auto kConfig       = "config";
} // namespace

PluginContext::PluginContext(PluginMetaData meta, QSet<QString> granted)
    : m_meta(std::move(meta)), m_granted(std::move(granted)) {}

PluginContext::~PluginContext()
{
    delete m_metadataReader;
    delete m_workspaceController;
    delete m_commandRegistrar;
    delete m_viewRegistrar;
    delete m_menuInjector;
    delete m_secretStorage;
    delete m_processSpawner;
}

void PluginContext::setCoreServices(MetadataCache *m, Workspace *w,
                                     CommandRegistry *c, ViewRegistry *vr,
                                     MenuEventEmitter *me,
                                     QNetworkAccessManager *n)
{
    m_metadata = m;
    m_workspace = w;
    m_commandRegistry = c;
    m_viewRegistry = vr;
    m_menuEmitter = me;
    m_network = n;
}

MetadataCacheReader *PluginContext::metadataCache() const
{
    if (!hasPermission(QLatin1String(kMetadataRead)) || !m_metadata) return nullptr;
    if (!m_metadataReader) m_metadataReader = new MetadataCacheReader(m_metadata);
    return m_metadataReader;
}

WorkspaceController *PluginContext::workspace() const
{
    if (!hasPermission(QLatin1String(kWorkspace)) || !m_workspace) return nullptr;
    if (!m_workspaceController) m_workspaceController = new WorkspaceController(m_workspace);
    return m_workspaceController;
}

CommandRegistrar *PluginContext::commands() const
{
    if (!hasPermission(QLatin1String(kUiCommands)) || !m_commandRegistry) return nullptr;
    if (!m_commandRegistrar) {
        m_commandRegistrar = new CommandRegistrar(m_commandRegistry,
                                                  m_meta.base().pluginId());
    }
    return m_commandRegistrar;
}

ViewRegistrar *PluginContext::views() const
{
    if (!hasPermission(QLatin1String(kUiViews)) || !m_viewRegistry) return nullptr;
    if (!m_viewRegistrar) m_viewRegistrar = new ViewRegistrar(m_viewRegistry);
    return m_viewRegistrar;
}

MenuInjector *PluginContext::menus() const
{
    if (!hasPermission(QLatin1String(kUiMenus)) || !m_menuEmitter) return nullptr;
    if (!m_menuInjector) m_menuInjector = new MenuInjector(m_menuEmitter);
    return m_menuInjector;
}

QNetworkAccessManager *PluginContext::network() const
{
    if (!hasPermission(QLatin1String(kNetwork))) return nullptr;
    return m_network;
}

SecretStorage *PluginContext::secrets() const
{
    if (!hasPermission(QLatin1String(kSecrets))) return nullptr;
    if (!m_secretStorage) m_secretStorage = new SecretStorage();
    return m_secretStorage;
}

ProcessSpawner *PluginContext::process() const
{
    if (!hasPermission(QLatin1String(kProcess))) return nullptr;
    if (!m_processSpawner) m_processSpawner = new ProcessSpawner(m_meta.base().pluginId());
    return m_processSpawner;
}

KConfigGroup PluginContext::config()
{
    if (!hasPermission(QLatin1String(kConfig))) return {};
    return KConfigGroup(KSharedConfig::openConfig(),
                        QStringLiteral("Plugin-") + m_meta.base().pluginId());
}

} // namespace Corbomite
