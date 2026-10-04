// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M Phase M5.5 — "Canvas: Convert text card to file", app layer.
//
// libs/canvas/tests/tst_canvas_convert_to_file.cpp covers the node swap
// behind a fake writer. This covers the two things only a real
// FileManager write can answer:
//
//  1. The note body is the card's text, VERBATIM. Obsidian's
//     `fileManager.createNewMarkdownFile(parent, name, cardText)`
//     (app/own/2022-W7.js:403) hands the card text straight to
//     `vault.create(path, content)` (app/own/1312-hR.js:332) — no embed, no
//     wrapper, no frontmatter, no added title. The plan's cluster comment
//     claimed this; verified here against the source and against bytes on
//     disk rather than taken on trust.
//
//  2. One undo restores the text card and does NOT delete the created note.
//     Obsidian cannot delete it: its canvas history is a stack of
//     whole-document snapshots (`k7`, app/own/2018-k7.js:2-23) restored by
//     applyHistory -> importData (`:481-490`), and the note lives in the
//     vault, outside that history entirely. Deleting on undo would also
//     destroy any edits made to the note since. So the file is orphaned by
//     design, and that is what this pins.
//
// The tab's name prompt is replaced with a fixed answer (a real
// QInputDialog::exec() is a nested event loop), while the write goes through
// the production FileManager::createNewMarkdownFile path.

#include <QTest>
#include <QTemporaryDir>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUndoStack>

#include "canvas/CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasScene.h>
#include <corbomite/storage/FileSystemAdapter.h>
#include <corbomite/vault/FileManager.h>
#include <corbomite/vault/TFile.h>
#include <corbomite/vault/Vault.h>

using namespace Corbomite;

namespace {

const QString kBoardJson = QStringLiteral(R"({
  "nodes": [
    {"id":"t1","type":"text","x":10,"y":20,"width":250,"height":60,
     "text":"# Heading\n\nBody **verbatim**.\n"},
    {"id":"t2","type":"text","x":400,"y":20,"width":250,"height":60,"text":"other"}
  ],
  "edges": [{"id":"e1","fromNode":"t1","fromSide":"right","toNode":"t2","toSide":"left"}]
})");

const QString kCardBody = QStringLiteral("# Heading\n\nBody **verbatim**.\n");

} // namespace

class TstCanvasConvertToFileVault : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<FileSystemAdapter> m_fs;
    std::unique_ptr<Vault> m_vault;
    std::unique_ptr<FileManager> m_fileManager;
    std::unique_ptr<CanvasViewTab> m_tab;

    // A fresh vault per test: each run creates Converted.md through the real
    // FileManager, whose collision numbering would otherwise make the later
    // tests see "Converted 2.md".
    bool openVaultWithBoard()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        if (!m_dir->isValid())
            return false;
        m_fs = std::make_unique<FileSystemAdapter>();
        m_vault = std::make_unique<Vault>(m_fs.get());
        m_vault->load(m_dir->path());
        m_fileManager = std::make_unique<FileManager>(m_vault.get(), nullptr);

        QFile board(m_dir->path() + QStringLiteral("/Board.canvas"));
        if (!board.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        board.write(kBoardJson.toUtf8());
        board.close();

        m_tab = std::make_unique<CanvasViewTab>(m_dir->path() + QStringLiteral("/Board.canvas"),
                                                m_dir->path());
        // The production name prompt pre-fills "Untitled" and re-asks on an
        // invalid name (app/own/1312-hR.js:81-89); only the modal is replaced.
        m_tab->setFileNameRequestor([]() { return QStringLiteral("Converted"); });
        m_tab->setMarkdownFileCreator(
            [this](const QString &name, const QString &content) -> QString {
                auto *f = m_fileManager->createNewMarkdownFile(
                    m_fileManager->vault()->getRoot(), name, content.toUtf8());
                return f ? f->path : QString();
            });
        return true;
    }

    void teardown()
    {
        m_tab.reset();
        m_fileManager.reset();
        m_vault.reset();
        m_fs.reset();
        m_dir.reset();
    }

    static QByteArray readAll(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return f.readAll();
    }

