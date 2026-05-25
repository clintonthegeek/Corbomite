// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/SortCycle.h"
#include "corbomite/bases/BasesViewConfig.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }
QString dirs(const QVector<SortKey> &s) {  // compact "a:ASC,b:DESC" for asserts
    QStringList parts;
    for (const auto &k : s) parts << k.property.name + QLatin1Char(':') + k.direction;
    return parts.join(QLatin1Char(','));
}
}

class TestSortCycle : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPlainClickEmptySetsAsc()
    {
        QVector<SortKey> s;
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QStringLiteral("a:ASC"));
    }
    void testPlainClickPrimaryCyclesAscDescRemove()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QStringLiteral("a:DESC"));
        cycleHeaderSort(s, note("a"), false);
        QCOMPARE(dirs(s), QString{});                 // removed
    }
    void testPlainClickDifferentColumnReplaces()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("DESC")}};
        cycleHeaderSort(s, note("b"), false);
        QCOMPARE(dirs(s), QStringLiteral("b:ASC"));    // replaced, not appended
    }
    void testShiftClickAppendsSecondary()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("b"), true);
        QCOMPARE(dirs(s), QStringLiteral("a:ASC,b:ASC"));
    }
    void testShiftClickCyclesExistingKeyInPlace()
    {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}, {note("b"), QStringLiteral("ASC")}};
        cycleHeaderSort(s, note("a"), true);
        QCOMPARE(dirs(s), QStringLiteral("a:DESC,b:ASC")); // a flips, b kept, order kept
        cycleHeaderSort(s, note("a"), true);
        QCOMPARE(dirs(s), QStringLiteral("b:ASC"));         // a removed, b kept
    }
};

QTEST_APPLESS_MAIN(TestSortCycle)
#include "tst_sortcycle.moc"
