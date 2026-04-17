// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"

#include <KConfigGroup>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <memory>

class QNetworkAccessManager;

namespace Corbomite {

class PluginDataStore;

// Forward decls — core service types (passed in via setCoreServices).
class Vault;
class FileManager;
class MetadataCache;
class SQLiteIndex;
class Workspace;
class CommandRegistry;
class ViewRegistry;
class MenuEventEmitter;

// Forward decls — proxy types (owned by PluginContext, lazy-constructed).
class VaultProxy;
class FileManagerProxy;
class MetadataCacheReader;
class SearchProxy;
class WorkspaceController;
class CommandRegistrar;
class ViewRegistrar;
class MenuInjector;
class SecretStorage;
class ProcessSpawner;

/// Handed to `Plugin::onLoad()`. Lifetime equals the plugin's load span.
///
/// Owns proxy objects for services the plugin's *granted* permissions unlock.
/// Each accessor returns nullptr when either:
///   - the corresponding permission was not granted, OR
///   - the underlying core service has not been installed via setCoreServices().
class PluginContext
{
public:
    PluginContext(PluginMetaData meta, QSet<QString> granted);
    ~PluginContext();

    PluginContext(const PluginContext &) = delete;
    PluginContext &operator=(const PluginContext &) = delete;

    /// Install core-service references. Must be called before the plugin
    /// invokes any granted accessor. Pass nullptr for services the host
    /// does not provide; the corresponding accessor will then return nullptr.
    void setCoreServices(Vault *vault,
                         FileManager *fileManager,
                         MetadataCache *metadata,
                         SQLiteIndex *searchIndex,
                         Workspace *workspace,
                         CommandRegistry *commands,
                         ViewRegistry *views,
                         MenuEventEmitter *menus,
                         QNetworkAccessManager *network);

    // Metadata accessors
    const PluginMetaData &metaData() const { return m_meta; }
    const QSet<QString>  &grantedPermissions() const { return m_granted; }
    bool hasPermission(const QString &token) const { return m_granted.contains(token); }

    // Permission-gated accessors. Lazy-constructed; nullptr if either permission
    // is ungranted or the underlying core service is null.
    VaultProxy            *vault() const;           // "vault.read" || "vault.write" || "vault.events"
    FileManagerProxy      *fileManager() const;     // "vault.read" || "vault.write" || "metadata.read"
    MetadataCacheReader   *metadataCache() const;   // "metadata.read"
    SearchProxy           *search() const;          // "metadata.read"
    WorkspaceController   *workspace() const;       // "workspace"
    CommandRegistrar      *commands() const;        // "ui.commands"
    ViewRegistrar         *views() const;           // "ui.views"
    MenuInjector          *menus() const;           // "ui.menus"
    QNetworkAccessManager *network() const;         // "network"
    SecretStorage         *secrets() const;         // "secrets"
    ProcessSpawner        *process() const;         // "process"

    /// Per-plugin KConfig group. Returns an empty group if "config" is ungranted.
    KConfigGroup config();

    /// Persistent per-plugin JSON state at
    /// `<vault>/.obsidian/plugins/<plugin-id>/data.json`.
    /// Returns an empty object if the plugin hasn't saved yet or the
    /// host hasn't wired a data dir. Gated by "config" permission.
    QJsonObject loadData() const;
    bool        saveData(const QJsonObject &obj);

    /// Host wiring — PluginManager calls this before plugin->load().
    void setPluginDataDir(const QString &dir);

private:
    PluginMetaData m_meta;
    QSet<QString>  m_granted;

    // Owned proxies (lazy)
    mutable VaultProxy          *m_vaultProxy = nullptr;
    mutable FileManagerProxy    *m_fileManagerProxy = nullptr;
    mutable MetadataCacheReader *m_metadataReader = nullptr;
    mutable SearchProxy         *m_searchProxy = nullptr;
    mutable WorkspaceController *m_workspaceController = nullptr;
    mutable CommandRegistrar    *m_commandRegistrar = nullptr;
    mutable ViewRegistrar       *m_viewRegistrar = nullptr;
    mutable MenuInjector        *m_menuInjector = nullptr;
    mutable SecretStorage       *m_secretStorage = nullptr;
    mutable ProcessSpawner      *m_processSpawner = nullptr;

    // Plugin-data.json persistence
    QString                                  m_pluginDataDir;
    mutable std::unique_ptr<PluginDataStore> m_dataStore;

    // Non-owning core service references
    Vault                 *m_vault = nullptr;
    FileManager           *m_fileManager = nullptr;
    MetadataCache         *m_metadata = nullptr;
    SQLiteIndex           *m_searchIndex = nullptr;
    Workspace             *m_workspace = nullptr;
    CommandRegistry       *m_commandRegistry = nullptr;
    ViewRegistry          *m_viewRegistry = nullptr;
    MenuEventEmitter      *m_menuEmitter = nullptr;
    QNetworkAccessManager *m_network = nullptr;
};

} // namespace Corbomite
