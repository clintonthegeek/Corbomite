// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include "corbomite/vault/PluginManager.h"

class TestPluginManager : public QObject
{
    Q_OBJECT
private slots:
    void discoversNothingInEmptyPaths();
    void ingestRecordsOrigin();
    void userOriginStripsTrustedClaim();
    void minVersionRejectsFutureRequirement();
    void pluginByIdRoundTrips();
    void pluginDiscoveredSignalFires();
};

static KPluginMetaData makeFixture(const QString &id, const QJsonObject &extra = {})
{
    QJsonObject kplugin{{QStringLiteral("Id"), id},
                        {QStringLiteral("Name"), id}};
    QJsonObject full = extra;
    full.insert(QStringLiteral("KPlugin"), kplugin);
    return KPluginMetaData(full, id);
}

void TestPluginManager::discoversNothingInEmptyPaths()
{
    Corbomite::PluginManager mgr;
    mgr.setSystemSearchPath(QStringLiteral("/nonexistent/system/path"));
    mgr.setUserSearchPath(QStringLiteral("/nonexistent/user/path"));
    mgr.discoverPlugins();
    QCOMPARE(mgr.pluginCount(), 0);
}

void TestPluginManager::ingestRecordsOrigin()
{
    Corbomite::PluginManager mgr;
    mgr.ingest({makeFixture(QStringLiteral("a-system-plugin"))},
               Corbomite::PluginMetaData::Origin::System);
    mgr.ingest({makeFixture(QStringLiteral("a-user-plugin"))},
               Corbomite::PluginMetaData::Origin::User);

    QCOMPARE(mgr.pluginCount(), 2);
    const auto *sys = mgr.pluginById(QStringLiteral("a-system-plugin"));
    const auto *usr = mgr.pluginById(QStringLiteral("a-user-plugin"));
    QVERIFY(sys != nullptr);
    QVERIFY(usr != nullptr);
    QCOMPARE(sys->metaData.origin(), Corbomite::PluginMetaData::Origin::System);
    QCOMPARE(usr->metaData.origin(), Corbomite::PluginMetaData::Origin::User);
}

void TestPluginManager::userOriginStripsTrustedClaim()
{
    // A plugin declaring X-Corbomite-Trusted: true in the user path
    // must have its trusted() claim flipped to false. System-path
    // plugins keep whatever they declared.
    const QJsonObject trustedDecl{
        {QStringLiteral("X-Corbomite-Trusted"), true}};
    Corbomite::PluginManager mgr;
    mgr.ingest({makeFixture(QStringLiteral("sys-trusted"), trustedDecl)},
               Corbomite::PluginMetaData::Origin::System);
    mgr.ingest({makeFixture(QStringLiteral("user-trusted"), trustedDecl)},
               Corbomite::PluginMetaData::Origin::User);

    QVERIFY(mgr.pluginById(QStringLiteral("sys-trusted"))->metaData.trusted());
    QVERIFY(!mgr.pluginById(QStringLiteral("user-trusted"))->metaData.trusted());
}

void TestPluginManager::minVersionRejectsFutureRequirement()
{
    const QJsonObject future{
        {QStringLiteral("X-Corbomite-MinVersion"), QStringLiteral("99.0.0")}};
    Corbomite::PluginManager mgr;

    // Expect a qWarning about incompat during ingest
    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression(QStringLiteral("future-plugin")));
    mgr.ingest({makeFixture(QStringLiteral("future-plugin"), future)},
               Corbomite::PluginMetaData::Origin::System);

    QCOMPARE(mgr.pluginCount(), 0);
}

void TestPluginManager::pluginByIdRoundTrips()
{
    Corbomite::PluginManager mgr;
    mgr.ingest({makeFixture(QStringLiteral("alpha")),
                makeFixture(QStringLiteral("beta"))},
               Corbomite::PluginMetaData::Origin::System);

    QCOMPARE(mgr.pluginCount(), 2);
    QVERIFY(mgr.pluginById(QStringLiteral("alpha")) != nullptr);
    QVERIFY(mgr.pluginById(QStringLiteral("beta")) != nullptr);
    QCOMPARE(mgr.pluginById(QStringLiteral("ghost")), nullptr);
}

void TestPluginManager::pluginDiscoveredSignalFires()
{
    Corbomite::PluginManager mgr;
    QSignalSpy spy(&mgr, &Corbomite::PluginManager::pluginDiscovered);
    mgr.ingest({makeFixture(QStringLiteral("signaled"))},
               Corbomite::PluginMetaData::Origin::System);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("signaled"));
}

QTEST_MAIN(TestPluginManager)
#include "tst_plugin_manager.moc"
