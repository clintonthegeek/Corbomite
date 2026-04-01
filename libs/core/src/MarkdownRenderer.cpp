// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkdownRenderer.h"
#include <QRegularExpression>
#include <QStringList>

#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>

namespace {

class InlineHighlighter : public KSyntaxHighlighting::AbstractHighlighter {
public:
    QString highlightedHtml;
    QString m_currentLine;

    KSyntaxHighlighting::State processLine(const QString &text, const KSyntaxHighlighting::State &state)
    {
        m_currentLine = text;
        highlightedHtml.clear();
        return highlightLine(text, state);
    }

protected:
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) override
    {
        if (!format.isDefaultTextStyle(theme())) {
            QColor color = format.textColor(theme());
            if (format.isBold(theme())) {
                highlightedHtml += QStringLiteral("<b style='color:%1'>").arg(color.name());
                highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
                highlightedHtml += QStringLiteral("</b>");
            } else {
                highlightedHtml += QStringLiteral("<span style='color:%1'>").arg(color.name());
                highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
                highlightedHtml += QStringLiteral("</span>");
            }
        } else {
            highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
        }
    }
};

} // anonymous namespace

// TODO: This entire file should be replaced with a cmark-gfm based renderer.
// The regex approach below is a pragmatic first pass that handles the most
// common markdown patterns. Known limitations:
// - Nested emphasis (***bold italic***) may not render correctly
// - Reference-style links [text][ref] are not supported
// - Complex list nesting (mixed ordered/unordered, 3+ levels) is fragile
// - Table alignment (:---:) is ignored
// - Setext-style headings (underline with === or ---) are not supported

