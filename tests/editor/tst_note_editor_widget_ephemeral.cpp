// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster E Phase 2 — NoteEditorWidget save/restore ephemeral state
// round-trips through all three ViewMode targets (Source, LivePreview,
// Reading). Source rides on `SourceEditor::setScrollPosition`, LivePreview on
// `Markoff::Editor::setScrollPositionVisualLine`, Reading on the
// `Markoff::Reading::ReadingView` stub (Phase 3 fills the stub in).
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "SourceEditor.h"
#include "corbomite/storage/EphemeralState.h"

#include <markoff/Editor.h>

#include <QObject>
#include <QStringList>
#include <QTest>

using Corbomite::EphemeralState;
using Corbomite::NoteEditorWidget;
using Corbomite::SourceEditor;

namespace {

QString makeParagraphs(int count)
{
    QStringList blocks;
    blocks.reserve(count);
    for (int i = 0; i < count; ++i) {
        blocks.append(QStringLiteral("Paragraph %1 line A.").arg(i));
    }
    return blocks.join(QStringLiteral("\n\n"));
}

} // namespace

class NoteEditorWidgetEphemeralTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void sourceModeRoundTrip()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        widget.setViewMode(NoteEditorWidget::ViewMode::Source);

        // SourceEditor is the authoritative scroll source for Source mode;
        // drive it directly, then capture via the widget API.
        auto *source = widget.findChild<SourceEditor *>();
        QVERIFY(source);
        source->setPlainText(makeParagraphs(60));
        source->setScrollPosition(5.5f);
        QTest::qWait(20);

        const auto saved = widget.saveEphemeralState();
        // The plan's ±0.5 contract plus Phase-2 scrollbar-line-granularity
        // rounding (see tst_source_editor) gives us a ±0.55 envelope.
        QVERIFY2(std::abs(saved.scroll - 5.5f) <= 0.55f,
                 qPrintable(QStringLiteral("Source scroll save drift: %1")
                                .arg(saved.scroll)));

        source->setScrollPosition(0.0f);
        QTest::qWait(20);
        widget.restoreEphemeralState(saved);
        QTest::qWait(20);
        const float got = source->scrollPosition();
        QVERIFY2(std::abs(got - 5.5f) <= 0.55f,
                 qPrintable(QStringLiteral("Source scroll restore drift: %1").arg(got)));
    }

    void livePreviewModeRoundTrip()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);
        widget.editor()->setPlainText(makeParagraphs(40));
        QTest::qWait(30);

        widget.editor()->setScrollPositionVisualLine(8.0f);
        QTest::qWait(20);

        const auto saved = widget.saveEphemeralState();
        QVERIFY2(std::abs(saved.scroll - 8.0f) <= 0.5f,
                 qPrintable(QStringLiteral("LivePreview save drift: %1")
                                .arg(saved.scroll)));

        widget.editor()->setScrollPositionVisualLine(0.0f);
        QTest::qWait(20);
        widget.restoreEphemeralState(saved);
        QTest::qWait(20);
        const float got = widget.editor()->scrollPositionVisualLine();
        QVERIFY2(std::abs(got - 8.0f) <= 0.5f,
                 qPrintable(QStringLiteral("LivePreview restore drift: %1").arg(got)));
    }

    void readingModeStubRoundTrip()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        EphemeralState s;
        s.modeRaw = QStringLiteral("preview");
        s.sourceFlag = false;
        s.scroll = 0.0f;
        widget.restoreEphemeralState(s);

        const auto saved = widget.saveEphemeralState();
        // Phase-2 Reading stub returns 0.0 unconditionally — Phase 3 will
        // make this an actual visual-line value and upgrade the test to
        // enforce ±0.5 tolerance.
        QCOMPARE(saved.scroll, 0.0f);
    }
};

QTEST_MAIN(NoteEditorWidgetEphemeralTest)
#include "tst_note_editor_widget_ephemeral.moc"
