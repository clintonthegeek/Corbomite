// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QFont>
#include <QFontMetrics>
#include "corbomite/bases/CellHitTest.h"
#include "corbomite/bases/Values.h"

using namespace Corbomite::Bases;

class TestCellHitTest : public QObject
{
    Q_OBJECT
    QFont m_font;
private Q_SLOTS:
    void checkboxGlyphHitVsWhitespace() {
        const QRect cell(0, 0, 200, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<BooleanValue>(true);
        const QRect glyph = checkboxGlyphRect(cell);
        // a point inside the centered glyph -> Checkbox
        CellHit hit = hitTestCell(QStringLiteral("Boolean"), v, cell, glyph.center(), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Checkbox));
        // a point far in the left margin (outside the centered glyph) -> Whitespace
        CellHit ws = hitTestCell(QStringLiteral("Boolean"), v, cell, QPoint(2, 12), fm);
        QCOMPARE(int(ws.kind), int(CellHit::Whitespace));
    }
    void linkTextHitVsTrailingWhitespace() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<LinkValue>(QStringLiteral("Ridley Scott"));
        // a point near the left (over the text) -> Link with the target as payload
        CellHit hit = hitTestCell(QStringLiteral("Link"), v, cell, QPoint(10, 12), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Link));
        QCOMPARE(hit.payload, QStringLiteral("Ridley Scott"));
        // a point far to the right (past the text) -> Whitespace
        CellHit ws = hitTestCell(QStringLiteral("Link"), v, cell, QPoint(390, 12), fm);
        QCOMPARE(int(ws.kind), int(CellHit::Whitespace));
    }
    void urlHit() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        auto v = std::make_shared<UrlValue>(QStringLiteral("https://example.com"));
        CellHit hit = hitTestCell(QStringLiteral("URL"), v, cell, QPoint(10, 12), fm);
        QCOMPARE(int(hit.kind), int(CellHit::Url));
        QCOMPARE(hit.payload, QStringLiteral("https://example.com"));
    }
    void tagChipIndexResolution() {
        const QRect cell(0, 0, 400, 24);
        QFontMetrics fm(m_font);
        QVector<ValuePtr> tags{ std::make_shared<TagValue>(QStringLiteral("sci-fi")),
                                std::make_shared<TagValue>(QStringLiteral("noir")),
                                std::make_shared<TagValue>(QStringLiteral("dystopia")) };
        auto list = std::make_shared<ListValue>(tags);
        const QVector<QRect> chips = tagChipRects(list, cell, fm);
        QCOMPARE(chips.size(), 3);
        // click inside chip 0 and chip 2 resolve to the right tag
        CellHit h0 = hitTestCell(QStringLiteral("List"), list, cell, chips[0].center(), fm);
        QCOMPARE(int(h0.kind), int(CellHit::Tag));
        QCOMPARE(h0.tagIndex, 0);
        QCOMPARE(h0.payload, QStringLiteral("sci-fi"));
        CellHit h2 = hitTestCell(QStringLiteral("List"), list, cell, chips[2].center(), fm);
        QCOMPARE(h2.tagIndex, 2);
        QCOMPARE(h2.payload, QStringLiteral("dystopia"));
    }
    void plainAndNullCellsAreWhitespace() {
        const QRect cell(0, 0, 200, 24);
        QFontMetrics fm(m_font);
        CellHit s = hitTestCell(QStringLiteral("String"),
            std::make_shared<StringValue>(QStringLiteral("hello")), cell, QPoint(5, 12), fm);
        QCOMPARE(int(s.kind), int(CellHit::Whitespace));
        CellHit n = hitTestCell(QStringLiteral("Number"), nullptr, cell, QPoint(5, 12), fm);
        QCOMPARE(int(n.kind), int(CellHit::Whitespace));
    }
};

QTEST_MAIN(TestCellHitTest)
#include "tst_bases_cell_hittest.moc"
