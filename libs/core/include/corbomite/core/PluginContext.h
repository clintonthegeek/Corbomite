// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"

#include <KConfigGroup>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace Corbomite {

// Forward decls — core service types (passed in via setCoreServices).
// Note: Vault proxies were dropped in Cluster Q.0 Phase 1 (the path-only
// Task-7 Vault + VaultReader + VaultWriter all went with it). The new
// VaultProxy + FileManagerProxy land in libs/vault/ during Phase 9; this
// file wires them back in at that point.
class MetadataCache;
class Workspace;
class CommandRegistry;
class ViewRegistry;
class MenuEventEmitter;

// Forward decls — proxy types (owned by PluginContext, lazy-constructed).
class MetadataCacheReader;
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
    void setCoreServices(MetadataCache *metadata,
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
    MetadataCacheReader   *metadataCache() const;   // "metadata.read"
    WorkspaceController   *workspace() const;       // "workspace"
    CommandRegistrar      *commands() const;        // "ui.commands"
    ViewRegistrar         *views() const;           // "ui.views"
    MenuInjector          *menus() const;           // "ui.menus"
    QNetworkAccessManager *network() const;         // "network"
    SecretStorage         *secrets() const;         // "secrets"
    ProcessSpawner        *process() const;         // "process"

    /// Per-plugin KConfig group. Returns an empty group if "config" is ungranted.
    KConfigGroup config();

private:
    PluginMetaData m_meta;
    QSet<QString>  m_granted;

    // Owned proxies (lazy)
    mutable MetadataCacheReader *m_metadataReader = nullptr;
    mutable WorkspaceController *m_workspaceController = nullptr;
    mutable CommandRegistrar    *m_commandRegistrar = nullptr;
    mutable ViewRegistrar       *m_viewRegistrar = nullptr;
    mutable MenuInjector        *m_menuInjector = nullptr;
    mutable SecretStorage       *m_secretStorage = nullptr;
    mutable ProcessSpawner      *m_processSpawner = nullptr;

    // Non-owning core service references
    MetadataCache         *m_metadata = nullptr;
    Workspace             *m_workspace = nullptr;
    CommandRegistry       *m_commandRegistry = nullptr;
    ViewRegistry          *m_viewRegistry = nullptr;
    MenuEventEmitter      *m_menuEmitter = nullptr;
    QNetworkAccessManager *m_network = nullptr;
};

} // namespace Corbomite
