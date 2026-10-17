// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M M6.2 acceptance — end to end through FileManager with the real
// CanvasLinkRewriter. Renaming a note referenced by a `.canvas` updates the
// canvas's structural references (file node, text-card links) and preserves
// unknown fields. This is the vault seam (tst_file_manager_rename) and the
// rewriter (tst_canvas_link_rewriter) composed for real.
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "canvas/CanvasLinkRewriter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

class TestCanvasRenameRewrite : public QObject
{
    Q_OBJECT
private slots:
    void renameUpdatesCanvasReferences();
};

namespace {
void writeFile(const QString &p, const QByteArray &b)
{
    QFile f(p);
    f.open(QIODevice::WriteOnly);
    f.write(b);
    f.close();
}
QByteArray readFileAll(const QString &p)
{
    QFile f(p);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}
}  // namespace

void TestCanvasRenameRewrite::renameUpdatesCanvasReferences()
{
    QTemporaryDir dir;
    writeFile(dir.path() + "/Foo.md", "Foo body");

    const QByteArray canvas = QByteArrayLiteral(
        R"({"custom":{"keep":1},"nodes":[)"
        R"({"id":"n1","type":"file","file":"Foo.md","x":0,"y":0,"width":250,"height":60},)"
        R"({"id":"t1","type":"text","text":"See [[Foo.md]]","x":0,"y":0,"width":250,"height":60})"
        R"(],"edges":[]})");
    writeFile(dir.path() + "/board.canvas", canvas);

    Corbomite::FileSystemAdapter fs;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());

    Corbomite::LinkResolver resolver;
    resolver.setVaultPaths({QStringLiteral("Foo.md"), QStringLiteral("board.canvas")});
    Corbomite::MetadataCache cache(resolver);
    cache.open(dir.path() + "/.corbomite/metadata-cache.db");
    QSignalSpy indexSpy(&cache, &Corbomite::MetadataCache::indexFinished);
    cache.rebuildVault(dir.path(),
                       {QStringLiteral("Foo.md"), QStringLiteral("board.canvas")});
    QTRY_VERIFY_WITH_TIMEOUT(indexSpy.count() >= 1, 10000);

    Corbomite::FileManager fm(&vault, &cache);
    Canvas::CanvasLinkRewriter rewriter;
    fm.registerLinkRewriter(&rewriter);

    auto *foo = vault.getFileByPath(QStringLiteral("Foo.md"));
    QVERIFY(foo);
    QVERIFY(fm.renameFile(foo, QStringLiteral("Bar.md")));

    const QJsonObject o = QJsonDocument::fromJson(readFileAll(dir.path() + "/board.canvas"))
                              .object();
    // Unknown top-level key intact.
    QCOMPARE(o.value(QStringLiteral("custom")).toObject()
                 .value(QStringLiteral("keep")).toInt(), 1);
    const auto nodes = o.value(QStringLiteral("nodes")).toArray();
    QCOMPARE(nodes.at(0).toObject().value(QStringLiteral("file")).toString(),
             QStringLiteral("Bar.md"));
    QCOMPARE(nodes.at(1).toObject().value(QStringLiteral("text")).toString(),
             QStringLiteral("See [[Bar.md]]"));
}

QTEST_MAIN(TestCanvasRenameRewrite)
#include "tst_canvas_rename_rewrite.moc"
