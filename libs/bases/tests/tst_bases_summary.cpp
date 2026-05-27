// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/FunctionRegistry.h"
#include "corbomite/bases/SummaryContext.h"
#include "corbomite/bases/Values.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestBasesSummary : public QObject
{
    Q_OBJECT
private slots:
    void customSummaryFormula_evaluatesAgainstValues()
    {
        QVector<ValuePtr> nums{ std::make_shared<NumberValue>(1),
                                std::make_shared<NumberValue>(2),
                                std::make_shared<NumberValue>(3) };
        auto list = std::make_shared<ListValue>(nums);
        SummaryContext ctx(list);
        Formula f(QStringLiteral("values.sum() * 2"));
        QVERIFY(f.isValid());
        ValuePtr out = f.getValue(ctx, &FunctionRegistry::global());
        auto num = std::dynamic_pointer_cast<NumberValue>(out);
        QVERIFY(num);
        QCOMPARE(num->data(), 12.0);
    }
};

QTEST_MAIN(TestBasesSummary)
#include "tst_bases_summary.moc"
