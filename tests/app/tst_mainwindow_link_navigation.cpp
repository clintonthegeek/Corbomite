// SPDX-License-Identifier: GPL-3.0-or-later
//
// Task 0.2 — MainWindow wikilink navigation dispatch (Finding 1).
//
// Verifies the navigation lambda wired in MainWindow::propagateServicesToView
// (88ad1b46): when NoteEditorWidget::linkActivated(rawTarget) fires,
//   (a) if the target exists in the vault, onNoteActivated opens it, and
//   (b) if the target does not exist, onNoteActivated eagerly creates the
//       .md file on disk (Obsidian create-on-click parity) and opens it.
//
// Drive through the real seam: open a vault, open a source note to get a
// live NoteEditorWidget, then emit linkActivated(target) so the MainWindow
// lambda runs. Assert via the workspace active-leaf document path.
//
// Runs under QT_QPA_PLATFORM=offscreen (no display server required).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "editor/NoteEditorWidget.h"
#include "editor/MarkdownView.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void createFile(const QString &path, const QString &content = QStringLiteral("# Test\n"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content.toUtf8());
}

/// Return the vault-relative path of the note currently open in the active
/// leaf, or an empty string if no note is open.
static QString activeDocumentPath(MainWindow *mw)
{
    auto *ws = mw->findChild<Workspace *>();
    if (!ws || !ws->activeLeaf()) return {};
    auto *mv = qobject_cast<MarkdownView *>(ws->activeLeaf()->view());
    if (!mv) return {};
    auto *editor = mv->editorWidget();
    if (!editor || !editor->noteDocument()) return {};
    return editor->noteDocument()->relativePath();
}

/// Return the active NoteEditorWidget, or nullptr.
static NoteEditorWidget *activeEditor(MainWindow *mw)
{
    auto *ws = mw->findChild<Workspace *>();
    if (!ws || !ws->activeLeaf()) return nullptr;
    auto *mv = qobject_cast<MarkdownView *>(ws->activeLeaf()->view());
    if (!mv) return nullptr;
    return mv->editorWidget();
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TstMainWindowLinkNavigation : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        // KDE framework setup — required before constructing MainWindow.
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("link-navigation test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------------
    // Test 1: wikilink to an EXISTING note navigates to that note.
    // -----------------------------------------------------------------------
    void linkToExistingNote_navigatesToTarget()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Source note has a wikilink to Target.
        createFile(tmp.path() + QStringLiteral("/source.md"),
                   QStringLiteral("# Source\n[[Target]]\n"));
        // Target note exists in the vault.
        createFile(tmp.path() + QStringLiteral("/Target.md"),
                   QStringLiteral("# Target\n"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        // Open the source note so the editor widget exists and the
        // linkActivated signal has been connected to the navigation lambda.
        mw.onNoteActivated(QStringLiteral("source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY2(editor, "NoteEditorWidget must be active after onNoteActivated(source.md)");

        // Emit linkActivated with the raw wikilink target (no .md suffix,
        // as the DefaultLinkService/onLinkActivated path produces).
        Q_EMIT editor->linkActivated(QStringLiteral("Target"), false);
        QTest::qWait(300);

        const QString docPath = activeDocumentPath(&mw);
        QCOMPARE(docPath, QStringLiteral("Target.md"));
    }

    // -----------------------------------------------------------------------
    // Test 2: wikilink to a NON-EXISTENT note creates the file and opens it
    //         (Obsidian create-on-click parity).
    // -----------------------------------------------------------------------
    void linkToNonExistentNote_createsAndOpensNote()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Only source note in the vault; "NewNote" does not exist yet.
        createFile(tmp.path() + QStringLiteral("/source.md"),
                   QStringLiteral("# Source\n[[NewNote]]\n"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY2(editor, "NoteEditorWidget must be active after onNoteActivated(source.md)");

        // Verify the target does not exist before we click the link.
        QVERIFY2(!QFileInfo::exists(tmp.path() + QStringLiteral("/NewNote.md")),
                 "NewNote.md must not exist before link activation");

        // Fire the link — the fallback path in the navigation lambda appends
        // .md and calls onNoteActivated, which calls FileManager::createMarkdownNote
        // because the file does not exist in the vault.
        Q_EMIT editor->linkActivated(QStringLiteral("NewNote"), false);
        QTest::qWait(300);

        // The file must now exist on disk (eager create-on-click).
        QVERIFY2(QFileInfo::exists(tmp.path() + QStringLiteral("/NewNote.md")),
                 "Fallback path must create NewNote.md on disk");

        // And it must be open in the active editor.
        const QString docPath = activeDocumentPath(&mw);
        QCOMPARE(docPath, QStringLiteral("NewNote.md"));
    }

    // -----------------------------------------------------------------------
    // Test 3: a plain click (openInNewTab == false) navigates the active
    //         leaf IN PLACE — leaf count must not grow. Regression test for
    //         the bug where every link click (plain or middle) always went
    //         through onNoteActivated/openFileInWorkspace, which only ever
    //         creates-or-switches-to-a-leaf and never navigates in place, so
    //         the tab-frame's back/forward buttons never had any history to
    //         work with.
    // -----------------------------------------------------------------------
    void plainClick_navigatesActiveLeafInPlace_noNewLeaf()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + QStringLiteral("/source.md"),
                   QStringLiteral("# Source\n[[Target]]\n"));
        createFile(tmp.path() + QStringLiteral("/Target.md"),
                   QStringLiteral("# Target\n"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY2(editor, "NoteEditorWidget must be active after onNoteActivated(source.md)");

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        const int leafCountBefore = ws->allLeaves().size();

        Q_EMIT editor->linkActivated(QStringLiteral("Target"), /*openInNewTab=*/false);
        QTest::qWait(300);

        QCOMPARE(activeDocumentPath(&mw), QStringLiteral("Target.md"));
        QCOMPARE(ws->allLeaves().size(), leafCountBefore);
        QVERIFY2(ws->activeLeaf() && ws->activeLeaf()->history().canGoBack(),
                  "navigate() must push the pre-navigation state so the "
                  "back button has something to go back to");
    }

    // -----------------------------------------------------------------------
    // Test 4: an explicit middle-click (openInNewTab == true) still opens a
    //         NEW leaf, same as before this fix — the split must not have
    //         regressed the middle-click "open in new tab" path.
    // -----------------------------------------------------------------------
    void middleClick_opensNewLeaf()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + QStringLiteral("/source.md"),
                   QStringLiteral("# Source\n[[Target]]\n"));
        createFile(tmp.path() + QStringLiteral("/Target.md"),
                   QStringLiteral("# Target\n"));

        CorbomiteApp app;
        MainWindow mw(&app);

        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY2(editor, "NoteEditorWidget must be active after onNoteActivated(source.md)");

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        const int leafCountBefore = ws->allLeaves().size();

        Q_EMIT editor->linkActivated(QStringLiteral("Target"), /*openInNewTab=*/true);
        QTest::qWait(300);

        QCOMPARE(activeDocumentPath(&mw), QStringLiteral("Target.md"));
        QCOMPARE(ws->allLeaves().size(), leafCountBefore + 1);
    }
};

QTEST_MAIN(TstMainWindowLinkNavigation)
#include "tst_mainwindow_link_navigation.moc"
