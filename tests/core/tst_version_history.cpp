// tests/core/tst_version_history.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster F-a P1.9 — Version History: listing order, preview, restore round
// trip, and the pre-restore safety copy (P1.6 / Q4).
#include <QTest>
#include "corbomite/core/VersionHistoryService.h"
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

}  // namespace

class TestVersionHistory : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void listsNewestFirstAndPreviews()
    {
        MemoryAdapter a;
        a.files[QStringLiteral("/vault/foo.md")] = QStringLiteral("disk");
        RecoveryStore store(&a, QStringLiteral("/vault"));
        store.write(QStringLiteral("foo.md"), QStringLiteral("v2"), false, 2000);
        store.write(QStringLiteral("foo.md"), QStringLiteral("v1"), false, 1000);

        VersionHistoryService svc(&a, QStringLiteral("/vault"));
        const auto snaps = svc.snapshots(QStringLiteral("foo.md"));
        QCOMPARE(snaps.size(), 2);
        QCOMPARE(snaps.at(0).timestampMs, 2000LL);
        QCOMPARE(svc.preview(snaps.at(0)).value(), QStringLiteral("v2"));
    }

    void restoreOverwritesAndKeepsAPreRestoreCopy()
    {
        MemoryAdapter a;
        a.files[QStringLiteral("/vault/foo.md")] = QStringLiteral("current");
        RecoveryStore store(&a, QStringLiteral("/vault"));
        store.write(QStringLiteral("foo.md"), QStringLiteral("old version"), false, 1000);

        VersionHistoryService svc(&a, QStringLiteral("/vault"));
        const auto snaps = svc.snapshots(QStringLiteral("foo.md"));
        QCOMPARE(snaps.size(), 1);
        QVERIFY(svc.restore(QStringLiteral("foo.md"), snaps.at(0)));

        // The file now holds the snapshot content...
        QCOMPARE(a.files.value(QStringLiteral("/vault/foo.md")),
                 QStringLiteral("old version"));
        // ...and the content we overwrote survives as a new snapshot, so the
        // restore itself is undoable.
        const auto after = svc.snapshots(QStringLiteral("foo.md"));
        QCOMPARE(after.size(), 2);
        bool foundPre = false;
        for (const auto &s : after) {
            if (svc.preview(s).value() == QStringLiteral("current")) foundPre = true;
        }
        QVERIFY2(foundPre, "pre-restore content must be snapshotted");
    }

    void restoreSkipsSafetyCopyWhenDiskAlreadyMatches()
    {
        MemoryAdapter a;
        a.files[QStringLiteral("/vault/foo.md")] = QStringLiteral("same");
        RecoveryStore store(&a, QStringLiteral("/vault"));
        store.write(QStringLiteral("foo.md"), QStringLiteral("same"), false, 1000);

        VersionHistoryService svc(&a, QStringLiteral("/vault"));
        const auto snaps = svc.snapshots(QStringLiteral("foo.md"));
        QVERIFY(svc.restore(QStringLiteral("foo.md"), snaps.at(0)));
        // No redundant pre-restore copy when the content is identical.
        QCOMPARE(svc.snapshots(QStringLiteral("foo.md")).size(), 1);
    }

    void restoreFailsWhenSnapshotIsMissing()
    {
        MemoryAdapter a;
        a.files[QStringLiteral("/vault/foo.md")] = QStringLiteral("current");
        VersionHistoryService svc(&a, QStringLiteral("/vault"));
        RecoverySnapshot missing{storePathMissing(), 1000, false};
        QVERIFY(!svc.restore(QStringLiteral("foo.md"), missing));
        QCOMPARE(a.files.value(QStringLiteral("/vault/foo.md")),
                 QStringLiteral("current"));
    }

private:
    static QString storePathMissing() { return QStringLiteral("/nonexistent/snap.md"); }
};

QTEST_MAIN(TestVersionHistory)
#include "tst_version_history.moc"
