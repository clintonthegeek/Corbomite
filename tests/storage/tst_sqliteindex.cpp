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

    // --- Link extraction tests ---

    void testWikiLinkExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target Note]] for details."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target Note.md"));
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("wiki"));
    }

    void testWikiLinkWithAlias()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target|displayed text]] here."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target.md"));
        QCOMPARE(outlinks.at(0).displayText, QStringLiteral("displayed text"));
    }

    void testEmbedExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Embed: ![[image.png]]"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("embed"));
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("image.png"));
    }

    void testMarkdownLinkExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [click here](other.md) for more."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("other.md"));
        QCOMPARE(outlinks.at(0).linkType, QStringLiteral("markdown"));
    }

    void testHeadingLinkStripsFragment()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("See [[Target#Section One]] for info."));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Target.md"));
    }

    void testBacklinksFor()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("A"),
                        QStringLiteral("Links to [[Target]]"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("B"),
                        QStringLiteral("Also links to [[Target]]"));
        index.indexNote(QStringLiteral("c.md"), QStringLiteral("C"),
                        QStringLiteral("No links here"));

        auto backlinks = index.backlinksFor(QStringLiteral("Target.md"));
        QCOMPARE(backlinks.size(), 2);

        QStringList sources;
        for (const auto &link : backlinks) sources << link.sourcePath;
        QVERIFY(sources.contains(QStringLiteral("a.md")));
        QVERIFY(sources.contains(QStringLiteral("b.md")));
    }

    void testOutlinksFor()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[A]] and [[B]] and ![[C.png]]"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 3);
    }

    void testRemoveNoteRemovesLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[Target]]"));
        QCOMPARE(index.backlinksFor(QStringLiteral("Target.md")).size(), 1);

        index.removeNote(QStringLiteral("source.md"));
        QCOMPARE(index.backlinksFor(QStringLiteral("Target.md")).size(), 0);
    }

    void testOrphanLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        // source links to Target, but Target doesn't exist as an indexed note
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Links to [[Nonexistent Note]]"));

        auto orphans = index.orphanLinks();
        QVERIFY(orphans.contains(QStringLiteral("Nonexistent Note.md")));

        // Now index the target — it should no longer be orphan
        index.indexNote(QStringLiteral("Nonexistent Note.md"), QStringLiteral("Nonexistent Note"),
                        QStringLiteral("I exist now"));
        orphans = index.orphanLinks();
        QVERIFY(!orphans.contains(QStringLiteral("Nonexistent Note.md")));
    }

    void testLinksInCodeBlockExcluded()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("Real [[Link]]\n\n```\n[[Not A Link]]\n```\n"));

        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("Link.md"));
    }

    // --- Tag tests ---

    void testTagExtraction()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Hello #project and #status/active tag"));

        auto tags = index.allTags();
        QVERIFY(tags.contains(QStringLiteral("project")));
        QVERIFY(tags.contains(QStringLiteral("status/active")));
    }

    void testNotesWithTag()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("A"),
                        QStringLiteral("Has #shared and #onlyA"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("B"),
                        QStringLiteral("Has #shared and #onlyB"));

        auto shared = index.notesWithTag(QStringLiteral("shared"));
        QCOMPARE(shared.size(), 2);

        auto onlyA = index.notesWithTag(QStringLiteral("onlyA"));
        QCOMPARE(onlyA.size(), 1);
        QCOMPARE(onlyA.at(0), QStringLiteral("a.md"));
    }

    void testTagsInCodeBlockExcluded()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Real #tag\n\n```\n#not-a-tag\n```\n"));

        auto tags = index.allTags();
        QVERIFY(tags.contains(QStringLiteral("tag")));
        QVERIFY(!tags.contains(QStringLiteral("not-a-tag")));
    }

    void testReindexUpdatesLinks()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        // First index with link
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("[[OldTarget]]"));
        QCOMPARE(index.outlinksFor(QStringLiteral("source.md")).size(), 1);
        QCOMPARE(index.outlinksFor(QStringLiteral("source.md")).at(0).targetPath,
                 QStringLiteral("OldTarget.md"));

        // Re-index with different content
        index.indexNote(QStringLiteral("source.md"), QStringLiteral("Source"),
                        QStringLiteral("[[NewTarget]]"));
        auto outlinks = index.outlinksFor(QStringLiteral("source.md"));
        QCOMPARE(outlinks.size(), 1);
        QCOMPARE(outlinks.at(0).targetPath, QStringLiteral("NewTarget.md"));

        // Old target should have no backlinks
        QCOMPARE(index.backlinksFor(QStringLiteral("OldTarget.md")).size(), 0);
    }
};

QTEST_MAIN(TestSQLiteIndex)
#include "tst_sqliteindex.moc"
