// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/readingview/SectionLayout.h"

#include "corbomite/readingview/ReadingSection.h"
#include "corbomite/readingview/styling/StyleManager.h"

#include <QBrush>
#include <QColor>
#include <QCryptographicHash>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QPen>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextListFormat>
#include <QTextOption>

namespace Corbomite::ReadingView {

namespace {

// -- Inline-span → HTML conversion ------------------------------------------
//
// We render inline formatting by building a small HTML string and letting
// QTextDocument's HTML importer paint it. The Phase 3a surface is:
//   **strong** / __strong__
//   *emph* / _emph_
//   `code`
//   [text](url)
//   [[wiki]]  — rendered as underlined text (Phase 3b will resolve targets)
// Everything else is passed through as plain text with &, <, > escaped.
//
// This is a deliberately small subset. Phase 3b owns the per-span styling
// pass via CharacterStyle.

QString escapeHtml(const QString &in)
{
    QString out;
    out.reserve(in.size());
    for (QChar c : in) {
        if (c == QLatin1Char('&')) out += QStringLiteral("&amp;");
        else if (c == QLatin1Char('<')) out += QStringLiteral("&lt;");
        else if (c == QLatin1Char('>')) out += QStringLiteral("&gt;");
        else out += c;
    }
    return out;
}

QString inlineToHtml(const QString &text)
{
    // We process sequentially with a tiny scanner to handle nested inline
    // markers in a single pass. Precedence: inline-code > wiki-link >
    // link > strong > emph. Code spans must be emitted first because their
    // contents are literal.
    QString out;
    int i = 0;
    const int n = text.size();
    while (i < n) {
        const QChar c = text.at(i);

        // Inline code: `...` — shortest match
        if (c == QLatin1Char('`')) {
            int closeAt = text.indexOf(QLatin1Char('`'), i + 1);
            if (closeAt > i) {
                const QString code = text.mid(i + 1, closeAt - i - 1);
                out += QStringLiteral("<code>") + escapeHtml(code)
                     + QStringLiteral("</code>");
                i = closeAt + 1;
                continue;
            }
        }

        // Wiki-link: [[target|display]] or [[target]]
        if (c == QLatin1Char('[') && i + 1 < n
            && text.at(i + 1) == QLatin1Char('[')) {
            int closeAt = text.indexOf(QStringLiteral("]]"), i + 2);
            if (closeAt > i) {
                QString inner = text.mid(i + 2, closeAt - i - 2);
                QString display = inner;
                int pipe = inner.indexOf(QLatin1Char('|'));
                if (pipe >= 0)
                    display = inner.mid(pipe + 1);
                out += QStringLiteral("<a href=\"wiki:")
                     + escapeHtml(inner)
                     + QStringLiteral("\">")
                     + escapeHtml(display)
                     + QStringLiteral("</a>");
                i = closeAt + 2;
                continue;
            }
        }

        // Standard link: [text](url)
        if (c == QLatin1Char('[')) {
            int closeBr = text.indexOf(QLatin1Char(']'), i + 1);
            if (closeBr > i && closeBr + 1 < n
                && text.at(closeBr + 1) == QLatin1Char('(')) {
                int closeParen = text.indexOf(QLatin1Char(')'), closeBr + 2);
                if (closeParen > closeBr) {
                    const QString linkText =
                        text.mid(i + 1, closeBr - i - 1);
                    const QString url =
                        text.mid(closeBr + 2, closeParen - closeBr - 2);
                    out += QStringLiteral("<a href=\"")
                         + escapeHtml(url)
                         + QStringLiteral("\">")
                         + inlineToHtml(linkText)
                         + QStringLiteral("</a>");
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        // Strong: ** or __
        auto tryPaired = [&](const QString &delim, const QString &tag) -> bool {
            if (!text.mid(i, delim.size()).startsWith(delim))
                return false;
            int closeAt = text.indexOf(delim, i + delim.size());
            if (closeAt <= i) return false;
            const QString inner =
                text.mid(i + delim.size(), closeAt - i - delim.size());
            out += QStringLiteral("<") + tag + QStringLiteral(">")
                 + inlineToHtml(inner)
                 + QStringLiteral("</") + tag + QStringLiteral(">");
            i = closeAt + delim.size();
            return true;
        };

        if (tryPaired(QStringLiteral("**"), QStringLiteral("b"))) continue;
        if (tryPaired(QStringLiteral("__"), QStringLiteral("b"))) continue;

        // Emphasis: single * or _
        if (c == QLatin1Char('*') || c == QLatin1Char('_')) {
            const QChar dch = c;
            int closeAt = text.indexOf(dch, i + 1);
            if (closeAt > i) {
                const QString inner = text.mid(i + 1, closeAt - i - 1);
                // Avoid stealing ** starts.
                if (!(i + 1 < n && text.at(i + 1) == dch)) {
                    out += QStringLiteral("<i>")
                         + inlineToHtml(inner)
                         + QStringLiteral("</i>");
                    i = closeAt + 1;
                    continue;
                }
            }
        }

        // Default: escape one char.
        out += escapeHtml(QString(c));
        ++i;
    }
    return out;
}

// -- Block-level breakdown --------------------------------------------------
//
// We walk the section's lines and emit a small sequence of block records.
// Each block becomes one graphics item (or a tiny group) in the layout.

enum class BlockKind {
    Heading,
    Paragraph,
    CodeBlock,
    UnorderedList,
    OrderedList,
    HorizontalRule,
    Blockquote,
};

struct CodeBlockInfo {
    QString language;
    QString content;
};

struct ListItem {
    int indent = 0;   // leading-space count / 2 (rough nesting)
    QString text;
    bool ordered = false;
};

struct BlockRecord {
    BlockKind kind;
    int headingLevel = 0;          // for Heading
    QString text;                  // for Heading, Paragraph, Blockquote
    CodeBlockInfo code;            // for CodeBlock
    QList<ListItem> listItems;     // for UnorderedList / OrderedList
};

// Split a section into blocks. Simple line-based pass.
QList<BlockRecord> parseBlocks(const QString &md)
{
    QList<BlockRecord> out;
    const QStringList lines = md.split(QLatin1Char('\n'));

    static const QRegularExpression headingRe(
        QStringLiteral(R"(^ {0,3}(#{1,6})\s+(.*?)\s*#*\s*$)"));
    static const QRegularExpression hrRe(
        QStringLiteral(R"(^ {0,3}([-*_])(?:\s*\1){2,}\s*$)"));
    static const QRegularExpression fenceOpenRe(
        QStringLiteral(R"(^ {0,3}(`{3,}|~{3,})\s*([A-Za-z0-9_+\-.]*)\s*$)"));
    static const QRegularExpression ulRe(
        QStringLiteral(R"(^(\s*)[-*+]\s+(.*)$)"));
    static const QRegularExpression olRe(
        QStringLiteral(R"(^(\s*)\d+[.)]\s+(.*)$)"));
    static const QRegularExpression bqRe(
        QStringLiteral(R"(^ {0,3}>\s?(.*)$)"));

    int i = 0;
    const int n = lines.size();
    while (i < n) {
        const QString &ln = lines.at(i);

        if (ln.trimmed().isEmpty()) { ++i; continue; }

        // Fenced code block
        auto fm = fenceOpenRe.match(ln);
        if (fm.hasMatch()) {
            const QString fence = fm.captured(1);
            const QString lang = fm.captured(2);
            CodeBlockInfo cb;
            cb.language = lang;
            QStringList body;
            ++i;
            while (i < n) {
                const QString &cl = lines.at(i);
                if (cl.trimmed().startsWith(fence.left(1))
                    && cl.trimmed().count(fence.at(0)) >= fence.size()
                    && cl.trimmed().length() == cl.trimmed().count(fence.at(0))) {
                    ++i;
                    break;
                }
                body << cl;
                ++i;
            }
            cb.content = body.join(QLatin1Char('\n'));
            BlockRecord br;
            br.kind = BlockKind::CodeBlock;
            br.code = cb;
            out.push_back(br);
            continue;
        }

        // Heading
        auto hm = headingRe.match(ln);
        if (hm.hasMatch()) {
            BlockRecord br;
            br.kind = BlockKind::Heading;
            br.headingLevel = hm.captured(1).size();
            br.text = hm.captured(2);
            out.push_back(br);
            ++i;
            continue;
        }

        // Horizontal rule
        auto hrm = hrRe.match(ln);
        if (hrm.hasMatch()) {
            BlockRecord br;
            br.kind = BlockKind::HorizontalRule;
            out.push_back(br);
            ++i;
            continue;
        }

        // Blockquote
        auto bqm = bqRe.match(ln);
        if (bqm.hasMatch()) {
            QStringList bq;
            bq << bqm.captured(1);
            ++i;
            while (i < n) {
                auto m2 = bqRe.match(lines.at(i));
                if (!m2.hasMatch()) break;
                bq << m2.captured(1);
                ++i;
            }
            BlockRecord br;
            br.kind = BlockKind::Blockquote;
            br.text = bq.join(QLatin1Char('\n'));
            out.push_back(br);
            continue;
        }

        // List
        auto ulm = ulRe.match(ln);
        auto olm = olRe.match(ln);
        if (ulm.hasMatch() || olm.hasMatch()) {
            const bool ordered = olm.hasMatch();
            BlockRecord br;
            br.kind = ordered ? BlockKind::OrderedList
                              : BlockKind::UnorderedList;
            while (i < n) {
                auto a = ulRe.match(lines.at(i));
                auto b = olRe.match(lines.at(i));
                if (!a.hasMatch() && !b.hasMatch()) break;
                const QRegularExpressionMatch &m =
                    a.hasMatch() ? a : b;
                ListItem li;
                li.indent = m.captured(1).size() / 2;
                li.text = m.captured(2);
                li.ordered = b.hasMatch();
                br.listItems.push_back(li);
                ++i;
            }
            out.push_back(br);
            continue;
        }

        // Paragraph — collect until blank line or next block-start.
        QStringList para;
        while (i < n) {
            const QString &pl = lines.at(i);
            if (pl.trimmed().isEmpty()) break;
            if (headingRe.match(pl).hasMatch()) break;
            if (hrRe.match(pl).hasMatch()) break;
            if (fenceOpenRe.match(pl).hasMatch()) break;
            if (bqRe.match(pl).hasMatch()) break;
            if (ulRe.match(pl).hasMatch() || olRe.match(pl).hasMatch()) break;
            para << pl;
            ++i;
        }
        if (!para.isEmpty()) {
            BlockRecord br;
            br.kind = BlockKind::Paragraph;
            br.text = para.join(QLatin1Char('\n'));
            out.push_back(br);
        }
    }
    return out;
}

// -- Helpers to create each graphics item -----------------------------------

QGraphicsTextItem *makeTextItem(const QString &html,
                                const QFont &font,
                                const QColor &color,
                                qreal width)
{
    auto *item = new QGraphicsTextItem;
    item->setDefaultTextColor(color);
    item->setFont(font);
    item->setTextWidth(width);
    // Keep HTML formatting: headings need setHtml; paragraphs need setHtml.
    item->setHtml(html);
    return item;
}

QString listHtml(const QList<ListItem> &items, bool ordered)
{
    // Build nested HTML based on indent level.
    QString out;
    int currentIndent = 0;
    out += ordered ? QStringLiteral("<ol>") : QStringLiteral("<ul>");
    for (const auto &li : items) {
        while (currentIndent < li.indent) {
            out += ordered ? QStringLiteral("<ol>")
                           : QStringLiteral("<ul>");
            ++currentIndent;
        }
        while (currentIndent > li.indent) {
            out += ordered ? QStringLiteral("</ol>")
                           : QStringLiteral("</ul>");
            --currentIndent;
        }
        out += QStringLiteral("<li>")
             + inlineToHtml(li.text)
             + QStringLiteral("</li>");
    }
    while (currentIndent >= 0) {
        out += ordered ? QStringLiteral("</ol>")
                       : QStringLiteral("</ul>");
        --currentIndent;
    }
    return out;
}

} // namespace

SectionLayout::SectionLayout() = default;

SectionLayout::~SectionLayout()
{
    qDeleteAll(m_highlighters);
    m_highlighters.clear();
}

QGraphicsItemGroup *SectionLayout::layoutSection(ReadingSection &section,
                                                 const QString &sectionMarkdown,
                                                 const Context &ctx)
{
    if (!ctx.styles)
        return nullptr;

    auto *group = new QGraphicsItemGroup;
    qreal y = 0.0;
    const qreal contentWidth = ctx.contentWidth;

    QByteArray shapeSrc;

    const QList<BlockRecord> blocks = parseBlocks(sectionMarkdown);

    for (const BlockRecord &br : blocks) {
        QGraphicsItem *child = nullptr;
        qreal spaceAfter = 0.0;

        switch (br.kind) {
        case BlockKind::Heading: {
            const QString styleName =
                QStringLiteral("Heading%1").arg(br.headingLevel);
            ParagraphStyle ps = ctx.styles->resolvedParagraphStyle(styleName);
            QFont font;
            if (ps.hasFontFamily()) font.setFamily(ps.fontFamily());
            if (ps.hasFontSize()) font.setPointSizeF(ps.fontSize());
            if (ps.hasFontWeight()) font.setWeight(ps.fontWeight());
            if (ps.hasFontItalic()) font.setItalic(ps.fontItalic());
            const QColor color = ps.hasForeground() ? ps.foreground()
                                                    : QColor(Qt::black);
            auto *t = makeTextItem(inlineToHtml(br.text), font, color,
                                   contentWidth);
            child = t;
            spaceAfter = ps.hasSpaceAfter() ? ps.spaceAfter() : 10.0;
            shapeSrc += "H|";
            shapeSrc += QByteArray::number(br.headingLevel);
            shapeSrc += "|";
            shapeSrc += br.text.toUtf8();
            shapeSrc += ";";
            break;
        }

        case BlockKind::Paragraph: {
            ParagraphStyle ps =
                ctx.styles->resolvedParagraphStyle(QStringLiteral("Body"));
            QFont font;
            if (ps.hasFontFamily()) font.setFamily(ps.fontFamily());
            if (ps.hasFontSize()) font.setPointSizeF(ps.fontSize());
            const QColor color = ps.hasForeground() ? ps.foreground()
                                                    : QColor(Qt::black);
            // Fold line breaks into spaces — paragraph-level wrap.
            QString joined = br.text;
            joined.replace(QLatin1Char('\n'), QLatin1Char(' '));
            auto *t = makeTextItem(inlineToHtml(joined), font, color,
                                   contentWidth);
            child = t;
            spaceAfter = ps.hasSpaceAfter() ? ps.spaceAfter() : 8.0;
            shapeSrc += "P|";
            shapeSrc += br.text.toUtf8();
            shapeSrc += ";";
            break;
        }

        case BlockKind::CodeBlock: {
            ParagraphStyle ps =
                ctx.styles->resolvedParagraphStyle(QStringLiteral("CodeBlock"));
            QFont font;
            font.setFamily(ps.hasFontFamily() ? ps.fontFamily()
                                              : QStringLiteral("monospace"));
            font.setPointSizeF(ps.hasFontSize() ? ps.fontSize() : 13.0);
            font.setFixedPitch(true);
            font.setStyleHint(QFont::Monospace);
            const QColor color = ps.hasForeground() ? ps.foreground()
                                                    : QColor(Qt::black);

            auto *t = new QGraphicsTextItem;
            t->setFont(font);
            t->setDefaultTextColor(color);
            t->setTextWidth(contentWidth);
            t->setPlainText(br.code.content);

            // Tag every QTextBlock with the language so CodeBlockHighlighter
            // picks it up.
            QTextDocument *doc = t->document();
            if (!br.code.language.isEmpty()) {
                QTextCursor cursor(doc);
                QTextBlock blk = doc->begin();
                while (blk.isValid()) {
                    QTextCursor bc(blk);
                    QTextBlockFormat bf = blk.blockFormat();
                    bf.setProperty(QTextFormat::BlockCodeLanguage,
                                   br.code.language);
                    bc.setBlockFormat(bf);
                    blk = blk.next();
                }
            }

            // Background rect under the code.
            // Compute it after text geometry is known.
            auto *hl = new CodeBlockHighlighter(ctx.theme);
            hl->highlight(doc);
            m_highlighters.push_back(hl);

            if (ps.hasBackground()) {
                QRectF bbox = t->boundingRect();
                auto *bg = new QGraphicsRectItem(0, 0,
                                                  contentWidth,
                                                  bbox.height());
                bg->setBrush(ps.background());
                bg->setPen(Qt::NoPen);
                bg->setZValue(-1);
                group->addToGroup(bg);
                bg->setPos(0, y);
            }

            child = t;
            spaceAfter = ps.hasSpaceAfter() ? ps.spaceAfter() : 10.0;
            shapeSrc += "C|";
            shapeSrc += br.code.language.toUtf8();
            shapeSrc += "|";
            shapeSrc += br.code.content.toUtf8();
            shapeSrc += ";";
            break;
        }

        case BlockKind::UnorderedList:
        case BlockKind::OrderedList: {
            ParagraphStyle ps =
                ctx.styles->resolvedParagraphStyle(QStringLiteral("ListItem"));
            QFont font;
            if (ps.hasFontFamily()) font.setFamily(ps.fontFamily());
            if (ps.hasFontSize()) font.setPointSizeF(ps.fontSize());
            else font.setPointSizeF(14);
            const QColor color = ps.hasForeground() ? ps.foreground()
                                                    : QColor(Qt::black);
            const bool ordered = (br.kind == BlockKind::OrderedList);
            const QString html = listHtml(br.listItems, ordered);
            auto *t = makeTextItem(html, font, color, contentWidth);
            child = t;
            spaceAfter = ps.hasSpaceAfter() ? ps.spaceAfter() : 8.0;
            shapeSrc += ordered ? "OL|" : "UL|";
            for (const auto &it : br.listItems) {
                shapeSrc += QByteArray::number(it.indent);
                shapeSrc += ":";
                shapeSrc += it.text.toUtf8();
                shapeSrc += "|";
            }
            shapeSrc += ";";
            break;
        }

        case BlockKind::HorizontalRule: {
            auto *line = new QGraphicsLineItem(0, 0, contentWidth, 0);
            QPen pen(ctx.theme == Theme::Dark ? QColor(80, 80, 80)
                                              : QColor(200, 200, 200));
            pen.setWidth(1);
            line->setPen(pen);
            child = line;
            spaceAfter = 12.0;
            shapeSrc += "HR;";
            break;
        }

        case BlockKind::Blockquote: {
            ParagraphStyle ps =
                ctx.styles->resolvedParagraphStyle(QStringLiteral("Blockquote"));
            QFont font;
            if (ps.hasFontFamily()) font.setFamily(ps.fontFamily());
            if (ps.hasFontSize()) font.setPointSizeF(ps.fontSize());
            else font.setPointSizeF(14);
            if (ps.hasFontItalic()) font.setItalic(ps.fontItalic());
            const QColor color = ps.hasForeground() ? ps.foreground()
                                                    : QColor(Qt::darkGray);

            QString joined = br.text;
            auto *t = makeTextItem(inlineToHtml(joined), font, color,
                                   contentWidth - 24);
            // Left-border bar.
            auto *bar = new QGraphicsRectItem(0, 0, 4, 1);
            bar->setBrush(QColor(ctx.theme == Theme::Dark
                                     ? QColor(120, 120, 120)
                                     : QColor(160, 160, 160)));
            bar->setPen(Qt::NoPen);

            // Background.
            if (ps.hasBackground()) {
                auto *bg = new QGraphicsRectItem(0, 0,
                                                  contentWidth, 1);
                bg->setBrush(ps.background());
                bg->setPen(Qt::NoPen);
                bg->setZValue(-1);
                group->addToGroup(bg);
                const qreal h = t->boundingRect().height();
                bg->setRect(0, 0, contentWidth, h);
                bg->setPos(0, y);
            }

            const qreal h = t->boundingRect().height();
            bar->setRect(0, 0, 4, h);
            bar->setPos(0, y);
            group->addToGroup(bar);

            t->setPos(16, y);
            group->addToGroup(t);
            section.setGraphicsItem(group);
            y += h + (ps.hasSpaceAfter() ? ps.spaceAfter() : 10.0);

            shapeSrc += "BQ|";
            shapeSrc += br.text.toUtf8();
            shapeSrc += ";";
            continue; // already placed
        }
        }

        if (child) {
            child->setPos(0, y);
            group->addToGroup(child);
            QRectF bb = child->boundingRect();
            y += bb.height() + spaceAfter;
        }
    }

    const QByteArray digest =
        QCryptographicHash::hash(shapeSrc, QCryptographicHash::Sha256);
    section.setRenderedShape(digest);
    section.setGraphicsItem(group);
    return group;
}

} // namespace Corbomite::ReadingView
