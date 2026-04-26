// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginManager.h"

#include "corbomite/core/PluginApi.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginPermissionGrantDialog.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDebug>
#include <QHash>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

namespace Corbomite {

namespace {

/// Default system search path. Mirrors Kate's convention of using a
/// named subdirectory of `${KDE_INSTALL_PLUGINDIR}`. We don't hard-code
/// the absolute prefix here — `KPluginMetaData::findPlugins` resolves
/// relative names against all of `QCoreApplication::libraryPaths()`.
constexpr auto kDefaultSystemSubdir = "corbomite";

/// Default user search path under XDG_DATA_HOME (writable by the user).
QString defaultUserPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
         + QStringLiteral("/corbomite/plugins");
}

/// Hard-coded mapping for the 8 in-tree internal plugins shipped at
/// Cluster Q close. Used as a fallback when the plugin's manifest does
/// not declare X-Obsidian-Id. Covers the canonical Corbomite plugin slugs.
/// See `docs/superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md`.
const QHash<QString, QString> &internalAliasDict()
{
    static const QHash<QString, QString> aliases = {
        {QStringLiteral("corbomite_backlinks"),    QStringLiteral("backlink")},
        {QStringLiteral("corbomite_outline"),      QStringLiteral("outline")},
        {QStringLiteral("corbomite_tag-pane"),     QStringLiteral("tag-pane")},
        {QStringLiteral("corbomite_word-count"),   QStringLiteral("word-count")},
        {QStringLiteral("corbomite_random-note"),  QStringLiteral("random-note")},
        {QStringLiteral("corbomite_filerecovery"), QStringLiteral("file-recovery")},
        {QStringLiteral("corbomite_starred"),      QStringLiteral("bookmarks")},
        // corbomite_note-stats has no Obsidian counterpart — intentionally absent.
    };
    return aliases;
}

} // namespace

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
    , m_systemPath(QString::fromLatin1(kDefaultSystemSubdir))
    , m_userPath(defaultUserPath())
    , m_config(KSharedConfig::openConfig())
{
}

PluginManager::~PluginManager()
{
    // Reverse-order unload (KDevelop convention). Unlike disablePlugin(), we
    // do NOT write Enabled=false to KConfig — shutdown should preserve the
    // user's choice for next launch.
    for (int i = m_plugins.size() - 1; i >= 0; --i) {
        auto &info = m_plugins[i];
        if (!info.instance) continue;
        Q_EMIT pluginUnloading(info.metaData.base().pluginId());
        info.instance->unload();
        delete info.instance;
        delete info.context;
        info.instance = nullptr;
        info.context = nullptr;
    }
    if (m_config) m_config->sync();
}

void PluginManager::setSystemSearchPath(const QString &path) { m_systemPath = path; }
void PluginManager::setUserSearchPath(const QString &path)   { m_userPath = path; }

void PluginManager::setConfig(KSharedConfig::Ptr config)
{
    m_config = std::move(config);
}

void PluginManager::setFactoryOverride(FactoryFn fn)
{
    m_factoryOverride = std::move(fn);
}

void PluginManager::setPromptHandler(PromptFn fn)
{
    m_promptHandler = std::move(fn);
}

void PluginManager::setContextConfigurator(ContextConfigurator fn)
{
    m_contextConfigurator = std::move(fn);
}

void PluginManager::setVaultConfig(VaultConfig *vcfg)
{
    m_vaultConfig = vcfg;
}

QString PluginManager::obsidianIdFor(const QString &corbomiteId) const
{
    if (const auto *info = pluginById(corbomiteId)) {
        const QString fromManifest = info->metaData.obsidianId();
        if (!fromManifest.isEmpty()) return fromManifest;
    }
    return internalAliasDict().value(corbomiteId);
}

QVersionNumber PluginManager::appVersion() const
{
#ifdef CORBOMITE_APP_VERSION
    return QVersionNumber::fromString(QStringLiteral(CORBOMITE_APP_VERSION));
#else
    return {0, 0, 0};
#endif
}

void PluginManager::ingest(const QList<KPluginMetaData> &metas,
                            PluginMetaData::Origin origin)
{
    for (const auto &base : metas) {
        PluginMetaData meta(base);
        meta.setOrigin(origin);
        const QString id = base.pluginId();

        PluginInfo info;
        info.metaData = meta;
        m_plugins.append(info);

        // Compute the gate state up front so PluginsPage can render a
        // "requires Corbomite >= X.Y.Z" / "requires plugin API level >= N"
        // row without anyone having to click enable. The gate is also
        // re-checked in enablePlugin() as a defence-in-depth measure.
        LoadState state = LoadState::Compatible;
        const QVersionNumber required = meta.minAppVersion();
        if (!required.isNull() && appVersion() < required) {
            state = LoadState::IncompatibleVersion;
        } else if (meta.apiLevel() > CORBOMITE_PLUGIN_API_LEVEL) {
            state = LoadState::IncompatibleApiLevel;
        }
        m_loadState.insert(id, state);

        Q_EMIT pluginDiscovered(id);
    }
}

