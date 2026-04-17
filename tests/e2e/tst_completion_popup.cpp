// SPDX-License-Identifier: GPL-3.0-or-later
//
// E2E test for the completion popup contract:
//   - Typing "[[" shows the popup.
//   - Continued typing filters the popup AND inserts characters into
//     the editor (the bug we're fixing — the old popup ate input).
//   - Down/Up navigates the popup without moving the editor cursor.
//   - Enter accepts the highlighted item, dismisses, inserts text + "]]".
//   - Esc dismisses without inserting.
//
// Spec: docs/superpowers/specs/2026-04-15-completion-popup-rewrite.md.

#include <QTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <KAboutData>
#include <KLocalizedString>

#include "app/MainWindow.h"
#include "app/CorbomiteApp.h"
#include "editor/NoteEditorWidget.h"
#include "editor/CompletionPopup.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/core/NoteDocument.h"
#include "markoff/Editor.h"

using namespace Corbomite;

class TestCompletionPopup : public QObject {
    Q_OBJECT

private:
    CorbomiteApp *m_app = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QTemporaryDir m_vault;

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content.toUtf8());
    }

    void settle(int ms = 200) { QTest::qWait(ms); }

    NoteEditorWidget *activeNoteEditor()
    {
        return m_mainWindow->findChild<NoteEditorWidget *>();
    }

    Markoff::Editor *activeMarkoffEditor()
    {
        auto *nw = activeNoteEditor();
        return nw ? nw->editor() : nullptr;
    }

    CompletionPopup *visiblePopup()
    {
        auto *nw = activeNoteEditor();
        if (!nw) return nullptr;
        return nw->findChild<CompletionPopup *>(QString(), Qt::FindChildrenRecursively);
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("completion popup repro"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);

        QVERIFY(m_vault.isValid());
        // A few notes to populate the wikilink completion list.
        createFile(m_vault.path() + "/Apple.md", QStringLiteral("# Apple\n"));
        createFile(m_vault.path() + "/Apricot.md", QStringLiteral("# Apricot\n"));
        createFile(m_vault.path() + "/Banana.md", QStringLiteral("# Banana\n"));
        createFile(m_vault.path() + "/Target.md", QStringLiteral("# Target\n"));

        m_app = new CorbomiteApp(this);
        m_mainWindow = new MainWindow(m_app);
        m_mainWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mainWindow));
        settle();

        QVERIFY(m_app->openVault(m_vault.path()));
        settle(500);

        // Open a note so an editor exists.
        m_mainWindow->onNoteActivated(QStringLiteral("Target.md"));
        settle(300);
        QVERIFY(activeMarkoffEditor());
    }

    void cleanupTestCase()
    {
        delete m_mainWindow;
        m_mainWindow = nullptr;
        delete m_app;
        m_app = nullptr;
    }

    void testTypingBracketBracketShowsPopup()
    {
        auto *editor = activeMarkoffEditor();
        auto *nw = activeNoteEditor();
        QVERIFY(editor);
        QVERIFY(nw);
        // Defensively ensure the vault is set on this editor — the
        // active-editor signal may not have fired yet under offscreen.
        nw->setVault(m_mainWindow->vaultObj());

        m_mainWindow->activateWindow();
        editor->setFocus();
        // Click into the editor viewport to seat focus on a text item.
        QTest::mouseClick(editor->viewport(), Qt::LeftButton,
                          Qt::NoModifier, QPoint(40, 40));
        settle(150);

        QTest::keyClick(editor, Qt::Key_BracketLeft);
        QTest::qWait(40);
        QTest::keyClick(editor, Qt::Key_BracketLeft);
        settle(200);

        auto *popup = visiblePopup();
        if (!popup) {
            qWarning() << "Editor source after [[:" << editor->toPlainText();
            qWarning() << "Editor focus widget:" << QApplication::focusWidget();
        }
        QVERIFY2(popup, "Popup did not appear after typing [[");
        QVERIFY(popup->isVisible());
        QVERIFY(popup->visibleRowCount() > 0);
    }

    // The big one: continued typing must keep going into the editor
    // (filling the trigger filter at the same time).
    void testTypingFiltersAndStillReachesEditor()
    {
        auto *editor = activeMarkoffEditor();
        auto *popup = visiblePopup();
        QVERIFY(popup);

        const int rowsBefore = popup->visibleRowCount();
        const QString srcBefore = editor->toPlainText();

        // Type 'A' — should filter list to Apple/Apricot AND insert 'A'.
        QTest::keyClick(editor, Qt::Key_A);
        settle(100);

        const QString srcAfter = editor->toPlainText();
        QVERIFY2(srcAfter != srcBefore,
                 "Editor did not receive the 'A' keystroke — popup is stealing input!");
        QVERIFY(srcAfter.contains(QStringLiteral("[[a")));

        const int rowsAfter = popup->visibleRowCount();
        QVERIFY2(rowsAfter <= rowsBefore,
                 "Filter did not narrow the list");
        QVERIFY(rowsAfter > 0);
    }

    void testArrowKeysNavigatePopupNotEditor()
    {
        auto *editor = activeMarkoffEditor();
        auto *popup = visiblePopup();
        QVERIFY(popup);

        const int lineBefore = editor->cursorLine();
        const QString first = popup->selectedText();

        QTest::keyClick(editor, Qt::Key_Down);
        settle(60);
        const QString afterDown = popup->selectedText();
        QVERIFY2(afterDown != first, "Down arrow did not move popup selection");
        QCOMPARE(editor->cursorLine(), lineBefore);
    }

    void testEscapeDismissesWithoutInserting()
    {
        auto *editor = activeMarkoffEditor();
        auto *popup = visiblePopup();
        QVERIFY(popup);
        const QString srcBefore = editor->toPlainText();

        QTest::keyClick(editor, Qt::Key_Escape);
        settle(120);

        QVERIFY2(visiblePopup() == nullptr || !visiblePopup()->isVisible(),
                 "Esc did not dismiss popup");
        QCOMPARE(editor->toPlainText(), srcBefore);
    }

    // After Esc dismissed the popup, fresh "[[" should re-open it,
    // then Enter should accept the top item and close the popup.
    void testEnterAcceptsAndDismisses()
    {
        auto *editor = activeMarkoffEditor();

        // Move to a fresh line.
        QTest::keyClick(editor, Qt::Key_End);
        QTest::keyClick(editor, Qt::Key_Return);
        settle(50);

        QTest::keyClick(editor, Qt::Key_BracketLeft);
        QTest::qWait(30);
        QTest::keyClick(editor, Qt::Key_BracketLeft);
        settle(120);

        auto *popup = visiblePopup();
        QVERIFY(popup);
        QVERIFY(popup->visibleRowCount() > 0);

        const QString chosen = popup->selectedText();
        QTest::keyClick(editor, Qt::Key_Return);
        settle(120);

        QVERIFY2(visiblePopup() == nullptr || !visiblePopup()->isVisible(),
                 "Enter did not dismiss popup");
        const QString src = editor->toPlainText();
        QVERIFY2(src.contains(QStringLiteral("[[") + chosen + QStringLiteral("]]")),
                 qPrintable(QStringLiteral("Selected text not inserted as wikilink: ") + src));
    }
};

QTEST_MAIN(TestCompletionPopup)
#include "tst_completion_popup.moc"
