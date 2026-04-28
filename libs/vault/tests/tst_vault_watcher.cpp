// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/storage/FileSystemAdapter.h"

class TestVaultWatcher : public QObject
{
    Q_OBJECT
private slots:
    void externalCreateEmitsCreated();
    void externalModifyEmitsModified();
    void externalDeleteEmitsDeletedWithTombstone();
    void externalRenameEmitsRenamed();
    void unloadEmitsClosed();
    void externalChangeEmitsRaw();
    void externalConfigJsonEmitsConfigChanged();
    void externalNonConfigDoesNotEmitConfigChanged();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(body);
}
}

void TestVaultWatcher::externalCreateEmitsCreated()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::created);
    writeFile(dir.path() + "/new.md", "x");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalModifyEmitsModified()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::modified);
    // Ensure the mtime actually ticks forward across filesystem resolutions.
    QTest::qSleep(1100);
    writeFile(dir.path() + "/a.md", "two");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
}

void TestVaultWatcher::externalDeleteEmitsDeletedWithTombstone()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    auto *f = vault.getFileByPath(QStringLiteral("a.md"));
    QVERIFY(f);

    QSignalSpy spy(&vault, &Corbomite::Vault::deletedFile);
    QFile::remove(dir.path() + "/a.md");
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);

    auto *argPtr = qvariant_cast<Corbomite::TAbstractFile *>(spy.at(0).at(0));
    QVERIFY(argPtr);
    QCOMPARE(argPtr->deleted, true);
}

void TestVaultWatcher::externalRenameEmitsRenamed()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::renamed);
    QFile::rename(dir.path() + "/a.md", dir.path() + "/b.md");
    // Renamed or the fallback delete+create — both acceptable since rename
    // detection is a best-effort optimisation over identical mtime.
    QSignalSpy spyDel(&vault, &Corbomite::Vault::deletedFile);
    QSignalSpy spyNew(&vault, &Corbomite::Vault::created);
    QTRY_VERIFY_WITH_TIMEOUT(
        spy.count() >= 1 || (spyDel.count() >= 1 && spyNew.count() >= 1),
        5000);
}

void TestVaultWatcher::unloadEmitsClosed()
{
    QTemporaryDir dir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy spy(&vault, &Corbomite::Vault::closed);
    vault.unload();
    QCOMPARE(spy.count(), 1);
}

void TestVaultWatcher::externalChangeEmitsRaw()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "one");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QTest::qSleep(1100);
    writeFile(dir.path() + "/a.md", "two-different");
    QTRY_VERIFY_WITH_TIMEOUT(rawSpy.count() >= 1, 5000);
    QCOMPARE(rawSpy.first().at(0).toString(), QStringLiteral("a.md"));
}

void TestVaultWatcher::externalConfigJsonEmitsConfigChanged()
{
    QTemporaryDir dir;
    QDir().mkpath(dir.path() + "/.obsidian");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QSignalSpy cfgSpy(&vault, &Corbomite::Vault::configChanged);
    writeFile(dir.path() + "/.obsidian/appearance.json",
              R"({"theme":"obsidian"})");
    QTRY_VERIFY_WITH_TIMEOUT(cfgSpy.count() >= 1, 5000);
    QCOMPARE(cfgSpy.first().at(0).toString(),
             QStringLiteral(".obsidian/appearance.json"));
    QVERIFY(rawSpy.count() >= 1);
}

void TestVaultWatcher::externalNonConfigDoesNotEmitConfigChanged()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/note.md", "x");

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    QSignalSpy rawSpy(&vault, &Corbomite::Vault::raw);
    QSignalSpy cfgSpy(&vault, &Corbomite::Vault::configChanged);
    QTest::qSleep(1100);
    writeFile(dir.path() + "/note.md", "y-different");
    QTRY_VERIFY_WITH_TIMEOUT(rawSpy.count() >= 1, 5000);
    QCOMPARE(cfgSpy.count(), 0);
}

QTEST_MAIN(TestVaultWatcher)
#include "tst_vault_watcher.moc"
