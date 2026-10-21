// SPDX-License-Identifier: GPL-3.0-or-later
//
// The card view: a canvas text card is edited in Markoff's canvas leaf in
// embedded (card) mode, over the card's own session document. The leaf is a
// window onto the document — the document owns the text, and the projection
// it produces is what the card stores.
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QTest>
#include <QUndoStack>

#include <markoff/canvas/EditorWidget.h>
#include <markoff/core/MarkoffDocument.h>

#include "corbomite/canvas/CanvasDocument.h"
#include "corbomite/canvas/CanvasScene.h"
#include "corbomite/canvas/FileCardItem.h"
#include "corbomite/canvas/TextCardItem.h"
#include "corbomite/core/EditingSession.h"

class TestCanvasCardEditor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cardEditorIsTheMarkoffCardView();
    void cardEditStoresTheProjection();
    void focusInsideTheEditorKeepsTheEditAlive();
    void fileCardWritesTheProjection();
    void aCardWhoseDocumentCannotOpenIsNotEdited();
};

namespace {

/// A session source whose provider never opens: the card editor must not
/// start, because the document it would hold projects an empty text.
class FailingSessionSource : public Corbomite::EditingSessionSource
{
public:
    Corbomite::EditingBinding bindingFor(const Corbomite::EditingRequest &) override
    {
        return {};
    }
};

/// The card editor the scene put in its proxy, if any.
Markoff::Canvas::EditorWidget *cardEditor(Corbomite::Canvas::CanvasScene &scene)
{
    const QList<QGraphicsItem *> items = scene.items();
    for (QGraphicsItem *item : items) {
        auto *proxy = dynamic_cast<QGraphicsProxyWidget *>(item);
        if (!proxy)
            continue;
        if (auto *editor =
                dynamic_cast<Markoff::Canvas::EditorWidget *>(proxy->widget()))
            return editor;
    }
    return nullptr;
}

Corbomite::Canvas::CanvasNode makeTextNode(const QString &id, const QString &text)
{
    Corbomite::Canvas::CanvasNode node;
    node.id = id;
    node.type = Corbomite::Canvas::NodeType::Text;
    node.text = text;
    node.x = 100;
    node.y = 100;
    node.width = 200;
    node.height = 80;
    return node;
}

} // namespace

void TestCanvasCardEditor::cardEditorIsTheMarkoffCardView()
{
    Corbomite::Canvas::CanvasDocument doc;
    const Corbomite::Canvas::CanvasNode node =
        makeTextNode(QStringLiteral("card"), QStringLiteral("Hello card"));
    doc.addNode(node);

    Corbomite::Canvas::CanvasScene scene;
    scene.setDocument(&doc);
    scene.setSessionContext(QStringLiteral("/vault"),
                            QStringLiteral("board.canvas"));

    auto *item = scene.textCardItem(node.id);
    QVERIFY(item);
    emit item->editRequested();
    QVERIFY(scene.isEditing());

    Markoff::Canvas::EditorWidget *editor = cardEditor(scene);
    QVERIFY(editor);
    // The card view: embedded mode, promoted because this is the card being
    // edited, and bound to a document.
    QVERIFY(editor->embeddedMode());
    QVERIFY(!editor->isReadOnly());
    QVERIFY(editor->document());
    // The document carries the card's text, byte for byte.
    QCOMPARE(QString::fromUtf8(editor->document()->serializeForSave()),
             QStringLiteral("Hello card"));
}

void TestCanvasCardEditor::cardEditStoresTheProjection()
{
    Corbomite::Canvas::CanvasDocument doc;
    const Corbomite::Canvas::CanvasNode node =
        makeTextNode(QStringLiteral("card"), QStringLiteral("Hello card"));
    doc.addNode(node);

    Corbomite::Canvas::CanvasScene scene;
    scene.setDocument(&doc);
    scene.setSessionContext(QStringLiteral("/vault"),
                            QStringLiteral("board.canvas"));

    auto *item = scene.textCardItem(node.id);
    QVERIFY(item);
    emit item->editRequested();

    Markoff::Canvas::EditorWidget *editor = cardEditor(scene);
    QVERIFY(editor);

    const auto published =
        editor->document()->replaceMarkdown(QByteArray("Hello canvas"));
    QVERIFY(Corbomite::publicationCommitted(published));

    // The edit ends when the focus leaves the editor.
    emit qApp->focusChanged(editor, nullptr);
    QVERIFY(!scene.isEditing());

    // The card carries the projection, not a widget string.
    QCOMPARE(doc.node(node.id).text, QStringLiteral("Hello canvas"));

    // One undo step, and it restores the previous text.
    QCOMPARE(scene.undoStack()->count(), 1);
    scene.undoStack()->undo();
    QCOMPARE(doc.node(node.id).text, QStringLiteral("Hello card"));
}

