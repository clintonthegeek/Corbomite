// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaOps.h"

#include <QtTest>

using namespace Corbomite::Bases;

class TestFormulaOps : public QObject
{
    Q_OBJECT
private slots:
    void add_insertsAndTracksOrder()
    {
        QHash<QString, Formula> m; QStringList o;
        QVERIFY(FormulaOps::add(m, o, QStringLiteral("ppu"), QStringLiteral("note.a / note.b")));
        QVERIFY(m.contains(QStringLiteral("ppu")));
        QCOMPARE(o, QStringList{QStringLiteral("ppu")});
        QCOMPARE(m.value(QStringLiteral("ppu")).source(), QStringLiteral("note.a / note.b"));
    }
    void add_rejectsDuplicateAndEmpty()
    {
        QHash<QString, Formula> m; QStringList o;
        QVERIFY(FormulaOps::add(m, o, QStringLiteral("x"), QStringLiteral("1")));
        QVERIFY(!FormulaOps::add(m, o, QStringLiteral("x"), QStringLiteral("2")));
        QVERIFY(!FormulaOps::add(m, o, QString(), QStringLiteral("2")));
        QCOMPARE(o.size(), 1);
    }
    void rename_preservesPosition()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        FormulaOps::add(m, o, QStringLiteral("b"), QStringLiteral("2"));
        QVERIFY(FormulaOps::rename(m, o, QStringLiteral("a"), QStringLiteral("z")));
        QVERIFY(!m.contains(QStringLiteral("a")));
        QVERIFY(m.contains(QStringLiteral("z")));
        QCOMPARE(o, (QStringList{QStringLiteral("z"), QStringLiteral("b")}));
    }
    void rename_rejectsCollision()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        FormulaOps::add(m, o, QStringLiteral("b"), QStringLiteral("2"));
        QVERIFY(!FormulaOps::rename(m, o, QStringLiteral("a"), QStringLiteral("b")));
    }
    void setSource_updatesExisting()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        QVERIFY(FormulaOps::setSource(m, QStringLiteral("a"), QStringLiteral("9")));
        QCOMPARE(m.value(QStringLiteral("a")).source(), QStringLiteral("9"));
        QVERIFY(!FormulaOps::setSource(m, QStringLiteral("missing"), QStringLiteral("9")));
    }
    void remove_dropsKeyAndOrder()
    {
        QHash<QString, Formula> m; QStringList o;
        FormulaOps::add(m, o, QStringLiteral("a"), QStringLiteral("1"));
        QVERIFY(FormulaOps::remove(m, o, QStringLiteral("a")));
        QVERIFY(m.isEmpty());
        QVERIFY(o.isEmpty());
        QVERIFY(!FormulaOps::remove(m, o, QStringLiteral("a")));
    }
};

QTEST_APPLESS_MAIN(TestFormulaOps)
#include "tst_formula_ops.moc"
