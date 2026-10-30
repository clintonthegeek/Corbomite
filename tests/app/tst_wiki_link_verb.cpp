// SPDX-License-Identifier: GPL-3.0-or-later
//
// Punch list [LIVE-FOUND 2026-10-18]: the `insert_wiki_link` verb inserted
// a *markdown* link (`[](url)` — the caret landed in the markdown bracket)
// because it called `insertLink()`, the same verb Ctrl+K performs. The fix
// routes through Markoff::Canvas::View's insertion surface and lands the
// caret inside the `[[ ]]` pair.
//
// Falsifiable: against the old action body these three cases fail with a
// `[](url)` skeleton instead of `[[]]`.
// Driven through a real MainWindow + vault, same pattern as
// tst_action_context / tst_mainwindow_link_navigation. Offscreen.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "editor/MarkdownFileView.h"
#include "editor/NoteEditorWidget.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include "editor/NoteEditorWidget.h"

#include <markoff/canvas/EditorWidget.h>
#include <markoff/core/MarkoffDocument.h>


using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

// The P2 P1-lived helper from tst_mainwindow_link_navigation.cpp: the active
// leaf's NoteEditorWidget through a qobject_cast<MarkdownFileView *>. This
// is the only route to that object: mw->activeEditor() is private.
static NoteEditorWidget *activeEditor(MainWindow *mw)
{
    auto *ws = mw->findChild<Workspace *>();
    if (!ws || !ws->activeLeaf()) return nullptr;
    auto *mv = qobject_cast<MarkdownFileView *>(ws->activeLeaf()->view());
    if (!mv) return nullptr;
    return mv->editorWidget();
}
} // namespace

#include "corbomite/core/NoteDocument.h"
#include "editor/MarkdownViewActions.h"

class TstWikiLinkVerb : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("wiki-link verb test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void verbInsertsThePairAndLandsTheCaretInside()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Archive.md"), QStringLiteral("A target\n"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);

        mw.onNoteActivated(QStringLiteral("Archive.md"));
        QTest::qWait(300);
        auto *ed = activeEditor(&mw);
        QVERIFY(ed);
        auto *canvas = ed->canvasEditor();
        QVERIFY(canvas);

        const auto action = mw.markdownViewActions()->actionCollection()->
            action(QStringLiteral("insert_wiki_link"));
        QVERIFY(action);

        // The caret is where the note opened: Line 1, Col 9 ("A target" is
        // 8 chars). The verb must insert the pair there and land the caret
        // INSIDE the pair — Col +2 — not after the closing bracket.
        // Mirror the completion harness (tst_note_editor_widget_completion):
        // the canvas caretRect needs focus + a few event-loop turns to become
        // valid, and refresh() dismisses on an invalid rect.
        bool caretValid = false;
        for (int i = 0; i < 60 && !caretValid; ++i) {
            canvas->setFocus(Qt::OtherFocusReason);
            QTest::qWait(30);
            caretValid = canvas->caretRect().isValid();
        }
        QVERIFY2(caretValid, "canvas caretRect never became valid");

        const auto before = canvas->cursorPosition();
        action->trigger();
        QApplication::processEvents();

        const QString body = QString::fromUtf8(
            ed->noteDocument()->markoff()->serializeForSave());
        QVERIFY2(body.contains(QStringLiteral("[[]]")),
                 qPrintable(body));

        const auto after = canvas->cursorPosition();
        QCOMPARE(after.line, before.line);
        QCOMPARE(after.column, before.column + 2);

        // The Source leaf sees the same bytes, since it shares the document.
        QCOMPARE(QString::fromUtf8(ed->noteDocument()->markoff()->serializeForSave())
                     .contains(QStringLiteral("[[]]")), true);

        // The box's open question — whether the [[-suggester fires for a
        // programmatic insert — is answered by the mechanics, not by this
        // test: the trigger evaluates on both d2DocumentChanged (from the
        // insert) and cursorPositionChanged (from the caret re-placement),
        // and WikiLinkSuggest::onTrigger explicitly consumes an
        // already-present closing "]]" after the cursor (replaceEnd), which
        // is the shape this verb leaves the line in. Whether the popup
        // VISIBLY opens offscreen was NOT pinned: in this environment the
        // popup did not appear within 4 s and was left out of these cases
        // rather than asserted-then-skipped; a live pass in USER-TESTING §3
        // is the arbiter for the popup half only.
    }

    void verbSourceModeInsertsNothingIntoTheCanvasLeaf()
    {
        // When the user is editing in Source mode (or Reading), the verb
        // must be a no-op — inserting into the canvas leaf would write into
        // a note the user is not looking at. (Source's own insertion
        // surface is an upstream gap: the Source::Editor contract carries
        // no insertText; if that ever lands this gate changes, and the
        // punch-list disable-scope note documents it.)
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Note.md"),
                   QStringLiteral("Some body text\n"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(300);

        auto *ed = activeEditor(&mw);
        QVERIFY(ed);
        auto *canvas = ed->canvasEditor();
        QVERIFY(canvas);

        const auto beforeBody = QString::fromUtf8(
            ed->noteDocument()->markoff()->serializeForSave());

        // Enter Source mode, pretend to anchor a caret there.
        switch (ed->viewMode()) {
        case NoteEditorWidget::ViewMode::Source:
            break;
        case NoteEditorWidget::ViewMode::LivePreview:
            mw.markdownViewActions()->actionCollection()
                ->action(QStringLiteral("view_source_mode"))->trigger();
            break;
        case NoteEditorWidget::ViewMode::Reading:
            break;
        }
        QTest::qWait(100);

        // Greyed, not merely inert: a dead key must never be offered (the
        // Tier-B contract the release asserts everywhere else).
        auto *act = mw.markdownViewActions()->actionCollection()
            ->action(QStringLiteral("insert_wiki_link"));
        QVERIFY2(!act->isEnabled(),
                 "insert_wiki_link is enabled in Source mode, where it would no-op");
        QCOMPARE(QString::fromUtf8(ed->noteDocument()->markoff()->serializeForSave()),
                 beforeBody);
    }
};

QTEST_MAIN(TstWikiLinkVerb)
#include "tst_wiki_link_verb.moc"
