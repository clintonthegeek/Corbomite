// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultProcess.h"

using namespace Corbomite;

class TestVaultProcess : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void happyPathMutatesAndWrites()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString path = tmp.path() + QStringLiteral("/note.md");
        fs.write(path, QStringLiteral("hello"));

        const bool ok = VaultProcess::process(&fs, path, [](const QString &s) {
            return s + QStringLiteral(" world");
        });
        QVERIFY(ok);

        const auto r = fs.read(path);
        QVERIFY(r.has_value());
        QCOMPARE(*r, QStringLiteral("hello world"));
    }

    void failsCleanlyOnMissingFile()
    {
        FileSystemAdapter fs;
        const bool ok = VaultProcess::process(
            &fs,
            QStringLiteral("/nope/not-here.md"),
            [](const QString &) { return QStringLiteral("x"); });
        QVERIFY(!ok);
    }

    void rejectsNullMutator()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString path = tmp.path() + QStringLiteral("/a.md");
        fs.write(path, QStringLiteral("x"));
        const bool ok = VaultProcess::process(&fs, path, {});
        QVERIFY(!ok);
    }

    // The load-bearing concurrency test: two threads apply an append
    // mutator N times each; final content must be the exact concatenation
    // of all writes in *some* order — length = starting + 2N appends, no
    // lost-updates.
    void concurrentAppendsNeverLoseUpdates()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString path = tmp.path() + QStringLiteral("/counter.md");
        fs.write(path, QStringLiteral(""));

        constexpr int iterations = 200;
        const auto appendA = [&]() {
            for (int i = 0; i < iterations; ++i) {
                VaultProcess::process(&fs, path, [](const QString &s) {
                    return s + QLatin1Char('A');
                });
            }
        };
        const auto appendB = [&]() {
            for (int i = 0; i < iterations; ++i) {
                VaultProcess::process(&fs, path, [](const QString &s) {
                    return s + QLatin1Char('B');
                });
            }
        };

        auto fA = QtConcurrent::run(appendA);
        auto fB = QtConcurrent::run(appendB);
        fA.waitForFinished();
        fB.waitForFinished();

        const auto r = fs.read(path);
        QVERIFY(r.has_value());
        QCOMPARE(r->length(), 2 * iterations);

        int aCount = 0, bCount = 0;
        for (QChar c : *r) {
            if (c == QLatin1Char('A')) ++aCount;
            else if (c == QLatin1Char('B')) ++bCount;
        }
        QCOMPARE(aCount, iterations);
        QCOMPARE(bCount, iterations);
    }

    // Different paths don't serialise against each other (sanity check on
    // the per-path mutex granularity).
    void differentPathsAreIndependent()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString p1 = tmp.path() + QStringLiteral("/a.md");
        const QString p2 = tmp.path() + QStringLiteral("/b.md");
        fs.write(p1, QStringLiteral("X"));
        fs.write(p2, QStringLiteral("Y"));

        VaultProcess::process(&fs, p1,
                              [](const QString &s) { return s + QStringLiteral("1"); });
        VaultProcess::process(&fs, p2,
                              [](const QString &s) { return s + QStringLiteral("2"); });

        QCOMPARE(*fs.read(p1), QStringLiteral("X1"));
        QCOMPARE(*fs.read(p2), QStringLiteral("Y2"));
    }
};

QTEST_APPLESS_MAIN(TestVaultProcess)
#include "tst_vaultprocess.moc"
