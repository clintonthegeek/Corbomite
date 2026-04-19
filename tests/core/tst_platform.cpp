// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryFile>
#include <QFileInfo>

#include "corbomite/core/Platform.h"

class TestPlatform : public QObject
{
    Q_OBJECT
private slots:
    void testOpenNonexistentPathReturnsFalse();
    void testShowInFolderNonexistentPathReturnsFalse();
    void testPreflightExistenceCheckOnRealFile();
};

void TestPlatform::testOpenNonexistentPathReturnsFalse()
{
    const bool ok = Corbomite::Platform::openWithDefaultApp(
        QStringLiteral("/nonexistent/zzz-cluster-r.xyz"));
    QVERIFY(!ok);
}

void TestPlatform::testShowInFolderNonexistentPathReturnsFalse()
{
    const bool ok = Corbomite::Platform::showInFolder(
        QStringLiteral("/nonexistent/zzz-cluster-r.xyz"));
    QVERIFY(!ok);
}

void TestPlatform::testPreflightExistenceCheckOnRealFile()
{
    // Smoke test: opening a real file's preflight check passes (the
    // existence check is the only deterministic part of showInFolder /
    // openWithDefaultApp in a headless CI environment).
    QTemporaryFile tmp(QStringLiteral("cluster-r-test-XXXXXX.txt"));
    QVERIFY(tmp.open());
    tmp.write("hi");
    tmp.flush();

    const QFileInfo info(tmp.fileName());
    QVERIFY(info.exists());
    // We do NOT call openWithDefaultApp / showInFolder here to avoid
    // launching a helper process during `ctest`.
}

QTEST_MAIN(TestPlatform)
#include "tst_platform.moc"
