// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Renderer.h"
#include "markoff/Document.h"
#include "markoff/RenderSettings.h"
#include "DocumentBuilder_p.h"

#include <QString>
#include <QTextDocument>
#include <QImage>
#include <QBuffer>
#include <jkqtmathtext/jkqtmathtext.h>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>

namespace Markoff {

// ---------------------------------------------------------------------------
// Math rendering
// ---------------------------------------------------------------------------

static QString renderMathToDataUri(const QString &latex, bool displayMode)
{
    JKQTMathText mt;
    mt.useXITS();
    mt.setFontSize(displayMode ? 14 : 12);

    const QString wrapped = QStringLiteral("$") + latex + QStringLiteral("$");
    if (!mt.parse(wrapped))
        return {};

    QImage img = mt.drawIntoImage(false, Qt::transparent, 2, 1.0, 96);
    if (img.isNull())
        return {};

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");

    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(ba.toBase64());
}

// ---------------------------------------------------------------------------
// Syntax highlighting for code blocks
// ---------------------------------------------------------------------------

class CodeHighlighter : public KSyntaxHighlighting::AbstractHighlighter {
public:
    QString highlightedHtml;
    QString m_currentLine;

    KSyntaxHighlighting::State processLine(const QString &text,
                                            const KSyntaxHighlighting::State &state)
    {
        m_currentLine = text;
        highlightedHtml.clear();
        return highlightLine(text, state);
    }

protected:
    void applyFormat(int offset, int length,
                     const KSyntaxHighlighting::Format &format) override
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

static QString highlightCodeBlock(const QString &code, const QString &lang)
{
    if (lang.isEmpty())
        return code.toHtmlEscaped();

    static KSyntaxHighlighting::Repository repo;
    auto def = repo.definitionForName(lang);
    if (!def.isValid())
        def = repo.definitionForFileName(QStringLiteral("file.") + lang);
    if (!def.isValid())
        return code.toHtmlEscaped();

    CodeHighlighter highlighter;
    highlighter.setDefinition(def);
    highlighter.setTheme(repo.defaultTheme(KSyntaxHighlighting::Repository::LightTheme));

    QString result;
    KSyntaxHighlighting::State state;
    const QStringList lines = code.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        state = highlighter.processLine(lines[i], state);
        result += highlighter.highlightedHtml;
        if (i + 1 < lines.size())
            result += QLatin1Char('\n');
    }
    return result;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

struct Renderer::Private {
    RenderSettings settings;
};

// ---------------------------------------------------------------------------
// HTML helpers
// ---------------------------------------------------------------------------

static QString escapeHtml(const QString &text)
{
    return text.toHtmlEscaped();
}

static QString alignAttr(MD_ALIGN align)
{
    switch (align) {
    case MD_ALIGN_LEFT:   return QStringLiteral(" align=\"left\"");
    case MD_ALIGN_CENTER: return QStringLiteral(" align=\"center\"");
    case MD_ALIGN_RIGHT:  return QStringLiteral(" align=\"right\"");
    default:              return QString();
    }
}

static QString renderInlines(const QList<InlineRun> &inlines, const RenderSettings &settings = {})
{
    QString html;
    for (const InlineRun &run : inlines) {
        // Comments are hidden in rendered output
        if (run.comment)
            continue;

        // Image rendering
        if (!run.imageSrc.isEmpty()) {
            if (settings.renderImages) {
                QString src = run.imageSrc;
                QString alt = escapeHtml(run.text);
                html += QStringLiteral("<img src=\"%1\" alt=\"%2\" style=\"max-width: 100%;\"/>")
                            .arg(escapeHtml(src), alt);
            } else {
                html += escapeHtml(run.text);
            }
            continue;
        }

        QString text = escapeHtml(run.text);

        // Wrap in span/tag based on formatting flags
        if (!run.wikiTarget.isEmpty()) {
            const QString href = QStringLiteral("wikilink:") + escapeHtml(run.wikiTarget);
            text = QStringLiteral("<a href=\"%1\">%2</a>").arg(href, text.isEmpty() ? escapeHtml(run.wikiTarget) : text);
        } else if (!run.linkHref.isEmpty()) {
            text = QStringLiteral("<a href=\"%1\">%2</a>").arg(escapeHtml(run.linkHref), text);
        } else if (run.math || run.mathDisplay) {
            QString dataUri = renderMathToDataUri(run.text, run.mathDisplay);
            if (!dataUri.isEmpty()) {
                QString style = run.mathDisplay
                    ? QStringLiteral("display: block; margin: 8px auto;")
                    : QStringLiteral("vertical-align: middle;");
                text = QStringLiteral("<img src=\"%1\" alt=\"%2\" style=\"%3\"/>")
                           .arg(dataUri, escapeHtml(run.text), style);
            } else {
                // Fallback if rendering fails
                text = QStringLiteral("<code>%1</code>").arg(text);
            }
        } else if (run.code) {
            text = QStringLiteral("<code>%1</code>").arg(text);
        } else {
            if (run.strikethrough)
                text = QStringLiteral("<s>%1</s>").arg(text);
            if (run.italic)
                text = QStringLiteral("<i>%1</i>").arg(text);
            if (run.bold)
                text = QStringLiteral("<b>%1</b>").arg(text);
            if (run.highlight)
                text = QStringLiteral("<mark>%1</mark>").arg(text);
            if (run.isTag)
                text = QStringLiteral("<span style=\"color: #E65100;\">#%1</span>")
                           .arg(text.startsWith(QLatin1Char('#')) ? text.mid(1) : text);
        }

        html += text;
    }
    return html;
}

static QString renderBlocks(const QList<Block> &blocks, const RenderSettings &settings);

static QString renderBlock(const Block &block, const RenderSettings &settings)
{
    QString html;

    switch (block.type) {
    case MD_BLOCK_DOC:
        html += renderBlocks(block.children, settings);
        break;

    case MD_BLOCK_H: {
        const int level = qBound(1, block.headingLevel, 6);
        const QString tag = QStringLiteral("h%1").arg(level);
        html += QStringLiteral("<%1>%2</%1>").arg(tag, renderInlines(block.inlines, settings));
        break;
    }

    case MD_BLOCK_P:
        html += QStringLiteral("<p>%1</p>").arg(renderInlines(block.inlines, settings));
        break;

    case MD_BLOCK_CODE: {
        // Collect raw code text from inlines
        QString rawCode;
        for (const auto &run : block.inlines)
            rawCode += run.text;

        // Apply syntax highlighting if enabled and language is known
        QString codeHtml;
        if (settings.renderCodeHighlighting && !block.codeInfo.isEmpty()) {
            codeHtml = highlightCodeBlock(rawCode, block.codeInfo);
        } else {
            codeHtml = escapeHtml(rawCode);
        }

        html += QStringLiteral("<pre><code>%1</code></pre>").arg(codeHtml);
        break;
    }

    case MD_BLOCK_QUOTE:
        if (block.isCallout) {
            // Callout rendering with colored box
            QString color = QStringLiteral("#448aff"); // default blue
            const QString &t = block.calloutType;
            if (t == QStringLiteral("warning") || t == QStringLiteral("caution") || t == QStringLiteral("attention"))
                color = QStringLiteral("#ff9100");
            else if (t == QStringLiteral("danger") || t == QStringLiteral("error"))
                color = QStringLiteral("#ff5252");
            else if (t == QStringLiteral("success") || t == QStringLiteral("check") || t == QStringLiteral("done"))
                color = QStringLiteral("#00c853");
            else if (t == QStringLiteral("tip") || t == QStringLiteral("hint") || t == QStringLiteral("important"))
                color = QStringLiteral("#00bfa5");
            else if (t == QStringLiteral("question") || t == QStringLiteral("help") || t == QStringLiteral("faq"))
                color = QStringLiteral("#ffab00");
            else if (t == QStringLiteral("bug"))
                color = QStringLiteral("#ff1744");
            else if (t == QStringLiteral("example"))
                color = QStringLiteral("#7c4dff");
            else if (t == QStringLiteral("quote") || t == QStringLiteral("cite"))
                color = QStringLiteral("#9e9e9e");
            else if (t == QStringLiteral("failure") || t == QStringLiteral("fail") || t == QStringLiteral("missing"))
                color = QStringLiteral("#ff5252");
            else if (t == QStringLiteral("abstract") || t == QStringLiteral("summary") || t == QStringLiteral("tldr"))
                color = QStringLiteral("#00b8d4");

            QString title = block.calloutTitle.isEmpty()
                ? block.calloutType.at(0).toUpper() + block.calloutType.mid(1)
                : block.calloutTitle;

            html += QStringLiteral(
                "<div style=\"border-left: 4px solid %1; background-color: %1; "
                "background-color: rgba(%2, 0.1); padding: 8px 12px; margin: 8px 0; border-radius: 4px;\">"
                "<b style=\"color: %1;\">%3</b>"
                "%4</div>")
                .arg(color,
                     color, // used twice — once for border, once for faint bg
                     escapeHtml(title),
                     renderBlocks(block.children, settings));
        } else {
            html += QStringLiteral("<blockquote>%1</blockquote>").arg(renderBlocks(block.children, settings));
        }
        break;

    case MD_BLOCK_UL:
        html += QStringLiteral("<ul>%1</ul>").arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_OL:
        html += QStringLiteral("<ol start=\"%1\">%2</ol>").arg(block.listStart).arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_LI: {
        QString prefix;
        if (block.isTaskItem) {
            switch (block.taskMark) {
            case 'x': case 'X': prefix = QStringLiteral("\u2611 "); break; // ☑ checked
            case ' ':           prefix = QStringLiteral("\u2610 "); break; // ☐ unchecked
            case '/':           prefix = QStringLiteral("\u25D2 "); break; // ◒ in progress
            case '-':           prefix = QStringLiteral("\u2796 "); break; // ➖ cancelled
            case '?':           prefix = QStringLiteral("\u2753 "); break; // ❓ question
            case '!':           prefix = QStringLiteral("\u2757 "); break; // ❗ important
            case '>':           prefix = QStringLiteral("\u27A1 "); break; // ➡ deferred
            default:            prefix = QStringLiteral("\u2610 "); break; // ☐ fallback
            }
        }
        html += QStringLiteral("<li style=\"list-style-type: none;\">%1%2%3</li>")
            .arg(prefix, renderInlines(block.inlines, settings), renderBlocks(block.children, settings));
        break;
    }

    case MD_BLOCK_HR:
        html += QStringLiteral("<hr/>");
        break;

    case MD_BLOCK_TABLE:
        html += QStringLiteral("<table>%1</table>").arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_THEAD:
        html += QStringLiteral("<thead>%1</thead>").arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_TBODY:
        html += QStringLiteral("<tbody>%1</tbody>").arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_TR:
        html += QStringLiteral("<tr>%1</tr>").arg(renderBlocks(block.children, settings));
        break;

    case MD_BLOCK_TH:
        html += QStringLiteral("<th%1>%2</th>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines, settings));
        break;

    case MD_BLOCK_TD:
        html += QStringLiteral("<td%1>%2</td>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines, settings));
        break;

    default:
        // Unknown block — render children and inlines inline
        html += renderInlines(block.inlines, settings);
        html += renderBlocks(block.children, settings);
        break;
    }

    return html;
}

static QString renderBlocks(const QList<Block> &blocks, const RenderSettings &settings)
{
    QString html;
    for (const Block &block : blocks)
        html += renderBlock(block, settings);
    return html;
}

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------

Renderer::Renderer()
    : d(std::make_unique<Private>())
{
}

Renderer::~Renderer() = default;

void Renderer::setSettings(const RenderSettings &settings)
{
    d->settings = settings;
}

RenderSettings Renderer::settings() const
{
    return d->settings;
}

std::unique_ptr<QTextDocument> Renderer::renderToTextDocument(const Document &doc) const
{
    auto textDoc = std::make_unique<QTextDocument>();

    if (doc.isEmpty())
        return textDoc;

    // Access the pre-parsed AST directly (no re-parsing)
    const QList<Block> &blocks = DocumentBlockAccessor::blocks(doc);

    // Build body HTML
    const RenderSettings &s = d->settings;
    QString bodyHtml;

    // Optionally show frontmatter
    if (s.showFrontmatter && !doc.frontmatter().isEmpty()) {
        bodyHtml += QStringLiteral(
            "<pre style=\"background-color: #f5f5f5; border: 1px solid #e0e0e0; "
            "padding: 8px; border-radius: 4px; color: #78909c;\">---\n%1\n---</pre>")
            .arg(escapeHtml(doc.frontmatter()));
    }

    bodyHtml += renderBlocks(blocks, s);

    // Append footnotes section
    if (doc.footnoteCount() > 0) {
        bodyHtml += QStringLiteral("<hr style=\"margin-top: 24px;\"/><ol style=\"font-size: 0.9em; color: #555;\">");
        for (int i = 1; i <= doc.footnoteCount(); ++i) {
            bodyHtml += QStringLiteral("<li>%1</li>").arg(escapeHtml(doc.footnoteContent(i)));
        }
        bodyHtml += QStringLiteral("</ol>");
    }

    // Build CSS
    QString bodyStyle = QStringLiteral("font-size: 14pt;");
    if (s.marginPx > 0)
        bodyStyle += QStringLiteral(" margin: %1px;").arg(s.marginPx);
    if (s.maxWidthPx > 0)
        bodyStyle += QStringLiteral(" max-width: %1px;").arg(s.maxWidthPx);

    const QString css = QStringLiteral(
        "body { %1 }\n"
        "code { font-family: monospace; background-color: #f0f0f0; padding: 1px 3px; }\n"
        "pre  { background-color: #f0f0f0; padding: 8px; border-radius: 4px; }\n"
        "pre code { background-color: transparent; padding: 0; }\n"
        "blockquote { border-left: 4px solid #aaa; margin-left: 0; padding-left: 12px; color: #555; }\n"
        "hr { border: none; border-top: 1px solid #ccc; }\n"
        "table { border-collapse: collapse; }\n"
        "th, td { border: 1px solid #ccc; padding: 4px 8px; }\n"
        "mark { background-color: #fff9c4; padding: 1px 2px; }\n"
    ).arg(bodyStyle);

    const QString fullHtml = QStringLiteral(
        "<html><head><style>%1</style></head><body>%2</body></html>"
    ).arg(css, bodyHtml);

    textDoc->setHtml(fullHtml);
    return textDoc;
}

} // namespace Markoff
