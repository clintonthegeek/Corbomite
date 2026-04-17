// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/PluginDataStore.h"

#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class tst_plugin_data_store : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip();
    void emptyOnMissing();
    void atomicOverwrite();
};

void tst_plugin_data_store::roundTrip()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());

    QJsonObject payload;
    payload.insert(QStringLiteral("count"), 42);
    payload.insert(QStringLiteral("label"), QStringLiteral("hello"));

    QVERIFY(store.save(payload));
    QJsonObject out = store.load();
    QCOMPARE(out.value(QStringLiteral("count")).toInt(), 42);
    QCOMPARE(out.value(QStringLiteral("label")).toString(), QStringLiteral("hello"));
}

void tst_plugin_data_store::emptyOnMissing()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());
    QCOMPARE(store.load().size(), 0);
}

void tst_plugin_data_store::atomicOverwrite()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    PluginDataStore store(dir.path());
    store.save(QJsonObject{ {QStringLiteral("v"), 1} });
    store.save(QJsonObject{ {QStringLiteral("v"), 2} });
    QCOMPARE(store.load().value(QStringLiteral("v")).toInt(), 2);
}

QTEST_APPLESS_MAIN(tst_plugin_data_store)
#include "tst_plugin_data_store.moc"
