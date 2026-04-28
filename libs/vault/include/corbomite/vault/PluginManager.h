// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"

#include <KSharedConfig>

#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QVersionNumber>

#include <functional>

namespace Corbomite {

class Plugin;
class PluginContext;
class VaultConfig;

/// Owns the lifecycle of all Corbomite plugins (built-in + community).
///
/// Singleton-ish — owned by `CorbomiteApp`, one per application. Modelled on
/// Kate's `KatePluginManager` (see `docs/kde-power-software-design-guide/
/// 06-plugin-architecture.md` §"Kate Plugin Manager Pattern").
///
/// Task 5 scope: discovery + trust normalization + version-compat gate. Load,
/// enable/disable, and KConfig persistence land in Task 6.
class PluginManager : public QObject
{
    Q_OBJECT
public:
    /// Per-plugin gating state surfaced to PluginsPage.
    ///
    /// * `Compatible` — default; plugin has not been refused by the
    ///   MinVersion / ApiLevel gate. Matches both "never attempted" and
    ///   "attempted and passed the gate" — the load success itself is
    ///   captured by `PluginInfo::enabled` / `PluginInfo::instance`.
    /// * `IncompatibleVersion` — plugin's declared X-Corbomite-MinVersion
    ///   exceeds the host's reported appVersion(). Recorded at ingest()
    ///   so Settings can render "Requires Corbomite >= X.Y.Z" without
    ///   the user ever clicking enable; also re-asserted in enablePlugin()
    ///   should the gate be reached for any reason.
    /// * `IncompatibleApiLevel` — plugin's declared X-Corbomite-ApiLevel
    ///   exceeds CORBOMITE_PLUGIN_API_LEVEL. Recorded at ingest().
    /// * `OnLoadThrew` — `Plugin::onLoad()` raised an exception. The
    ///   instance was auto-unloaded and destroyed; Settings shows a
    ///   "this plugin failed to load" notice. Persisted enabled-state
    ///   is left intact so a fixed plugin re-enables on next launch.
    enum class LoadState {
        Compatible,
        IncompatibleVersion,
        IncompatibleApiLevel,
        OnLoadThrew,
    };

    struct PluginInfo
    {
        PluginMetaData metaData;
        Plugin        *instance = nullptr;
        PluginContext *context  = nullptr;
        bool           enabled  = false;
    };

    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    // Paths — overridable for tests.
    void setSystemSearchPath(const QString &path);
    void setUserSearchPath(const QString &path);

    const QString &systemSearchPath() const { return m_systemPath; }
    const QString &userSearchPath() const { return m_userPath; }

    /// Scan both search paths via `KPluginMetaData::findPlugins` and feed
    /// results through `ingest`. Does not load any plugin yet.
    void discoverPlugins();

    /// Register a batch of metadata records as if they came from `origin`.
    /// Separated out so tests can drive normalization without real `.so`s.
    ///
    /// Applies: origin stamping, MinVersion rejection, and the trusted-claim
    /// override (enforced by PluginMetaData::trusted() when origin==User).
    void ingest(const QList<KPluginMetaData> &metas,
                PluginMetaData::Origin origin);

    int pluginCount() const { return m_plugins.size(); }
    const PluginInfo &pluginByIndex(int i) const { return m_plugins[i]; }
    const PluginInfo *pluginById(const QString &id) const;

    /// Gate state for the given plugin id. Returns Compatible for
    /// unknown ids so callers that just want to ask "is there a reason
    /// to grey out this row?" can do so without null-checks.
    LoadState loadState(const QString &id) const;

    // Load / unload lifecycle (Task 6)
    bool enablePlugin(const QString &id);

    /// Unload a plugin. When `persist` is true (default — user action via
    /// Settings) the disabled state is written to KConfig so the plugin
    /// stays off across restarts. When false (vault-lifecycle teardown)
    /// the in-memory instance is destroyed but KConfig is left alone, so
    /// the next vault-open / app-launch can re-enable based on the
    /// user's persisted choice.
    bool disablePlugin(const QString &id, bool persist = true);

