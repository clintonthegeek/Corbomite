// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/core/Vault.h"
#include "corbomite/core/proxies/VaultReader.h"
#include "corbomite/core/proxies/VaultWriter.h"

class TestProxyVault : public QObject
{
    Q_OBJECT
private slots:
    void readerReadReturnsFileBytes();
    void readerReadReturnsEmptyOnMissingFile();
    void readerExistsReflectsOnDiskState();
    void readerListEnumeratesEntries();
    void readerRejectsAbsolutePaths();
    void readerRejectsParentEscapes();
    void writerCreateRejectsExisting();
    void writerCreatePersistsBytes();
    void writerCreateMakesParentDirs();
    void writerWriteOverwritesExisting();
    void writerRenameMovesFile();
    void writerRemoveDeletesFile();
};

void TestProxyVault::readerReadReturnsFileBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile f(dir.filePath(QStringLiteral("note.md")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("# Hello\n");
    f.close();

    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    QCOMPARE(reader.read(QStringLiteral("note.md")),
             QByteArrayLiteral("# Hello\n"));
}

void TestProxyVault::readerReadReturnsEmptyOnMissingFile()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    QCOMPARE(reader.read(QStringLiteral("nope.md")), QByteArray{});
}

void TestProxyVault::readerExistsReflectsOnDiskState()
{
    QTemporaryDir dir;
    QFile f(dir.filePath(QStringLiteral("a.md")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();

    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    QVERIFY(reader.exists(QStringLiteral("a.md")));
    QVERIFY(!reader.exists(QStringLiteral("b.md")));
}

void TestProxyVault::readerListEnumeratesEntries()
{
    QTemporaryDir dir;
    { QFile f(dir.filePath(QStringLiteral("a.md"))); QVERIFY(f.open(QIODevice::WriteOnly)); }
    { QFile f(dir.filePath(QStringLiteral("b.md"))); QVERIFY(f.open(QIODevice::WriteOnly)); }
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("sub")));

    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    const QStringList entries = reader.list();
    QVERIFY(entries.contains(QStringLiteral("a.md")));
    QVERIFY(entries.contains(QStringLiteral("b.md")));
    QVERIFY(entries.contains(QStringLiteral("sub")));
}

void TestProxyVault::readerRejectsAbsolutePaths()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    QCOMPARE(reader.read(QStringLiteral("/etc/passwd")), QByteArray{});
    QVERIFY(!reader.exists(QStringLiteral("/etc/passwd")));
}

void TestProxyVault::readerRejectsParentEscapes()
{
    QTemporaryDir dir;
    // Make a file outside the vault via a sibling temp dir.
    QTemporaryDir outside;
    QFile f(outside.filePath(QStringLiteral("leak.md")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("secret");
    f.close();

    Corbomite::Vault vault(dir.path());
    Corbomite::VaultReader reader(&vault);
    const QString escape =
        QStringLiteral("../") + QFileInfo(outside.path()).fileName()
        + QStringLiteral("/leak.md");
    QCOMPARE(reader.read(escape), QByteArray{});
}

void TestProxyVault::writerCreateRejectsExisting()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.create(QStringLiteral("x.md"), QByteArrayLiteral("a")));
    QVERIFY(!writer.create(QStringLiteral("x.md"), QByteArrayLiteral("b")));
}

void TestProxyVault::writerCreatePersistsBytes()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.create(QStringLiteral("x.md"), QByteArrayLiteral("hello")));
    QFile f(dir.filePath(QStringLiteral("x.md")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArrayLiteral("hello"));
}

void TestProxyVault::writerCreateMakesParentDirs()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.create(QStringLiteral("a/b/c.md"),
                          QByteArrayLiteral("x")));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a/b/c.md"))));
}

void TestProxyVault::writerWriteOverwritesExisting()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.write(QStringLiteral("x.md"), QByteArrayLiteral("one")));
    QVERIFY(writer.write(QStringLiteral("x.md"), QByteArrayLiteral("two")));
    QFile f(dir.filePath(QStringLiteral("x.md")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArrayLiteral("two"));
}

void TestProxyVault::writerRenameMovesFile()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.create(QStringLiteral("a.md"), QByteArrayLiteral("x")));
    QVERIFY(writer.rename(QStringLiteral("a.md"),
                          QStringLiteral("sub/b.md")));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("a.md"))));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("sub/b.md"))));
}

void TestProxyVault::writerRemoveDeletesFile()
{
    QTemporaryDir dir;
    Corbomite::Vault vault(dir.path());
    Corbomite::VaultWriter writer(&vault);
    QVERIFY(writer.create(QStringLiteral("x.md"), QByteArray{}));
    QVERIFY(writer.remove(QStringLiteral("x.md")));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("x.md"))));
}

QTEST_MAIN(TestProxyVault)
#include "tst_proxy_vault.moc"
