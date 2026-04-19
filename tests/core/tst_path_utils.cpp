// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/core/PathUtils.h"

class TestPathUtils : public QObject
{
    Q_OBJECT
private slots:
    void testObsidianUrlFor();
    void testObsidianUrlForWithSubpath();
    void testObsidianUrlForPercentEncodesSpace();
    void testCorbomiteUrlFor();
    void testCorbomiteUrlForWithSubpath();
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

QTEST_MAIN(TestPathUtils)
#include "tst_path_utils.moc"