namespace Corbomite {

QString MarkdownRenderer::renderToHtml(const QString &markdown) const
{
    QString body = processBlocks(markdown);
    return wrapDocument(body);
}

QString MarkdownRenderer::processBlocks(const QString &markdown) const
{
    QStringList lines = markdown.split(QLatin1Char('\n'));
    QString html;
    bool inCodeBlock = false;
    QString codeBlockLang;
    QString codeContent;
    bool inList = false;
    bool isOrderedList = false;
    bool inBlockquote = false;
    bool inCallout = false;
    QString calloutType;
    QString calloutContent;

    static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));
    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^```(\w*)$)"));
    static const QRegularExpression ulPattern(QStringLiteral(R"(^[-*+]\s+(.+)$)"));
    static const QRegularExpression olPattern(QStringLiteral(R"(^\d+\.\s+(.+)$)"));
    static const QRegularExpression checkboxUnchecked(QStringLiteral(R"(^[-*+]\s+\[ \]\s+(.+)$)"));
    static const QRegularExpression checkboxChecked(QStringLiteral(R"(^[-*+]\s+\[x\]\s+(.+)$)"));
    static const QRegularExpression blockquotePattern(QStringLiteral(R"(^>\s?(.*)$)"));
    static const QRegularExpression calloutPattern(QStringLiteral(R"(^>\s*\[!(\w+)\]\s*(.*)$)"));
    static const QRegularExpression hrPattern(QStringLiteral(R"(^(---|\*\*\*|___)$)"));

    auto closeList = [&]() {
        if (inList) {
            html += isOrderedList ? QStringLiteral("</ol>\n") : QStringLiteral("</ul>\n");
            inList = false;
        }
    };

    auto closeBlockquote = [&]() {
        if (inBlockquote && !inCallout) {
            html += QStringLiteral("</blockquote>\n");
            inBlockquote = false;
        }
    };

    auto closeCallout = [&]() {
        if (inCallout) {
            html += QStringLiteral("<p>") + processInline(calloutContent.trimmed()) + QStringLiteral("</p>\n");
            html += QStringLiteral("</div>\n");
            inCallout = false;
            calloutContent.clear();
            inBlockquote = false;
        }
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // Code fence handling
        auto codeFenceMatch = codeFencePattern.match(line);
        if (codeFenceMatch.hasMatch()) {
            if (!inCodeBlock) {
                closeList();
                closeBlockquote();
                closeCallout();
                inCodeBlock = true;
                codeBlockLang = codeFenceMatch.captured(1);
                codeContent.clear();
            } else {
                // Try KSyntaxHighlighting for code blocks
                QString highlightedCode;
                if (!codeBlockLang.isEmpty()) {
                    static KSyntaxHighlighting::Repository repo;
                    auto def = repo.definitionForName(codeBlockLang);
                    if (!def.isValid()) {
                        // Try common aliases
                        def = repo.definitionForFileName(QStringLiteral("file.") + codeBlockLang);
                    }
                    if (def.isValid()) {
                        InlineHighlighter highlighter;
                        highlighter.setDefinition(def);
                        highlighter.setTheme(repo.defaultTheme(
                            KSyntaxHighlighting::Repository::LightTheme));

                        KSyntaxHighlighting::State state;
                        const auto codeLines = codeContent.split(QLatin1Char('\n'));
                        for (int ci = 0; ci < codeLines.size(); ++ci) {
                            state = highlighter.processLine(codeLines[ci], state);
                            highlightedCode += highlighter.highlightedHtml;
                            if (ci < codeLines.size() - 1) highlightedCode += QLatin1Char('\n');
                        }
                    }
                }

                if (highlightedCode.isEmpty()) {
                    // Fallback: plain escaped text
                    highlightedCode = escapeHtml(codeContent);
                }

                QString langAttr = codeBlockLang.isEmpty()
                    ? QString()
                    : QStringLiteral(" class=\"language-%1\"").arg(codeBlockLang);
                html += QStringLiteral("<pre><code%1>%2</code></pre>\n")
                            .arg(langAttr, highlightedCode);
                inCodeBlock = false;
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeContent.isEmpty()) codeContent += QLatin1Char('\n');
            codeContent += line;
            continue;
        }

        // Horizontal rule
        if (hrPattern.match(line).hasMatch()) {
            closeList();
            closeBlockquote();
            closeCallout();
            html += QStringLiteral("<hr/>\n");
            continue;
        }

        // Empty line
        if (line.trimmed().isEmpty()) {
            closeList();
            if (inCallout) {
                // Empty line in callout continues the callout
            } else {
                closeBlockquote();
            }
            continue;
        }

        // Callout: > [!type] Title
        auto calloutMatch = calloutPattern.match(line);
        if (calloutMatch.hasMatch() && !inCallout) {
            closeList();
            closeBlockquote();
            closeCallout();
            inCallout = true;
            inBlockquote = true;
            calloutType = calloutMatch.captured(1).toLower();
            QString title = calloutMatch.captured(2);
            html += QStringLiteral("<div class=\"callout callout-%1\">\n").arg(calloutType);
            if (!title.isEmpty()) {
                html += QStringLiteral("<div class=\"callout-title\">%1</div>\n").arg(escapeHtml(title));
            }
            calloutContent.clear();
            continue;
        }

        // Blockquote continuation (including callout body)
        auto bqMatch = blockquotePattern.match(line);
        if (bqMatch.hasMatch()) {
            if (inCallout) {
                if (!calloutContent.isEmpty()) calloutContent += QLatin1Char('\n');
                calloutContent += bqMatch.captured(1);
                continue;
            }
            closeList();
            if (!inBlockquote) {
                html += QStringLiteral("<blockquote>\n");
                inBlockquote = true;
            }
            html += QStringLiteral("<p>") + processInline(bqMatch.captured(1)) + QStringLiteral("</p>\n");
            continue;
        } else {
            closeCallout();
            closeBlockquote();
        }

        // Heading
        auto headingMatch = headingPattern.match(line);
        if (headingMatch.hasMatch()) {
            closeList();
            int level = headingMatch.captured(1).length();
            QString content = processInline(headingMatch.captured(2));
            html += QStringLiteral("<h%1>%2</h%1>\n").arg(level).arg(content);
            continue;
        }

        // Checkbox (before generic list -- more specific pattern)
        auto cbUnchecked = checkboxUnchecked.match(line);
        if (cbUnchecked.hasMatch()) {
            if (!inList) {
                html += QStringLiteral("<ul class=\"checklist\">\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li><input type=\"checkbox\" disabled> %1</li>\n")
                        .arg(processInline(cbUnchecked.captured(1)));
            continue;
        }
        auto cbChecked = checkboxChecked.match(line);
        if (cbChecked.hasMatch()) {
            if (!inList) {
                html += QStringLiteral("<ul class=\"checklist\">\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li><input type=\"checkbox\" checked disabled> %1</li>\n")
                        .arg(processInline(cbChecked.captured(1)));
            continue;
        }

        // Unordered list
        auto ulMatch = ulPattern.match(line);
        if (ulMatch.hasMatch()) {
            if (!inList || isOrderedList) {
                closeList();
                html += QStringLiteral("<ul>\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li>%1</li>\n").arg(processInline(ulMatch.captured(1)));
            continue;
        }

        // Ordered list
        auto olMatch = olPattern.match(line);
        if (olMatch.hasMatch()) {
            if (!inList || !isOrderedList) {
                closeList();
                html += QStringLiteral("<ol>\n");
                inList = true;
                isOrderedList = true;
            }
            html += QStringLiteral("<li>%1</li>\n").arg(processInline(olMatch.captured(1)));
            continue;
        }

        // Regular paragraph
        closeList();
        html += QStringLiteral("<p>%1</p>\n").arg(processInline(line));
    }

    // Close any open blocks
    closeList();
    closeCallout();
    closeBlockquote();

    return html;
}

QString MarkdownRenderer::processInline(const QString &text) const
{
    QString result = text;

    // Strip Obsidian comments: %%...%%
    static const QRegularExpression commentPattern(QStringLiteral(R"(%%.+?%%)"));
    result.replace(commentPattern, QString());

    // Escape HTML in remaining text (but we need to do it carefully to not break our own tags)
    // Process patterns from most specific to least specific

    // Wikilinks: [[Note]] or [[Note|Display]]
    static const QRegularExpression wikiLinkAlias(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])"));
    result.replace(wikiLinkAlias, QStringLiteral(R"(<a class="internal-link" href="\1.md">\2</a>)"));

    static const QRegularExpression wikiLink(QStringLiteral(R"(\[\[([^\]]+)\]\])"));
    result.replace(wikiLink, QStringLiteral(R"(<a class="internal-link" href="\1.md">\1</a>)"));

    // Images: ![alt](src)
    static const QRegularExpression imagePattern(QStringLiteral(R"(!\[([^\]]*)\]\(([^)]+)\))"));
    result.replace(imagePattern, QStringLiteral(R"(<img src="\2" alt="\1"/>)"));

    // Links: [text](url)
    static const QRegularExpression linkPattern(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))"));
    result.replace(linkPattern, QStringLiteral(R"(<a href="\2">\1</a>)"));

    // Highlight: ==text==
    static const QRegularExpression highlightPattern(QStringLiteral(R"(==(.+?)==)"));
    result.replace(highlightPattern, QStringLiteral(R"(<mark>\1</mark>)"));

    // Bold: **text**
    static const QRegularExpression boldPattern(QStringLiteral(R"(\*\*(.+?)\*\*)"));
    result.replace(boldPattern, QStringLiteral(R"(<strong>\1</strong>)"));

    // Italic: *text*
    static const QRegularExpression italicPattern(QStringLiteral(R"(\*(.+?)\*(?!\*))"));
    result.replace(italicPattern, QStringLiteral(R"(<em>\1</em>)"));

    // Strikethrough: ~~text~~
    static const QRegularExpression strikePattern(QStringLiteral(R"(~~(.+?)~~)"));
    result.replace(strikePattern, QStringLiteral(R"(<del>\1</del>)"));

    // Inline code: `code`
    static const QRegularExpression codePattern(QStringLiteral(R"(`([^`]+)`)"));
    result.replace(codePattern, QStringLiteral(R"(<code>\1</code>)"));

    // Tags: #tag
    static const QRegularExpression tagPattern(QStringLiteral(R"((?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*))"));
    result.replace(tagPattern, QStringLiteral(R"(<span class="tag">#\1</span>)"));

    return result;
}

