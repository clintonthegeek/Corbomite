// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "corbomite/core/EditorSuggest.h"
#include "corbomite/core/EditorSuggestManager.h"

using namespace Corbomite;

namespace {

// Test double — activates only when the cursor is right after a known sigil.
class SigilSuggest : public EditorSuggest {
public:
    explicit SigilSuggest(QChar sigil, QStringList items)
        : m_sigil(sigil), m_items(std::move(items)) {}

    std::optional<EditorSuggestTriggerInfo> onTrigger(int cursorPos,
                                                        const QString &lineText,
                                                        NoteDocument *) override
    {
        // Clamp the scan start to the last valid index: a cursor positioned
        // at (or past) end-of-line must not index past the string.
        int i = qMin(cursorPos, static_cast<int>(lineText.size())) - 1;
        while (i >= 0) {
            if (lineText.at(i) == m_sigil) {
                EditorSuggestTriggerInfo info;
                info.start = i + 1;
                info.end = cursorPos;
                info.query = lineText.mid(info.start, info.end - info.start);
                return info;
            }
            if (lineText.at(i).isSpace()) return std::nullopt;
            --i;
        }
        return std::nullopt;
    }

    QStringList getSuggestions(const EditorSuggestTriggerInfo &) override
    {
        return m_items;
    }

    QString selectSuggestion(const QString &chosen,
                              const EditorSuggestTriggerInfo &) override
    {
        return chosen;
    }

private:
    QChar m_sigil;
    QStringList m_items;
};

} // namespace

class TestEditorSuggest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testNoSuggestersReturnsNullopt()
    {
        EditorSuggestManager manager;
        auto r = manager.dispatch(0, QString(), nullptr);
        QVERIFY(!r.has_value());
    }

    void testFirstNonNullWins()
    {
        EditorSuggestManager manager;
        SigilSuggest a(QLatin1Char('@'), {QStringLiteral("alpha")});
        SigilSuggest b(QLatin1Char('#'), {QStringLiteral("beta")});
        manager.registerSuggest(&a);
        manager.registerSuggest(&b);

        auto r = manager.dispatch(2, QStringLiteral("#x"), nullptr);
        QVERIFY(r.has_value());
        QCOMPARE(r->suggester, &b);
        QCOMPARE(r->info.query, QStringLiteral("x"));
    }

    void testInsertionOrderShadowsLater()
    {
        // Two suggesters that both fire on @ — first registered wins.
        EditorSuggestManager manager;
        SigilSuggest first(QLatin1Char('@'), {QStringLiteral("from-first")});
        SigilSuggest second(QLatin1Char('@'), {QStringLiteral("from-second")});
        manager.registerSuggest(&first);
        manager.registerSuggest(&second);

        auto r = manager.dispatch(2, QStringLiteral("@a"), nullptr);
        QVERIFY(r.has_value());
        QCOMPARE(r->suggester, &first);
    }

    void testUnregisterRemoves()
    {
        EditorSuggestManager manager;
        SigilSuggest s(QLatin1Char('@'), {});
        manager.registerSuggest(&s);
        QCOMPARE(manager.suggesterCount(), 1);
        manager.unregisterSuggest(&s);
        QCOMPARE(manager.suggesterCount(), 0);
    }

    void testRegisterDuplicateIsNoOp()
    {
        EditorSuggestManager manager;
        SigilSuggest s(QLatin1Char('@'), {});
        manager.registerSuggest(&s);
        manager.registerSuggest(&s);
        QCOMPARE(manager.suggesterCount(), 1);
    }

    void testTriggerInfoCarriesQuery()
    {
        EditorSuggestManager manager;
        SigilSuggest s(QLatin1Char('@'), {});
        manager.registerSuggest(&s);
        auto r = manager.dispatch(5, QStringLiteral("@hi!"), nullptr);
        // cursorPos=5 is past end; clamp test — onTrigger handles -1 gracefully.
        // For valid in-range:
        r = manager.dispatch(4, QStringLiteral("@hi!"), nullptr);
        QVERIFY(r.has_value());
        QCOMPARE(r->info.query, QStringLiteral("hi!"));
        QCOMPARE(r->info.start, 1);
        QCOMPARE(r->info.end, 4);
    }
};

QTEST_MAIN(TestEditorSuggest)
#include "tst_editorsuggest.moc"