private Q_SLOTS:
    void init() { QVERIFY(openVaultWithBoard()); }
    void cleanup() { teardown(); }

    // The card's text is the note body, byte for byte.
    void convertToFileWritesTheCardBodyVerbatim()
    {
        auto *scene = m_tab->canvasScene();
        QVERIFY(scene);
        QVERIFY(scene->textCardItem(QStringLiteral("t1")));

        const QString newId = scene->convertTextCardToFile(QStringLiteral("t1"));
        QVERIFY(!newId.isEmpty());

        // Vault root, matching Obsidian's default newFileLocation "root"
        // (app/own/0920-eE.js:1205 for markdown).
        const QString written = m_dir->path() + QStringLiteral("/Converted.md");
        QVERIFY2(QFileInfo::exists(written), qPrintable(written));

        const QByteArray bytes = readAll(written);
        QCOMPARE(QString::fromUtf8(bytes), kCardBody);
        QVERIFY2(!bytes.contains("!["), "no embed: the node is re-typed, not wrapped");
        QVERIFY2(!bytes.startsWith("---"), "no frontmatter block is prepended");

        // ...and the canvas node points at that note.
        const Canvas::CanvasNode node = scene->document()->node(newId);
        QCOMPARE(node.type, Canvas::NodeType::File);
        QCOMPARE(node.file, QStringLiteral("Converted.md"));
    }

    // One Ctrl+Z brings the text card back — and Obsidian leaves the created
    // note on disk, so we do too. See the file header for why.
    void undoRestoresTheTextCardAndLeavesTheCreatedNote()
    {
        auto *scene = m_tab->canvasScene();
        QVERIFY(scene);

        const QString newId = scene->convertTextCardToFile(QStringLiteral("t1"));
        QVERIFY(!newId.isEmpty());
        QCOMPARE(scene->undoStack()->count(), 1);

        scene->undoStack()->undo();

        QVERIFY(!scene->document()->hasNode(newId));
        const Canvas::CanvasNode back = scene->document()->node(QStringLiteral("t1"));
        QCOMPARE(back.type, Canvas::NodeType::Text);
        QCOMPARE(back.text, kCardBody);
        QCOMPARE(back.x, 10);
        QCOMPARE(back.y, 20);
        QCOMPARE(back.width, 250);
        QCOMPARE(back.height, 60);
        // The edge survives, reattached to the restored text card.
        QCOMPARE(scene->document()->edges().size(), 1);
        QCOMPARE(scene->document()->edge(QStringLiteral("e1")).fromNode,
                 QStringLiteral("t1"));

        // The created note is NOT removed: Obsidian's canvas undo restores
        // document snapshots only, and the note is not part of that history.
        QVERIFY2(QFileInfo::exists(m_dir->path() + QStringLiteral("/Converted.md")),
                 "undo must not delete the note Obsidian leaves behind");
    }

    // The re-type is visible in the saved canvas, not just in memory: the
    // node array carries a `file` node where the `text` node was.
    void savedCanvasCarriesTheFileNode()
    {
        auto *scene = m_tab->canvasScene();
        QVERIFY(scene);
        const QString newId = scene->convertTextCardToFile(QStringLiteral("t1"));
        QVERIFY(!newId.isEmpty());
        QVERIFY(m_tab->save());

        const QJsonObject saved =
            QJsonDocument::fromJson(readAll(m_dir->path() + QStringLiteral("/Board.canvas")))
                .object();
        const QJsonArray nodes = saved.value(QStringLiteral("nodes")).toArray();
        QCOMPARE(nodes.size(), 2);

        QJsonObject converted;
        QJsonObject other;
        for (const QJsonValue &v : nodes) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("id")).toString() == newId)
                converted = o;
            else
                other = o;
        }
        QVERIFY(!converted.isEmpty());
        QCOMPARE(converted.value(QStringLiteral("type")).toString(), QStringLiteral("file"));
        QCOMPARE(converted.value(QStringLiteral("file")).toString(),
                 QStringLiteral("Converted.md"));
        QCOMPARE(converted.value(QStringLiteral("x")).toInt(), 10);
        QCOMPARE(converted.value(QStringLiteral("y")).toInt(), 20);
        QVERIFY2(!converted.contains(QStringLiteral("text")),
                 "a re-typed node must not keep the card's text field");
        QCOMPARE(other.value(QStringLiteral("type")).toString(), QStringLiteral("text"));
        QCOMPARE(saved.value(QStringLiteral("edges")).toArray().size(), 1);
    }
};

QTEST_MAIN(TstCanvasConvertToFileVault)
#include "tst_canvas_convert_to_file_vault.moc"