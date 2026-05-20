// SPDX-License-Identifier: GPL-3.0-or-later
//
// C4 Task 14 — NoteEditorWidget mermaid renderer injection test.
//
// Verifies that after setMermaidRenderer() is called:
//   1. The Live leaf (Markoff::Editor) receives the renderer pointer.
//   2. A lazily-constructed Reading leaf receives the same pointer
//      (injected during ensureWidgetConstructed when mode switches to Reading).
//
// This is a pointer-equality test — behavioural rendering tests live in the
// markoff-live and markoff-reading unit suites; here we verify the wiring
// layer at the NoteEditorWidget seam.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/Editor.h>
#include <markoff/MermaidRenderer.h>
#include <markoff/reading/ReadingView.h>
#include <markoff/core/MarkoffDocument.h>

#include "corbomite/core/NoteDocument.h"

#include <QObject>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

namespace {

/// Minimal stub that records the last rendered source so we can verify it
/// was actually called. renderSvg returns a trivially-valid empty SVG so
/// callers don't crash on a null result.
class StubMermaidRenderer : public Markoff::MermaidRenderer
{
public:
    mutable QString lastSource;
    mutable int callCount = 0;

    QByteArray renderSvg(const QString &source) const override
    {
        ++callCount;
        lastSource = source;
        // Return a minimal valid SVG so downstream consumers don't reject it.
        return QByteArray(R"(<svg xmlns="http://www.w3.org/2000/svg"/>)");
    }
};

} // namespace

class MermaidInjectionTest : public QObject {
    Q_OBJECT

private Q_SLOTS:

    // --- Test 1: Live leaf receives renderer immediately ---
    void liveLeafReceivesRenderer()
    {
        NoteEditorWidget widget;

        StubMermaidRenderer stub;
        widget.setMermaidRenderer(&stub);

        // The Live leaf (Markoff::Editor) is always eagerly constructed —
        // confirm its mermaidRenderer() accessor returns our stub.
        auto *ed = widget.editor();
        QVERIFY(ed);
        QCOMPARE(ed->mermaidRenderer(), static_cast<Markoff::MermaidRenderer *>(&stub));
    }

    // --- Test 2: Reading leaf receives renderer when lazily constructed ---
    void readingLeafReceivesRendererOnLazyConstruct()
    {
        NoteEditorWidget widget;
        widget.resize(600, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        StubMermaidRenderer stub;
        widget.setMermaidRenderer(&stub);

        // Reading leaf is not yet constructed — confirm it doesn't exist.
        QVERIFY(widget.readingView() == nullptr);

        // Switch to Reading mode — this triggers ensureWidgetConstructed.
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("Hello, world."));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        // Now the Reading leaf should exist and carry our renderer.
        auto *rv = widget.readingView();
        QVERIFY(rv != nullptr);
        QCOMPARE(rv->mermaidRenderer(),
                 static_cast<Markoff::MermaidRenderer *>(&stub));
    }

    // --- Test 3: setMermaidRenderer after Reading leaf already exists ---
    void readingLeafUpdatedAfterConstruct()
    {
        NoteEditorWidget widget;
        widget.resize(600, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        // Trigger Reading leaf construction first (no renderer yet).
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("Hello, world."));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QVERIFY(widget.readingView() != nullptr);

        // Now inject the renderer — must propagate to the already-existing leaf.
        StubMermaidRenderer stub;
        widget.setMermaidRenderer(&stub);

        QCOMPARE(widget.readingView()->mermaidRenderer(),
                 static_cast<Markoff::MermaidRenderer *>(&stub));
        QCOMPARE(widget.editor()->mermaidRenderer(),
                 static_cast<Markoff::MermaidRenderer *>(&stub));
    }

    // --- Test 4: both leaves return the same pointer (same instance) ---
    void bothLeavesSamePointer()
    {
        NoteEditorWidget widget;
        widget.resize(600, 400);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        StubMermaidRenderer stub;
        widget.setMermaidRenderer(&stub);

        // Force Reading leaf construction.
        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("Hello."));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *rv = widget.readingView();
        auto *ed = widget.editor();
        QVERIFY(rv);
        QVERIFY(ed);
        QCOMPARE(rv->mermaidRenderer(), ed->mermaidRenderer());
    }
};

QTEST_MAIN(MermaidInjectionTest)
#include "tst_mermaid_injection.moc"
