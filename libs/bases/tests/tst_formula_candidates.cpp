// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaCandidates.h"
#include "corbomite/bases/FunctionRegistry.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaCandidates : public QObject
{
    Q_OBJECT
private slots:
    void build_namedMode_hasRootsPropsAndFuncs()
    {
        QVector<PropertyId> props{
            {PropertyKind::Note, QStringLiteral("status")},
            {PropertyKind::Formula, QStringLiteral("ppu")},
        };
        QStringList c = FormulaCandidates::build(props, &FunctionRegistry::global(),
                                                 FormulaCandidates::Mode::NamedFormula);
        QVERIFY(c.contains(QStringLiteral("note")));
        QVERIFY(c.contains(QStringLiteral("file")));
        QVERIFY(c.contains(QStringLiteral("formula")));
        QVERIFY(c.contains(QStringLiteral("status")));
        QVERIFY(c.contains(QStringLiteral("formula.ppu")));
        QVERIFY(c.contains(QStringLiteral("now")));     // a global function
        // Note: "values" is a per-type method in the registry so it appears
        // via allNames() regardless of mode. The mode flag controls only the
        // extra binding for the `values` summary-context variable; there is no
        // duplication issue since the set deduplicates.
    }
    void build_namedMode_noValuesBindingWithoutRegistry()
    {
        // Without a registry the only source of "values" would be the mode
        // logic. In NamedFormula mode it should not be injected.
        QStringList c = FormulaCandidates::build({}, nullptr,
                                                 FormulaCandidates::Mode::NamedFormula);
        QVERIFY(!c.contains(QStringLiteral("values")));
    }
    void build_summaryMode_addsValues()
    {
        QStringList c = FormulaCandidates::build({}, &FunctionRegistry::global(),
                                                 FormulaCandidates::Mode::SummaryFormula);
        QVERIFY(c.contains(QStringLiteral("values")));
    }
    void tokenAt_scansLeftOverIdentChars()
    {
        auto t = FormulaCandidates::tokenAt(QStringLiteral("note.sta"), 8);
        QCOMPARE(t.start, 5);
        QCOMPARE(t.token, QStringLiteral("sta"));
    }
    void tokenAt_emptyAfterNonIdent()
    {
        auto t = FormulaCandidates::tokenAt(QStringLiteral("a + "), 4);
        QCOMPARE(t.token, QString());
        QCOMPARE(t.start, 4);
    }
};

QTEST_APPLESS_MAIN(TestFormulaCandidates)
#include "tst_formula_candidates.moc"
