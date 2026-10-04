// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M Phase M5.5 — "Canvas: Convert text card to file".
//
// The contract under test is Obsidian's, not ours. From the restored 1.13.7
// corpus:
//
//   app/own/2022-W7.js:390-424  TextCardItem.convertToFile — the whole op
//   app/own/1312-hR.js:81-89    fR, the name modal: validates, defaults to
//                               "Untitled", returns the trimmed name
//   app/own/2018-k7.js:749-758  createFileNode — copies pos/size, sets file
//   app/own/1985-R8.js:18       R8 + app/own/0618-mv.js:2 — pos+size resolve
//                               to the old node's own rect, so geometry is
//                               preserved and nothing else is carried over
//   app/own/2022-W7.js:405-416  edges are rebound to the new node *before*
//                               the old one is removed
//   app/own/2018-k7.js:481-490  undo is a whole-document snapshot restore
//
// Refutes the plan's worry about an embed: `createNewMarkdownFile(a, t, n)`
// passes the card's text as the file body unchanged — there is no wrapper,
// no frontmatter, no heading. The node is genuinely re-typed, not wrapped.
//
// The one thing Obsidian does NOT do is delete the created note on undo (see
// tests/app/tst_canvas_convert_to_file_vault.cpp, which pins that against a
// real FileManager write); nothing here can assert it, because the write is
// behind a seam.

#include <QTest>

#include <KLocalizedString>
#include <QUndoStack>

#include <QMenu>

#include "canvas/CanvasCommands.h"
#include "canvas/CanvasDocument.h"
#include "canvas/CanvasScene.h"
#include "canvas/FileCardItem.h"
#include "canvas/GroupItem.h"
#include "canvas/TextCardItem.h"

using namespace Canvas;

namespace {

/// Stands in for libs/vault's FileManager write (which libs/canvas may not
/// reach) and for the name modal, recording what it was asked for.
struct FakeConverter {
    QString name = QStringLiteral("Note");
    QString path = QStringLiteral("Note.md");
    QString askedName;
    QString askedContent;
    int calls = 0;

    void install(CanvasScene &scene)
    {
        scene.setFileNameRequestor([this]() {
            ++calls;
            return name;
        });
        scene.setMarkdownFileCreator([this](const QString &n, const QString &c) {
            askedName = n;
            askedContent = c;
            return path;
        });
    }
};

CanvasNode textNode(const QString &id, const QString &text)
{
    CanvasNode n;
    n.id = id;
    n.type = NodeType::Text;
    n.text = text;
    n.x = -400;
    n.y = 275;
    n.width = 333;
    n.height = 111;
    return n;
}

CanvasEdge edge(const QString &id, const QString &from, const QString &to)
{
    CanvasEdge e;
    e.id = id;
    e.fromNode = from;
    e.toNode = to;
    return e;
}

} // namespace

