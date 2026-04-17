// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"

namespace {

/// Counters live outside the FakePlugin so tests can observe load/unload
/// after the plugin instance is destroyed (PluginManager owns + deletes it).
struct FakeStats
{
    int loaded = 0;
    int unloaded = 0;
};

class FakePlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    explicit FakePlugin(FakeStats *stats, QObject *parent = nullptr)
        : Corbomite::Plugin(parent), m_stats(stats) {}
protected:
    void onLoad(Corbomite::PluginContext *) override { ++m_stats->loaded; }
    void onUnload() override { ++m_stats->unloaded; }
private:
    FakeStats *m_stats;
};

KPluginMetaData makeFixture(const QString &id,
                              bool trusted,
                              const QStringList &perms)
{
    QJsonObject kplugin{
        {QStringLiteral("Id"), id},
        {QStringLiteral("Name"), id},
        {QStringLiteral("EnabledByDefault"), false}};
    QJsonArray permArr;
    for (const auto &p : perms) permArr.append(p);
    QJsonObject full{
        {QStringLiteral("KPlugin"), kplugin},
        {QStringLiteral("X-Corbomite-Trusted"), trusted},
        {QStringLiteral("X-Corbomite-Permissions"), permArr}};
    return KPluginMetaData(full, id);
}

/// Build a PluginManager wired up for test isolation:
/// - KSharedConfig redirected to a temp file
/// - factory override returning FakePlugin instances backed by per-id FakeStats
Corbomite::PluginManager *makeTestManager(QTemporaryDir &cfgDir,
                                            QHash<QString, FakeStats *> *stats)
{
    auto *mgr = new Corbomite::PluginManager;
    mgr->setConfig(KSharedConfig::openConfig(
        cfgDir.filePath(QStringLiteral("corbomite-test.rc")),
        KConfig::SimpleConfig));
    mgr->setFactoryOverride([stats](const Corbomite::PluginMetaData &meta)
                                -> Corbomite::Plugin * {
        FakeStats *s = nullptr;
        if (stats) {
            s = stats->value(meta.base().pluginId(), nullptr);
            if (!s) {
                s = new FakeStats;
                stats->insert(meta.base().pluginId(), s);
            }
        } else {
            s = new FakeStats; // leaked; only for tests that don't care
        }
        return new FakePlugin(s);
    });
    return mgr;
}

} // namespace

class TestPluginManagerLifecycle : public QObject
{
    Q_OBJECT
private slots:
    void enableTrustedPluginAutoGrantsAndLoads();
    void trustedPluginBypassesPromptHandler();
    void disablePluginUnloadsAndDeletes();
    void enableThenDisableSignalsFire();
    void kconfigPersistsEnabledState();
    void kconfigPersistsGrantedPermissions();
    void untrustedPluginWithPromptHandlerGetsPerms();
    void enablePluginUnknownIdReturnsFalse();
    void doubleEnableIsNoOp();
};

void TestPluginManagerLifecycle::enableTrustedPluginAutoGrantsAndLoads()
{
    QTemporaryDir cfg;
    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);
    mgr->ingest({makeFixture(QStringLiteral("trusted-a"), true,
                             {QStringLiteral("vault.read"),
                              QStringLiteral("ui.views")})},
                Corbomite::PluginMetaData::Origin::System);

    QVERIFY(mgr->enablePlugin(QStringLiteral("trusted-a")));
    const auto *info = mgr->pluginById(QStringLiteral("trusted-a"));
    QVERIFY(info != nullptr);
    QVERIFY(info->instance != nullptr);
    QVERIFY(info->context != nullptr);
    QVERIFY(info->enabled);

    // Auto-granted the full declared set (trusted path)
    const auto granted = info->context->grantedPermissions();
    QCOMPARE(granted.size(), 2);
    QVERIFY(granted.contains(QStringLiteral("vault.read")));
    QVERIFY(granted.contains(QStringLiteral("ui.views")));

    // Our fake plugin counted the onLoad call
    QCOMPARE(stats.value(QStringLiteral("trusted-a"))->loaded, 1);

    delete mgr;
    qDeleteAll(stats);
}

