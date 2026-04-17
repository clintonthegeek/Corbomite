// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/PluginMetaData.h"

#include <KSharedConfig>

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QVersionNumber>

#include <functional>

namespace Corbomite {

class Plugin;
class PluginContext;

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

protected:
    /// The version this build of the app reports for MinVersion comparisons.
    /// Virtual so tests can override if ever needed (today we accept any
    /// MinVersion ≤ CORBOMITE_APP_VERSION or, absent that define, 0.0.0).
    virtual QVersionNumber appVersion() const;

private:
    QSet<QString> loadGrantedPermissions(const QString &id) const;
    void saveGrantedPermissions(const QString &id, const QSet<QString> &granted);
    void writeEnabledState(const QString &id, bool enabled);

    QString m_systemPath;
    QString m_userPath;
    QList<PluginInfo> m_plugins;
    KSharedConfig::Ptr m_config;
    FactoryFn m_factoryOverride;
    PromptFn  m_promptHandler;
    ContextConfigurator m_contextConfigurator;
};

} // namespace Corbomite
