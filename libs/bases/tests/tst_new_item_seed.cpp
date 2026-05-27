// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterTree.h"
#include "corbomite/bases/Formula.h"
#include "corbomite/bases/NewItemSeed.h"

#include <QtTest>

using namespace Corbomite::Bases;

namespace {
FilterPtr rule(const QString &src) { return std::make_shared<FilterRule>(Formula(src)); }
FilterPtr conj(Conj c, QVector<FilterPtr> kids) { return std::make_shared<FilterConjunction>(c, std::move(kids)); }

bool hasPair(const NewItemSeed::SeedList &s, const QString &k, const QString &v)
{
    for (const auto &p : s) if (p.first == k && p.second == v) return true;
    return false;
}
bool hasKey(const NewItemSeed::SeedList &s, const QString &k)
{
    for (const auto &p : s) if (p.first == k) return true;
    return false;
}
}  // namespace

class TestNewItemSeed : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void noFilterNoTemplate();
    void singleEquality();
    void andChain();
    void orSkipped();
    void negationSkipped();
    void inequalitySkipped();
    void filePropertySkipped();
    void templateVerbatim();
    void equalityOverridesTemplate();
};

void TestNewItemSeed::noFilterNoTemplate()
{
    QVERIFY(NewItemSeed::compute(nullptr, {}).isEmpty());
}

void TestNewItemSeed::singleEquality()
{
    auto s = NewItemSeed::compute(rule(QStringLiteral("status == \"active\"")), {});
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
}

void TestNewItemSeed::andChain()
{
    auto f = conj(Conj::And, {rule(QStringLiteral("status == \"active\"")),
                              rule(QStringLiteral("kind == \"note\""))});
    auto s = NewItemSeed::compute(f, {});
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
    QVERIFY(hasPair(s, QStringLiteral("kind"), QStringLiteral("note")));
}

void TestNewItemSeed::orSkipped()
{
    auto f = conj(Conj::Or, {rule(QStringLiteral("status == \"active\"")),
                             rule(QStringLiteral("status == \"done\""))});
    QVERIFY(NewItemSeed::compute(f, {}).isEmpty());
}

void TestNewItemSeed::negationSkipped()
{
    auto f = conj(Conj::Not, {rule(QStringLiteral("status == \"active\""))});
    QVERIFY(NewItemSeed::compute(f, {}).isEmpty());
}

void TestNewItemSeed::inequalitySkipped()
{
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("count > 3")), {}), QStringLiteral("count")));
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("status != \"x\"")), {}), QStringLiteral("status")));
}

void TestNewItemSeed::filePropertySkipped()
{
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("file.name == \"x\"")), {}), QStringLiteral("file.name")));
    QVERIFY(!hasKey(NewItemSeed::compute(rule(QStringLiteral("file.name == \"x\"")), {}), QStringLiteral("name")));
}

void TestNewItemSeed::templateVerbatim()
{
    NewItemSeed::SeedList tmpl{{QStringLiteral("tags"), QStringLiteral("inbox")}};
    auto s = NewItemSeed::compute(nullptr, tmpl);
    QVERIFY(hasPair(s, QStringLiteral("tags"), QStringLiteral("inbox")));
}

void TestNewItemSeed::equalityOverridesTemplate()
{
    NewItemSeed::SeedList tmpl{{QStringLiteral("status"), QStringLiteral("draft")}};
    auto s = NewItemSeed::compute(rule(QStringLiteral("status == \"active\"")), tmpl);
    QVERIFY(hasPair(s, QStringLiteral("status"), QStringLiteral("active")));
    int n = 0;
    for (const auto &p : s) if (p.first == QStringLiteral("status")) ++n;
    QCOMPARE(n, 1);
}

QTEST_MAIN(TestNewItemSeed)
#include "tst_new_item_seed.moc"
