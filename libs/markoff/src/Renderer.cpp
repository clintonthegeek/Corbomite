// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Renderer.h"
#include "markoff/Document.h"
#include "markoff/RenderSettings.h"
#include "DocumentBuilder_p.h"

#include <QString>
#include <QTextDocument>

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

static QString renderInlines(const QList<InlineRun> &inlines)
{
    QString html;
    for (const InlineRun &run : inlines) {
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
        }

        html += text;
    }
    return html;
}

static QString renderBlocks(const QList<Block> &blocks);

static QString renderBlock(const Block &block)
{
    QString html;

    switch (block.type) {
    case MD_BLOCK_DOC:
        html += renderBlocks(block.children);
        break;

    case MD_BLOCK_H: {
        const int level = qBound(1, block.headingLevel, 6);
        const QString tag = QStringLiteral("h%1").arg(level);
        html += QStringLiteral("<%1>%2</%1>").arg(tag, renderInlines(block.inlines));
        break;
    }

    case MD_BLOCK_P:
        html += QStringLiteral("<p>%1</p>").arg(renderInlines(block.inlines));
        break;

    case MD_BLOCK_CODE: {
        const QString lang = block.codeInfo.isEmpty()
            ? QString()
            : QStringLiteral(" class=\"language-%1\"").arg(escapeHtml(block.codeInfo));
        const QString code = renderInlines(block.inlines);
        html += QStringLiteral("<pre><code%1>%2</code></pre>").arg(lang, code);
        break;
    }

    case MD_BLOCK_QUOTE:
        html += QStringLiteral("<blockquote>%1</blockquote>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_UL:
        html += QStringLiteral("<ul>%1</ul>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_OL:
        html += QStringLiteral("<ol start=\"%1\">%2</ol>").arg(block.listStart).arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_LI: {
        QString prefix;
        if (block.isTaskItem) {
            const bool checked = (block.taskMark == 'x' || block.taskMark == 'X');
            prefix = checked ? QStringLiteral("[x] ") : QStringLiteral("[ ] ");
        }
        html += QStringLiteral("<li>%1%2%3</li>")
            .arg(prefix, renderInlines(block.inlines), renderBlocks(block.children));
        break;
    }

    case MD_BLOCK_HR:
        html += QStringLiteral("<hr/>");
        break;

    case MD_BLOCK_TABLE:
        html += QStringLiteral("<table>%1</table>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_THEAD:
        html += QStringLiteral("<thead>%1</thead>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_TBODY:
        html += QStringLiteral("<tbody>%1</tbody>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_TR:
        html += QStringLiteral("<tr>%1</tr>").arg(renderBlocks(block.children));
        break;

    case MD_BLOCK_TH:
        html += QStringLiteral("<th%1>%2</th>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines));
        break;

    case MD_BLOCK_TD:
        html += QStringLiteral("<td%1>%2</td>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines));
        break;

    default:
        // Unknown block — render children and inlines inline
        html += renderInlines(block.inlines);
        html += renderBlocks(block.children);
        break;
    }

    return html;
}

static QString renderBlocks(const QList<Block> &blocks)
{
    QString html;
    for (const Block &block : blocks)
        html += renderBlock(block);
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

    // Re-parse source text (Phase 1 shortcut — Document doesn't expose AST yet)
    DocumentBuilder builder;
    builder.parse(doc.sourceText());
    const QList<Block> blocks = builder.takeBlocks();

    // Build body HTML
    const QString bodyHtml = renderBlocks(blocks);

    // Build CSS
    const RenderSettings &s = d->settings;
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
    ).arg(bodyStyle);

    const QString fullHtml = QStringLiteral(
        "<html><head><style>%1</style></head><body>%2</body></html>"
    ).arg(css, bodyHtml);

    textDoc->setHtml(fullHtml);
    return textDoc;
}

} // namespace Markoff
