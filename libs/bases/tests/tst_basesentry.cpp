// SPDX-License-Identifier: GPL-3.0-or-later
//
// BasesEntry + BasesQueryResult tests — exercised with null Vault/Cache
// for unit scope. Full end-to-end fixtures live in the integration
// tests.
#include <QTest>

#include <optional>

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/BasesQueryResult.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestBasesEntry : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testFormulaCycleDetection()
    {
        BasesQuery q;
        q.formulas.insert(QStringLiteral("a"), Formula(QStringLiteral("formula.b")));
        q.formulas.insert(QStringLiteral("b"), Formula(QStringLiteral("formula.a")));

        BasesEntry e(nullptr, nullptr, nullptr, nullptr, q);
        auto v = e.formulaValue(QStringLiteral("a"));
        // First eval starts a->b->a; inner returns FormulaError; outer
        // memoises whatever `a` evaluated to. Either shows up in the
        // result chain with Error somewhere. Assert the outer result
        // is a valid Value (no crash) and that a second eval returns
        // the same memoised pointer (cache hit).
        QVERIFY(v != nullptr);
        auto v2 = e.formulaValue(QStringLiteral("a"));
        QCOMPARE(v.get(), v2.get());
    }

    void testFormulaCachedAcrossCalls()
    {
        BasesQuery q;
        q.formulas.insert(QStringLiteral("x"), Formula(QStringLiteral("42")));
        BasesEntry e(nullptr, nullptr, nullptr, nullptr, q);
        auto a = e.formulaValue(QStringLiteral("x"));
        auto b = e.formulaValue(QStringLiteral("x"));
        QCOMPARE(a.get(), b.get());
    }

    void testBasesQueryResultSortAsc()
    {
        BasesQuery q;
        BasesViewConfig cfg;
        cfg.sort.push_back({PropertyId{PropertyKind::Note, QStringLiteral("v")},
                            QStringLiteral("ASC")});

        // Construct three entries with null Vault/Cache/File — they will
        // return NullValue from getValue for most PropertyIds. Without a
        // fixture the sort is a no-op — we just verify it doesn't crash
        // and the row count is preserved.
        QVector<std::shared_ptr<BasesEntry>> entries;
        for (int i = 0; i < 3; ++i)
            entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));

        BasesQueryResult result(cfg, entries);
        QCOMPARE(result.rows().size(), 3);
    }

    void testBasesQueryResultLimit()
    {
        BasesQuery q;
        BasesViewConfig cfg;
        cfg.limit = 2;
        QVector<std::shared_ptr<BasesEntry>> entries;
        for (int i = 0; i < 5; ++i)
            entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
        BasesQueryResult result(cfg, entries);
        QCOMPARE(result.rows().size(), 2);
    }

    void testBasesQueryResultSingleGroupWhenNoGroupBy()
    {
        BasesQuery q;
        BasesViewConfig cfg;
        QVector<std::shared_ptr<BasesEntry>> entries;
        for (int i = 0; i < 3; ++i)
            entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
        BasesQueryResult result(cfg, entries);
        QCOMPARE(result.groups().size(), 1);
        QCOMPARE(result.groups().front().entries.size(), 3);
    }

    void testSummaryValueCount()
    {
        BasesQuery q;
        BasesViewConfig cfg;
        QVector<std::shared_ptr<BasesEntry>> entries;
        for (int i = 0; i < 4; ++i)
            entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
        BasesQueryResult result(cfg, entries);
        auto v = result.summaryValue(0,
                                     PropertyId{PropertyKind::Note, QStringLiteral("x")},
                                     QStringLiteral("count"));
        QCOMPARE(std::static_pointer_cast<NumberValue>(v)->data(), 4.0);
    }

    void testSummaryValueBuiltInNamesResolve()
    {
        // Every built-in summary name must resolve to a value rather than
        // Null. The entries carry no Vault, so every property reads as
        // Null: Empty counts them all, Filled none, and the rest fall
        // back to Null because there is nothing to aggregate. The point
        // here is the *wiring* — a name that reached no branch would also
        // be Null, so the count names are the ones that discriminate.
        BasesQuery q;
        BasesViewConfig cfg;
        QVector<std::shared_ptr<BasesEntry>> entries;
        for (int i = 0; i < 3; ++i)
            entries.push_back(std::make_shared<BasesEntry>(nullptr, nullptr, nullptr, nullptr, q));
        BasesQueryResult result(cfg, entries);
        const PropertyId prop{PropertyKind::Note, QStringLiteral("x")};

        // A helper that reports cleanly rather than dereferencing a null
        // cast: a name that reached no branch answers Null, and the test
        // should say so rather than crash.
        const auto number = [&result, &prop](const char *name) -> std::optional<double> {
            auto v = result.summaryValue(0, prop, QString::fromLatin1(name));
            auto n = std::dynamic_pointer_cast<NumberValue>(v);
            if (!n) return std::nullopt;
            return n->data();
        };

        QCOMPARE(number("empty").value_or(-1.0), 3.0);
        QCOMPARE(number("filled").value_or(-1.0), 0.0);
        QCOMPARE(number("checked").value_or(-1.0), 0.0);
        QCOMPARE(number("unchecked").value_or(-1.0), 0.0);

        // The date aggregates have nothing to work with here, but they
        // must still answer as a value rather than a crash.
        QVERIFY(result.summaryValue(0, prop, QStringLiteral("earliest")));
        QVERIFY(result.summaryValue(0, prop, QStringLiteral("latest")));
        QVERIFY(result.summaryValue(0, prop, QStringLiteral("range")));

        // An unknown name is still Null.
        QCOMPARE(result.summaryValue(0, prop, QStringLiteral("nonesuch"))->type(),
                 QStringLiteral("Null"));
    }
};

QTEST_APPLESS_MAIN(TestBasesEntry)
#include "tst_basesentry.moc"
