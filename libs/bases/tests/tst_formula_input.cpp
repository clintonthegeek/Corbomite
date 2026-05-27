// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaInput.h"

#include <QSignalSpy>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaInput : public QObject
{
    Q_OBJECT
private slots:
    void emptyIsNeutralValid()
    {
        FormulaInput in;
        QVERIFY(in.isExpressionValid());
    }
    void validExpression_marksValid()
    {
        FormulaInput in;
        QSignalSpy spy(&in, &FormulaInput::validityChanged);
        in.setText(QStringLiteral("note.a + 1"));
        QVERIFY(in.isExpressionValid());
    }
    void invalidExpression_marksInvalid()
    {
        FormulaInput in;
        QSignalSpy spy(&in, &FormulaInput::validityChanged);
        in.setText(QStringLiteral("((1"));    // unbalanced paren — definitely invalid
        QVERIFY(!in.isExpressionValid());
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toBool(), false);
    }
};

QTEST_MAIN(TestFormulaInput)
#include "tst_formula_input.moc"
