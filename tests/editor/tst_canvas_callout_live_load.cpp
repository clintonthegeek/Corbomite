// SPDX-License-Identifier: GPL-3.0-or-later
//
// Verifies callout rendering (`> [!note] ...`) actually fires along
// Corbomite's real document-load path, not just in a synthetic
// MarkoffDocument::loadFromMarkdown unit test (markoff-canvas's own
// tst_canvas_side_content already covers that in isolation).
//
// Vault::openDocument's FirstOpen path calls
// `doc->markoff()->loadFromMarkdown(bytes)` (see Vault.cpp) specifically
// because plain resetContent() only populates the legacy flat buffer, not
// the D2 per-block CRDT state that `iterateBlocks()`/View/BlockPresentation
// consume — the same class of gap that caused the 2026-08 image-block
// promotion bug. This test reproduces that exact call sequence (not
// NoteDocument::setMarkdown(), which still routes through resetContent)
// and confirms the canvas LivePreview leaf's View::isCalloutBlock() fires
// for the resulting block, i.e. callouts are NOT blocked on any live-load
// promotion gap.
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
        // Vault::openDocument's exact FirstOpen sequence — loadFromMarkdown,
        // not setMarkdown()/resetContent().
        doc.markoff()->loadFromMarkdown(
            QByteArrayLiteral("> [!note]\n> Some callout body text.\n"));

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
        doc.markoff()->loadFromMarkdown(
            QByteArrayLiteral("> Just a plain quote, no callout marker.\n"));

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