QString MarkdownRenderer::wrapDocument(const QString &body) const
{
    return QStringLiteral("<!DOCTYPE html><html><head><style>%1</style></head><body>%2</body></html>")
        .arg(defaultStylesheet(), body);
}

QString MarkdownRenderer::escapeHtml(const QString &text)
{
    QString result = text;
    result.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    result.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    result.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    result.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return result;
}

QString MarkdownRenderer::defaultStylesheet()
{
    return QStringLiteral(R"(
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            font-size: 16px;
            line-height: 1.6;
            max-width: 700px;
            margin: 0 auto;
            padding: 20px;
            color: #333;
        }
        h1, h2, h3, h4, h5, h6 { margin-top: 1.2em; margin-bottom: 0.5em; }
        h1 { font-size: 2em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
        h2 { font-size: 1.5em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
        h3 { font-size: 1.25em; }
        a { color: #7b6cd9; text-decoration: none; }
        a:hover { text-decoration: underline; }
        a.internal-link { color: #7b6cd9; }
        code { background: #f0f0f0; padding: 2px 4px; border-radius: 3px; font-size: 0.9em; }
        pre { background: #f6f8fa; padding: 16px; border-radius: 6px; overflow-x: auto; }
        pre code { background: none; padding: 0; }
        blockquote { border-left: 3px solid #ddd; margin: 0; padding: 0 1em; color: #666; }
        mark { background: #fff3b0; padding: 1px 2px; }
        img { max-width: 100%; }
        hr { border: none; border-top: 1px solid #ddd; margin: 2em 0; }
        .tag { color: #e06c75; }
        .callout { border-left: 4px solid #d19a66; background: #fdf6e3; padding: 12px 16px; margin: 1em 0; border-radius: 4px; }
        .callout-title { font-weight: bold; margin-bottom: 4px; }
        .callout-warning { border-color: #e5c07b; background: #fdf6e3; }
        .callout-note { border-color: #61afef; background: #eef6ff; }
        .callout-tip { border-color: #98c379; background: #eef8ee; }
        .callout-danger, .callout-error { border-color: #e06c75; background: #fdeaea; }
        .callout-info { border-color: #61afef; background: #eef6ff; }
        .checklist { list-style: none; padding-left: 0; }
        .checklist li { padding: 2px 0; }
        input[type="checkbox"] { margin-right: 6px; }
        del { color: #999; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background: #f6f8fa; font-weight: bold; }
    )");
}

} // namespace Corbomite
