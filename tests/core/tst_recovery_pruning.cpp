// tests/core/tst_recovery_pruning.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster F-a P1 — RecoveryStore retention: age ceiling, count ceiling,
// save-failure exemption, vault-exit purge, newest-first listing.
#include <QTest>
#include "corbomite/core/RecoveryStore.h"
#include "corbomite/core/DataAdapter.h"

using namespace Corbomite;

namespace {

class MemoryAdapter : public DataAdapter
{
public:
    QHash<QString, QString> files;
    bool exists(const QString &p) const override { return files.contains(p); }
    std::optional<QString> read(const QString &p) const override {
        if (files.contains(p)) return files[p];
        return std::nullopt;
    }
    std::optional<QByteArray> readBinary(const QString &) const override { return std::nullopt; }
    FileStat stat(const QString &) const override { return {}; }
    QStringList list(const QString &dir) const override {
        QStringList out;
        const QString prefix = dir.endsWith(QLatin1Char('/')) ? dir : dir + QLatin1Char('/');
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            if (!it.key().startsWith(prefix)) continue;
            const QString rest = it.key().mid(prefix.size());
            if (rest.isEmpty() || rest.contains(QLatin1Char('/'))) continue;
            out.append(rest);
        }
        return out;
    }
    bool write(const QString &p, const QString &c, const WriteHints & = {}) override {
        files[p] = c;
        return true;
    }
    bool writeBinary(const QString &, const QByteArray &, const WriteHints & = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &p) override { return files.remove(p) > 0; }
    bool rmdir(const QString &) override { return true; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

constexpr qint64 kDay = 24LL * 60 * 60 * 1000;

}  // namespace

class TestRecoveryPruning : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void listIsNewestFirst()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        store.write(QStringLiteral("notes/foo.md"), QStringLiteral("one"), false, 1000);
        store.write(QStringLiteral("notes/foo.md"), QStringLiteral("three"), false, 3000);
        store.write(QStringLiteral("notes/foo.md"), QStringLiteral("two"), false, 2000);

        const auto snaps = store.list(QStringLiteral("notes/foo.md"));
        QCOMPARE(snaps.size(), 3);
        QCOMPARE(snaps.at(0).timestampMs, 3000LL);
        QCOMPARE(snaps.at(1).timestampMs, 2000LL);
        QCOMPARE(snaps.at(2).timestampMs, 1000LL);
    }

    void storeIsOutsideTheVaultAndPerFile()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        const QString p = store.write(QStringLiteral("notes/foo.md"),
                                      QStringLiteral("x"), false, 1000);
        QVERIFY(!p.isEmpty());
        QVERIFY(!p.startsWith(QStringLiteral("/vault")));
        QVERIFY(p.contains(QStringLiteral("file-recovery")));
        QVERIFY(p.endsWith(QStringLiteral(".snapshot.md")));
        QVERIFY(store.fileDir(QStringLiteral("a/foo.md"))
                != store.fileDir(QStringLiteral("b/foo.md")));
    }

    void ageCeilingDropsOldPeriodicSnapshots()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        const qint64 now = 100 * kDay;
        store.write(QStringLiteral("foo.md"), QStringLiteral("old"), false, now - 8 * kDay);
        store.write(QStringLiteral("foo.md"), QStringLiteral("recent"), false, now - 6 * kDay);

        QCOMPARE(store.prune(QStringLiteral("foo.md"), /*maxAgeDays=*/7,
                             /*maxPerFile=*/0, now),
                 1);
        const auto snaps = store.list(QStringLiteral("foo.md"));
        QCOMPARE(snaps.size(), 1);
        QCOMPARE(snaps.at(0).timestampMs, now - 6 * kDay);
    }

    void countCeilingKeepsNewest()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        for (int i = 1; i <= 5; ++i)
            store.write(QStringLiteral("foo.md"), QStringLiteral("v%1").arg(i),
                        false, i * kDay);

        QCOMPARE(store.prune(QStringLiteral("foo.md"), /*maxAgeDays=*/0,
                             /*maxPerFile=*/2, 5 * kDay),
                 3);
        const auto snaps = store.list(QStringLiteral("foo.md"));
        QCOMPARE(snaps.size(), 2);
        QCOMPARE(snaps.at(0).timestampMs, 5 * kDay);
        QCOMPARE(snaps.at(1).timestampMs, 4 * kDay);
    }

    void failureBackupsAreNeverPruned()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        const qint64 now = 100 * kDay;
        // A very old save-failure backup, plus periodic snapshots that would
        // all be dropped.
        store.write(QStringLiteral("foo.md"), QStringLiteral("oops"), true, now - 50 * kDay);
        store.write(QStringLiteral("foo.md"), QStringLiteral("p1"), false, now - 50 * kDay);
        store.write(QStringLiteral("foo.md"), QStringLiteral("p2"), false, now - 40 * kDay);

        store.prune(QStringLiteral("foo.md"), /*maxAgeDays=*/7,
                    /*maxPerFile=*/1, now);
        const auto snaps = store.list(QStringLiteral("foo.md"));
        QCOMPARE(snaps.size(), 1);
        QVERIFY(snaps.at(0).failure);
        QCOMPARE(a.files.value(snaps.at(0).path), QStringLiteral("oops"));
    }

    void removeAllPurgesEverySnapshotForTheFile()
    {
        MemoryAdapter a;
        RecoveryStore store(&a, QStringLiteral("/vault"));
        store.write(QStringLiteral("foo.md"), QStringLiteral("p"), false, 1000);
        store.write(QStringLiteral("foo.md"), QStringLiteral("f"), true, 2000);
        store.write(QStringLiteral("bar.md"), QStringLiteral("other"), false, 3000);

        store.removeAll(QStringLiteral("foo.md"));
        QVERIFY(store.list(QStringLiteral("foo.md")).isEmpty());
        // A sibling file's entry is untouched.
        QCOMPARE(store.list(QStringLiteral("bar.md")).size(), 1);
    }
};

QTEST_MAIN(TestRecoveryPruning)
#include "tst_recovery_pruning.moc"
