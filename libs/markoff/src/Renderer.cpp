// SPDX-License-Identifier: GPL-3.0-or-later
#include "Renderer.h"
#include "markoff/Document.h"
#include "markoff/RenderSettings.h"
#include "markoff/ResourceProvider.h"
#include "markoff/Theme.h"
#include "DocumentBuilder_p.h"
#include "MathRenderer.h"

#include <QColor>
#include <QString>
#include <QTextDocument>
#include <QUrl>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>

namespace Markoff {

// ---------------------------------------------------------------------------
// Callout type → color
// ---------------------------------------------------------------------------

namespace {
struct CalloutColor { const char *type; const char *color; };
constexpr CalloutColor kCalloutColors[] = {
    // Default (used when no other type matches)
    {"note",      "#448aff"},
    {"info",      "#448aff"},
    // Warning family
    {"warning",   "#ff9100"},
    {"caution",   "#ff9100"},
    {"attention", "#ff9100"},
    // Danger / failure family
    {"danger",    "#ff5252"},
    {"error",     "#ff5252"},
    {"failure",   "#ff5252"},
    {"fail",      "#ff5252"},
    {"missing",   "#ff5252"},
    // Success family
    {"success",   "#00c853"},
    {"check",     "#00c853"},
    {"done",      "#00c853"},
    // Tip family
    {"tip",       "#00bfa5"},
    {"hint",      "#00bfa5"},
    {"important", "#00bfa5"},
    // Question family
    {"question",  "#ffab00"},
    {"help",      "#ffab00"},
    {"faq",       "#ffab00"},
    // Other singletons
    {"bug",       "#ff1744"},
    {"example",   "#7c4dff"},
    {"quote",     "#9e9e9e"},
    {"cite",      "#9e9e9e"},
    {"abstract",  "#00b8d4"},
    {"summary",   "#00b8d4"},
    {"tldr",      "#00b8d4"},
};

QString colorForCalloutType(const QString &type)
{
    const QByteArray utf8 = type.toUtf8();
    for (const auto &cc : kCalloutColors) {
        if (qstrcmp(utf8.constData(), cc.type) == 0)
            return QString::fromLatin1(cc.color);
    }
    return QStringLiteral("#448aff");  // default blue
}
} // namespace

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
    ResourceProvider *resourceProvider = nullptr;
    // Optional theme — only used to derive a few CSS colors. Stored as
    // pointer-or-nullptr-equivalent via a default-constructed Theme,
    // detected by checking if the format hash is empty.
    Theme theme;
};

namespace {
/// Per-render context bundling the settings and the (optional) resource
/// provider, so it can be threaded through the static helpers without
/// adding individual parameters at every call site.
struct RenderContext {
    const RenderSettings &settings;
    ResourceProvider *resourceProvider = nullptr;
};
}

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

