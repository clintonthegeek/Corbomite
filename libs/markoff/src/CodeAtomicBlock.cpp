// SPDX-License-Identifier: GPL-3.0-or-later
#include "CodeAtomicBlock.h"

#include <QPainter>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QTextDocument>

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>

namespace Markoff {

// ---------------------------------------------------------------------------
// Syntax highlighting helper (paints directly via QPainter)
// ---------------------------------------------------------------------------

class CodeBlockHighlighter : public KSyntaxHighlighting::AbstractHighlighter {
public:
    struct Run {
        int start;
        int length;
        QColor color;
        bool bold = false;
        bool italic = false;
    };

    QList<Run> runs;
    QString m_line;

    KSyntaxHighlighting::State processLine(const QString &text,
                                            const KSyntaxHighlighting::State &state)
    {
        m_line = text;
        runs.clear();
        return highlightLine(text, state);
    }

protected:
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &format) override
    {
        Run r;
        r.start = offset;
        r.length = length;
        r.color = format.isDefaultTextStyle(theme())
                      ? QColor(0x33, 0x33, 0x33)
                      : format.textColor(theme());
        r.bold = format.isBold(theme());
        r.italic = format.isItalic(theme());
        runs.append(r);
    }
};

// ---------------------------------------------------------------------------
// CodeAtomicBlock
// ---------------------------------------------------------------------------

CodeAtomicBlock::CodeAtomicBlock(QObject *parent)
    : AtomicBlock(parent)
{
    m_codeFont = QFont(QStringLiteral("JetBrains Mono, Fira Code, Source Code Pro, monospace"));
    m_codeFont.setStyleHint(QFont::Monospace);
    m_codeFont.setPointSize(11);
}

void CodeAtomicBlock::setCode(const QString &code, const QString &language)
{
    m_code = code;
    m_language = language;
    invalidateCache();
    Q_EMIT contentChanged();
}

QSizeF CodeAtomicBlock::sizeForWidth(qreal width) const
{
    if (m_cachedWidth == width && m_cachedSize.isValid())
        return m_cachedSize;

    rebuildCache(width);
    return m_cachedSize;
}

void CodeAtomicBlock::paint(QPainter *painter, const QRectF &rect) const
{
    if (m_cache.isNull() || m_cachedWidth != rect.width())
        rebuildCache(rect.width());

    if (!m_cache.isNull())
        painter->drawPixmap(rect.topLeft(), m_cache);
}

bool CodeAtomicBlock::handleKeyPress(QKeyEvent *event)
{
    if (!m_focused)
        return false;

    if (event->key() == Qt::Key_Escape) {
        leaveBlock();
        return true;
    }

    // For now, code editing in atomic blocks is read-only in live preview.
    // Full editing comes later. Consume the event to prevent it from
    // modifying the underlying text blocks.
    return true;
}

bool CodeAtomicBlock::handleMousePress(QMouseEvent *, const QPointF &)
{
    if (!m_focused) {
        enterBlock(0);
        return true;
    }
    return false;
}

void CodeAtomicBlock::enterBlock(int cursorPosition)
{
    AtomicBlock::enterBlock(cursorPosition);
    invalidateCache(); // repaint with focus indicator
}

void CodeAtomicBlock::leaveBlock()
{
    AtomicBlock::leaveBlock();
    invalidateCache(); // repaint without focus indicator
}

void CodeAtomicBlock::invalidateCache()
{
    m_cache = QPixmap();
    m_cachedWidth = -1;
    m_cachedSize = QSizeF();
}

