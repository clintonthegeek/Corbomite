// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Renderer.h"
#include "markoff/Document.h"
#include "markoff/RenderSettings.h"
#include "DocumentBuilder_p.h"

#include <QString>
#include <QTextDocument>
#include <QFileInfo>
#include <QDir>

namespace Markoff {

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
                // Resolve relative paths against basePath
                if (!settings.basePath.isEmpty() && !src.startsWith(QStringLiteral("http"))
                    && !src.startsWith(QStringLiteral("data:"))) {
                    QFileInfo fi(QDir(settings.basePath), src);
                    if (fi.exists())
                        src = fi.absoluteFilePath();
                }
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
            text = QStringLiteral("<code>%1</code>").arg(text);
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
        const QString lang = block.codeInfo.isEmpty()
            ? QString()
            : QStringLiteral(" class=\"language-%1\"").arg(escapeHtml(block.codeInfo));
        const QString code = renderInlines(block.inlines, settings);
        html += QStringLiteral("<pre><code%1>%2</code></pre>").arg(lang, code);
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

    // Re-parse markdown content (without frontmatter)
    DocumentBuilder builder;
    builder.parse(doc.markdownContent());
    QList<Block> blocks = builder.takeBlocks();
    DocumentBuilder::postProcess(blocks);

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

    // Build CSS
    QString bodyStyle = QStringLiteral("font-size: %1pt;").arg(s.baseFontSizePt);
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