void PluginManager::discoverPlugins()
{
    m_plugins.clear();
    ingest(KPluginMetaData::findPlugins(m_systemPath),
           PluginMetaData::Origin::System);
    ingest(KPluginMetaData::findPlugins(m_userPath),
           PluginMetaData::Origin::User);
}

const PluginManager::PluginInfo *PluginManager::pluginById(const QString &id) const
{
    for (const auto &info : m_plugins) {
        if (info.metaData.base().pluginId() == id) return &info;
    }
    return nullptr;
}

PluginManager::LoadState PluginManager::loadState(const QString &id) const
{
    return m_loadState.value(id, LoadState::Compatible);
}

bool PluginManager::enablePlugin(const QString &id)
{
    auto *info = const_cast<PluginInfo *>(pluginById(id));
    if (!info || info->instance) return false;

    // MinVersion / ApiLevel compat gate. Runs BEFORE the permission-grant
    // dialog so we never prompt the user for a plugin we'll refuse to
    // load anyway. Warning log matches the id so test fixtures can
    // QTest::ignoreMessage() the expected output.
    const QVersionNumber required = info->metaData.minAppVersion();
    if (!required.isNull() && appVersion() < required) {
        m_loadState.insert(id, LoadState::IncompatibleVersion);
        qWarning().noquote()
            << "PluginManager:" << id << "requires Corbomite >="
            << required.toString() << "; host is" << appVersion().toString();
        return false;
    }
    if (info->metaData.apiLevel() > CORBOMITE_PLUGIN_API_LEVEL) {
        m_loadState.insert(id, LoadState::IncompatibleApiLevel);
        qWarning().noquote()
            << "PluginManager:" << id << "declares API level"
            << info->metaData.apiLevel()
            << "; host supports up to" << CORBOMITE_PLUGIN_API_LEVEL;
        return false;
    }

    // Determine granted permissions.
    QSet<QString> granted;
    const QStringList declaredList = info->metaData.permissions();
    const QSet<QString> declared(declaredList.begin(), declaredList.end());

    if (info->metaData.trusted()) {
        granted = declared; // auto-grant
    } else {
        granted = loadGrantedPermissions(id);
        QSet<QString> ungranted = declared;
        ungranted.subtract(granted);
        if (!ungranted.isEmpty()) {
            // Untrusted plugin with one or more ungranted declared perms.
            QSet<QString> userGranted;
            if (m_promptHandler) {
                userGranted = m_promptHandler(info->metaData, declared);
            } else {
                PluginPermissionGrantDialog dlg(info->metaData.base().name(),
                                                 info->metaData.base().description(),
                                                 declaredList);
                if (dlg.exec() != QDialog::Accepted || dlg.wasCancelled()) {
                    return false;
                }
                userGranted = dlg.grantedIfAccepted();
            }
            if (userGranted.isEmpty() && !declared.isEmpty()) {
                // User cancelled or unchecked everything; abort enable.
                return false;
            }
            granted = userGranted;
            saveGrantedPermissions(id, granted);
        }
    }

    // Construct the Plugin instance.
    Plugin *plugin = nullptr;
    if (m_factoryOverride) {
        plugin = m_factoryOverride(info->metaData);
    } else {
        const auto factoryResult = KPluginFactory::loadFactory(info->metaData.base());
        if (!factoryResult.plugin) {
            qWarning().noquote() << "PluginManager: failed to load factory for"
                                  << id << "—" << factoryResult.errorString;
            return false;
        }
        // KPluginFactory::create<T> looks up the registered class by T's
        // metaobject name. Pass the BacklinksPlugin/etc subclass name
        // (resolved later) — for now use the typeless virtual create()
        // with the empty iface, which makes KPluginFactory return the
        // first registered plugin class.
        QObject *raw = factoryResult.plugin->create<QObject>(this);
        plugin = qobject_cast<Plugin *>(raw);
        if (!plugin && raw) {
            qWarning().noquote() << "PluginManager: factory for" << id
                << "produced a" << raw->metaObject()->className()
                << "which is not a Corbomite::Plugin";
            delete raw;
        }
    }
    if (!plugin) {
        qWarning().noquote() << "PluginManager: factory returned nullptr for" << id;
        return false;
    }

    info->context = new PluginContext(info->metaData, granted);
    if (m_contextConfigurator) m_contextConfigurator(info->context);
    info->instance = plugin;
    plugin->load(info->context);
    info->enabled = true;

    writeEnabledState(id, true);
    Q_EMIT pluginLoaded(id);
    Q_EMIT pluginEnabled(id);
    return true;
}

