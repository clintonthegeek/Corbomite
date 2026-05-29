// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderDialog.h"
#include "corbomite/bases/FilterSpec.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterBuilderDialog : public QObject
{
    Q_OBJECT
    static QPushButton *okButton(QDialog *d)
    {
        return d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    }
private slots:
    void accessors_returnEditedScopes()
    {
        FilterBuilderDialog d;
        FilterSpec g = FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) });
        FilterSpec pv = FilterSpec::group(Conj::Or, { FilterSpec::leaf(QStringLiteral("b == 2")) });
        d.setScopes(g, pv, {});
        QCOMPARE(d.globalSpec(), g);
        QCOMPARE(d.perViewSpec(), pv);
    }
    void okDisabled_whenEitherScopeInvalid()
    {
        FilterBuilderDialog d;
        d.setScopes(FilterSpec::group(Conj::And),                       // global: empty (valid)
                    FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("((1")) }),  // per-view invalid
                    {});
        QVERIFY(!okButton(&d)->isEnabled());
    }
    void okEnabled_whenBothValid()
    {
        FilterBuilderDialog d;
        d.setScopes(FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) }),
                    FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("b == 2")) }),
                    {});
        QVERIFY(okButton(&d)->isEnabled());
    }
};

QTEST_MAIN(TestFilterBuilderDialog)
#include "tst_filter_builder_dialog.moc"