void CodeAtomicBlock::rebuildCache(qreal width) const
{
    if (width <= 0) {
        m_cachedSize = QSizeF(0, 0);
        return;
    }

    QStringList lines = m_code.split(QLatin1Char('\n'));
    // Remove trailing empty line (common from code blocks ending with \n)
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    const QFontMetricsF fm(m_codeFont);
    const qreal lineHeight = fm.height();

    // Line number gutter width
    const int lineNumDigits = QString::number(lines.size()).length();
    const qreal gutterWidth = fm.horizontalAdvance(QLatin1Char('9')) * (lineNumDigits + 2);

    // Total height: label bar + code lines + bottom padding
    const qreal codeHeight = lines.size() * lineHeight;
    const qreal totalHeight = m_labelHeight + codeHeight + m_padding;

    m_cachedSize = QSizeF(width, totalHeight);
    m_cachedWidth = width;

    // Create pixmap
    const int pw = static_cast<int>(width);
    const int ph = static_cast<int>(totalHeight);
    if (pw <= 0 || ph <= 0)
        return;

    QPixmap pixmap(pw, ph);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    // Background with rounded corners
    QRectF bgRect(0, 0, width, totalHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(m_bgColor);
    p.drawRoundedRect(bgRect, m_cornerRadius, m_cornerRadius);

    // Border
    p.setPen(QPen(m_focused ? QColor(0x42, 0xa5, 0xf5) : m_borderColor, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), m_cornerRadius, m_cornerRadius);

    // Language label (top-right, inside the block)
    if (!m_language.isEmpty()) {
        QFont labelFont = m_codeFont;
        labelFont.setPointSize(8);
        p.setFont(labelFont);
        p.setPen(m_labelColor);
        QFontMetricsF labelFm(labelFont);
        qreal labelWidth = labelFm.horizontalAdvance(m_language) + m_padding;
        QRectF labelRect(width - labelWidth - m_padding, 2, labelWidth, m_labelHeight - 4);
        p.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, m_language);
    }

    // Syntax highlighting setup
    static KSyntaxHighlighting::Repository repo;
    CodeBlockHighlighter highlighter;
    bool hasHighlighting = false;

    if (!m_language.isEmpty()) {
        auto def = repo.definitionForName(m_language);
        if (!def.isValid())
            def = repo.definitionForFileName(QStringLiteral("file.") + m_language);
        if (def.isValid()) {
            highlighter.setDefinition(def);
            highlighter.setTheme(repo.defaultTheme(
                KSyntaxHighlighting::Repository::LightTheme));
            hasHighlighting = true;
        }
    }

    // Draw code lines
    p.setFont(m_codeFont);
    qreal y = m_labelHeight + m_padding;

    KSyntaxHighlighting::State hlState;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // Line number
        p.setPen(QColor(0xbd, 0xbd, 0xbd));
        QRectF numRect(m_padding / 2, y, gutterWidth - m_padding, lineHeight);
        p.drawText(numRect, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(i + 1));

        // Code text
        qreal textX = gutterWidth + m_padding;

        if (hasHighlighting) {
            hlState = highlighter.processLine(line, hlState);
            // Paint each highlighted run
            int lastEnd = 0;
            for (const auto &run : highlighter.runs) {
                // Gap before this run (default color)
                if (run.start > lastEnd) {
                    p.setPen(QColor(0x33, 0x33, 0x33));
                    QString gap = line.mid(lastEnd, run.start - lastEnd);
                    p.drawText(QPointF(textX + fm.horizontalAdvance(line.left(lastEnd)), y + fm.ascent()), gap);
                }

                QFont runFont = m_codeFont;
                if (run.bold) runFont.setBold(true);
                if (run.italic) runFont.setItalic(true);
                p.setFont(runFont);
                p.setPen(run.color);

                QString text = line.mid(run.start, run.length);
                qreal xOff = fm.horizontalAdvance(line.left(run.start));
                p.drawText(QPointF(textX + xOff, y + fm.ascent()), text);

                p.setFont(m_codeFont); // reset
                lastEnd = run.start + run.length;
            }
            // Remainder after last run
            if (lastEnd < line.size()) {
                p.setPen(QColor(0x33, 0x33, 0x33));
                qreal xOff = fm.horizontalAdvance(line.left(lastEnd));
                p.drawText(QPointF(textX + xOff, y + fm.ascent()),
                           line.mid(lastEnd));
            }
        } else {
            // No highlighting — plain text
            p.setPen(QColor(0x33, 0x33, 0x33));
            p.drawText(QPointF(textX, y + fm.ascent()), line);
        }

        y += lineHeight;
    }

    p.end();
    m_cache = pixmap;
}

} // namespace Markoff
