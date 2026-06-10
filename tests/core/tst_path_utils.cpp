// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "corbomite/core/PathUtils.h"

class TestPathUtils : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void testObsidianUrlFor();
    void testObsidianUrlForWithSubpath();
    void testObsidianUrlForPercentEncodesSpace();
    void testCorbomiteUrlFor();
    void testCorbomiteUrlForWithSubpath();

    // vaultLocalDataDir invariants (guards the 0.7 defect fix)
    void vaultLocalDataDir_nonEmpty();
    void vaultLocalDataDir_notUnderVault();
    void vaultLocalDataDir_emptyForEmptyRoot();
};

void TestPathUtils::testObsidianUrlFor()
{
    const QString url = Corbomite::PathUtils::obsidianUrlFor(
        QStringLiteral("my-vault"),
        QStringLiteral("notes/foo.md"));
    QCOMPARE(url, QStringLiteral(
        "obsidian://open?vault=my-vault&file=notes%2Ffoo.md"));
}

void TestPathUtils::testObsidianUrlForWithSubpath()
{
    const QString url = Corbomite::PathUtils::obsidianUrlFor(
        QStringLiteral("my-vault"),
        QStringLiteral("notes/foo.md"),
        QStringLiteral("#Heading"));
    QCOMPARE(url, QStringLiteral(
        "obsidian://open?vault=my-vault&file=notes%2Ffoo.md%23Heading"));
}

void TestPathUtils::testObsidianUrlForPercentEncodesSpace()
{
    const QString url = Corbomite::PathUtils::obsidianUrlFor(
        QStringLiteral("my vault"),
        QStringLiteral("my note.md"));
    QCOMPARE(url, QStringLiteral(
        "obsidian://open?vault=my%20vault&file=my%20note.md"));
}

void TestPathUtils::testCorbomiteUrlFor()
{
    const QString url = Corbomite::PathUtils::corbomiteUrlFor(
        QStringLiteral("my-vault"),
        QStringLiteral("notes/foo.md"));
    QCOMPARE(url, QStringLiteral(
        "corbomite://open?vault=my-vault&file=notes%2Ffoo.md"));
}

void TestPathUtils::testCorbomiteUrlForWithSubpath()
{
    const QString url = Corbomite::PathUtils::corbomiteUrlFor(
        QStringLiteral("vault"),
        QStringLiteral("a.md"),
        QStringLiteral("#^block"));
    QCOMPARE(url, QStringLiteral(
        "corbomite://open?vault=vault&file=a.md%23%5Eblock"));
}

void TestPathUtils::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

// -----------------------------------------------------------------------
// vaultLocalDataDir invariants
//
// The 0.7 defect: when AppLocalDataLocation was empty the helper returned
// an empty string and MainWindow fell back to configPath (inside the vault),
// causing a destroy-rebuild loop.  These three tests guard the invariants
// that the fix must maintain:
//   1. Non-empty for a normal (non-empty) vault root.
//   2. The returned path is NOT under the vault root.
//   3. Empty string is returned only when vaultRoot itself is empty.
//
// The AppLocalDataLocation-empty branch is exercised by inspection:
// vaultLocalDataDir() now falls back to TempLocation/corbomite/..., so
// there is no code path that returns empty for a non-empty vaultRoot.
// -----------------------------------------------------------------------

void TestPathUtils::vaultLocalDataDir_nonEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dir = Corbomite::PathUtils::vaultLocalDataDir(tmp.path());
    QVERIFY2(!dir.isEmpty(),
             "vaultLocalDataDir must never return empty for a non-empty vaultRoot");
}

void TestPathUtils::vaultLocalDataDir_notUnderVault()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString vaultRoot = tmp.path();
    const QString dir = Corbomite::PathUtils::vaultLocalDataDir(vaultRoot);
    QVERIFY(!dir.isEmpty());
    QVERIFY2(!dir.startsWith(vaultRoot),
             qPrintable(QStringLiteral(
                 "vaultLocalDataDir '%1' must NOT be under vaultRoot '%2'")
                 .arg(dir, vaultRoot)));
}

void TestPathUtils::vaultLocalDataDir_emptyForEmptyRoot()
{
    const QString dir = Corbomite::PathUtils::vaultLocalDataDir(QString{});
    QVERIFY2(dir.isEmpty(),
             "vaultLocalDataDir must return empty string when vaultRoot is empty");
}

QTEST_MAIN(TestPathUtils)
#include "tst_path_utils.moc"
