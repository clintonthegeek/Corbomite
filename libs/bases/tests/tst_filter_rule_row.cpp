// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterRuleRow.h"

#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QToolButton>
#include <QtTest>

using namespace Corbomite::Bases;

namespace {

QVector<FilterPropertyInfo> testProps()
{
    return {
        { PropertyId{PropertyKind::Note, QStringLiteral("Practical")}, QStringLiteral("Practical"), QStringLiteral("String") },
        { PropertyId{PropertyKind::Note, QStringLiteral("Age")}, QStringLiteral("Age"), QStringLiteral("Number") },
        { PropertyId{PropertyKind::Note, QStringLiteral("Done")}, QStringLiteral("Done"), QStringLiteral("Boolean") },
        { PropertyId{PropertyKind::Note, QStringLiteral("Due")}, QStringLiteral("Due"), QStringLiteral("Date") },
        { PropertyId{PropertyKind::Note, QStringLiteral("My Key")}, QStringLiteral("My Key"), QStringLiteral("String") },
    };
}

}  // namespace

class TestFilterRuleRow : public QObject
{
    Q_OBJECT
private slots:
    void defaultRow_isNotEmptyOnFirstProperty()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        QCOMPARE(row.expression(), QStringLiteral("!note.Practical.isEmpty()"));
        QVERIFY(row.isExpressionValid());
    }

    void textEquals_synthesizesQuotedComparison()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *opCombo = row.findChild<QComboBox *>(QStringLiteral("operatorCombo"));
        QVERIFY(opCombo);
        opCombo->setCurrentIndex(opCombo->findText(QStringLiteral("is")));
        auto *value = row.findChild<QLineEdit *>(QStringLiteral("valueText"));
        QVERIFY(value);
        value->setText(QStringLiteral("hello \"world\""));
        QCOMPARE(row.expression(), QStringLiteral("note.Practical == \"hello \\\"world\\\"\""));
    }

    void numberGreaterThan_synthesizesUnquotedNumber()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        propCombo->setCurrentIndex(1);  // Age (Number)
        auto *opCombo = row.findChild<QComboBox *>(QStringLiteral("operatorCombo"));
        opCombo->setCurrentIndex(opCombo->findText(QStringLiteral("is greater than")));
        auto *value = row.findChild<QLineEdit *>(QStringLiteral("valueText"));
        value->setText(QStringLiteral("5"));
        QCOMPARE(row.expression(), QStringLiteral("note.Age > 5"));
        QVERIFY(row.isExpressionValid());
    }

    void numberWithNonNumericValue_isInvalid()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        propCombo->setCurrentIndex(1);  // Age (Number)
        auto *opCombo = row.findChild<QComboBox *>(QStringLiteral("operatorCombo"));
        opCombo->setCurrentIndex(opCombo->findText(QStringLiteral("is greater than")));
        auto *value = row.findChild<QLineEdit *>(QStringLiteral("valueText"));
        value->setText(QStringLiteral("abc"));
        QVERIFY(!row.isExpressionValid());
    }

    void booleanIsTrue_needsNoValueWidget()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        propCombo->setCurrentIndex(2);  // Done (Boolean)
        auto *opCombo = row.findChild<QComboBox *>(QStringLiteral("operatorCombo"));
        opCombo->setCurrentIndex(opCombo->findText(QStringLiteral("is true")));
        QCOMPARE(row.expression(), QStringLiteral("note.Done == true"));
        QVERIFY(row.isExpressionValid());
    }

    void dateBefore_synthesizesDateLiteral()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        propCombo->setCurrentIndex(3);  // Due (Date)
        auto *opCombo = row.findChild<QComboBox *>(QStringLiteral("operatorCombo"));
        opCombo->setCurrentIndex(opCombo->findText(QStringLiteral("is before")));
        auto *dateValue = row.findChild<QDateEdit *>(QStringLiteral("valueDate"));
        QVERIFY(dateValue);
        dateValue->setDate(QDate(2025, 1, 1));
        QCOMPARE(row.expression(), QStringLiteral("note.Due < date(\"2025-01-01\")"));
    }

    void nonIdentifierKey_usesBracketAccess()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        propCombo->setCurrentIndex(4);  // "My Key"
        QCOMPARE(row.expression(), QStringLiteral("!note[\"My Key\"].isEmpty()"));
    }

    void setExpression_recognisedTemplate_entersSimpleMode()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        row.setExpression(QStringLiteral("note.Age > 5"));
        auto *toggle = row.findChild<QToolButton *>(QStringLiteral("advancedModeToggle"));
        QVERIFY(toggle);
        QVERIFY(!toggle->isChecked());  // simple mode
        auto *propCombo = row.findChild<QComboBox *>(QStringLiteral("propertyCombo"));
        QCOMPARE(propCombo->currentIndex(), 1);
        auto *value = row.findChild<QLineEdit *>(QStringLiteral("valueText"));
        QCOMPARE(value->text(), QStringLiteral("5"));
        QCOMPARE(row.expression(), QStringLiteral("note.Age > 5"));
    }

    void setExpression_unrecognisedFormula_fallsBackToAdvancedLosslessly()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        const QString compound = QStringLiteral("note.Age > 5 && note.Practical.contains(\"x\")");
        row.setExpression(compound);
        auto *toggle = row.findChild<QToolButton *>(QStringLiteral("advancedModeToggle"));
        QVERIFY(toggle->isChecked());  // advanced mode
        QCOMPARE(row.expression(), compound);
    }

    void emptyExpression_isNeutralValid()
    {
        FilterRuleRow row;
        row.setProperties(testProps());
        row.setExpression(QString());
        QVERIFY(row.isExpressionValid());
    }

    void noProperties_forcesAdvancedMode()
    {
        FilterRuleRow row;
        row.setProperties({});
        auto *toggle = row.findChild<QToolButton *>(QStringLiteral("advancedModeToggle"));
        QVERIFY(toggle->isChecked());
        QVERIFY(!toggle->isEnabled());
        row.setExpression(QStringLiteral("note.anything == 1"));
        QCOMPARE(row.expression(), QStringLiteral("note.anything == 1"));
    }
};

QTEST_MAIN(TestFilterRuleRow)
#include "tst_filter_rule_row.moc"