void TestPluginManagerLifecycle::trustedPluginBypassesPromptHandler()
{
    // Trust-skip contract: an in-tree plugin that ships
    // X-Corbomite-Trusted: true (via corbomite_add_plugin(... TRUSTED))
    // must never be routed to the permission-grant prompt, even when
    // the PluginManager has a handler registered. Its declared perms
    // are auto-granted on first enable.
    QTemporaryDir cfg;
    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);

    bool promptFired = false;
    mgr->setPromptHandler([&](const Corbomite::PluginMetaData &,
                               const QSet<QString> &declared) {
        promptFired = true;
        return declared;
    });

    mgr->ingest({makeFixture(QStringLiteral("trusted-b"), true,
                             {QStringLiteral("vault.read"),
                              QStringLiteral("network")})},
                Corbomite::PluginMetaData::Origin::System);

    QVERIFY(mgr->enablePlugin(QStringLiteral("trusted-b")));
    QVERIFY(!promptFired); // trusted path never consults the handler

    const auto *info = mgr->pluginById(QStringLiteral("trusted-b"));
    QVERIFY(info && info->context);
    // Both declared perms were auto-granted on the trusted path.
    QCOMPARE(info->context->grantedPermissions().size(), 2);
    QVERIFY(info->context->hasPermission(QStringLiteral("vault.read")));
    QVERIFY(info->context->hasPermission(QStringLiteral("network")));

    delete mgr;
    qDeleteAll(stats);
}

void TestPluginManagerLifecycle::disablePluginUnloadsAndDeletes()
{
    QTemporaryDir cfg;
    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);
    mgr->ingest({makeFixture(QStringLiteral("x"), true, {})},
                Corbomite::PluginMetaData::Origin::System);

    QVERIFY(mgr->enablePlugin(QStringLiteral("x")));
    auto *s = stats.value(QStringLiteral("x"));
    QVERIFY(s != nullptr);

    QVERIFY(mgr->disablePlugin(QStringLiteral("x")));
    QCOMPARE(s->unloaded, 1);

    const auto *info = mgr->pluginById(QStringLiteral("x"));
    QCOMPARE(info->instance, nullptr);
    QCOMPARE(info->context, nullptr);
    QVERIFY(!info->enabled);

    delete mgr;
    qDeleteAll(stats);
}

void TestPluginManagerLifecycle::enableThenDisableSignalsFire()
{
    QTemporaryDir cfg;
    auto *mgr = makeTestManager(cfg, nullptr);
    mgr->ingest({makeFixture(QStringLiteral("sig"), true, {})},
                Corbomite::PluginMetaData::Origin::System);

    QSignalSpy loadedSpy(mgr, &Corbomite::PluginManager::pluginLoaded);
    QSignalSpy enabledSpy(mgr, &Corbomite::PluginManager::pluginEnabled);
    QSignalSpy unloadingSpy(mgr, &Corbomite::PluginManager::pluginUnloading);
    QSignalSpy disabledSpy(mgr, &Corbomite::PluginManager::pluginDisabled);

    mgr->enablePlugin(QStringLiteral("sig"));
    QCOMPARE(loadedSpy.count(), 1);
    QCOMPARE(enabledSpy.count(), 1);

    mgr->disablePlugin(QStringLiteral("sig"));
    QCOMPARE(unloadingSpy.count(), 1);
    QCOMPARE(disabledSpy.count(), 1);

    delete mgr;
}