bool PluginManager::disablePlugin(const QString &id, bool persist)
{
    auto *info = const_cast<PluginInfo *>(pluginById(id));
    if (!info || !info->instance) return false;

    Q_EMIT pluginUnloading(id);

    info->instance->unload();
    delete info->instance;
    delete info->context;
    info->instance = nullptr;
    info->context = nullptr;
    info->enabled = false;

    if (persist) writeEnabledState(id, false);
    Q_EMIT pluginDisabled(id);
    return true;
}

void PluginManager::loadEnabledStateFromConfig()
{
    if (!m_config) return;
    KConfigGroup grp(m_config, QStringLiteral("Plugins"));

    // Overlay vault-side state first: if `.obsidian/core-plugins.json` or
    // `community-plugins.json` exists for an Obsidian-mapped plugin, that
    // state wins on vault-open. Plugins lacking an Obsidian counterpart
    // are unaffected. See spec §2 ("JSON wins on vault-open").
    QJsonObject coreState;
    QStringList communityList;
    bool communityFilePresent = false;
    if (m_vaultConfig) {
        if (auto core = m_vaultConfig->readCorePlugins())
            coreState = core->raw;
        if (auto comm = m_vaultConfig->readCommunityPlugins()) {
            communityList = *comm;
            communityFilePresent = true;
        }
    }

    for (auto &info : m_plugins) {
        const QString id = info.metaData.base().pluginId();
        // Embedded metadata via Q_PLUGIN_METADATA flattens KPlugin's
        // children to the rawData top level — KPluginMetaData::
        // isEnabledByDefault() walks both layouts. Trusted built-ins
        // are always on by default; untrusted plugins fall back to the
        // metadata-declared EnabledByDefault flag.
        const bool defaultOn = info.metaData.trusted()
            || info.metaData.base().isEnabledByDefault();
        bool enabled = grp.readEntry(id + QStringLiteral("Enabled"), defaultOn);

        const QString obsId = obsidianIdFor(id);
        if (!obsId.isEmpty()) {
            if (info.metaData.trusted()) {
                if (coreState.contains(obsId))
                    enabled = coreState.value(obsId).toBool();
            } else if (communityFilePresent) {
                // community-plugins.json is presence-encoded. Only override
                // when the file existed; a missing file means "no Obsidian
                // opinion", keep KConfig.
                enabled = communityList.contains(obsId);
            }
        }

        if (enabled) enablePlugin(id);
    }
}

QSet<QString> PluginManager::loadGrantedPermissions(const QString &id) const
{
    if (!m_config) return {};
    KConfigGroup grp(m_config, QStringLiteral("PluginPermissions"));
    const QStringList list = grp.readEntry(
        id + QStringLiteral("Granted"), QStringList());
    return QSet<QString>(list.begin(), list.end());
}

void PluginManager::saveGrantedPermissions(const QString &id,
                                            const QSet<QString> &granted)
{
    if (!m_config) return;
    KConfigGroup grp(m_config, QStringLiteral("PluginPermissions"));
    QStringList list(granted.begin(), granted.end());
    list.sort(); // deterministic on disk
    grp.writeEntry(id + QStringLiteral("Granted"), list);
    grp.sync();
}

void PluginManager::writeEnabledState(const QString &id, bool enabled)
{
    if (m_config) {
        KConfigGroup grp(m_config, QStringLiteral("Plugins"));
        grp.writeEntry(id + QStringLiteral("Enabled"), enabled);
        grp.sync();
    }

    // Mirror to .obsidian/{core,community}-plugins.json for plugins with
    // an Obsidian counterpart. Skipped silently when no VaultConfig is
    // attached or no mapping exists. See spec §4 ("dual-write on toggle").
    if (!m_vaultConfig) return;
    const QString obsId = obsidianIdFor(id);
    if (obsId.isEmpty()) return;

    const auto *info = pluginById(id);
    if (!info) return;
    if (info->metaData.trusted()) {
        VaultConfig::CorePlugins cur =
            m_vaultConfig->readCorePlugins().value_or(VaultConfig::CorePlugins{});
        cur.raw.insert(obsId, enabled);
        m_vaultConfig->writeCorePlugins(cur);
    } else {
        QStringList cur = m_vaultConfig->readCommunityPlugins().value_or(QStringList{});
        cur.removeAll(obsId);
        if (enabled) cur.append(obsId);
        m_vaultConfig->writeCommunityPlugins(cur);
    }
}

} // namespace Corbomite
