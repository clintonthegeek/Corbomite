// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/storage/SQLiteIndex.h"

class TestSQLiteIndex : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testOpenClose()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        QVERIFY(index.open(tmp.path() + "/test.sqlite"));
        index.close();
    }

    void testIndexAndSearch()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("My Note"),
                        QStringLiteral("This is some content about programming and Qt."));

        auto results = index.search(QStringLiteral("programming"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).notePath, QStringLiteral("note.md"));
        QVERIFY(!results.at(0).snippet.isEmpty());
    }

    void testSearchNoMatch()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Hello world"));

        auto results = index.search(QStringLiteral("nonexistent"));
        QCOMPARE(results.size(), 0);
    }

    void testRemoveNote()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("findable content"));
        QCOMPARE(index.search(QStringLiteral("findable")).size(), 1);

        index.removeNote(QStringLiteral("note.md"));
        QCOMPARE(index.search(QStringLiteral("findable")).size(), 0);
    }

    void testUpdateExistingNote()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("old content"));
        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("new content"));

        QCOMPARE(index.search(QStringLiteral("old")).size(), 0);
        QCOMPARE(index.search(QStringLiteral("new")).size(), 1);
    }

    void testMultipleNotes()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("Alpha"),
                        QStringLiteral("shared word unique_alpha"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("Beta"),
                        QStringLiteral("shared word unique_beta"));

        QCOMPARE(index.search(QStringLiteral("shared")).size(), 2);
        QCOMPARE(index.search(QStringLiteral("unique_alpha")).size(), 1);
    }

    void testSearchByTitle()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Special Title"),
                        QStringLiteral("boring content"));

        auto results = index.search(QStringLiteral("Special"));
        QCOMPARE(results.size(), 1);
    }

    void testRebuildIndex()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        QFile f1(tmp.path() + "/vault/note1.md");
        f1.open(QIODevice::WriteOnly);
        f1.write("# First\n\nContent one");
        f1.close();
        QFile f2(tmp.path() + "/vault/sub/note2.md");
        QDir().mkpath(tmp.path() + "/vault/sub");
        f2.open(QIODevice::WriteOnly);
        f2.write("# Second\n\nContent two");
        f2.close();

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(tmp.path() + "/vault");

        QCOMPARE(index.search(QStringLiteral("Content")).size(), 2);
        QCOMPARE(index.search(QStringLiteral("First")).size(), 1);
    }
};

QTEST_MAIN(TestSQLiteIndex)
#include "tst_sqliteindex.moc"