class TestCanvasConvertToFile : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
    }

    // The named M5.5 test: the node is re-typed, not wrapped, and it keeps
    // its geometry exactly.
    void testConvertToFileSwapsNodePreservingGeometry()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("body text")));

        const QString newId = scene.convertTextCardToFile(QStringLiteral("t1"));
        QVERIFY(!newId.isEmpty());
        QVERIFY(conv.calls == 1);
        QCOMPARE(conv.askedName, QStringLiteral("Note"));
        QCOMPARE(conv.askedContent, QStringLiteral("body text"));

        // The old node is gone; a file node took its place.
        QVERIFY(!doc.hasNode(QStringLiteral("t1")));
        QVERIFY(doc.hasNode(newId));
        const CanvasNode converted = doc.node(newId);
        QCOMPARE(converted.type, NodeType::File);
        QCOMPARE(converted.file, QStringLiteral("Note.md"));
        // No embed: the node points straight at the note, with no subpath.
        QVERIFY(converted.subpath.isEmpty());

        // Geometry is preserved verbatim.
        QCOMPARE(converted.x, -400);
        QCOMPARE(converted.y, 275);
        QCOMPARE(converted.width, 333);
        QCOMPARE(converted.height, 111);

        // A file card, not a text card, and it is the selection
        // (app/own/2022-W7.js:416 — o.selectOnly(l)).
        QVERIFY(scene.fileCardItem(newId));
        QVERIFY(!scene.textCardItem(newId));
        QCOMPARE(scene.selectedTextNodeId(), QString());
        QVERIFY(scene.fileCardItem(newId)->isSelected());
    }

    // The colour does NOT survive, because Obsidian's createFileNode is
    // handed only {pos, size, file, save, focus}
    // (app/own/2018-k7.js:405-410). Pin it so a later "preserve the card's
    // colour" change is a deliberate one.
    void testConvertToFileDropsCardColour()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        CanvasNode n = textNode(QStringLiteral("t1"), QStringLiteral("x"));
        n.color = QStringLiteral("4");
        doc.addNode(n);

        const QString newId = scene.convertTextCardToFile(QStringLiteral("t1"));
        QVERIFY(doc.node(newId).color.isEmpty());
    }

    // Every edge on the old node is rebound to the new one before the old
    // node is removed, so none is silently deleted with it
    // (app/own/2022-W7.js:405-415).
    void testConvertToFileRewiresEdgesToNewNode()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("x")));
        doc.addNode(textNode(QStringLiteral("other"), QStringLiteral("y")));
        doc.addEdge(edge(QStringLiteral("in"), QStringLiteral("other"),
                         QStringLiteral("t1")));
        doc.addEdge(edge(QStringLiteral("out"), QStringLiteral("t1"),
                         QStringLiteral("other")));

        const QString newId = scene.convertTextCardToFile(QStringLiteral("t1"));

        QCOMPARE(doc.edges().size(), 2);
        for (const CanvasEdge &e : doc.edges()) {
            QVERIFY(e.fromNode != QStringLiteral("t1"));
            QVERIFY(e.toNode != QStringLiteral("t1"));
        }
        QCOMPARE(doc.edge(QStringLiteral("in")).toNode, newId);
        QCOMPARE(doc.edge(QStringLiteral("out")).fromNode, newId);
    }

    // The whole conversion is ONE undo step. Obsidian gets this from its
    // snapshot history (app/own/2018-k7.js:81); our granular commands would
    // otherwise leave a stray file card behind after a single Ctrl+Z.
    void testConvertToFileIsOneUndoStep()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("body")));
        doc.addNode(textNode(QStringLiteral("other"), QStringLiteral("y")));
        doc.addEdge(edge(QStringLiteral("e1"), QStringLiteral("t1"),
                         QStringLiteral("other")));

        const QString newId = scene.convertTextCardToFile(QStringLiteral("t1"));
        QCOMPARE(scene.undoStack()->count(), 1);

        scene.undoStack()->undo();
        QCOMPARE(scene.undoStack()->index(), 0);

        QVERIFY(!doc.hasNode(newId));
        QVERIFY(doc.hasNode(QStringLiteral("t1")));
        const CanvasNode back = doc.node(QStringLiteral("t1"));
        QCOMPARE(back.type, NodeType::Text);
        QCOMPARE(back.text, QStringLiteral("body"));
        QCOMPARE(back.x, -400);
        QCOMPARE(back.y, 275);
        QCOMPARE(back.width, 333);
        QCOMPARE(back.height, 111);
        QCOMPARE(doc.edges().size(), 1);
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromNode, QStringLiteral("t1"));
        QVERIFY(scene.textCardItem(QStringLiteral("t1")));

        // ...and redo puts the converted card back, once.
        scene.undoStack()->redo();
        QVERIFY(doc.hasNode(newId));
        QVERIFY(!doc.hasNode(QStringLiteral("t1")));
        QCOMPARE(doc.edges().size(), 1);
        QCOMPARE(doc.edge(QStringLiteral("e1")).fromNode, newId);
    }

    void testConvertToFileCancellingThePromptWritesNothing()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.name = QString();   // cancelled
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("body")));

        QVERIFY(scene.convertTextCardToFile(QStringLiteral("t1")).isEmpty());
        QVERIFY(doc.hasNode(QStringLiteral("t1")));
        QCOMPARE(scene.undoStack()->count(), 0);
    }

    void testConvertToFileFailedWriteLeavesTheCardAlone()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.path = QString();   // write failed
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("body")));

        QVERIFY(scene.convertTextCardToFile(QStringLiteral("t1")).isEmpty());
        QVERIFY(doc.hasNode(QStringLiteral("t1")));
        QCOMPARE(scene.undoStack()->count(), 0);
    }

    void testConvertToFileRejectsNonTextNodes()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        CanvasNode g;
        g.id = QStringLiteral("g1");
        g.type = NodeType::Group;
        doc.addNode(g);

        QVERIFY(scene.convertTextCardToFile(QStringLiteral("g1")).isEmpty());
        QVERIFY(scene.convertTextCardToFile(QStringLiteral("nope")).isEmpty());
        QCOMPARE(conv.calls, 0);
    }

    void testConvertToFileRefusedWhileReadOnly()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        FakeConverter conv;
        conv.install(scene);

        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("body")));
        scene.setReadOnly(true);

        QVERIFY(scene.convertTextCardToFile(QStringLiteral("t1")).isEmpty());
        QVERIFY(doc.hasNode(QStringLiteral("t1")));
        QCOMPARE(conv.calls, 0);
    }

    // Obsidian's checkCallback (app/own/2028-n9.js:682-692) enables
    // canvas:convert-to-file only when the selection is exactly one text
    // node; selectedTextNodeId() is the action's Tier-B probe.
    void testSelectedTextNodeIdRequiresExactlyOneTextNode()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("a")));
        doc.addNode(textNode(QStringLiteral("t2"), QStringLiteral("b")));
        CanvasNode g;
        g.id = QStringLiteral("g1");
        g.type = NodeType::Group;
        doc.addNode(g);

        QVERIFY(scene.selectedTextNodeId().isEmpty());

        scene.textCardItem(QStringLiteral("t1"))->setSelected(true);
        QCOMPARE(scene.selectedTextNodeId(), QStringLiteral("t1"));

        scene.textCardItem(QStringLiteral("t2"))->setSelected(true);
        QVERIFY(scene.selectedTextNodeId().isEmpty());

        scene.clearSelection();
        scene.groupItem(QStringLiteral("g1"))->setSelected(true);
        QVERIFY(scene.selectedTextNodeId().isEmpty());
    }

    // The text card's own context menu carries the entry, hidden while
    // locked — app/own/2022-W7.js:381-388.
    void testTextCardContextMenuOffersConvertOnlyWhenWired()
    {
        CanvasDocument doc;
        CanvasScene scene;
        scene.setDocument(&doc);
        doc.addNode(textNode(QStringLiteral("t1"), QStringLiteral("a")));
        QVERIFY(scene.textCardItem(QStringLiteral("t1")));

        QMenu menu;
        scene.populateContextMenu(&menu, QPointF(-400 + 10, 275 + 10));
        QVERIFY(!hasAction(&menu, QStringLiteral("Convert to File...")));

        FakeConverter conv;
        conv.install(scene);
        QMenu wired;
        scene.populateContextMenu(&wired, QPointF(-400 + 10, 275 + 10));
        QVERIFY(hasAction(&wired, QStringLiteral("Convert to File...")));

        scene.setReadOnly(true);
        QMenu locked;
        scene.populateContextMenu(&locked, QPointF(-400 + 10, 275 + 10));
        QVERIFY(!hasAction(&locked, QStringLiteral("Convert to File...")));
    }

private:
    static bool hasAction(QMenu *menu, const QString &label)
    {
        for (const QAction *a : menu->actions())
            if (a->text() == label)
                return true;
        return false;
    }
};

QTEST_MAIN(TestCanvasConvertToFile)
#include "tst_canvas_convert_to_file.moc"