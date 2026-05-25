// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/bases/ViewConfigOps.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesViewConfig.h"

using namespace Corbomite::Bases;

namespace {
PropertyId note(const char *n) { return PropertyId{PropertyKind::Note, QString::fromLatin1(n)}; }
QString names(const QVector<PropertyId> &v) {
    QStringList p; for (const auto &x : v) p << x.name; return p.join(QLatin1Char(','));
}
QString sortStr(const QVector<SortKey> &s) {
    QStringList p; for (const auto &k : s) p << k.property.name + QLatin1Char(':') + k.direction;
    return p.join(QLatin1Char(','));
}
std::unique_ptr<BasesQuery> queryWithViews(int n) {
    auto q = std::make_unique<BasesQuery>();
    for (int i = 0; i < n; ++i) {
        auto v = std::make_unique<BasesViewConfig>();
        v->type = QStringLiteral("table");
        v->name = QStringLiteral("view%1").arg(i);
        q->views.push_back(std::move(v));
    }
    return q;
}
QString viewNames(const BasesQuery &q) {
    QStringList p; for (const auto &v : q.views) p << v->name; return p.join(QLatin1Char(','));
}
}

class TestViewConfigOps : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testHideColumnRemoves() {
        QVector<PropertyId> order{note("a"), note("b"), note("c")};
        const QVector<PropertyId> all{note("a"), note("b"), note("c")};
        setColumnVisible(order, note("b"), false, all);
        QCOMPARE(names(order), QStringLiteral("a,c"));
    }
    void testShowColumnInsertsAtAllPropsPosition() {
        QVector<PropertyId> order{note("a"), note("c")};
        const QVector<PropertyId> all{note("a"), note("b"), note("c")};
        setColumnVisible(order, note("b"), true, all);
        QCOMPARE(names(order), QStringLiteral("a,b,c"));
    }
    void testShowAlreadyVisibleIsNoop() {
        QVector<PropertyId> order{note("a"), note("b")};
        const QVector<PropertyId> all{note("a"), note("b")};
        setColumnVisible(order, note("a"), true, all);
        QCOMPARE(names(order), QStringLiteral("a,b"));
    }
    void testMoveColumn() {
        QVector<PropertyId> order{note("a"), note("b"), note("c")};
        moveColumn(order, 0, 2);
        QCOMPARE(names(order), QStringLiteral("b,c,a"));
    }
    void testHideAll() {
        QVector<PropertyId> order{note("a"), note("b")};
        hideAllColumns(order);
        QCOMPARE(names(order), QString{});
    }
    void testAddSortKeyAppends() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        addSortKey(s, note("b"), QStringLiteral("DESC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC,b:DESC"));
    }
    void testAddSortKeyAbsentOnlyOnce() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        addSortKey(s, note("a"), QStringLiteral("DESC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC"));
    }
    void testSetSortDirectionExisting() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}};
        setSortDirection(s, note("a"), QStringLiteral("DESC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:DESC"));
    }
    void testSetSortDirectionInsertsWhenAbsent() {
        QVector<SortKey> s;
        setSortDirection(s, note("a"), QStringLiteral("ASC"));
        QCOMPARE(sortStr(s), QStringLiteral("a:ASC"));
    }
    void testRemoveSortKey() {
        QVector<SortKey> s{{note("a"), QStringLiteral("ASC")}, {note("b"), QStringLiteral("DESC")}};
        removeSortKey(s, note("a"));
        QCOMPARE(sortStr(s), QStringLiteral("b:DESC"));
    }
    void testSetGroupBy() {
        BasesViewConfig cfg;
        setGroupBy(cfg, note("status"), QStringLiteral("ASC"));
        QVERIFY(cfg.groupBy.has_value());
        QCOMPARE(cfg.groupBy->property.name, QStringLiteral("status"));
        QCOMPARE(cfg.groupBy->direction, QStringLiteral("ASC"));
    }
    void testClearGroupBy() {
        BasesViewConfig cfg;
        cfg.groupBy = GroupBy{note("x"), QStringLiteral("ASC")};
        setGroupBy(cfg, std::nullopt, QString{});
        QVERIFY(!cfg.groupBy.has_value());
    }
    void testDuplicateView() {
        auto q = queryWithViews(2);
        QVERIFY(duplicateView(*q, QStringLiteral("view0"), QStringLiteral("view0 copy")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1,view0 copy"));
    }
    void testDuplicateRefusesCollision() {
        auto q = queryWithViews(2);
        QVERIFY(!duplicateView(*q, QStringLiteral("view0"), QStringLiteral("view1")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1"));
    }
    void testDeleteView() {
        auto q = queryWithViews(2);
        QVERIFY(deleteView(*q, QStringLiteral("view0")));
        QCOMPARE(viewNames(*q), QStringLiteral("view1"));
    }
    void testDeleteRefusesLastView() {
        auto q = queryWithViews(1);
        QVERIFY(!deleteView(*q, QStringLiteral("view0")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0"));
    }
    void testRenameView() {
        auto q = queryWithViews(2);
        QVERIFY(renameView(*q, QStringLiteral("view0"), QStringLiteral("Active")));
        QCOMPARE(viewNames(*q), QStringLiteral("Active,view1"));
    }
    void testRenameRefusesCollision() {
        auto q = queryWithViews(2);
        QVERIFY(!renameView(*q, QStringLiteral("view0"), QStringLiteral("view1")));
        QCOMPARE(viewNames(*q), QStringLiteral("view0,view1"));
    }
    void testSetDefaultMovesToFront() {
        auto q = queryWithViews(3);
        QVERIFY(setDefaultView(*q, QStringLiteral("view2")));
        QCOMPARE(viewNames(*q), QStringLiteral("view2,view0,view1"));
    }
};

QTEST_APPLESS_MAIN(TestViewConfigOps)
#include "tst_view_config_ops.moc"
