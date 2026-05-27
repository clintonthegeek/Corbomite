// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaEditDialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaEditDialog : public QObject
{
    Q_OBJECT
    static QPushButton *okButton(QDialog *d)
    {
        auto *box = d->findChild<QDialogButtonBox *>();
        return box->button(QDialogButtonBox::Ok);
    }
private slots:
    void okDisabledUntilNameAndValidExpr()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        QVERIFY(!okButton(&d)->isEnabled());            // empty name + empty expr
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("((1"));  // invalid expr
        QVERIFY(!okButton(&d)->isEnabled());
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("note.a + 1"));
        QVERIFY(okButton(&d)->isEnabled());
    }
    void okDisabledOnNameCollision()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        d.setExistingNames({QStringLiteral("taken")});
        // Use a known-valid expression so OK can only be disabled by the name
        // collision, not by an invalid expression.
        d.setInitial(QStringLiteral("taken"), QStringLiteral("note.a + 1"));
        QVERIFY(!okButton(&d)->isEnabled());
    }
    void accessorsReturnEnteredValues()
    {
        FormulaEditDialog d(FormulaCandidates::Mode::NamedFormula);
        d.setInitial(QStringLiteral("ppu"), QStringLiteral("note.a / note.b"));
        QCOMPARE(d.formulaName(), QStringLiteral("ppu"));
        QCOMPARE(d.formulaSource(), QStringLiteral("note.a / note.b"));
    }
};

QTEST_MAIN(TestFormulaEditDialog)
#include "tst_formula_edit_dialog.moc"
