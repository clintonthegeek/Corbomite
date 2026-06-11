// SPDX-License-Identifier: GPL-3.0-or-later
// LineResolve — contract-v2 flat visual line ⟷ (block, offsets, lineText).
// Spec §5. The line space matches MarkdownView::cursorPosition(): each
// block contributes 1 + count('\n') lines.
#include "LineResolve.h"
#include <markoff/core/MarkoffDocument.h>
#include <QObject>
#include <QTest>

using namespace Corbomite;

class LineResolveTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void singleLineBlocks()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("Alpha\n\nBeta\n\nGamma\n"));
        auto r1 = LineResolve::resolveLine(&doc, 1);
        QVERIFY(r1.has_value());
        QCOMPARE(r1->blockRow, 0);
        QCOMPARE(r1->lineStartCharInBlock, 0);
        QCOMPARE(r1->lineText, QStringLiteral("Alpha"));
        auto r3 = LineResolve::resolveLine(&doc, 3);
        QVERIFY(r3.has_value());
        QCOMPARE(r3->blockRow, 2);
        QCOMPARE(r3->lineText, QStringLiteral("Gamma"));
    }

    void multiLineBlock()
    {
        // A code block keeps internal newlines in its buffer.
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("Intro\n\n```\nline a\nline b\n```\n"));
        bool found = false;
        for (int line = 1; line <= 8; ++line) {
            auto r = LineResolve::resolveLine(&doc, line);
            if (!r) break;
            if (r->lineText == QStringLiteral("line b")) {
                QVERIFY(r->lineStartCharInBlock > 0);
                found = true;
            }
        }
        QVERIFY2(found, "multi-line block's inner line not resolvable");
    }

    void outOfRangeAndNull()
    {
        Markoff::MarkoffDocument doc(1);
        doc.loadFromMarkdown(QByteArrayLiteral("One\n"));
        QVERIFY(!LineResolve::resolveLine(&doc, 0).has_value());
        QVERIFY(!LineResolve::resolveLine(&doc, 99).has_value());
        QVERIFY(!LineResolve::resolveLine(nullptr, 1).has_value());
    }

    void byteOffsetMultibyte()
    {
        // "héllo 日本" — é is 2 UTF-8 bytes, 日/本 are 3 each.
        // Chars (UTF-16): 0:h 1:é 2:l 3:l 4:o 5:space 6:日 7:本.
        const QString s = QString::fromUtf8("h\xC3\xA9llo \xE6\x97\xA5\xE6\x9C\xAC");
        QCOMPARE(LineResolve::byteOffsetForChar(s, 0), 0u);
        QCOMPARE(LineResolve::byteOffsetForChar(s, 1), 1u);   // before é
        QCOMPARE(LineResolve::byteOffsetForChar(s, 2), 3u);   // after é
        QCOMPARE(LineResolve::byteOffsetForChar(s, 6), 7u);   // after the space, before 日
        QCOMPARE(LineResolve::byteOffsetForChar(s, 7), 10u);  // after 日, before 本
        QCOMPARE(LineResolve::byteOffsetForChar(s, 8), 13u);  // after 本 (end)
        QCOMPARE(LineResolve::byteOffsetForChar(s, 99), uint32_t(s.toUtf8().size()));  // clamps
    }
};

QTEST_MAIN(LineResolveTest)
#include "tst_line_resolve.moc"
