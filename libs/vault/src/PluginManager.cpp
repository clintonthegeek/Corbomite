// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginManager.h"

#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginPermissionGrantDialog.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KSharedConfig>

#include <QDebug>
#include <QJsonObject>
#include <QStandardPaths>

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

        const QVersionNumber required = meta.minAppVersion();
        if (!required.isNull() && appVersion() < required) {
            qWarning().noquote()
                << "PluginManager: skipping" << base.pluginId()
                << "— requires Corbomite >=" << required.toString()
                << "but this build is" << appVersion().toString();
            continue;
        }

        PluginInfo info;
        info.metaData = meta;
        m_plugins.append(info);
        Q_EMIT pluginDiscovered(base.pluginId());
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

bool PluginManager::enablePlugin(const QString &id)
{
    auto *info = const_cast<PluginInfo *>(pluginById(id));
    if (!info || info->instance) return false;

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
        plugin = factoryResult.plugin->create<Plugin>(this);
    }
    if (!plugin) {
        qWarning().noquote() << "PluginManager: factory returned nullptr for" << id;
        return false;
    }

    info->context = new PluginContext(info->metaData, granted);
    info->instance = plugin;
    plugin->load(info->context);
    info->enabled = true;

    writeEnabledState(id, true);
    Q_EMIT pluginLoaded(id);
    Q_EMIT pluginEnabled(id);
    return true;
}

bool PluginManager::disablePlugin(const QString &id)
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

    writeEnabledState(id, false);
    Q_EMIT pluginDisabled(id);
    return true;
}

void PluginManager::loadEnabledStateFromConfig()
{
    if (!m_config) return;
    KConfigGroup grp(m_config, QStringLiteral("Plugins"));
    for (auto &info : m_plugins) {
        const QString id = info.metaData.base().pluginId();
        const bool defaultOn = info.metaData.base().rawData()
            .value(QStringLiteral("KPlugin")).toObject()
            .value(QStringLiteral("EnabledByDefault")).toBool(false);
        const bool enabled = grp.readEntry(id + QStringLiteral("Enabled"), defaultOn);
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
    if (!m_config) return;
    KConfigGroup grp(m_config, QStringLiteral("Plugins"));
    grp.writeEntry(id + QStringLiteral("Enabled"), enabled);
    grp.sync();
}

} // namespace Corbomite
