// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultTrash : public QObject
{
    Q_OBJECT
private slots:
    void trashLocalMovesToDotTrash();
    void trashCollisionRenames();
};

void TestVaultTrash::trashLocalMovesToDotTrash()
{
    QTemporaryDir dir;
    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(vault.trash(tf, /*useSystem=*/false));
    QVERIFY(QFileInfo::exists(dir.path() + "/.trash/a.md"));
    QVERIFY(!QFileInfo::exists(dir.path() + "/a.md"));
}

void TestVaultTrash::trashCollisionRenames()
{
    QTemporaryDir dir;
    QVERIFY(QDir().mkpath(dir.path() + "/.trash"));
    QFile existing(dir.path() + "/.trash/a.md");
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("old");
    existing.close();

    QFile f(dir.path() + "/a.md");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("new");
    f.close();

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *tf = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(tf);
    QVERIFY(vault.trash(tf, false));
    QVERIFY(QFileInfo::exists(dir.path() + "/.trash/a 2.md"));
}

QTEST_MAIN(TestVaultTrash)
#include "tst_vault_trash.moc"
