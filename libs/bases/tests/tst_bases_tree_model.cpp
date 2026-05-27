// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QAbstractItemModelTester>
#include <QFile>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/BasesQueryResult.h"   // BasesEntryGroup
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/Values.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"

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

    void testGroupRowData()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        m.populateForTesting({ grp("Active", 2, q) }, {note("status"), note("title")});
        // The single keyed group is NOT flat -> a group row exists.
        const QModelIndex g0 = m.index(0, 0, QModelIndex());
        QVERIFY(m.isGroupRow(g0));
        QCOMPARE(m.data(g0, BasesTreeModel::IsGroupRowRole).toBool(), true);
        QCOMPARE(m.data(g0, BasesTreeModel::GroupCountRole).toInt(), 2);
        QCOMPARE(m.data(g0, Qt::DisplayRole).toString(), QStringLiteral("Active"));
        // entry rows are not group rows
        const QModelIndex e = m.index(0, 0, g0);
        QCOMPARE(m.data(e, BasesTreeModel::IsGroupRowRole).toBool(), false);
    }
    void testNullKeyGroupLabel()
    {
        BasesQuery q;
        BasesTreeModel m(nullptr, nullptr);
        // two groups so it's not flat; second is keyless
        m.populateForTesting({ grp("Active", 1, q), grp(nullptr, 1, q) }, {note("status")});
        const QModelIndex g1 = m.index(1, 0, QModelIndex());
        QCOMPARE(m.data(g1, Qt::DisplayRole).toString(), QStringLiteral("(no value)"));
    }

    void testEmptyModel()
    {
        BasesTreeModel m(nullptr, nullptr);   // never populated
        QCOMPARE(m.rowCount(QModelIndex()), 0);
        QCOMPARE(m.columnCount(QModelIndex()), 0);
        QVERIFY(!m.index(0, 0, QModelIndex()).isValid());
    }
    void testInvalidIndexDataIsSafe()
    {
        BasesTreeModel m(nullptr, nullptr);
        QVERIFY(!m.data(QModelIndex(), Qt::DisplayRole).isValid());
        QCOMPARE(m.setData(QModelIndex(), QVariant(42), Qt::EditRole), false);
        QCOMPARE(m.flags(QModelIndex()), Qt::NoItemFlags);
    }

    void mimeDataYieldsWikilinkForEntries() {
        // One flat group with a single entry backed by a real TFile.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        { QFile f(dir.path() + QStringLiteral("/Alien.md"));
          QVERIFY(f.open(QIODevice::WriteOnly)); f.write("# Alien\n"); }
        Corbomite::FileSystemAdapter adapter;
        Corbomite::Vault vault(&adapter);
        vault.load(dir.path());
        Corbomite::TFile *tf = vault.getFileByPath(QStringLiteral("Alien.md"));
        QVERIFY(tf);

        BasesQuery q;
        BasesEntryGroup g;
        g.entries.push_back(std::make_shared<BasesEntry>(&vault, nullptr, tf, tf, q));
        BasesTreeModel m(nullptr, nullptr);
        m.populateForTesting({ g }, { note("title") });   // single keyless group -> flat

        const QModelIndex idx = m.index(0, 0, QModelIndex());
        QVERIFY(idx.isValid());
        QVERIFY(m.flags(idx) & Qt::ItemIsDragEnabled);
        std::unique_ptr<QMimeData> md(m.mimeData({ idx }));
        QVERIFY(md);
        QCOMPARE(md->text(), QStringLiteral("[[Alien]]"));
    }

    void setDataEmitsRequestAndDoesNotWrite()
    {
        QTemporaryDir dir;
        {
            QFile f(dir.path() + "/n.md");
            f.open(QIODevice::WriteOnly);
            f.write("---\nstatus: old\n---\nbody\n");
            f.close();
        }
        Corbomite::FileSystemAdapter fs;
        Corbomite::Vault vault(&fs);
        vault.load(dir.path());
        Corbomite::FileManager fm(&vault, nullptr);
        auto *tf = vault.getFileByPath(QStringLiteral("n.md"));
        QVERIFY(tf);

        BasesQuery q;
        BasesTreeModel m(nullptr, &fm);
        BasesEntryGroup g;   // one keyless group holding our real-file entry
        g.entries.push_back(std::make_shared<BasesEntry>(&vault, nullptr, tf, nullptr, q));
        m.populateForTesting({g}, {note("status")});

        const QModelIndex idx = m.index(0, 0, QModelIndex());
        QVERIFY(idx.isValid());
        QVERIFY(!m.isGroupRow(idx));

        QSignalSpy spy(&m, &BasesTreeModel::frontMatterEditRequested);
        QVERIFY(m.setData(idx, QStringLiteral("new"), Qt::EditRole));
        QCOMPARE(spy.count(), 1);

        // The model must NOT have written to disk (only the chokepoint does).
        QFile after(dir.path() + "/n.md");
        after.open(QIODevice::ReadOnly);
        const QByteArray bytes = after.readAll();
        QVERIFY(bytes.contains("status: old"));
        QVERIFY(!bytes.contains("status: new"));
    }
};

QTEST_MAIN(TestBasesTreeModel)
#include "tst_bases_tree_model.moc"
