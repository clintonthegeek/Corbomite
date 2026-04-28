// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <KConfig>
#include <KSharedConfig>

#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"

namespace {

struct FakeStats
{
    int loaded = 0;
    int unloaded = 0;
    int externalSettingsChanges = 0;
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
public:
    void onExternalSettingsChange() override
    { ++m_stats->externalSettingsChanges; }
private:
    FakeStats *m_stats;
};

KPluginMetaData makeFixture(const QString &id)
{
    QJsonObject kplugin{
        {QStringLiteral("Id"), id},
        {QStringLiteral("Name"), id},
        {QStringLiteral("EnabledByDefault"), false}};
    QJsonObject full{
        {QStringLiteral("KPlugin"), kplugin},
        {QStringLiteral("X-Corbomite-Trusted"), true},
        {QStringLiteral("X-Corbomite-Permissions"), QJsonArray{}}};
    return KPluginMetaData(full, id);
}

} // namespace

class TestPluginExternalSettings : public QObject
{
    Q_OBJECT
private slots:
    void simulateInvokesVirtualOnEnabledPlugin();
    void simulateOnDisabledPluginIsNoOp();
    void simulateOnUnknownPluginIsNoOp();
};

void TestPluginExternalSettings::simulateInvokesVirtualOnEnabledPlugin()
{
    QTemporaryDir cfg;
    FakeStats stats;
    Corbomite::PluginManager mgr;
    mgr.setConfig(KSharedConfig::openConfig(
        cfg.filePath(QStringLiteral("corbomite-test.rc")),
        KConfig::SimpleConfig));
    mgr.setFactoryOverride([&stats](const Corbomite::PluginMetaData &)
                                -> Corbomite::Plugin * {
        return new FakePlugin(&stats);
    });
    mgr.ingest({makeFixture(QStringLiteral("plug-a"))},
                Corbomite::PluginMetaData::Origin::System);
    QVERIFY(mgr.enablePlugin(QStringLiteral("plug-a")));
    QCOMPARE(stats.externalSettingsChanges, 0);

    mgr.simulateExternalSettingsChange(QStringLiteral("plug-a"));
    QCOMPARE(stats.externalSettingsChanges, 1);

    mgr.simulateExternalSettingsChange(QStringLiteral("plug-a"));
    QCOMPARE(stats.externalSettingsChanges, 2);
}

void TestPluginExternalSettings::simulateOnDisabledPluginIsNoOp()
{
    QTemporaryDir cfg;
    FakeStats stats;
    Corbomite::PluginManager mgr;
    mgr.setConfig(KSharedConfig::openConfig(
        cfg.filePath(QStringLiteral("corbomite-test.rc")),
        KConfig::SimpleConfig));
    mgr.setFactoryOverride([&stats](const Corbomite::PluginMetaData &)
                                -> Corbomite::Plugin * {
        return new FakePlugin(&stats);
    });
    mgr.ingest({makeFixture(QStringLiteral("plug-b"))},
                Corbomite::PluginMetaData::Origin::System);
    mgr.enablePlugin(QStringLiteral("plug-b"));
    mgr.disablePlugin(QStringLiteral("plug-b"));
    stats.externalSettingsChanges = 0; // reset post-disable

    mgr.simulateExternalSettingsChange(QStringLiteral("plug-b"));
    QCOMPARE(stats.externalSettingsChanges, 0);
}

void TestPluginExternalSettings::simulateOnUnknownPluginIsNoOp()
{
    Corbomite::PluginManager mgr;
    mgr.simulateExternalSettingsChange(QStringLiteral("nonexistent"));
    // No assertion needed — just verifying no crash.
}

QTEST_MAIN(TestPluginExternalSettings)
#include "tst_plugin_external_settings.moc"
