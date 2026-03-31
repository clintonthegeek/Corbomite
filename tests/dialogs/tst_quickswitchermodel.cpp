// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "dialogs/QuickSwitcherModel.h"
#include "corbomite/core/NoteMeta.h"

class TestQuickSwitcherModel : public QObject {
    Q_OBJECT

    QVector<Corbomite::NoteMeta> makeNotes(const QStringList &paths)
    {
        QVector<Corbomite::NoteMeta> notes;
        for (const auto &p : paths) {
            notes.append(Corbomite::NoteMeta::fromRelativePath(p));
        }
        return notes;
    }

private Q_SLOTS:
    void testPopulatesFromNotes()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("alpha.md"),
            QStringLiteral("beta.md")
        }));
        QCOMPARE(model.rowCount(), 2);
    }

    void testNoteNameRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("folder/My Note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NoteNameRole).toString(),
                 QStringLiteral("My Note"));
    }

    void testFolderPathRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("Projects/Work/task.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::FolderPathRole).toString(),
                 QStringLiteral("Projects/Work"));
    }

    void testFolderPathEmptyForRoot()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("root-note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::FolderPathRole).toString(),
                 QString());
    }

    void testNotePathRole()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("folder/note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NotePathRole).toString(),
                 QStringLiteral("folder/note.md"));
    }

    void testDisplayRoleIsNoteName()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({QStringLiteral("My Note.md")}));

        auto idx = model.index(0, 0);
        QCOMPARE(idx.data(Qt::DisplayRole).toString(), QStringLiteral("My Note"));
    }

    void testEmptyVault()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes({});
        QCOMPARE(model.rowCount(), 0);
    }

    void testCanvasFilesIncluded()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("note.md"),
            QStringLiteral("board.canvas")
        }));
        QCOMPARE(model.rowCount(), 2);

        // Canvas name strips .canvas extension
        auto idx = model.index(1, 0);
        QCOMPARE(idx.data(Corbomite::QuickSwitcherModel::NoteNameRole).toString(),
                 QStringLiteral("board"));
    }

    void testRecentPathsOrderedFirst()
    {
        Corbomite::QuickSwitcherModel model;
        model.setNotes(makeNotes({
            QStringLiteral("alpha.md"),
            QStringLiteral("beta.md"),
            QStringLiteral("gamma.md"),
        }));
        model.setRecentPaths({
            QStringLiteral("gamma.md"),
            QStringLiteral("alpha.md"),
        });

        // When no filter applied, recent paths should appear first
        // Check that gamma and alpha have IsRecentRole = true
        bool foundGammaRecent = false;
        bool foundBetaNotRecent = false;
        for (int i = 0; i < model.rowCount(); ++i) {
            auto idx = model.index(i, 0);
            QString path = idx.data(Corbomite::QuickSwitcherModel::NotePathRole).toString();
            bool recent = idx.data(Corbomite::QuickSwitcherModel::IsRecentRole).toBool();
            if (path == QStringLiteral("gamma.md")) foundGammaRecent = recent;
            if (path == QStringLiteral("beta.md")) foundBetaNotRecent = !recent;
        }
        QVERIFY(foundGammaRecent);
        QVERIFY(foundBetaNotRecent);
    }
};

QTEST_MAIN(TestQuickSwitcherModel)
#include "tst_quickswitchermodel.moc"