void TestCanvasCardEditor::focusInsideTheEditorKeepsTheEditAlive()
{
    Corbomite::Canvas::CanvasDocument doc;
    const Corbomite::Canvas::CanvasNode node =
        makeTextNode(QStringLiteral("card"), QStringLiteral("Hello card"));
    doc.addNode(node);

    Corbomite::Canvas::CanvasScene scene;
    scene.setDocument(&doc);
    scene.setSessionContext(QStringLiteral("/vault"),
                            QStringLiteral("board.canvas"));

    auto *item = scene.textCardItem(node.id);
    QVERIFY(item);
    emit item->editRequested();

    Markoff::Canvas::EditorWidget *editor = cardEditor(scene);
    QVERIFY(editor);

    // The focus sits on the leaf's view — that is still the editor, so the
    // edit must not end.
    emit qApp->focusChanged(nullptr, editor->view());
    QVERIFY(scene.isEditing());

    // Leaving the editor ends the edit.
    emit qApp->focusChanged(editor->view(), nullptr);
    QVERIFY(!scene.isEditing());
}

void TestCanvasCardEditor::fileCardWritesTheProjection()
{
    Corbomite::Canvas::CanvasDocument doc;
    Corbomite::Canvas::CanvasNode node;
    node.id = QStringLiteral("filecard");
    node.type = Corbomite::Canvas::NodeType::File;
    node.file = QStringLiteral("note.md");
    node.x = 100;
    node.y = 100;
    node.width = 200;
    node.height = 80;
    doc.addNode(node);

    Corbomite::Canvas::CanvasScene scene;
    scene.setDocument(&doc);
    scene.setSessionContext(QStringLiteral("/vault"),
                            QStringLiteral("board.canvas"));
    scene.setFileResolver([](const QString &) {
        return QStringLiteral("# Title\n\nbody\n");
    });
    QString saved;
    scene.setFileSaver([&saved](const QString &, const QString &content) {
        saved = content;
    });

    auto *item = scene.fileCardItem(node.id);
    QVERIFY(item);
    emit item->editRequested();
    QVERIFY(scene.isEditing());

    Markoff::Canvas::EditorWidget *editor = cardEditor(scene);
    QVERIFY(editor);
    QCOMPARE(QString::fromUtf8(editor->document()->serializeForSave()),
             QStringLiteral("# Title\n\nbody\n"));

    const auto published = editor->document()->replaceMarkdown(
        QByteArray("# Title\n\nedited\n"));
    QVERIFY(Corbomite::publicationCommitted(published));

    emit qApp->focusChanged(editor, nullptr);
    QVERIFY(!scene.isEditing());

    // The file card writes the projection through the saver.
    QCOMPARE(saved, QStringLiteral("# Title\n\nedited\n"));
}

void TestCanvasCardEditor::aCardWhoseDocumentCannotOpenIsNotEdited()
{
    Corbomite::Canvas::CanvasDocument doc;
    const Corbomite::Canvas::CanvasNode node =
        makeTextNode(QStringLiteral("card"), QStringLiteral("Hello card"));
    doc.addNode(node);

    Corbomite::Canvas::CanvasScene scene;
    scene.setDocument(&doc);
    scene.setSessionContext(QStringLiteral("/vault"),
                            QStringLiteral("board.canvas"));

    FailingSessionSource failing;
    Corbomite::setEditingSessionSource(&failing);

    auto *item = scene.textCardItem(node.id);
    QVERIFY(item);
    emit item->editRequested();

    // No editor, and the card keeps its text: an empty projection must
    // never be written back over the card.
    QVERIFY(!scene.isEditing());
    QVERIFY(!cardEditor(scene));
    QCOMPARE(doc.node(node.id).text, QStringLiteral("Hello card"));

    Corbomite::setEditingSessionSource(nullptr);
}

QTEST_MAIN(TestCanvasCardEditor)
#include "tst_canvas_card_editor.moc"
