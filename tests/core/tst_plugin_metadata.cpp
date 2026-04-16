// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <KPluginMetaData>
#include "corbomite/core/PluginMetaData.h"

class TestPluginMetaData : public QObject
{
    Q_OBJECT
private slots:
    void parsesPermissions();
    void parsesTrustedFlag();
    void parsesMinVersion();
    void absentKeysGiveDefaults();
    void userOriginForcesUntrusted();
};

static KPluginMetaData makeMeta(const QJsonObject &body)
{
    QJsonObject full = body;
    full.insert(QStringLiteral("KPlugin"),
        QJsonObject{{QStringLiteral("Id"), QStringLiteral("test-plugin")},
                    {QStringLiteral("Name"), QStringLiteral("Test")}});
    return KPluginMetaData(full, QStringLiteral("test-plugin"));
}

void TestPluginMetaData::parsesPermissions()
{
    QJsonObject body{
        {QStringLiteral("X-Corbomite-Permissions"),
         QJsonArray{QStringLiteral("vault.read"), QStringLiteral("ui.views")}}
    };
    Corbomite::PluginMetaData meta(makeMeta(body));
    const QStringList perms = meta.permissions();
    QCOMPARE(perms.size(), 2);
    QVERIFY(perms.contains(QStringLiteral("vault.read")));
    QVERIFY(perms.contains(QStringLiteral("ui.views")));
}

void TestPluginMetaData::parsesTrustedFlag()
{
    Corbomite::PluginMetaData trusted(makeMeta({
        {QStringLiteral("X-Corbomite-Trusted"), true}}));
    QVERIFY(trusted.trusted());

    Corbomite::PluginMetaData untrusted(makeMeta({
        {QStringLiteral("X-Corbomite-Trusted"), false}}));
    QVERIFY(!untrusted.trusted());
}

void TestPluginMetaData::parsesMinVersion()
{
    Corbomite::PluginMetaData meta(makeMeta({
        {QStringLiteral("X-Corbomite-MinVersion"), QStringLiteral("1.2.3")}}));
    QCOMPARE(meta.minAppVersion(), QVersionNumber(1, 2, 3));
}

void TestPluginMetaData::absentKeysGiveDefaults()
{
    Corbomite::PluginMetaData meta(makeMeta({}));
    QVERIFY(meta.permissions().isEmpty());
    QVERIFY(!meta.trusted());
    QVERIFY(meta.minAppVersion().isNull());
}

void TestPluginMetaData::userOriginForcesUntrusted()
{
    // A plugin in user path may declare Trusted: true in JSON, but
    // PluginMetaData::trusted() must return false once origin is set to User.
    Corbomite::PluginMetaData meta(makeMeta({
        {QStringLiteral("X-Corbomite-Trusted"), true}}));
    QVERIFY(meta.trusted()); // before origin set, JSON is honoured
    meta.setOrigin(Corbomite::PluginMetaData::Origin::User);
    QVERIFY(!meta.trusted()); // user-path plugins are never trusted
    meta.setOrigin(Corbomite::PluginMetaData::Origin::System);
    QVERIFY(meta.trusted()); // system-path keeps the JSON claim
}

QTEST_MAIN(TestPluginMetaData)
#include "tst_plugin_metadata.moc"
