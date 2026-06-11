// SPDX-License-Identifier: GPL-3.0-or-later
#include "TagSuggest.h"
#include <QObject>
#include <QTest>

using namespace Corbomite;

class TagSuggestTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void trigger_afterHashAtLineStart()
    {
        TagSuggest s(nullptr);
        auto info = s.onTrigger(3, QStringLiteral("#pr"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->start, 1);
        QCOMPARE(info->query, QStringLiteral("pr"));
    }
    void trigger_afterHashMidLineNeedsSpace()
    {
        TagSuggest s(nullptr);
        QVERIFY(s.onTrigger(5, QStringLiteral("x #pr"), nullptr).has_value());
        QVERIFY(!s.onTrigger(5, QStringLiteral("x#prq"), nullptr).has_value());
    }
    void nullIndex_returnsEmptyUniverse()
    {
        TagSuggest s(nullptr);
        EditorSuggestTriggerInfo ctx; ctx.start = 1; ctx.end = 3;
        ctx.query = QStringLiteral("pr");
        const auto set = s.getSuggestions(ctx);
        QVERIFY(set.items.isEmpty());
        QCOMPARE(set.filter, QStringLiteral("pr"));
    }
};

QTEST_MAIN(TagSuggestTest)
#include "tst_tag_suggest.moc"
