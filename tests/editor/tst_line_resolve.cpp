// SPDX-License-Identifier: GPL-3.0-or-later
// LineResolve — contract-v2 flat visual line ⟷ (block, offsets, lineText).
// Spec §5. The line space matches MarkdownFileView::cursorPosition(): each
// block contributes 1 + count('\n') lines.
#include "LineResolve.h"
#include <markoff/core/MarkoffDocument.h>
#include <markoff/vnext/ActionPublicationCoordinator.h>
#include <QObject>
#include <QTest>

#include <memory>

// This target does not link corbomite-core. Open the same way the host
// session does: one ephemeral provider, one document, one import.
struct OpenedMarkoff {
    std::unique_ptr<Markoff::VNext::DocumentStorageProvider> provider;
    std::unique_ptr<Markoff::MarkoffDocument> document;
    bool imported = false;
    Markoff::MarkoffDocument *get() const { return document.get(); }
};

static OpenedMarkoff loaded(const char *bytes)
{
    static int serial = 0;
    OpenedMarkoff out;
    auto provider = Markoff::VNext::makeEphemeralDocumentStorageProvider();
    if (!provider.ok())
        return out;
    out.provider = std::move(provider).value();
    auto opened = Markoff::MarkoffDocument::open(
        *out.provider,
        {QStringLiteral("line-resolve-test-%1").arg(++serial), 1},
        Markoff::VNext::StorageMode::ephemeral);
    if (!opened.ok())
        return out;
    out.document = std::move(opened).value();
    const auto published = out.document->importInitialMarkdown(QByteArray(bytes));
    out.imported = published.state == Markoff::VNext::ActionPublicationState::committed
        || published.state == Markoff::VNext::ActionPublicationState::committedHistoryPending;
    return out;
}

using namespace Corbomite;

class LineResolveTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void singleLineBlocks()
    {
        auto opened = loaded("Alpha\n\nBeta\n\nGamma\n");
        QVERIFY(opened.imported);
        auto *doc = opened.get();
        auto r1 = LineResolve::resolveLine(doc, 1);
        QVERIFY(r1.has_value());
        QCOMPARE(r1->blockRow, 0);
        QCOMPARE(r1->lineStartCharInBlock, 0);
        QCOMPARE(r1->lineText, QStringLiteral("Alpha"));
        auto r3 = LineResolve::resolveLine(doc, 3);
        QVERIFY(r3.has_value());
        QCOMPARE(r3->blockRow, 2);
        QCOMPARE(r3->lineText, QStringLiteral("Gamma"));
    }

    void multiLineBlock()
    {
        // A code block keeps internal newlines in its buffer.
        auto opened = loaded("Intro\n\n```\nline a\nline b\n```\n");
        QVERIFY(opened.imported);
        auto *doc = opened.get();
        bool found = false;
        for (int line = 1; line <= 8; ++line) {
            auto r = LineResolve::resolveLine(doc, line);
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
        auto opened = loaded("One\n");
        QVERIFY(opened.imported);
        auto *doc = opened.get();
        QVERIFY(!LineResolve::resolveLine(doc, 0).has_value());
        QVERIFY(!LineResolve::resolveLine(doc, 99).has_value());
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

    void globalByteOffsetAcrossBlocks()
    {
        // No-separator concatenation of block texts: "Alpha"+"Beta"+
        // "Gamma" → byte bases 0, 5, 9.
        auto opened = loaded("Alpha\n\nBeta\n\nGamma\n");
        QVERIFY(opened.imported);
        auto *doc = opened.get();

        // Line 1 col 1 → start of "Alpha".
        QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, 1, 1).value(), 0u);
        // Line 1 col 3 → two chars into "Alpha".
        QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, 1, 3).value(), 2u);
        // Line 2 ("Beta") col 1 → after "Alpha" (5 bytes, no separator).
        QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, 2, 1).value(), 5u);
        // Line 3 ("Gamma") col 1 → after "Alpha"+"Beta" = 9.
        QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, 3, 1).value(), 9u);
        // Column past line end clamps to the line's end, not into the next.
        QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, 1, 99).value(), 5u);
        // Unresolvable line → nullopt.
        QVERIFY(!LineResolve::globalByteOffsetForCursor(doc, 99, 1).has_value());
    }

    void globalByteOffsetWithinMultiLineBlock()
    {
        // A code block holds internal newlines in one buffer; the no-sep
        // base for a caret on its second inner line includes the preceding
        // block plus the in-block bytes up to that line.
        auto opened = loaded("Intro\n\n```\nline a\nline b\n```\n");
        QVERIFY(opened.imported);
        auto *doc = opened.get();

        // Find the visual line whose text is "line b" and verify its col-1
        // offset lands inside the code block (> "Intro" length) and matches
        // the resolveLine/byteOffsetForChar composition.
        bool checked = false;
        for (int line = 1; line <= 10; ++line) {
            const auto rl = LineResolve::resolveLine(doc, line);
            if (!rl) break;
            if (rl->lineText != QStringLiteral("line b")) continue;
            const auto ids = doc->iterateBlocks();
            uint32_t base = 0;
            for (int r = 0; r < rl->blockRow; ++r)
                base += uint32_t(doc->blockText(ids[size_t(r)]).size());
            const QString blockStr = QString::fromUtf8(doc->blockText(rl->blockId));
            const uint32_t expected =
                base + LineResolve::byteOffsetForChar(blockStr, rl->lineStartCharInBlock);
            QCOMPARE(LineResolve::globalByteOffsetForCursor(doc, line, 1).value(), expected);
            QVERIFY(expected > 5u);  // past "Intro"
            checked = true;
        }
        QVERIFY2(checked, "multi-line block inner line not found");
    }

    void caretAfterInsertSameLine()
    {
        // No newline before the caret → same flat line, column advances by the
        // inserted char count.
        const auto c = LineResolve::caretAfterFlatInsert({2, 4},
                                                         QStringLiteral("abc"));
        QCOMPARE(c.line, 2);
        QCOMPARE(c.column, 7);   // 4 + 3
    }

    void caretAfterInsertNewLines()
    {
        // Two newlines before the caret → descend two lines; column restarts
        // from the content after the last newline ("xy" → col 3).
        const auto c = LineResolve::caretAfterFlatInsert({1, 5},
                                                         QStringLiteral("p\nq\nxy"));
        QCOMPARE(c.line, 3);
        QCOMPARE(c.column, 3);   // "xy" → 2 chars, 1-based
    }

    void caretAfterInsertTrailingNewline()
    {
        // Caret immediately after a newline → fresh line, column 1.
        const auto c = LineResolve::caretAfterFlatInsert({3, 2},
                                                         QStringLiteral("head\n"));
        QCOMPARE(c.line, 4);
        QCOMPARE(c.column, 1);
    }
};

QTEST_MAIN(LineResolveTest)
#include "tst_line_resolve.moc"