    /// For each discovered plugin, read KConfig to decide if it should be
    /// enabled (fall back to metadata's EnabledByDefault).
    void loadEnabledStateFromConfig();

    // Config + factory overrides — for test isolation and future DI.
    void setConfig(KSharedConfig::Ptr config);
    KSharedConfig::Ptr config() const { return m_config; }

    /// Bind a VaultConfig (lifetime ≥ this PluginManager). When set,
    /// enable/disable mirror their state into `.obsidian/core-plugins.json`
    /// or `.obsidian/community-plugins.json` for plugins that have an
    /// Obsidian-counterpart ID (declared via `X-Obsidian-Id` manifest
    /// field, or covered by the internal alias dictionary). Pass `nullptr`
    /// on vault close to detach. See spec
    /// `docs/superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md`.
    void setVaultConfig(VaultConfig *vcfg);
    VaultConfig *vaultConfig() const { return m_vaultConfig; }

    /// Resolve a Corbomite plugin id to its Obsidian counterpart slug.
    /// Honours `X-Obsidian-Id` manifest field first, falls back to a
    /// hard-coded alias dictionary covering the in-tree internal plugins
    /// shipped at Cluster Q close. Returns an empty string for
    /// Corbomite-only plugins that have neither.
    QString obsidianIdFor(const QString &corbomiteId) const;

    using FactoryFn = std::function<Plugin *(const PluginMetaData &)>;
    /// When set, enablePlugin bypasses KPluginFactory and calls this instead.
    /// Used by tests to inject synthesised Plugin instances. Production code
    /// leaves this empty — the default path goes through KPluginFactory.
    void setFactoryOverride(FactoryFn fn);

    using PromptFn = std::function<QSet<QString>(const PluginMetaData &meta,
                                                  const QSet<QString> &declared)>;
    /// When set, untrusted plugins with ungranted declared permissions route
    /// through this callback instead of `PluginPermissionGrantDialog`. Return
    /// value is the granted subset; empty = user cancelled.
    void setPromptHandler(PromptFn fn);

    using ContextConfigurator = std::function<void(PluginContext *)>;
    /// When set, every freshly-constructed PluginContext is handed to this
    /// callback before plugin->load(ctx). The host uses this hook to call
    /// PluginContext::setCoreServices() so onLoad sees fully-wired proxies.
    void setContextConfigurator(ContextConfigurator fn);

Q_SIGNALS:
    void pluginDiscovered(const QString &id);
    void pluginLoaded(const QString &id);
    void pluginUnloading(const QString &id);
    void pluginEnabled(const QString &id);
    void pluginDisabled(const QString &id);

public:
    /// Test-only: directly fire a data.json change for `pluginId`. Bypasses
    /// the QFileSystemWatcher so unit tests don't depend on filesystem
    /// timing.
    void simulateExternalSettingsChange(const QString &pluginId);

protected:
    /// The version this build of the app reports for MinVersion comparisons.
    /// Virtual so tests can override if ever needed (today we accept any
    /// MinVersion ≤ CORBOMITE_APP_VERSION or, absent that define, 0.0.0).
    virtual QVersionNumber appVersion() const;

private Q_SLOTS:
    void onDataJsonChanged(const QString &path);

private:
    QSet<QString> loadGrantedPermissions(const QString &id) const;
    void saveGrantedPermissions(const QString &id, const QSet<QString> &granted);
    void writeEnabledState(const QString &id, bool enabled);

    QString m_systemPath;
    QString m_userPath;
    QList<PluginInfo> m_plugins;
    QHash<QString, LoadState> m_loadState;
    KSharedConfig::Ptr m_config;
    VaultConfig *m_vaultConfig = nullptr; // non-owning; set per-vault by host
    FactoryFn m_factoryOverride;
    PromptFn  m_promptHandler;
    ContextConfigurator m_contextConfigurator;

    // Cluster B Phase 3.3 — per-plugin data.json watcher. Maps absolute
    // data.json path -> plugin id so the watcher's fileChanged slot can
    // dispatch to the right plugin.
    QFileSystemWatcher m_dataJsonWatcher;
    QHash<QString, QString> m_dataJsonPathToPluginId;
};

} // namespace Corbomite
