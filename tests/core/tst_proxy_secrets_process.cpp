// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QProcess>

#include "corbomite/core/proxies/ProcessSpawner.h"
#include "corbomite/core/proxies/SecretStorage.h"

using namespace Corbomite;

class TestProxySecretsProcess : public QObject
{
    Q_OBJECT

private slots:
    // SecretStorage
    void secretRoundTripsForOnePlugin();
    void secretsListedSortedAndFiltered();
    void secretDeleteRemoves();
    void secretsIsolatedAcrossPlugins();
    void emptyIdRejected();

    // ProcessSpawner
    void startReturnsRunningQProcess();
    void startDetachedSucceedsForTrueProgram();
    void startDetachedFailsForBogusProgram();
};

// ---------- SecretStorage ------------------------------------------------

void TestProxySecretsProcess::secretRoundTripsForOnePlugin()
{
    SecretStorage s(QStringLiteral("plugin-roundtrip"));
    QVERIFY(s.setSecret(QStringLiteral("api-key"),
                        QStringLiteral("hunter2")));
    QCOMPARE(s.getSecret(QStringLiteral("api-key")),
             QStringLiteral("hunter2"));
}

void TestProxySecretsProcess::secretsListedSortedAndFiltered()
{
    SecretStorage s(QStringLiteral("plugin-listed"));
    s.setSecret(QStringLiteral("zeta"), QStringLiteral("v1"));
    s.setSecret(QStringLiteral("alpha"), QStringLiteral("v2"));
    s.setSecret(QStringLiteral("beta"), QStringLiteral("v3"));

    const QStringList list = s.listSecrets();
    QCOMPARE(list, QStringList({QStringLiteral("alpha"),
                                QStringLiteral("beta"),
                                QStringLiteral("zeta")}));
}

void TestProxySecretsProcess::secretDeleteRemoves()
{
    SecretStorage s(QStringLiteral("plugin-delete"));
    s.setSecret(QStringLiteral("k"), QStringLiteral("v"));
    QVERIFY(s.deleteSecret(QStringLiteral("k")));
    QVERIFY(s.getSecret(QStringLiteral("k")).isEmpty());
    QVERIFY(!s.deleteSecret(QStringLiteral("k"))); // already gone
}

void TestProxySecretsProcess::secretsIsolatedAcrossPlugins()
{
    SecretStorage a(QStringLiteral("plugin-a"));
    SecretStorage b(QStringLiteral("plugin-b"));
    a.setSecret(QStringLiteral("shared-key"), QStringLiteral("a-val"));
    b.setSecret(QStringLiteral("shared-key"), QStringLiteral("b-val"));

    QCOMPARE(a.getSecret(QStringLiteral("shared-key")),
             QStringLiteral("a-val"));
    QCOMPARE(b.getSecret(QStringLiteral("shared-key")),
             QStringLiteral("b-val"));
    QCOMPARE(a.listSecrets(), QStringList{QStringLiteral("shared-key")});
}

void TestProxySecretsProcess::emptyIdRejected()
{
    SecretStorage s(QStringLiteral("plugin-empty"));
    QVERIFY(!s.setSecret(QString(), QStringLiteral("v")));
}

// ---------- ProcessSpawner -----------------------------------------------

void TestProxySecretsProcess::startReturnsRunningQProcess()
{
    ProcessSpawner p(QStringLiteral("plugin-run"));
    QProcess *proc = p.start(QStringLiteral("true"));
    QVERIFY(proc != nullptr);
    QVERIFY(proc->waitForFinished(2000));
    QCOMPARE(proc->exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc->exitCode(), 0);
    delete proc;
}

void TestProxySecretsProcess::startDetachedSucceedsForTrueProgram()
{
    ProcessSpawner p(QStringLiteral("plugin-detach"));
    QVERIFY(p.startDetached(QStringLiteral("true")));
}

void TestProxySecretsProcess::startDetachedFailsForBogusProgram()
{
    ProcessSpawner p(QStringLiteral("plugin-bogus"));
    QVERIFY(!p.startDetached(
        QStringLiteral("/nonexistent/path/to/bogus-program-xyz")));
}

QTEST_MAIN(TestProxySecretsProcess)
#include "tst_proxy_secrets_process.moc"
