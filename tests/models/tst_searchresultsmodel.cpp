// SPDX-License-Identifier: GPL-3.0-or-later
//
// SearchResultsModel is a two-level tree (file group -> matches) consumed by
// SearchView (src/plugins/search/SearchView.cpp) and SearchResultsDelegate
// (src/plugins/search/SearchResultsDelegate.cpp). It had zero test coverage
// despite having two production consumers.

#include <QSignalSpy>
#include <QTest>

#include "corbomite/models/SearchResultsModel.h"

using Corbomite::SearchMatch;
using Corbomite::SearchResultsModel;

// Undefeats the preprocessor: the comma inside QPair<int, int> would otherwise
// be read as a second macro argument to QVERIFY/QCOMPARE.
using Ranges = QVector<QPair<int, int>>;

class TestSearchResultsModel : public QObject
{
    Q_OBJECT

    static SearchMatch makeMatch(const QString &path,
                                 const QString &snippet,
                                 const QVector<QPair<int, int>> &ranges = {})
    {
        SearchMatch m;
        m.notePath = path;
        m.snippet = snippet;
        m.matches = ranges;
        return m;
    }

private Q_SLOTS:

    // ---- empty / freshly constructed ----

    void testFreshModelIsEmpty()
    {
        SearchResultsModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.columnCount(), 1);
        QCOMPARE(model.fileCount(), 0);
        QCOMPARE(model.totalMatchCount(), 0);
        QVERIFY(!model.index(0, 0).isValid());
    }

    void testDataOnInvalidIndexIsEmptyVariant()
    {
        SearchResultsModel model;
        for (int role : {int(Qt::DisplayRole), int(SearchResultsModel::NotePathRole),
                         int(SearchResultsModel::SnippetRole),
                         int(SearchResultsModel::MatchCountRole),
                         int(SearchResultsModel::MatchRangesRole)}) {
            const QVariant v = model.data(QModelIndex(), role);
            QVERIFY2(!v.isValid(), qPrintable(QStringLiteral("role %1 leaked a value").arg(role)));
        }
    }

    // ---- setResults / grouping ----

    void testGroupsMatchesByFileInFirstAppearanceOrder()
    {
        SearchResultsModel model;
        // Interleaved on purpose: b, a, b, c, a
        model.setResults({
            makeMatch(QStringLiteral("notes/b.md"), QStringLiteral("bee")),
            makeMatch(QStringLiteral("notes/a.md"), QStringLiteral("aye")),
            makeMatch(QStringLiteral("notes/b.md"), QStringLiteral("bee again")),
            makeMatch(QStringLiteral("notes/c.md"), QStringLiteral("sea")),
            makeMatch(QStringLiteral("notes/a.md"), QStringLiteral("aye again")),
        });

        QCOMPARE(model.fileCount(), 3);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.totalMatchCount(), 5);

        // Order follows first appearance, not alphabetical.
        QCOMPARE(model.index(0, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("notes/b.md"));
        QCOMPARE(model.index(1, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("notes/a.md"));
        QCOMPARE(model.index(2, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("notes/c.md"));

        // Per-group counts.
        QCOMPARE(model.index(0, 0).data(SearchResultsModel::MatchCountRole).toInt(), 2);
        QCOMPARE(model.index(1, 0).data(SearchResultsModel::MatchCountRole).toInt(), 2);
        QCOMPARE(model.index(2, 0).data(SearchResultsModel::MatchCountRole).toInt(), 1);

        // Matches land in the right group, in arrival order.
        const QModelIndex b = model.index(0, 0);
        QCOMPARE(model.rowCount(b), 2);
        QCOMPARE(model.index(0, 0, b).data(SearchResultsModel::SnippetRole).toString(),
                 QStringLiteral("bee"));
        QCOMPARE(model.index(1, 0, b).data(SearchResultsModel::SnippetRole).toString(),
                 QStringLiteral("bee again"));
    }

    void testGroupDisplayRoleIsNameAndCount()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("deep/nested/path/My Note.md"), QStringLiteral("x")),
            makeMatch(QStringLiteral("deep/nested/path/My Note.md"), QStringLiteral("y")),
        });
        // Directory stripped, ".md" stripped, count appended.
        QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(),
                 QStringLiteral("My Note (2)"));
    }

    void testNoteNameDotHandling()
    {
        SearchResultsModel model;
        model.setResults({
            // No extension -> whole basename.
            makeMatch(QStringLiteral("a/README"), QStringLiteral("x")),
            // Leading-dot name (".gitignore"): dot at index 0 must NOT be treated
            // as an extension separator.
            makeMatch(QStringLiteral("a/.gitignore"), QStringLiteral("y")),
            // Dots inside the stem are kept.
            makeMatch(QStringLiteral("a/archive.2026.md"), QStringLiteral("z")),
        });
        QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("README (1)"));
        QCOMPARE(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral(".gitignore (1)"));
        QCOMPARE(model.index(2, 0).data(Qt::DisplayRole).toString(), QStringLiteral("archive.2026 (1)"));
    }

    void testRootLevelNoteHasNoDirectory()
    {
        SearchResultsModel model;
        model.setResults({makeMatch(QStringLiteral("Top.md"), QStringLiteral("x"))});
        QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("Top (1)"));
        QCOMPARE(model.index(0, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("Top.md"));
    }

    // ---- tree shape ----

    void testParentChildRelationship()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("a.md"), QStringLiteral("one")),
            makeMatch(QStringLiteral("a.md"), QStringLiteral("two")),
            makeMatch(QStringLiteral("b.md"), QStringLiteral("three")),
        });

        const QModelIndex groupA = model.index(0, 0);
        const QModelIndex child0 = model.index(0, 0, groupA);
        const QModelIndex child1 = model.index(1, 0, groupA);

        // A top-level row has no parent.
        QVERIFY(!model.parent(groupA).isValid());
        // A child points back at its group.
        QCOMPARE(model.parent(child0).row(), groupA.row());
        QCOMPARE(model.parent(child0).internalId(), groupA.internalId());
        QCOMPARE(model.parent(child1).row(), groupA.row());
        QVERIFY(!model.parent(QModelIndex()).isValid());
    }

    void testSingleColumnOnly()
    {
        SearchResultsModel model;
        model.setResults({makeMatch(QStringLiteral("a.md"), QStringLiteral("one"))});
        QCOMPARE(model.columnCount(), 1);
        QCOMPARE(model.columnCount(model.index(0, 0)), 1);
        QVERIFY(!model.index(0, 1).isValid());

        // Two levels only: a group row accepts children, but a match row does
        // not (its internalId is a group index, not the top-level sentinel).
        const QModelIndex group = model.index(0, 0);
        QVERIFY(model.index(0, 0, group).isValid());
        QVERIFY(!model.index(0, 0, model.index(0, 0, group)).isValid());
    }

    void testOutOfRangeIndicesAreInvalid()
    {
        SearchResultsModel model;
        model.setResults({makeMatch(QStringLiteral("a.md"), QStringLiteral("one"))});

        QVERIFY(!model.index(1, 0).isValid());      // past last group
        QVERIFY(!model.index(-1, 0).isValid());
        QVERIFY(!model.index(5, 0, model.index(0, 0)).isValid()); // past last match

        // An out-of-range index() collapses to invalid, and an invalid parent
        // means "top level" per the Qt convention — so rowCount() falls back
        // to the group count rather than 0.
        QCOMPARE(model.rowCount(model.index(1, 0)), 1);
        QCOMPARE(model.rowCount(QModelIndex()), 1);
    }

    // ---- roles ----

    void testNotePathRoleAvailableOnBothLevels()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("dir/note.md"), QStringLiteral("one")),
            makeMatch(QStringLiteral("dir/note.md"), QStringLiteral("two")),
        });
        // SearchView::onResultClicked reads NotePathRole off whatever row was
        // clicked, so it must resolve on the match rows too, not just groups.
        QCOMPARE(model.index(0, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("dir/note.md"));
        const QModelIndex child = model.index(1, 0, model.index(0, 0));
        QCOMPARE(child.data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("dir/note.md"));
    }

    void testMatchRangesRoleRoundTrips()
    {
        SearchResultsModel model;
        const Ranges ranges{{0, 3}, {7, 12}};
        model.setResults({makeMatch(QStringLiteral("a.md"), QStringLiteral("foo bar baz"), ranges)});

        const QModelIndex child = model.index(0, 0, model.index(0, 0));
        const QVariant v = child.data(SearchResultsModel::MatchRangesRole);
        QVERIFY(v.isValid());
        QVERIFY(v.canConvert<Ranges>());
        QCOMPARE(v.value<Ranges>(), ranges);
    }

    void testMatchRangesRoleAbsentOnGroupRows()
    {
        SearchResultsModel model;
        model.setResults({makeMatch(QStringLiteral("a.md"), QStringLiteral("one"))});

        // The header documents this role as returning an empty QVector on
        // group rows. In fact setResults()/data() leave it unset entirely
        // (invalid QVariant) — SearchResultsDelegate guards on
        // `!rangesVar.isValid()`, so both spellings are harmless, but the
        // assertion below pins what actually happens.
        const QVariant groupRanges =
            model.index(0, 0).data(SearchResultsModel::MatchRangesRole);
        QVERIFY(!groupRanges.isValid());
    }

    void testMatchCountRoleIsGroupOnly()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("a.md"), QStringLiteral("one")),
            makeMatch(QStringLiteral("a.md"), QStringLiteral("two")),
        });
        const QModelIndex child = model.index(0, 0, model.index(0, 0));
        QVERIFY(!child.data(SearchResultsModel::MatchCountRole).isValid());
    }

    // ---- mutation signalling ----

    void testSetResultsEmitsModelReset()
    {
        SearchResultsModel model;
        QSignalSpy spy(&model, &QAbstractItemModel::modelReset);

        model.setResults({makeMatch(QStringLiteral("a.md"), QStringLiteral("one"))});
        QCOMPARE(spy.count(), 1);

        // Replacing with an empty set must also reset, not leave stale rows.
        model.setResults({});
        QCOMPARE(spy.count(), 2);
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.fileCount(), 0);
    }

    void testSetResultsReplacesPreviousResults()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("a.md"), QStringLiteral("one")),
            makeMatch(QStringLiteral("b.md"), QStringLiteral("two")),
        });
        QCOMPARE(model.fileCount(), 2);

        model.setResults({makeMatch(QStringLiteral("c.md"), QStringLiteral("three"))});
        QCOMPARE(model.fileCount(), 1);
        QCOMPARE(model.totalMatchCount(), 1);
        QCOMPARE(model.index(0, 0).data(SearchResultsModel::NotePathRole).toString(),
                 QStringLiteral("c.md"));
    }

    void testClearEmptiesModel()
    {
        SearchResultsModel model;
        model.setResults({
            makeMatch(QStringLiteral("a.md"), QStringLiteral("one")),
            makeMatch(QStringLiteral("b.md"), QStringLiteral("two")),
        });

        QSignalSpy spy(&model, &QAbstractItemModel::modelReset);
        model.clear();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.fileCount(), 0);
        QCOMPARE(model.totalMatchCount(), 0);
        QVERIFY(!model.index(0, 0).isValid());
    }

    void testClearOnAlreadyEmptyModelIsHarmless()
    {
        SearchResultsModel model;
        QSignalSpy spy(&model, &QAbstractItemModel::modelReset);
        model.clear();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.rowCount(), 0);
    }

    void testUnicodeSnippetAndPathSurvive()
    {
        SearchResultsModel model;
        const QString path = QStringLiteral("笔记/Ünïcode Note.md");
        model.setResults({makeMatch(path, QString::fromUtf8("雪だるま \xF0\x9F\x8D\x8A"))});

        QCOMPARE(model.index(0, 0).data(SearchResultsModel::NotePathRole).toString(), path);
        QCOMPARE(model.index(0, 0, model.index(0, 0)).data(SearchResultsModel::SnippetRole).toString(),
                 QString::fromUtf8("雪だるま \xF0\x9F\x8D\x8A"));
        // Basename extraction operates on the raw path.
        QCOMPARE(model.index(0, 0).data(Qt::DisplayRole).toString(),
                 QString::fromUtf8("Ünïcode Note (1)"));
    }

    void testManyGroupsAndMatches()
    {
        SearchResultsModel model;
        QVector<SearchMatch> results;
        // 20 files x 5 matches each, interleaved round-robin.
        for (int rep = 0; rep < 5; ++rep) {
            for (int f = 0; f < 20; ++f) {
                results.append(makeMatch(QStringLiteral("f%1.md").arg(f),
                                         QStringLiteral("m%1").arg(rep)));
            }
        }
        model.setResults(results);

        QCOMPARE(model.fileCount(), 20);
        QCOMPARE(model.totalMatchCount(), 100);
        for (int f = 0; f < 20; ++f) {
            const QModelIndex g = model.index(f, 0);
            QCOMPARE(model.rowCount(g), 5);
            QCOMPARE(g.data(SearchResultsModel::MatchCountRole).toInt(), 5);
        }
    }
};

QTEST_MAIN(TestSearchResultsModel)
#include "tst_searchresultsmodel.moc"
