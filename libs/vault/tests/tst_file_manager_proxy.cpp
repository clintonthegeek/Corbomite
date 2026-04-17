// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"

class TestFileManagerProxy : public QObject
{
    Q_OBJECT
private slots:
    void renameRequiresWritePermission();
    void writePermissionGrantsRename();
    void frontMatterRequiresWritePermission();
    void queryRequiresReadPermission();
    void generateMarkdownLinkRequiresMetadataRead();
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

void TestFileManagerProxy::renameRequiresWritePermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&v, &cache);

    Corbomite::FileManagerProxy proxy(&fm, QSet<QString>{},
                                      QStringLiteral("p"));
    QCOMPARE(proxy.renameFile(v.getFileByPath(QStringLiteral("a.md")),
                              QStringLiteral("b.md")),
             false);
    // File should still be called a.md
    QVERIFY(v.getFileByPath(QStringLiteral("a.md")) != nullptr);
    QVERIFY(v.getFileByPath(QStringLiteral("b.md")) == nullptr);
}

void TestFileManagerProxy::writePermissionGrantsRename()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&v, &cache);

    Corbomite::FileManagerProxy proxy(
        &fm, QSet<QString>{QStringLiteral("vault.write")},
        QStringLiteral("p"));
    QVERIFY(proxy.renameFile(v.getFileByPath(QStringLiteral("a.md")),
                             QStringLiteral("b.md")));
    QVERIFY(v.getFileByPath(QStringLiteral("b.md")) != nullptr);
}

void TestFileManagerProxy::frontMatterRequiresWritePermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "---\ntitle: a\n---\nbody");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&v, &cache);

    Corbomite::FileManagerProxy proxy(&fm,
                                      QSet<QString>{QStringLiteral("vault.read")},
                                      QStringLiteral("p"));
    QCOMPARE(proxy.processFrontMatter(
                 v.getFileByPath(QStringLiteral("a.md")),
                 [](QVariantMap &m) { m[QStringLiteral("title")] = "b"; }),
             false);
}

void TestFileManagerProxy::queryRequiresReadPermission()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/a.md", "x");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&v, &cache);

    Corbomite::FileManagerProxy proxyUngranted(&fm, QSet<QString>{},
                                               QStringLiteral("p"));
    QCOMPARE(
        proxyUngranted.getAvailablePathForAttachment(QStringLiteral("x.png")),
        QString{});
    QVERIFY(proxyUngranted.getNewFileParent(QString{}) == nullptr);

    Corbomite::FileManagerProxy proxyGranted(
        &fm, QSet<QString>{QStringLiteral("vault.read")},
        QStringLiteral("p"));
    QVERIFY(proxyGranted.getNewFileParent(QString{}) != nullptr);
}

void TestFileManagerProxy::generateMarkdownLinkRequiresMetadataRead()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/target.md", "");
    writeFile(dir.path() + "/source.md", "");
    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault v(&fs);
    v.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&v, &cache);

    auto *target = v.getFileByPath(QStringLiteral("target.md"));

    Corbomite::FileManagerProxy ungranted(
        &fm, QSet<QString>{QStringLiteral("vault.read")}, QStringLiteral("p"));
    QCOMPARE(ungranted.generateMarkdownLink(target, QStringLiteral("source.md")),
             QString{});

    Corbomite::FileManagerProxy granted(
        &fm, QSet<QString>{QStringLiteral("metadata.read")},
        QStringLiteral("p"));
    QVERIFY(!granted.generateMarkdownLink(target, QStringLiteral("source.md"))
                 .isEmpty());
}

QTEST_MAIN(TestFileManagerProxy)
#include "tst_file_manager_proxy.moc"
