// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>

#include "app/RecentVaults.h"

using Corbomite::RecentVaults;

class TestRecentVaults : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        RecentVaults rv;
        rv.clear();
    }

    void addAndListReturnsExistingPaths()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        RecentVaults rv;
        rv.add(dir.path());

        const QStringList got = rv.list();
        QCOMPARE(got.size(), 1);
        QCOMPARE(got.first(), dir.path());
    }

    void nonExistingPathsAreDroppedFromList()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString gonePath = dir.path() + QStringLiteral("/nope");

        RecentVaults rv;
        rv.add(dir.path());
        rv.add(gonePath);

        const QStringList got = rv.list();
        QCOMPARE(got.size(), 1);
        QCOMPARE(got.first(), dir.path());
    }

    void addHoistsExistingEntryToFront()
    {
        QTemporaryDir a;
        QTemporaryDir b;
        QVERIFY(a.isValid());
        QVERIFY(b.isValid());

        RecentVaults rv;
        rv.add(a.path());
        rv.add(b.path());
        rv.add(a.path());  // hoist

        const QStringList got = rv.list();
        QCOMPARE(got.size(), 2);
        QCOMPARE(got.first(), a.path());
        QCOMPARE(got.at(1), b.path());
    }

    void listCapsAtTenEntries()
    {
        QList<QTemporaryDir *> dirs;
        RecentVaults rv;
        for (int i = 0; i < 12; ++i) {
            auto *d = new QTemporaryDir();
            QVERIFY(d->isValid());
            dirs.append(d);
            rv.add(d->path());
        }

        const QStringList got = rv.list();
        QCOMPARE(got.size(), 10);
        // Most-recently-added appears first.
        QCOMPARE(got.first(), dirs.at(11)->path());

        qDeleteAll(dirs);
    }

    void savePersistsLoadRecovers()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        {
            RecentVaults rv;
            rv.add(dir.path());
            rv.save();
        }

        {
            RecentVaults rv;
            rv.load();
            const QStringList got = rv.list();
            QCOMPARE(got.size(), 1);
            QCOMPARE(got.first(), dir.path());
        }
    }
};

QTEST_MAIN(TestRecentVaults)
#include "tst_recent_vaults.moc"
