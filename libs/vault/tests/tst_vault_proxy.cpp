// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

class TestVaultProxy : public QObject
{
    Q_OBJECT
private slots:
    void readRequiresReadPermission();
    void modifyRequiresWritePermission();
    void readPermissionGrantsAccess();
    void writePermissionGrantsAccess();
    void eventsSubscriptionRequiresEventsPermission();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(b);
    f.close();
}
}  // namespace

void TestVaultProxy::readRequiresReadPermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    Corbomite::VaultProxy proxy(&v, QSet<QString>{}, QStringLiteral("p"));
    QCOMPARE(proxy.read(v.getFileByPath(QStringLiteral("a.md"))), QByteArray());
}

void TestVaultProxy::modifyRequiresWritePermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    Corbomite::VaultProxy proxy(&v, QSet<QString>{QStringLiteral("vault.read")},
                                QStringLiteral("p"));
    QCOMPARE(proxy.modify(v.getFileByPath(QStringLiteral("a.md")),
                          QByteArray("y")),
             false);
}

void TestVaultProxy::readPermissionGrantsAccess()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    Corbomite::VaultProxy proxy(&v, QSet<QString>{QStringLiteral("vault.read")},
                                QStringLiteral("p"));
    QCOMPARE(proxy.read(v.getFileByPath(QStringLiteral("a.md"))),
             QByteArray("x"));
}

void TestVaultProxy::writePermissionGrantsAccess()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());

    Corbomite::VaultProxy proxy(&v,
                                QSet<QString>{QStringLiteral("vault.write")},
                                QStringLiteral("p"));
    QVERIFY(proxy.modify(v.getFileByPath(QStringLiteral("a.md")),
                         QByteArray("y")));
}

void TestVaultProxy::eventsSubscriptionRequiresEventsPermission()
{
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    Corbomite::VaultProxy proxy(&v, QSet<QString>{}, QStringLiteral("p"));
    const QUuid token = proxy.on(QStringLiteral("modify"), [](auto *) {});
    QVERIFY(token.isNull());
}

QTEST_MAIN(TestVaultProxy)
#include "tst_vault_proxy.moc"
