// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/TextCardItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>
#include <QRegularExpression>

namespace Canvas {

static constexpr qreal kCornerRadius = 8.0;
static constexpr qreal kColorStripeHeight = 20.0;
static constexpr qreal kTextPadding = 8.0;

TextCardItem::TextCardItem(const CanvasNode &data, QGraphicsItem *parent)
    : CanvasNodeItem(data, parent)
{
    setZValue(1);
}

void TextCardItem::setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc)
{
    m_renderedDoc = std::move(doc);
    update();
}

QRectF TextCardItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void TextCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Draw rounded rect with white fill
    QPainterPath cardPath;
    cardPath.addRoundedRect(rect, kCornerRadius, kCornerRadius);

    painter->fillPath(cardPath, QColor(255, 255, 255));

    // Pre-process text: extract header and strip frontmatter
    QString cardText = m_data.text;
    QString headerText;

    // Strip YAML frontmatter (---\n...\n---)
    static const QRegularExpression frontmatterPattern(
        QStringLiteral(R"(^---\n[\s\S]*?\n---\n?)"), QRegularExpression::MultilineOption);
    cardText.replace(frontmatterPattern, QString());

    // Extract callout-style header: >[!cc-header] Title or >[!some-type] Title
    static const QRegularExpression calloutHeaderPattern(
        QStringLiteral(R"(^>\s*\[![\w-]+\]\s*(.+)$)"), QRegularExpression::MultilineOption);
    auto headerMatch = calloutHeaderPattern.match(cardText);
    if (headerMatch.hasMatch()) {
        headerText = headerMatch.captured(1).trimmed();
        cardText.replace(calloutHeaderPattern, QString());
    }

    // Also extract regular markdown # headers as card header (if no callout header found)
    if (headerText.isEmpty()) {
        static const QRegularExpression mdHeaderPattern(
            QStringLiteral(R"(^#{1,3}\s+(.+)$)"), QRegularExpression::MultilineOption);
        auto mdMatch = mdHeaderPattern.match(cardText);
        if (mdMatch.hasMatch()) {
            headerText = mdMatch.captured(1).trimmed();
            cardText.replace(mdMatch.capturedStart(), mdMatch.capturedLength(), QString());
        }
    }

    // Trim leading/trailing whitespace after stripping
    cardText = cardText.trimmed();

    // 2. Draw color stripe / header bar
    const QColor stripeColor = colorFromCanvasColor(m_data.color);
    qreal headerBarHeight = 0;

    // Process header text: strip markdown syntax, convert to HTML for rendering
    auto processHeaderHtml = [](const QString &raw) -> QString {
        QString h = raw;
        // Strip leading # markers: "# Welcome!" → "Welcome!"
        h.replace(QRegularExpression(QStringLiteral(R"(^#{1,6}\s+)")), QString());
        // Bold: **text** → <b>text</b>
        h.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                  QStringLiteral("<b>\\1</b>"));
        // Italic: *text* → <i>text</i>
        h.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*([^*]+?)\*(?!\*))")),
                  QStringLiteral("<i>\\1</i>"));
        return h;
    };

    if (!headerText.isEmpty() && stripeColor.isValid()) {
        // Full header bar with text inside the color stripe
        headerBarHeight = 32.0;
        painter->save();
        painter->setClipPath(cardPath);
        painter->fillRect(QRectF(0, 0, rect.width(), headerBarHeight), stripeColor);
        painter->restore();

        // Render header with QTextDocument for inline markdown support
        QTextDocument headerDoc;
        QString headerHtml = QStringLiteral("<span style='color:white;font-size:11pt'>")
            + processHeaderHtml(headerText) + QStringLiteral("</span>");
        headerDoc.setHtml(headerHtml);
        headerDoc.setTextWidth(rect.width() - 2 * kTextPadding);
        headerDoc.setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, (headerBarHeight - headerDoc.size().height()) / 2.0);
        headerDoc.drawContents(painter);
        painter->restore();
    } else if (!headerText.isEmpty()) {
        // Header text without color
        headerBarHeight = 28.0;

        QTextDocument headerDoc;
        QString headerHtml = QStringLiteral("<span style='color:#282828;font-size:11pt'>")
            + processHeaderHtml(headerText) + QStringLiteral("</span>");
        headerDoc.setHtml(headerHtml);
        headerDoc.setTextWidth(rect.width() - 2 * kTextPadding);
        headerDoc.setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, (headerBarHeight - headerDoc.size().height()) / 2.0);
        headerDoc.drawContents(painter);
        painter->restore();
    } else if (stripeColor.isValid()) {
        // Color stripe only (thin accent, no header text)
        headerBarHeight = kColorStripeHeight;
        painter->save();
        painter->setClipPath(cardPath);
        painter->fillRect(QRectF(0, 0, rect.width(), headerBarHeight), stripeColor);
        painter->restore();
    }

    // 3. Draw border (1px gray, darker if selected)
    QPen borderPen(selected ? QColor(58, 134, 255) : QColor(200, 200, 200));
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // 4. Render body text
    if (!cardText.isEmpty()) {
        const qreal textTop = headerBarHeight > 0 ? headerBarHeight + kTextPadding : kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;
        const qreal availableHeight = rect.height() - textTop - kTextPadding;

        QTextDocument *doc = nullptr;
        QTextDocument localDoc;

        if (m_renderedDoc && m_renderedDoc->toQTextDocument()) {
            doc = m_renderedDoc->toQTextDocument();
        } else {
            // Fallback: inline regex conversion (legacy path)
            QString html = cardText;
            html.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                          QStringLiteral("<b>\\1</b>"));
            html.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*([^*]+?)\*(?!\*))")),
                          QStringLiteral("<i>\\1</i>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\2</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]]+)\]\])")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(`([^`]+)`)")),
                          QStringLiteral("<code style='background:#f0f0f0;padding:1px 3px'>\\1</code>"));
            html.replace(QRegularExpression(QStringLiteral(R"(^- (.+)$)"), QRegularExpression::MultilineOption),
                          QStringLiteral("&bull; \\1"));
            html.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
            localDoc.setHtml(html);
            doc = &localDoc;
        }

        doc->setTextWidth(textWidth);
        doc->setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, textTop);
        painter->setClipRect(QRectF(0, 0, textWidth, availableHeight));
        doc->drawContents(painter);
        painter->restore();
    }

    // Resize-handle drawing moved to CanvasNodeChromeOverlay (M4.4) — one
    // shared, zoom-constant overlay retargeted to the active node, instead
    // of this triplicated per-item block. Do not re-add handle painting
    // here; see CanvasNodeChromeOverlay.h.
}

} // namespace Canvas
