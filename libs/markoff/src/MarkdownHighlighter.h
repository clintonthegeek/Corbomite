// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_MARKDOWNHIGHLIGHTER_H
#define MARKOFF_MARKDOWNHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>
#include "markoff/Theme.h"
#include "SourceSpan.h"
#include "DecoratedRange.h"

namespace Markoff {

/// Markdown syntax highlighter driven by the parsed AST.
///
/// Instead of regex, this highlighter uses a pre-built span map
/// (from buildSpanMap()) that tells it exactly which byte ranges have
/// which formatting. The AST handles all nesting and precedence correctly.
class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument *parent);

    enum class Mode { Source, LivePreview };
    void setMode(Mode mode);

    /// Replace the current theme and rehighlight.
    void setTheme(const Theme &theme);
    Mode mode() const { return m_mode; }

    /// Update the span map after a reparse.
    void setSpanMap(QList<SourceSpan> spans);

    /// Set decorated ranges (for callout title formatting)
    void setDecoratedRanges(const QList<DecoratedRange> &ranges) { m_decoratedRanges = ranges; }

    /// Cursor position for per-element delimiter hiding.
    void setCursorPosition(int blockNumber, int columnInBlock);

    /// Access the current span map (for block-level formatting by Editor)
    const QList<SourceSpan> &spans() const { return m_spans; }

    /// Access format colors (for Editor decoration painting)
    QColor blockquoteColor() const {
        return m_theme.formats.value(Element::BlockQuote).foreground().color();
    }

protected:
    void highlightBlock(const QString &text) override;

private:
    void hideRange(int start, int length);
    bool cursorInRange(int cursorCol, int matchStart, int matchEnd) const;
    void applySpanFormat(const SourceSpan &span, int blockCharStart, int blockCharEnd,
                          bool shouldHideDelim, int cursorCol);

    Mode m_mode = Mode::Source;
    int m_cursorBlock = -1;
    int m_cursorColumn = -1;

public:
    /// Adjust span map offsets after a text edit. Call BEFORE Qt's
    /// automatic rehighlight runs (connect to contentsChange, not contentsChanged).
    void adjustSpanOffsets(int editPos, int charsRemoved, int charsAdded);
private:

    void highlightCodeBlock(const QString &text, const DecoratedRange &dr, int blockNum);

    QList<SourceSpan> m_spans;
    QList<DecoratedRange> m_decoratedRanges;
    KSyntaxHighlighting::Repository m_syntaxRepo;

    Theme m_theme;
};

} // namespace Markoff
#endif
