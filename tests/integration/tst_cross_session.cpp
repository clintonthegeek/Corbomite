// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cross-session scenario tests — Tier B of the test enrichment cycle.
// Each test method drives a sequence like "open → mutate → close → reopen"
// against the storage + models stack (no widgets). Targets seams × lifecycle
// cells from docs/test-coverage-matrix.md.
//
// Bugs discovered during a cycle are wrapped with QEXPECT_FAIL and a
// BUG-YYYYMMDD-NNN reference into docs/test-coverage-bug-hunt.md.

#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "corbomite/storage/CachedMetadataStore.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"

using namespace Corbomite;

class TestCrossSession : public QObject {
    Q_OBJECT

private:
    static void writeFile(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    // Wait for a QSignalSpy to receive at least `target` emissions, polling
    // the event loop. Returns true on success, false on timeout.
    static bool waitForSpy(QSignalSpy &spy, int target, int timeoutMs = 3000)
    {
        QElapsedTimer timer;
        timer.start();
        while (spy.count() < target && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            QTest::qWait(20);
        }
        return spy.count() >= target;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // No app-wide setup needed; each test owns its QTemporaryDir.
    }
};

QTEST_MAIN(TestCrossSession)
#include "tst_cross_session.moc"
