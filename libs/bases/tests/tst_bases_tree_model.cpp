// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QAbstractItemModelTester>
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/BasesQueryResult.h"   // BasesEntryGroup
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }

// A group with a string key and `n` placeholder entries (null vault/cache/file).
BasesEntryGroup grp(const char *key, int n, const BasesQuery &q) {
    BasesEntryGroup g;
    if (key) g.key = std::make_shared<StringValue>(QString::fromLatin1(key));
    for (int i = 0; i < n; ++i)
        g.entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
    return g;
}
}

class TestBasesTreeModel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGroupedStructure()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);   // controller/fm null; populate directly
        QVector<BasesEntryGroup> groups{ grp("Active", 2, q), grp("Done", 3, q) };
        m.populateForTesting(groups, {note("status"), note("title")});

        QAbstractItemModelTester tester(&m);  // validates model invariants
        QCOMPARE(m.columnCount(QModelIndex()), 2);
        QCOMPARE(m.rowCount(QModelIndex()), 2);                 // two group rows
        const QModelIndex g0 = m.index(0, 0, QModelIndex());
        QVERIFY(g0.isValid());
        QVERIFY(m.isGroupRow(g0));
        QCOMPARE(m.rowCount(g0), 2);                            // group 0 has 2 entries
        const QModelIndex e = m.index(1, 0, g0);
        QVERIFY(e.isValid());
        QVERIFY(!m.isGroupRow(e));
        QCOMPARE(m.parent(e), g0);                             // entry's parent is its group
        QCOMPARE(m.rowCount(m.index(1, 0, QModelIndex())), 3); // group 1 has 3 entries
    }

    void testFlatWhenSingleKeylessGroup()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        QVector<BasesEntryGroup> groups{ grp(nullptr, 4, q) };  // one keyless group
        m.populateForTesting(groups, {note("title")});

        QCOMPARE(m.rowCount(QModelIndex()), 4);                 // flat: entries at root
        const QModelIndex e = m.index(2, 0, QModelIndex());
        QVERIFY(e.isValid());
        QVERIFY(!m.isGroupRow(e));
        QVERIFY(!m.parent(e).isValid());                       // parent is root
    }
};

QTEST_MAIN(TestBasesTreeModel)
#include "tst_bases_tree_model.moc"
