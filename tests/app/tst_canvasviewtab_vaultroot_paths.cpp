// SPDX-License-Identifier: GPL-3.0-or-later
//
// Punch-list P2 (audit-2026-06-10) — "Canvas file-card paths resolve
// relative to the canvas file's dir, not vault root". Obsidian's .canvas
// spec resolves file-node `file` paths against the vault root, so a
// .canvas file living in a subfolder (e.g. Projects/board.canvas) that
// references a file-card at Notes/idea.md must resolve to
// <vaultRoot>/Notes/idea.md, not <vaultRoot>/Projects/Notes/idea.md.
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "canvas/CanvasViewTab.h"
#include <canvas/CanvasScene.h>
#include <canvas/FileCardItem.h>
#include <corbomite/core/RegexRenderEngine.h>

using Corbomite::CanvasViewTab;

namespace {
QString canvasJsonReferencingNote()
{
    return QStringLiteral(R"({
        "nodes": [
            {"id":"f1","type":"file","file":"Notes/idea.md","x":0,"y":0,"width":250,"height":60}
        ],
        "edges": []
    })");
}
} // namespace

class TestCanvasViewTabVaultRootPaths : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void fileCardResolvesAgainstVaultRoot()
    {
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());
        const QString vaultRoot = vaultDir.path();

        // .canvas file lives in a subfolder of the vault.
        QVERIFY(QDir(vaultRoot).mkpath(QStringLiteral("Projects")));
        const QString canvasPath = vaultRoot + QStringLiteral("/Projects/board.canvas");
        QFile canvasFile(canvasPath);
        QVERIFY(canvasFile.open(QIODevice::WriteOnly | QIODevice::Text));
        canvasFile.write(canvasJsonReferencingNote().toUtf8());
        canvasFile.close();

        // Referenced note lives at vault-root-relative "Notes/idea.md",
        // NOT "Projects/Notes/idea.md".
        QVERIFY(QDir(vaultRoot).mkpath(QStringLiteral("Notes")));
        QFile noteFile(vaultRoot + QStringLiteral("/Notes/idea.md"));
        QVERIFY(noteFile.open(QIODevice::WriteOnly | QIODevice::Text));
        noteFile.write(QByteArrayLiteral("idea content"));
        noteFile.close();

        CanvasViewTab tab(canvasPath, vaultRoot);
        Corbomite::RegexRenderEngine engine;
        tab.setRenderEngine(&engine);

        auto *scene = tab.canvasScene();
        QVERIFY(scene);
        auto *card = scene->fileCardItem(QStringLiteral("f1"));
        QVERIFY(card);
        // Resolver found the note against vault root -> content rendered.
        QVERIFY(card->hasRenderedDocument());
    }

    void fileCardEmptyWhenResolvedAgainstCanvasDirOnly()
    {
        // Sanity check for the old (buggy) behavior: if the note only
        // exists at the vault-root-relative path and NOT nested under the
        // canvas file's own directory, resolving against the canvas dir
        // (vaultRoot left empty, forcing the fallback) must fail to find it.
        QTemporaryDir vaultDir;
        QVERIFY(vaultDir.isValid());
        const QString vaultRoot = vaultDir.path();

        QVERIFY(QDir(vaultRoot).mkpath(QStringLiteral("Projects")));
        const QString canvasPath = vaultRoot + QStringLiteral("/Projects/board.canvas");
        QFile canvasFile(canvasPath);
        QVERIFY(canvasFile.open(QIODevice::WriteOnly | QIODevice::Text));
        canvasFile.write(canvasJsonReferencingNote().toUtf8());
        canvasFile.close();

        QVERIFY(QDir(vaultRoot).mkpath(QStringLiteral("Notes")));
        QFile noteFile(vaultRoot + QStringLiteral("/Notes/idea.md"));
        QVERIFY(noteFile.open(QIODevice::WriteOnly | QIODevice::Text));
        noteFile.write(QByteArrayLiteral("idea content"));
        noteFile.close();

        // Empty vault root -> falls back to resolving against the canvas
        // file's own directory ("Projects/"), where "Notes/idea.md" does
        // not exist.
        CanvasViewTab tab(canvasPath, QString());
        Corbomite::RegexRenderEngine engine;
        tab.setRenderEngine(&engine);

        auto *scene = tab.canvasScene();
        QVERIFY(scene);
        auto *card = scene->fileCardItem(QStringLiteral("f1"));
        QVERIFY(card);
        QVERIFY(!card->hasRenderedDocument());
    }
};

QTEST_MAIN(TestCanvasViewTabVaultRootPaths)
#include "tst_canvasviewtab_vaultroot_paths.moc"
