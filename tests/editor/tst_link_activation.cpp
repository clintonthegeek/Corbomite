// SPDX-License-Identifier: GPL-3.0-or-later
//
// NoteEditorWidget::linkActivated is emitted when the shared LinkService
// fires linkActivated, and the resolved target is forwarded correctly for
// both WikiLink and plain-file cases (LivePreview canvas + Reading).
//
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"

#include <markoff/core/LinkActivation.h>
#include <markoff/core/LinkKind.h>
#include <markoff/core/LinkService.h>
#include <markoff/core/MarkdownView.h>
#include <markoff/styled/Editor.h>

#include "corbomite/core/NoteDocument.h"

#include <QSignalSpy>
#include <QTest>
#include <QUrl>

using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

class LinkActivationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:

    void livePreview_wikilinkActivation_emitsLinkActivated()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[TargetNote]]"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind     = Markoff::LinkKind::WikiLink;
        act.page     = QStringLiteral("TargetNote");
        act.rawText  = QStringLiteral("[[TargetNote]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("TargetNote"));
    }

    void livePreview_wikilinkWithSection_emitsPageOnly()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[TargetNote#Heading]]"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("TargetNote");
        act.section = QStringLiteral("Heading");
        act.rawText = QStringLiteral("[[TargetNote#Heading]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("TargetNote"));
    }

    void livePreview_externalLink_doesNotEmitSignal()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[link](https://example.com)"));
        widget.setNoteDocument(&doc);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind            = Markoff::LinkKind::External;
        act.rawText         = QStringLiteral("https://example.com");
        act.resolvedTarget  = QUrl(QStringLiteral("https://example.com"));
        svc->activate(act);

        QCOMPARE(spy.count(), 0);
    }

    void readingMode_wikilinkActivation_emitsLinkActivated()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[OtherNote]]"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *styled = qobject_cast<Markoff::Styled::Editor *>(widget.activeLeaf());
        QVERIFY2(styled, "Reading leaf must be a Markoff::Styled::Editor");

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

    void modeTransition_sharedService_noDoubleEmission()
    {
        NoteEditorWidget widget;
        NoteDocument doc(QStringLiteral("/tmp/vault"),
                         QStringLiteral("source.md"));
        doc.setMarkdown(QStringLiteral("[[Note]]"));
        widget.setNoteDocument(&doc);

        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        widget.setViewMode(NoteEditorWidget::ViewMode::LivePreview);

        QSignalSpy spy(&widget, &NoteEditorWidget::linkActivated);

        auto *svc = widget.linkService();
        QVERIFY(svc);

        Markoff::LinkActivation act;
        act.kind    = Markoff::LinkKind::WikiLink;
        act.page    = QStringLiteral("Note");
        act.rawText = QStringLiteral("[[Note]]");
        svc->activate(act);

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(LinkActivationTest)
#include "tst_link_activation.moc"