static QString renderInlines(const QList<InlineRun> &inlines, const RenderContext &ctx)
{
    QString html;
    for (const InlineRun &run : inlines) {
        // Comments are hidden in rendered output
        if (run.comment)
            continue;

        // Image rendering
        if (!run.imageSrc.isEmpty()) {
            if (ctx.settings.renderImages) {
                QString src = run.imageSrc;
                // Resolve relative paths via ResourceProvider when available.
                // Skip http(s):// and data: URIs which are absolute.
                if (ctx.resourceProvider
                    && !src.startsWith(QStringLiteral("http://"))
                    && !src.startsWith(QStringLiteral("https://"))
                    && !src.startsWith(QStringLiteral("data:"))) {
                    QUrl resolved = ctx.resourceProvider->resolveImage(src);
                    if (!resolved.isEmpty())
                        src = resolved.toString();
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
            QString dataUri = MathRenderer::renderToDataUri(run.text, run.mathDisplay);
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

static QString renderBlocks(const QList<Block> &blocks, const RenderContext &ctx);

static QString renderBlock(const Block &block, const RenderContext &ctx)
{
    const RenderSettings &settings = ctx.settings;
    QString html;

    switch (block.type) {
    case MD_BLOCK_DOC:
        html += renderBlocks(block.children, ctx);
        break;

    case MD_BLOCK_H: {
        const int level = qBound(1, block.headingLevel, 6);
        const QString tag = QStringLiteral("h%1").arg(level);
        html += QStringLiteral("<%1>%2</%1>").arg(tag, renderInlines(block.inlines, ctx));
        break;
    }

    case MD_BLOCK_P:
        html += QStringLiteral("<p>%1</p>").arg(renderInlines(block.inlines, ctx));
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
            const QString color = colorForCalloutType(block.calloutType);
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
                     renderBlocks(block.children, ctx));
        } else {
            html += QStringLiteral("<blockquote>%1</blockquote>").arg(renderBlocks(block.children, ctx));
        }
        break;

    case MD_BLOCK_UL:
        html += QStringLiteral("<ul>%1</ul>").arg(renderBlocks(block.children, ctx));
        break;

    case MD_BLOCK_OL:
        html += QStringLiteral("<ol start=\"%1\">%2</ol>").arg(block.listStart).arg(renderBlocks(block.children, ctx));
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
            .arg(prefix, renderInlines(block.inlines, ctx), renderBlocks(block.children, ctx));
        break;
    }

    case MD_BLOCK_HR:
        html += QStringLiteral("<hr/>");
        break;

    case MD_BLOCK_TABLE:
        html += QStringLiteral("<table>%1</table>").arg(renderBlocks(block.children, ctx));
        break;

    case MD_BLOCK_THEAD:
        html += QStringLiteral("<thead>%1</thead>").arg(renderBlocks(block.children, ctx));
        break;

    case MD_BLOCK_TBODY:
        html += QStringLiteral("<tbody>%1</tbody>").arg(renderBlocks(block.children, ctx));
        break;

    case MD_BLOCK_TR:
        html += QStringLiteral("<tr>%1</tr>").arg(renderBlocks(block.children, ctx));
        break;

    case MD_BLOCK_TH:
        html += QStringLiteral("<th%1>%2</th>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines, ctx));
        break;

    case MD_BLOCK_TD:
        html += QStringLiteral("<td%1>%2</td>")
            .arg(alignAttr(block.tableAlign), renderInlines(block.inlines, ctx));
        break;

    default:
        // Unknown block — render children and inlines inline
        html += renderInlines(block.inlines, ctx);
        html += renderBlocks(block.children, ctx);
        break;
    }

    return html;
}

static QString renderBlocks(const QList<Block> &blocks, const RenderContext &ctx)
{
    QString html;
    for (const Block &block : blocks)
        html += renderBlock(block, ctx);
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

void Renderer::setResourceProvider(ResourceProvider *provider)
{
    d->resourceProvider = provider;
}

void Renderer::setTheme(const Theme &theme)
{
    d->theme = theme;
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
    const RenderContext ctx{s, d->resourceProvider};
    QString bodyHtml;

    // Optionally show frontmatter
    if (s.showFrontmatter && !doc.frontmatter().isEmpty()) {
        bodyHtml += QStringLiteral(
            "<pre style=\"background-color: #f5f5f5; border: 1px solid #e0e0e0; "
            "padding: 8px; border-radius: 4px; color: #78909c;\">---\n%1\n---</pre>")
            .arg(escapeHtml(doc.frontmatter()));
    }

    bodyHtml += renderBlocks(blocks, ctx);

    // Append footnotes section
    if (doc.footnoteCount() > 0) {
        bodyHtml += QStringLiteral("<hr style=\"margin-top: 24px;\"/><ol style=\"font-size: 0.9em; color: #555;\">");
        for (int i = 1; i <= doc.footnoteCount(); ++i) {
            bodyHtml += QStringLiteral("<li>%1</li>").arg(escapeHtml(doc.footnoteContent(i)));
        }
        bodyHtml += QStringLiteral("</ol>");
    }

    // Build CSS. Hardcoded fallbacks are used when the theme is empty
    // (default-constructed) so the renderer is still usable without a
    // theme — that's the legacy behavior.
    auto colorOrFallback = [&](Element el, const QColor &fallback, bool background = false) {
        const QTextCharFormat fmt = d->theme.formats.value(el);
        const QColor c = background ? fmt.background().color() : fmt.foreground().color();
        return c.isValid() && c.alpha() > 0 ? c.name() : fallback.name();
    };
    const QString blockquoteBorder = colorOrFallback(Element::BlockQuote, QColor(QStringLiteral("#aaa")));
    const QString blockquoteText   = colorOrFallback(Element::BlockQuote, QColor(QStringLiteral("#555")));
    const QString codeBg           = colorOrFallback(Element::CodeBlock, QColor(QStringLiteral("#f0f0f0")), /*background=*/true);
    const QString hrColor          = colorOrFallback(Element::HorizontalRule, QColor(QStringLiteral("#ccc")));
    const QString highlightBg      = colorOrFallback(Element::Highlight, QColor(QStringLiteral("#fff9c4")), /*background=*/true);

    QString bodyStyle = QStringLiteral("font-size: 14pt;");
    if (s.marginPx > 0)
        bodyStyle += QStringLiteral(" margin: %1px;").arg(s.marginPx);
    if (s.maxWidthPx > 0)
        bodyStyle += QStringLiteral(" max-width: %1px;").arg(s.maxWidthPx);

    const QString css = QStringLiteral(
        "body { %1 }\n"
        "code { font-family: monospace; background-color: %2; padding: 1px 3px; }\n"
        "pre  { background-color: %2; padding: 8px; border-radius: 4px; }\n"
        "pre code { background-color: transparent; padding: 0; }\n"
        "blockquote { border-left: 4px solid %3; margin-left: 0; padding-left: 12px; color: %4; }\n"
        "hr { border: none; border-top: 1px solid %5; }\n"
        "table { border-collapse: collapse; }\n"
        "th, td { border: 1px solid %5; padding: 4px 8px; }\n"
        "mark { background-color: %6; padding: 1px 2px; }\n"
    ).arg(bodyStyle, codeBg, blockquoteBorder, blockquoteText, hrColor, highlightBg);

    const QString fullHtml = QStringLiteral(
        "<html><head><style>%1</style></head><body>%2</body></html>"
    ).arg(css, bodyHtml);

    textDoc->setHtml(fullHtml);
    return textDoc;
}

} // namespace Markoff
