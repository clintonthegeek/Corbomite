// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 5 unit tests for MetadataWorker: serial queue discipline, burst
// survival, clean shutdown, pendingCount introspection.

#include <QTest>
#include <QSignalSpy>

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSet>
#include <QString>
#include <QStringList>

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataWorker.h"

using namespace Corbomite;

namespace {

LinkResolver makeResolver(const QStringList &paths)
{
    LinkResolver r;
    r.setVaultPaths(paths);
    return r;
}

QStringList generatePaths(int n)
{
    QStringList out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        out.append(QStringLiteral("note_%1.md").arg(i));
    }
    return out;
}

} // namespace

class TestMetadataWorker : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // 1. Enqueue one parse, assert the parsed signal carries the expected
    //    path, mtime, heading, and a non-empty 64-char hex hash.
    void testSingleParseRoundTrip()
    {
        LinkResolver resolver = makeResolver({QStringLiteral("a.md")});
        MetadataWorker worker(resolver);
        QSignalSpy spy(&worker, &MetadataWorker::parsed);

        worker.enqueueParse(QStringLiteral("a.md"),
                            QByteArray("# Hi\n"),
                            /*mtimeMs*/ 100,
                            /*size*/ 5);

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);

        const QVariantList args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("a.md"));
        QCOMPARE(args.at(1).toLongLong(), qint64{100});
        QCOMPARE(args.at(2).toLongLong(), qint64{5});
        const auto cache = args.at(3).value<CachedMetadata>();
        QVERIFY(cache.headings.has_value());
        QCOMPARE(cache.headings->size(), 1);
        QCOMPARE(cache.headings->at(0).heading, QStringLiteral("Hi"));

        const QString hash = args.at(4).toString();
        QCOMPARE(hash.size(), 64);
        // Lowercase hex only.
        for (const QChar c : hash) {
            const bool ok = (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                            || (c >= QLatin1Char('a') && c <= QLatin1Char('f'));
            QVERIFY2(ok, qPrintable(QStringLiteral("non-hex char in hash: ") + hash));
        }
    }

    // 2. Enqueue 10 distinct paths; parsed emissions must arrive in FIFO
    //    enqueue order.
    void testSerialQueueDiscipline()
    {
        const QStringList paths = generatePaths(10);
        LinkResolver resolver = makeResolver(paths);
        MetadataWorker worker(resolver);
        QSignalSpy spy(&worker, &MetadataWorker::parsed);

        for (int i = 0; i < paths.size(); ++i) {
            worker.enqueueParse(paths.at(i),
                                QByteArray("# Note ").append(QByteArray::number(i)).append('\n'),
                                100 + i,
                                10);
        }

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), paths.size(), 5000);

        for (int i = 0; i < paths.size(); ++i) {
            QCOMPARE(spy.at(i).at(0).toString(), paths.at(i));
        }
    }

    // 3. Enqueue 100 paths rapid-fire. All must arrive. No drops, no dupes.
    void testQueueSurvivesBurst()
    {
        const QStringList paths = generatePaths(100);
        LinkResolver resolver = makeResolver(paths);
        MetadataWorker worker(resolver);
        QSignalSpy spy(&worker, &MetadataWorker::parsed);

        for (int i = 0; i < paths.size(); ++i) {
            worker.enqueueParse(paths.at(i),
                                QByteArray("# N").append(QByteArray::number(i)).append('\n'),
                                100 + i,
                                10);
        }

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), paths.size(), 10000);

        QSet<QString> seen;
        for (int i = 0; i < spy.count(); ++i) {
            seen.insert(spy.at(i).at(0).toString());
        }
        QCOMPARE(seen.size(), paths.size());
    }

    // 4. Enqueue 5 paths and destroy the worker immediately. The destructor
    //    must block cleanly; no crash, no hang longer than the timeout.
    void testStopsCleanly()
    {
        const QStringList paths = generatePaths(5);
        LinkResolver resolver = makeResolver(paths);

        QElapsedTimer timer;
        timer.start();
        {
            MetadataWorker worker(resolver);
            for (int i = 0; i < paths.size(); ++i) {
                worker.enqueueParse(paths.at(i),
                                    QByteArray("# N\n"),
                                    100 + i,
                                    5);
            }
            // Destructor runs here.
        }
        const qint64 elapsedMs = timer.elapsed();
        QVERIFY2(elapsedMs < 2000,
                 qPrintable(QStringLiteral("destructor took %1ms").arg(elapsedMs)));
    }

    // 5. Before enqueue: pendingCount() == 0. After enqueuing 3, somewhere in
    //    [0,3] (race-tolerant). After parses complete: == 0.
    void testPendingCount()
    {
        const QStringList paths = generatePaths(3);
        LinkResolver resolver = makeResolver(paths);
        MetadataWorker worker(resolver);
        QSignalSpy spy(&worker, &MetadataWorker::parsed);

        QCOMPARE(worker.pendingCount(), 0);

        for (int i = 0; i < paths.size(); ++i) {
            worker.enqueueParse(paths.at(i),
                                QByteArray("# N\n"),
                                100 + i,
                                5);
        }

        const int mid = worker.pendingCount();
        QVERIFY2(mid >= 0 && mid <= paths.size(),
                 qPrintable(QStringLiteral("pendingCount out of range: %1").arg(mid)));

        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), paths.size(), 5000);

        QTRY_COMPARE_WITH_TIMEOUT(worker.pendingCount(), 0, 1000);
    }
};

QTEST_MAIN(TestMetadataWorker)
#include "tst_metadataworker.moc"
