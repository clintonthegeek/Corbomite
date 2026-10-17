// SPDX-License-Identifier: GPL-3.0-or-later
//
// Verifies callout rendering (`> [!note] ...`) actually fires along
// Corbomite's real document-load path, not just in a synthetic
// MarkoffDocument::loadFromMarkdown unit test (markoff-canvas's own
// tst_canvas_side_content already covers that in isolation).
//
// Vault::openDocument's first-open path calls NoteDocument::publishMarkdown,
// which is the initial import into the provider-backed document. This test
// uses that same call and confirms the canvas LivePreview leaf's
// View::isCalloutBlock() fires for the resulting block.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/canvas/EditorWidget.h>
#include <markoff/canvas/View.h>
#include <markoff/core/MarkoffDocument.h>

#include <QObject>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class CanvasCalloutLiveLoadTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void calloutBlockquote_rendersAsCallout_viaRealLoadPath()
    {
        NoteEditorWidget widget;
        widget.resize(600, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        QVERIFY(doc.publishMarkdown(
            QByteArrayLiteral("> [!note]\n> Some callout body text.\n")));

        widget.setNoteDocument(&doc);

        auto *canvasEditor = widget.canvasEditor();
        QVERIFY(canvasEditor);
        auto *view = canvasEditor->view();
        QVERIFY(view);

        const auto blocks = doc.markoff()->iterateBlocks();
        QVERIFY(!blocks.empty());
        QVERIFY2(view->isCalloutBlock(blocks.front()),
                 "callout blockquote failed to render as a typed callout "
                 "along Corbomite's real load path");
    }

    void plainBlockquote_isNotFlaggedAsCallout_viaRealLoadPath()
    {
        NoteEditorWidget widget;
        widget.resize(600, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note2.md"));
        QVERIFY(doc.publishMarkdown(
            QByteArrayLiteral("> Just a plain quote, no callout marker.\n")));

        widget.setNoteDocument(&doc);

        auto *canvasEditor = widget.canvasEditor();
        QVERIFY(canvasEditor);
        auto *view = canvasEditor->view();
        QVERIFY(view);

        const auto blocks = doc.markoff()->iterateBlocks();
        QVERIFY(!blocks.empty());
        QVERIFY(!view->isCalloutBlock(blocks.front()));
    }
};

QTEST_MAIN(CanvasCalloutLiveLoadTest)
#include "tst_canvas_callout_live_load.moc"
