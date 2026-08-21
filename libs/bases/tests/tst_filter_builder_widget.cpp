// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterBuilderWidget.h"
#include "corbomite/bases/FilterSpec.h"

#include <QPushButton>
#include <QtTest>

using namespace Corbomite::Bases;

class TestFilterBuilderWidget : public QObject
{
    Q_OBJECT
private slots:
    void setSpec_thenSpec_roundTrips()
    {
        FilterBuilderWidget w;
        FilterSpec g = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::leaf(QStringLiteral("b == 2")),
        });
        w.setSpec(g, {}, {});
        QCOMPARE(w.spec(), g);
    }
    void addRuleButton_growsSpec()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("a == 1")) }), {}, {});
        auto *add = w.findChild<QPushButton *>(QStringLiteral("addRuleButton"));
        QVERIFY(add);
        add->click();
        QCOMPARE(w.spec().children.size(), 2);
        QCOMPARE(w.spec().children[1].kind, FilterSpec::Kind::Leaf);
        QCOMPARE(w.spec().children[1].expression, QString());
    }
    void invalidLeaf_makesGroupInvalid()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("((1")),   // unbalanced paren -> invalid
        }), {}, {});
        QVERIFY(!w.isValid());
    }
    void emptyLeaf_isValid()
    {
        FilterBuilderWidget w;
        w.setSpec(FilterSpec::group(Conj::And, { FilterSpec::leaf(QString()) }), {}, {});
        QVERIFY(w.isValid());
    }
    void nestedGroup_roundTrips()
    {
        FilterBuilderWidget w;
        FilterSpec g = FilterSpec::group(Conj::And, {
            FilterSpec::leaf(QStringLiteral("a == 1")),
            FilterSpec::group(Conj::Or, { FilterSpec::leaf(QStringLiteral("b == 2")) }),
        });
        w.setSpec(g, {}, {});
        QCOMPARE(w.spec(), g);
    }
};

QTEST_MAIN(TestFilterBuilderWidget)
#include "tst_filter_builder_widget.moc"
