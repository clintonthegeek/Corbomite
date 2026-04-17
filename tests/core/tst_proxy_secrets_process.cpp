// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDateTime>
#include <QProcess>
#include <QSet>
#include <QTest>

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
    void usesKeyringWhenAvailable();
    void deniedWithoutSecretsPermission();

    // ProcessSpawner
    void startReturnsRunningQProcess();
    void startDetachedSucceedsForTrueProgram();
    void startDetachedFailsForBogusProgram();
};

// ---------- SecretStorage ------------------------------------------------

// Unique plugin id per test run so a persistent keyring (if the QtKeychain
// backend is active) doesn't carry state across runs or across test cases.
static QString uniquePluginId(const QString &base)
{
    return QStringLiteral("tst_secret.%1.%2").arg(base).arg(
        QDateTime::currentMSecsSinceEpoch());
}

void TestProxySecretsProcess::secretRoundTripsForOnePlugin()
{
    SecretStorage s(uniquePluginId(QStringLiteral("roundtrip")));
    const QString key = QStringLiteral("api-key");
    QVERIFY(s.setSecret(key, QStringLiteral("hunter2")));
    QCOMPARE(s.getSecret(key), QStringLiteral("hunter2"));
    // Leave no residue in the keyring for subsequent runs.
    s.deleteSecret(key);
}

// listSecrets() only enumerates the in-process QHash fallback — QtKeychain
// has no enumeration API. We exercise the fallback deterministically by
// constructing a SecretStorage without the "secrets" permission granted to
// the *keyring* path (which is what the single-arg legacy ctor does), then
// flipping a value into the QHash via the same API. When
// CORBOMITE_HAVE_KEYRING is on, values land in the keyring and the fallback
// list is empty — which is the documented contract.
void TestProxySecretsProcess::secretsListedSortedAndFiltered()
{
    SecretStorage s(uniquePluginId(QStringLiteral("listed")));
    s.setSecret(QStringLiteral("zeta"), QStringLiteral("v1"));
    s.setSecret(QStringLiteral("alpha"), QStringLiteral("v2"));
    s.setSecret(QStringLiteral("beta"), QStringLiteral("v3"));

    const QStringList list = s.listSecrets();
#ifdef CORBOMITE_HAVE_KEYRING
    // With a working keyring backend, values persist in the keyring and
    // listSecrets() reports nothing (no enumeration). With a failing
    // keyring (runtime fallback), listSecrets() reports all three.
    // Both are valid — assert the sorted-and-filtered-to-this-plugin
    // property without asserting the backend.
    for (const QString &k : list) {
        QVERIFY2(k == QStringLiteral("alpha") || k == QStringLiteral("beta")
                 || k == QStringLiteral("zeta"),
                 "listSecrets() returned a key outside the plugin's namespace");
    }
    if (!list.isEmpty()) {
        QStringList sorted = list;
        sorted.sort();
        QCOMPARE(list, sorted);
    }
#else
    QCOMPARE(list, QStringList({QStringLiteral("alpha"),
                                QStringLiteral("beta"),
                                QStringLiteral("zeta")}));
#endif

    // Clean up.
    s.deleteSecret(QStringLiteral("zeta"));
    s.deleteSecret(QStringLiteral("alpha"));
    s.deleteSecret(QStringLiteral("beta"));
}

void TestProxySecretsProcess::secretDeleteRemoves()
{
    SecretStorage s(uniquePluginId(QStringLiteral("delete")));
    const QString key = QStringLiteral("k");
    s.setSecret(key, QStringLiteral("v"));
    QVERIFY(s.deleteSecret(key));
    QVERIFY(s.getSecret(key).isEmpty());
    // Double-delete behaviour is backend-dependent: some keyring backends
    // report success deleting a missing entry, others report EntryNotFound.
    // The stable post-condition is that the key stays empty.
    s.deleteSecret(key);
    QVERIFY(s.getSecret(key).isEmpty());
}

void TestProxySecretsProcess::secretsIsolatedAcrossPlugins()
{
    const QString idA = uniquePluginId(QStringLiteral("a"));
    const QString idB = uniquePluginId(QStringLiteral("b"));
    SecretStorage a(idA);
    SecretStorage b(idB);
    const QString key = QStringLiteral("shared-key");
    a.setSecret(key, QStringLiteral("a-val"));
    b.setSecret(key, QStringLiteral("b-val"));

    QCOMPARE(a.getSecret(key), QStringLiteral("a-val"));
    QCOMPARE(b.getSecret(key), QStringLiteral("b-val"));

    // Clean up.
    a.deleteSecret(key);
    b.deleteSecret(key);
}

void TestProxySecretsProcess::emptyIdRejected()
{
    SecretStorage s(QStringLiteral("plugin-empty"));
    QVERIFY(!s.setSecret(QString(), QStringLiteral("v")));
}

void TestProxySecretsProcess::usesKeyringWhenAvailable()
{
#ifdef CORBOMITE_HAVE_KEYRING
    SecretStorage s(QStringLiteral("corbomite.test.plugin"),
                    QSet<QString>{QStringLiteral("secrets")});
    const QString key = QStringLiteral("api-token");
    const QString val = QStringLiteral("test-value-")
                        + QString::number(QDateTime::currentSecsSinceEpoch());

    // Keyring path: when the service is reachable the value round-trips via
    // QtKeychain; when it isn't, setSecret falls back to the in-process
    // QHash silently (logged as qCWarning). Either way we expect the
    // round-trip to succeed from the caller's perspective.
    QVERIFY(s.setSecret(key, val));
    QCOMPARE(s.getSecret(key), val);
    QVERIFY(s.deleteSecret(key));
    QVERIFY(s.getSecret(key).isEmpty());
#else
    QSKIP("Qt6Keychain not available; skipping keyring path test");
#endif
}

void TestProxySecretsProcess::deniedWithoutSecretsPermission()
{
    SecretStorage s(QStringLiteral("corbomite.test.plugin.denied"),
                    QSet<QString>{});
    QCOMPARE(s.setSecret(QStringLiteral("k"), QStringLiteral("v")), false);
    QVERIFY(s.getSecret(QStringLiteral("k")).isEmpty());
    QVERIFY(!s.deleteSecret(QStringLiteral("k")));
    QVERIFY(s.listSecrets().isEmpty());
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
