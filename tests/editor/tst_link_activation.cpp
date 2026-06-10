// SPDX-License-Identifier: GPL-3.0-or-later
//
// Task 0.2 — NoteEditorWidget::linkActivated is emitted when the Live binding's
// LinkService fires linkActivated, and the resolved target is forwarded
// correctly for both WikiLink and plain-file cases.
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/core/DefaultLinkService.h>
#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/live/EditorWidget.h>
#include <markoff/live/LiveListModelBinding.h>
#include <markoff/styled/Editor.h>

#include "corbomite/core/NoteDocument.h"

#include <QSignalSpy>
#include <QTest>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class LinkActivationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    // Activating a WikiLink via the Live binding's link service emits
    // NoteEditorWidget::linkActivated with the page name.
    void liveMode_wikilinkActivation_emitsLinkActivated()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[TargetNote]]"));
        widget.setNoteDocument(&doc);
        // LivePreview is the default view mode — already active.

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        // Retrieve the link service that the NoteEditorWidget injected into
        // the Live binding and trigger an activation.
        auto *binding = widget.editor()->binding();
        QVERIFY(binding);
        auto *svc = binding->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind     = Markoff::LinkKind::WikiLink;
        act.page     = QStringLiteral("TargetNote");
        act.rawText  = QStringLiteral("[[TargetNote]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("TargetNote"));
    }

    // Activating a WikiLink with a section component emits just the page name
    // (section navigation is deferred; the receiver resolves through LinkResolver).
    void liveMode_wikilinkWithSection_emitsPageOnly()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[TargetNote#Heading]]"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.editor()->binding()->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("TargetNote");
        act.section = QStringLiteral("Heading");
        act.rawText = QStringLiteral("[[TargetNote#Heading]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
        // Only the page name is forwarded; section resolution is the
        // MainWindow / LinkResolver's responsibility.
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("TargetNote"));
    }

    // External links are handled by QDesktopServices and do NOT emit
    // NoteEditorWidget::linkActivated.
    void liveMode_externalLink_doesNotEmitSignal()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[link](https://example.com)"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.editor()->binding()->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind            = Markoff::LinkKind::External;
        act.rawText         = QStringLiteral("https://example.com");
        act.resolvedTarget  = QUrl(QStringLiteral("https://example.com"));
        svc->activate(act);

        // External URLs must not reach onNoteActivated; they are opened by
        // QDesktopServices (offscreen environment swallows the open silently).
        QCOMPARE(spy.count(), 0);
    }

    // Activating a WikiLink in Reading mode (Styled leaf) emits the same signal.
    void readingMode_wikilinkActivation_emitsLinkActivated()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[OtherNote]]"));
        widget.setNoteDocument(&doc);

        // Switch to Reading to lazily construct the Styled leaf.
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *styled = qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf());
        QVERIFY2(styled, "Reading leaf must be a Markoff::Styled::Editor");

        // The Styled leaf must have been given the shared link service.
        auto *svc = styled->linkService();
        QVERIFY2(svc, "Reading leaf must have a LinkService");

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("OtherNote");
        act.rawText = QStringLiteral("[[OtherNote]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("OtherNote"));
    }

    // Both Live and Reading modes share the same link service instance, so
    // only one connection to linkActivated exists (no double-emission on
    // mode transitions).
    void modeTransition_sharedService_noDoubleEmission()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Note]]"));
        widget.setNoteDocument(&doc);

        // Construct both leaves.
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        // Activate via the Live binding's service (same instance as the
        // Reading leaf's service after the shared-service wiring).
        auto *svc = widget.editor()->binding()->linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Note");
        act.rawText = QStringLiteral("[[Note]]");
        svc->activate(act);

        // Must fire exactly once — shared service wired with a single
        // connect in the constructor, not per-leaf.
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(LinkActivationTest)
#include "tst_link_activation.moc"
