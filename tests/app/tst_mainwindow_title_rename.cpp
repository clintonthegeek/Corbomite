// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster K — canvas leaf's inline document-title band (rename-via-header
// affordance) surfaced in Corbomite. Verifies:
//   (a) the band is seeded from the note's filename on attach,
//   (b) an edit through the band (debounced) commits a real vault rename
//       via FileManager, and
//   (c) the open NoteDocument's path (and the band itself, via
//       pathChanged) tracks the rename.
//
// Runs under QT_QPA_PLATFORM=offscreen. Requires CorbomiteSettings::
// canvasLivePreview() set BEFORE MainWindow construction (leaf backend is
// decided once, at construction — see NoteEditorWidget's ctor).

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

#include <markoff/canvas/EditorWidget.h>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "editor/NoteEditorWidget.h"
#include "editor/MarkdownView.h"
#include "corbomitesettings.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

static void createFile(const QString &path, const QString &content = QStringLiteral("# Test\n"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(content.toUtf8());
}

static NoteEditorWidget *activeEditor(MainWindow *mw)
{
    auto *ws = mw->findChild<Workspace *>();
    if (!ws || !ws->activeLeaf()) return nullptr;
    auto *mv = qobject_cast<MarkdownView *>(ws->activeLeaf()->view());
    if (!mv) return nullptr;
    return mv->editorWidget();
}

class TstMainWindowTitleRename : public QObject {
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("title-rename test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void init()
    {
        // Leaf backend is decided once at NoteEditorWidget construction —
        // must be set before each MainWindow is built.
        CorbomiteSettings::self()->setCanvasLivePreview(true);
        CorbomiteSettings::self()->save();
    }

    void cleanup()
    {
        CorbomiteSettings::self()->setCanvasLivePreview(false);
        CorbomiteSettings::self()->save();
    }

    void titleBand_seededFromFilename_onAttach()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Source.md"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY2(editor, "NoteEditorWidget must be active after onNoteActivated");
        auto *canvas = editor->canvasEditor();
        QVERIFY2(canvas, "canvasEditor() must be non-null with canvasLivePreview=true");

        QCOMPARE(canvas->inlineTitle(), QStringLiteral("Source"));
        QVERIFY(canvas->inlineTitleVisible());
    }

    void titleBand_edit_debounces_thenRenamesFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Source.md"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY(editor);
        auto *canvas = editor->canvasEditor();
        QVERIFY(canvas);

        // Simulate a band edit exactly the way handleTitleKeyPress does —
        // fire titleEdited directly, same idiom tst_link_activation.cpp
        // uses for LinkService::activate.
        Q_EMIT canvas->titleEdited(QStringLiteral("Renamed"));

        // Immediately after: must NOT have renamed yet (debounce).
        QVERIFY2(QFileInfo::exists(tmp.path() + QStringLiteral("/Source.md")),
                 "a single titleEdited must not rename synchronously");

        // Past the debounce window: must have renamed.
        QTest::qWait(900);

        QVERIFY2(QFileInfo::exists(tmp.path() + QStringLiteral("/Renamed.md")),
                 "debounced titleEdited must commit a vault rename");
        QVERIFY2(!QFileInfo::exists(tmp.path() + QStringLiteral("/Source.md")),
                 "old filename must no longer exist after rename");
        QCOMPARE(editor->noteDocument()->relativePath(), QStringLiteral("Renamed.md"));

        // pathChanged must have re-synced the band to the new name.
        QCOMPARE(canvas->inlineTitle(), QStringLiteral("Renamed"));
    }

    void titleBand_slashInText_doesNotMoveFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Source.md"));
        QDir(tmp.path()).mkpath(QStringLiteral("sub"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY(editor);
        auto *canvas = editor->canvasEditor();
        QVERIFY(canvas);

        Q_EMIT canvas->titleEdited(QStringLiteral("sub/Escaped"));
        QTest::qWait(900);

        QVERIFY2(QFileInfo::exists(tmp.path() + QStringLiteral("/Source.md")),
                 "a title containing '/' must be rejected, not turned into a folder move");
        QVERIFY(!QFileInfo::exists(tmp.path() + QStringLiteral("/sub/Escaped.md")));
    }

    // -----------------------------------------------------------------------
    // Cluster K punch-list P5 — readable-line-width setting. View already
    // implemented ContentWidthPolicy fully; this only verifies Corbomite's
    // wiring (construction-time seed + live settings-apply update) actually
    // reaches it.
    // -----------------------------------------------------------------------
    void readableLineWidth_appliedAtConstruction_andLiveOnSettingsChange()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Source.md"));

        CorbomiteSettings::self()->setReadableLineWidth(true);
        CorbomiteSettings::self()->save();

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        mw.onNoteActivated(QStringLiteral("Source.md"));
        QTest::qWait(300);

        auto *editor = activeEditor(&mw);
        QVERIFY(editor);
        auto *canvas = editor->canvasEditor();
        QVERIFY(canvas);

        QCOMPARE(canvas->contentWidthPolicy().kind,
                 Markoff::Canvas::ContentWidthPolicy::FixedColumn);
        QCOMPARE(canvas->contentWidthPolicy().fixedColumnWidth, 700.0);

        // Flip live (simulates SettingsDialog::applySettings -> configChanged
        // -> MainWindow::onSettingsApplied -> applyReadableLineWidth).
        CorbomiteSettings::self()->setReadableLineWidth(false);
        CorbomiteSettings::self()->save();
        editor->applyReadableLineWidth(false);

        QCOMPARE(canvas->contentWidthPolicy().kind,
                 Markoff::Canvas::ContentWidthPolicy::FullWidth);

        CorbomiteSettings::self()->setReadableLineWidth(true);
        CorbomiteSettings::self()->save();
    }
};

QTEST_MAIN(TstMainWindowTitleRename)
#include "tst_mainwindow_title_rename.moc"
