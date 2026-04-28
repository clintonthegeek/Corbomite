// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginContext.h"

#include "corbomite/core/PluginPermissions.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/MenuInjector.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/core/proxies/ProcessSpawner.h"
#include "corbomite/core/proxies/SecretStorage.h"
#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/core/proxies/HoverLinkSourceRegistrar.h"
#include "corbomite/core/proxies/EditorSuggestRegistrar.h"
#include "corbomite/core/proxies/PostProcessorRegistrar.h"
#include "corbomite/core/proxies/RibbonRegistrar.h"
#include "corbomite/core/proxies/EmbedRegistrar.h"
#include "corbomite/core/proxies/CodeBlockRegistrar.h"
#include "corbomite/core/proxies/StatusBarRegistrar.h"
#include "corbomite/vault/PluginDataStore.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KSharedConfig>

namespace Corbomite {

using namespace Corbomite::Permissions;

PluginContext::PluginContext(PluginMetaData meta, QSet<QString> granted)
    : m_meta(std::move(meta)), m_granted(std::move(granted)) {}

PluginContext::~PluginContext()
{
    delete m_vaultProxy;
    delete m_fileManagerProxy;
    delete m_metadataReader;
    delete m_searchProxy;
    delete m_workspaceController;
    delete m_commandRegistrar;
    delete m_viewRegistrar;
    delete m_menuInjector;
    delete m_secretStorage;
    delete m_processSpawner;
    delete m_hoverLinkRegistrar;
    delete m_editorSuggestRegistrar;
    delete m_postProcessorRegistrar;
    delete m_ribbonRegistrar;
    delete m_embedRegistrar;
    delete m_codeBlockRegistrar;
    delete m_statusBarRegistrar;
}

void PluginContext::setExtensionRegistries(
    HoverLinkSourceRegistry *hoverLinkSources,
    EditorSuggestManager *editorSuggests,
    Corbomite::Core::PostProcessorRegistry *postProcessors,
    RibbonHandle *ribbon,
    Markoff::EmbedRegistry *embeds,
    Markoff::CodeBlockProcessorRegistry *codeBlocks,
    StatusBarRegistry *statusBar)
{
    m_hoverLinkSourceRegistry = hoverLinkSources;
    m_editorSuggestManager = editorSuggests;
    m_postProcessorRegistry = postProcessors;
    m_ribbonHandle = ribbon;
    m_embedRegistry = embeds;
    m_codeBlockRegistry = codeBlocks;
    m_statusBarRegistry = statusBar;
}

void PluginContext::setCoreServices(Vault *v, FileManager *fm,
                                     MetadataCache *m, SQLiteIndex *si,
                                     Workspace *w, CommandRegistry *c,
                                     ViewRegistry *vr, MenuEventEmitter *me,
                                     QNetworkAccessManager *n)
{
    m_vault = v;
    m_fileManager = fm;
    m_metadata = m;
    m_searchIndex = si;
    m_workspace = w;
    m_commandRegistry = c;
    m_viewRegistry = vr;
    m_menuEmitter = me;
    m_network = n;
}

VaultProxy *PluginContext::vault() const
{
    // VaultProxy gates each method on its own token. Construct the proxy if
    // the plugin holds ANY of the vault.* tokens — otherwise there is no
    // reachable method and nullptr is the right answer.
    const bool anyVaultPerm =
        hasPermission(QLatin1String(kVaultRead))
        || hasPermission(QLatin1String(kVaultWrite))
        || hasPermission(QLatin1String(kVaultEvents));
    if (!anyVaultPerm || !m_vault) return nullptr;
    if (!m_vaultProxy) {
        m_vaultProxy = new VaultProxy(m_vault, m_granted,
                                      m_meta.base().pluginId());
    }
    return m_vaultProxy;
}

FileManagerProxy *PluginContext::fileManager() const
{
    // FileManagerProxy gates per-method: vault.read for queries,
    // vault.write for mutations, metadata.read for generateMarkdownLink.
    const bool anyFmPerm =
        hasPermission(QLatin1String(kVaultRead))
        || hasPermission(QLatin1String(kVaultWrite))
        || hasPermission(QLatin1String(kMetadataRead));
    if (!anyFmPerm || !m_fileManager) return nullptr;
    if (!m_fileManagerProxy) {
        m_fileManagerProxy = new FileManagerProxy(m_fileManager, m_granted,
                                                  m_meta.base().pluginId());
    }
    return m_fileManagerProxy;
}

MetadataCacheReader *PluginContext::metadataCache() const
{
    if (!hasPermission(QLatin1String(kMetadataRead)) || !m_metadata) return nullptr;
    if (!m_metadataReader) m_metadataReader = new MetadataCacheReader(m_metadata);
    return m_metadataReader;
}

SearchProxy *PluginContext::search() const
{
    if (!m_searchIndex) return nullptr;
    if (!hasPermission(QLatin1String(kMetadataRead))) return nullptr;
    if (!m_searchProxy) {
        m_searchProxy = new SearchProxy(m_searchIndex, m_granted,
                                        m_meta.base().pluginId());
    }
    return m_searchProxy;
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
    if (!m_secretStorage) {
        m_secretStorage = new SecretStorage(m_meta.base().pluginId(), m_granted);
    }
    return m_secretStorage;
}

ProcessSpawner *PluginContext::process() const
{
    if (!hasPermission(QLatin1String(kProcess))) return nullptr;
    if (!m_processSpawner) m_processSpawner = new ProcessSpawner(m_meta.base().pluginId());
    return m_processSpawner;
}

HoverLinkSourceRegistrar *PluginContext::hoverLinkSources() const
{
    if (!hasPermission(QLatin1String(kUiRendering)) || !m_hoverLinkSourceRegistry) return nullptr;
    if (!m_hoverLinkRegistrar) {
        m_hoverLinkRegistrar = new HoverLinkSourceRegistrar(
            m_hoverLinkSourceRegistry, m_meta.base().pluginId());
    }
    return m_hoverLinkRegistrar;
}

EditorSuggestRegistrar *PluginContext::editorSuggests() const
{
    if (!hasPermission(QLatin1String(kUiEditor)) || !m_editorSuggestManager) return nullptr;
    if (!m_editorSuggestRegistrar) {
        m_editorSuggestRegistrar = new EditorSuggestRegistrar(m_editorSuggestManager);
    }
    return m_editorSuggestRegistrar;
}

PostProcessorRegistrar *PluginContext::postProcessors() const
{
    if (!hasPermission(QLatin1String(kUiRendering)) || !m_postProcessorRegistry) return nullptr;
    if (!m_postProcessorRegistrar) {
        m_postProcessorRegistrar = new PostProcessorRegistrar(m_postProcessorRegistry);
    }
    return m_postProcessorRegistrar;
}

RibbonRegistrar *PluginContext::ribbon() const
{
    if (!hasPermission(QLatin1String(kUiCommands)) || !m_ribbonHandle) return nullptr;
    if (!m_ribbonRegistrar) {
        m_ribbonRegistrar = new RibbonRegistrar(m_ribbonHandle, m_meta.base().pluginId());
    }
    return m_ribbonRegistrar;
}

EmbedRegistrar *PluginContext::embeds() const
{
    if (!hasPermission(QLatin1String(kUiRendering)) || !m_embedRegistry) return nullptr;
    if (!m_embedRegistrar) {
        m_embedRegistrar = new EmbedRegistrar(m_embedRegistry);
    }
    return m_embedRegistrar;
}

CodeBlockRegistrar *PluginContext::codeBlocks() const
{
    if (!hasPermission(QLatin1String(kUiRendering)) || !m_codeBlockRegistry) return nullptr;
    if (!m_codeBlockRegistrar) {
        m_codeBlockRegistrar = new CodeBlockRegistrar(m_codeBlockRegistry);
    }
    return m_codeBlockRegistrar;
}

StatusBarRegistrar *PluginContext::statusBar() const
{
    if (!hasPermission(QLatin1String(kUiStatusbar)) || !m_statusBarRegistry) return nullptr;
    if (!m_statusBarRegistrar) {
        m_statusBarRegistrar = new StatusBarRegistrar(
            m_statusBarRegistry, m_meta.base().pluginId());
    }
    return m_statusBarRegistrar;
}

KConfigGroup PluginContext::config()
{
    if (!hasPermission(QLatin1String(kConfig))) return {};
    return KConfigGroup(KSharedConfig::openConfig(),
                        QStringLiteral("Plugin-") + m_meta.base().pluginId());
}

void PluginContext::setPluginDataDir(const QString &dir)
{
    m_pluginDataDir = dir;
    m_dataStore.reset();
}

QJsonObject PluginContext::loadData() const
{
    if (!hasPermission(QLatin1String(kConfig))) return {};
    if (m_pluginDataDir.isEmpty()) return {};
    if (!m_dataStore) m_dataStore = std::make_unique<PluginDataStore>(m_pluginDataDir);
    return m_dataStore->load();
}

bool PluginContext::saveData(const QJsonObject &obj)
{
    if (!hasPermission(QLatin1String(kConfig))) return false;
    if (m_pluginDataDir.isEmpty()) return false;
    if (!m_dataStore) m_dataStore = std::make_unique<PluginDataStore>(m_pluginDataDir);
    return m_dataStore->save(obj);
}

} // namespace Corbomite