void TestPluginManagerLifecycle::kconfigPersistsEnabledState()
{
    QTemporaryDir cfg;
    const QString cfgPath = cfg.filePath(QStringLiteral("corbomite-test.rc"));

    {
        QHash<QString, FakeStats *> stats;
        auto *mgr = makeTestManager(cfg, &stats);
        mgr->ingest({makeFixture(QStringLiteral("persist"), true, {})},
                    Corbomite::PluginMetaData::Origin::System);
        mgr->enablePlugin(QStringLiteral("persist"));
        delete mgr; // triggers sync-on-destroy
    }

    // Reopen config, verify entry landed
    KSharedConfig::Ptr cfgPtr = KSharedConfig::openConfig(cfgPath, KConfig::SimpleConfig);
    KConfigGroup grp(cfgPtr, QStringLiteral("Plugins"));
    QVERIFY(grp.readEntry(QStringLiteral("persistEnabled"), false));

    // New manager: same ingest, should load it back via loadEnabledStateFromConfig()
    {
        QHash<QString, FakeStats *> stats;
        auto *mgr = makeTestManager(cfg, &stats);
        mgr->ingest({makeFixture(QStringLiteral("persist"), true, {})},
                    Corbomite::PluginMetaData::Origin::System);
        mgr->loadEnabledStateFromConfig();
        QVERIFY(mgr->pluginById(QStringLiteral("persist"))->enabled);
        delete mgr;
    }
}

void TestPluginManagerLifecycle::kconfigPersistsGrantedPermissions()
{
    QTemporaryDir cfg;
    const QString cfgPath = cfg.filePath(QStringLiteral("corbomite-test.rc"));

    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);
    mgr->ingest({makeFixture(QStringLiteral("untrust"), false,
                             {QStringLiteral("network")})},
                Corbomite::PluginMetaData::Origin::User);
    mgr->setPromptHandler([](const Corbomite::PluginMetaData &,
                              const QSet<QString> &declared) {
        return declared; // grant everything
    });
    QVERIFY(mgr->enablePlugin(QStringLiteral("untrust")));

    // After save, a fresh manager should recall the grant without prompting.
    delete mgr;

    KSharedConfig::Ptr cfgPtr = KSharedConfig::openConfig(cfgPath, KConfig::SimpleConfig);
    KConfigGroup grp(cfgPtr, QStringLiteral("PluginPermissions"));
    const QStringList saved = grp.readEntry(
        QStringLiteral("untrustGranted"), QStringList());
    QCOMPARE(saved.size(), 1);
    QVERIFY(saved.contains(QStringLiteral("network")));
}

void TestPluginManagerLifecycle::untrustedPluginWithPromptHandlerGetsPerms()
{
    QTemporaryDir cfg;
    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);

    bool promptFired = false;
    mgr->setPromptHandler([&](const Corbomite::PluginMetaData &,
                               const QSet<QString> &declared) {
        promptFired = true;
        // Grant a SUBSET — uncheck network
        QSet<QString> subset = declared;
        subset.remove(QStringLiteral("network"));
        return subset;
    });

    mgr->ingest({makeFixture(QStringLiteral("halfway"), false,
                             {QStringLiteral("vault.read"),
                              QStringLiteral("network")})},
                Corbomite::PluginMetaData::Origin::User);
    QVERIFY(mgr->enablePlugin(QStringLiteral("halfway")));
    QVERIFY(promptFired);

    const auto *info = mgr->pluginById(QStringLiteral("halfway"));
    QCOMPARE(info->context->grantedPermissions().size(), 1);
    QVERIFY(info->context->hasPermission(QStringLiteral("vault.read")));
    QVERIFY(!info->context->hasPermission(QStringLiteral("network")));

    delete mgr;
}

void TestPluginManagerLifecycle::enablePluginUnknownIdReturnsFalse()
{
    QTemporaryDir cfg;
    auto *mgr = makeTestManager(cfg, nullptr);
    QVERIFY(!mgr->enablePlugin(QStringLiteral("ghost")));
    delete mgr;
}

void TestPluginManagerLifecycle::doubleEnableIsNoOp()
{
    QTemporaryDir cfg;
    QHash<QString, FakeStats *> stats;
    auto *mgr = makeTestManager(cfg, &stats);
    mgr->ingest({makeFixture(QStringLiteral("dup"), true, {})},
                Corbomite::PluginMetaData::Origin::System);
    QVERIFY(mgr->enablePlugin(QStringLiteral("dup")));
    QVERIFY(!mgr->enablePlugin(QStringLiteral("dup"))); // already loaded
    QCOMPARE(stats.value(QStringLiteral("dup"))->loaded, 1);
    delete mgr;
    qDeleteAll(stats);
}

QTEST_MAIN(TestPluginManagerLifecycle)
#include "tst_plugin_manager_lifecycle.moc"
